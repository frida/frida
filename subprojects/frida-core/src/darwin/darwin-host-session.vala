namespace Frida {
	public sealed class DarwinHostSessionBackend : LocalHostSessionBackend {
		protected override LocalHostSessionProvider make_provider () {
			return new DarwinHostSessionProvider ();
		}
	}

	public sealed class DarwinHostSessionProvider : LocalHostSessionProvider {
		protected override LocalHostSession make_host_session (HostSessionOptions? options) throws Error {
			var tempdir = new TemporaryDirectory ();
			return new DarwinHostSession (new DarwinHelperProcess (tempdir), tempdir);
		}

		protected override Variant? load_icon () {
			return _try_extract_icon ();
		}

		public extern static Variant? _try_extract_icon ();
	}

	public sealed class DarwinHostSession : LocalHostSession {
		public DarwinHelper helper {
			get;
			construct;
		}

		public TemporaryDirectory tempdir {
			get;
			construct;
		}

		public bool report_crashes {
			get;
			construct;
		}

		public string? agent_path {
			owned get {
#if HAVE_EMBEDDED_ASSETS
				return null;
#else
				return Frida.agent_path;
#endif
			}
		}

		private AgentContainer? system_session_container;

		private Fruitjector fruitjector;
		private AgentResource? agent;
#if IOS || TVOS
		private FruitController fruit_controller;
#endif

		private ApplicationEnumerator application_enumerator = new ApplicationEnumerator ();
		private ProcessEnumerator process_enumerator = new ProcessEnumerator ();

		public DarwinHostSession (owned DarwinHelper helper, owned TemporaryDirectory tempdir, bool report_crashes = true) {
			Object (
				helper: helper,
				tempdir: tempdir,
				report_crashes: report_crashes
			);
		}

		construct {
			helper.output.connect (on_output);
			helper.gating_cancelled.connect (on_spawn_gating_cancelled);
			helper.spawn_added.connect (on_spawn_added);
			helper.spawn_removed.connect (on_spawn_removed);

			fruitjector = new Fruitjector (helper, false, tempdir);
			injector = fruitjector;
			fruitjector.injected.connect (on_injected);
			injector.uninjected.connect (on_uninjected);

#if HAVE_EMBEDDED_ASSETS
			var blob = Frida.Data.Agent.get_frida_agent_dylib_blob ();
			agent = new AgentResource (blob.name, copy_to_aligned_pages (blob.data), tempdir);
#endif

#if IOS || TVOS
			fruit_controller = new FruitController (this, io_cancellable);
			fruit_controller.gating_cancelled.connect (on_spawn_gating_cancelled);
			fruit_controller.spawn_added.connect (on_spawn_added);
			fruit_controller.spawn_removed.connect (on_spawn_removed);
			fruit_controller.process_crashed.connect (on_process_crashed);
#endif
		}

		public override async void close (Cancellable? cancellable) throws IOError {
			yield base.close (cancellable);

#if IOS || TVOS
			yield fruit_controller.close (cancellable);
			fruit_controller.gating_cancelled.disconnect (on_spawn_gating_cancelled);
			fruit_controller.spawn_added.disconnect (on_spawn_added);
			fruit_controller.spawn_removed.disconnect (on_spawn_removed);
			fruit_controller.process_crashed.disconnect (on_process_crashed);
#endif

			var fruitjector = (Fruitjector) injector;

			yield wait_for_uninject (injector, cancellable, () => {
				return fruitjector.any_still_injected ();
			});

			fruitjector.injected.disconnect (on_injected);
			injector.uninjected.disconnect (on_uninjected);
			yield injector.close (cancellable);

			if (system_session_container != null) {
				yield system_session_container.destroy (cancellable);
				system_session_container = null;
			}

			yield helper.close (cancellable);
			helper.output.disconnect (on_output);
			helper.gating_cancelled.disconnect (on_spawn_gating_cancelled);
			helper.spawn_added.disconnect (on_spawn_added);
			helper.spawn_removed.disconnect (on_spawn_removed);

			agent = null;

			tempdir.destroy ();
		}

		protected override async AgentSessionProvider create_system_session_provider (Cancellable? cancellable,
				out DBusConnection connection) throws Error, IOError {
#if IOS || TVOS
			yield helper.preload (cancellable);

			var pid = helper.pid;

			string remote_address;
			var stream_request = yield helper.open_pipe_stream (pid, cancellable, out remote_address);

			var id = yield inject_agent (pid, remote_address, cancellable);
			injectee_by_pid[pid] = id;

			IOStream stream = yield stream_request.wait_async (cancellable);

			DBusConnection conn;
			AgentSessionProvider provider;
			try {
				conn = yield new DBusConnection (stream, ServerGuid.HOST_SESSION_SERVICE,
					AUTHENTICATION_SERVER | AUTHENTICATION_ALLOW_ANONYMOUS, null, cancellable);

				provider = yield conn.get_proxy (null, ObjectPath.AGENT_SESSION_PROVIDER, DO_NOT_LOAD_PROPERTIES,
					cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			connection = conn;

			return provider;
#else
			string path;
#if HAVE_EMBEDDED_ASSETS
			path = agent.get_file ().path;
#else
			path = agent_path;
#endif

			system_session_container = yield AgentContainer.create (path, cancellable);

			connection = system_session_container.connection;

			return system_session_container;
#endif
		}

		public override async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return System.get_frontmost_application (FrontmostQueryOptions._deserialize (options));
		}

		public override async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return yield application_enumerator.enumerate_applications (ApplicationQueryOptions._deserialize (options));
		}

		public override async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return yield process_enumerator.enumerate_processes (ProcessQueryOptions._deserialize (options));
		}

		public override async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var scope = SpawnGatingOptions._deserialize (options).scope;
#if IOS || TVOS
			if (scope == PROCESSES)
				throw new Error.NOT_SUPPORTED (
					"Process-level gating is unavailable on this OS; use the 'applications' scope");

			yield fruit_controller.enable_spawn_gating (cancellable);
#else
			yield helper.enable_spawn_gating (scope, cancellable);
#endif
		}

		public override async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
