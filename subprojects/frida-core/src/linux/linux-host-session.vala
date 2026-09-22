namespace Frida {
	public sealed class LinuxHostSessionBackend : LocalHostSessionBackend {
		protected override LocalHostSessionProvider make_provider () {
			return new LinuxHostSessionProvider ();
		}
	}

	public sealed class LinuxHostSessionProvider : LocalHostSessionProvider {
		protected override LocalHostSession make_host_session (HostSessionOptions? options) throws Error {
			var tempdir = new TemporaryDirectory ();
			return new LinuxHostSession (new LinuxHelperProcess (tempdir), tempdir);
		}
	}

	public sealed class LinuxHostSession : LocalHostSession {
		public LinuxHelper helper {
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

		private AgentContainer system_session_container;

		private AgentDescriptor? agent;

#if ANDROID
		private Promise<AndroidHelperClient>? android_helper_request;
		private RoboLauncher robo_launcher;
		private CrashMonitor? crash_monitor;
#endif

		private SpawnGater? spawn_gater;

#if !ANDROID
		private ApplicationEnumerator application_enumerator = new ApplicationEnumerator ();
#endif
		private ProcessEnumerator process_enumerator = new ProcessEnumerator ();

		public LinuxHostSession (owned LinuxHelper helper, owned TemporaryDirectory tempdir, bool report_crashes = true) {
			Object (
				helper: helper,
				tempdir: tempdir,
				report_crashes: report_crashes
			);
		}

		construct {
			helper.output.connect (on_output);

			injector = new Linjector (helper, false, tempdir);
			injector.uninjected.connect (on_uninjected);

#if HAVE_EMBEDDED_ASSETS
			var blob32 = Frida.Data.Agent.get_frida_agent_32_so_blob ();
			var blob64 = Frida.Data.Agent.get_frida_agent_64_so_blob ();
			var emulated_arm = Frida.Data.Agent.get_frida_agent_arm_so_blob ();
			var emulated_arm64 = Frida.Data.Agent.get_frida_agent_arm64_so_blob ();
			agent = new AgentDescriptor (PathTemplate ("frida-agent-<arch>.so"),
				new Bytes.static (blob32.data),
				new Bytes.static (blob64.data),
				new AgentResource[] {
					new AgentResource ("frida-agent-arm.so", new Bytes.static (emulated_arm.data), tempdir),
					new AgentResource ("frida-agent-arm64.so", new Bytes.static (emulated_arm64.data), tempdir),
				},
				AgentMode.INSTANCED,
				tempdir);
#endif

#if ANDROID
			robo_launcher = new RoboLauncher (this, io_cancellable);
			robo_launcher.gating_cancelled.connect (on_spawn_gating_cancelled);
			robo_launcher.spawn_added.connect (on_spawn_added);
			robo_launcher.spawn_removed.connect (on_spawn_removed);

			if (report_crashes) {
				crash_monitor = new CrashMonitor ();
				crash_monitor.process_crashed.connect (on_process_crashed);
			}
#endif
		}

		public override async void preload (Cancellable? cancellable) throws Error, IOError {
#if ANDROID
			yield get_android_helper_client (cancellable);
			yield robo_launcher.preload (cancellable);
#endif
		}

		public override async void close (Cancellable? cancellable) throws IOError {
#if ANDROID
			yield robo_launcher.close (cancellable);
			robo_launcher.gating_cancelled.disconnect (on_spawn_gating_cancelled);
			robo_launcher.spawn_added.disconnect (on_spawn_added);
			robo_launcher.spawn_removed.disconnect (on_spawn_removed);

			if (android_helper_request != null) {
				try {
					var client = yield get_android_helper_client (cancellable);
					yield client.transport.close (cancellable);
				} catch (Error e) {
				}
			}
#endif

			if (spawn_gater != null) {
				spawn_gater.gating_cancelled.disconnect (on_spawn_gating_cancelled);
				spawn_gater.spawn_added.disconnect (on_spawn_added);
				spawn_gater.spawn_removed.disconnect (on_spawn_removed);
				spawn_gater.stop ();
				spawn_gater = null;
			}

			yield base.close (cancellable);

#if ANDROID
			if (crash_monitor != null) {
				crash_monitor.process_crashed.disconnect (on_process_crashed);
				yield crash_monitor.close (cancellable);
			}
#endif

			var linjector = (Linjector) injector;

			yield wait_for_uninject (injector, cancellable, () => {
				return linjector.any_still_injected ();
			});

			injector.uninjected.disconnect (on_uninjected);
			yield injector.close (cancellable);

			if (system_session_container != null) {
				yield system_session_container.destroy (cancellable);
				system_session_container = null;
			}

			yield helper.close (cancellable);
			helper.output.disconnect (on_output);

			agent = null;

			tempdir.destroy ();
		}

		protected override async AgentSessionProvider create_system_session_provider (Cancellable? cancellable,
				out DBusConnection connection) throws Error, IOError {
			unowned string arch_name = (sizeof (void *) == 8) ? "64" : "32";

			string? path = null;
			PathTemplate? tpl = null;
#if HAVE_EMBEDDED_ASSETS
			if (MemoryFileDescriptor.is_supported ()) {
				string agent_name = agent.name_template.expand (arch_name);
				AgentResource resource = agent.resources.first_match (r => r.name == agent_name);
				path = "/proc/self/fd/%d".printf (resource.get_memfd ().fd);
			} else {
				tpl = agent.get_path_template ();
			}
#else
			tpl = PathTemplate (Frida.agent_path);
#endif
			if (path == null)
				path = tpl.expand (arch_name);

#if HAVE_EMBEDDED_ASSETS
			try {
#endif
				system_session_container = yield AgentContainer.create (path, cancellable);
#if HAVE_EMBEDDED_ASSETS
			} catch (Error e) {
				if (MemoryFileDescriptor.is_supported ()) {
					path = agent.get_path_template ().expand (arch_name);
					system_session_container = yield AgentContainer.create (path, cancellable);
				} else {
					throw e;
				}
			}
#endif

			connection = system_session_container.connection;

			return system_session_container;
		}

		public override async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var opts = FrontmostQueryOptions._deserialize (options);
#if ANDROID
			var client = yield get_android_helper_client (cancellable);

			var app = yield client.get_frontmost_application (opts, cancellable);
			if (app.pid == 0)
				return app;

			if (opts.scope != MINIMAL) {
				var process_opts = new ProcessQueryOptions ();
				process_opts.select_pid (app.pid);
				process_opts.scope = METADATA;

				var processes = yield process_enumerator.enumerate_processes (process_opts);
				if (processes.length == 0)
					return HostApplicationInfo.empty ();

				add_app_process_state (app, processes[0].parameters);
			}

			return app;
#else
			return System.get_frontmost_application (opts);
#endif
		}

		public override async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var opts = ApplicationQueryOptions._deserialize (options);
#if ANDROID
			var client = yield get_android_helper_client (cancellable);

			var apps = yield client.enumerate_applications (opts, cancellable);

			if (opts.scope != MINIMAL) {
				var app_index_by_pid = new Gee.HashMap<uint, uint> ();
				int i = 0;
				foreach (var app in apps) {
					if (app.pid != 0)
						app_index_by_pid[app.pid] = i;
					i++;
				}

				if (!app_index_by_pid.is_empty) {
					var process_opts = new ProcessQueryOptions ();
					foreach (uint pid in app_index_by_pid.keys)
						process_opts.select_pid (pid);
					process_opts.scope = METADATA;

					var processes = yield process_enumerator.enumerate_processes (process_opts);

					foreach (var process in processes) {
						add_app_process_state (apps[app_index_by_pid[process.pid]], process.parameters);
						app_index_by_pid.unset (process.pid);
					}

					foreach (uint index in app_index_by_pid.values)
						apps[index].pid = 0;
				}
			}

			return apps;
#else
			return yield application_enumerator.enumerate_applications (opts);
#endif
		}

#if ANDROID
		private void add_app_process_state (HostApplicationInfo app, HashTable<string, Variant> process_params) {
			var app_params = app.parameters;
			app_params["user"] = process_params["user"];
			app_params["ppid"] = process_params["ppid"];
			app_params["started"] = process_params["started"];
		}
#endif

		public override async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var opts = ProcessQueryOptions._deserialize (options);
#if ANDROID
			var client = yield get_android_helper_client (cancellable);
			return yield client.enumerate_processes (opts, cancellable);
#else
			return yield process_enumerator.enumerate_processes (opts);
#endif
		}

		public override async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var helper_process = helper as LinuxHelperProcess;
			if (helper_process != null)
				yield helper_process.preload (cancellable);

#if ANDROID
			// RoboLauncher gates app launches; only add the system-wide eBPF gater for PROCESSES.
			// Let a start failure propagate — the caller opted in, so don't silently downgrade.
			if (SpawnGatingOptions._deserialize (options).scope == PROCESSES) {
				var gater = ensure_spawn_gater ();
				if (gater.state == STOPPED)
					gater.start ();
			}

			yield robo_launcher.enable_spawn_gating (cancellable);
#else
			// No portable way to tell a GUI app from any other execve here, so both scopes map to
			// the same system-wide gater.
			var gater = ensure_spawn_gater ();
			if (gater.state == STOPPED)
				gater.start ();
#endif
		}

		public override async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			spawn_gater?.stop ();

#if ANDROID
			yield robo_launcher.disable_spawn_gating (cancellable);
#endif

			spawn_gating_disabled (APPLICATION_REQUESTED);
		}

		private SpawnGater ensure_spawn_gater () {
			if (spawn_gater == null) {
				spawn_gater = new SpawnGater (helper);
				spawn_gater.gating_cancelled.connect (on_spawn_gating_cancelled);
				spawn_gater.spawn_added.connect (on_spawn_added);
				spawn_gater.spawn_removed.connect (on_spawn_removed);
			}
			return spawn_gater;
		}

		public override async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			var result = new HostSpawnInfo[0];

			if (spawn_gater != null) {
				foreach (var spawn in spawn_gater.enumerate_pending_spawn ())
					result += spawn;
			}

#if ANDROID
			foreach (var spawn in robo_launcher.enumerate_pending_spawn ())
				result += spawn;
#endif

			return result;
		}

		public override async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable)
				throws Error, IOError {
#if ANDROID
			if (!program.has_prefix ("/"))
				return yield robo_launcher.spawn (program, options, cancellable);
#endif

			UnixInputStream? stdin_stream = null;
			UnixOutputStream? stdout_stream = null;
			UnixOutputStream? stderr_stream = null;
			if (options.stdio == INHERIT) {
				stdin_stream = new UnixInputStream (0, false);
				stdout_stream = new UnixOutputStream (1, false);
				stderr_stream = new UnixOutputStream (2, false);
			}

			return yield helper.spawn (program, options, stdin_stream, stdout_stream, stderr_stream, cancellable);
		}

		protected override async void prepare_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield helper.prepare_exec_transition (pid, cancellable);
		}

		protected override async void await_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield helper.await_exec_transition (pid, cancellable);
		}

		protected override async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield helper.cancel_exec_transition (pid, cancellable);
		}

		protected override bool process_is_alive (uint pid) {
			return Posix.kill ((Posix.pid_t) pid, 0) == 0 || Posix.errno == Posix.EPERM;
		}

		public override async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			yield helper.input (pid, data, cancellable);
		}

		protected override async void perform_resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			if (spawn_gater != null && spawn_gater.try_resume (pid))
				return;

#if ANDROID
			if (robo_launcher.try_resume (pid))
				return;
#endif

			yield helper.resume (pid, cancellable);
		}

		public override async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
#if ANDROID
			var client = yield get_android_helper_client (cancellable);

			if (yield client.try_stop_package_by_pid (pid, cancellable))
				return;
