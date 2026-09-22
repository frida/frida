namespace Frida {
	public class DarwinHelperBackend : Object, DarwinHelper {
		public signal void idle ();
		public signal void child_dead (uint pid);
		public signal void spawn_instance_ready (uint pid);
		public signal void spawn_instance_error (uint pid, Error error);

		public uint pid {
			get {
				return Posix.getpid ();
			}
		}

		public bool is_idle {
			get {
				return inject_instances.is_empty;
			}
		}

		protected delegate void DispatchWorker ();
		protected delegate void LaunchCompletionHandler (owned StdioPipes? pipes, owned Error? error);

		private MainContext main_context;
		public void * dispatch_context;

		public Gee.HashMap<uint, void *> spawn_instances = new Gee.HashMap<uint, void *> ();
		private Gee.HashMap<uint, OutputStream> stdin_streams = new Gee.HashMap<uint, OutputStream> ();
		private Gee.HashMap<string, PendingLaunch> pending_launches = new Gee.HashMap<string, PendingLaunch> ();

		private Gee.HashMap<uint, Promise<bool>> suspension_waiters = new Gee.HashMap<uint, Promise<bool>> ();

		public Gee.HashMap<uint, void *> inject_instances = new Gee.HashMap<uint, void *> ();
		private Gee.HashMap<void *, uint> inject_cleaner_by_instance = new Gee.HashMap<void *, uint> ();
		private Gee.HashMap<uint, uint> inject_expiry_timers = new Gee.HashMap<uint, uint> ();

		private Gee.Set<void *> instances = new Gee.HashSet<void *> ();
		private SourceFunc? on_last_instance_destroyed = null;

		public uint next_id = 1;

		private PolicySoftener policy_softener;
		private DTraceAgent dtrace_agent;

		private Cancellable io_cancellable = new Cancellable ();

		construct {
			main_context = MainContext.ref_thread_default ();
			_create_dispatch_context ();

			dtrace_agent = DTraceAgent.try_open ();
			if (dtrace_agent != null) {
				dtrace_agent.gating_cancelled.connect (on_dtrace_agent_gating_cancelled);
				dtrace_agent.spawn_added.connect (on_dtrace_agent_spawn_added);
				dtrace_agent.spawn_removed.connect (on_dtrace_agent_spawn_removed);
			}

#if IOS || TVOS
			if (InternalIOSTVOSPolicySoftener.is_available ())
				policy_softener = new InternalIOSTVOSPolicySoftener ();
			else if (ElectraPolicySoftener.is_available ())
				policy_softener = new ElectraPolicySoftener ();
			else if (Unc0verPolicySoftener.is_available ())
				policy_softener = new Unc0verPolicySoftener ();
			else
				policy_softener = new IOSTVOSPolicySoftener ();
#else
			policy_softener = new NullPolicySoftener ();
#endif
		}

		~DarwinHelperBackend () {
			_destroy_dispatch_context ();
		}

		public async void close (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			foreach (var pending in pending_launches.values.to_array ())
				pending.complete ();

			foreach (var entry in inject_cleaner_by_instance.entries) {
				_close_inject_instance (entry.key);
				Source.remove (entry.value);
			}
			inject_cleaner_by_instance.clear ();

			foreach (var instance in spawn_instances.values)
				_close_spawn_instance (instance);
			spawn_instances.clear ();

			bool any_pending = false;
			lock (instances) {
				any_pending = !instances.is_empty;
				if (any_pending)
					on_last_instance_destroyed = close.callback;
			}
			if (any_pending)
				yield;

			if (dtrace_agent != null) {
				dtrace_agent.gating_cancelled.disconnect (on_dtrace_agent_gating_cancelled);
				dtrace_agent.spawn_added.disconnect (on_dtrace_agent_spawn_added);
				dtrace_agent.spawn_removed.disconnect (on_dtrace_agent_spawn_removed);
				yield dtrace_agent.close (cancellable);
				dtrace_agent = null;
			}
		}

		public async void preload (Cancellable? cancellable) throws Error, IOError {
		}

		public async void enable_spawn_gating (SpawnGatingScope scope, Cancellable? cancellable) throws Error, IOError {
			get_dtrace_agent ().enable_spawn_gating (scope);
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			var agent = get_dtrace_agent ();
			yield agent.disable_spawn_gating (cancellable);
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			return get_dtrace_agent ().enumerate_pending_spawn ();
		}

		private DTraceAgent get_dtrace_agent () throws Error {
			if (dtrace_agent == null)
				throw new Error.NOT_SUPPORTED ("Spawn gating requires additional privileges");
			return dtrace_agent;
		}

		public async uint spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
				UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, Cancellable? cancellable) throws Error, IOError {
			if (!FileUtils.test (path, EXISTS))
				throw new Error.EXECUTABLE_NOT_FOUND ("Unable to find executable at '%s'", path);

			StdioPipes? pipes;
			var child_pid = _spawn (path, options, stdin_stream, stdout_stream, stderr_stream, out pipes);

			ChildWatch.add ((Pid) child_pid, on_child_dead);

			if (pipes != null) {
				stdin_streams[child_pid] = pipes.input;
				process_next_output_from.begin (pipes.output, child_pid, 1, pipes);
				process_next_output_from.begin (pipes.error, child_pid, 2, pipes);
			}

			return child_pid;
		}

		public async void launch (string identifier, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			var pending = pending_launches[identifier];
			if (pending != null)
				pending.complete ();

			StdioPipes? pipes = null;
			Error error = null;

			_launch (identifier, options, (p, e) => {
				schedule_on_frida_thread (() => {
					pipes = p;
					error = e;
					launch.callback ();
					return Source.REMOVE;
				});
			});

			yield;

			if (error != null)
				throw error;

			pending = new PendingLaunch (identifier, pipes);
			pending.completed.connect (on_launch_completed);
			pending_launches[identifier] = pending;
		}

		public async void notify_launch_completed (string identifier, uint pid, Cancellable? cancellable) throws Error, IOError {
			var pending = pending_launches[identifier];
			if (pending == null)
				return;

			pending.complete ();

			var pipes = pending.pipes;
			if (pipes != null) {
				process_next_output_from.begin (pipes.output, pid, 1, pipes);
				process_next_output_from.begin (pipes.error, pid, 2, pipes);
			}
		}

		public async void notify_exec_completed (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield flush_dispatch_queue ();

			var dead_instances = new Gee.ArrayList<void *> ();
			foreach (var instance in inject_cleaner_by_instance.keys) {
				if (_get_pid_of_inject_instance (instance) == pid)
					dead_instances.add (instance);
			}

			foreach (var instance in dead_instances) {
				uint source_id;
				inject_cleaner_by_instance.unset (instance, out source_id);
				Source.remove (source_id);

				_close_inject_instance (instance);
			}

			policy_softener.forget (pid);
		}

		private void on_launch_completed (PendingLaunch pending) {
			pending_launches.unset (pending.identifier);
			pending.completed.disconnect (on_launch_completed);
		}

		private void on_child_dead (Pid pid, int status) {
			var child_pid = (uint) pid;

			stdin_streams.unset (child_pid);

			void * instance;
			if (spawn_instances.unset (pid, out instance))
				_close_spawn_instance (instance);

			child_dead (pid);
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			var stream = stdin_streams[pid];
			if (stream == null)
				throw new Error.INVALID_ARGUMENT ("Invalid PID");
			try {
				yield stream.write_all_async (data, Priority.DEFAULT, null, null);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}
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

		public async void wait_until_suspended (uint pid, Cancellable? cancellable) throws Error, IOError {
			var wait_request = suspension_waiters[pid];
			if (wait_request != null) {
				yield wait_request.future.wait_async (cancellable);
				return;
			}

			wait_request = new Promise<bool> ();
			suspension_waiters[pid] = wait_request;

			try {
				var timer = new Timer ();

				do {
					var task = task_for_pid (pid);
					try {
						if (_is_suspended (task)) {
							wait_request.resolve (true);
							return;
						}
					} catch (Error e) {
						if (!(e is Error.PROCESS_NOT_FOUND))
							throw e;
					} finally {
						deallocate_port (task);
					}

					var delay_source = new TimeoutSource (20);
					delay_source.set_callback (wait_until_suspended.callback);
					delay_source.attach (MainContext.get_thread_default ());

					yield;

					if (!suspension_waiters.has (pid, wait_request))
						throw new Error.INVALID_OPERATION ("Cancelled");
				} while (timer.elapsed () < 2);

				throw new Error.TIMED_OUT ("Unexpectedly timed out while waiting for process to suspend");
			} catch (Error e) {
				wait_request.reject (e);
				throw e;
			} finally {
				if (suspension_waiters.has (pid, wait_request))
					suspension_waiters.unset (pid);
			}
		}

		public async void cancel_pending_waits (uint pid, Cancellable? cancellable) throws Error, IOError {
			suspension_waiters.unset (pid);
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			try {
				void * instance;
				if (spawn_instances.unset (pid, out instance)) {
					_resume_spawn_instance (instance);
					_close_spawn_instance (instance);
				} else {
					resume_with_validation (pid);
				}
			} finally {
				if (dtrace_agent != null)
					dtrace_agent.on_resume (pid);
			}

			process_resumed (pid);
		}

		public static void resume_with_validation (uint pid) {
			uint task = 0;
			try {
				task = task_for_pid (pid);
				resume_process (task);
			} catch (Error e) {
			} finally {
				if (task != 0)
					deallocate_port (task);
			}
		}

		public static void resume_without_validation (uint pid) {
			uint task = 0;
			try {
				task = task_for_pid (pid);
				resume_process_fast (task);
			} catch (Error e) {
			} finally {
				if (task != 0)
					deallocate_port (task);
			}
		}

		public async void kill_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			_kill_process (pid);
			process_killed (pid);
		}

		public async void kill_application (string identifier, Cancellable? cancellable) throws Error, IOError {
			var killed_pid = _kill_application (identifier);
			if (killed_pid > 0)
				process_killed (killed_pid);
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			return yield _inject (pid, path, null, entrypoint, data, cancellable);
		}

		public async uint inject_library_blob (uint pid, string name, MappedLibraryBlob blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			return yield _inject (pid, name, blob, entrypoint, data, cancellable);
		}

		private async uint _inject (uint pid, string path_or_name, MappedLibraryBlob? blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			yield prepare_target (pid, cancellable);

			var task = task_for_pid (pid);
			try {
				return _inject_into_task (pid, task, path_or_name, blob, entrypoint, data);
			} finally {
				deallocate_port (task);
			}
		}

		public async void prepare_target (uint pid, Cancellable? cancellable) throws Error, IOError {
			policy_softener.soften (pid);

			var task = task_for_pid (pid);
			try {
				var spawn_instance = spawn_instances[pid];
				bool instance_created_here = false;
				bool not_yet_booted = _is_suspended (task) && is_booting (task);
				if (spawn_instance == null && not_yet_booted) {
					spawn_instance = _create_spawn_instance (pid);
					instance_created_here = true;
				}

				if (not_yet_booted) {
					try {
						_prepare_spawn_instance_for_injection (spawn_instance, task);

						resume_process_fast (task);

						Error? pending_error = null;
						var ready_handler = spawn_instance_ready.connect ((ready_pid) => {
							if (ready_pid != pid)
								return;
							prepare_target.callback ();
						});
						var error_handler = spawn_instance_error.connect ((error_pid, error_value) => {
							if (error_pid != pid)
								return;
							pending_error = error_value;
							prepare_target.callback ();
						});
						var timeout_source = new TimeoutSource.seconds (10);
						timeout_source.set_callback (() => {
							pending_error = new Error.TIMED_OUT ("Unexpectedly timed out while initializing suspended process");
							prepare_target.callback ();
							return false;
						});
						timeout_source.attach (MainContext.get_thread_default ());

						yield;

						timeout_source.destroy ();
						disconnect (error_handler);
						disconnect (ready_handler);

						if (pending_error != null)
							throw pending_error;
					} catch (GLib.Error e) {
						if (instance_created_here) {
							spawn_instances.unset (pid);
							_close_spawn_instance (spawn_instance);
						}

						throw_api_error (e);
					}
				}
			} finally {
				deallocate_port (task);
			}
		}

		private bool is_booting (uint task) throws Error {
			Gum.Darwin.AllImageInfos infos;
			try {
				Gum.Darwin.query_all_image_infos (task, out infos);
			} catch (Gum.Error e) {
				throw new Error.PROCESS_NOT_FOUND ("%s", e.message);
			}

			return !infos.libsystem_initialized;
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			var instance = inject_instances[id];
			if (instance == null)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			_demonitor (instance);

			schedule_inject_expiry_for_id (id);
		}

		public async uint demonitor_and_clone_injectee_state (uint id, Cancellable? cancellable) throws Error, IOError {
			var instance = inject_instances[id];
			if (instance == null)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			var clone_id = _demonitor_and_clone_injectee_state (instance);

			schedule_inject_expiry_for_id (id);
			schedule_inject_expiry_for_id (clone_id);

			return clone_id;
		}

		public async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			var instance = inject_instances[id];
			if (instance == null)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			var task = task_for_pid (pid);
			try {
				cancel_inject_expiry_for_id (id);

				_recreate_injectee_thread (instance, pid, task);
			} finally {
				deallocate_port (task);
			}
		}

		public async Future<IOStream> open_pipe_stream (uint remote_pid, Cancellable? cancellable, out string remote_address)
				throws Error, IOError {
			remote_address = null;

			yield prepare_target (remote_pid, cancellable);

			var remote_task = task_for_pid (remote_pid);
			try {
				PipeEndpoints endpoints = make_pipe_endpoints (0, remote_pid, remote_task);

				remote_address = endpoints.remote_address;

				return Pipe.open (endpoints.local_address, cancellable);
			} finally {
				deallocate_port (remote_task);
			}
		}

		public async MappedLibraryBlob? try_mmap (Bytes blob, Cancellable? cancellable) throws Error, IOError {
			if (!is_mmap_available ())
				return null;

			return mmap (0, blob);
		}

		public void _on_spawn_instance_ready (uint pid) {
			schedule_on_frida_thread (() => {
				spawn_instance_ready (pid);
				return Source.REMOVE;
			});
		}

		public void _on_spawn_instance_error (uint pid, Error error) {
			schedule_on_frida_thread (() => {
				spawn_instance_error (pid, error);
				return Source.REMOVE;
			});
		}

		public void _on_mach_thread_dead (uint id, void * posix_thread) {
			schedule_on_frida_thread (() => {
				var instance = inject_instances[id];
				assert (instance != null);

				if (posix_thread != null)
					_join_inject_instance_posix_thread (instance, posix_thread);
				else
					_destroy_inject_instance (id);

				return Source.REMOVE;
			});
		}

		public void _on_posix_thread_dead (uint id) {
			schedule_on_frida_thread (() => {
				_destroy_inject_instance (id);
				return Source.REMOVE;
			});
		}

		protected void _destroy_inject_instance (uint id) {
			void * instance;
			bool instance_id_found = inject_instances.unset (id, out instance);
			assert (instance_id_found);

			schedule_inject_instance_cleanup (instance);

			uninjected (id);

			if (inject_instances.is_empty)
				idle ();
		}

		private void schedule_inject_instance_cleanup (void * instance) {
			var cleanup_source = new TimeoutSource (50);
			cleanup_source.set_callback (() => {
				_close_inject_instance (instance);

				var removed = inject_cleaner_by_instance.unset (instance);
				assert (removed);

				return false;
			});
			inject_cleaner_by_instance[instance] = cleanup_source.attach (MainContext.get_thread_default ());
		}

		private void schedule_inject_expiry_for_id (uint id) {
			uint previous_timer;
			if (inject_expiry_timers.unset (id, out previous_timer))
				Source.remove (previous_timer);

			var expiry_source = new TimeoutSource.seconds (20);
			expiry_source.set_callback (() => {
				var removed = inject_expiry_timers.unset (id);
				assert (removed);

				_destroy_inject_instance (id);

				return false;
			});
			inject_expiry_timers[id] = expiry_source.attach (MainContext.get_thread_default ());
		}

		private void cancel_inject_expiry_for_id (uint id) {
			uint timer;
			var found = inject_expiry_timers.unset (id, out timer);
			assert (found);

			Source.remove (timer);
		}

		public void _on_inject_instance_loaded (uint id, uint pid, DarwinModuleDetails? mapped_module) {
			try {
				policy_softener.retain (pid);
			} catch (Error e) {
				assert_not_reached ();
			}

			if (mapped_module != null)
				injected (id, pid, true, mapped_module);
			else
				injected (id, pid, false, DarwinModuleDetails (0, "", ""));
		}

		public void _on_inject_instance_unloaded (uint id, uint pid) {
			policy_softener.release (pid);
		}

		public void _on_inject_instance_detached (uint id, uint pid) {
			policy_softener.forget (pid);
		}

		public void _on_instance_created (void * instance) {
			lock (instances)
				instances.add (instance);
		}

		public void _on_instance_destroyed (void * instance) {
			lock (instances) {
				instances.remove (instance);
				if (on_last_instance_destroyed != null && instances.is_empty)
					schedule_on_frida_thread ((owned) on_last_instance_destroyed);
			}
		}

		private void on_dtrace_agent_gating_cancelled () {
			gating_cancelled ();
		}

		private void on_dtrace_agent_spawn_added (HostSpawnInfo info) {
			spawn_added (info);
		}

		private void on_dtrace_agent_spawn_removed (HostSpawnInfo info) {
			spawn_removed (info);
		}

		private async void flush_dispatch_queue () {
			_schedule_on_dispatch_queue (() => {
				schedule_on_frida_thread (flush_dispatch_queue.callback);
			});
			yield;
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}

		public extern static PipeEndpoints make_pipe_endpoints (uint local_task, uint remote_pid, uint remote_task) throws Error;
		public extern static void make_pipe_endpoint_from_socket (uint pid, uint task, GLib.Socket sock, out string address)
			throws Error;

		public extern static uint task_for_pid (uint pid) throws Error;
		public extern static void deallocate_port (uint port);

		public extern static bool is_mmap_available ();
		public extern static MappedLibraryBlob mmap (uint task, Bytes blob) throws Error;

		protected extern void _create_dispatch_context ();
		protected extern void _destroy_dispatch_context ();
		protected extern void _schedule_on_dispatch_queue (DispatchWorker worker);

		protected extern uint _spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
			UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, out StdioPipes? pipes) throws Error;
		protected extern static void _launch (string identifier, HostSpawnOptions options, LaunchCompletionHandler on_complete);
		protected extern static bool _is_suspended (uint task) throws Error;
		public extern static void resume_process (uint task) throws Error;
		public extern static void resume_process_fast (uint task) throws Error;
		protected extern static void _kill_process (uint pid);
		protected extern static uint _kill_application (string identifier);
		public extern static string path_for_pid (uint pid) throws Error;
		public extern static bool is_application_process (uint pid);
		protected extern void * _create_spawn_instance (uint pid);
		protected extern void _prepare_spawn_instance_for_injection (void * instance, uint task) throws Error;
		protected extern void _resume_spawn_instance (void * instance);
		protected extern void _close_spawn_instance (void * instance);

		protected extern uint _inject_into_task (uint pid, uint task, string path_or_name, MappedLibraryBlob? blob, string entrypoint, string data) throws Error;
		protected extern void _demonitor (void * instance);
		protected extern uint _demonitor_and_clone_injectee_state (void * instance);
		protected extern void _recreate_injectee_thread (void * instance, uint pid, uint task) throws Error;
		protected extern void _join_inject_instance_posix_thread (void * instance, void * posix_thread);
		protected extern uint _get_pid_of_inject_instance (void * instance);
		protected extern void _close_inject_instance (void * instance);
	}

	public sealed class DTraceAgent : Object {
		public signal void gating_cancelled ();
		public signal void spawn_added (HostSpawnInfo info);
		public signal void spawn_removed (HostSpawnInfo info);

		private Subprocess? dtrace;
		private DataInputStream input;
		private SpawnGatingScope scope;
		private Gee.HashMap<uint, HostSpawnInfo?> pending_spawn = new Gee.HashMap<uint, HostSpawnInfo?> ();
		private SpawnGatingWatchdog watchdog = new SpawnGatingWatchdog ();

		private Cancellable io_cancellable = new Cancellable ();

		// Present in every path inside an app bundle, so APPLICATIONS gates the whole app
		// (helpers, XPC services, ...), not just its entry point.
		private const string APP_BUNDLE_MARKER = ".app/";

		public static DTraceAgent? try_open () {
			if (Posix.getuid () != 0)
				return null;

			return new DTraceAgent ();
		}

		construct {
			watchdog.expired.connect (on_watchdog_expired);
		}

		private void on_watchdog_expired () {
			disable_spawn_gating.begin (io_cancellable);
			gating_cancelled ();
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (dtrace != null) {
				try {
					yield disable_spawn_gating (cancellable);
				} catch (Error e) {
					assert_not_reached ();
				}
			}
		}

		public void enable_spawn_gating (SpawnGatingScope scope) throws Error {
			if (dtrace != null)
				throw new Error.INVALID_OPERATION ("Already enabled");

			this.scope = scope;

			string? predicate = Environment.get_variable ("FRIDA_DTRACE_PREDICATE");
			string predicate_clause;
			if (predicate != null)
				predicate_clause = "/" + predicate + "/";
			else
				predicate_clause = "";

			try {
				dtrace = new Subprocess.newv ({
					"dtrace", "-x", "switchrate=100hz", "-w", "-n", """
						syscall::getpid:entry """ + predicate_clause + """ {
							printf("pid=%u caller=%p", pid, ucaller);
							stop();
						}
					"""
					}, STDIN_INHERIT | STDOUT_PIPE | STDERR_SILENCE);

				input = (DataInputStream) Object.new (typeof (DataInputStream),
					"base-stream", dtrace.get_stdout_pipe (),
					"close-base-stream", false,
					"newline-type", DataStreamNewlineType.LF);
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			process_incoming_messages.begin ();
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			if (dtrace == null)
				throw new Error.INVALID_OPERATION ("Already disabled");

			dtrace.send_signal (Posix.Signal.TERM);

			try {
				yield dtrace.wait_async (cancellable);
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			dtrace = null;

			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (disable_spawn_gating.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			io_cancellable = new Cancellable ();

			yield process_incoming_messages ();

			foreach (var e in pending_spawn.entries) {
				DarwinHelperBackend.resume_without_validation (e.key);
				spawn_removed (e.value);
			}
			pending_spawn.clear ();
			watchdog.clear ();
		}

		public HostSpawnInfo[] enumerate_pending_spawn () {
			var result = new HostSpawnInfo[pending_spawn.size];
			var index = 0;
			foreach (var spawn in pending_spawn.values)
				result[index++] = spawn;
			return result;
		}

		public void on_resume (uint pid) {
			HostSpawnInfo? info;
			if (pending_spawn.unset (pid, out info)) {
				watchdog.cancel (pid);
				spawn_removed (info);
			}
		}

		private async void process_incoming_messages () {
			try {
				while (true) {
					var line = yield input.read_line_async (Priority.DEFAULT, io_cancellable);
					if (line == null)
						break;

					MatchInfo m;
					if (!/\s*(\d)\s+(\d+)\s+getpid:entry\s+pid=(\d+)\s+caller=(.+)/.match (line, 0, out m))
						continue;

					uint pid = (uint) uint64.parse (m.fetch (3));
					Gum.Address caller = uint64.parse (m.fetch (4), 16);

					uint task = 0;
					try {
						task = DarwinHelperBackend.task_for_pid (pid);

						bool caller_is_dyld = false;

						Gum.Darwin.MappingDetails mapping;
						if (Gum.Darwin.query_mapped_address (task, caller, out mapping)) {
							if (mapping.path == "/usr/lib/dyld")
								caller_is_dyld = true;
						}

						if (!caller_is_dyld) {
							DarwinHelperBackend.resume_process_fast (task);
							continue;
						}
					} catch (Error e) {
						continue;
					} finally {
						if (task != 0)
							DarwinHelperBackend.deallocate_port (task);
					}

					string path;
					try {
						path = DarwinHelperBackend.path_for_pid (pid);
					} catch (Error e) {
						DarwinHelperBackend.resume_without_validation (pid);
						continue;
					}

					if (scope != PROCESSES && !path.contains (APP_BUNDLE_MARKER)) {
						DarwinHelperBackend.resume_without_validation (pid);
						continue;
					}

					var info = HostSpawnInfo (pid, path);
					pending_spawn[pid] = info;
					watchdog.arm (pid);
					spawn_added (info);
				}
			} catch (IOError e) {
			}
		}
	}

	private sealed class PendingLaunch : Object {
		public signal void completed ();

		public string identifier {
			get;
			construct;
		}

		public StdioPipes? pipes {
			get;
			construct;
		}

		private Source expiry_timer;

		public PendingLaunch (string identifier, StdioPipes? pipes) {
			Object (identifier: identifier, pipes: pipes);
		}

		construct {
			var source = new TimeoutSource.seconds (20);
			source.set_callback (on_timeout);
			source.attach (MainContext.get_thread_default ());
			expiry_timer = source;
		}

		public void complete () {
			if (expiry_timer != null) {
				expiry_timer.destroy ();
				expiry_timer = null;
			}

			completed ();
		}

		private bool on_timeout () {
			expiry_timer = null;

			complete ();

			return false;
		}
	}
}