#if IOS || TVOS
			yield fruit_controller.disable_spawn_gating (cancellable);
#else
			yield helper.disable_spawn_gating (cancellable);
#endif

			spawn_gating_disabled (APPLICATION_REQUESTED);
		}

		public override async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
#if IOS || TVOS
			return fruit_controller.enumerate_pending_spawn ();
#else
			return yield helper.enumerate_pending_spawn (cancellable);
#endif
		}

		public override async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable)
				throws Error, IOError {
			string path = program;

#if MACOS
			if (!path.has_prefix ("/"))
				path = _path_for_application_identifier (path);
#elif IOS || TVOS
			if (!path.has_prefix ("/"))
				return yield fruit_controller.spawn (path, options, cancellable);
#endif

			UnixInputStream? stdin_stream = null;
			UnixOutputStream? stdout_stream = null;
			UnixOutputStream? stderr_stream = null;
			if (options.stdio == INHERIT) {
				stdin_stream = new UnixInputStream (0, false);
				stdout_stream = new UnixOutputStream (1, false);
				stderr_stream = new UnixOutputStream (2, false);
			}

			return yield helper.spawn (path, options, stdin_stream, stdout_stream, stderr_stream, cancellable);
		}

		protected override async void await_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield helper.wait_until_suspended (pid, cancellable);
			yield helper.notify_exec_completed (pid, cancellable);
		}

		protected override async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield helper.cancel_pending_waits (pid, cancellable);
		}

		protected override bool process_is_alive (uint pid) {
			return Posix.kill ((Posix.pid_t) pid, 0) == 0 || Posix.errno == Posix.EPERM;
		}

		public override async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			yield helper.input (pid, data, cancellable);
		}

		protected override async void perform_resume (uint pid, Cancellable? cancellable) throws Error, IOError {
#if IOS || TVOS
			if (yield fruit_controller.try_resume (pid, cancellable))
				return;
#endif

			yield helper.resume (pid, cancellable);
		}

		public override async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield helper.kill_process (pid, cancellable);
		}

		protected override async Future<IOStream> perform_attach_to (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable, out Object? transport) throws Error, IOError {
			transport = null;

			string remote_address;
			var stream_future = yield helper.open_pipe_stream (pid, cancellable, out remote_address);

			var id = yield inject_agent (pid, make_agent_parameters (pid, remote_address, options), cancellable);
			injectee_by_pid[pid] = id;

			return stream_future;
		}

#if IOS || TVOS
		public void activate_crash_reporter_integration () {
			fruit_controller.activate_crash_reporter_integration ();
		}

		protected override async CrashInfo? try_collect_crash (uint pid, Cancellable? cancellable) throws IOError {
			return yield fruit_controller.try_collect_crash (pid, cancellable);
		}
#endif

		private void on_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, data);
		}

		private void on_spawn_gating_cancelled () {
			spawn_gating_disabled (WATCHDOG_TIMEOUT);
		}

		private void on_spawn_added (HostSpawnInfo info) {
			spawn_added (info);
		}

		private void on_spawn_removed (HostSpawnInfo info) {
			spawn_removed (info);
		}

#if IOS || TVOS
		private void on_process_crashed (CrashInfo info) {
			process_crashed (info);
		}