#endif

			yield helper.kill (pid, cancellable);
		}

		protected override async Future<IOStream> perform_attach_to (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable, out Object? transport) throws Error, IOError {
			uint id;
			string entrypoint = "frida_agent_main";
			string parameters = make_agent_parameters (pid, "", options);
			AgentFeatures features = CONTROL_CHANNEL;
			var linjector = (Linjector) injector;
#if HAVE_EMBEDDED_ASSETS
			id = yield linjector.inject_library_resource (pid, agent, entrypoint, parameters, features, cancellable);
#else
			id = yield linjector.inject_library_file_with_template (pid, PathTemplate (Frida.agent_path), entrypoint,
				parameters, features, cancellable);
#endif
			injectee_by_pid[pid] = id;

			var stream_request = new Promise<IOStream> ();
			IOStream stream = yield linjector.request_control_channel (id, cancellable);
			stream_request.resolve (stream);

			transport = null;

			return stream_request.future;
		}

		protected override string? get_emulated_agent_path (uint pid) throws Error {
			unowned string name;
			switch (cpu_type_from_pid (pid)) {
				case Gum.CpuType.IA32:
					name = "frida-agent-arm.so";
					break;
				case Gum.CpuType.AMD64:
					name = "frida-agent-arm64.so";
					break;
				default:
					throw new Error.NOT_SUPPORTED ("Emulated realm is not supported on this architecture");
			}

			AgentResource? resource = agent.resources.first_match (r => r.name == name);
			if (resource == null)
				throw new Error.NOT_SUPPORTED ("Unable to handle emulated processes due to build configuration");

			return resource.get_file ().path;
		}

		public override async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
			var session = yield do_open_service (address, cancellable);

			var id = ServiceSessionId.generate ();
			service_session_registry.register (id, session);

			return id;
		}

		private async ServiceSession do_open_service (string address, Cancellable? cancellable) throws Error, IOError {
			string[] tokens = address.split (":", 2);
			unowned string protocol = tokens[0];

			if (protocol == "syscall-trace")
				return new LinuxSyscallTraceServiceSession ();

			throw new Error.NOT_SUPPORTED ("Unsupported service address");
		}

#if ANDROID
		internal async AndroidHelperClient get_android_helper_client (Cancellable? cancellable) throws Error, IOError {
			while (android_helper_request != null) {
				try {
					return yield android_helper_request.future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			android_helper_request = new Promise<AndroidHelperClient> ();

			try {
				string instance_id = Uuid.string_random ().replace ("-", "");
				string helper_path = "/data/local/tmp/frida-helper-" + instance_id + ".dex";
				FileUtils.set_data (helper_path, Frida.Data.Android.get_helper_dex_blob ().data);
				Posix.chmod (helper_path, 0644);

				try {
					var service = new AndroidHelperService ();
					yield service.start (helper_path, cancellable);

					var helper = new AndroidHelperClient (service);

					android_helper_request.resolve (helper);

					return helper;
				} finally {
					Posix.unlink (helper_path);
				}
			} catch (GLib.Error e) {
				var api_error = (e is Error) ? e : new Error.PERMISSION_DENIED ("%s", e.message);

				android_helper_request.reject (api_error);

				throw_api_error (api_error);
			}
		}

		protected override async CrashInfo? try_collect_crash (uint pid, Cancellable? cancellable) throws IOError {
			if (crash_monitor == null)
				return null;
			return yield crash_monitor.try_collect_crash (pid, cancellable);
		}

		private void on_process_crashed (CrashInfo info) {
			process_crashed (info);

			if (crash_monitor != null && still_attached_to (info.pid)) {
				/*
				 * May take a while as a Java fatal exception typically won't terminate the process until
				 * the user dismisses the dialog.
				 */
				crash_monitor.disable_crash_delivery_timeout (info.pid);
			}
		}
#endif

		private void on_spawn_gating_cancelled () {
			spawn_gating_disabled (WATCHDOG_TIMEOUT);
		}

		private void on_spawn_added (HostSpawnInfo info) {
			spawn_added (info);
		}

		private void on_spawn_removed (HostSpawnInfo info) {
			spawn_removed (info);
		}

		private void on_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, data);
		}
	}

