namespace Frida {
	public sealed class WindowsHostSessionBackend : LocalHostSessionBackend {
		protected override LocalHostSessionProvider make_provider () {
			return new WindowsHostSessionProvider ();
		}
	}

	public sealed class WindowsHostSessionProvider : LocalHostSessionProvider {
		protected override LocalHostSession make_host_session (HostSessionOptions? options) throws Error {
			var tempdir = new TemporaryDirectory ();
			return new WindowsHostSession (new WindowsHelperProcess (tempdir), tempdir);
		}

		protected override Variant? load_icon () {
			return _try_extract_icon ();
		}

		public extern static Variant? _try_extract_icon ();
	}

	public sealed class WindowsHostSession : LocalHostSession {
		public WindowsHelper helper {
			get;
			construct;
		}

		public TemporaryDirectory tempdir {
			get;
			construct;
		}

		private AgentContainer system_session_container;

		private AgentDescriptor? agent;

		private ApplicationEnumerator application_enumerator = new ApplicationEnumerator ();
		private ProcessEnumerator process_enumerator = new ProcessEnumerator ();

		private Gee.HashMap<uint, ChildProcess> process_by_pid = new Gee.HashMap<uint, ChildProcess> ();

		public WindowsHostSession (owned WindowsHelper helper, owned TemporaryDirectory tempdir) {
			Object (helper: helper, tempdir: tempdir);
		}

		construct {
			injector = new Winjector (helper, false, tempdir);
			injector.uninjected.connect (on_uninjected);

#if HAVE_EMBEDDED_ASSETS
			agent = new AgentDescriptor (PathTemplate ("<arch>\\frida-agent.dll"),
				new Bytes.static (Frida.Data.Agent.get_frida_agent_arm64_dll_blob ().data),
				new Bytes.static (Frida.Data.Agent.get_frida_agent_x86_64_dll_blob ().data),
				new Bytes.static (Frida.Data.Agent.get_frida_agent_x86_dll_blob ().data),
				new AgentResource[] {
					new AgentResource ("arm64\\dbghelp.dll",
						new Bytes.static (Frida.Data.Agent.get_dbghelp_arm64_dll_blob ().data), tempdir),
					new AgentResource ("arm64\\symsrv.dll",
						new Bytes.static (Frida.Data.Agent.get_symsrv_arm64_dll_blob ().data), tempdir),
					new AgentResource ("x86_64\\dbghelp.dll",
						new Bytes.static (Frida.Data.Agent.get_dbghelp_x86_64_dll_blob ().data), tempdir),
					new AgentResource ("x86_64\\symsrv.dll",
						new Bytes.static (Frida.Data.Agent.get_symsrv_x86_64_dll_blob ().data), tempdir),
					new AgentResource ("x86\\dbghelp.dll",
						new Bytes.static (Frida.Data.Agent.get_dbghelp_x86_dll_blob ().data), tempdir),
					new AgentResource ("x86\\symsrv.dll",
						new Bytes.static (Frida.Data.Agent.get_symsrv_x86_dll_blob ().data), tempdir),
				},
				tempdir
			);
#endif
		}

#if !HAVE_EMBEDDED_ASSETS
		private static PathTemplate installed_agent_path_template () {
			return PathTemplate (Frida.agent_path);
		}

		private static string[] installed_agent_dependencies () {
			string lib_root = Path.get_dirname (Path.get_dirname (Frida.agent_path));
			string[] deps = {};
			foreach (unowned string arch in new string[] { "arm64", "x86_64", "x86" }) {
				foreach (unowned string name in new string[] { "dbghelp.dll", "symsrv.dll" }) {
					string p = Path.build_filename (lib_root, arch, name);
					if (FileUtils.test (p, EXISTS))
						deps += p;
				}
			}
			return deps;
		}
#endif

		public override async void close (Cancellable? cancellable) throws IOError {
			yield base.close (cancellable);

			var winjector = injector as Winjector;

			yield wait_for_uninject (injector, cancellable, () => {
				return winjector.any_still_injected ();
			});

			injector.uninjected.disconnect (on_uninjected);
			yield injector.close (cancellable);

			if (system_session_container != null) {
				yield system_session_container.destroy (cancellable);
				system_session_container = null;
			}

			foreach (var process in process_by_pid.values)
				process.close ();
			process_by_pid.clear ();

			yield helper.close (cancellable);

			agent = null;

			tempdir.destroy ();
		}