#endif

		private async uint inject_agent (uint pid, string agent_parameters, Cancellable? cancellable) throws Error, IOError {
			uint id;

			unowned string entrypoint = "frida_agent_main";
#if HAVE_EMBEDDED_ASSETS
			id = yield fruitjector.inject_library_resource (pid, agent, entrypoint, agent_parameters, cancellable);
#else
			id = yield fruitjector.inject_library_file (pid, agent_path, entrypoint, agent_parameters, cancellable);
#endif

			return id;
		}

		private void on_injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module) {
#if IOS || TVOS
			DarwinModuleDetails? mapped_module_value = null;
			if (has_mapped_module)
				mapped_module_value = mapped_module;
			fruit_controller.on_agent_injected (id, pid, mapped_module_value);
#endif
		}

		protected override void on_uninjected (uint id) {
#if IOS || TVOS
			fruit_controller.on_agent_uninjected (id);
#endif

			base.on_uninjected (id);
		}

		public extern static string _path_for_application_identifier (string identifier) throws Error;

#if HAVE_EMBEDDED_ASSETS
		private static Bytes copy_to_aligned_pages (uint8[] data) {
			size_t page_size = Gum.query_page_size ();
			size_t size = data.length;
			uint8 * pages = (uint8 *) Gum.memory_allocate (null, size, page_size, READ | WRITE);
			Memory.copy (pages, data, size);
			unowned uint8[] view = (uint8[]) pages;
			view.length = (int) size;
			return Bytes.new_with_owner<AlignedPages> (view, new AlignedPages (pages, size));
		}

		private class AlignedPages {
			private uint8 * pages;
			private size_t size;

			public AlignedPages (uint8 * pages, size_t size) {
				this.pages = pages;
				this.size = size;
			}

			~AlignedPages () {
				Gum.memory_free (pages, size);
			}
		}
#endif
	}