#if ANDROID
	private sealed class AndroidHelperService : Object, AndroidHelperTransport {
		private MainLoop main_loop;
		private MainContext main_context = new MainContext ();
		private Thread<void>? worker_thread;

		private Android.Module? vm_mod;
		private SigchainCompat? sigchain_compat;
		private Android.Module? rt_mod;
		private JNI.InvokeInterface ** vm;
		private JNI.NativeInterface ** env;
		private JNI.ObjectRef * backend;
		private JNI.MethodID * handle_request_string;

		construct {
			main_loop = new MainLoop (main_context, false);
			worker_thread = new Thread<void> ("frida-android-helper", run);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			var promise = new Promise<bool> ();
			var caller_context = MainContext.ref_thread_default ();

			var source = new IdleSource ();
			source.set_callback (() => {
				close_and_fulfill (promise, caller_context);
				return Source.REMOVE;
			});
			source.attach (main_context);

			try {
				yield promise.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}

			worker_thread.join ();
			worker_thread = null;

			if (vm != null) {
				var res = (*vm)->destroy_java_vm (vm);
				assert (res == OK);
				vm = null;
			}

			rt_mod = null;
			vm_mod = null;

			if (sigchain_compat != null) {
				sigchain_compat.disable ();
				sigchain_compat = null;
			}
		}

		private void close_and_fulfill (Promise<bool> promise, MainContext caller_context) {
			do_close ();

			var source = new IdleSource ();
			source.set_callback (() => {
				promise.resolve (true);
				return Source.REMOVE;
			});
			source.attach (caller_context);
		}

		private void do_close () {
			if (env != null) {
				if (backend != null) {
					(*env)->delete_global_ref (env, backend);
					backend = null;
				}

				handle_request_string = null;

				var res = (*vm)->detach_current_thread (vm);
				assert (res == OK);
				env = null;
			}

			main_loop.quit ();
		}

		public async void start (string helper_path, Cancellable? cancellable) throws Error, IOError {
			var promise = new Promise<bool> ();
			var caller_context = MainContext.ref_thread_default ();

			var source = new IdleSource ();
			source.set_callback (() => {
				start_and_fulfill (helper_path, promise, caller_context);
				return Source.REMOVE;
			});
			source.attach (main_context);

			yield promise.future.wait_async (cancellable);
		}

		private void start_and_fulfill (string helper_path, Promise<bool> promise, MainContext caller_context) {
			Error? error = null;
			try {
				do_start (helper_path);
			} catch (Error e) {
				error = e;
			} finally {
				var source = new IdleSource ();
				source.set_callback (() => {
					if (error == null)
						promise.resolve (true);
					else
						promise.reject (error);
					return Source.REMOVE;
				});
				source.attach (caller_context);
			}
		}

		private void do_start (string helper_path) throws Error {
			var linker = Gum.Android.get_linker_module ();

			string vm_soname = (Gum.Android.get_api_level () >= 21) ? "libart.so" : "libdvm.so";

			Android.Namespace * art_ns = null;
			var get_exported_namespace = (GetExportedNamespaceFunc) linker.find_export_by_name (
				"__loader_android_get_exported_namespace");
			if (get_exported_namespace != null)
				art_ns = get_exported_namespace ("com_android_art");
			if (art_ns != null) {
				var dlopen_ext = (DlopenExtFunc) linker.find_export_by_name ("__loader_android_dlopen_ext");
				if (dlopen_ext == null)
					throw new Error.NOT_SUPPORTED ("Missing android_dlopen_ext");

				var info = Android.DlExtInfo ();
				info.flags = USE_NAMESPACE;
				info.library_namespace = art_ns;

				vm_mod = dlopen_ext (vm_soname, LAZY | LOCAL, info);
			} else {
				vm_mod = Android.Module.open (vm_soname, LAZY | LOCAL);

				if (vm_mod == null) {
					var libdir = ((sizeof (void *) == 8) ? "lib64" : "lib");
					ensure_directories_on_ld_library_path ({
						"/apex/com.android.runtime/" + libdir,
						"/apex/com.android.art/" + libdir,
						"/apex/com.android.conscrypt/" + libdir
					});

					vm_mod = Android.Module.open (vm_soname, LAZY | LOCAL);
				}
			}
			if (vm_mod == null)
				throw new Error.NOT_SUPPORTED ("Unable to load %s: %s", vm_soname, Android.Module.get_last_error ());
			var create_java_vm = (CreateVMFunc) vm_mod.symbol ("JNI_CreateJavaVM");
			assert (create_java_vm != null);

			var sigchain = Gum.Process.find_module_by_name ("libsigchain.so");
			if (sigchain != null) {
				bool is_stub = true;
				sigchain.enumerate_imports (imp => {
					if (imp.name == "sigaddset") {
						is_stub = false;
						return false;
					}
					return true;
				});
				if (is_stub) {
					sigchain_compat = new SigchainCompat ();
					sigchain_compat.make_current ();
					sigchain_compat.enable_for_module (sigchain);
				}
			}

			rt_mod = Android.Module.open ("libandroid_runtime.so", LAZY | LOCAL);
			assert (rt_mod != null);

			var args = JNI.VMInitArgs () {
				version = JNI.VERSION_1_2,
				options = {
					JNI.VMOption () { option_string = "-Djava.class.path=" + helper_path },
				},
			};

			var res = create_java_vm (out vm, out env, args);
			assert (res == OK);

			var register_natives = (RegisterFrameworkNativesFunc) rt_mod.symbol ("registerFrameworkNatives");
			if (register_natives != null) {
				res = register_natives (env);
				assert (res == OK);
			} else {
				var register_natives_legacy = (RegisterFrameworkNativesLegacyFunc) rt_mod.symbol (
					"Java_com_android_internal_util_WithFramework_registerNatives");
				assert (register_natives_legacy != null);

				res = register_natives_legacy (env, null);
				assert (res == OK);
			}

			(*env)->push_local_frame (env, 5);
			try {
				var backend_class = (*env)->find_class (env, "re/frida/HelperBackend");
				assert (backend_class != null);

				var ctor = (*env)->get_method_id (env, backend_class, "<init>", "()V");
				assert (ctor != null);

				handle_request_string = (*env)->get_method_id (env, backend_class, "handleRequestString",
					"(Ljava/lang/String;)Ljava/lang/String;");
				assert (handle_request_string != null);

				var backend_local = (*env)->new_object (env, backend_class, ctor);
				check_java_exception (env);

				backend = (*env)->new_global_ref (env, backend_local);
			} finally {
				(*env)->pop_local_frame (env, null);
			}
		}

		public async Json.Node request (Json.Node stanza, Cancellable? cancellable) throws Error, IOError {
			var promise = new Promise<Json.Node> ();
			var caller_context = MainContext.ref_thread_default ();

			var source = new IdleSource ();
			source.set_callback (() => {
				handle_request_and_fulfill (stanza, promise, caller_context);
				return Source.REMOVE;
			});
			source.attach (main_context);

			return yield promise.future.wait_async (cancellable);
		}

		private void handle_request_and_fulfill (Json.Node stanza, Promise<Json.Node> promise, MainContext caller_context) {
			Json.Node? response = null;
			Error? error = null;
			try {
				response = handle_request (stanza);
			} catch (Error e) {
				error = e;
			} finally {
				var source = new IdleSource ();
				source.set_callback (() => {
					if (error == null)
						promise.resolve (response);
					else
						promise.reject (error);
					return Source.REMOVE;
				});
				source.attach (caller_context);
			}
		}

		private Json.Node handle_request (Json.Node stanza) throws Error {
			(*env)->push_local_frame (env, 5);
			try {
				var stanza_val = (*env)->new_string_utf (env, Json.to_string (stanza, false));
				assert (stanza_val != null);

				var response_val = (*env)->call_object_method (env, backend, handle_request_string, stanza_val);
				check_java_exception (env);

				if (response_val != null) {
					unowned string response_str = (*env)->get_string_utf_chars (env, (JNI.StringRef *) response_val, null);
					try {
						return Json.from_string (response_str);
					} catch (GLib.Error e) {
						throw new Error.PROTOCOL ("%s", e.message);
					} finally {
						(*env)->release_string_utf_chars (env, (JNI.StringRef *) response_val, response_str);
					}
				} else {
					return new Json.Node.alloc ().init_null ();
				}
			} finally {
				(*env)->pop_local_frame (env, null);
			}
		}

		private void run () {
			main_context.push_thread_default ();
			main_loop.run ();
			main_context.pop_thread_default ();
		}

		private static void check_java_exception (JNI.NativeInterface ** env) throws Error {
			var throwable = (*env)->exception_occurred (env);
			if (throwable == null)
				return;

			(*env)->exception_clear (env);

			var throwable_class = (*env)->get_object_class (env, (JNI.ObjectRef *) throwable);
			var to_string = (*env)->get_method_id (env, throwable_class, "toString", "()Ljava/lang/String;");
			var text_val = (JNI.StringRef *) (*env)->call_object_method (env, (JNI.ObjectRef *) throwable, to_string);

			unowned string text_str = (*env)->get_string_utf_chars (env, text_val);
			var e = new Error.NOT_SUPPORTED ("%s", text_str);
			(*env)->release_string_utf_chars (env, text_val, text_str);

			(*env)->delete_local_ref (env, (JNI.ObjectRef *) text_val);
			(*env)->delete_local_ref (env, (JNI.ObjectRef *) throwable_class);
			(*env)->delete_local_ref (env, (JNI.ObjectRef *) throwable);

			throw e;
		}

		private static void ensure_directories_on_ld_library_path (string[] dirs) {
			var linker = Gum.Android.get_linker_module ();
			var get_path = (GetLdLibraryPathFunc) linker.find_export_by_name ("__loader_android_get_LD_LIBRARY_PATH");
			var update_path = (UpdateLdLibraryPathFunc) linker.find_export_by_name ("__loader_android_update_LD_LIBRARY_PATH");

			if (get_path == null || update_path == null)
				return;

			var path_buf = new char[4096];
			get_path (path_buf);

			var entries = new Gee.ArrayList<string> ();
			foreach (unowned string dir in dirs)
				entries.add (dir);

			unowned string old_path = (string) path_buf;
			foreach (var dir in old_path.split (":")) {
				if (!(dir in dirs))
					entries.add (dir);
			}

			string new_path = string.joinv (":", entries.to_array ());
			update_path (new_path);
		}

		[CCode (has_target = false)]
		private delegate Android.Module? DlopenExtFunc (string filename, Android.DlOpenFlags flags, Android.DlExtInfo info);

		[CCode (has_target = false)]
		private delegate Android.Namespace * GetExportedNamespaceFunc (string ns);

		[CCode (has_target = false)]
		private delegate JNI.Result CreateVMFunc (out JNI.InvokeInterface ** vm, out JNI.NativeInterface ** env,
			JNI.VMInitArgs vm_args);

		[CCode (has_target = false)]
		private delegate JNI.Result RegisterFrameworkNativesFunc (void * env);

		[CCode (has_target = false)]
		private delegate JNI.Result RegisterFrameworkNativesLegacyFunc (void * env, JNI.ClassRef * clazz);

		[CCode (has_target = false)]
		private delegate void GetLdLibraryPathFunc (char[] buf);

		[CCode (has_target = false)]
		private delegate void UpdateLdLibraryPathFunc (string path);
	}

	private class SigchainCompat : Object {
		private static unowned SigchainCompat? current_instance;

		private Gum.Interceptor interceptor;
		private ChainState[] chains;
		private bool initialized = false;
		private bool enabled = false;

		private struct ChainState {
			public bool claimed;
			public bool owns_signal;
			public bool skip_add;
			public CompatSigactionHandler previous_action;
			public CompatSigactionHandler user_action;
			public SigchainAction special0;
			public SigchainAction special1;
			public uint n_special;
		}

		private const int MAX_SPECIAL_HANDLERS = 2;

		public SigchainCompat () {
			interceptor = Gum.Interceptor.obtain ();
			chains = new ChainState[Android.NSIG];
		}

		public void make_current () {
			current_instance = this;
		}

		public void enable_for_module (Gum.Module sigchain) {
			initialize ();

			sigchain.enumerate_exports (e => {
				switch (e.name) {
					case "ClaimSignalChain":
						interceptor.replace ((void *) e.address, (void *) shim_claim_signal_chain);
						break;
					case "UnclaimSignalChain":
						interceptor.replace ((void *) e.address, (void *) shim_unclaim_signal_chain);
						break;
					case "InvokeUserSignalHandler":
						interceptor.replace ((void *) e.address, (void *) shim_invoke_user_signal_handler);
						break;
					case "InitializeSignalChain":
						interceptor.replace ((void *) e.address, (void *) shim_initialize_signal_chain);
						break;
					case "EnsureFrontOfChain":
						interceptor.replace ((void *) e.address, (void *) shim_ensure_front_of_chain);
						break;
					case "SetSpecialSignalHandlerFn":
						interceptor.replace ((void *) e.address, (void *) shim_set_special_signal_handler);
						break;
					case "AddSpecialSignalHandlerFn":
						interceptor.replace ((void *) e.address, (void *) shim_add_special_signal_handler);
						break;
					case "RemoveSpecialSignalHandlerFn":
						interceptor.replace ((void *) e.address, (void *) shim_remove_special_signal_handler);
						break;
					case "SkipAddSignalHandler":
						interceptor.replace ((void *) e.address, (void *) shim_skip_add_signal_handler);
						break;
					default:
						break;
				}
				return true;
			});

			enabled = true;
		}

		public void disable () {
			if (!enabled)
				return;

			if (current_instance == this)
				current_instance = null;

			for (int sig = 1; sig != Android.NSIG; sig++) {
				var chain = &chains[sig];
				if (!chain->claimed)
					continue;

				restore_previous_kernel_handler (sig);

				chain->claimed = false;
				chain->owns_signal = false;
				chain->skip_add = false;
				chain->n_special = 0;

				chain->special0.sc_sigaction = null;
				chain->special1.sc_sigaction = null;
				chain->special0.sc_flags = 0;
				chain->special1.sc_flags = 0;
				Posix.sigemptyset (out chain->special0.sc_mask);
				Posix.sigemptyset (out chain->special1.sc_mask);

				chain->previous_action = {};
				chain->user_action = {};
			}

			enabled = false;
		}

		private void initialize () {
			if (initialized)
				return;

			for (int i = 0; i != Android.NSIG; i++) {
				Posix.sigemptyset (out chains[i].special0.sc_mask);
				Posix.sigemptyset (out chains[i].special1.sc_mask);
				Posix.sigemptyset (out chains[i].user_action.sa_mask);
				Posix.sigemptyset (out chains[i].previous_action.sa_mask);
			}

			initialized = true;
		}

		private void claim_signal_chain (int sig, out CompatSigactionHandler old_action) {
			validate_signal (sig);

			var chain = &chains[sig];

			if (!chain->claimed)
				register_chain (sig);

			chain->owns_signal = true;
			chain->user_action = chain->previous_action;

			old_action = chain->previous_action;
		}

		private void unclaim_signal_chain (int sig) {
			validate_signal (sig);

			var chain = &chains[sig];
			if (!chain->claimed)
				return;

			chain->owns_signal = false;

			if (chain->n_special == 0) {
				restore_previous_kernel_handler (sig);
				chain->claimed = false;
			}
		}

		private void set_special_handler (int sig, SigchainAction action) {
			validate_signal (sig);

			var chain = &chains[sig];

			chain->special0 = action;
			chain->special1.sc_sigaction = null;
			chain->special1.sc_flags = 0;
			Posix.sigemptyset (out chain->special1.sc_mask);

			chain->n_special = 1;

			if (!chain->skip_add && !chain->claimed)
				register_chain (sig);
		}

		private void add_special_handler (int sig, SigchainAction action) {
			validate_signal (sig);

			var chain = &chains[sig];
			assert (chain->n_special < MAX_SPECIAL_HANDLERS);

			var slot = get_special_slot (chain, chain->n_special);
			*slot = action;
			chain->n_special++;

			if (!chain->skip_add && !chain->claimed)
				register_chain (sig);
		}

		private void remove_special_handler (int sig, void * fn) {
			validate_signal (sig);

			var chain = &chains[sig];

			for (uint i = 0; i != chain->n_special; i++) {
				var slot = get_special_slot (chain, i);
				if ((void *) slot->sc_sigaction == fn) {
					for (uint j = i + 1; j != chain->n_special; j++)
						*get_special_slot (chain, j - 1) = *get_special_slot (chain, j);

					var last = get_special_slot (chain, chain->n_special - 1);
					last->sc_sigaction = null;
					last->sc_flags = 0;
					Posix.sigemptyset (out last->sc_mask);

					chain->n_special--;

					if (!chain->owns_signal && chain->n_special == 0 && chain->claimed) {
						restore_previous_kernel_handler (sig);
						chain->claimed = false;
					}
					return;
				}
			}

			assert_not_reached ();
		}

		private void invoke_user_signal_handler (int sig, Posix.siginfo_t info, void * ucontext) {
			validate_signal (sig);

			var chain = &chains[sig];

			if (chain->owns_signal)
				chain_to_action (sig, info, ucontext, &chain->user_action);
			else
				chain_to_action (sig, info, ucontext, &chain->previous_action);
		}

		private void ensure_front_of_chain (int sig) {
			validate_signal (sig);

			var chain = &chains[sig];
			if (!chain->claimed)
				return;

			CompatSigactionSiginfo current_action;
			if (compat_sigaction_siginfo (sig, null, out current_action) != 0)
				assert_not_reached ();

			if ((void *) current_action.sa_sigaction != (void *) compat_signal_handler) {
				chain->user_action = compat_sigaction_siginfo_to_handler (current_action);
				reinstall_front_of_chain (sig);
			}
		}

		private void skip_add_signal_handler (bool val) {
			for (int i = 0; i != Android.NSIG; i++)
				chains[i].skip_add = val;
		}

		private void handle_signal (int sig, Posix.siginfo_t info, void * ucontext) {
			var chain = &chains[sig];

			for (uint i = 0; i != chain->n_special; i++) {
				var action = get_special_slot (chain, i);

				Posix.sigset_t previous_mask;
				Posix.sigprocmask (Posix.SIG_SETMASK, action->sc_mask, out previous_mask);

				bool handled = false;
				if (action->sc_sigaction != null)
					handled = action->sc_sigaction (sig, info, ucontext);

				Posix.sigset_t ignored_mask;
				Posix.sigprocmask (Posix.SIG_SETMASK, previous_mask, out ignored_mask);

				if (handled)
					return;
			}

			if (chain->owns_signal)
				chain_to_action (sig, info, ucontext, &chain->user_action);
			else
				chain_to_action (sig, info, ucontext, &chain->previous_action);
		}

		private void validate_signal (int sig) {
			assert (sig > 0 && sig < Android.NSIG);
		}

		private void register_chain (int sig) {
			var chain = &chains[sig];
			if (chain->claimed)
				return;

			CompatSigactionSiginfo new_action = {};
			Posix.sigfillset (out new_action.sa_mask);
			new_action.sa_sigaction = compat_signal_handler;
			new_action.sa_flags = Posix.SA_SIGINFO | Posix.SA_ONSTACK | Posix.SA_RESTART;

			CompatSigactionSiginfo old_action;
			if (compat_sigaction_siginfo (sig, &new_action, out old_action) != 0)
				assert_not_reached ();

			chain->previous_action = compat_sigaction_siginfo_to_handler (old_action);
			chain->claimed = true;
		}

		private void reinstall_front_of_chain (int sig) {
			var chain = &chains[sig];

			CompatSigactionSiginfo new_action = {};
			Posix.sigfillset (out new_action.sa_mask);
			new_action.sa_sigaction = compat_signal_handler;
			new_action.sa_flags = Posix.SA_SIGINFO | Posix.SA_ONSTACK | Posix.SA_RESTART;

			CompatSigactionSiginfo old_action;
			if (compat_sigaction_siginfo (sig, &new_action, out old_action) != 0)
				assert_not_reached ();

			chain->previous_action = compat_sigaction_siginfo_to_handler (old_action);
			chain->claimed = true;
		}

		private void restore_previous_kernel_handler (int sig) {
			var chain = &chains[sig];

			if (compat_sigaction_handler (sig, &chain->previous_action, null) != 0)
				assert_not_reached ();
		}

		private void chain_to_action (int sig, Posix.siginfo_t info, void * ucontext, CompatSigactionHandler * action) {
			if ((action->sa_flags & Posix.SA_SIGINFO) != 0) {
				var siginfo_action = compat_sigaction_handler_to_siginfo (*action);
				if ((void *) siginfo_action.sa_sigaction != null) {
					siginfo_action.sa_sigaction (sig, info, ucontext);
					return;
				}
			} else {
				if ((void *) action->sa_handler == (void *) Posix.SIG_IGN)
					return;

				if ((void *) action->sa_handler == (void *) Posix.SIG_DFL) {
					CompatSigactionHandler dfl = {};
					Posix.sigemptyset (out dfl.sa_mask);
					dfl.sa_handler = Posix.SIG_DFL;
					dfl.sa_flags = 0;

					CompatSigactionHandler ignored;
					compat_sigaction_handler (sig, &dfl, out ignored);
					return;
				}

				if ((void *) action->sa_handler != null) {
					action->sa_handler (sig);
					return;
				}
			}
		}

		private static CompatSigactionHandler compat_sigaction_siginfo_to_handler (CompatSigactionSiginfo action) {
			CompatSigactionHandler result = {};
			result.sa_handler = (CompatSignalHandler) action.sa_sigaction;
			result.sa_mask = action.sa_mask;
			result.sa_flags = action.sa_flags;
			result.sa_restorer = action.sa_restorer;
			return result;
		}

		private static CompatSigactionSiginfo compat_sigaction_handler_to_siginfo (CompatSigactionHandler action) {
			CompatSigactionSiginfo result = {};
			result.sa_sigaction = (CompatSiginfoHandler) action.sa_handler;
			result.sa_mask = action.sa_mask;
			result.sa_flags = action.sa_flags;
			result.sa_restorer = action.sa_restorer;
			return result;
		}

		private static SigchainAction * get_special_slot (ChainState * chain, uint index) {
			switch (index) {
				case 0:
					return &chain->special0;
				case 1:
					return &chain->special1;
				default:
					assert_not_reached ();
			}
		}

		private static unowned SigchainCompat get_current_instance () {
			unowned SigchainCompat compat = current_instance;
			assert (compat != null);
			return compat;
		}

		private static void shim_claim_signal_chain (int sig, out CompatSigactionHandler old_action) {
			get_current_instance ().claim_signal_chain (sig, out old_action);
		}

		private static void shim_unclaim_signal_chain (int sig) {
			get_current_instance ().unclaim_signal_chain (sig);
		}

		private static void shim_invoke_user_signal_handler (int sig, Posix.siginfo_t info, void * ucontext) {
			get_current_instance ().invoke_user_signal_handler (sig, info, ucontext);
		}

		private static void shim_initialize_signal_chain () {
		}

		private static void shim_set_special_signal_handler (int sig, SigchainAction sa) {
			get_current_instance ().set_special_handler (sig, sa);
		}

		private static void shim_add_special_signal_handler (int sig, SigchainAction sa) {
			get_current_instance ().add_special_handler (sig, sa);
		}

		private static void shim_remove_special_signal_handler (int sig, void * fn) {
			get_current_instance ().remove_special_handler (sig, fn);
		}

		private static void shim_ensure_front_of_chain (int sig) {
			get_current_instance ().ensure_front_of_chain (sig);
		}

		private static void shim_skip_add_signal_handler (bool val) {
			get_current_instance ().skip_add_signal_handler (val);
		}

		private static void compat_signal_handler (int sig, Posix.siginfo_t info, void * ucontext) {
			get_current_instance ().handle_signal (sig, info, ucontext);
		}

		private struct SigchainAction {
			public SigchainHandler sc_sigaction;
			public Posix.sigset_t sc_mask;
			public uint64 sc_flags;
		}

		[CCode (has_target = false)]
		private delegate bool SigchainHandler (int sig, Posix.siginfo_t info, void * ucontext);

		[CCode (cname = "sigaction", cheader_filename = "signal.h")]
		private extern static int compat_sigaction_handler (int signum, CompatSigactionHandler * act,
			out CompatSigactionHandler oldact);

		[CCode (cname = "sigaction", cheader_filename = "signal.h")]
		private extern static int compat_sigaction_siginfo (int signum, CompatSigactionSiginfo * act,
			out CompatSigactionSiginfo oldact);

#if X86_64 || ARM64
		private struct CompatSigactionHandler {
			public int sa_flags;
			public CompatSignalHandler sa_handler;
			public Posix.sigset_t sa_mask;
			public CompatSigRestorer sa_restorer;
		}

		private struct CompatSigactionSiginfo {
			public int sa_flags;
			public CompatSiginfoHandler sa_sigaction;
			public Posix.sigset_t sa_mask;
			public CompatSigRestorer sa_restorer;
		}
#else
		private struct CompatSigactionHandler {
			public CompatSignalHandler sa_handler;
			public Posix.sigset_t sa_mask;
			public int sa_flags;
			public CompatSigRestorer sa_restorer;
		}

		private struct CompatSigactionSiginfo {
			public CompatSiginfoHandler sa_sigaction;
			public Posix.sigset_t sa_mask;
			public int sa_flags;
			public CompatSigRestorer sa_restorer;
		}
#endif

		[CCode (has_target = false)]
		private delegate void CompatSignalHandler (int sig);

		[CCode (has_target = false)]
		private delegate void CompatSiginfoHandler (int sig, Posix.siginfo_t info, void * ucontext);

		[CCode (has_target = false)]
		private delegate void CompatSigRestorer ();
	}

	private sealed class RoboLauncher : Object {
		public signal void gating_cancelled ();
		public signal void spawn_added (HostSpawnInfo info);
		public signal void spawn_removed (HostSpawnInfo info);

		public weak LinuxHostSession host_session {
			get;
			construct;
		}

		public Cancellable io_cancellable {
			get;
			construct;
		}

		private Promise<bool> ensure_request;

		private string? server_name;
		private UnixSocketAddress? server_address;
		private Gee.Map<uint, ZymbiotePatches> zymbiote_patches = new Gee.HashMap<uint, ZymbiotePatches> ();
		private Gee.Map<uint, ZymbioteConnection> zymbiote_connections = new Gee.HashMap<uint, ZymbioteConnection> ();

		private bool spawn_gating_enabled = false;
		private Gee.HashMap<string, Promise<uint>> spawn_requests = new Gee.HashMap<string, Promise<uint>> ();
		private Gee.HashMap<uint, HostSpawnInfo?> pending_spawn = new Gee.HashMap<uint, HostSpawnInfo?> ();
		private SpawnGatingWatchdog watchdog = new SpawnGatingWatchdog ();

		private delegate void CompletionNotify (GLib.Error? error);

		private const string CHROME_ZYGOTE_PACKAGE_NAME = "com.android.chrome_zygote";

		public RoboLauncher (LinuxHostSession host_session, Cancellable io_cancellable) {
			Object (
				host_session: host_session,
				io_cancellable: io_cancellable
			);
		}

		construct {
			watchdog.expired.connect (on_watchdog_expired);
		}

		private void on_watchdog_expired () {
			disable_spawn_gating.begin (io_cancellable);
			gating_cancelled ();
		}

		public async void preload (Cancellable? cancellable) throws Error, IOError {
			yield ensure_loaded (cancellable);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			watchdog.clear ();

			if (ensure_request != null) {
				try {
					yield ensure_loaded (cancellable);
				} catch (GLib.Error e) {
				}
			}

			foreach (var request in spawn_requests.values.to_array ())
				request.reject (new Error.INVALID_OPERATION ("Cancelled by shutdown"));
			spawn_requests.clear ();

			foreach (var e in zymbiote_patches.entries) {
				uint pid = e.key;
				var patches = e.value;
				try {
					Posix.kill ((Posix.pid_t) pid, Posix.Signal.STOP);
					yield wait_until_stopped (pid, cancellable);
					try {
						var fd = open_process_memory (pid);
						patches.revert (fd);
					} finally {
						Posix.kill ((Posix.pid_t) pid, Posix.Signal.CONT);
					}
				} catch (Error e) {
				}
			}
			zymbiote_patches.clear ();
		}

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield ensure_loaded (cancellable);

			spawn_gating_enabled = true;
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			spawn_gating_enabled = false;

			watchdog.clear ();

			var pending = pending_spawn.values.to_array ();
			pending_spawn.clear ();
			foreach (var spawn in pending) {
				spawn_removed (spawn);

				host_session.resume.begin (spawn.pid, io_cancellable);
			}
		}

		public HostSpawnInfo[] enumerate_pending_spawn () {
			var result = new HostSpawnInfo[pending_spawn.size];
			var index = 0;
			foreach (var spawn in pending_spawn.values)
				result[index++] = spawn;
			return result;
		}

		public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			string package = program;

			if (options.has_argv)
				throw new Error.NOT_SUPPORTED ("The 'argv' option is not supported when spawning Android apps");

			if (options.has_envp)
				throw new Error.NOT_SUPPORTED ("The 'envp' option is not supported when spawning Android apps");

			if (options.has_env)
				throw new Error.NOT_SUPPORTED ("The 'env' option is not supported when spawning Android apps");

			if (options.cwd.length > 0)
				throw new Error.NOT_SUPPORTED ("The 'cwd' option is not supported when spawning Android apps");

			if (options.stdio != INHERIT)
				throw new Error.NOT_SUPPORTED ("Redirected stdio is not supported when spawning Android apps");

			var entrypoint = PackageEntrypoint.parse (package, options);

			yield ensure_loaded (cancellable);

			var helper = yield host_session.get_android_helper_client (cancellable);

			var process_name = yield helper.get_process_name (package, entrypoint.uid, cancellable);

			if (spawn_requests.has_key (process_name))
				throw new Error.INVALID_OPERATION ("Spawn already in progress for the specified package name");

			var request = new Promise<uint> ();
			spawn_requests[process_name] = request;

			uint pid = 0;
			try {
				yield helper.stop_package (package, entrypoint.uid, cancellable);
				yield helper.start_package (package, entrypoint, cancellable);

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
			} catch (GLib.Error e) {
				if (!spawn_requests.unset (process_name)) {
					var pending_pid = request.future.value;
					if (pending_pid != 0)
						host_session.resume.begin (pending_pid, io_cancellable);
				}

				throw_api_error (e);
			}

			return pid;
		}

		public bool try_resume (uint pid) {
			ZymbioteConnection? connection;
			if (!zymbiote_connections.unset (pid, out connection))
				return false;

			connection.resume.begin (io_cancellable);

			HostSpawnInfo? info;
			if (pending_spawn.unset (pid, out info)) {
				watchdog.cancel (pid);
				spawn_removed (info);
			}

			return true;
		}

		private async void ensure_loaded (Cancellable? cancellable) throws Error, IOError {
			while (ensure_request != null) {
				try {
					yield ensure_request.future.wait_async (cancellable);
					return;
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			ensure_request = new Promise<bool> ();

			if (server_name == null) {
				string name = "/frida-zymbiote-" + Uuid.string_random ().replace ("-", "");
				var address = new UnixSocketAddress.with_type (name, -1, UnixSocketAddressType.ABSTRACT);

				try {
					var socket = new Socket (SocketFamily.UNIX, SocketType.STREAM, SocketProtocol.DEFAULT);
					socket.bind (address, true);
					socket.listen ();

					server_name = name;
					server_address = address;

					handle_zymbiote_connections.begin (socket);
				} catch (GLib.Error raw_err) {
					var err = new Error.TRANSPORT ("%s", raw_err.message);
					ensure_request.reject (err);
					throw err;
				}
			}

			uint pending = 1;
			GLib.Error? first_error = null;

			CompletionNotify on_complete = error => {
				pending--;
				if (error != null && first_error == null)
					first_error = error;

				if (pending == 0) {
					var source = new IdleSource ();
					source.set_callback (ensure_loaded.callback);
					source.attach (MainContext.get_thread_default ());
				}
			};

			foreach (HostProcessInfo info in System.enumerate_processes (new ProcessQueryOptions ())) {
				var name = info.name;
				if (name == "zygote" || name == "zygote64" ||
						name == "usap32" || name == "usap64" ||
						name == CHROME_ZYGOTE_PACKAGE_NAME) {
					uint pid = info.pid;
					if (zymbiote_patches.has_key (pid))
						continue;

					pending++;
					do_inject_zygote_agent.begin (pid, name, cancellable, on_complete);
				}
			}

			on_complete (null);

			yield;

			on_complete = null;

			if (first_error == null) {
				ensure_request.resolve (true);
			} else {
				ensure_request.reject (first_error);
				ensure_request = null;

				throw_api_error (first_error);
			}
		}

		private async void do_inject_zygote_agent (uint pid, string name, Cancellable? cancellable, CompletionNotify on_complete) {
			try {
				yield inject_zymbiote (pid, cancellable);

				on_complete (null);
			} catch (GLib.Error e) {
				if (is_transient_usap_patch_failure (name, e)) {
					on_complete (null);
					return;
				}

				on_complete (e);
			}
		}

		private static bool is_transient_usap_patch_failure (string process_name, GLib.Error error) {
			return process_name.has_prefix ("usap") &&
				error is Error.NOT_SUPPORTED &&
				error.message == "Unable to locate android.os.Process.setArgV0() slot; please file a bug";
		}

		private async void inject_zymbiote (uint pid, Cancellable? cancellable) throws Error, IOError {
			var prep = yield prepare_zymbiote_injection (pid, cancellable);

			Posix.kill ((Posix.pid_t) pid, Posix.Signal.STOP);
			yield wait_until_stopped (pid, cancellable);
			try {
				var patches = new ZymbiotePatches ();

				unowned uint8[] payload = prep.payload.get_data ();
				if (prep.already_patched) {
					var handle = Posix.open (prep.payload_path, Posix.O_RDONLY);
					if (handle == -1) {
						throw new Error.PERMISSION_DENIED ("Unable to open payload backing file: %s",
							strerror (errno));
					}
					var backing_file = new FileDescriptor (handle);
					var original = new uint8[payload.length];
					backing_file.pread_all (original, prep.payload_file_offset);
					patches.apply (payload, prep.process_memory, prep.payload_base, new Bytes.take ((owned) original));
				} else {
					patches.apply (payload, prep.process_memory, prep.payload_base);
				}

				if (prep.already_patched) {
					patches.apply (prep.replaced_setargv0_ptr, prep.process_memory, prep.setargv0_slot,
						new Bytes (prep.original_setargv0_ptr));
				} else {
					patches.apply (prep.replaced_setargv0_ptr, prep.process_memory, prep.setargv0_slot);
				}

				if (prep.setcontext_slot != 0) {
					if (prep.already_patched) {
						patches.apply (prep.replaced_setcontext_ptr, prep.process_memory, prep.setcontext_slot,
							new Bytes (prep.original_setcontext_ptr));
					} else {
						patches.apply (prep.replaced_setcontext_ptr, prep.process_memory, prep.setcontext_slot);
					}
				}

				zymbiote_patches[pid] = patches;
			} finally {
				Posix.kill ((Posix.pid_t) pid, Posix.Signal.CONT);
			}
		}

		private class ZymbiotePrepResult : Object {
			public FileDescriptor process_memory;
			public bool already_patched;

			public uint64 setargv0_slot;
			public uint8[] original_setargv0_ptr;
			public uint8[] replaced_setargv0_ptr;

			public uint64 setcontext_slot;
			public uint8[] original_setcontext_ptr;
			public uint8[] replaced_setcontext_ptr;

			public Bytes payload;
			public uint64 payload_base;
			public string payload_path;
			public uint64 payload_file_offset;
		}

		private async ZymbiotePrepResult prepare_zymbiote_injection (uint pid, Cancellable? cancellable) throws Error, IOError {
			var task = new Task (this, cancellable, (obj, res) => {
				prepare_zymbiote_injection.callback ();
			});
			task.set_task_data ((void *) pid, null);
			task.run_in_thread ((t, source_object, task_data, c) => {
				unowned RoboLauncher launcher = (RoboLauncher) t.get_unowned_source_object ();
				uint pid_to_prep = (uint) task_data;
				try {
					var r = do_prepare_zymbiote_injection (pid_to_prep, launcher.server_name);
					t.return_pointer ((owned) r, Object.unref);
				} catch (GLib.Error e) {
					t.return_error ((owned) e);
				}
			});
			yield;

			try {
				return (ZymbiotePrepResult) (owned) task.propagate_pointer ();
			} catch (GLib.Error e) {
				throw_api_error (e);
			}
		}

		private static ZymbiotePrepResult do_prepare_zymbiote_injection (uint pid, string server_name) throws Error, IOError {
			uint64 payload_base = 0;
			int payload_original_protection = 0;
			unowned string? payload_path = null;
			uint64 payload_file_offset = 0;
			unowned string? libc_path = null;
			unowned string? libselinux_path = null;
			unowned string? runtime_path = null;
			Gee.List<HeapCandidate> heap_candidates = new Gee.ArrayList<HeapCandidate> ();

			var maps = ProcMapsSnapshot.from_pid (pid);
			string raw_maps;
			try {
				FileUtils.get_contents ("/proc/%u/maps".printf (pid), out raw_maps);
			} catch (FileError e) {
				throw new Error.PROCESS_NOT_FOUND ("%s", e.message);
			}

			foreach (var m in maps) {
				unowned string path = m.path;
				if (payload_base == 0 && m.executable &&
						(path.has_suffix ("/libstagefright.so") || path.has_suffix ("/libmedia.so"))) {
					payload_base = m.end - Gum.query_page_size ();
					payload_original_protection = Posix.PROT_READ | Posix.PROT_EXEC;
					if (m.writable)
						payload_original_protection |= Posix.PROT_WRITE;
					payload_path = path;
					payload_file_offset = m.file_offset;
				} else if (path.has_suffix ("/libc.so")) {
					if (libc_path == null)
						libc_path = path;
				} else if (path.has_suffix ("/libselinux.so")) {
					if (libselinux_path == null)
						libselinux_path = path;
				} else if (path.has_suffix ("/libandroid_runtime.so")) {
					if (runtime_path == null)
						runtime_path = path;
				} else if (is_boot_heap (m)) {
					heap_candidates.add (new HeapCandidate () {
						start = m.start,
						end = m.end,
					});
				}
			}

			MatchInfo info;
			if (!/^([0-9a-f]+)-([0-9a-f]+) rw-p 00000000 00:00 0 *$/m.match (raw_maps, 0, out info)) {
				assert_not_reached ();
			}
			assert (info != null);

			while (info.matches ()) {
				uint64 start = uint64.parse (info.fetch (1), 16);
				uint64 end = uint64.parse (info.fetch (2), 16);

				heap_candidates.add (new HeapCandidate () {
					start = start,
					end = end,
				});

				try {
					info.next ();
				} catch (RegexError e) {
					break;
				}
			}

			if (payload_base == 0)
				throw new Error.NOT_SUPPORTED ("Unable to pick a payload base");
			if (libc_path == null)
				throw new Error.NOT_SUPPORTED ("Unable to detect libc.so path");
			if (runtime_path == null)
				throw new Error.NOT_SUPPORTED ("Unable to detect libandroid_runtime.so path");

			var libc_mapping = maps.find_module_by_path (libc_path);
			if (libc_mapping == null)
				throw new Error.NOT_SUPPORTED ("Unable to detect mapping for %s", libc_path);

			var runtime_mapping = maps.find_module_by_path (runtime_path);
			if (runtime_mapping == null)
				throw new Error.NOT_SUPPORTED ("Unable to detect mapping for %s", runtime_path);

			Gum.ElfModule libc;
			try {
				libc = new Gum.ElfModule.from_file (libc_path);
			} catch (Gum.Error e) {
				throw new Error.NOT_SUPPORTED ("Unable to parse %s: %s", libc_path, e.message);
			}

			uint64 original_setcontext = 0;
			if (libselinux_path != null) {
				Gum.ElfModule? libselinux;
				try {
					libselinux = new Gum.ElfModule.from_file (libselinux_path);
				} catch (Gum.Error e) {
					throw new Error.NOT_SUPPORTED ("Unable to parse %s: %s", libselinux_path, e.message);
				}

				var libselinux_mapping = maps.find_module_by_path (libselinux_path);
				if (libselinux_mapping == null)
					throw new Error.NOT_SUPPORTED ("Unable to detect mapping for %s", libselinux_path);

				libselinux.enumerate_exports (e => {
					if (e.name == "selinux_android_setcontext") {
						original_setcontext = libselinux_mapping.start + e.address;
						return false;
					}
					return true;
				});
			}

			Gum.ElfModule runtime;
			try {
				runtime = new Gum.ElfModule.from_file (runtime_path);
			} catch (Gum.Error e) {
				throw new Error.NOT_SUPPORTED ("Unable to parse %s: %s", runtime_path, e.message);
			}

			uint64 original_setargv0 = 0;
			uint64 setcontext_slot = 0;
			runtime.enumerate_exports (e => {
				if (e.name == "_Z27android_os_Process_setArgV0P7_JNIEnvP8_jobjectP8_jstring") {
					original_setargv0 = runtime_mapping.start + e.address;
					return false;
				}
				return true;
			});
			if (original_setcontext != 0) {
				runtime.enumerate_imports (i => {
					if (i.name == "selinux_android_setcontext") {
						setcontext_slot = runtime_mapping.start + i.slot;
						return false;
					}
					return true;
				});
			}
			if (original_setargv0 == 0)
				throw new Error.NOT_SUPPORTED ("Unable to locate android.os.Process.setArgV0(); please file a bug");

			uint pointer_size = libc.pointer_size;

			uint64 replacement_setargv0;
			uint64 replacement_setcontext;
			Bytes payload = make_zymbiote_payload (server_name, payload_base, payload_original_protection, libc, libc_mapping,
				original_setargv0, original_setcontext, out replacement_setargv0, out replacement_setcontext);

			var fd = open_process_memory (pid);

			var original_setargv0_ptr = new uint8[pointer_size];
			var original_setcontext_ptr = new uint8[pointer_size];
			var replaced_setargv0_ptr = new uint8[pointer_size];
			var replaced_setcontext_ptr = new uint8[pointer_size];

			(new Buffer (new Bytes.static (original_setargv0_ptr), ByteOrder.HOST, pointer_size))
				.write_pointer (0, original_setargv0);
			(new Buffer (new Bytes.static (original_setcontext_ptr), ByteOrder.HOST, pointer_size))
				.write_pointer (0, original_setcontext);
			(new Buffer (new Bytes.static (replaced_setargv0_ptr), ByteOrder.HOST, pointer_size))
				.write_pointer (0, replacement_setargv0);
			(new Buffer (new Bytes.static (replaced_setcontext_ptr), ByteOrder.HOST, pointer_size))
				.write_pointer (0, replacement_setcontext);

			uint64 setargv0_slot = 0;
			bool already_patched = false;
			foreach (var candidate in heap_candidates) {
				var heap = new uint8[candidate.size];
				size_t n;
				try {
					n = fd.pread (heap, candidate.start);
				} catch (Error e) {
					continue;
				}
				if (n != heap.length)
					continue;

				void * p = memmem (heap, original_setargv0_ptr);
				if (p == null) {
					p = memmem (heap, replaced_setargv0_ptr);
					already_patched = p != null;
				}

				if (p != null) {
					setargv0_slot = candidate.start + ((uint8 *) p - (uint8 *) heap);
					break;
				}
			}
			if (setargv0_slot == 0)
				throw new Error.NOT_SUPPORTED ("Unable to locate android.os.Process.setArgV0() slot; please file a bug");

			return new ZymbiotePrepResult () {
				process_memory = fd,
				already_patched = already_patched,

				setargv0_slot = setargv0_slot,
				original_setargv0_ptr = original_setargv0_ptr,
				replaced_setargv0_ptr = replaced_setargv0_ptr,

				setcontext_slot = setcontext_slot,
				original_setcontext_ptr = original_setcontext_ptr,
				replaced_setcontext_ptr = replaced_setcontext_ptr,

				payload = payload,
				payload_base = payload_base,
				payload_path = payload_path,
				payload_file_offset = payload_file_offset,
			};
		}

		private class HeapCandidate {
			public uint64 start;
			public uint64 end;
			public size_t size {
				get {
					return (size_t) (end - start);
				}
			}
		}

		private static Bytes make_zymbiote_payload (string server_name, uint64 payload_base, int payload_original_protection,
				Gum.ElfModule libc, ProcMapsSnapshot.Mapping libc_mapping, uint64 original_setargv0,
				uint64 original_setcontext, out uint64 replacement_setargv0, out uint64 replacement_setcontext) {
			var pointer_size = libc.pointer_size;

			var blob = (pointer_size == 8)
#if ARM || ARM64
				? Frida.Data.Android.get_zymbiote_arm64_elf_blob ()
				: Frida.Data.Android.get_zymbiote_arm_elf_blob ();
#else
				? Frida.Data.Android.get_zymbiote_x86_64_elf_blob ()
				: Frida.Data.Android.get_zymbiote_x86_elf_blob ();
#endif

			Gum.ElfModule zymbiote;
			try {
				zymbiote = new Gum.ElfModule.from_blob (new Bytes.static (blob.data));
			} catch (Gum.Error e) {
				assert_not_reached ();
			}

			Gum.ElfSegmentDetails? text = null;
			zymbiote.enumerate_segments (s => {
				if ((s.protection & Gum.PageProtection.EXECUTE) != 0) {
					text = s;
					return false;
				}
				return true;
			});
			assert (text != null);

			uint64 setargv0 = 0;
			uint64 setcontext = 0;
			zymbiote.enumerate_exports (e => {
				if (e.name == "frida_zymbiote_replacement_setargv0")
					setargv0 = payload_base + (e.address - text.vm_address);
				else if (e.name == "frida_zymbiote_replacement_setcontext")
					setcontext = payload_base + (e.address - text.vm_address);
				return true;
			});
			assert (setargv0 != 0);
			assert (setcontext != 0);
			replacement_setargv0 = setargv0;
			replacement_setcontext = setcontext;

			unowned uint8[] payload_template = blob.data[text.file_offset:text.file_offset + text.file_size];

			void * p = memmem (payload_template, "/frida-zymbiote-00000000000000000000000000000000".data);
			assert (p != null);
			size_t data_offset = (uint8 *) p - (uint8 *) payload_template;

			var payload = new Buffer (new Bytes (payload_template), ByteOrder.HOST, pointer_size);

			size_t cursor = data_offset;
			payload.write_string (cursor, server_name);
			cursor += 64;

			payload.write_pointer (cursor, payload_base);
			cursor += pointer_size;

			payload.write_pointer (cursor, payload_template.length);
			cursor += pointer_size;

			payload.write_pointer (cursor, payload_original_protection);
			cursor += pointer_size;

			cursor += pointer_size;

			payload.write_pointer (cursor, original_setcontext);
			cursor += pointer_size;

			payload.write_pointer (cursor, original_setargv0);
			cursor += pointer_size;

			string[] wanted = {
				"mprotect",
				"strdup",
				"free",
				"socket",
				"connect",
				"__errno",
				"getpid",
				"getppid",
				"sendmsg",
				"recv",
				"close",
				"raise",
			};

			var index_of = new Gee.HashMap<string, int> ();
			for (int i = 0; i != wanted.length; i++)
				index_of[wanted[i]] = i;

			var addrs = new uint64[wanted.length];
			uint pending = wanted.length;
			libc.enumerate_exports (e => {
				if (index_of.has_key (e.name)) {
					int idx = index_of[e.name];
					addrs[idx] = libc_mapping.start + e.address;
					pending--;
				}
				return pending != 0;
			});

			for (int i = 0; i != addrs.length; i++) {
				assert (addrs[i] != 0);
				payload.write_pointer (cursor, addrs[i]);
				cursor += pointer_size;
			}

			return payload.bytes;
		}

		private static bool is_boot_heap (ProcMapsSnapshot.Mapping m) {
			if (!m.readable || !m.writable || m.executable || m.shared)
				return false;
			return
				"boot.art" in m.path ||
				"boot-framework.art" in m.path ||
				"dalvik-LinearAlloc" in m.path;
		}

		[CCode (cname = "memmem", cheader_filename = "string.h")]
		private extern static void * memmem (uint8[] haystack, uint8[] needle);

		private async void handle_zymbiote_connections (Socket server_socket) {
			var listener = new SocketListener ();
			try {
				listener.add_socket (server_socket, null);

				while (true) {
					var raw_connection = (UnixConnection) yield listener.accept_async (io_cancellable);
					var connection = new ZymbioteConnection (raw_connection);
					handle_zymbiote_connection.begin (connection);
				}
			} catch (GLib.Error e) {
			} finally {
				listener.close ();
			}
		}

		private async void handle_zymbiote_connection (ZymbioteConnection connection) {
			try {
				var hello = yield connection.read_hello (io_cancellable);

				if (hello.process_name == CHROME_ZYGOTE_PACKAGE_NAME) {
					var patches = zymbiote_patches[hello.ppid];
					if (patches != null)
						zymbiote_patches[hello.pid] = patches;
					/*
					 * By dropping the connection without sending an ack, zymbiote knows it
					 * should stick around and not be prepared to have its hooks reverted.
					 */
					return;
				}

				connection.patches_to_revert = zymbiote_patches[hello.ppid];

				bool needs_resume = false;

				string package_name = hello.process_name;
				int colon = package_name.index_of_char (':');
				if (colon != -1)
					package_name = package_name[:colon];

				Promise<uint> spawn_request;
				if (spawn_requests.unset (package_name, out spawn_request)) {
					spawn_request.resolve (hello.pid);
					needs_resume = true;
				} else if (spawn_gating_enabled) {
					var spawn_info = HostSpawnInfo (hello.pid, hello.process_name);
					pending_spawn[hello.pid] = spawn_info;
					watchdog.arm (hello.pid);
					spawn_added (spawn_info);
					needs_resume = true;
				}

				if (needs_resume)
					zymbiote_connections[hello.pid] = connection;
				else
					connection.resume.begin (io_cancellable);
			} catch (GLib.Error e) {
			}
		}

		private class ZymbiotePatches {
			public Gee.Queue<Entry> entries = new Gee.ArrayQueue<Entry> ();

			public class Entry {
				public uint64 address;
				public Bytes original;
			}

			public void apply (uint8[] patch, FileDescriptor fd, uint64 address, Bytes? original = null) throws Error {
				Bytes? orig = original;

				if (orig == null) {
					var buf = new uint8[patch.length];
					fd.pread (buf, address);
					orig = new Bytes.take ((owned) buf);
				}

				fd.pwrite (patch, address);

				entries.offer (new Entry () {
					address = address,
					original = orig,
				});
			}

			public void revert (FileDescriptor fd) throws Error {
				foreach (var e in entries)
					fd.pwrite (e.original.get_data (), e.address);
			}
		}

		private class ZymbioteConnection : Object {
			public ZymbiotePatches? patches_to_revert = null;

			private UnixConnection connection;
			private DataInputStream input;

			private Hello? hello = null;

			public ZymbioteConnection (UnixConnection conn) {
				connection = conn;

				input = new DataInputStream (conn.get_input_stream ());
				input.byte_order = HOST_ENDIAN;
			}

			public async Hello read_hello (Cancellable? cancellable) throws Error, IOError {
				size_t header_size = 12;

				try {
					yield prepare_to_read (header_size, cancellable);

					uint32 process_name_len = 0;
					input.peek ((uint8[]) &process_name_len, 8);

					size_t message_size = header_size + process_name_len;

					yield prepare_to_read (message_size, cancellable);

					var r = new BufferReader (new Buffer (input.read_bytes (message_size, cancellable)));
					uint32 pid = r.read_uint32 ();
					uint32 ppid = r.read_uint32 ();
					r.skip (4);
					string process_name = r.read_fixed_string (process_name_len);

					hello = new Hello () {
						pid = pid,
						ppid = ppid,
						process_name = process_name,
					};

					return hello;
				} catch (GLib.Error e) {
					if (e is Error)
						throw (Error) e;
					throw new Error.TRANSPORT ("%s", e.message);
				}
			}

			public class Hello {
				public uint pid;
				public uint ppid;
				public string process_name;
			}

			public async void resume (Cancellable? cancellable) throws Error, IOError {
				var priority = Priority.DEFAULT;

				try {
					uint8 ack[1] = { 0x42 };
					yield connection.get_output_stream ().write_async (ack, priority, cancellable);

					uint8 bye[1];
					yield input.read_async (bye, priority, cancellable);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("%s", e.message);
				}

				yield wait_until_stopped (hello.pid, cancellable);

				if (patches_to_revert != null)
					patches_to_revert.revert (open_process_memory (hello.pid));

				Posix.kill ((Posix.pid_t) hello.pid, Posix.Signal.CONT);
			}

			private async void prepare_to_read (size_t required, Cancellable? cancellable) throws GLib.Error {
				while (true) {
					size_t available = input.get_available ();
					if (available >= required)
						return;
					ssize_t n = yield input.fill_async ((ssize_t) (required - available), Priority.DEFAULT,
						cancellable);
					if (n == 0)
						throw new Error.TRANSPORT ("Connection closed");
				}
			}
		}
	}

	private sealed class CrashMonitor : Object {
		public signal void process_crashed (CrashInfo crash);

		private Object logcat;

		private DataInputStream input;
		private Cancellable io_cancellable = new Cancellable ();

		private Gee.HashMap<uint, CrashDelivery> crash_deliveries = new Gee.HashMap<uint, CrashDelivery> ();
		private Gee.HashMap<uint, CrashBuilder> crash_builders = new Gee.HashMap<uint, CrashBuilder> ();

		private Timer since_start;

		construct {
			since_start = new Timer ();

			start_monitoring.begin ();
		}

		public async void close (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			if (logcat != null) {
				if (logcat is Subprocess) {
					var process = logcat as Subprocess;
					process.send_signal (Posix.Signal.TERM);
				} else if (logcat is SuperSU.Process) {
					var process = logcat as SuperSU.Process;
					yield process.detach (cancellable); // TODO: Figure out how we can terminate it.
				}
				logcat = null;
			}
		}

		public async CrashInfo? try_collect_crash (uint pid, Cancellable? cancellable) throws IOError {
			var delivery = get_crash_delivery_for_pid (pid);
			try {
				return yield delivery.future.wait_async (cancellable);
			} catch (Error e) {
				return null;
			}
		}

		public void disable_crash_delivery_timeout (uint pid) {
			var delivery = crash_deliveries[pid];
			if (delivery != null)
				delivery.disable_timeout ();
		}

		private void on_crash_received (CrashInfo crash) {
			var delivery = get_crash_delivery_for_pid (crash.pid);
			delivery.complete (crash);

			process_crashed (crash);
		}

		private void on_log_entry (LogEntry entry) {
			if (since_start.elapsed () < 2.0)
				return;

			if (entry.tag == "libc") {
				var delivery = get_crash_delivery_for_pid (entry.pid);
				delivery.extend_timeout ();
				return;
			}

			bool is_java_crash = entry.message.has_prefix ("FATAL EXCEPTION: ");
			if (is_java_crash) {
				try {
					var crash = parse_java_report (entry.message);
					on_crash_received (crash);
				} catch (Error e) {
				}

				return;
			}

			var builder = get_crash_builder_for_reporter_pid (entry.pid);
			builder.append (entry.message);
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

		private CrashBuilder get_crash_builder_for_reporter_pid (uint pid) {
			var builder = crash_builders[pid];
			if (builder == null) {
				builder = new CrashBuilder (pid);
				builder.completed.connect (on_crash_builder_completed);
				crash_builders[pid] = builder;
			}
			return builder;
		}

		private void on_crash_builder_completed (CrashBuilder builder, string report) {
			crash_builders.unset (builder.reporter_pid);

			try {
				var crash = parse_native_report (report);
				on_crash_received (crash);
			} catch (Error e) {
			}
		}

		private static CrashInfo parse_java_report (string report) throws Error {
			MatchInfo info;
			if (!/^Process: (.+), PID: (\d+)$/m.match (report, 0, out info)) {
				throw new Error.INVALID_ARGUMENT ("Malformed Java crash report");
			}

			string process_name = info.fetch (1);

			string raw_pid = info.fetch (2);
			uint pid = (uint) uint64.parse (raw_pid);

			string summary = summarize_java_report (report);

			return CrashInfo (pid, process_name, summary, report);
		}

		private static CrashInfo parse_native_report (string report) throws Error {
			MatchInfo info;
			if (!/^pid: (\d+), tid: \d+, name: (\S+) +>>>/m.match (report, 0, out info)) {
				throw new Error.INVALID_ARGUMENT ("Malformed native crash report");
			}

			string raw_pid = info.fetch (1);
			uint pid = (uint) uint64.parse (raw_pid);

			string process_name = info.fetch (2);

			string summary = summarize_native_report (report);

			return CrashInfo (pid, process_name, summary, report);
		}

		private static string summarize_java_report (string report) throws Error {
			string? last_cause = null;
			var cause_pattern = /^Caused by: (.+)$/m;
			try {
				MatchInfo info;
				for (cause_pattern.match (report, 0, out info); info.matches (); info.next ())
					last_cause = info.fetch (1);
			} catch (RegexError e) {
			}
			if (last_cause != null)
				return last_cause;

			var lines = report.split ("\n", 4);
			if (lines.length < 3)
				throw new Error.INVALID_ARGUMENT ("Malformed Java crash report");
			return lines[2];
		}

		private static string summarize_native_report (string report) throws Error {
			MatchInfo info;
			if (!/^signal \d+ \((\w+)\), code \S+ \((\w+)\)/m.match (report, 0, out info)) {
				return "Unknown error";
			}
			string signal_name = info.fetch (1);
			string code_name = info.fetch (2);

			if (signal_name == "SIGSEGV") {
				if (code_name == "SEGV_MAPERR")
					return "Bad access due to invalid address";
				if (code_name == "SEGV_ACCERR")
					return "Bad access due to protection failure";
			}

			if (signal_name == "SIGABRT")
				return "Trace/BPT trap";

			if (signal_name == "SIGILL")
				return "Illegal instruction";

			return "%s %s".printf (signal_name, code_name);
		}

		private async void start_monitoring () {
			InputStream? stdout_pipe = null;

			try {
				string cwd = "/";
				string[] argv = new string[] { "su", "-c", "logcat", "-b", "crash", "-B" };
				string[]? envp = null;
				bool capture_output = true;
				var process = yield SuperSU.spawn (cwd, argv, envp, capture_output, io_cancellable);

				logcat = process;
				stdout_pipe = process.output;
			} catch (GLib.Error e) {
			}

			if (stdout_pipe == null) {
				try {
					var process = new Subprocess.newv ({ "logcat", "-b", "crash", "-B" },
						STDIN_INHERIT | STDOUT_PIPE | STDERR_SILENCE);

					logcat = process;
					stdout_pipe = process.get_stdout_pipe ();
				} catch (GLib.Error e) {
				}
			}

			if (stdout_pipe == null)
				return;

			input = new DataInputStream (stdout_pipe);
			input.byte_order = HOST_ENDIAN;

			process_messages.begin ();
		}

		private async void process_messages () {
			try {
				while (true) {
					yield prepare_to_read (2 * sizeof (uint16));
					size_t payload_size = input.read_uint16 (io_cancellable);
					size_t header_size = input.read_uint16 (io_cancellable);
					if (header_size < 24)
						throw new Error.PROTOCOL ("Header too short");
					yield prepare_to_read (header_size + payload_size - 4);

					var entry = new LogEntry ();

					entry.pid = input.read_int32 (io_cancellable);
					entry.tid = input.read_uint32 (io_cancellable);
					entry.sec = input.read_uint32 (io_cancellable);
					entry.nsec = input.read_uint32 (io_cancellable);
					entry.lid = input.read_uint32 (io_cancellable);
					size_t ignored_size = header_size - 24;
					if (ignored_size > 0)
						input.skip (ignored_size, io_cancellable);

					var payload_buf = new uint8[payload_size + 1];
					input.read (payload_buf[0:payload_size], io_cancellable);
					payload_buf[payload_size] = 0;

					uint8 * payload_start = payload_buf;

					entry.priority = payload_start[0];
					unowned string tag = (string) (payload_start + 1);
					unowned string message = (string) (payload_start + 1 + tag.length + 1);
					entry.tag = tag;
					entry.message = message;

					on_log_entry (entry);
				}
			} catch (GLib.Error e) {
			}
		}

		private async void prepare_to_read (size_t required) throws GLib.Error {
			while (true) {
				size_t available = input.get_available ();
				if (available >= required)
					return;
				ssize_t n = yield input.fill_async ((ssize_t) (required - available), Priority.DEFAULT, io_cancellable);
				if (n == 0)
					throw new Error.TRANSPORT ("Disconnected");
			}
		}

		private class LogEntry {
			public int32 pid;
			public uint32 tid;
			public uint32 sec;
			public uint32 nsec;
			public uint32 lid;
			public uint priority;
			public string tag;
			public string message;
		}

		private class CrashDelivery : Object {
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

			private Promise<CrashInfo?> promise = new Promise <CrashInfo?> ();
			private TimeoutSource expiry_source;

			public CrashDelivery (uint pid) {
				Object (pid: pid);
			}

			construct {
				expiry_source = make_expiry_source (500);
			}

			public void disable_timeout () {
				if (expiry_source != null) {
					expiry_source.destroy ();
					expiry_source = null;
				}
			}

			private TimeoutSource make_expiry_source (uint timeout) {
				var source = new TimeoutSource (timeout);
				source.set_callback (on_timeout);
				source.attach (MainContext.get_thread_default ());
				return source;
			}

			public void extend_timeout () {
				if (future.ready)
					return;

				disable_timeout ();
				expiry_source = make_expiry_source (2000);
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

		private class CrashBuilder : Object {
			public signal void completed (string report);

			public uint reporter_pid {
				get;
				construct;
			}

			private StringBuilder report = new StringBuilder ();
			private TimeoutSource completion_source = null;

			public CrashBuilder (uint reporter_pid) {
				Object (reporter_pid: reporter_pid);
			}

			construct {
				start_polling_reporter_pid ();
			}

			public void append (string chunk) {
				report.append (chunk);

				defer_completion ();
			}

			private void start_polling_reporter_pid () {
				var source = new TimeoutSource (50);
				source.set_callback (on_poll_tick);
				source.attach (MainContext.get_thread_default ());
			}

			private bool on_poll_tick () {
				bool reporter_still_alive = Posix.kill ((Posix.pid_t) reporter_pid, 0) == 0;
				if (!reporter_still_alive)
					schedule_completion ();

				return reporter_still_alive;
			}

			private void schedule_completion () {
				completion_source = new TimeoutSource (250);
				completion_source.set_callback (on_complete);
				completion_source.attach (MainContext.get_thread_default ());
			}

			private void defer_completion () {
				if (completion_source == null)
					return;

				completion_source.destroy ();
				completion_source = null;

				schedule_completion ();
			}

			private bool on_complete () {
				completed (report.str);

				completion_source = null;

				return false;
			}
		}
	}

	private FileDescriptor open_process_memory (uint pid) throws Error {
		int handle = Posix.open ("/proc/%u/mem".printf (pid), Posix.O_RDWR);
		if (handle == -1)
			throw new Error.PERMISSION_DENIED ("%s", strerror (errno));
		return new FileDescriptor (handle);
	}

	private async void wait_until_stopped (uint pid, Cancellable? cancellable) throws Error, IOError {
		var main_context = MainContext.get_thread_default ();

		bool timed_out = false;
		var timeout_source = new TimeoutSource.seconds (5);
		timeout_source.set_callback (() => {
			timed_out = true;
			return Source.REMOVE;
		});
		timeout_source.attach (main_context);

		uint[] delays = { 0, 1, 2, 5, 10, 20, 50, 250 };

		try {
			for (uint i = 0; !timed_out && !cancellable.set_error_if_cancelled (); i++) {
				if (is_process_stopped (pid))
					break;

				uint delay_ms = (i < delays.length) ? delays[i] : delays[delays.length - 1];

				var delay_source = new TimeoutSource (delay_ms);
				delay_source.set_callback (wait_until_stopped.callback);
				delay_source.attach (main_context);

				var cancel_source = new CancellableSource (cancellable);
				cancel_source.set_callback (wait_until_stopped.callback);
				cancel_source.attach (main_context);

				yield;

				cancel_source.destroy ();
				delay_source.destroy ();
			}
		} finally {
			timeout_source.destroy ();
		}

		if (timed_out)
			throw new Error.TIMED_OUT ("Unexpectedly timed out while waiting for process with PID %u to stop", pid);
	}

	private bool is_process_stopped (uint pid) throws Error {
		string path = "/proc/%u/stat".printf (pid);

		string contents;
		size_t length;
		try {
			GLib.FileUtils.get_contents (path, out contents, out length);
		} catch (FileError e) {
			throw new Error.PROCESS_NOT_FOUND ("%s", e.message);
		}

		int rparen = contents.last_index_of (")");
		assert (rparen != -1 && rparen + 2 < contents.length);

		char state = contents.get (rparen + 2);

		return state == 'T';
	}
#endif

	private sealed class LinuxSyscallTraceServiceSession : Object, ServiceSession {
		private SyscallTracer? tracer = new SyscallTracer ();

		private const size_t MAX_BATCH_BYTES = 4U * 1024U * 1024U;

		construct {
			tracer.events_available.connect (on_events_available);
		}

		public async void activate (Cancellable? cancellable) throws Error, IOError {
			ensure_active ();
		}

		private void ensure_active () throws Error {
			if (tracer == null)
				throw new Error.INVALID_OPERATION ("Service is closed");

			if (tracer.state == STOPPED)
				tracer.start ();
		}

		public async void cancel (Cancellable? cancellable) throws IOError {
			if (tracer == null)
				return;

			tracer.stop ();
			tracer = null;

			close ();
		}

		public async Variant request (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			ensure_active ();

			var reader = new VariantReader (parameters);

			string type = reader.read_member ("type").get_string_value ();
			reader.end_member ();

			var reply = new VariantBuilder (VariantType.VARDICT);

			if (type == "read-events") {
				var events = new VariantBuilder (new VariantType ("av"));
				var processes = new VariantBuilder (new VariantType ("a(us)"));

				var abi_by_tgid = new Gee.HashMap<uint32, SyscallTracer.Abi> ();
				var expired_tgids = new Gee.HashSet<uint32> ();

				size_t total = 0;

				var drain_status = tracer.drain_events (ev => {
					var event = ev.event;
					uint32 tgid = event->tgid;

					if (expired_tgids.contains (tgid))
						return CONTINUE;

					var abi = abi_by_tgid[tgid];
					if (abi == INVALID) {
						try {
							abi = tracer.get_process_abi (tgid);
						} catch (Error e) {
							expired_tgids.add (tgid);
							return CONTINUE;
						}
						abi_by_tgid[tgid] = abi;
					}

					unowned LinuxSyscallSignature[] sigs = (abi == COMPAT32)
						? get_compat32_syscall_signatures ()
						: get_syscall_signatures ();
					LinuxSyscallSignature * sig = find_sig_by_nr (sigs, ev.se->syscall_nr);

					Variant item = build_event_variant (ev, sig);
					size_t item_size = (size_t) item.get_size ();

					if (total != 0 && total + item_size > MAX_BATCH_BYTES)
						return STOP;

					events.add_value (new Variant.variant (item));
					total += item_size;

					return CONTINUE;
				});

				foreach (var e in abi_by_tgid.entries) {
					unowned string abi = (e.value == COMPAT32) ? "compat32" : "native";
					processes.add ("(us)", e.key, abi);
				}

				vardict_add (reply, "events", events.end ());
				vardict_add (reply, "processes", processes.end ());
				vardict_add (reply, "status",
					(drain_status == SyscallTracer.DrainStatus.DRAINED) ? "drained" : "more");

				return reply.end ();
			}

			if (type == "resolve-stacks") {
				reader.read_member ("ids");
				var ids = read_uint32_array (reader);
				reader.end_member ();

				var stacks = new VariantBuilder (new VariantType ("aat"));

				tracer.resolve_stacks (ids, (sid, frames) => {
					stacks.open (new VariantType ("at"));
					foreach (var frame in frames)
						stacks.add_value (frame);
					stacks.close ();
				});

				vardict_add (reply, "stacks", stacks.end ());

				return reply.end ();
			}

			if (type == "resolve-symbols") {
				var pid = (uint) reader.read_member ("pid").get_int64_value ();
				reader.end_member ();

				var gen = (uint32) reader.read_member ("gen").get_int64_value ();
				reader.end_member ();

				reader.read_member ("addresses");
				var addrs = read_uint64_array (reader);
				reader.end_member ();

				var symbols = new VariantBuilder (new VariantType ("a(uu)"));
				var modules = new VariantBuilder (new VariantType ("av"));

				tracer.resolver.resolve_addresses (pid, gen, addrs,
					(addr, mod_idx, rel) => {
						symbols.add ("(uu)", mod_idx, rel);
					},
					module_list => {
						foreach (var path in module_list)
							modules.add_value (new Variant.variant (new Variant.tuple ({ path })));
					});

				vardict_add (reply, "modules", modules.end ());
				vardict_add (reply, "symbols", symbols.end ());

				return reply.end ();
			}

			if (type == "read-stats") {
				var stats = tracer.read_stats ();

				vardict_add (reply, "emitted-events", stats.emitted_events);
				vardict_add (reply, "emitted-bytes", stats.emitted_bytes);

				vardict_add (reply, "dropped-events", stats.dropped_events);
				vardict_add (reply, "dropped-bytes", stats.dropped_bytes);

				return reply.end ();
			}

			if (type == "add-targets") {
				if (reader.has_member ("pids")) {
					reader.read_member ("pids");
					foreach_uint32 (reader, pid => tracer.add_target_tgid (pid));
					reader.end_member ();
				}

				if (reader.has_member ("uids")) {
					reader.read_member ("uids");
					foreach_uint32 (reader, uid => tracer.add_target_uid (uid));
					reader.end_member ();
				}

				if (reader.has_member ("users")) {
					reader.read_member ("users");
					foreach_string (reader, username => {
						var uid = resolve_uid_from_username (username);
						tracer.add_target_uid (uid);
					});
					reader.end_member ();
				}

				return reply.end ();
			}

			if (type == "remove-targets") {
				if (reader.has_member ("pids")) {
					reader.read_member ("pids");
					foreach_uint32 (reader, pid => tracer.remove_target_tgid (pid));
					reader.end_member ();
				}

				if (reader.has_member ("uids")) {
					reader.read_member ("uids");
					foreach_uint32 (reader, uid => tracer.remove_target_uid (uid));
					reader.end_member ();
				}

				if (reader.has_member ("users")) {
					reader.read_member ("users");
					foreach_string (reader, username => {
						var uid = resolve_uid_from_username (username);
						tracer.remove_target_uid (uid);
					});
					reader.end_member ();
				}

				return reply.end ();
			}

			if (type == "get-signatures") {
				vardict_add (reply, "native", build_signatures_variant (get_syscall_signatures ()));

				unowned LinuxSyscallSignature[]? compat = get_compat32_syscall_signatures ();
				if (compat != null)
					vardict_add (reply, "compat32", build_signatures_variant (compat));

				return reply.end ();
			}

			if (type == "exclude-syscalls") {
				if (reader.has_member ("native")) {
					reader.read_member ("native");
					foreach_uint32 (reader, nr => tracer.exclude_syscall (NATIVE, nr));
					reader.end_member ();
				}

				if (reader.has_member ("compat32")) {
					reader.read_member ("compat32");
					foreach_uint32 (reader, nr => tracer.exclude_syscall (COMPAT32, nr));
					reader.end_member ();
				}

				return reply.end ();
			}

			if (type == "include-syscalls") {
				if (reader.has_member ("native")) {
					reader.read_member ("native");
					foreach_uint32 (reader, nr => tracer.include_syscall (NATIVE, nr));
					reader.end_member ();
				}

				if (reader.has_member ("compat32")) {
					reader.read_member ("compat32");
					foreach_uint32 (reader, nr => tracer.include_syscall (COMPAT32, nr));
					reader.end_member ();
				}

				return reply.end ();
			}

			throw new Error.INVALID_ARGUMENT ("Unsupported request type: %s", type);
		}

		private void on_events_available () {
			var b = new VariantBuilder (VariantType.VARDICT);
			vardict_add (b, "type", "events-available");
			message (b.end ());
		}

		private static Variant build_event_variant (SyscallTracer.SyscallEventView ev, LinuxSyscallSignature * sig) {
			var event = ev.event;
			var type = (SyscallTracer.EventType) event->type;

			unowned uint8[] buf = ev.bytes;
			size_t event_size = buf.length;
			uint8 * payload_end = (uint8 *) buf + event_size;

			uint8 nargs = (sig != null) ? sig->nargs : (uint8) SyscallTracer.SYSCALL_NARGS;
			uint64[]? args = null;
			int64 retval = 0;

			uint8 * a;

			if (type == SYSCALL_ENTER) {
				assert (event_size >= sizeof (SyscallTracer.SyscallEnterEvent));
				var e = (SyscallTracer.SyscallEnterEvent *) buf;
				args = new uint64[nargs];
				for (int i = 0; i != nargs; i++)
					args[i] = e->args[i];
				a = (uint8 *) (e + 1);
			} else {
				assert (event_size >= sizeof (SyscallTracer.SyscallExitEvent));
				var e = (SyscallTracer.SyscallExitEvent *) buf;
				retval = e->retval;
				a = (uint8 *) (e + 1);
			}

			var attachments = new VariantBuilder (new VariantType ("a(uv)"));

			for (uint i = 0; i != event->attachment_count; i++) {
				assert (payload_end - a >= sizeof (SyscallTracer.AttachmentHeader));
				var h = (SyscallTracer.AttachmentHeader *) a;
				a += sizeof (SyscallTracer.AttachmentHeader);

				uint32 idx = h->arg_index;
				assert (idx < nargs);

				var capacity = h->capacity;
				assert (a + capacity <= payload_end);

				var size = h->size;
				assert (a + size <= payload_end);

				Variant v = decode_attachment_value (type, sig, idx, (SyscallTracer.AttachmentType) h->type, a, size);
				attachments.add_value (new Variant.tuple ({ idx, new Variant.variant (v) }));

				a += capacity;
			}

			var se = ev.se;
			if (type == SYSCALL_ENTER) {
				Variant args_v;
				var ab = new VariantBuilder (new VariantType ("at"));
				for (int i = 0; i != nargs; i++)
					ab.add_value (args[i]);
				args_v = ab.end ();

				return new Variant.tuple ({
					"enter",
					event->time_ns,
					(uint64) event->tid,
					event->tgid,
					se->syscall_nr,
					se->stack_id,
					se->map_gen,
					args_v,
					attachments.end ()
				});
			} else {
				return new Variant.tuple ({
					"exit",
					event->time_ns,
					(uint64) event->tid,
					event->tgid,
					se->syscall_nr,
					se->stack_id,
					se->map_gen,
					retval,
					attachments.end ()
				});
			}
		}

		private static Variant decode_attachment_value (SyscallTracer.EventType event_type, LinuxSyscallSignature * sig,
				uint arg_index, SyscallTracer.AttachmentType attachment_type, uint8 * data, size_t len) {
			unowned string? arg_type = null;
			if (sig != null)
				arg_type = sig->args[arg_index].type;

			switch (attachment_type) {
				case SyscallTracer.AttachmentType.STRING:
					return (string) data;
				case SyscallTracer.AttachmentType.BYTES: {
					if (arg_type != null) {
						Variant? v = try_decode_timespec (arg_type, data, len);
						if (v == null)
							v = try_decode_sockaddr (arg_type, data, len);
						if (v != null)
							return v;
					}

					var b = new Bytes (((uint8[]) data)[:len]);
					return Variant.new_from_data (new VariantType ("ay"), b.get_data (), true, (owned) b);
				}
				default:
					assert_not_reached ();
			}
		}

		private static Variant? try_decode_timespec (string arg_type, uint8 * data, size_t len) {
			if (arg_type.has_suffix ("struct __kernel_timespec *")) {
				assert (len == 2 * sizeof (int64));

				var p = (int64 *) data;
				return new Variant.tuple ({ p[0], p[1] });
			}

			if (arg_type.has_suffix ("struct old_timespec32 *")) {
				assert (len == 2 * sizeof (int32));

				var p = (int32 *) data;
				return new Variant.tuple ({ p[0], p[1] });
			}

			return null;
		}

		private static Variant? try_decode_sockaddr (string arg_type, uint8 * data, size_t len) {
			if (arg_type != "struct sockaddr *")
				return null;

			if (len < 2)
				return null;

			uint16 fam = *((uint16 *) data);

			switch ((int) fam) {
			case Posix.AF_INET: {
				if (len < sizeof (Posix.SockAddrIn))
					return null;

				var sa = (Posix.SockAddrIn *) data;
				var port = uint16.from_network (sa->sin_port);

				uint8 ip_bytes[4];
				Memory.copy (ip_bytes, &sa->sin_addr.s_addr, 4);
				string ip = inet_ntop_to_string (Posix.AF_INET, ip_bytes, 4);

				return new Variant.tuple ({ "inet", ip, (uint32) port });
			}
			case Posix.AF_INET6: {
				if (len < sizeof (Posix.SockAddrIn6))
					return null;

				var sa6 = (Posix.SockAddrIn6 *) data;
				var port = uint16.from_network (sa6->sin6_port);

				uint8 ip_bytes[16];
				Memory.copy (ip_bytes, sa6->sin6_addr.s6_addr, 16);
				string ip = inet_ntop_to_string (Posix.AF_INET6, ip_bytes, 16);

				uint32 scope_id = sa6->sin6_scope_id;

				return new Variant.tuple ({
					"inet6",
					ip,
					(uint32) port,
					scope_id
				});
			}
			case Posix.AF_UNIX: {
				var path = (char *) (data + 2);

				size_t path_bytes = size_t.min (len - 2, 108);
				if (path_bytes == 0)
					return new Variant.tuple ({ "unix", "" });

				if (path[0] == 0) {
					size_t n = 1;
					while (n < path_bytes && path[n] != 0)
						n++;

					size_t name_len = (n > 1) ? (n - 1) : 0;

					var str = new uint8[name_len + 1];
					if (name_len != 0)
						Memory.copy (str, path + 1, name_len);

					return new Variant.tuple ({ "unix-abstract", (string) str });
				}

				size_t n = 1;
				while (n < path_bytes && path[n] != 0)
					n++;

				var str = new uint8[n + 1];
				Memory.copy (str, path, n);

				return new Variant.tuple ({ "unix", (string) str });
			}
			default:
				return null;
			}
		}

		private static Variant build_signatures_variant (LinuxSyscallSignature[] signatures) {
			var result = new VariantBuilder (new VariantType ("a(isa(ss))"));

			foreach (unowned LinuxSyscallSignature sig in signatures) {
				var args = new VariantBuilder (new VariantType ("a(ss)"));
				for (uint8 i = 0; i != sig.nargs; i++) {
					unowned LinuxSyscallArg arg = sig.args[i];
					args.add_value (new Variant.tuple ({ arg.type, arg.name }));
				}
				result.add_value (new Variant.tuple ({ sig.nr, sig.name, args.end () }));
			}

			return result.end ();
		}

		private static LinuxSyscallSignature * find_sig_by_nr (LinuxSyscallSignature[] sigs, int32 nr) {
			if (nr < 0)
				return null;

			uint lo = 0;
			uint hi = (uint) sigs.length;

			while (lo < hi) {
				uint mid = lo + ((hi - lo) / 2);

				LinuxSyscallSignature * sig = &sigs[mid];

				if (sig->nr == nr)
					return sig;

				if (sig->nr < nr)
					lo = mid + 1;
				else
					hi = mid;
			}

			return null;
		}

		private static string inet_ntop_to_string (int af, uint8 * src, size_t srclen) {
			uint8 buf[46];
			unowned string? res = Posix.inet_ntop (af, src, buf);
			return (res != null) ? res : "?";
		}

		private static uint32 resolve_uid_from_username (string name) throws Error {
			unowned Posix.Passwd? pw = Posix.getpwnam (name);
			if (pw == null)
				throw new Error.INVALID_ARGUMENT ("Unknown user: %s", name);

			return (uint32) pw.pw_uid;
		}

		private delegate void UInt32Handler (uint32 v) throws Error;
		private delegate void StringHandler (string s) throws Error;

		private static uint32[] read_uint32_array (VariantReader reader) throws Error {
			uint n = reader.count_elements ();
			var arr = new uint32[n];

			for (uint i = 0; i != n; i++) {
				int64 v = reader.read_element (i).get_int64_value ();
				if (v < 0 || v > uint32.MAX)
					throw new Error.INVALID_ARGUMENT ("Value is out of range");

				arr[i] = (uint32) v;
				reader.end_element ();
			}

			return arr;
		}

		private static uint64[] read_uint64_array (VariantReader reader) throws Error {
			uint n = reader.count_elements ();
			var arr = new uint64[n];

			for (uint i = 0; i != n; i++) {
				arr[i] = reader.read_element (i).get_uint64_value ();
				reader.end_element ();
			}

			return arr;
		}

		private static void foreach_uint32 (VariantReader reader, UInt32Handler cb) throws Error {
			uint n = reader.count_elements ();
			for (uint i = 0; i != n; i++) {
				int64 v = reader.read_element (i).get_int64_value ();
				if (v < 0 || v > uint32.MAX)
					throw new Error.INVALID_ARGUMENT ("Value is out of range");

				cb ((uint32) v);

				reader.end_element ();
			}
		}

		private static void foreach_string (VariantReader reader, StringHandler cb) throws Error {
			uint n = reader.count_elements ();
			for (uint i = 0; i != n; i++) {
				unowned string s = reader.read_element (i).get_string_value ();
				cb (s);
				reader.end_element ();
			}
		}

		private static void vardict_add (VariantBuilder b, string key, Variant val) {
			b.add_value (new Variant.dict_entry (new Variant.string (key), new Variant.variant (val)));
		}
	}
}