		protected override async AgentSessionProvider create_system_session_provider (Cancellable? cancellable,
				out DBusConnection connection) throws Error, IOError {
#if HAVE_EMBEDDED_ASSETS
			var path_template = agent.get_path_template ();
#else
			var path_template = installed_agent_path_template ();
#endif

			unowned string arch;
			switch (Gum.NATIVE_CPU) {
				case ARM64:
					arch = "arm64";
					break;
				case AMD64:
					arch = "x86_64";
					break;
				case IA32:
					arch = "x86";
					break;
				default:
					assert_not_reached ();
			}

			var agent_path = path_template.expand (arch);

			system_session_container = yield AgentContainer.create (agent_path, cancellable);

			connection = system_session_container.connection;

			return system_session_container;
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
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public override async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public override async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public override async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable)
				throws Error, IOError {
			var path = program;

			if (!FileUtils.test (path, EXISTS))
				throw new Error.EXECUTABLE_NOT_FOUND ("Unable to find executable at '%s'", path);

			var process = _spawn (path, options);

			var pid = process.pid;
			process_by_pid[pid] = process;

			var pipes = process.pipes;
			if (pipes != null) {
				process_next_output_from.begin (pipes.output, pid, 1, pipes);
				process_next_output_from.begin (pipes.error, pid, 2, pipes);
			}

			return pid;
		}

		public void _on_child_dead (ChildProcess process, int status) {
			process_by_pid.unset (process.pid);
		}

		private async void process_next_output_from (InputStream stream, uint pid, int fd, Object resource) {
			try {
				var buf = new uint8[4096];
				var n = yield stream.read_async (buf, Priority.DEFAULT, io_cancellable);

				var data = buf[0:n];
				output (pid, fd, data);

				if (n > 0)
					process_next_output_from.begin (stream, pid, fd, resource);
			} catch (GLib.Error e) {
				if (!(e is IOError.CANCELLED))
					output (pid, fd, new uint8[0]);
			}
		}

		protected override bool process_is_alive (uint pid) {
			return _process_is_alive (pid);
		}

		public override async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			var process = process_by_pid[pid];
			if (process == null)
				throw new Error.INVALID_ARGUMENT ("Invalid PID");
			try {
				yield process.pipes.input.write_all_async (data, Priority.DEFAULT, cancellable, null);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}
		}

		protected override async void perform_resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			var process = process_by_pid[pid];
			if (process == null)
				throw new Error.INVALID_ARGUMENT ("Invalid PID");
			process.resume ();
		}

		public override async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			System.kill (pid);
		}

		protected override async Future<IOStream> perform_attach_to (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable, out Object? transport) throws Error, IOError {
			var t = new PipeTransport ();

			var stream_request = Pipe.open (t.local_address, cancellable);

			var winjector = injector as Winjector;
#if HAVE_EMBEDDED_ASSETS
			var id = yield winjector.inject_library_resource (pid, agent, "frida_agent_main",
				make_agent_parameters (pid, t.remote_address, options), cancellable);
#else
			var id = yield winjector.inject_library_file_with_template (pid,
				installed_agent_path_template (), "frida_agent_main",
				make_agent_parameters (pid, t.remote_address, options),
				installed_agent_dependencies (), cancellable);
#endif
			injectee_by_pid[pid] = id;

			transport = t;

			return stream_request;
		}

		public extern ChildProcess _spawn (string path, HostSpawnOptions options) throws Error;
		public extern static bool _process_is_alive (uint pid);
	}

	public sealed class ChildProcess : Object {
		public unowned Object parent {
			get;
			construct;
		}

		public uint pid {
			get;
			construct;
		}

		public void * handle {
			get;
			construct;
		}

		public void * main_thread {
			get;
			construct;
		}

		public StdioPipes? pipes {
			get;
			construct;
		}

		public Source? watch {
			get;
			set;
		}

		protected bool closed = false;
		protected bool resumed = false;

		public ChildProcess (Object parent, uint pid, void * handle, void * main_thread, StdioPipes? pipes) {
			Object (parent: parent, pid: pid, handle: handle, main_thread: main_thread, pipes: pipes);
		}

		~ChildProcess () {
			close ();
		}

		public void close () {
			if (closed)
				return;
			_do_close ();
			closed = true;
		}

		public extern void _do_close ();

		public void resume () throws Error {
			if (resumed)
				throw new Error.INVALID_OPERATION ("Already resumed");
			_do_resume ();
			resumed = true;
		}

		public extern void _do_resume ();
	}
}
