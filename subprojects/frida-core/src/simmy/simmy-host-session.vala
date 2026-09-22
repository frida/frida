namespace Frida {
	public sealed class SimmyHostSessionBackend : Object, HostSessionBackend {
		private Gee.Map<string, SimmyHostSessionProvider> providers = new Gee.HashMap<string, SimmyHostSessionProvider> ();

		private void * simmy_context;
		private MainContext main_context;

		private Cancellable io_cancellable = new Cancellable ();

		construct {
			main_context = MainContext.ref_thread_default ();
		}

		public async void start (Cancellable? cancellable) throws IOError {
			simmy_context = _start (on_device_added, () => {
				schedule_on_frida_thread (start.callback);
			});
			yield;
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			_stop (simmy_context, () => {
				schedule_on_frida_thread (stop.callback);
			});
			yield;
			simmy_context = null;

			io_cancellable.cancel ();

			foreach (var provider in providers.values) {
				provider_unavailable (provider);
				yield provider.close (cancellable);
			}
			providers.clear ();
		}

		private void on_device_added (Simmy.Device device) {
			schedule_on_frida_thread (() => {
				var prov = new SimmyHostSessionProvider (device);
				providers[device.udid] = prov;
				provider_available (prov);

				return Source.REMOVE;
			});
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}

		public extern static void * _start (DeviceAddedFunc on_device_added, owned Simmy.CompleteFunc on_complete);
		public extern static void _stop (void * simmy_context, owned Simmy.CompleteFunc on_complete);

		public delegate void DeviceAddedFunc (Simmy.Device device);
	}

	public sealed class SimmyHostSessionProvider : Object, HostSessionProvider {
		public Simmy.Device device {
			get;
			construct;
		}

		public string id {
			get { return device.udid; }
		}

		public string name {
			get { return device.name; }
		}

		public Variant? icon {
			get { return _icon; }
		}

		public HostSessionProviderKind kind {
			get {
				return HostSessionProviderKind.REMOTE;
			}
		}

		private static Variant _icon;
		private SimmyHostSession? host_session;

		static construct {
			_icon = make_provider_icon (Frida.Data.Icons.get_simmy_png_blob ().data);
		}

		public SimmyHostSessionProvider (Simmy.Device device) {
			Object (device: device);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (host_session == null)
				return;

			host_session.agent_session_detached.disconnect (on_agent_session_detached);

			yield host_session.close (cancellable);
			host_session = null;
		}

		public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != null)
				throw new Error.INVALID_OPERATION ("Already created");

			HostSessionEntry local_system = yield hub.resolve_host_session ("local", cancellable);

			host_session = new SimmyHostSession (device, local_system);
			host_session.agent_session_detached.connect (on_agent_session_detached);

			return host_session;
		}

		public async void destroy (HostSession session, Cancellable? cancellable) throws Error, IOError {
			if (session != host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			host_session.agent_session_detached.disconnect (on_agent_session_detached);

			yield host_session.close (cancellable);
			host_session = null;
		}

		public async AgentSession link_agent_session (HostSession host_session, AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != this.host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			return yield this.host_session.link_agent_session (id, sink, cancellable);
		}

		public void unlink_agent_session (HostSession host_session, AgentSessionId id) {
			if (host_session != this.host_session)
				return;

			this.host_session.unlink_agent_session (id);
		}

		public async IOStream link_channel (HostSession host_session, ChannelId id, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
		}

		public void unlink_channel (HostSession host_session, ChannelId id) {
		}

		public async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Services are not supported by this backend");
		}

		public void unlink_service_session (HostSession host_session, ServiceSessionId id) {
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			agent_session_detached (id, reason, crash);
		}
	}

	public sealed class SimmyHostSession : Object, HostSession, HostSessionConnection {
		public Simmy.Device device {
			get;
			construct;
		}

		public HostSessionEntry local_system {
			get;
			construct;
		}

		public HostSession host_session {
			get {
				return this;
			}
		}

		private Gee.Map<uint, Simmy.SpawnedProcess> spawned_processes = new Gee.HashMap<uint, Simmy.SpawnedProcess> ();

		private SpringboardAgent springboard_agent;

		private Cancellable io_cancellable = new Cancellable ();

		public SimmyHostSession (Simmy.Device device, HostSessionEntry local_system) {
			Object (device: device, local_system: local_system);
		}

		construct {
			var s = local_system.session;
			s.process_crashed.connect (on_process_crashed);
			s.agent_session_detached.connect (on_agent_session_detached);
			s.uninjected.connect (on_uninjected);

			springboard_agent = new SpringboardAgent (this);
			springboard_agent.unloaded.connect (on_springboard_agent_unloaded);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			springboard_agent.unloaded.disconnect (on_springboard_agent_unloaded);
			yield springboard_agent.close (cancellable);

			io_cancellable.cancel ();
		}

		public async void ping (uint interval_seconds, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable) throws Error, IOError {
			var parameters = new HashTable<string, Variant> (str_hash, str_equal);

			var runtime = device.runtime;
			unowned string os_name = runtime.short_name;

			var os = new HashTable<string, Variant> (str_hash, str_equal);
			os["id"] = os_name.down ();
			os["name"] = os_name.replace ("iOS", "iPhone OS");
			os["version"] = runtime.version_string;
			parameters["os"] = os;

			parameters["platform"] = "darwin";

			parameters["arch"] = "arm64";

			var hardware = new HashTable<string, Variant> (str_hash, str_equal);
			hardware["product"] = device.model_identifier;
			parameters["hardware"] = hardware;

			parameters["access"] = "full";

			parameters["name"] = device.name;
			parameters["udid"] = device.udid;

			return parameters;
		}

		public async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var opts = FrontmostQueryOptions._deserialize (options);

			try {
				return yield springboard_agent.get_frontmost_application (opts, cancellable);
			} catch (Error e) {
				if (e is Error.PERMISSION_DENIED)
					throw new Error.NOT_SUPPORTED ("Frontmost query is only supported with SIP disabled (for now)");
				throw e;
			}
		}

		public async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var opts = ApplicationQueryOptions._deserialize (options);
			var scope = opts.scope;

			Future<Gee.List<Simmy.Application>> list_apps_request = device.list_applications ();
			Future<Gee.List<LaunchdJob>> list_jobs_request = list_launchd_jobs (cancellable);

			var selected_ids = new Gee.HashSet<string> ();
			opts.enumerate_selected_identifiers (identifier => {
				selected_ids.add (identifier);
			});
			bool include_all_apps = selected_ids.is_empty;

			var apps = new Gee.ArrayList<Simmy.Application> ();
			foreach (Simmy.Application app in yield list_apps_request.wait_async (cancellable)) {
				unowned string id = app.identifier;
				if (include_all_apps || id in selected_ids)
					apps.add (app);
			}

			Gee.Map<string, Bytes>? icons = null;
			if (scope == FULL) {
				var app_ids = new Gee.ArrayList<string> ();
				foreach (var app in apps)
					app_ids.add (app.identifier);
				try {
					icons = yield springboard_agent.fetch_application_icons (app_ids, cancellable);
				} catch (Error e) {
				}
			}

			var pids = new Gee.HashMap<string, uint> ();
			foreach (var job in yield list_jobs_request.wait_async (cancellable)) {
				if (job.pid == 0)
					continue;

				unowned string label = job.label;
				if (!label.has_prefix ("UIKitApplication:"))
					continue;
				string identifier = label[17:].split ("[", 2)[0];

				pids[identifier] = job.pid;
			}

			var result = new HostApplicationInfo[0];
			foreach (var app in apps) {
				unowned string identifier = app.identifier;

				var info = HostApplicationInfo (identifier, app.display_name, pids[identifier], make_parameters_dict ());
				if (scope == FULL && icons != null)
					add_app_icons (info.parameters, icons[identifier]);
				result += info;
			}
			return result;
		}

		public async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options, Cancellable? cancellable)
				throws Error, IOError {
			var opts = ProcessQueryOptions._deserialize (options);

			if (!opts.has_selected_pids ()) {
				foreach (var job in yield list_launchd_jobs (cancellable).wait_async (cancellable)) {
					if (job.pid != 0)
						opts.select_pid (job.pid);
				}
			}

			try {
				return yield local_system.session.enumerate_processes (opts._serialize (), cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}
		}

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			Future<Simmy.SpawnedProcess> f;
			if (program[0] == '/')
				f = device.spawn_program (program, options);
			else
				f = device.launch_application (program, options);
			var process = yield f.wait_async (cancellable);
			uint pid = process.pid;

			if (options.stdio == PIPE) {
				spawned_processes[pid] = process;
				process.terminated.connect (on_spawned_process_terminated);
				process.output.connect (on_spawned_process_output);
			}

			return pid;
		}

		private void on_spawned_process_terminated (Simmy.SpawnedProcess process, int status) {
			spawned_processes.unset (process.pid);
		}

		private void on_spawned_process_output (Simmy.SpawnedProcess process, int fd, uint8[] data) {
			output (process.pid, fd, data);

			if (data.length == 0)
				spawned_processes.unset (process.pid);
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			var process = spawned_processes[pid];
			if (process == null)
				throw new Error.INVALID_ARGUMENT ("Invalid PID");
			yield process.input (data, cancellable);
		}

		public async void resume (uint pid, Cancellable? cancellable) throws GLib.Error {
			yield local_system.session.resume (pid, cancellable);
		}

		public async void kill (uint pid, Cancellable? cancellable) throws GLib.Error {
			yield local_system.session.kill (pid, cancellable);
		}

		public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable) throws GLib.Error {
			return yield local_system.session.attach (pid, options, cancellable);
		}

		public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		public async AgentSession link_agent_session (AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			return yield local_system.provider.link_agent_session (local_system.session, id, sink, cancellable);
		}

		public void unlink_agent_session (AgentSessionId id) {
			local_system.provider.unlink_agent_session (local_system.session, id);
		}

		public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable) throws GLib.Error {
			return yield local_system.session.inject_library_file (pid, path, entrypoint, data, cancellable);
		}

		public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
				Cancellable? cancellable) throws GLib.Error {
			return yield local_system.session.inject_library_blob (pid, blob, entrypoint, data, cancellable);
		}

		public async ChannelId open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public IOStream link_channel (ChannelId id) throws Error {
			throw_not_supported ();
		}

		public void unlink_channel (ChannelId id) {
			assert_not_reached ();
		}

		public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public ServiceSession link_service_session (ServiceSessionId id) throws Error {
			throw_not_supported ();
		}

		public void unlink_service_session (ServiceSessionId id) {
			assert_not_reached ();
		}

		private void on_process_crashed (CrashInfo crash) {
			process_crashed (crash);
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			agent_session_detached (id, reason, crash);
		}

		private void on_uninjected (InjectorPayloadId id) {
			uninjected (id);
		}

		internal Future<Gee.List<LaunchdJob>> list_launchd_jobs (Cancellable? cancellable) {
			var promise = new Promise<Gee.List<LaunchdJob>> ();
			perform_list_launchd_jobs.begin (cancellable, promise);
			return promise.future;
		}

		private async void perform_list_launchd_jobs (Cancellable? cancellable, Promise<Gee.List<LaunchdJob>> request) {
			try {
				RunResult res = yield run_and_capture_output ({
					Path.build_filename (device.runtime.root, "bin", "launchctl"),
					"list"
				}, cancellable).wait_async (cancellable);

				res.check ();

				var jobs = new Gee.ArrayList<LaunchdJob> ();
				string[] lines = res.standard_output.chomp ().split ("\n");
				foreach (string line in lines[1:]) {
					string[] tokens = line.split ("\t", 3);
					if (tokens.length != 3)
						throw new Error.PROTOCOL ("Unexpected launchctl output");

					unowned string raw_pid = tokens[0];
					unowned string label = tokens[2];

					jobs.add (new LaunchdJob () {
						pid = uint.parse (raw_pid),
						label = label,
					});
				}

				request.resolve (jobs);
			} catch (GLib.Error e) {
				request.reject (e);
			}
		}

		internal class LaunchdJob {
			public uint pid;
			public string label;
		}

		private Future<RunResult> run_and_capture_output (string[] argv, Cancellable? cancellable) {
			var promise = new Promise<RunResult> ();
			perform_run_and_capture_output.begin (argv, cancellable, promise);
			return promise.future;
		}

		private async void perform_run_and_capture_output (string[] argv, Cancellable? cancellable, Promise<RunResult> request) {
			string[] argv_copy = argv;

			var opts = HostSpawnOptions ();
			opts.has_argv = true;
			opts.argv = argv_copy;
			opts.stdio = PIPE;

			Simmy.SpawnedProcess process;
			try {
				process = yield device.spawn_program (argv_copy[0], opts).wait_async (cancellable);
			} catch (GLib.Error e) {
				request.reject (e);
				return;
			}

			int status = -1;
			var standard_output = new StringBuilder.sized (1024);
			var standard_error = new StringBuilder.sized (1024);

			process.terminated.connect ((s) => {
				status = s;
				perform_run_and_capture_output.callback ();
			});

			process.output.connect ((fd, data) => {
				var buf = new char[data.length + 1];
				Memory.copy (buf, data, data.length);
				buf[data.length] = '\0';
				unowned StringBuilder target = (fd == 1) ? standard_output : standard_error;
				target.append ((string) buf);
			});

			process.resume ();
			yield;

			request.resolve (new RunResult (argv_copy) {
				status = status,
				standard_output = standard_output.str,
				standard_error = standard_error.str,
			});
		}

		private class RunResult {
			public int status;
			public string standard_output;
			public string standard_error;

			private string[] argv;

			public RunResult (string[] argv) {
				this.argv = argv;
			}

			public void check () throws Error {
				if (status != 0) {
					throw new Error.NOT_SUPPORTED ("Failed to run `%s`, it exited with status %d: %s",
						string.joinv (" ", argv),
						status,
						standard_error.chomp ());
				}
			}
		}

		private void on_springboard_agent_unloaded (InternalAgent dead_agent) {
			dead_agent.unloaded.disconnect (on_springboard_agent_unloaded);

			springboard_agent = new SpringboardAgent (this);
			springboard_agent.unloaded.connect (on_springboard_agent_unloaded);
		}

		[NoReturn]
		private static void throw_not_supported () throws Error {
			throw new Error.NOT_SUPPORTED ("Not yet supported by the Simmy backend");
		}
	}

	private sealed class SpringboardAgent : InternalAgent {
		public SpringboardAgent (HostSessionConnection connection) {
			Object (connection: connection);
		}

		public async HostApplicationInfo get_frontmost_application (FrontmostQueryOptions options,
				Cancellable? cancellable) throws Error, IOError {
			var scope = options.scope;
			var scope_node = new Json.Node.alloc ().init_string (scope.to_nick ());

			Json.Node response = yield call ("getFrontmostApplication", new Json.Node[] { scope_node }, null, cancellable);

			if (response.get_node_type () == NULL)
				return HostApplicationInfo.empty ();

			var item = response.get_array ();
			var identifier = item.get_string_element (0);
			var name = item.get_string_element (1);
			var pid = (uint) item.get_int_element (2);
			var info = HostApplicationInfo (identifier, name, pid, make_parameters_dict ());
			if (scope == FULL)
				add_app_icons (info.parameters, new Bytes.take (Base64.decode (item.get_string_element (3))));
			return info;
		}

		public async Gee.Map<string, Bytes> fetch_application_icons (Gee.List<string> app_ids, Cancellable? cancellable)
				throws Error, IOError {
			var app_id_values = new Json.Array ();
			foreach (var id in app_ids)
				app_id_values.add_string_element (id);

			var args = new Json.Node[] { new Json.Node.alloc ().init_array (app_id_values) };

			Json.Node response = yield call ("fetchApplicationIcons", args, null, cancellable);

			var icons = new Gee.HashMap<string, Bytes> ();
			var reader = make_json_reader_taking_node ((owned) response);
			int n = reader.count_elements ();
			for (int i = 0; i != n; i++) {
				reader.read_element (i);
				if (!reader.get_null_value ())
					icons[app_ids[i]] = new Bytes (Base64.decode (reader.get_string_value ()));
				reader.end_element ();
			}
			return icons;
		}

		protected override async uint get_target_pid (Cancellable? cancellable) throws Error, IOError {
			var host_session = (SimmyHostSession) connection.host_session;

			foreach (var job in yield host_session.list_launchd_jobs (cancellable).wait_async (cancellable)) {
				if (job.label == "com.apple.SpringBoard")
					return job.pid;
			}

			throw new Error.NOT_SUPPORTED ("Unable to locate SpringBoard process");
		}

		protected override async string? load_source (Cancellable? cancellable) throws Error, IOError {
			return (string) Frida.Data.Simmy.get_springboard_js_blob ().data;
		}
	}

	private void add_app_icons (HashTable<string, Variant> parameters, Bytes png) {
		var icons = new VariantBuilder (new VariantType.array (VariantType.VARDICT));

		icons.open (VariantType.VARDICT);
		icons.add ("{sv}", "format", new Variant.string ("png"));
		icons.add ("{sv}", "image", Variant.new_from_data (new VariantType ("ay"), png.get_data (), true, png));
		icons.close ();

		parameters["icons"] = icons.end ();
	}

	[CCode (gir_namespace = "FridaSimmy", gir_version = "1.0")]
	namespace Simmy {
		public sealed class Device : Object {
			public void * handle {
				get;
				construct;
			}

			public string udid {
				get;
				construct;
			}

			public string name {
				get;
				construct;
			}

			public string model_identifier {
				get;
				construct;
			}

			public Runtime runtime {
				get;
				construct;
			}

			public void * simmy_context {
				get;
				construct;
			}

			public Device (void * handle, string udid, string name, string model_identifier, Runtime runtime,
					void * simmy_context) {
				Object (
					handle: handle,
					udid: udid,
					name: name,
					model_identifier: model_identifier,
					runtime: runtime,
					simmy_context: simmy_context
				);
			}

			public Future<Gee.List<Application>> list_applications () {
				var promise = new Promise<Gee.List<Application>> ();

				_list_applications (applications => {
					promise.resolve (applications);
				});

				return promise.future;
			}

			public Future<SpawnedProcess> launch_application (string identifier, HostSpawnOptions opts) {
				var promise = new Promise<SpawnedProcess> ();

				_launch_application (identifier, opts, (error_message, process) => {
					if (error_message != null)
						promise.reject (new Error.NOT_SUPPORTED ("%s", error_message));
					else
						promise.resolve (process);
				});

				return promise.future;
			}

			public Future<SpawnedProcess> spawn_program (string program, HostSpawnOptions opts) {
				var promise = new Promise<SpawnedProcess> ();

				_spawn_program (program, opts, (error_message, process) => {
					if (error_message != null)
						promise.reject (new Error.NOT_SUPPORTED ("%s", error_message));
					else
						promise.resolve (process);
				});

				return promise.future;
			}

			public extern void _list_applications (owned ListApplicationsCompleteFunc on_complete);
			public extern void _launch_application (string identifier, HostSpawnOptions options,
				owned LaunchApplicationCompleteFunc on_complete);
			public extern void _spawn_program (string program, HostSpawnOptions options,
				owned SpawnProgramCompleteFunc on_complete);

			public delegate void ListApplicationsCompleteFunc (Gee.List<Application> device);
			public delegate void LaunchApplicationCompleteFunc (string? error_message, SpawnedProcess? process);
			public delegate void SpawnProgramCompleteFunc (string? error_message, SpawnedProcess? process);
		}

		public sealed class Runtime : Object {
			public void * handle {
				get;
				construct;
			}

			public string identifier {
				get {
					return _get_identifier ();
				}
			}

			public string short_name {
				get {
					return _get_short_name ();
				}
			}

			public string version_string {
				get {
					return _get_version_string ();
				}
			}

			public string root {
				get {
					return _get_root ();
				}
			}

			public Runtime (void * handle) {
				Object (handle: handle);
			}

			public extern unowned string _get_identifier ();
			public extern unowned string _get_short_name ();
			public extern unowned string _get_version_string ();
			public extern unowned string _get_root ();
		}

		public sealed class Application : Object {
			public string identifier {
				get;
				construct;
			}

			public string display_name {
				get;
				construct;
			}

			public Application (string identifier, string display_name) {
				Object (identifier: identifier, display_name: display_name);
			}
		}

		public delegate void CompleteFunc ();

		public sealed class SpawnedProcess : Object {
			public signal void terminated (int status);
			public signal void output (int fd, uint8[] data);

			public uint pid {
				get;
				construct;
			}

			public StdioPipes? pipes {
				get;
				construct;
			}

			public MainContext main_context {
				get;
				construct;
			}

			private OutputStream? stdin_stream;

			private Cancellable io_cancellable = new Cancellable ();

			public SpawnedProcess (uint pid, StdioPipes? pipes, MainContext main_context) {
				Object (
					pid: pid,
					pipes: pipes,
					main_context: main_context
				);
			}

			construct {
				if (pipes != null) {
					var source = new IdleSource ();
					source.set_callback (() => {
						start_processing_pipes ();
						return Source.REMOVE;
					});
					source.attach (main_context);
				}
			}

			private void start_processing_pipes () {
				stdin_stream = pipes.input;
				process_next_output_from.begin (pipes.output, 1);
				process_next_output_from.begin (pipes.error, 2);
			}

			public void close () {
				io_cancellable.cancel ();
			}

			public void resume () {
				Posix.kill ((Posix.pid_t) pid, Posix.Signal.CONT);
			}

			public void _on_termination (int status) {
				var source = new IdleSource ();
				source.set_callback (() => {
					if (stdin_stream != null) {
						stdin_stream.close_async.begin ();
						stdin_stream = null;
					}

					close ();

					terminated (status);

					return Source.REMOVE;
				});
				source.attach (main_context);
			}

			public async void input (uint8[] data, Cancellable? cancellable) throws Error, IOError {
				if (stdin_stream == null)
					throw new Error.NOT_SUPPORTED ("Unable to pass input to process spawned without piped stdio");

				try {
					yield stdin_stream.write_all_async (data, Priority.DEFAULT, cancellable, null);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("%s", e.message);
				}
			}

			private async void process_next_output_from (InputStream stream, int fd) {
				try {
					var buf = new uint8[4096];
					var n = yield stream.read_async (buf, Priority.DEFAULT, io_cancellable);

					var data = buf[0:n];
					output (fd, data);

					if (n > 0)
						process_next_output_from.begin (stream, fd);
				} catch (GLib.Error e) {
					stream.close_async.begin ();

					if (!(e is IOError.CANCELLED))
						output (fd, {});
				}
			}
		}
	}
}