#if IOS || TVOS
	private sealed class FruitController : Object, MappedAgentContainer {
		public signal void gating_cancelled ();
		public signal void spawn_added (HostSpawnInfo info);
		public signal void spawn_removed (HostSpawnInfo info);
		public signal void process_crashed (CrashInfo crash);

		public weak DarwinHostSession host_session {
			get;
			construct;
		}

		public DarwinHelper helper {
			get;
			construct;
		}

		public Cancellable io_cancellable {
			get;
			construct;
		}

		private LaunchdAgent launchd_agent;

		private bool spawn_gating_enabled = false;
		private Gee.HashMap<string, Promise<uint>> spawn_requests = new Gee.HashMap<string, Promise<uint>> ();
		private Gee.HashMap<uint, HostSpawnInfo?> pending_spawn = new Gee.HashMap<uint, HostSpawnInfo?> ();
		private SpawnGatingWatchdog watchdog = new SpawnGatingWatchdog ();

		private CrashReporterState crash_reporter_state;
		private Gee.HashMap<uint, ReportCrashAgent> crash_agents = new Gee.HashMap<uint, ReportCrashAgent> ();
		private Gee.HashMap<uint, OSAnalyticsAgent> osa_agents = new Gee.HashMap<uint, OSAnalyticsAgent> ();
		private Gee.HashMap<uint, CrashDelivery> crash_deliveries = new Gee.HashMap<uint, CrashDelivery> ();
		private Gee.HashMap<uint, MappedAgent> mapped_agents = new Gee.HashMap<uint, MappedAgent> ();
		private Gee.HashMap<MappedAgent, Source> mapped_agents_dying = new Gee.HashMap<MappedAgent, Source> ();

		private Gee.HashSet<uint> xpcproxies = new Gee.HashSet<uint> ();

		private enum CrashReporterState {
			DISABLED,
			INACTIVE,
			ACTIVE,
		}

		public FruitController (DarwinHostSession host_session, Cancellable io_cancellable) {
			Object (
				host_session: host_session,
				helper: host_session.helper,
				io_cancellable: io_cancellable
			);
		}

		construct {
			crash_reporter_state = host_session.report_crashes
				? CrashReporterState.INACTIVE
				: CrashReporterState.DISABLED;

			launchd_agent = new LaunchdAgent (host_session, io_cancellable);
			launchd_agent.app_launch_started.connect (on_app_launch_started);
			launchd_agent.app_launch_completed.connect (on_app_launch_completed);
			launchd_agent.spawn_preparation_started.connect (on_spawn_preparation_started);
			launchd_agent.spawn_preparation_aborted.connect (on_spawn_preparation_aborted);
			launchd_agent.spawn_captured.connect (on_spawn_captured);

			helper.process_resumed.connect (on_process_resumed);
			helper.process_killed.connect (on_process_killed);

			watchdog.expired.connect (on_watchdog_expired);
		}

		~FruitController () {
			launchd_agent.spawn_captured.disconnect (on_spawn_captured);
			launchd_agent.spawn_preparation_aborted.disconnect (on_spawn_preparation_aborted);
			launchd_agent.spawn_preparation_started.disconnect (on_spawn_preparation_started);
			launchd_agent.app_launch_completed.disconnect (on_app_launch_completed);
			launchd_agent.app_launch_started.disconnect (on_app_launch_started);

			helper.process_resumed.disconnect (on_process_resumed);
			helper.process_killed.disconnect (on_process_killed);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (spawn_gating_enabled) {
				try {
					yield disable_spawn_gating (cancellable);
				} catch (Error e) {
				}
			}

			yield launchd_agent.close (cancellable);
		}

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield launchd_agent.enable_spawn_gating (cancellable);

			spawn_gating_enabled = true;
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			spawn_gating_enabled = false;

			watchdog.clear ();

			yield launchd_agent.disable_spawn_gating (cancellable);

			var pending = pending_spawn.values.to_array ();
			pending_spawn.clear ();
			foreach (var spawn in pending) {
				spawn_removed (spawn);

				helper.resume.begin (spawn.pid, io_cancellable);
			}
		}

		public HostSpawnInfo[] enumerate_pending_spawn () {
			var result = new HostSpawnInfo[pending_spawn.size];
			var index = 0;
			foreach (var spawn in pending_spawn.values)
				result[index++] = spawn;
			return result;
		}

		public async uint spawn (string identifier, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			if (spawn_requests.has_key (identifier))
				throw new Error.INVALID_OPERATION ("Spawn already in progress for the specified identifier");

			var request = new Promise<uint> ();
			spawn_requests[identifier] = request;

			uint pid = 0;
			try {
				yield launchd_agent.prepare_for_launch (identifier, cancellable);

				yield helper.launch (identifier, options, cancellable);

				var timeout = new TimeoutSource.seconds (20);
				timeout.set_callback (() => {
					request.reject (new Error.TIMED_OUT ("Unexpectedly timed out while waiting for app to launch"));
					return false;
				});
				timeout.attach (MainContext.get_thread_default ());
				try {
					pid = yield request.future.wait_async (cancellable);
				} finally {
					timeout.destroy ();
				}

				helper.notify_launch_completed.begin (identifier, pid, io_cancellable);
			} catch (GLib.Error e) {
				launchd_agent.cancel_launch.begin (identifier, io_cancellable);

				if (!spawn_requests.unset (identifier)) {
					var pending_pid = request.future.value;
					if (pending_pid != 0)
						helper.resume.begin (pending_pid, io_cancellable);
				}

				throw_api_error (e);
			}

			return pid;
		}

		public async bool try_resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			HostSpawnInfo? info;
			if (!pending_spawn.unset (pid, out info))
				return false;
			watchdog.cancel (pid);
			spawn_removed (info);

			yield helper.resume (pid, cancellable);

			return true;
		}

		public void activate_crash_reporter_integration () {
			if (crash_reporter_state != INACTIVE)
				return;

			foreach (var process in System.enumerate_processes (new ProcessQueryOptions ())) {
				if (is_osanalytics_process (process)) {
					var agent = try_add_osanalytics_agent (process.pid);
					if (agent != null)
						try_start_osanalytics_agent.begin (agent);
				}
			}

			launchd_agent.activate_crash_reporter_integration ();

			crash_reporter_state = ACTIVE;
		}

		public async CrashInfo? try_collect_crash (uint pid, Cancellable? cancellable) throws IOError {
			if (crash_reporter_state == DISABLED)
				return null;

			if (crash_agents.has_key (pid) || xpcproxies.contains (pid))
				return null;

			var delivery = get_crash_delivery_for_pid (pid);
			try {
				return yield delivery.future.wait_async (cancellable);
			} catch (Error e) {
				return null;
			}
		}

		public void enumerate_mapped_agents (FoundMappedAgentFunc func) {
			foreach (var mapped_agent in mapped_agents.values)
				func (mapped_agent);
		}

		public void on_agent_injected (uint id, uint pid, DarwinModuleDetails? mapped_module) {
			if (mapped_module == null)
				return;

			var dead_agents = new Gee.ArrayList<MappedAgent> ();
			foreach (var agent in mapped_agents_dying.keys) {
				if (agent.pid == pid)
					dead_agents.add (agent);
			}
			foreach (var agent in dead_agents) {
				Source source;
				mapped_agents_dying.unset (agent, out source);
				source.destroy ();

				mapped_agents.unset (agent.id);
			}

			mapped_agents[id] = new MappedAgent (id, pid, mapped_module);
		}

		public void on_agent_uninjected (uint id) {
			var agent = mapped_agents[id];
			if (agent == null)
				return;

			var timeout = new TimeoutSource.seconds (20);
			timeout.set_callback (() => {
				mapped_agents.unset (id);
				return false;
			});
			timeout.attach (MainContext.get_thread_default ());
			mapped_agents_dying[agent] = timeout;
		}

		private void on_app_launch_started (string identifier, uint pid) {
			xpcproxies.add (pid);
		}

		private void on_app_launch_completed (string identifier, uint pid, GLib.Error? error) {
			xpcproxies.remove (pid);

			Promise<uint> request;
			if (spawn_requests.unset (identifier, out request)) {
				if (error == null)
					request.resolve (pid);
				else
					request.reject (error);
			} else {
				if (error == null)
					helper.resume.begin (pid, io_cancellable);
			}
		}

		private void on_spawn_preparation_started (HostSpawnInfo info) {
			xpcproxies.add (info.pid);

			if (is_reportcrash_spawn (info)) {
				add_crash_reporter_agent (info.pid);
				return;
			}

			if (is_osanalytics_spawn (info)) {
				try_add_osanalytics_agent (info.pid);
				return;
			}
		}

		private void on_spawn_preparation_aborted (HostSpawnInfo info) {
			var pid = info.pid;
			xpcproxies.remove (pid);
			crash_agents.unset (pid);
			osa_agents.unset (pid);
		}

		private void on_spawn_captured (HostSpawnInfo info) {
			xpcproxies.remove (info.pid);
			launchd_agent.unclaim_process.begin (info.pid, io_cancellable);
			handle_spawn.begin (info);
		}

		private void on_process_resumed (uint pid) {
			launchd_agent.claim_process.begin (pid, io_cancellable);
		}

		private void on_process_killed (uint pid) {
			launchd_agent.claim_process.begin (pid, io_cancellable);
		}

		private void on_watchdog_expired () {
			disable_spawn_gating.begin (io_cancellable);
			gating_cancelled ();
		}

		private async void handle_spawn (HostSpawnInfo info) {
			try {
				var pid = info.pid;

				var crash_agent = crash_agents[pid];
				if (crash_agent != null)
					yield try_start_crash_reporter_agent (crash_agent);

				var osa_agent = osa_agents[pid];
				if (osa_agent != null)
					yield try_start_osanalytics_agent (osa_agent);

				if (!spawn_gating_enabled) {
					yield helper.resume (pid, io_cancellable);
					return;
				}

				pending_spawn[pid] = info;
				watchdog.arm (pid);
				spawn_added (info);
			} catch (GLib.Error e) {
			}
		}

		private ReportCrashAgent add_crash_reporter_agent (uint pid) {
			foreach (var delivery in crash_deliveries.values)
				delivery.extend_timeout (5000);

			var agent = new ReportCrashAgent (host_session, pid, this, io_cancellable);
			crash_agents[pid] = agent;

			agent.unloaded.connect (on_crash_agent_unloaded);
			agent.crash_detected.connect (on_crash_detected);
			agent.crash_received.connect (on_crash_received);

			return agent;
		}

		private async void try_start_crash_reporter_agent (ReportCrashAgent agent) {
			try {
				yield agent.start (io_cancellable);
			} catch (GLib.Error e) {
				crash_agents.unset (agent.pid);
			}
		}

		private void on_crash_agent_unloaded (InternalAgent agent) {
			var crash_agent = (ReportCrashAgent) agent;
			crash_agents.unset (crash_agent.pid);
		}

		private void on_crash_detected (ReportCrashAgent agent, uint pid) {
			var delivery = get_crash_delivery_for_pid (pid);
			delivery.extend_timeout ();
		}

		private void on_crash_received (ReportCrashAgent agent, CrashInfo crash) {
			var delivery = get_crash_delivery_for_pid (crash.pid);
			delivery.complete (crash);

			process_crashed (crash);
		}

		private OSAnalyticsAgent? try_add_osanalytics_agent (uint pid) {
			if (osa_agents.has_key (pid))
				return null;

			var agent = new OSAnalyticsAgent (host_session, pid, io_cancellable);
			osa_agents[pid] = agent;

			agent.unloaded.connect (on_osa_agent_unloaded);

			return agent;
		}

		private async void try_start_osanalytics_agent (OSAnalyticsAgent agent) {
			try {
				yield agent.start (io_cancellable);
			} catch (GLib.Error e) {
				osa_agents.unset (agent.pid);
			}
		}

		private void on_osa_agent_unloaded (InternalAgent agent) {
			var osa_agent = (OSAnalyticsAgent) agent;
			osa_agents.unset (osa_agent.pid);
		}

		private static bool is_reportcrash_spawn (HostSpawnInfo info) {
			return info.identifier == "com.apple.ReportCrash";
		}

		private static bool is_osanalytics_spawn (HostSpawnInfo info) {
			return info.identifier == "com.apple.osanalytics.osanalyticshelper";
		}

		private static bool is_osanalytics_process (HostProcessInfo info) {
			return info.name == "osanalyticshelper";
		}

		private CrashDelivery get_crash_delivery_for_pid (uint pid) {
			var delivery = crash_deliveries[pid];
			if (delivery == null) {
				delivery = new CrashDelivery (pid);
				delivery.expired.connect (on_crash_delivery_expired);
				crash_deliveries[pid] = delivery;
			}
			return delivery;
		}

		private void on_crash_delivery_expired (CrashDelivery delivery) {
			crash_deliveries.unset (delivery.pid);
		}

		private sealed class CrashDelivery : Object {
			public signal void expired ();

			public uint pid {
				get;
				construct;
			}

			public Future<CrashInfo?> future {
				get {
					return promise.future;
				}
			}

			private Promise<CrashInfo?> promise = new Promise<CrashInfo?> ();
			private TimeoutSource expiry_source;

			public CrashDelivery (uint pid) {
				Object (pid: pid);
			}

			construct {
				expiry_source = make_expiry_source (500);
			}

			private TimeoutSource make_expiry_source (uint timeout) {
				var source = new TimeoutSource (timeout);
				source.set_callback (on_timeout);
				source.attach (MainContext.get_thread_default ());
				return source;
			}

			public void extend_timeout (uint timeout = 20000) {
				if (future.ready)
					return;

				expiry_source.destroy ();
				expiry_source = make_expiry_source (timeout);
			}

			public void complete (CrashInfo? crash) {
				if (future.ready)
					return;

				promise.resolve (crash);

				expiry_source.destroy ();
				expiry_source = make_expiry_source (1000);
			}

			private bool on_timeout () {
				if (!future.ready)
					promise.reject (new Error.TIMED_OUT ("Crash delivery timed out"));

				expired ();

				return false;
			}
		}
	}

	private interface MappedAgentContainer : Object {
		public abstract void enumerate_mapped_agents (FoundMappedAgentFunc func);
	}

	private delegate void FoundMappedAgentFunc (MappedAgent agent);

	private sealed class MappedAgent {
		public uint id {
			get;
			private set;
		}

		public uint pid {
			get;
			private set;
		}

		public DarwinModuleDetails module {
			get;
			private set;
		}

		public MappedAgent (uint id, uint pid, DarwinModuleDetails module) {
			this.id = id;
			this.pid = pid;
			this.module = module;
		}
	}

	private sealed class LaunchdAgent : InternalAgent {
		public signal void app_launch_started (string identifier, uint pid);
		public signal void app_launch_completed (string identifier, uint pid, GLib.Error? error);
		public signal void spawn_preparation_started (HostSpawnInfo info);
		public signal void spawn_preparation_aborted (HostSpawnInfo info);
		public signal void spawn_captured (HostSpawnInfo info);

		private const string XPC_PROXY_PATH = "/usr/libexec/xpcproxy";

		public Cancellable io_cancellable {
			get;
			construct;
		}

		public LaunchdAgent (HostSessionConnection connection, Cancellable io_cancellable) {
			Object (connection: connection, io_cancellable: io_cancellable);
		}

		construct {
			attach_options["exit-monitor"] = "off";
			attach_options["thread-suspend-monitor"] = "off";
		}

		public void activate_crash_reporter_integration () {
			ensure_loaded.begin (io_cancellable);
		}

		public async void prepare_for_launch (string identifier, Cancellable? cancellable) throws Error, IOError {
			yield call ("prepareForLaunch", new Json.Node[] { new Json.Node.alloc ().init_string (identifier) }, null,
				cancellable);
		}

		public async void cancel_launch (string identifier, Cancellable? cancellable) throws Error, IOError {
			yield call ("cancelLaunch", new Json.Node[] { new Json.Node.alloc ().init_string (identifier) }, null, cancellable);
		}

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield call ("enableSpawnGating", new Json.Node[] {}, null, cancellable);
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield call ("disableSpawnGating", new Json.Node[] {}, null, cancellable);
		}

		public async void claim_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield call ("claimProcess", new Json.Node[] { new Json.Node.alloc ().init_int (pid) }, null, cancellable);
		}

		public async void unclaim_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield call ("unclaimProcess", new Json.Node[] { new Json.Node.alloc ().init_int (pid) }, null, cancellable);
		}

		protected override void on_event (string type, Json.Array event) {
			var path = event.get_string_element (1);
			var identifier = event.get_string_element (2);
			var pid = (uint) event.get_int_element (3);

			switch (type) {
				case "launch:app":
					prepare_app.begin (path, identifier, pid);
					break;
				case "spawn":
					prepare_spawn.begin (path, identifier, pid);
					break;
				default:
					assert_not_reached ();
			}
		}

		private async void prepare_app (string path, string identifier, uint pid) {
			app_launch_started (identifier, pid);

			try {
				if (path == XPC_PROXY_PATH) {
					var agent = new XpcProxyAgent (connection, identifier, pid);
					yield agent.run_until_exec (io_cancellable);
				}

				app_launch_completed (identifier, pid, null);
			} catch (GLib.Error e) {
				app_launch_completed (identifier, pid, e);
			}
		}

		private async void prepare_spawn (string path, string identifier, uint pid) {
			var info = HostSpawnInfo (pid, identifier);
			spawn_preparation_started (info);

			try {
				if (path == XPC_PROXY_PATH) {
					var agent = new XpcProxyAgent (connection, identifier, pid);
					yield agent.run_until_exec (io_cancellable);
				}

				spawn_captured (info);
			} catch (GLib.Error e) {
				spawn_preparation_aborted (info);
			}
		}

		protected override async uint get_target_pid (Cancellable? cancellable) throws Error, IOError {
			return 1;
		}

		protected override async string? load_source (Cancellable? cancellable) throws Error, IOError {
			string * raw_source = Frida.Data.Darwin.get_launchd_js_blob ().data;
			return raw_source->replace ("@REPORT_CRASHES@",
				((DarwinHostSession) connection.host_session).report_crashes.to_string ());
		}
	}

	private sealed class XpcProxyAgent : InternalAgent {
		public string identifier {
			get;
			construct;
		}

		public uint pid {
			get;
			construct;
		}

		public XpcProxyAgent (HostSessionConnection connection, string identifier, uint pid) {
			Object (connection: connection, identifier: identifier, pid: pid);
		}

		construct {
			attach_options["exceptor"] = "off";
			attach_options["exit-monitor"] = "off";
			attach_options["thread-suspend-monitor"] = "off";
		}

		public async void run_until_exec (Cancellable? cancellable) throws Error, IOError {
			yield ensure_loaded (cancellable);

			var helper = ((DarwinHostSession) connection.host_session).helper;
			yield helper.resume (pid, cancellable);

			yield wait_for_unload (cancellable);

			yield helper.wait_until_suspended (pid, cancellable);
			yield helper.notify_exec_completed (pid, cancellable);
		}

		protected override async uint get_target_pid (Cancellable? cancellable) throws Error, IOError {
			return pid;
		}

		protected override async string? load_source (Cancellable? cancellable) throws Error, IOError {
			return (string) Frida.Data.Darwin.get_xpcproxy_js_blob ().data;
		}
	}

	private sealed class ReportCrashAgent : InternalAgent {
		public signal void crash_detected (uint pid);
		public signal void crash_received (CrashInfo crash);

		public uint pid {
			get;
			construct;
		}

		public MappedAgentContainer mapped_agent_container {
			get;
			construct;
		}

		public Cancellable io_cancellable {
			get;
			construct;
		}

		public ReportCrashAgent (HostSessionConnection connection, uint pid, MappedAgentContainer mapped_agent_container,
				Cancellable io_cancellable) {
			Object (
				connection: connection,
				pid: pid,
				mapped_agent_container: mapped_agent_container,
				io_cancellable: io_cancellable
			);
		}

		construct {
			attach_options["exceptor"] = "off";
			attach_options["exit-monitor"] = "off";
			attach_options["thread-suspend-monitor"] = "off";
		}

		public async void start (Cancellable? cancellable) throws Error, IOError {
			yield ensure_loaded (cancellable);
		}

		protected override void on_event (string type, Json.Array event) {
			switch (type) {
				case "crash-detected":
					var pid = (uint) event.get_int_element (1);

					send_mapped_agents (pid);

					crash_detected (pid);

					break;
				case "crash-received":
					var pid = (uint) event.get_int_element (1);
					var raw_report = event.get_string_element (2);

					var crash = parse_report (pid, raw_report);
					crash_received (crash);

					break;
				default:
					assert_not_reached ();
			}
		}

		private static CrashInfo parse_report (uint pid, string raw_report) {
			var tokens = raw_report.split ("\n", 2);
			var raw_header = tokens[0];
			var report = tokens[1];

			var parameters = make_parameters_dict ();
			try {
				var header = make_json_reader (raw_header);
				foreach (string member in header.list_members ()) {
					header.read_member (member);

					Variant? val = null;
					if (header.is_value ()) {
						Json.Node node = header.get_value ();
						Type t = node.get_value_type ();
						if (t == typeof (string))
							val = new Variant.string (node.get_string ());
						else if (t == typeof (int64))
							val = new Variant.int64 (node.get_int ());
						else if (t == typeof (bool))
							val = new Variant.boolean (node.get_boolean ());
					}

					if (val != null)
						parameters[canonicalize_parameter_name (member)] = val;

					header.end_member ();
				}
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			Variant? name_val = parameters["name"];
			assert (name_val != null && name_val.is_of_type (VariantType.STRING));
			string process_name = name_val.get_string ();
			assert (process_name != null);

			string summary = summarize (report);

			return CrashInfo (pid, process_name, summary, report, parameters);
		}

		private static string summarize (string report) {
			MatchInfo info;

			string? exception_type = null;
			if (/^Exception Type: +(.+)$/m.match (report, 0, out info)) {
				exception_type = info.fetch (1);
			}

			string? exception_subtype = null;
			if (/^Exception Subtype: +(.+)$/m.match (report, 0, out info)) {
				exception_subtype = info.fetch (1);
			}

			string? signal_description = null;
			if (/^Termination Signal: +(.+): \d+$/m.match (report, 0, out info)) {
				signal_description = info.fetch (1);
			}

			string? reason_namespace = null;
			string? reason_code = null;
			if (/^Termination Reason: +Namespace (.+), Code (.+)$/m.match (report, 0, out info)) {
				reason_namespace = info.fetch (1);
				reason_code = info.fetch (2);
			} else {
				reason_namespace = "SIGNAL";
				reason_code = "unknown";
			}

			if (reason_namespace == null)
				return "Unknown error";

			if (reason_namespace == "SIGNAL") {
				if (exception_subtype != null) {
					string? problem = null;
					if (exception_type != null && /^EXC_(\w+)/.match (exception_type, 0, out info)) {
						string raw_problem = info.fetch (1).replace ("_", " ");
						problem = "%c%s".printf (raw_problem[0].toupper (), raw_problem.substring (1).down ());
					}

					string? cause = null;
					if (/^KERN_(.+) at /.match (exception_subtype, 0, out info)) {
						cause = info.fetch (1).replace ("_", " ").down ();
					}

					if (problem != null && cause != null)
						return "%s due to %s".printf (problem, cause);
				}

				if (signal_description != null)
					return signal_description;

				if (exception_type != null && / \((SIG\w+)\)/.match (exception_type, 0, out info)) {
					return info.fetch (1);
				}
			}

			if (reason_namespace == "CODESIGNING")
				return "Codesigning violation";

			if (reason_namespace == "JETSAM" && exception_subtype != null)
				return "Jetsam %s budget exceeded".printf (exception_subtype.down ());

			return "Unknown %s error %s".printf (reason_namespace.down (), reason_code);
		}

		private void send_mapped_agents (uint pid) {
			var stanza = new Json.Builder ();
			stanza
				.begin_object ()
				.set_member_name ("type")
				.add_string_value ("mapped-agents")
				.set_member_name ("payload")
				.begin_array ();
			mapped_agent_container.enumerate_mapped_agents (agent => {
				if (agent.pid == pid) {
					DarwinModuleDetails details = agent.module;

					stanza
						.begin_object ()
						.set_member_name ("machHeaderAddress")
						.add_string_value (details.mach_header_address.to_string ())
						.set_member_name ("uuid")
						.add_string_value (details.uuid)
						.set_member_name ("path")
						.add_string_value (details.path)
						.end_object ();
				}
			});
			stanza
				.end_array ()
				.end_object ();
			string json = Json.to_string (stanza.get_root (), false);

			session.post_messages.begin ({ AgentMessage (SCRIPT, script, json, false, {}) }, 0, io_cancellable);
		}

		private static string canonicalize_parameter_name (string name) {
			var result = new StringBuilder ();

			unichar c;
			bool need_dash = true;
			for (int i = 0; name.get_next_char (ref i, out c);) {
				if (c.isupper ()) {
					if (i != 0 && need_dash) {
						result.append_c ('-');
						need_dash = false;
					}

					c = c.tolower ();
				} else {
					need_dash = true;
				}

				result.append_unichar (c);
			}

			return result.str;
		}

		protected override async uint get_target_pid (Cancellable? cancellable) throws Error, IOError {
			return pid;
		}

		protected override async string? load_source (Cancellable? cancellable) throws Error, IOError {
			return (string) Frida.Data.Darwin.get_reportcrash_js_blob ().data;
		}
	}

	private sealed class OSAnalyticsAgent : InternalAgent {
		public uint pid {
			get;
			construct;
		}

		public Cancellable io_cancellable {
			get;
			construct;
		}

		public OSAnalyticsAgent (HostSessionConnection connection, uint pid, Cancellable io_cancellable) {
			Object (
				connection: connection,
				pid: pid,
				io_cancellable: io_cancellable
			);
		}

		construct {
			attach_options["exceptor"] = "off";
			attach_options["exit-monitor"] = "off";
			attach_options["thread-suspend-monitor"] = "off";
		}

		public async void start (Cancellable? cancellable) throws Error, IOError {
			yield ensure_loaded (cancellable);
		}

		protected override void on_event (string type, Json.Array event) {
		}

		protected override async uint get_target_pid (Cancellable? cancellable) throws Error, IOError {
			return pid;
		}

		protected override async string? load_source (Cancellable? cancellable) throws Error, IOError {
			return (string) Frida.Data.Darwin.get_osanalytics_js_blob ().data;
		}
	}
#endif
}
