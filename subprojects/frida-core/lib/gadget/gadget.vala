namespace Frida.Gadget {
	private sealed class Config : Object, Json.Serializable {
		public Object interaction {
			get;
			set;
			default = new ListenInteraction ();
		}

		public Gum.TeardownRequirement teardown {
			get;
			set;
			default = Gum.TeardownRequirement.MINIMAL;
		}

		public ScriptRuntime runtime {
			get;
			set;
			default = ScriptRuntime.DEFAULT;
		}

		public Gum.CodeSigningPolicy code_signing {
			get;
			set;
			default = Gum.CodeSigningPolicy.OPTIONAL;
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "interaction" && property_node.get_node_type () == Json.NodeType.OBJECT) {
				var interaction_node = property_node.get_object ();
				var interaction_type = interaction_node.get_string_member ("type");
				if (interaction_type != null) {
					Type t = 0;

					switch (interaction_type) {
						case "script":
							t = typeof (ScriptInteraction);
							break;
						case "script-directory":
							t = typeof (ScriptDirectoryInteraction);
							break;
						case "listen":
							t = typeof (ListenInteraction);
							break;
						case "connect":
							t = typeof (ConnectInteraction);
							break;
					}

					if (t != 0) {
						var obj = Json.gobject_deserialize (t, property_node);
						if (obj != null) {
							bool valid = true;

							if (obj is ScriptInteraction) {
								valid = ((ScriptInteraction) obj).path != null;
							} else if (obj is ScriptDirectoryInteraction) {
								valid = ((ScriptDirectoryInteraction) obj).path != null;
							}

							if (valid) {
								var v = Value (t);
								v.set_object (obj);
								value = v;
								return true;
							}
						}
					}
				}
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	private sealed class ScriptInteraction : Object, Json.Serializable {
		public string path {
			get;
			set;
			default = null;
		}

		public Json.Node parameters {
			get;
			set;
			default = make_empty_json_object ();
		}

		public Script.ChangeBehavior on_change {
			get;
			set;
			default = Script.ChangeBehavior.IGNORE;
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "parameters" && property_node.get_node_type () == Json.NodeType.OBJECT) {
				var v = Value (typeof (Json.Node));
				v.set_boxed (property_node);
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	private sealed class ScriptDirectoryInteraction : Object {
		public string path {
			get;
			set;
			default = null;
		}

		public ChangeBehavior on_change {
			get;
			set;
			default = ChangeBehavior.IGNORE;
		}

		public enum ChangeBehavior {
			IGNORE,
			RESCAN
		}
	}

	private sealed class ScriptConfig : Object, Json.Serializable {
		public ProcessFilter? filter {
			get;
			set;
			default = null;
		}

		public Json.Node parameters {
			get;
			set;
			default = make_empty_json_object ();
		}

		public Script.ChangeBehavior on_change {
			get;
			set;
			default = Script.ChangeBehavior.IGNORE;
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "parameters" && property_node.get_node_type () == Json.NodeType.OBJECT) {
				var v = Value (typeof (Json.Node));
				v.set_boxed (property_node.copy ());
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	private sealed class ProcessFilter : Object {
		public string[] executables {
			get;
			set;
			default = new string[0];
		}

		public string[] bundles {
			get;
			set;
			default = new string[0];
		}

		public string[] objc_classes {
			get;
			set;
			default = new string[0];
		}
	}

	private abstract class SocketInteraction : Object {
		public string? address {
			get;
			set;
		}

		public uint16 port {
			get;
			set;
		}

		public string? certificate {
			get;
			set;
		}

		public string? token {
			get;
			set;
		}
	}

	private sealed class ListenInteraction : SocketInteraction {
		public PortConflictBehavior on_port_conflict {
			get;
			set;
			default = PortConflictBehavior.FAIL;
		}

		public LoadBehavior on_load {
			get;
			set;
			default = LoadBehavior.WAIT;
		}

		public enum LoadBehavior {
			RESUME,
			WAIT
		}

		public string? origin {
			get;
			set;
		}

		public string? asset_root {
			get;
			set;
		}
	}

	private sealed class ConnectInteraction : SocketInteraction, Json.Serializable {
		public string[]? acl {
			get;
			set;
		}

		public Json.Node parameters {
			get;
			set;
			default = make_empty_json_object ();
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "parameters" && property_node.get_node_type () == Json.NodeType.OBJECT) {
				var v = Value (typeof (Json.Node));
				v.set_boxed (property_node);
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	private sealed class Location : Object {
		public string executable_name {
			get;
			construct;
		}

		public string? bundle_id {
			get {
				if (!did_fetch_bundle_id) {
					cached_bundle_id = Environment.detect_bundle_id ();
					did_fetch_bundle_id = true;
				}
				return cached_bundle_id;
			}
		}

		public string? bundle_name {
			get {
				if (!did_fetch_bundle_name) {
					cached_bundle_name = Environment.detect_bundle_name ();
					did_fetch_bundle_name = true;
				}
				return cached_bundle_name;
			}
		}

		public string? path {
			get;
			construct;
		}

		public string? asset_dir {
			get {
				if (!did_compute_asset_dir) {
					string? gadget_path = path;
					if (gadget_path != null) {
						string? dir = null;
#if DARWIN
						dir = try_derive_framework_resource_dir_from_module_path (gadget_path);
#endif
						if (dir == null)
							dir = Path.get_dirname (gadget_path);
						cached_asset_dir = dir;
					}
					did_compute_asset_dir = true;
				}
				return cached_asset_dir;
			}
		}

		public Gum.MemoryRange range {
			get;
			construct;
		}

		private bool did_fetch_bundle_id = false;
		private string? cached_bundle_id = null;

		private bool did_fetch_bundle_name = false;
		private string? cached_bundle_name = null;

		private bool did_compute_asset_dir = false;
		private string? cached_asset_dir = null;

		public Location (string executable_name, string? path, Gum.MemoryRange range) {
			Object (
				executable_name: executable_name,
				path: path,
				range: range
			);
		}

#if ANDROID
		construct {
			if (executable_name.has_prefix ("app_process")) {
				try {
					string cmdline;
					FileUtils.get_contents ("/proc/self/cmdline", out cmdline);
					if (cmdline != "zygote" && cmdline != "zygote64") {
						executable_name = cmdline;

						cached_bundle_id = cmdline.split (":", 2)[0];
						cached_bundle_name = cached_bundle_id;
						did_fetch_bundle_id = true;
						did_fetch_bundle_name = true;
					}
				} catch (FileError e) {
				}
			}
		}
#endif

		public string resolve_asset_path (string asset_path) {
			if (!Path.is_absolute (asset_path)) {
				string? dir = asset_dir;
				if (dir != null)
					return Path.build_filename (dir, asset_path);
			}

			return asset_path;
		}
	}

	private enum State {
		CREATED,
		STARTED,
		STOPPED,
		DETACHED
	}

	private bool loaded = false;
	private State state = State.CREATED;
	private Config? config;
	private Location? location;
	private bool wait_for_resume_needed;
	private MainLoop? wait_for_resume_loop;
	private MainContext? wait_for_resume_context;
	private ThreadIgnoreScope? worker_ignore_scope;
	private Controller? controller;
	private Gum.Interceptor? interceptor;
	private Gum.Exceptor? exceptor;
	private Mutex mutex;
	private Cond cond;

	public void load (Gum.MemoryRange? mapped_range, string? config_data, int * result) {
		if (loaded)
			return;
		loaded = true;

		Environment.init ();

		Gee.Promise<int>? request = null;
		if (result != null)
			request = new Gee.Promise<int> ();

		location = detect_location (mapped_range);

		try {
			config = (config_data != null)
				? parse_config (config_data)
				: load_config (location);
		} catch (Error e) {
			log_warning (e.message);
			return;
		}

		Gum.Process.set_teardown_requirement (config.teardown);
		Gum.Process.set_code_signing_policy (config.code_signing);

		Gum.Cloak.add_range (location.range);

		interceptor = Gum.Interceptor.obtain ();
		interceptor.begin_transaction ();

#if DARWIN
		if (mapped_range != null)
			Environment.ensure_debugger_breakpoints_only ();
#endif

		exceptor = Gum.Exceptor.obtain ();

		try {
			var interaction = config.interaction;
			if (interaction is ScriptInteraction) {
				controller = new ScriptRunner (config, location);
			} else if (interaction is ScriptDirectoryInteraction) {
				controller = new ScriptDirectoryRunner (config, location);
			} else if (interaction is ListenInteraction) {
				controller = new ControlServer (config, location);
			} else if (interaction is ConnectInteraction) {
				controller = new ClusterClient (config, location);
			} else {
				throw new Error.NOT_SUPPORTED ("Invalid interaction specified");
			}
		} catch (Error e) {
			resume ();

			if (request != null) {
				request.set_exception (e);
			} else {
				log_warning ("Failed to start: " + e.message);
			}
		}

		interceptor.end_transaction ();

		if (controller == null)
			return;

		wait_for_resume_needed = true;

		var listen_interaction = config.interaction as ListenInteraction;
		if (listen_interaction != null && listen_interaction.on_load == ListenInteraction.LoadBehavior.RESUME) {
			wait_for_resume_needed = false;
		}

		if (!wait_for_resume_needed)
			resume ();

		if (wait_for_resume_needed && Environment.can_block_at_load_time ()) {
			var scheduler = Gum.ScriptBackend.get_scheduler ();

			scheduler.disable_background_thread ();

			wait_for_resume_context = scheduler.get_js_context ();

			var ignore_scope = new ThreadIgnoreScope (APPLICATION_THREAD);

			start (request);

			var loop = new MainLoop (wait_for_resume_context, true);
			wait_for_resume_loop = loop;

			wait_for_resume_context.push_thread_default ();
			loop.run ();
			wait_for_resume_context.pop_thread_default ();

			scheduler.enable_background_thread ();

			ignore_scope = null;
		} else {
			start (request);
		}

		if (result != null) {
			try {
				*result = request.future.wait ();
			} catch (Gee.FutureError e) {
				*result = -1;
			}
		}
	}

	public void wait_for_permission_to_resume () {
		mutex.lock ();
		while (state != State.STARTED)
			cond.wait (mutex);
		mutex.unlock ();
	}

	public void unload () {
		if (!loaded)
			return;
		loaded = false;

		{
			var source = new IdleSource ();
			source.set_callback (() => {
				stop.begin ();
				return false;
			});
			source.attach (Environment.get_worker_context ());
		}

		State final_state;
		mutex.lock ();
		while (state < State.STOPPED)
			cond.wait (mutex);
		final_state = state;
		mutex.unlock ();

		if (final_state == DETACHED)
			return;

		if (config.teardown == Gum.TeardownRequirement.FULL) {
			config = null;

			invalidate_dbus_context ();

			Environment.deinit ();
		}
	}

	public void resume () {
		mutex.lock ();
		if (state != State.CREATED) {
			mutex.unlock ();
			return;
		}
		state = State.STARTED;
		cond.signal ();
		mutex.unlock ();

		if (wait_for_resume_context != null) {
			var source = new IdleSource ();
			source.set_callback (() => {
				wait_for_resume_loop.quit ();
				return false;
			});
			source.attach (wait_for_resume_context);
		}
	}

	public void kill () {
		kill_process (get_process_id ());
	}

	private State peek_state () {
		State result;

		mutex.lock ();
		result = state;
		mutex.unlock ();

		return result;
	}

	private void start (Gee.Promise<int>? request) {
		var source = new IdleSource ();
		source.set_callback (() => {
			perform_start.begin (request);
			return false;
		});
		source.attach (Environment.get_worker_context ());
	}

	private async void perform_start (Gee.Promise<int>? request) {
		worker_ignore_scope = new ThreadIgnoreScope (FRIDA_THREAD);

		try {
			yield controller.start ();

			var server = controller as ControlServer;
			if (server != null) {
				var listen_address = server.listen_address;
				var inet_address = listen_address as InetSocketAddress;
				if (inet_address != null) {
					uint16 listen_port = inet_address.get_port ();
					Environment.set_thread_name ("frida-gadget-tcp-%u".printf (listen_port));
					if (request != null) {
						request.set_value (listen_port);
					} else {
						log_info ("Listening on %s TCP port %u".printf (
							inet_address.get_address ().to_string (),
							listen_port));
					}
				} else {
#if !WINDOWS
					var unix_address = (UnixSocketAddress) listen_address;
					Environment.set_thread_name ("frida-gadget-unix");
					if (request != null) {
						request.set_value (0);
					} else {
						log_info ("Listening on UNIX socket at “%s”".printf (unix_address.get_path ()));
					}
#else
					assert_not_reached ();
#endif
				}
			} else {
				if (request != null)
					request.set_value (0);
			}
		} catch (GLib.Error e) {
			resume ();

			if (request != null) {
				request.set_exception (e);
			} else {
				log_warning ("Failed to start: " + e.message);
			}
		}
	}

	private async void stop () {
		State pending_state = STOPPED;

		if (controller != null) {
			if (controller.is_eternal) {
				pending_state = DETACHED;
			} else {
				if (config.teardown == Gum.TeardownRequirement.MINIMAL) {
					yield controller.prepare_for_termination (TerminationReason.EXIT);
				} else {
					yield controller.stop ();
					controller = null;

					exceptor = null;

#if DARWIN
					Environment.allow_stolen_breakpoints ();
#endif

					interceptor = null;
				}
			}
		}

		if (pending_state == STOPPED)
			worker_ignore_scope = null;

		mutex.lock ();
		state = pending_state;
		cond.signal ();
		mutex.unlock ();
	}

	private Config load_config (Location location) throws Error {
		unowned string? gadget_path = location.path;
		if (gadget_path == null)
			return new Config ();

		string? config_path = null;
#if DARWIN
		string? resource_dir = try_derive_framework_resource_dir_from_module_path (gadget_path);
		if (resource_dir != null)
			config_path = Path.build_filename (resource_dir, "config.json");
#endif
		if (config_path == null)
			config_path = derive_config_path_from_file_path (gadget_path);

#if IOS || TVOS
		if (resource_dir == null && !FileUtils.test (config_path, FileTest.EXISTS)) {
			var config_dir = Path.get_dirname (config_path);
			if (Path.get_basename (config_dir) == "Frameworks") {
				var app_dir = Path.get_dirname (config_dir);
				config_path = Path.build_filename (app_dir, Path.get_basename (config_path));
			}
		}
#endif

#if ANDROID
		if (!FileUtils.test (config_path, FileTest.EXISTS)) {
			var ext_index = config_path.last_index_of_char ('.');
			if (ext_index != -1) {
				config_path = config_path[0:ext_index] + ".config.so";
			} else {
				config_path = config_path + ".config.so";
			}
		}
#endif

		string config_data;
		try {
			load_asset_text (config_path, out config_data);
		} catch (FileError e) {
			if (e is FileError.NOENT)
				return new Config ();
			throw new Error.PERMISSION_DENIED ("%s", e.message);
		}

		try {
			return (Config) Json.gobject_from_data (typeof (Config), config_data);
		} catch (GLib.Error e) {
			throw new Error.INVALID_ARGUMENT ("Invalid config: %s", e.message);
		}
	}

	private Config parse_config (string config_data) throws Error {
		try {
			return (Config) Json.gobject_from_data (typeof (Config), config_data);
		} catch (GLib.Error e) {
			throw new Error.INVALID_ARGUMENT ("Invalid config: %s", e.message);
		}
	}

	private Location detect_location (Gum.MemoryRange? mapped_range) {
		string? executable_name = null;
		string? our_path = null;
		Gum.MemoryRange? our_range = mapped_range;

		Gum.Address our_address = Gum.Address.from_pointer (Gum.strip_code_pointer ((void *) detect_location));

#if DARWIN
		Environment.detect_darwin_location_fields (our_address, ref executable_name, ref our_path, ref our_range);
#else
		var index = 0;
		Gum.Process.enumerate_modules ((details) => {
			var range = details.range;

			if (index == 0) {
				executable_name = details.name;
			}

			if (mapped_range != null)
				return false;

			if (our_address >= range.base_address && our_address < range.base_address + range.size) {
				our_path = details.path;
				our_range = range;
				return false;
			}

			index++;

			return true;
		});
#endif

		assert (our_range != null);

		return new Location (executable_name, our_path, our_range);
	}

	private interface Controller : Object {
		public abstract bool is_eternal {
			get;
		}

		public abstract async void start () throws Error, IOError;
		public abstract async void prepare_for_termination (TerminationReason reason);
		public abstract async void stop ();
	}

	private abstract class BaseController : Object, Controller, ProcessInvader, ExitHandler {
		public bool is_eternal {
			get {
				return _is_eternal;
			}
		}
		protected bool _is_eternal = false;

		public Config config {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}

		private ExitMonitor exit_monitor;
		private ThreadSuspendMonitor thread_suspend_monitor;
		private UnwindSitter unwind_sitter;

		private Gum.ScriptBackend? qjs_backend;
		private Gum.ScriptBackend? v8_backend;

		private Gee.Map<PortalMembershipId?, PortalClient> portal_clients =
			new Gee.HashMap<PortalMembershipId?, PortalClient> (PortalMembershipId.hash, PortalMembershipId.equal);
		private uint next_portal_membership_id = 1;

		construct {
			exit_monitor = new ExitMonitor (this, MainContext.default ());
			thread_suspend_monitor = new ThreadSuspendMonitor (this);
			unwind_sitter = new UnwindSitter (this);
		}

		public async void start () throws Error, IOError {
			yield on_start ();
		}

		protected abstract async void on_start () throws Error, IOError;

		public async void prepare_for_termination (TerminationReason reason) {
			yield on_terminate (reason);
		}

		protected abstract async void on_terminate (TerminationReason reason);

		public async void stop () {
			yield on_stop ();
		}

		protected abstract async void on_stop ();

		protected SpawnStartState query_current_spawn_state () {
			return (peek_state () == CREATED)
				? SpawnStartState.SUSPENDED
				: SpawnStartState.RUNNING;
		}

		protected Gum.MemoryRange get_memory_range () {
			return location.range;
		}

		protected Gum.ScriptBackend get_script_backend (ScriptRuntime runtime) throws Error {
			switch (runtime) {
				case DEFAULT:
					var config_runtime = config.runtime;
					if (config_runtime != DEFAULT)
						return get_script_backend (config_runtime);
					break;
				case QJS:
					if (qjs_backend == null) {
						qjs_backend = Gum.ScriptBackend.obtain_qjs ();
						if (qjs_backend == null) {
							throw new Error.NOT_SUPPORTED (
								"QuickJS runtime not available due to build configuration");
						}
					}
					return qjs_backend;
				case V8:
					if (v8_backend == null) {
						v8_backend = Gum.ScriptBackend.obtain_v8 ();
						if (v8_backend == null) {
							throw new Error.NOT_SUPPORTED (
								"V8 runtime not available due to build configuration");
						}
					}
					return v8_backend;
			}

			try {
				return get_script_backend (QJS);
			} catch (Error e) {
			}
			return get_script_backend (V8);
		}

		protected Gum.ScriptBackend? get_active_script_backend () {
			return (v8_backend != null) ? v8_backend : qjs_backend;
		}

		protected void acquire_child_gating () throws Error {
			throw new Error.NOT_SUPPORTED ("Not yet implemented");
		}

		protected void release_child_gating () {
		}

		protected async PortalMembershipId join_portal (string address, PortalOptions options,
				Cancellable? cancellable) throws Error, IOError {
			var client = new PortalClient (this, parse_cluster_address (address), address, options.certificate, options.token,
				options.acl, compute_app_info ());
			client.eternalized.connect (on_eternalized);
			client.resume.connect (Frida.Gadget.resume);
			client.kill.connect (Frida.Gadget.kill);
			yield client.start (cancellable);

			var id = PortalMembershipId (next_portal_membership_id++);
			portal_clients[id] = client;

			_is_eternal = true;

			return id;
		}

		protected async void leave_portal (PortalMembershipId membership_id, Cancellable? cancellable) throws Error, IOError {
			PortalClient client;
			if (!portal_clients.unset (membership_id, out client))
				throw new Error.INVALID_ARGUMENT ("Invalid membership ID");

			yield client.stop (cancellable);
		}

		private void on_eternalized () {
			_is_eternal = true;
		}

		private bool supports_async_exit () {
			// Avoid deadlocking in case a fork() happened that we weren't made aware of.
			return Gum.Process.has_thread (Environment.get_worker_tid ());
		}

		protected async void prepare_to_exit () {
			yield on_terminate (TerminationReason.EXIT);
		}

		protected void prepare_to_exit_sync () {
		}

		protected virtual HostApplicationInfo compute_app_info () {
			string identifier = location.bundle_id;
			if (identifier == null)
				identifier = get_executable_path ();

			string name = location.bundle_name;
			if (name == null)
				name = Path.get_basename (get_executable_path ());

			uint pid = get_process_id ();

			var info = HostApplicationInfo (identifier, name, pid, make_parameters_dict ());
			info.parameters["system"] = compute_system_parameters ();

			return info;
		}
	}

	private sealed class ScriptRunner : BaseController {
		private ScriptEngine engine;
		private Script script;

		public ScriptRunner (Config config, Location location) {
			Object (config: config, location: location);
		}

		construct {
			engine = new ScriptEngine (this);

			var path = resolve_script_path (config, location);
			var interaction = config.interaction as ScriptInteraction;
			script = new Script (path, interaction.parameters, interaction.on_change, engine);
		}

		protected override async void on_start () throws Error, IOError {
			yield script.start ();

			Frida.Gadget.resume ();
		}

		protected override async void on_terminate (TerminationReason reason) {
			yield script.prepare_for_termination (reason);
		}

		protected override async void on_stop () {
			yield script.stop ();

			yield engine.close ();
		}

		private static string resolve_script_path (Config config, Location location) {
			var raw_path = ((ScriptInteraction) config.interaction).path;

			if (!Path.is_absolute (raw_path)) {
				string? documents_dir = Environment.detect_documents_dir ();
				if (documents_dir != null) {
					var script_path = Path.build_filename (documents_dir, raw_path);
					if (FileUtils.test (script_path, FileTest.EXISTS))
						return script_path;
				}
			}

			return location.resolve_asset_path (raw_path);
		}
	}

	private sealed class ScriptDirectoryRunner : BaseController {
		public string directory_path {
			get;
			construct;
		}

		private ScriptEngine engine;
		private Gee.HashMap<string, Script> scripts = new Gee.HashMap<string, Script> ();
		private bool scan_in_progress = false;
		private GLib.FileMonitor monitor;
		private Source unchanged_timeout;

		public ScriptDirectoryRunner (Config config, Location location) {
			Object (
				config: config,
				location: location,
				directory_path: location.resolve_asset_path (((ScriptDirectoryInteraction) config.interaction).path)
			);
		}

		construct {
			engine = new ScriptEngine (this);
		}

		protected override async void on_start () throws Error, IOError {
			var interaction = config.interaction as ScriptDirectoryInteraction;

			if (interaction.on_change == ScriptDirectoryInteraction.ChangeBehavior.RESCAN) {
				try {
					var path = directory_path;
					var monitor = File.new_for_path (path).monitor_directory (FileMonitorFlags.NONE);
					monitor.changed.connect (on_file_changed);
					this.monitor = monitor;
				} catch (GLib.Error e) {
					log_warning ("Failed to watch directory: " + e.message);
				}
			}

			yield scan ();

			Frida.Gadget.resume ();
		}

		protected override async void on_terminate (TerminationReason reason) {
			foreach (var script in scripts.values.to_array ())
				yield script.prepare_for_termination (reason);
		}

		protected override async void on_stop () {
			if (monitor != null) {
				monitor.changed.disconnect (on_file_changed);
				monitor.cancel ();
				monitor = null;
			}

			foreach (var script in scripts.values.to_array ())
				yield script.stop ();
			scripts.clear ();

			yield engine.close ();
		}

		private async void scan () throws Error {
			scan_in_progress = true;

			try {
				var directory_path = this.directory_path;

				Dir dir;
				try {
					dir = Dir.open (directory_path);
				} catch (FileError e) {
					return;
				}

				string? name;
				var names_seen = new Gee.HashSet<string> ();
				while ((name = dir.read_name ()) != null) {
					if (name[0] == '.' || !name.has_suffix (".js"))
						continue;

					names_seen.add (name);

					var script_path = Path.build_filename (directory_path, name);
					var config_path = derive_config_path_from_file_path (script_path);

					try {
						var config = load_config (config_path);

						var matches_filter = current_process_matches (config.filter);
						if (matches_filter) {
							var script = scripts[name];
							var parameters = config.parameters;
							var on_change = config.on_change;

							if (script != null && (!script.parameters.equal (parameters) ||
									script.on_change != on_change)) {
								yield script.stop ();
								script = null;
							}

							if (script == null) {
								script = new Script (script_path, parameters, on_change, engine);
								yield script.start ();
							}

							scripts[name] = script;
						}

						Script script = null;
						if (!matches_filter && scripts.unset (name, out script)) {
							yield script.stop ();
						}
					} catch (Error e) {
						log_warning ("Skipping %s: %s".printf (name, e.message));
						continue;
					}
				}

				foreach (var script_name in scripts.keys.to_array ()) {
					var deleted = !names_seen.contains (script_name);
					if (deleted) {
						Script script;
						scripts.unset (script_name, out script);
						yield script.stop ();
					}
				}
			} finally {
				scan_in_progress = false;
			}
		}

		private bool current_process_matches (ProcessFilter? filter) {
			if (filter == null)
				return true;

			var executables = filter.executables;
			var num_executables = executables.length;
			if (num_executables > 0) {
				var executable_name = location.executable_name;

				for (var index = 0; index != num_executables; index++) {
					if (executables[index] == executable_name)
						return true;
				}
			}

			var bundles = filter.bundles;
			var num_bundles = bundles.length;
			if (num_bundles > 0) {
				var bundle_id = location.bundle_id;
				if (bundle_id != null) {
					for (var index = 0; index != num_bundles; index++) {
						if (bundles[index] == bundle_id)
							return true;
					}
				}
			}

			var classes = filter.objc_classes;
			var num_classes = classes.length;
			for (var index = 0; index != num_classes; index++) {
				if (Environment.has_objc_class (classes[index]))
					return true;
			}

			return false;
		}

		private void on_file_changed (File file, File? other_file, FileMonitorEvent event_type) {
			if (event_type == FileMonitorEvent.CHANGES_DONE_HINT)
				return;

			var source = new TimeoutSource (50);
			source.set_callback (() => {
				if (scan_in_progress)
					return true;
				scan.begin ();
				return false;
			});
			source.attach (Environment.get_worker_context ());

			if (unchanged_timeout != null)
				unchanged_timeout.destroy ();
			unchanged_timeout = source;
		}

		private ScriptConfig load_config (string path) throws Error {
			string data;
			try {
				load_asset_text (path, out data);
			} catch (FileError e) {
				if (e is FileError.NOENT)
					return new ScriptConfig ();
				throw new Error.PERMISSION_DENIED ("%s", e.message);
			}

			try {
				return (ScriptConfig) Json.gobject_from_data (typeof (ScriptConfig), data);
			} catch (GLib.Error e) {
				throw new Error.INVALID_ARGUMENT ("Invalid config: %s", e.message);
			}
		}
	}

	private sealed class Script : Object, RpcPeer {
		private const uint8 QUICKJS_BYTECODE_MAGIC =
#if BIG_ENDIAN
			0x42
#else
			0x02
#endif
			;

		public enum ChangeBehavior {
			IGNORE,
			RELOAD
		}

		public signal void message (string json, Bytes? data);

		public string path {
			get;
			construct;
		}

		public Json.Node parameters {
			get;
			construct;
		}

		public ChangeBehavior on_change {
			get;
			construct;
		}

		public ScriptEngine engine {
			get;
			construct;
		}

		private AgentScriptId id;
		private bool load_in_progress = false;
		private GLib.FileMonitor monitor;
		private Source unchanged_timeout;
		private RpcClient rpc_client;

		public Script (string path, Json.Node parameters, ChangeBehavior on_change, ScriptEngine engine) {
			Object (
				path: path,
				parameters: parameters,
				on_change: on_change,
				engine: engine
			);
		}

		construct {
			rpc_client = new RpcClient (this);
		}

		public async void start () throws Error {
			engine.message_from_script.connect (on_message);

			if (on_change == ChangeBehavior.RELOAD) {
				try {
					var monitor = File.new_for_path (path).monitor_file (FileMonitorFlags.NONE);
					monitor.changed.connect (on_file_changed);
					this.monitor = monitor;
				} catch (GLib.Error e) {
					log_warning ("Failed to watch %s: %s".printf (path, e.message));
				}

				yield try_reload ();
			} else {
				try {
					yield load ();
				} catch (Error e) {
					engine.message_from_script.disconnect (on_message);
					throw e;
				}
			}
		}

		public async void prepare_for_termination (TerminationReason reason) {
			yield engine.prepare_for_termination (reason);
		}

		public async void stop () {
			if (monitor != null) {
				monitor.changed.disconnect (on_file_changed);
				monitor.cancel ();
				monitor = null;
			}

			if (id.handle != 0) {
				try {
					yield engine.destroy_script (id);
				} catch (Error e) {
				}
				id = AgentScriptId (0);
			}

			engine.message_from_script.disconnect (on_message);
		}

		private async void try_reload () {
			try {
				yield load ();
			} catch (Error e) {
				log_warning ("Failed to load %s: %s".printf (path, e.message));
			}
		}

		private async void load () throws Error {
			load_in_progress = true;

			try {
				var path = this.path;

				Bytes contents;
				try {
					load_asset_bytes (path, out contents);
				} catch (FileError e) {
					throw new Error.INVALID_ARGUMENT ("%s", e.message);
				}

				var options = new ScriptOptions ();
				options.name = Path.get_basename (path).split (".", 2)[0];

				ScriptEngine.ScriptInstance instance;
				if (contents.length > 0 && contents[0] == QUICKJS_BYTECODE_MAGIC)
					instance = yield engine.create_script (null, contents, options);
				else
					instance = yield engine.create_script ((string) contents.get_data (), null, options);

				if (id.handle != 0)
					yield engine.destroy_script (id);
				id = instance.script_id;

				yield engine.load_script (id);
				yield call_init ();
			} finally {
				load_in_progress = false;
			}
		}

		private async void call_init () {
			var stage = new Json.Node.alloc ().init_string ((peek_state () == State.CREATED) ? "early" : "late");

			try {
				yield rpc_client.call ("init", new Json.Node[] { stage, parameters }, null, null);
			} catch (GLib.Error e) {
			}
		}

		private void on_file_changed (File file, File? other_file, FileMonitorEvent event_type) {
			if (event_type == FileMonitorEvent.CHANGES_DONE_HINT)
				return;

			var source = new TimeoutSource (50);
			source.set_callback (() => {
				if (load_in_progress)
					return true;
				try_reload.begin ();
				return false;
			});
			source.attach (Environment.get_worker_context ());

			if (unchanged_timeout != null)
				unchanged_timeout.destroy ();
			unchanged_timeout = source;
		}

		private void on_message (AgentScriptId script_id, string json, Bytes? data) {
			if (script_id != id)
				return;

			bool handled = rpc_client.try_handle_message (json);
			if (handled)
				return;

			var parser = new Json.Parser ();
			try {
				parser.load_from_data (json);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
			var message = parser.get_root ().get_object ();

			var type = message.get_string_member ("type");
			if (type == "log")
				handled = try_handle_log_message (message);

			if (!handled) {
				stdout.puts (json);
				stdout.putc ('\n');
			}
		}

		private bool try_handle_log_message (Json.Object message) {
			var level = message.get_string_member ("level");
			var payload = message.get_string_member ("payload");
			switch (level) {
				case "info":
					print ("%s\n", payload);
					break;

				case "warning":
					printerr ("\033[0;33m%s\033[0m\n", payload);
					break;

				case "error":
					printerr ("\033[0;31m%s\033[0m\n", payload);
					break;
			}
			return true;
		}

		private async void post_rpc_message (string json, Bytes? data, Cancellable? cancellable) throws Error, IOError {
			engine.post_to_script (id, json, data);
		}
	}

	private sealed class ControlServer : BaseController {
		public EndpointParameters endpoint_params {
			get;
			construct;
		}

		public SocketAddress? listen_address {
			get {
				return (service != null) ? service.listen_address : null;
			}
		}

		private WebService? service;
		private AuthenticationService? auth_service;
		private Gee.Map<DBusConnection, Peer> peers = new Gee.HashMap<DBusConnection, Peer> ();

		private Gee.Map<AgentSessionId?, LiveAgentSession> sessions =
			new Gee.HashMap<AgentSessionId?, LiveAgentSession> (AgentSessionId.hash, AgentSessionId.equal);
		private Gee.ArrayList<Gum.Script> eternalized_scripts = new Gee.ArrayList<Gum.Script> ();

		private Cancellable io_cancellable = new Cancellable ();

		public ControlServer (Config config, Location location) throws Error {
			Object (config: config, location: location);
		}

		construct {
		}

		protected override async void on_start () throws Error, IOError {
			var interaction = (ListenInteraction) config.interaction;

			string? token = interaction.token;
			auth_service = (token != null) ? new StaticAuthenticationService (token) : null;

			File? asset_root = null;
			string? asset_root_path = interaction.asset_root;
			if (asset_root_path != null)
				asset_root = File.new_for_path (location.resolve_asset_path (asset_root_path));

			var endpoint_params = new EndpointParameters (interaction.address, interaction.port,
				parse_certificate (interaction.certificate, location), interaction.origin, auth_service, asset_root);

			service = new WebService (endpoint_params, WebServiceFlavor.CONTROL, interaction.on_port_conflict,
				new TunnelInterfaceObserver ());
			service.incoming.connect (on_incoming_connection);
			yield service.start (io_cancellable);
		}

		protected override async void on_terminate (TerminationReason reason) {
			foreach (LiveAgentSession session in sessions.values.to_array ())
				yield session.prepare_for_termination (reason);

			foreach (var connection in peers.keys.to_array ()) {
				try {
					yield connection.flush ();
				} catch (GLib.Error e) {
				}
			}
		}

		protected override async void on_stop () {
			service.stop ();

			io_cancellable.cancel ();

			while (!peers.is_empty) {
				var iterator = peers.keys.iterator ();
				iterator.next ();
				var connection = iterator.get ();

				Peer peer;
				peers.unset (connection, out peer);
				try {
					yield peer.close ();
				} catch (IOError e) {
					assert_not_reached ();
				}
			}
		}

		private void on_incoming_connection (IOStream connection, SocketAddress remote_address) {
			handle_incoming_connection.begin (connection);
		}

		private async void handle_incoming_connection (IOStream raw_connection) throws GLib.Error {
			var connection = yield new DBusConnection (raw_connection, null, DELAY_MESSAGE_PROCESSING, null, io_cancellable);
			connection.on_closed.connect (on_connection_closed);

			Peer peer;
			if (auth_service != null)
				peer = new AuthenticationChannel (this, connection);
			else
				peer = setup_control_channel (connection);
			peers[connection] = peer;

			connection.start_message_processing ();
		}

		private void on_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			Peer peer;
			if (peers.unset (connection, out peer))
				peer.close.begin (io_cancellable);
		}

		private async void promote_authentication_channel (AuthenticationChannel channel) throws GLib.Error {
			DBusConnection connection = channel.connection;

			peers.unset (connection);
			yield channel.close (io_cancellable);

			peers[connection] = setup_control_channel (connection);
		}

		private void kick_authentication_channel (AuthenticationChannel channel) {
			Idle.add (() => {
				channel.connection.close.begin (io_cancellable);
				return false;
			});
		}

		private ControlChannel setup_control_channel (DBusConnection connection) throws IOError {
			return new ControlChannel (this, connection);
		}

		private void teardown_control_channel (ControlChannel channel) {
			foreach (AgentSessionId id in channel.sessions) {
				LiveAgentSession session = sessions[id];

				unregister_session (session);

				if (session.persist_timeout == 0) {
					sessions.unset (id);
					session.close.begin (io_cancellable);
				} else {
					session.controller = null;
					session.message_sink = null;
					session.interrupt.begin (io_cancellable);
				}
			}
		}

		private async AgentSessionId attach (HashTable<string, Variant> options, ControlChannel requester,
				Cancellable? cancellable) throws Error, IOError {
			var opts = SessionOptions._deserialize (options);
			if (opts.realm != NATIVE)
				throw new Error.NOT_SUPPORTED ("Only native realm is supported when embedded");

			var id = AgentSessionId.generate ();

			DBusConnection controller_connection = requester.connection;

			AgentMessageSink sink;
			try {
				sink = yield controller_connection.get_proxy (null, ObjectPath.for_agent_message_sink (id),
					DO_NOT_LOAD_PROPERTIES, cancellable);
			} catch (IOError e) {
				throw_dbus_error (e);
			}

			MainContext dbus_context = yield get_dbus_context ();

			var session = new LiveAgentSession (this, id, opts.persist_timeout, sink, dbus_context);
			sessions[id] = session;
			session.closed.connect (on_session_closed);
			session.script_eternalized.connect (on_script_eternalized);

			try {
				session.registration_id = controller_connection.register_object (ObjectPath.for_agent_session (id),
					(AgentSession) session);
			} catch (IOError io_error) {
				assert_not_reached ();
			}

			session.controller = requester;

			requester.sessions.add (id);

			return id;
		}

		private async void reattach (AgentSessionId id, ControlChannel requester, Cancellable? cancellable) throws Error, IOError {
			LiveAgentSession? session = sessions[id];
			if (session == null || session.controller != null)
				throw new Error.INVALID_OPERATION ("Invalid session ID");

			DBusConnection controller_connection = requester.connection;

			try {
				session.message_sink = yield controller_connection.get_proxy (null, ObjectPath.for_agent_message_sink (id),
					DO_NOT_LOAD_PROPERTIES, cancellable);
			} catch (IOError e) {
				throw_dbus_error (e);
			}

			assert (session.registration_id == 0);
			try {
				session.registration_id = controller_connection.register_object (ObjectPath.for_agent_session (id),
					(AgentSession) session);
			} catch (IOError io_error) {
				assert_not_reached ();
			}

			session.controller = requester;

			requester.sessions.add (id);
		}

		private void on_session_closed (BaseAgentSession base_session) {
			LiveAgentSession session = (LiveAgentSession) base_session;
			AgentSessionId id = session.id;

			session.script_eternalized.disconnect (on_script_eternalized);
			session.closed.disconnect (on_session_closed);
			sessions.unset (id);

			ControlChannel? controller = session.controller;
			if (controller != null) {
				unregister_session (session);
				controller.sessions.remove (id);
				controller.agent_session_detached (id, APPLICATION_REQUESTED, CrashInfo.empty ());
			}
		}

		private void unregister_session (LiveAgentSession session) {
			var id = session.registration_id;
			if (id != 0) {
				session.controller.connection.unregister_object (id);
				session.registration_id = 0;
			}
		}

		private void on_script_eternalized (Gum.Script script) {
			eternalized_scripts.add (script);
			_is_eternal = true;
		}

		private interface Peer : Object {
			public abstract async void close (Cancellable? cancellable = null) throws IOError;
		}

		private class AuthenticationChannel : Object, Peer, AuthenticationService {
			public weak ControlServer parent {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();

			public AuthenticationChannel (ControlServer parent, DBusConnection connection) {
				Object (parent: parent, connection: connection);
			}

			construct {
				try {
					AuthenticationService auth_service = this;
					registrations.add (connection.register_object (ObjectPath.AUTHENTICATION_SERVICE, auth_service));

					HostSession host_session = new UnauthorizedHostSession ();
					registrations.add (connection.register_object (ObjectPath.HOST_SESSION, host_session));
				} catch (IOError e) {
					assert_not_reached ();
				}
			}

			public async void close (Cancellable? cancellable) throws IOError {
				foreach (var id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}

			public async string authenticate (string token, Cancellable? cancellable) throws GLib.Error {
				try {
					string session_info = yield parent.auth_service.authenticate (token, cancellable);
					yield parent.promote_authentication_channel (this);
					return session_info;
				} catch (GLib.Error e) {
					if (e is Error.INVALID_ARGUMENT)
						parent.kick_authentication_channel (this);
					throw e;
				}
			}
		}

		private class ControlChannel : Object, Peer, HostSession, GadgetSession {
			public weak ControlServer parent {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public Gee.Set<AgentSessionId?> sessions {
				get;
				default = new Gee.HashSet<AgentSessionId?> (AgentSessionId.hash, AgentSessionId.equal);
			}

			private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();
			private HostApplicationInfo this_app;
			private HostProcessInfo this_process;
			private TimeoutSource? ping_timer;
			private bool resume_on_attach = true;

			public ControlChannel (ControlServer parent, DBusConnection connection) {
				Object (parent: parent, connection: connection);
			}

			construct {
				try {
					HostSession host_session = this;
					registrations.add (connection.register_object (Frida.ObjectPath.HOST_SESSION, host_session));

					GadgetSession gadget_session = this;
					registrations.add (connection.register_object (Frida.ObjectPath.GADGET_SESSION, gadget_session));

					AuthenticationService null_auth = new NullAuthenticationService ();
					registrations.add (connection.register_object (Frida.ObjectPath.AUTHENTICATION_SERVICE, null_auth));
				} catch (IOError e) {
					assert_not_reached ();
				}

				uint pid = get_process_id ();
				string identifier = "re.frida.Gadget";
				string name = "Gadget";
				var no_parameters = make_parameters_dict ();
				this_app = HostApplicationInfo (identifier, name, pid, no_parameters);
				this_process = HostProcessInfo (pid, name, no_parameters);
			}

			public async void close (Cancellable? cancellable) throws IOError {
				discard_ping_timer ();

				parent.teardown_control_channel (this);

				foreach (var id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}

			public async void ping (uint interval_seconds, Cancellable? cancellable) throws Error, IOError {
				discard_ping_timer ();

				if (interval_seconds != 0) {
					ping_timer = new TimeoutSource (interval_seconds * 1500);
					ping_timer.set_callback (on_ping_timeout);
					ping_timer.attach (MainContext.get_thread_default ());
				}
			}

			private void discard_ping_timer () {
				if (ping_timer == null)
					return;
				ping_timer.destroy ();
				ping_timer = null;
			}

			private bool on_ping_timeout () {
				connection.close.begin ();
				return false;
			}

			public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable) throws Error, IOError {
				return compute_system_parameters ();
			}

			public async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				return this_app;
			}

			public async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				var opts = ApplicationQueryOptions._deserialize (options);

				if (opts.has_selected_identifiers ()) {
					bool gadget_is_selected = false;
					opts.enumerate_selected_identifiers (identifier => {
						if (identifier == this_app.identifier)
							gadget_is_selected = true;
					});
					if (!gadget_is_selected)
						return {};
				}

				return new HostApplicationInfo[] { this_app };
			}

			public async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				var opts = ProcessQueryOptions._deserialize (options);

				if (opts.has_selected_pids ()) {
					bool gadget_is_selected = false;
					opts.enumerate_selected_pids (pid => {
						if (pid == this_process.pid)
							gadget_is_selected = true;
					});
					if (!gadget_is_selected)
						return {};
				}

				return new HostProcessInfo[] { this_process };
			}

			public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not possible when embedded");
			}

			public async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not possible when embedded");
			}

			public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not possible when embedded");
			}

			public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not possible when embedded");
			}

			public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not yet implemented");
			}

			public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
				if (program != this_app.identifier)
					throw new Error.NOT_SUPPORTED ("Unable to spawn other apps when embedded");

				resume_on_attach = false;

				return this_process.pid;
			}

			public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not possible when embedded");
			}

			public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
				validate_pid (pid);

				Frida.Gadget.resume ();
			}

			public async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
				validate_pid (pid);

				Frida.Gadget.kill ();
			}

			public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				validate_pid (pid);

				if (resume_on_attach)
					Frida.Gadget.resume ();

				return yield parent.attach (options, this, cancellable);
			}

			public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
				yield parent.reattach (id, this, cancellable);
			}

			public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Unable to inject libraries when embedded");
			}

			public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Unable to inject libraries when embedded");
			}

			public async ChannelId open_channel (string address, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Unable to open channels when embedded");
			}

			public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Unable to open services when embedded");
			}

			public async void break_and_resume (Cancellable? cancellable) throws Error, IOError {
#if DARWIN
				Environment.break_and_resume ();
#else
				throw new Error.NOT_SUPPORTED ("This API is only applicable on Darwin");
#endif
			}

			public async void break_and_detach (Cancellable? cancellable) throws Error, IOError {
#if DARWIN
				Environment.break_and_detach ();
#else
				throw new Error.NOT_SUPPORTED ("This API is only applicable on Darwin");
#endif
			}

			private void validate_pid (uint pid) throws Error {
				if (pid != this_process.pid)
					throw new Error.NOT_SUPPORTED ("Unable to act on other processes when embedded");
			}
		}

		private class LiveAgentSession : BaseAgentSession {
			public ControlChannel? controller {
				get;
				set;
			}

			public uint registration_id {
				get;
				set;
			}

			public LiveAgentSession (ProcessInvader invader, AgentSessionId id, uint persist_timeout, AgentMessageSink sink,
					MainContext dbus_context) {
				Object (
					invader: invader,
					id: id,
					persist_timeout: persist_timeout,
					message_sink: sink,
					frida_context: MainContext.ref_thread_default (),
					dbus_context: dbus_context
				);
			}
		}
	}

	private sealed class ClusterClient : BaseController {
		public SocketConnectable connectable {
			get;
			construct;
		}

		public string host {
			get;
			construct;
		}

		public TlsCertificate? certificate {
			get;
			construct;
		}

		public string? token {
			get;
			construct;
		}

		public string[]? acl {
			get;
			construct;
		}

		private PortalClient client;

		public ClusterClient (Config config, Location location) throws Error {
			var interaction = (ConnectInteraction) config.interaction;
			string? address = interaction.address;
			Object (
				config: config,
				location: location,
				connectable: parse_cluster_address (address, interaction.port),
				host: (address != null) ? address : "lolcathost",
				certificate: parse_certificate (interaction.certificate, location),
				token: interaction.token,
				acl: interaction.acl
			);
		}

		construct {
			client = new PortalClient (this, connectable, host, certificate, token, acl, compute_app_info ());
			client.eternalized.connect (on_eternalized);
			client.resume.connect (Frida.Gadget.resume);
			client.kill.connect (Frida.Gadget.kill);
		}

		protected override HostApplicationInfo compute_app_info () {
			var info = base.compute_app_info ();
			var interaction = config.interaction as ConnectInteraction;

			try {
				info.parameters["config"] = Json.gvariant_deserialize (interaction.parameters, null);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			return info;
		}

		protected override async void on_start () throws Error, IOError {
			yield client.start ();
		}

		protected override async void on_terminate (TerminationReason reason) {
		}

		protected override async void on_stop () {
			try {
				yield client.stop ();
			} catch (IOError e) {
				assert_not_reached ();
			}
		}

		private void on_eternalized () {
			_is_eternal = true;
		}
	}

	private string derive_config_path_from_file_path (string path) {
		var dirname = Path.get_dirname (path);
		var filename = Path.get_basename (path);

		string stem;
		var ext_index = filename.last_index_of_char ('.');
		if (ext_index != -1)
			stem = filename[0:ext_index];
		else
			stem = filename;

		return Path.build_filename (dirname, stem + ".config");
	}

#if DARWIN
	private string? try_derive_framework_resource_dir_from_module_path (string module_path) {
#if MACOS
		string[] parts = module_path.split ("/");
		int n = parts.length;

		bool is_framework = (n >= 2 && parts[n - 2].has_suffix (".framework")) ||
			(n >= 4 && parts[n - 4].has_suffix (".framework") && parts[n - 3] == "Versions");
		if (!is_framework)
			return null;

		return Path.build_filename (Path.get_dirname (module_path), "Resources");
#else
		string module_dir = Path.get_dirname (module_path);
		if (!module_dir.has_suffix (".framework"))
			return null;
		return module_dir;
#endif
	}
#endif

	private void load_asset_text (string filename, out string text) throws FileError {
		Bytes raw_contents;
		load_asset_bytes (filename, out raw_contents);

		unowned string str = (string) raw_contents.get_data ();
		if (!str.validate ())
			throw new FileError.FAILED ("%s: invalid UTF-8", filename);

		text = str;
	}

	private void load_asset_bytes (string filename, out Bytes bytes) throws FileError {
#if ANDROID
		if (maybe_load_asset_bytes_from_apk (filename, out bytes))
			return;
#endif

		uint8[] data;
		FileUtils.get_data (filename, out data);
		bytes = new Bytes.take ((owned) data);
	}

#if ANDROID
	private bool maybe_load_asset_bytes_from_apk (string filename, out Bytes contents) throws FileError {
		contents = null;

		var tokens = filename.split ("!", 2);
		if (tokens.length != 2 || !tokens[0].has_suffix (".apk"))
			return false;
		unowned string apk_path = tokens[0];
		unowned string file_path = tokens[1];

		var reader = Minizip.Reader.create ();
		try {
			if (reader.open_file (apk_path) != OK)
				throw new FileError.FAILED ("Unable to open APK");

			if (reader.locate_entry (file_path[1:], true) != OK)
				throw new FileError.FAILED ("Unable to locate %s inside APK", file_path);

			var size = reader.entry_save_buffer_length ();
			var data = new uint8[size + 1];
			if (reader.entry_save_buffer (data[:size]) != OK)
				throw new FileError.FAILED ("Unable to extract %s from APK", file_path);

			contents = new Bytes.take ((owned) data);
			return true;
		} finally {
			reader.close ();
			Minizip.Reader.destroy (ref reader);
		}
	}
#endif

	private Json.Node make_empty_json_object () {
		return new Json.Node.alloc ().init_object (new Json.Object ());
	}

	private TlsCertificate? parse_certificate (string? str, Location location) throws Error {
		if (str == null)
			return null;

		try {
			if (str.index_of_char ('\n') != -1)
				return new TlsCertificate.from_pem (str, -1);
			else
				return new TlsCertificate.from_file (location.resolve_asset_path (str));
		} catch (GLib.Error e) {
			throw new Error.INVALID_ARGUMENT ("%s", e.message);
		}
	}

	namespace Environment {
		private extern void init ();
		private extern void deinit ();

		private extern bool can_block_at_load_time ();

		private extern Gum.ThreadId get_worker_tid ();
		private extern unowned MainContext get_worker_context ();

		private extern string? detect_bundle_id ();
		private extern string? detect_bundle_name ();
		private extern string? detect_documents_dir ();
		private extern bool has_objc_class (string name);

		private extern void set_thread_name (string name);
#if DARWIN
		private extern void detect_darwin_location_fields (Gum.Address our_address, ref string? executable_name,
			ref string? our_path, ref Gum.MemoryRange? our_range);
		private extern void ensure_debugger_breakpoints_only ();
		private extern void allow_stolen_breakpoints ();
		private extern void break_and_resume ();
		private extern void break_and_detach ();
#endif
	}

	private extern void log_info (string message);
	private extern void log_warning (string message);

	private Mutex gc_mutex;
	private uint gc_generation = 0;
	private bool gc_scheduled = false;

	public void _on_pending_thread_garbage (void * data) {
		gc_mutex.lock ();
		gc_generation++;
		bool already_scheduled = gc_scheduled;
		gc_scheduled = true;
		gc_mutex.unlock ();

		if (already_scheduled)
			return;

		Timeout.add (50, () => {
			gc_mutex.lock ();
			uint generation = gc_generation;
			gc_mutex.unlock ();

			bool collected_everything = Thread.garbage_collect ();

			gc_mutex.lock ();
			bool same_generation = generation == gc_generation;
			bool repeat = !collected_everything || !same_generation;
			if (!repeat)
				gc_scheduled = false;
			gc_mutex.unlock ();

			return repeat;
		});
	}
}
