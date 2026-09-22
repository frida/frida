namespace Frida {
	public sealed class LinuxHelperBackend : Object, LinuxHelper {
		public signal void idle ();

		public bool is_idle {
			get {
				return agents.is_empty;
			}
		}

		private Gee.Map<uint, SpawnedProcess> spawned_processes = new Gee.HashMap<uint, SpawnedProcess> ();
		private Gee.Map<uint, ExecTransitionSession> exec_transitions = new Gee.HashMap<uint, ExecTransitionSession> ();
		private Gee.Map<uint, AwaitExecTransitionTask> exec_waiters = new Gee.HashMap<uint, AwaitExecTransitionTask> ();
		private Gee.Map<uint, PausedSyscallSession> paused_syscalls = new Gee.HashMap<uint, PausedSyscallSession> ();
		private Gee.Map<uint, InjectSession> suspended_by_inject = new Gee.HashMap<uint, InjectSession> ();
		private Gee.Map<uint, RemoteAgent> agents = new Gee.HashMap<uint, RemoteAgent> ();
		private Gee.Map<uint, Source> agent_expiries = new Gee.HashMap<uint, Source> ();
		private Gee.Map<uint, Gee.Queue<TaskEntry>> task_queues = new Gee.HashMap<uint, Gee.Queue<TaskEntry>> ();

		public async void close (Cancellable? cancellable) throws IOError {
			if (!is_idle) {
				var idle_handler = idle.connect (() => {
					close.callback ();
				});
				yield;
				disconnect (idle_handler);
			}

			foreach (SpawnedProcess p in spawned_processes.values)
				p.close ();
		}

		public async uint spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
				UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, Cancellable? cancellable) throws Error, IOError {
			if (!FileUtils.test (path, EXISTS))
				throw new Error.EXECUTABLE_NOT_FOUND ("Unable to find executable at '%s'", path);

			string[] argv = options.compute_argv (path);
			string[] envp = options.compute_envp ();

			FileDescriptor? in_fd, out_fd, err_fd;
			StdioPipes? pipes = make_stdio_pipes (options.stdio, true, out in_fd, null, out out_fd, null, out err_fd, null);

			string? old_cwd = null;
			if (options.cwd.length > 0) {
				old_cwd = Environment.get_current_dir ();
				if (Environment.set_current_dir (options.cwd) != 0)
					throw new Error.INVALID_ARGUMENT ("Unable to change directory to '%s'", options.cwd);
			}

			Posix.pid_t pid;
			try {
				pid = Posix.fork ();
				if (pid == -1)
					throw new Error.NOT_SUPPORTED ("Unable to fork(): %s", strerror (errno));

				if (pid == 0) {
					Posix.setsid ();

					if (stdin_stream != null) {
						Posix.dup2 (stdin_stream.get_fd (), 0);
						Posix.dup2 (stdout_stream.get_fd (), 1);
						Posix.dup2 (stderr_stream.get_fd (), 2);
					} else if (pipes != null) {
						Posix.dup2 (in_fd.handle, 0);
						Posix.dup2 (out_fd.handle, 1);
						Posix.dup2 (err_fd.handle, 2);
					}

					if (_ptrace (TRACEME) == -1) {
						stderr.printf ("Unexpected error while spawning process (ptrace failed: %s)\n", Posix.strerror (errno));
						Posix._exit (1);
					}
					Posix.raise (Posix.Signal.STOP);

					if (execve (path, argv, envp) == -1) {
						stderr.printf ("Unexpected error while spawning process (execve failed: %s)\n", Posix.strerror (errno));
						Posix._exit (2);
					}
				}
			} finally {
				if (old_cwd != null)
					Environment.set_current_dir (old_cwd);
			}

			bool ready = false;
			try {
				yield ChildProcess.wait_for_early_signal (pid, STOP, cancellable);
				ptrace (CONT, pid);
				yield ChildProcess.wait_for_early_signal (pid, TRAP, cancellable);
				ready = true;
			} finally {
				if (!ready)
					Posix.kill (pid, Posix.Signal.KILL);
			}

			var p = new SpawnedProcess (pid, pipes);
			p.terminated.connect (on_spawned_process_terminated);
			p.output.connect (on_spawned_process_output);
			spawned_processes[pid] = p;

			return pid;
		}

		private void on_spawned_process_terminated (SpawnedProcess process) {
			spawned_processes.unset (process.pid);
		}

		private void on_spawned_process_output (SpawnedProcess process, int fd, uint8[] data) {
			output (process.pid, fd, data);
		}

		public async void prepare_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield perform<ExecTransitionSession> (new PrepareExecTransitionTask (this), pid, cancellable);
		}

		private class PrepareExecTransitionTask : Object, Task<ExecTransitionSession> {
			private weak LinuxHelperBackend backend;

			public PrepareExecTransitionTask (LinuxHelperBackend backend) {
				this.backend = backend;
			}

			public async ExecTransitionSession run (uint pid, Cancellable? cancellable) throws Error, IOError {
				SpawnedProcess? p = backend.spawned_processes[pid];
				if (p != null)
					p.demonitor ();

				var session = yield ExecTransitionSession.open (pid, cancellable);
				backend.exec_transitions[pid] = session;

				backend.update_process_status (pid, EXEC_PENDING);

				return session;
			}
		}

		public async void await_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			if (exec_waiters.has_key (pid))
				throw new Error.INVALID_ARGUMENT ("Wait operation already in progress");

			var task = new AwaitExecTransitionTask (this);
			exec_waiters[pid] = task;
			try {
				yield perform<ExecTransitionSession> (task, pid, cancellable);
			} finally {
				exec_waiters.unset (pid);

				SpawnedProcess? p = spawned_processes[pid];
				if (p != null)
					p.monitor ();

				update_process_status (pid, NORMAL);
			}
		}

		private class AwaitExecTransitionTask : Object, Task<ExecTransitionSession> {
			private weak LinuxHelperBackend backend;

			private Cancellable wait_cancellable = new Cancellable ();

			public AwaitExecTransitionTask (LinuxHelperBackend backend) {
				this.backend = backend;
			}

			public async ExecTransitionSession run (uint pid, Cancellable? cancellable) throws Error, IOError {
				ExecTransitionSession? session = backend.exec_transitions[pid];
				if (session == null)
					throw new Error.INVALID_ARGUMENT ("Invalid PID");

				var cancel_source = new CancellableSource (cancellable);
				cancel_source.set_callback (() => {
					wait_cancellable.cancel ();
					return Source.REMOVE;
				});
				cancel_source.attach (MainContext.get_thread_default ());

				GLib.Error? pending_error = null;
				try {
					yield session.wait_for_exec (wait_cancellable);
				} catch (GLib.Error e) {
					pending_error = e;
				} finally {
					cancel_source.destroy ();
				}

				if (pending_error != null) {
					backend.exec_transitions.unset (pid);
					try {
						session.close ();
					} catch (Error e) {
						yield session.suspend (null);
						try {
							session.close ();
						} catch (Error e) {
						}
					}
					throw_api_error (pending_error);
				}

				return session;
			}

			public void cancel () {
				wait_cancellable.cancel ();
			}
		}

		public async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			AwaitExecTransitionTask? task = exec_waiters[pid];
			if (task == null)
				throw new Error.INVALID_ARGUMENT ("Invalid PID");

			task.cancel ();

			yield perform<FlushExecTransitionTask> (new FlushExecTransitionTask (), pid, cancellable);
		}

		private class FlushExecTransitionTask : Object, Task<FlushExecTransitionTask> {
			public FlushExecTransitionTask () {
			}

			public async FlushExecTransitionTask run (uint pid, Cancellable? cancellable) throws Error, IOError {
				return this;
			}
		}

		public async void await_syscall (uint pid, LinuxSyscallMask mask, Cancellable? cancellable) throws Error, IOError {
			yield perform<PausedSyscallSession> (new AwaitSyscallTask (this, mask), pid, cancellable);
		}

		private class AwaitSyscallTask : Object, Task<PausedSyscallSession> {
			private weak LinuxHelperBackend backend;
			private LinuxSyscallMask mask;

			public AwaitSyscallTask (LinuxHelperBackend backend, LinuxSyscallMask mask) {
				this.backend = backend;
				this.mask = mask;
			}

			public async PausedSyscallSession run (uint pid, Cancellable? cancellable) throws Error, IOError {
				var session = yield PausedSyscallSession.open (pid, cancellable);
				yield session.wait_for_syscall (mask, cancellable);
				backend.paused_syscalls[pid] = session;
				return session;
			}
		}

		public async void resume_syscall (uint pid, Cancellable? cancellable) throws Error, IOError {
			PausedSyscallSession session;
			if (!paused_syscalls.unset (pid, out session))
				throw new Error.INVALID_ARGUMENT ("Invalid PID");

			session.close ();
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			SpawnedProcess? p = spawned_processes[pid];
			if (p == null)
				throw new Error.INVALID_ARGUMENT ("Invalid PID");
			yield p.input (data, cancellable);
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield perform<SpawnedProcess> (new ResumeTask (this), pid, cancellable);
		}

		private class ResumeTask : Object, Task<SpawnedProcess?> {
			private weak LinuxHelperBackend backend;

			public ResumeTask (LinuxHelperBackend backend) {
				this.backend = backend;
			}

			public async SpawnedProcess? run (uint pid, Cancellable? cancellable) throws Error, IOError {
				if (backend.exec_waiters.has_key (pid))
					throw new Error.INVALID_OPERATION ("Invalid operation");

				ExecTransitionSession session;
				if (backend.exec_transitions.unset (pid, out session)) {
					session.close ();
					return null;
				}

				InjectSession inject_session;
				if (backend.suspended_by_inject.unset (pid, out inject_session)) {
					inject_session.close ();
					return null;
				}

				SpawnedProcess? p = backend.spawned_processes[pid];
				if (p != null) {
					p.resume ();
					return p;
				}

				throw new Error.INVALID_ARGUMENT ("Invalid PID");
			}
		}

		public async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			Posix.kill ((Posix.pid_t) pid, Posix.Signal.KILL);
		}

		public async void inject_library (uint pid, UnixInputStream library_so, string entrypoint, string data,
				AgentFeatures features, uint id, Cancellable? cancellable) throws Error, IOError {
			var spec = new InjectSpec (library_so, entrypoint, data, features, id);
			var task = new InjectTask (this, spec);
			RemoteAgent agent = yield perform (task, pid, cancellable);
			take_agent (agent);
		}

		private class InjectTask : Object, Task<RemoteAgent> {
			private weak LinuxHelperBackend backend;
			private InjectSpec spec;

			public InjectTask (LinuxHelperBackend backend, InjectSpec spec) {
				this.backend = backend;
				this.spec = spec;
			}

			public async RemoteAgent run (uint pid, Cancellable? cancellable) throws Error, IOError {
				PausedSyscallSession? pss = backend.paused_syscalls[pid];
				if (pss != null)
					yield pss.interrupt (cancellable);

				if (KernelInjectSession.is_available ()) {
					var session = yield KernelInjectSession.open (pid, cancellable);
					return yield session.inject (spec, cancellable);
				}

				try {
					var session = yield InjectSession.open (pid, cancellable);
					RemoteAgent agent = yield session.inject (spec, cancellable);
					if (session.was_group_stopped)
						backend.suspended_by_inject[pid] = session;
					else
						session.close ();
					return agent;
				} catch (Error e) {
					if (!(e is Error.PERMISSION_DENIED) || !ProcMemInjectSession.is_available ())
						throw e;
					var session = yield ProcMemInjectSession.open (pid, cancellable);
					return yield session.inject (spec, cancellable);
				}
			}
		}

		public async IOStream request_control_channel (uint id, Cancellable? cancellable) throws Error, IOError {
			RemoteAgent agent = agents[id];
			if (agent == null)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			UnixConnection agent_ctrl = agent.agent_ctrl;
			if (agent_ctrl == null)
				throw new Error.NOT_SUPPORTED ("Control channel feature not enabled");

			return agent_ctrl;
		}

		private void update_process_status (uint pid, ProcessStatus status) {
			foreach (RemoteAgent agent in agents.values) {
				if (agent.pid == pid)
					agent.process_status = status;
			}
		}

		private void take_agent (RemoteAgent agent) {
			if (agent.state == STOPPED) {
				var source = new IdleSource ();
				source.set_callback (() => {
					on_agent_stopped (agent);
					return Source.REMOVE;
				});
				source.attach (MainContext.get_thread_default ());
				return;
			}
			agents[agent.inject_spec.id] = agent;
			agent.notify["state"].connect (on_agent_state_changed);
		}

		private void on_agent_state_changed (Object object, ParamSpec pspec) {
			var agent = (RemoteAgent) object;
			if (agent.state == STOPPED)
				on_agent_stopped (agent);
		}

		private void on_agent_stopped (RemoteAgent agent) {
			uint id = agent.inject_spec.id;

			uninjected (id);

			if (agent.unload_policy == IMMEDIATE && agent.process_status == NORMAL) {
				// TODO: Implement did_not_exec() guard.
				deallocate_agent.begin (agent);
			} else {
				agents.unset (id);
				maybe_emit_idle ();
			}
		}

		private async void deallocate_agent (RemoteAgent agent) {
			uint pid = agent.pid;
			try {
				yield perform<RemoteAgent> (new DeallocateTask (agent), pid, null);
			} catch (GLib.Error e) {
			}

			agents.unset (agent.inject_spec.id);
			maybe_emit_idle ();
		}

		private class DeallocateTask : Object, Task<RemoteAgent> {
			private RemoteAgent agent;

			public DeallocateTask (RemoteAgent agent) {
				this.agent = agent;
			}

			public async RemoteAgent run (uint pid, Cancellable? cancellable) throws Error, IOError {
				var session = yield CleanupSession.open (pid, cancellable);
				yield session.deallocate (agent.bootstrap_result, cancellable);
				session.close ();
				return agent;
			}
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			RemoteAgent? agent = agents[id];
			if (agent == null || agent.state != STARTED)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			yield agent.demonitor (cancellable);

			schedule_agent_expiry_for_id (id);
		}

		public async void demonitor_and_clone_injectee_state (uint id, uint clone_id, AgentFeatures features,
				Cancellable? cancellable) throws Error, IOError {
			RemoteAgent? agent = agents[id];
			if (agent == null || agent.state != STARTED)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			yield agent.demonitor (cancellable);

			agents[clone_id] = agent.clone (clone_id, features);

			schedule_agent_expiry_for_id (id);
			schedule_agent_expiry_for_id (clone_id);
		}

		public async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			RemoteAgent? old_agent = agents[id];
			if (old_agent == null || old_agent.state != PAUSED)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			cancel_agent_expiry_for_id (id);

			var task = new RejuvenateTask (old_agent);
			RemoteAgent new_agent = yield perform (task, pid, cancellable);
			take_agent (new_agent);
		}

		private class RejuvenateTask : Object, Task<RemoteAgent> {
			private RemoteAgent old_agent;

			public RejuvenateTask (RemoteAgent old_agent) {
				this.old_agent = old_agent;
			}

			public async RemoteAgent run (uint pid, Cancellable? cancellable) throws Error, IOError {
				var session = yield InjectSession.open (pid, cancellable);
				RemoteAgent new_agent = yield session.rejuvenate (old_agent, cancellable);
				session.close ();
				return new_agent;
			}
		}

		private void maybe_emit_idle () {
			if (is_idle)
				idle ();
		}

		private void schedule_agent_expiry_for_id (uint id) {
			Source previous_source;
			if (agent_expiries.unset (id, out previous_source))
				previous_source.destroy ();

			var source = new TimeoutSource.seconds (20);
			source.set_callback (() => {
				bool removed = agent_expiries.unset (id);
				assert (removed);

				RemoteAgent agent = agents[id];
				agent.stop ();

				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());
			agent_expiries[id] = source;
		}

		private void cancel_agent_expiry_for_id (uint id) {
			Source source;
			bool found = agent_expiries.unset (id, out source);
			assert (found);

			source.destroy ();
		}

		private async T perform<T> (Task<T> task, uint pid, Cancellable? cancellable) throws Error, IOError {
			Gee.Queue<TaskEntry> queue = task_queues[pid];
			if (queue == null) {
				queue = new Gee.ArrayQueue<TaskEntry> ();
				task_queues[pid] = queue;

				var source = new IdleSource ();
				source.set_callback (() => {
					process_tasks.begin (queue, pid);
					return Source.REMOVE;
				});
				source.attach (MainContext.get_thread_default ());
			}

			var entry = new TaskEntry ((Task<Object>) task, cancellable);
			queue.offer (entry);

			return yield entry.promise.future.wait_async (cancellable);
		}

		private async void process_tasks (Gee.Queue<TaskEntry> queue, uint pid) {
			var main_context = MainContext.get_thread_default ();

			TaskEntry entry;
			while ((entry = queue.poll ()) != null) {
				try {
					entry.cancellable.set_error_if_cancelled ();
					var result = yield entry.task.run (pid, entry.cancellable);
					entry.promise.resolve (result);
				} catch (GLib.Error e) {
					entry.promise.reject (e);
				}

				var source = new IdleSource ();
				source.set_callback (process_tasks.callback);
				source.attach (main_context);
				yield;
			}

			task_queues.unset (pid);
		}

		private class TaskEntry {
			public Task<Object> task;
			public Cancellable? cancellable;
			public Promise<Object> promise = new Promise<Object> ();

			public TaskEntry (Task<Object> task, Cancellable? cancellable) {
				this.task = task;
				this.cancellable = cancellable;
			}
		}

		private interface Task<T> : Object {
			public abstract async T run (uint pid, Cancellable? cancellable) throws Error, IOError;
		}
	}

	public unowned string arch_name_from_pid (uint pid) throws Error {
		Gum.CpuType cpu_type = cpu_type_from_pid (pid);

		switch (cpu_type) {
			case Gum.CpuType.IA32:
			case Gum.CpuType.ARM:
			case Gum.CpuType.MIPS:
				return "32";

			case Gum.CpuType.AMD64:
			case Gum.CpuType.ARM64:
				return "64";

			default:
				assert_not_reached ();
		}
	}

	public Gum.CpuType cpu_type_from_file (string path) throws Error {
		try {
			return Gum.Linux.cpu_type_from_file (path);
		} catch (Gum.Error e) {
			if (e is Gum.Error.NOT_FOUND)
				throw new Error.EXECUTABLE_NOT_FOUND ("Unable to find executable at '%s'", path);
			else if (e is Gum.Error.NOT_SUPPORTED)
				throw new Error.EXECUTABLE_NOT_SUPPORTED ("Unable to parse executable at '%s'", path);
			else if (e is Gum.Error.PERMISSION_DENIED)
				throw new Error.PERMISSION_DENIED ("Unable to access executable at '%s'", path);
			else
				throw new Error.NOT_SUPPORTED ("%s", e.message);
		}
	}

	public Gum.CpuType cpu_type_from_pid (uint pid) throws Error {
		try {
			return Gum.Linux.cpu_type_from_pid ((Posix.pid_t) pid);
		} catch (Gum.Error e) {
			if (e is Gum.Error.NOT_FOUND)
				throw new Error.PROCESS_NOT_FOUND ("Unable to find process with pid %u", pid);
			else if (e is Gum.Error.PERMISSION_DENIED)
				throw new Error.PERMISSION_DENIED ("Unable to access process with pid %u", pid);
			else
				throw new Error.NOT_SUPPORTED ("%s", e.message);
		}
	}

	private sealed class SpawnedProcess : Object {
		public signal void terminated ();
		public signal void output (int fd, uint8[] data);

		public uint pid {
			get;
			construct;
		}

		public StdioPipes? pipes {
			get;
			construct;
		}

		private State state = SUSPENDED;
		private uint watch_id;
		private OutputStream? stdin_stream;

		private Cancellable io_cancellable = new Cancellable ();

		private enum State {
			SUSPENDED,
			RUNNING,
		}

		public SpawnedProcess (uint pid, StdioPipes? pipes) {
			Object (pid: pid, pipes: pipes);
		}

		~SpawnedProcess () {
			try {
				resume ();
			} catch (Error e) {
			}
		}

		construct {
			monitor ();

			if (pipes != null) {
				stdin_stream = pipes.input;
				process_next_output_from.begin (pipes.output, 1);
				process_next_output_from.begin (pipes.error, 2);
			}
		}

		public void close () {
			demonitor ();

			io_cancellable.cancel ();
		}

		public void resume () throws Error {
			if (state == SUSPENDED) {
				ptrace (DETACH, pid);
				state = RUNNING;
			}
		}

		public void monitor () {
			if (watch_id == 0)
				watch_id = ChildWatch.add ((Pid) pid, on_termination);
		}

		public void demonitor () {
			if (watch_id != 0) {
				Source.remove (watch_id);
				watch_id = 0;
			}
		}

		private void on_termination (Pid pid, int status) {
			watch_id = 0;
			stdin_stream = null;

			terminated ();
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
				if (!(e is IOError.CANCELLED))
					output (fd, {});
			}
		}
	}

	private sealed class ExecTransitionSession : SeizeSession {
		private ExecTransitionSession (uint pid) {
			Object (pid: pid, on_init: SeizeSession.InitBehavior.CONTINUE);
		}

		public static async ExecTransitionSession open (uint pid, Cancellable? cancellable) throws Error, IOError {
			var session = new ExecTransitionSession (pid);

			try {
				yield session.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return session;
		}

		public async void wait_for_exec (Cancellable? cancellable) throws Error, IOError {
			yield wait_for_signal (TRAP, cancellable);
			step ();
			yield wait_for_signal (TRAP, cancellable);
		}
	}

	private sealed class PausedSyscallSession : SeizeSession {
		private State state = PENDING;

		private enum State {
			PENDING,
			SATISFIED
		}

		private PausedSyscallSession (uint pid) {
			Object (pid: pid);
		}

		public static async PausedSyscallSession open (uint pid, Cancellable? cancellable) throws Error, IOError {
			var session = new PausedSyscallSession (pid);

			try {
				yield session.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return session;
		}

		public async void wait_for_syscall (LinuxSyscallMask mask, Cancellable? cancellable) throws Error, IOError {
			bool on_syscall_entry = true;
			int pending_signal = 0;
			while (state != SATISFIED) {
				ptrace (SYSCALL, tid, null, (void *) pending_signal);
				pending_signal = 0;

				var wr = yield ChildProcess.wait_for_next_stop (tid, cancellable);
				wr.check_stopped ();

				if (wr.is_ptrace_event_stop) {
					if (wr.ptrace_event == PtraceEvent.EXEC)
						on_syscall_entry = true;

					continue;
				}

				if (wr.is_syscall_stop) {
					if (on_syscall_entry) {
						get_regs (&saved_regs);
						var id = get_syscall_id (saved_regs);

						if (_syscall_satisfies (id, mask))
							state = SATISFIED;
					}

					on_syscall_entry = !on_syscall_entry;
					continue;
				}

				if (wr.should_deliver_to_tracee)
					pending_signal = wr.stop_signal;
				else
					pending_signal = 0;
			}
		}

		public async void interrupt (Cancellable? cancellable) throws Error, IOError {
			ptrace (CONT, tid, null, (void *) Posix.Signal.STOP);
			yield wait_for_signal (STOP, cancellable);
			var regs = GPRegs ();
			get_regs (&regs);

			saved_regs.orig_syscall = -1;
			saved_regs.program_counter = regs.program_counter;
			set_regs (saved_regs);
		}

		private static int get_syscall_id (GPRegs regs) {
#if X86
			return regs.orig_eax;
#elif X86_64
			return (int) regs.orig_rax;
#elif ARM
			return (int) regs.r[7];
#elif ARM64
			return (int) regs.x[8];
#elif MIPS
			return (int) regs.v[0];
#endif
		}
	}

	private const size_t DUMMY_RETURN_ADDRESS = 0x320;

	public enum ProcessStatus {
		NORMAL,
		EXEC_PENDING
	}

	private const size_t MAP_FAILED = ~0;

	private const uint64 SOCK_CLOEXEC = 0x80000;

	private sealed class InjectSession : SeizeSession {
		private static ProcMapsSnapshot.Mapping local_libc;
		private static uint64 mmap_offset;
		private static uint64 munmap_offset;

		private static string fallback_ld;
		private static string fallback_libc;

		private static ProcMapsSnapshot.Mapping? local_android_ld;

		static construct {
			var libc = Gum.Process.get_libc_module ();
			uint local_pid = Posix.getpid ();
			var local_maps = ProcMapsSnapshot.from_pid (local_pid);
			local_libc = local_maps.find_module_by_path (libc.path);
			assert (local_libc != null);
			mmap_offset = (uint64) (uintptr) libc.find_export_by_name ("mmap") - local_libc.start;
			munmap_offset = (uint64) (uintptr) libc.find_export_by_name ("munmap") - local_libc.start;

			try {
				var program = new Gum.ElfModule.from_file ("/proc/self/exe");
				fallback_ld = program.interpreter;
				fallback_libc = Path.get_basename (local_libc.path);
			} catch (Gum.Error e) {
				assert_not_reached ();
			}

			try {
				string target = FileUtils.read_link (fallback_ld);
				string parent_dir = Path.get_dirname (fallback_ld);
				fallback_ld = Filename.canonicalize (target, parent_dir);
			} catch (FileError e) {
			}

#if ANDROID
			local_android_ld = local_maps.find_module_by_path (fallback_ld);
#endif
		}

		private InjectSession (uint pid) {
			Object (pid: pid);
		}

		public static async InjectSession open (uint pid, Cancellable? cancellable) throws Error, IOError {
			var session = new InjectSession (pid);

			try {
				yield session.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return session;
		}

		public async RemoteAgent inject (InjectSpec spec, Cancellable? cancellable) throws Error, IOError {
			string fallback_address = make_fallback_address ();
			LoaderLayout loader_layout = compute_loader_layout (spec, fallback_address);

			BootstrapResult bootstrap_result = yield bootstrap (loader_layout.size, cancellable);
			uint64 loader_base = (uintptr) bootstrap_result.context.allocation_base;

			try {
				unowned uint8[] loader_code = Frida.Data.HelperBackend.get_loader_bin_blob ().data;
				write_memory (loader_base, loader_code);
				maybe_fixup_helper_code (loader_base, loader_code);

				var loader_ctx = HelperLoaderContext ();
				loader_ctx.ctrlfds = bootstrap_result.context.ctrlfds;
				loader_ctx.agent_entrypoint = (string *) (loader_base + loader_layout.agent_entrypoint_offset);
				loader_ctx.agent_data = (string *) (loader_base + loader_layout.agent_data_offset);
				loader_ctx.fallback_address = (string *) (loader_base + loader_layout.fallback_address_offset);
				loader_ctx.libc = (HelperLibcApi *) (loader_base + loader_layout.libc_api_offset);
				write_memory (loader_base + loader_layout.ctx_offset, (uint8[]) &loader_ctx);
				write_memory (loader_base + loader_layout.libc_api_offset, (uint8[]) &bootstrap_result.libc);
				write_memory_string (loader_base + loader_layout.agent_entrypoint_offset, spec.entrypoint);
				write_memory_string (loader_base + loader_layout.agent_data_offset, spec.data);
				write_memory_string (loader_base + loader_layout.fallback_address_offset, fallback_address);

				return yield launch_loader (FROM_SCRATCH, spec, bootstrap_result, null, fallback_address, loader_layout,
					cancellable);
			} catch (GLib.Error error) {
				try {
					yield deallocate_memory ((uintptr) bootstrap_result.libc.munmap, loader_base, loader_layout.size,
						null);
				} catch (GLib.Error e) {
				}

				if (error is IOError)
					throw (IOError) error;
				throw (Error) error;
			}
		}

		public async RemoteAgent rejuvenate (RemoteAgent old_agent, Cancellable? cancellable) throws Error, IOError {
			InjectSpec spec = old_agent.inject_spec;
			BootstrapResult bootstrap_result = old_agent.bootstrap_result;

			string fallback_address = make_fallback_address ();
			LoaderLayout loader_layout = compute_loader_layout (spec, fallback_address);
			uint64 loader_base = (uintptr) bootstrap_result.context.allocation_base;
			uint64 loader_ctrlfds_location = loader_base + loader_layout.ctx_offset;

			if (bootstrap_result.context.enable_ctrlfds) {
				var builder = new RemoteCallBuilder ((uintptr) bootstrap_result.libc.socketpair, saved_regs);
				builder
					.add_argument (Posix.AF_UNIX)
					.add_argument (Posix.SOCK_STREAM | SOCK_CLOEXEC)
					.add_argument (0)
					.add_argument (loader_ctrlfds_location);
				RemoteCall call = builder.build (this);
				RemoteCallResult res = yield call.execute (cancellable);
				if (res.status != COMPLETED)
					throw new Error.NOT_SUPPORTED ("Unexpected crash while trying to re-create ctrlfds");
				if (res.return_value == 0) {
					uint8[] raw_fds = read_memory (loader_ctrlfds_location, 2 * sizeof (int));
					Memory.copy (&bootstrap_result.context.ctrlfds, raw_fds, raw_fds.length);
				} else {
					bootstrap_result.context.ctrlfds[0] = -1;
					bootstrap_result.context.ctrlfds[1] = -1;
				}
			}

			write_memory (loader_base + loader_layout.fallback_address_offset, fallback_address.data);

			return yield launch_loader (RELAUNCH, spec, bootstrap_result, old_agent.agent_ctrl, fallback_address, loader_layout,
				cancellable);
		}

		private struct LoaderLayout {
			public size_t size;

			public size_t ctx_offset;
			public size_t libc_api_offset;
			public size_t agent_entrypoint_offset;
			public size_t agent_data_offset;
			public size_t fallback_address_offset;
		}

		private LoaderLayout compute_loader_layout (InjectSpec spec, string fallback_address) {
			var layout = LoaderLayout ();

			unowned uint8[] code = Frida.Data.HelperBackend.get_loader_bin_blob ().data;

			size_t code_size = round_size_to_page_size (code.length);

			size_t agent_entrypoint_size = spec.entrypoint.data.length + 1;
			size_t agent_data_size = spec.data.data.length + 1;

			size_t data_size = 0;
			data_size += sizeof (HelperLoaderContext);
			data_size += sizeof (HelperLibcApi);
			data_size += agent_entrypoint_size;
			data_size += agent_data_size;
			data_size += fallback_address.data.length + 1;
			data_size = round_size_to_page_size (data_size);

			layout.size = code_size + data_size;

			layout.ctx_offset = code_size;
			layout.libc_api_offset = layout.ctx_offset + sizeof (HelperLoaderContext);
			layout.agent_entrypoint_offset = layout.libc_api_offset + sizeof (HelperLibcApi);
			layout.agent_data_offset = layout.agent_entrypoint_offset + agent_entrypoint_size;
			layout.fallback_address_offset = layout.agent_data_offset + agent_data_size;

			return layout;
		}

		private async RemoteAgent launch_loader (LoaderLaunch launch, InjectSpec spec, BootstrapResult bres,
				UnixConnection? agent_ctrl, string fallback_address, LoaderLayout loader_layout, Cancellable? cancellable)
				throws Error, IOError {
			Future<RemoteAgent> future_agent =
				establish_connection (launch, spec, bres, agent_ctrl, fallback_address, cancellable);

			uint64 loader_base = (uintptr) bres.context.allocation_base;
			GPRegs regs = saved_regs;
			regs.stack_pointer = bres.allocated_stack.stack_root;
			var call_builder = new RemoteCallBuilder (loader_base, regs);
			call_builder.add_argument (loader_base + loader_layout.ctx_offset);
			RemoteCall loader_call = call_builder.build (this);
			RemoteCallResult loader_result = yield loader_call.execute (cancellable);
			if (loader_result.status != COMPLETED) {
				uint64 pc = loader_result.regs.program_counter;
				if (pc >= loader_base && pc < loader_base + Frida.Data.HelperBackend.get_loader_bin_blob ().data.length) {
					throw new Error.NOT_SUPPORTED (
						"Loader crashed with signal %d at offset 0x%x; please file a bug\n%s",
						loader_result.stop_signal,
						(uint) (pc - loader_base),
						loader_result.regs.to_string ());
				} else {
					throw new Error.NOT_SUPPORTED ("Loader crashed with signal %d; please file a bug\n%s",
						loader_result.stop_signal,
						loader_result.regs.to_string ());
				}
			}

			var establish_cancellable = new Cancellable ();
			var main_context = MainContext.get_thread_default ();

			var timeout_source = new TimeoutSource.seconds (5);
			timeout_source.set_callback (() => {
				establish_cancellable.cancel ();
				return Source.REMOVE;
			});
			timeout_source.attach (main_context);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				establish_cancellable.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (main_context);

			RemoteAgent agent = null;
			try {
				agent = yield future_agent.wait_async (establish_cancellable);
			} catch (IOError e) {
				cancellable.set_error_if_cancelled ();
				throw new Error.PROCESS_NOT_RESPONDING ("Unexpectedly timed out trying to sync up with agent");
			} finally {
				cancel_source.destroy ();
				timeout_source.destroy ();
			}

			agent.ack ();

			return agent;
		}

		private async BootstrapResult bootstrap (size_t loader_size, Cancellable? cancellable) throws Error, IOError {
			var result = new BootstrapResult ();

			unowned uint8[] bootstrapper_code = Frida.Data.HelperBackend.get_bootstrapper_bin_blob ().data;
			size_t bootstrapper_size = round_size_to_page_size (bootstrapper_code.length);

			size_t stack_size = 64 * 1024;

			uint64 allocation_base = 0;
			size_t allocation_size = size_t.max (bootstrapper_size, loader_size) + stack_size;

			var remote_maps = ProcMapsSnapshot.from_pid (pid);
			var remote_libc = remote_maps.find_module_by_path (local_libc.path);
			uint64 remote_mmap = 0;
			uint64 remote_munmap = 0;
#if ANDROID
			bool same_libc = remote_libc != null
					&& remote_libc.device.major == local_libc.device.major
					&& remote_libc.inode == local_libc.inode
					&& remote_libc.path == local_libc.path;
#else
			bool same_libc = remote_libc != null
					&& remote_libc.device.equals (local_libc.device)
					&& remote_libc.inode == local_libc.inode
					&& remote_libc.path == local_libc.path;
#endif
			if (same_libc) {
				remote_mmap = remote_libc.start + mmap_offset;
				remote_munmap = remote_libc.start + munmap_offset;
			}

			if (remote_mmap != 0) {
				allocation_base = yield allocate_memory (remote_mmap, allocation_size,
					Posix.PROT_READ | Posix.PROT_WRITE | Posix.PROT_EXEC, cancellable);
			} else {
				var code_swap = yield new ProcessCodeSwapScope (this, bootstrapper_code, cancellable);
				uint64 code_start = code_swap.code_start;
				uint64 code_end = code_start + bootstrapper_size;
				maybe_fixup_helper_code (code_start, bootstrapper_code);

				var call_builder = new RemoteCallBuilder (code_start, saved_regs);

				uint64 bootstrap_ctx_location;
				call_builder.reserve_stack_space (sizeof (HelperBootstrapContext), out bootstrap_ctx_location);

				var bootstrap_ctx = HelperBootstrapContext ();
				bootstrap_ctx.allocation_size = allocation_size;
				write_memory (bootstrap_ctx_location, (uint8[]) &bootstrap_ctx);

				call_builder.add_argument (bootstrap_ctx_location);

				RemoteCallResult bootstrap_result = yield call_builder.build (this).execute (cancellable);
				var status = (HelperBootstrapStatus) bootstrap_result.return_value;
				if (bootstrap_result.status != COMPLETED || status != ALLOCATION_SUCCESS)
					throw_bootstrap_error (bootstrap_result, status, code_start, code_end);

				uint8[] output_context = read_memory (bootstrap_ctx_location, sizeof (HelperBootstrapContext));
				Memory.copy (&bootstrap_ctx, output_context, output_context.length);

				allocation_base = (uintptr) bootstrap_ctx.allocation_base;
				code_swap.revert ();
			}

			result.allocated_stack.stack_base = (void *) (allocation_base + allocation_size - stack_size);
			result.allocated_stack.stack_size = stack_size;

			try {
				write_memory (allocation_base, bootstrapper_code);
				maybe_fixup_helper_code (allocation_base, bootstrapper_code);
				uint64 code_start = allocation_base;
				uint64 code_end = code_start + bootstrapper_size;

				HelperBootstrapStatus status = SUCCESS;
				do {
					GPRegs regs = saved_regs;
					regs.stack_pointer = result.allocated_stack.stack_root;
					var call_builder = new RemoteCallBuilder (code_start, regs);

					unowned uint8[] fallback_ld_data = fallback_ld.data;
					unowned uint8[] fallback_libc_data = fallback_libc.data;

					uint64 libc_api_location, bootstrap_ctx_location, fallback_ld_location, fallback_libc_location;
					call_builder
						.reserve_stack_space (sizeof (HelperLibcApi), out libc_api_location)
						.reserve_stack_space (sizeof (HelperBootstrapContext), out bootstrap_ctx_location)
						.reserve_stack_space (fallback_ld_data.length + 1, out fallback_ld_location)
						.reserve_stack_space (fallback_libc_data.length + 1, out fallback_libc_location);

					var bootstrap_ctx = HelperBootstrapContext ();
					bootstrap_ctx.allocation_base = (void *) allocation_base;
					bootstrap_ctx.allocation_size = allocation_size;
					bootstrap_ctx.page_size = Gum.query_page_size ();
					bootstrap_ctx.fallback_ld = (string *) fallback_ld_location;
					bootstrap_ctx.fallback_libc = (string *) fallback_libc_location;
					bootstrap_ctx.enable_ctrlfds = PidFileDescriptor.getfd_is_supported ();
					bootstrap_ctx.libc = (HelperLibcApi *) libc_api_location;
					write_memory (bootstrap_ctx_location, (uint8[]) &bootstrap_ctx);
					unowned uint8[] fallback_ld_cstr = fallback_ld_data[:fallback_ld_data.length + 1];
					unowned uint8[] fallback_libc_cstr = fallback_libc_data[:fallback_libc_data.length + 1];
					write_memory (fallback_ld_location, fallback_ld_cstr);
					write_memory (fallback_libc_location, fallback_libc_cstr);
					call_builder.add_argument (bootstrap_ctx_location);

					RemoteCall bootstrap_call = call_builder.build (this);
					RemoteCallResult bootstrap_result = yield bootstrap_call.execute (cancellable);
					status = (HelperBootstrapStatus) bootstrap_result.return_value;

					bool restart_after_libc_load =
						bootstrap_result.status == RAISED_SIGNAL && bootstrap_result.stop_signal == Posix.Signal.STOP;
					if (restart_after_libc_load) {
						bootstrap_result = yield bootstrap_call.execute (cancellable);
						status = (HelperBootstrapStatus) bootstrap_result.return_value;
					}

					if (!(bootstrap_result.status == COMPLETED && (status == SUCCESS || status == TOO_EARLY)))
						throw_bootstrap_error (bootstrap_result, status, code_start, code_end);

					uint8[] output_context = read_memory (bootstrap_ctx_location, sizeof (HelperBootstrapContext));
					Memory.copy (&result.context, output_context, output_context.length);

					uint8[] output_libc = read_memory (libc_api_location, sizeof (HelperLibcApi));
					Memory.copy (&result.libc, output_libc, output_libc.length);

					result.context.libc = &result.libc;

					if (result.context.rtld_flavor == ANDROID && result.libc.dlopen == null) {
						if (restart_after_libc_load)
							remote_maps = ProcMapsSnapshot.from_pid (pid);
						var remote_ld = remote_maps.find_mapping ((uintptr) result.context.rtld_base);
						bool same_ld = remote_ld != null && local_android_ld != null
							&& remote_ld.device.equals (local_android_ld.device)
							&& remote_ld.inode == local_android_ld.inode;
						if (!same_ld)
							throw new Error.NOT_SUPPORTED ("Unable to locate Android dynamic linker; please file a bug");
						result.libc.dlopen = rebase_pointer ((uintptr) dlopen, local_android_ld, remote_ld);
						result.libc.dlclose = rebase_pointer ((uintptr) dlclose, local_android_ld, remote_ld);
						result.libc.dlsym = rebase_pointer ((uintptr) dlsym, local_android_ld, remote_ld);
						result.libc.dlerror = rebase_pointer ((uintptr) dlerror, local_android_ld, remote_ld);
					}

					if (status == TOO_EARLY)
						yield resume_until_execution_reaches ((uintptr) result.context.r_brk, cancellable);
				} while (status == TOO_EARLY);
			} catch (GLib.Error e) {
				if (remote_munmap != 0) {
					try {
						yield deallocate_memory (remote_munmap, allocation_base, allocation_size, null);
					} catch (GLib.Error e) {
					}
				}

				throw_api_error (e);
			}

			return result;
		}

		[NoReturn]
		private static void throw_bootstrap_error (RemoteCallResult bootstrap_result, HelperBootstrapStatus status,
				uint64 code_start, uint64 code_end) throws Error {
			if (bootstrap_result.status == COMPLETED) {
				throw new Error.NOT_SUPPORTED ("Bootstrapper failed due to '%s'; " +
					"please file a bug",
					Marshal.enum_to_nick<HelperBootstrapStatus> (status));
			} else {
				uint64 pc = bootstrap_result.regs.program_counter;
				if (pc >= code_start && pc < code_end) {
					throw new Error.NOT_SUPPORTED (
						"Bootstrapper crashed with signal %d at offset 0x%x; please file a bug\n%s",
						bootstrap_result.stop_signal,
						(uint) (pc - code_start),
						bootstrap_result.regs.to_string ());
				} else {
					throw new Error.NOT_SUPPORTED ("Bootstrapper crashed with signal %d; please file a bug\n%s",
						bootstrap_result.stop_signal,
						bootstrap_result.regs.to_string ());
				}
			}
		}

		private static void * rebase_pointer (uintptr local_ptr, ProcMapsSnapshot.Mapping local, ProcMapsSnapshot.Mapping remote) {
			var offset = local_ptr - local.start;
			return (void *) (remote.start + offset);
		}

		private static string make_fallback_address () {
			return "/frida-" + Uuid.string_random ();
		}

		private Future<RemoteAgent> establish_connection (LoaderLaunch launch, InjectSpec spec, BootstrapResult bres,
				UnixConnection? agent_ctrl, string fallback_address, Cancellable? cancellable) throws Error, IOError {
			var promise = new Promise<RemoteAgent> ();

			FileDescriptor? sockfd = null;
			if (PidFileDescriptor.getfd_is_supported () && bres.context.ctrlfds[0] != -1) {
				try {
					var pidfd = PidFileDescriptor.from_pid (pid);
					sockfd = pidfd.getfd (bres.context.ctrlfds[0]);
				} catch (Error e) {
				}
			}

			if (sockfd != null) {
				Socket socket;
				try {
					socket = new Socket.from_fd (sockfd.steal ());
				} catch (GLib.Error e) {
					assert_not_reached ();
				}
				var connection = (UnixConnection) SocketConnection.factory_create_connection (socket);

				do_establish_connection.begin (connection, launch, spec, bres, agent_ctrl, promise, cancellable);
			} else {
				var server_address = new UnixSocketAddress.with_type (fallback_address, -1, UnixSocketAddressType.ABSTRACT);

				Socket server_socket;
				try {
					var socket = new Socket (SocketFamily.UNIX, SocketType.STREAM, SocketProtocol.DEFAULT);
					socket.bind (server_address, true);
					socket.listen ();
					server_socket = socket;
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("%s", e.message);
				}

				do_establish_connection_through_server.begin (server_socket, launch, spec, bres, agent_ctrl, promise,
					cancellable);
			}

			return promise.future;
		}

		private async void do_establish_connection (UnixConnection connection, LoaderLaunch launch, InjectSpec spec,
				BootstrapResult bres, UnixConnection? agent_ctrl, Promise<RemoteAgent> promise, Cancellable? cancellable) {
			try {
				var agent = yield RemoteAgent.start (launch, spec, pid, bres, connection, agent_ctrl, cancellable);
				promise.resolve (agent);
			} catch (Error e) {
				promise.reject (e);
			} catch (IOError e) {
				promise.reject (e);
			}
		}

		private async void do_establish_connection_through_server (Socket server_socket, LoaderLaunch launch, InjectSpec spec,
				BootstrapResult bres, UnixConnection? agent_ctrl, Promise<RemoteAgent> promise, Cancellable? cancellable) {
			var listener = new SocketListener ();
			try {
				listener.add_socket (server_socket, null);

				var connection = (UnixConnection) yield listener.accept_async (cancellable);
				do_establish_connection.begin (connection, launch, spec, bres, agent_ctrl, promise, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					promise.reject ((IOError) e);
				else
					promise.reject (new Error.TRANSPORT ("%s", e.message));
			} finally {
				listener.close ();
			}
		}

		private void maybe_fixup_helper_code (uint64 base_address, uint8[] code) throws Error {
#if MIPS
			//
			// To avoid having to implement a dynamic linker, we carefully craft our helpers to avoid the need for relocations.
			// For MIPS however, it seems we cannot avoid them entirely. This means we need to fix up the .got section, as it
			// contains some absolute addresses. To find it without embedding the ELF of each helper and parsing that at
			// runtime, we use a linker script (helpers/helper.lds) to ensure that our .got is:
			// - Last
			// - Aligned on a 64-byte boundary
			// - Padded to 64 bytes
			// We assume that 64 bytes is sufficient for both of our helpers.
			//
			size_t padded_got_size = 64;
			size_t entries_start_offset = 8;
			size_t entries_size = padded_got_size - entries_start_offset;
			uint8[] entries = code[code.length - entries_size:];
			for (ulong offset = 0; offset != entries_size; offset += sizeof (size_t)) {
				size_t * entry = &entries[offset];
				*entry += base_address;
			}
			write_memory (base_address + code.length - entries_size, entries);
#endif
		}
	}

	public sealed class InjectSpec {
		public FileDescriptorBased library_so {
			get;
			private set;
		}

		public string entrypoint {
			get;
			private set;
		}

		public string data {
			get;
			private set;
		}

		public AgentFeatures features {
			get;
			private set;
		}

		public uint id {
			get;
			private set;
		}

		public InjectSpec (FileDescriptorBased library_so, string entrypoint, string data, AgentFeatures features, uint id) {
			this.library_so = library_so;
			this.entrypoint = entrypoint;
			this.data = data;
			this.features = features;
			this.id = id;
		}

		public InjectSpec clone (uint clone_id, AgentFeatures features) {
			return new InjectSpec (library_so, entrypoint, data, features, clone_id);
		}
	}

	private sealed class CleanupSession : SeizeSession {
		private CleanupSession (uint pid) {
			Object (pid: pid);
		}

		public static async CleanupSession open (uint pid, Cancellable? cancellable) throws Error, IOError {
			var session = new CleanupSession (pid);

			try {
				yield session.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return session;
		}

		public async void deallocate (BootstrapResult bres, Cancellable? cancellable) throws Error, IOError {
			yield deallocate_memory ((uintptr) bres.libc.munmap, (uintptr) bres.context.allocation_base,
				bres.context.allocation_size, cancellable);
		}
	}

	private sealed class ThreadSuspendSession : SeizeSession {
		private ThreadSuspendSession (uint pid, uint tid) {
			Object (pid: pid, tid: tid);
		}

		public static async ThreadSuspendSession open (uint pid, uint tid, Cancellable? cancellable) throws Error, IOError {
			var session = new ThreadSuspendSession (pid, tid);

			try {
				yield session.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return session;
		}
	}

	private struct AllocatedStack {
		public void * stack_base;
		public size_t stack_size;

		public uint64 stack_root {
			get {
				return (uint64) stack_base + (uint64) stack_size;
			}
		}
	}


	private sealed class BootstrapResult {
		public HelperBootstrapContext context;
		public HelperLibcApi libc;
		public AllocatedStack allocated_stack;

		public BootstrapResult clone () {
			var res = new BootstrapResult ();
			res.context = context;
			res.libc = libc;
			res.allocated_stack = allocated_stack;
			return res;
		}
	}

	private enum LoaderLaunch {
		FROM_SCRATCH,
		RELAUNCH
	}

	private sealed class RemoteAgent : Object {
		public uint pid {
			get;
			construct;
		}

		public InjectSpec inject_spec {
			get;
			construct;
		}

		public BootstrapResult bootstrap_result {
			get;
			construct;
		}

		public UnixConnection frida_ctrl {
			get;
			construct;
		}

		public UnixConnection? agent_ctrl {
			get {
				return _agent_ctrl;
			}
			construct {
				_agent_ctrl = value;
			}
		}

		public State state {
			get {
				return _state;
			}
		}

		public UnloadPolicy unload_policy {
			get {
				return _unload_policy;
			}
		}

		public ProcessStatus process_status {
			get;
			set;
			default = NORMAL;
		}

		public enum State {
			STARTED,
			STOPPED,
			PAUSED
		}

		private State _state = STARTED;
		private UnloadPolicy _unload_policy = IMMEDIATE;

		public UnixConnection? _agent_ctrl;
		private FileDescriptor? agent_ctrlfd_for_peer;

		private Promise<bool>? start_request = new Promise<bool> ();
		private Promise<bool> cancel_request = new Promise<bool> ();
		private Cancellable io_cancellable = new Cancellable ();

		private RemoteAgent (uint pid, InjectSpec spec, BootstrapResult bres, UnixConnection frida_ctrl,
				UnixConnection? agent_ctrl = null) {
			Object (
				pid: pid,
				inject_spec: spec,
				bootstrap_result: bres,
				frida_ctrl: frida_ctrl,
				agent_ctrl: agent_ctrl
			);
		}

		public override void constructed () {
			if ((inject_spec.features & AgentFeatures.CONTROL_CHANNEL) != 0 && _agent_ctrl == null) {
				int fds[2];
				Posix.socketpair (Posix.AF_UNIX, Posix.SOCK_STREAM | SOCK_CLOEXEC, 0, fds);
				var agent_ctrlfd = new FileDescriptor (fds[0]);
				agent_ctrlfd_for_peer = new FileDescriptor (fds[1]);

				UnixSocket.tune_buffer_sizes (agent_ctrlfd.handle);
				UnixSocket.tune_buffer_sizes (agent_ctrlfd_for_peer.handle);

				Socket socket;
				try {
					socket = new Socket.from_fd (agent_ctrlfd.handle);
				} catch (GLib.Error e) {
					assert_not_reached ();
				}
				agent_ctrlfd.steal ();
				_agent_ctrl = (UnixConnection) SocketConnection.factory_create_connection (socket);
			}
		}

		internal static async RemoteAgent start (LoaderLaunch launch, InjectSpec spec, uint pid, BootstrapResult bres,
				UnixConnection frida_ctrl, UnixConnection? agent_ctrl, Cancellable? cancellable) throws Error, IOError {
			var agent = new RemoteAgent (pid, spec, bres, frida_ctrl, agent_ctrl);

			try {
				var io_priority = Priority.DEFAULT;

				if (launch == FROM_SCRATCH)
					frida_ctrl.send_fd (spec.library_so.get_fd (), cancellable);

				if (agent.agent_ctrlfd_for_peer != null) {
					frida_ctrl.send_fd (agent.agent_ctrlfd_for_peer.handle, cancellable);
					agent.agent_ctrlfd_for_peer = null;
				} else {
					yield frida_ctrl.get_output_stream ().write_async ({ 0 }, io_priority, cancellable);
				}
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
				throw new Error.TRANSPORT ("%s", e.message);
			}

			agent.monitor.begin ();
			Future<bool> started = agent.start_request.future;
			yield started.wait_async (cancellable);
			return agent;
		}

		public void ack () {
			uint8 raw_type = HelperMessageType.ACK;
			frida_ctrl.get_output_stream ().write_all_async.begin ((uint8[]) &raw_type, Priority.DEFAULT, null);
		}

		private async void monitor () {
			Error? pending_start_error = null;
			try {
				var unload_policy = UnloadPolicy.IMMEDIATE;

				InputStream input = frida_ctrl.get_input_stream ();
				var io_priority = Priority.DEFAULT;
				size_t n;

				bool done = false;
				HelperHelloMessage? hello = null;
				HelperByeMessage? bye = null;
				do {
					uint8 raw_type = 0;
					yield input.read_all_async ((uint8[]) &raw_type, io_priority, io_cancellable, out n);
					if (n == 0)
						break;
					var type = (HelperMessageType) raw_type;

					switch (type) {
						case HELLO: {
							var m = HelperHelloMessage ();
							yield input.read_all_async ((uint8[]) &m, io_priority, io_cancellable, out n);
							if (n == 0)
								break;
							hello = m;

							break;
						}
						case READY: {
							if (start_request != null) {
								start_request.resolve (true);
								start_request = null;
							}
							break;
						}
						case BYE: {
							done = true;

							var m = HelperByeMessage ();
							yield input.read_all_async ((uint8[]) &m, io_priority, io_cancellable, out n);
							if (n == 0)
								break;
							bye = m;

							break;
						}
						case ERROR_DLOPEN:
						case ERROR_DLSYM: {
							uint16 length = 0;
							yield input.read_all_async ((uint8[]) &length, io_priority, io_cancellable, out n);
							if (n == 0)
								break;

							var data = new uint8[length + 1];
							yield input.read_all_async (data[:length], io_priority, io_cancellable, out n);
							if (n == 0)
								break;
							data[length] = 0;

							unowned string message = (string) data;

							pending_start_error = new Error.INVALID_ARGUMENT ("%s", message);

							break;
						}
						default:
							break;
					}
				} while (!done);

				if (bye != null)
					unload_policy = bye.unload_policy;

				if (hello != null) {
					string thread_path = "/proc/%u/task/%u".printf (pid, hello.thread_id);
					while (FileUtils.test (thread_path, EXISTS)) {
						var source = new TimeoutSource (50);
						source.set_callback (monitor.callback);
						source.attach (MainContext.get_thread_default ());
						yield;
					}
				}

				on_stop (unload_policy);
			} catch (GLib.Error e) {
				if (!(e is IOError.CANCELLED))
					on_stop (IMMEDIATE);
			} finally {
				if (start_request != null) {
					Error error = (pending_start_error != null)
						? pending_start_error
						: new Error.TRANSPORT ("Agent connection closed unexpectedly");
					start_request.reject (error);
					start_request = null;
				}
			}

			cancel_request.resolve (true);
		}

		public async void demonitor (Cancellable? cancellable) {
			io_cancellable.cancel ();

			try {
				yield cancel_request.future.wait_async (cancellable);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			_state = PAUSED;
			notify_property ("state");
		}

		public RemoteAgent clone (uint clone_id, AgentFeatures features) {
			var agent = new RemoteAgent (0, inject_spec.clone (clone_id, features), bootstrap_result.clone (), frida_ctrl);
			agent._state = _state;
			return agent;
		}

		public void stop () {
			on_stop (IMMEDIATE);
		}

		private void on_stop (UnloadPolicy unload_policy) {
			_unload_policy = unload_policy;
			_state = STOPPED;
			notify_property ("state");
		}
	}

	protected enum HelperBootstrapStatus {
		ALLOCATION_SUCCESS,
		ALLOCATION_ERROR,

		SUCCESS,
		AUXV_NOT_FOUND,
		TOO_EARLY,
		LIBC_LOAD_ERROR,
		LIBC_UNSUPPORTED,
	}

	protected struct HelperBootstrapContext {
		void * allocation_base;
		size_t allocation_size;

		size_t page_size;
		string * fallback_ld;
		string * fallback_libc;
		HelperRtldFlavor rtld_flavor;
		void * rtld_base;
		void * r_brk;
		bool enable_ctrlfds;
		int ctrlfds[2];
		HelperLibcApi * libc;
	}

	protected struct HelperLoaderContext {
		int ctrlfds[2]; // Must be first, as rejuvenate() assumes it.
		string * agent_entrypoint;
		string * agent_data;
		string * fallback_address;
		HelperLibcApi * libc;

		void * worker;
		void * agent_handle;
		void * agent_entrypoint_impl;
	}

	protected struct HelperLibcApi {
		void * printf;
		void * sprintf;

		void * mmap;
		void * munmap;
		void * socket;
		void * socketpair;
		void * connect;
		void * recvmsg;
		void * send;
		void * fcntl;
		void * close;

		void * pthread_create;
		void * pthread_detach;

		void * dlopen;
		int dlopen_flags;
		void * dlclose;
		void * dlsym;
		void * dlerror;
	}

	protected enum HelperMessageType {
		HELLO,
		READY,
		ACK,
		BYE,
		ERROR_DLOPEN,
		ERROR_DLSYM
	}

	protected struct HelperHelloMessage {
		uint thread_id;
	}

	protected struct HelperByeMessage {
		UnloadPolicy unload_policy;
	}

	protected enum HelperRtldFlavor {
		UNKNOWN,
		NONE,
		GLIBC,
		UCLIBC,
		MUSL,
		ANDROID,
	}

	protected enum HelperElfDynamicAddressState {
		PRISTINE,
		ADJUSTED
	}

	protected class SeizeSession : Object, AsyncInitable {
		public uint pid {
			get;
			construct;
		}

		public uint tid {
			get {
				return _tid;
			}
			construct {
				_tid = value;
			}
		}

		public InitBehavior on_init {
			get;
			construct;
			default = INTERRUPT;
		}

		public GPRegs saved_registers {
			get {
				return saved_regs;
			}
		}

		public bool was_group_stopped {
			get;
			private set;
		}

		public enum InitBehavior {
			INTERRUPT,
			CONTINUE
		}

		private AttachState attach_state = ALREADY_ATTACHED;
		private uint _tid;
		protected GPRegs saved_regs;

		private static bool seize_supported;
		private static bool regset_supported = true;
		protected static ProcessVmIoFunc? process_vm_readv;
		protected static ProcessVmIoFunc? process_vm_writev;

		[CCode (has_target = false)]
		protected delegate ssize_t ProcessVmIoFunc (uint pid,
			[CCode (array_length_type = "unsigned long")]
			Posix.iovector[] local_iov,
			[CCode (array_length_type = "unsigned long")]
			Posix.iovector[] remote_iov,
			ulong flags);

		static construct {
			seize_supported = check_kernel_version (3, 4);

			if (check_kernel_version (3, 2)) {
				process_vm_readv = process_vm_readv_impl;
				process_vm_writev = process_vm_writev_impl;
			}
		}

		public override void constructed () {
			if (_tid == 0)
				_tid = pid;
		}

		public override void dispose () {
			if (attach_state == ATTACHED)
				close_potentially_running.begin ();

			base.dispose ();
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			PtraceOptions options = PtraceOptions.TRACESYSGOOD | PtraceOptions.TRACEEXEC;

			bool was_stopped = seize_supported && query_process_state (tid) == 'T';

			PtraceRequest req;
			long res;
			if (seize_supported) {
				req = SEIZE;
				res = _ptrace (req, tid, null, (void *) options);
			} else {
				req = ATTACH;
				res = _ptrace (req, tid);
			}
			int errsv = errno;

			switch (on_init) {
				case INTERRUPT: {
					bool maybe_already_attached = res == -1 && errsv == Posix.EPERM;
					if (maybe_already_attached) {
						try {
							get_regs (&saved_regs);
						} catch (Error e) {
							throw new Error.PERMISSION_DENIED ("Process is traced by another process");
						}

						attach_state = ALREADY_ATTACHED;
					} else {
						if (res == -1)
							throw_ptrace_error (req, pid, errsv);

						attach_state = ATTACHED;

						if (seize_supported) {
							if (was_stopped) {
								was_group_stopped = true;
								yield wait_for_next_stop (cancellable);
								Posix.kill ((Posix.pid_t) pid, Posix.Signal.CONT);
							} else {
								ptrace (INTERRUPT, tid);
								yield wait_for_signal (TRAP, cancellable);
							}
						} else {
							yield wait_for_signal (STOP, cancellable);
							ptrace (SETOPTIONS, tid, null, (void *) options);
						}

						get_regs (&saved_regs);
					}

					break;
				}
				case CONTINUE:
					if (res == -1)
						throw_ptrace_error (req, pid, errsv);

					attach_state = ATTACHED;

					if (!seize_supported) {
						yield wait_for_signal (STOP, cancellable);
						ptrace (SETOPTIONS, tid, null, (void *) options);
						ptrace (CONT, tid);
					}

					break;
			}

			return true;
		}

		private static char query_process_state (uint tid) {
			try {
				string stat;
				FileUtils.get_contents ("/proc/%u/stat".printf (tid), out stat);
				int paren_end = stat.last_index_of_char (')');
				if (paren_end != -1 && paren_end + 2 < stat.length)
					return stat[paren_end + 2];
			} catch (FileError e) {
			}
			return '?';
		}

		public void close () throws Error {
			if (attach_state == ATTACHED) {
				ptrace (DETACH, tid);
				attach_state = ALREADY_ATTACHED;
			}
		}

		private async void close_potentially_running () {
			try {
				close ();
				return;
			} catch (Error e) {
			}

			try {
				yield suspend (null);
				close ();
			} catch (Error e) {
				// If the process is gone, then there's no point in retrying.
				if (e is Error.PROCESS_NOT_FOUND)
					attach_state = ALREADY_ATTACHED;
			} catch (GLib.Error e) {
			}
		}

		public async void suspend (Cancellable? cancellable) throws Error, IOError {
			if (seize_supported) {
				ptrace (INTERRUPT, tid);
				yield wait_for_signal (TRAP, cancellable);
			} else {
				tgkill (pid, tid, STOP);
				yield wait_for_signal (STOP, cancellable);
			}
		}

		public void resume () throws Error {
			ptrace (CONT, tid);
		}

		public void step () throws Error {
			ptrace (SINGLESTEP, tid);
		}

		public async void resume_until_execution_reaches (uint64 target, Cancellable? cancellable) throws Error, IOError {
			uint64 target_address = target;

			unowned uint8[] breakpoint_data;
#if X86 || X86_64
			uint8 breakpoint_val = 0xcc;
			breakpoint_data = (uint8[]) &breakpoint_val;
#elif ARM
			target_address &= ~1;

			uint32 arm_breakpoint_val = (0xe7f001f0U).to_little_endian ();
			uint16 thumb_breakpoint_val = ((uint16) 0xde01).to_little_endian ();
			bool is_thumb = (target & 1) != 0;
			if (is_thumb)
				breakpoint_data = (uint8[]) &thumb_breakpoint_val;
			else
				breakpoint_data = (uint8[]) &arm_breakpoint_val;
#elif ARM64
			uint32 breakpoint_val = (0xd4200000U).to_little_endian ();
			breakpoint_data = (uint8[]) &breakpoint_val;
#elif MIPS
			uint32 breakpoint_val = 0x0000000dU;
			breakpoint_data = (uint8[]) &breakpoint_val;
#endif

			if (saved_regs.program_counter == target) {
				step ();
				yield wait_for_signal (TRAP, cancellable);
				get_regs (&saved_regs);
			}

			uint8[] original_code = read_memory (target_address, breakpoint_data.length);
			write_memory (target_address, breakpoint_data);

			bool restored = false;
			try {
				resume ();
				yield wait_for_signal (TRAP, cancellable);

				restored = true;
				get_regs (&saved_regs);
				write_memory (target_address, original_code);

				bool hit_breakpoint = saved_regs.program_counter == target_address ||
					saved_regs.program_counter == target_address + breakpoint_data.length;
				if (!hit_breakpoint)
					throw new Error.NOT_SUPPORTED ("Unable to reach breakpoint (got unknown trap)");

				saved_regs.program_counter = target_address;
				set_regs (saved_regs);
			} finally {
				if (!restored) {
					try {
						get_regs (&saved_regs);
						write_memory (target_address, original_code);
					} catch (Error e) {
					}
				}
			}
		}

		public async void wait_for_signal (Posix.Signal sig, Cancellable? cancellable) throws Error, IOError {
			yield ChildProcess.wait_for_signal (tid, sig, cancellable);
		}

		public async Posix.Signal wait_for_signals (Posix.Signal[] sigs, Cancellable? cancellable) throws Error, IOError {
			return yield ChildProcess.wait_for_signals (tid, sigs, cancellable);
		}

		public async Posix.Signal wait_for_next_signal (Cancellable? cancellable) throws Error, IOError {
			return yield ChildProcess.wait_for_next_signal (tid, cancellable);
		}

		public async WaitResult wait_for_next_stop (Cancellable? cancellable) throws Error, IOError {
			return yield ChildProcess.wait_for_next_stop (tid, cancellable);
		}

		public void get_regs (GPRegs * regs) throws Error {
#if !MIPS
			if (regset_supported) {
				var io = Posix.iovector ();
				io.iov_base = regs;
				io.iov_len = sizeof (GPRegs);
				long res = _ptrace (GETREGSET, tid, (void *) NT_PRSTATUS, &io);
				if (res == 0)
					return;
				if (errno == Posix.EPERM || errno == Posix.ESRCH)
					throw_ptrace_error (GETREGSET, pid, errno);
				regset_supported = false;
			}
#endif

			ptrace (GETREGS, tid, null, regs);
		}

		public void get_fpregs (FPRegs * regs) throws Error {
			if (regset_supported) {
				var io = Posix.iovector ();
				io.iov_base = regs;
				io.iov_len = sizeof (FPRegs);
				long res = _ptrace (GETREGSET, tid, (void *) NT_PRFPREG, &io);
				if (res == 0)
					return;
				if (errno == Posix.EPERM || errno == Posix.ESRCH)
					throw_ptrace_error (GETREGSET, pid, errno);
				regset_supported = false;
			}

			ptrace (GETFPREGS, tid, null, regs);
		}

		public void set_regs (GPRegs regs) throws Error {
#if !MIPS
			if (regset_supported) {
				var io = Posix.iovector ();
				io.iov_base = &regs;
				io.iov_len = sizeof (GPRegs);
				long res = _ptrace (SETREGSET, tid, (void *) NT_PRSTATUS, &io);
				if (res == 0)
					return;
				if (errno == Posix.EPERM || errno == Posix.ESRCH)
					throw_ptrace_error (SETREGSET, pid, errno);
				regset_supported = false;
			}
#endif

			ptrace (SETREGS, tid, null, &regs);
		}

		public void set_fpregs (FPRegs regs) throws Error {
			if (regset_supported) {
				var io = Posix.iovector ();
				io.iov_base = &regs;
				io.iov_len = sizeof (FPRegs);
				long res = _ptrace (SETREGSET, tid, (void *) NT_PRFPREG, &io);
				if (res == 0)
					return;
				if (errno == Posix.EPERM || errno == Posix.ESRCH)
					throw_ptrace_error (SETREGSET, pid, errno);
				regset_supported = false;
			}

			ptrace (SETFPREGS, tid, null, &regs);
		}

		public async uint64 allocate_memory (uint64 mmap_impl, size_t size, int prot, Cancellable? cancellable)
				throws Error, IOError {
			var builder = new RemoteCallBuilder (mmap_impl, saved_regs);
			builder
				.add_argument (0)
				.add_argument (size)
				.add_argument (prot)
				.add_argument (Posix.MAP_PRIVATE | MAP_ANONYMOUS)
				.add_argument (~0)
				.add_argument (0);
			RemoteCall call = builder.build (this);

			RemoteCallResult res = yield call.execute (cancellable);
			if (res.status != COMPLETED)
				throw new Error.NOT_SUPPORTED ("Unexpected crash while trying to allocate memory");
			if (res.return_value == MAP_FAILED)
				throw new Error.NOT_SUPPORTED ("Unexpected failure while trying to allocate memory");
			return res.return_value;
		}

		public async void deallocate_memory (uint64 munmap_impl, uint64 address, size_t size, Cancellable? cancellable)
				throws Error, IOError {
			var builder = new RemoteCallBuilder (munmap_impl, saved_regs);
			builder
				.add_argument (address)
				.add_argument (size);
			RemoteCall call = builder.build (this);

			RemoteCallResult res = yield call.execute (cancellable);
			if (res.status != COMPLETED)
				throw new Error.NOT_SUPPORTED ("Unexpected crash while trying to deallocate memory");
			if (res.return_value != 0)
				throw new Error.NOT_SUPPORTED ("Unexpected failure while trying to deallocate memory");
		}

		public uint8[] read_memory (uint64 address, size_t size) throws Error {
			if (size == 0)
				return {};

			var result = new uint8[size];

			if (process_vm_readv != null) {
				var local = Posix.iovector ();
				local.iov_base = result;
				local.iov_len = result.length;

				var remote = Posix.iovector ();
				remote.iov_base = (void *) address;
				remote.iov_len = size;

				ssize_t res = process_vm_readv (pid, (Posix.iovector[]) &local, (Posix.iovector[]) &remote, 0);
				if (res != -1)
					return result;
				if (errno == Posix.ENOSYS)
					process_vm_readv = null;
				else if (errno != Posix.EPERM && errno != Posix.EFAULT)
					throw new Error.NOT_SUPPORTED ("Unable to read from process memory: %s", strerror (errno));
			}

			size_t offset = 0;
			uint bytes_per_word = (uint) sizeof (size_t);
			while (offset != size) {
				size_t word = (size_t) ptrace (PEEKDATA, tid, (void *) (address + offset));
				size_t chunk_size = size_t.min (size - offset, bytes_per_word);
				Memory.copy ((uint8 *) result + offset, &word, chunk_size);

				offset += chunk_size;
			}

			return result;
		}

		public void write_memory (uint64 address, uint8[] data) throws Error {
			if (data.length == 0)
				return;

#if X86 || X86_64
			if (process_vm_writev != null) {
				var local = Posix.iovector ();
				local.iov_base = data;
				local.iov_len = data.length;

				var remote = Posix.iovector ();
				remote.iov_base = (void *) address;
				remote.iov_len = data.length;

				ssize_t res = process_vm_writev (pid, (Posix.iovector[]) &local, (Posix.iovector[]) &remote, 0);
				if (res != -1)
					return;
				if (errno == Posix.ENOSYS)
					process_vm_writev = null;
				else if (errno != Posix.EPERM && errno != Posix.EFAULT)
					throw new Error.NOT_SUPPORTED ("Unable to write to process memory: %s", strerror (errno));
			}
#endif

			size_t offset = 0;
			size_t size = data.length;
			uint bytes_per_word = (uint) sizeof (size_t);
			while (offset != size) {
				size_t word = 0;
				size_t chunk_size = size_t.min (size - offset, bytes_per_word);
				if (chunk_size < bytes_per_word)
					word = (size_t) ptrace (PEEKDATA, tid, (void *) (address + offset));
				Memory.copy (&word, (uint8 *) data + offset, chunk_size);

				ptrace (POKEDATA, tid, (void *) (address + offset), (void *) word);

				offset += chunk_size;
			}
		}

		public void write_memory_string (uint64 address, string str) throws Error {
			unowned uint8[] data = str.data;
			write_memory (address, data[:data.length + 1]);
		}

		private static ssize_t process_vm_readv_impl (uint pid,
				[CCode (array_length_type = "unsigned long")]
				Posix.iovector[] local_iov,
				[CCode (array_length_type = "unsigned long")]
				Posix.iovector[] remote_iov,
				ulong flags) {
			return Linux.syscall (LinuxSyscall.PROCESS_VM_READV, pid, local_iov, local_iov.length, remote_iov,
				remote_iov.length, flags);
		}

		private static ssize_t process_vm_writev_impl (uint pid,
				[CCode (array_length_type = "unsigned long")]
				Posix.iovector[] local_iov,
				[CCode (array_length_type = "unsigned long")]
				Posix.iovector[] remote_iov,
				ulong flags) {
			return Linux.syscall (LinuxSyscall.PROCESS_VM_WRITEV, pid, local_iov, local_iov.length, remote_iov,
				remote_iov.length, flags);
		}
	}

	protected enum AttachState {
		ATTACHED,
		ALREADY_ATTACHED,
	}

	private sealed class ProcessCodeSwapScope {
		private State state = INACTIVE;

		private SeizeSession session;
		private ThreadSuspendScope thread_suspend_scope;
		public uint64 code_start;
		public uint64 code_end;
		private uint8[] original_code;

		private enum State {
			INACTIVE,
			ACTIVE
		}

		public async ProcessCodeSwapScope (SeizeSession session, uint8[] code, Cancellable? cancellable) throws Error, IOError {
			this.session = session;

			Gum.Linux.enumerate_ranges ((Posix.pid_t) session.pid, READ | EXECUTE, d => {
				unowned Gum.FileMapping? file = d.file;
				if (file != null && file.path.has_prefix ("memfd:"))
					return true;
				if (d.range.size >= code.length) {
					code_start = d.range.base_address + d.range.size - round_size_to_page_size (code.length);
					code_end = code_start + code.length;
				}
				return code_start == 0;
			});
			if (code_start == 0)
				throw new Error.NOT_SUPPORTED ("Unable to find suitable code pages");

			thread_suspend_scope = new ThreadSuspendScope (session.pid);
			thread_suspend_scope.exclude (session.tid);
			yield thread_suspend_scope.enable (cancellable);

			original_code = session.read_memory (code_start, code.length);
			session.write_memory (code_start, code);
			state = ACTIVE;
		}

		~ProcessCodeSwapScope () {
			try {
				revert ();
			} catch (Error e) {
			}
		}

		public void revert () throws Error {
			if (state == ACTIVE) {
				session.write_memory (code_start, original_code);

				thread_suspend_scope.disable ();

				state = INACTIVE;
			}
		}
	}

	private sealed class ThreadSuspendScope {
		private State state = INACTIVE;

		private uint pid;
		private Gee.Set<uint> excluded_tids = new Gee.HashSet<uint> ();
		private Gee.List<SeizeSession> suspended = new Gee.ArrayList<SeizeSession> ();

		private enum State {
			INACTIVE,
			ACTIVE
		}

		private delegate void CompletionNotify ();

		public ThreadSuspendScope (uint pid) throws Error {
			this.pid = pid;
		}

		public void exclude (uint tid) {
			assert (state == INACTIVE);
			excluded_tids.add (tid);
		}

		public async void enable (Cancellable? cancellable) throws Error, IOError {
			assert (state == INACTIVE);
			state = ACTIVE;

			uint pending = 1;

			CompletionNotify on_complete = () => {
				pending--;
				if (pending == 0) {
					var source = new IdleSource ();
					source.set_callback (enable.callback);
					source.attach (MainContext.get_thread_default ());
				}
			};

			var discovered_tids = new Gee.HashSet<uint> ();
			uint new_discoveries = 0;
			Error? pending_error = null;
			do {
				Dir dir;
				try {
					dir = Dir.open ("/proc/%u/task".printf (pid));
				} catch (FileError e) {
					pending_error = new Error.PROCESS_NOT_FOUND ("Process exited unexpectedly");
					break;
				}

				new_discoveries = 0;
				string? name;
				while ((name = dir.read_name ()) != null) {
					var tid = uint.parse (name);

					if (excluded_tids.contains (tid))
						continue;

					if (!discovered_tids.contains (tid)) {
						discovered_tids.add (tid);
						new_discoveries++;

						pending++;
						suspend_thread.begin (tid, cancellable, on_complete);
					}
				}
			} while (new_discoveries > 0);

			on_complete ();

			yield;

			on_complete = null;

			if (pending_error != null)
				throw pending_error;
		}

		private async void suspend_thread (uint tid, Cancellable? cancellable, CompletionNotify on_complete) {
			try {
				var session = yield ThreadSuspendSession.open (pid, tid, cancellable);
				suspended.add (session);
			} catch (GLib.Error e) {
			}

			on_complete ();
		}

		public void disable () throws Error {
			assert (state == ACTIVE);
			state = INACTIVE;

			foreach (SeizeSession session in suspended)
				session.close ();
			suspended.clear ();
		}
	}

	private sealed class RemoteCallBuilder {
		private uint64 target;
		private uint64[] args = {};
		private GPRegs regs;

		public RemoteCallBuilder (uint64 target, GPRegs regs) {
			this.target = target;
			this.regs = regs;

			this.regs.orig_syscall = -1;
			uint64 new_sp;
			this
				.reserve_stack_space (RED_ZONE_SIZE, out new_sp)
				.align_stack ();
		}

		public RemoteCallBuilder add_argument (uint64 val) {
			args += val;
			assert (args.length <= 6);

			return this;
		}

		public RemoteCallBuilder reserve_stack_space (size_t size, out uint64 location) {
			size_t allocated_size;
			if (size % STACK_ALIGNMENT != 0)
				allocated_size = size + (STACK_ALIGNMENT - (size % STACK_ALIGNMENT));
			else
				allocated_size = size;

			uint64 new_sp = regs.stack_pointer - allocated_size;
			regs.stack_pointer = new_sp;

			location = new_sp;

			return this;
		}

		private RemoteCallBuilder align_stack () {
			uint64 sp = regs.stack_pointer;
			sp -= sp % STACK_ALIGNMENT;
			regs.stack_pointer = sp;

			return this;
		}

		public RemoteCall build (SeizeSession session) {
			return new RemoteCall (session, target, args, regs);
		}
	}

	private sealed class RemoteCall {
		private SeizeSession session;
		private uint64 target;
		private uint64[] args;
		private GPRegs initial_regs;

		internal RemoteCall (SeizeSession session, uint64 target, uint64[] args, GPRegs regs) {
			this.session = session;
			this.target = target;
			this.args = args;
			this.initial_regs = regs;
		}

		public async RemoteCallResult execute (Cancellable? cancellable) throws Error, IOError {
			GPRegs regs = initial_regs;

			uint64 target_address = target;

#if X86
			if (args.length > 0) {
				uint32[] slots = {};
				foreach (uint64 arg in args)
					slots += (uint32) arg;

				unowned uint8[] raw_slots = (uint8[]) slots;
				raw_slots.length = slots.length * 4;

				regs.esp -= (uint32) ((regs.esp - (args.length * 4)) % STACK_ALIGNMENT);
				regs.esp -= raw_slots.length;
				session.write_memory (regs.esp, raw_slots);
			}

			regs.esp -= 4;
			uint32 return_address = (uint32) DUMMY_RETURN_ADDRESS;
			session.write_memory (regs.esp, (uint8[]) &return_address);
#elif X86_64
			uint i = 0;
			foreach (uint64 arg in args) {
				switch (i) {
					case 0:
						regs.rdi = arg;
						break;
					case 1:
						regs.rsi = arg;
						break;
					case 2:
						regs.rdx = arg;
						break;
					case 3:
						regs.rcx = arg;
						break;
					case 4:
						regs.r8 = arg;
						break;
					case 5:
						regs.r9 = arg;
						break;
					default:
						assert_not_reached ();
				}
				i++;
			}

			regs.rsp -= 8;
			uint64 return_address = DUMMY_RETURN_ADDRESS;
			session.write_memory (regs.rsp, (uint8[]) &return_address);
#elif ARM
			uint i = 0;
			foreach (uint64 arg in args) {
				regs.r[i++] = (uint32) arg;
				if (i == 4)
					break;
			}

			if (args.length > 4) {
				uint32[] slots = {};
				while (i < args.length)
					slots += (uint32) args[i++];

				unowned uint8[] raw_slots = (uint8[]) slots;
				raw_slots.length = slots.length * 4;

				regs.sp -= (uint32) ((regs.sp - ((args.length - 4) * 4)) % STACK_ALIGNMENT);
				regs.sp -= raw_slots.length;
				session.write_memory (regs.sp, raw_slots);
			}

			regs.lr = (uint32) DUMMY_RETURN_ADDRESS;

			if ((target_address & 1) != 0) {
				target_address &= ~1;
				regs.cpsr |= PSR_T_BIT;
			} else {
				regs.cpsr &= ~PSR_T_BIT;
			}
#elif ARM64
			uint i = 0;
			foreach (uint64 arg in args)
				regs.x[i++] = arg;

			regs.lr = DUMMY_RETURN_ADDRESS;
#elif MIPS
			regs.t9 = (size_t) target_address;

			uint i = 0;
			foreach (uint64 arg in args) {
				regs.a[i++] = (size_t) arg;
				if (i == 4)
					break;
			}

			if (args.length > 4) {
				uint32[] slots = {};
				while (i < args.length)
					slots += (uint32) args[i++];

				unowned uint8[] raw_slots = (uint8[]) slots;
				raw_slots.length = (int) (slots.length * sizeof (size_t));

				regs.sp -= (uint32) ((regs.sp - ((args.length - 4) * sizeof (size_t))) % STACK_ALIGNMENT);
				regs.sp -= raw_slots.length;
				session.write_memory (regs.sp, raw_slots);
			}

			/*
			 * We need to reserve space for 'incoming arguments', as per
			 * http://math-atlas.sourceforge.net/devel/assembly/mipsabi32.pdf section 3-15
			 */
			regs.sp -= 4 * sizeof (size_t);

			regs.ra = DUMMY_RETURN_ADDRESS;
#endif

			regs.program_counter = target_address;

			int sig = -1;
			var result_regs = GPRegs ();
			session.set_regs (regs);
			bool restored = false;
			try {
				session.resume ();
				sig = yield session.wait_for_signals ({ SEGV, STOP }, cancellable);

				session.get_regs (&result_regs);

				restored = true;
				session.set_regs (session.saved_registers);
			} finally {
				if (!restored) {
					try {
						session.set_regs (session.saved_registers);
					} catch (Error e) {
					}
				}
			}

			RemoteCallStatus status = (result_regs.program_counter == DUMMY_RETURN_ADDRESS)
				? RemoteCallStatus.COMPLETED
				: RemoteCallStatus.RAISED_SIGNAL;
			int stop_signal = (status == COMPLETED) ? -1 : sig;

			uint64 return_value;
			if (status == COMPLETED) {
#if X86
				return_value = result_regs.eax;
#elif X86_64
				return_value = result_regs.rax;
#elif ARM
				return_value = result_regs.r[0];
#elif ARM64
				return_value = result_regs.x[0];
#elif MIPS
				return_value = result_regs.v[0];
#endif
			} else {
				return_value = ~0;
			}

			return new RemoteCallResult (status, stop_signal, return_value, result_regs);
		}
	}

	private sealed class RemoteCallResult {
		public RemoteCallStatus status;
		public int stop_signal;
		public uint64 return_value;
		public GPRegs regs;

		public RemoteCallResult (RemoteCallStatus status, int stop_signal, uint64 return_value, GPRegs regs) {
			this.status = status;
			this.stop_signal = stop_signal;
			this.return_value = return_value;
			this.regs = regs;
		}
	}

	private enum RemoteCallStatus {
		COMPLETED,
		RAISED_SIGNAL,
	}

	private enum PtraceRequest {
		TRACEME			= 0x0000,
		PEEKDATA		= 0x0002,
		POKEDATA		= 0x0005,
		CONT			= 0x0007,
		SINGLESTEP		= 0x0009,
		ATTACH			= 0x0010,
		SYSCALL			= 0x0018,
		GETREGS			= 0x000c,
		SETREGS			= 0x000d,
		GETFPREGS		= 0x000e,
		SETFPREGS		= 0x000f,
		DETACH			= 0x0011,
		SETOPTIONS		= 0x4200,
		GETREGSET		= 0x4204,
		SETREGSET		= 0x4205,
		SEIZE			= 0x4206,
		INTERRUPT		= 0x4207,
	}

	[Flags]
	private enum PtraceOptions {
		TRACESYSGOOD	= (1 << 0),
		TRACEEXEC	= (1 << 4),
	}

	private const uint NT_PRSTATUS = 1;
	private const uint NT_PRFPREG = 2;

	private const uint32 PSR_T_BIT = 0x20;

	private const size_t RED_ZONE_SIZE = 128;
	private const size_t STACK_ALIGNMENT = 16;

	protected struct GPRegs {
#if X86
		uint32 ebx;
		uint32 ecx;
		uint32 edx;
		uint32 esi;
		uint32 edi;
		uint32 ebp;
		uint32 eax;
		uint32 xds;
		uint32 xes;
		uint32 xfs;
		uint32 xgs;
		int32 orig_eax;
		uint32 eip;
		uint32 xcs;
		uint32 eflags;
		uint32 esp;
		uint32 xss;

		public uint64 program_counter {
			get { return eip; }
			set { eip = (uint32) value; }
		}

		public uint64 stack_pointer {
			get { return esp; }
			set { esp = (uint32) value; }
		}

		public int orig_syscall {
			get { return orig_eax; }
			set { orig_eax = value; }
		}

		public string to_string () {
			var builder = new StringBuilder ();

			append_register_value (builder, "eip", eip);
			append_register_value (builder, "esp", esp);
			append_register_value (builder, "ebp", ebp);

			append_register_value (builder, "eax", eax);
			append_register_value (builder, "ecx", ecx);
			append_register_value (builder, "edx", edx);
			append_register_value (builder, "ebx", ebx);
			append_register_value (builder, "esi", esi);
			append_register_value (builder, "edi", edi);

			return builder.str;
		}
#elif X86_64
		uint64 r15;
		uint64 r14;
		uint64 r13;
		uint64 r12;
		uint64 rbp;
		uint64 rbx;
		uint64 r11;
		uint64 r10;
		uint64 r9;
		uint64 r8;
		uint64 rax;
		uint64 rcx;
		uint64 rdx;
		uint64 rsi;
		uint64 rdi;
		int64 orig_rax;
		uint64 rip;
		uint64 cs;
		uint64 eflags;
		uint64 rsp;
		uint64 ss;
		uint64 fs_base;
		uint64 gs_base;
		uint64 ds;
		uint64 es;
		uint64 fs;
		uint64 gs;

		public uint64 program_counter {
			get { return rip; }
			set { rip = value; }
		}

		public uint64 stack_pointer {
			get { return rsp; }
			set { rsp = value; }
		}

		public int orig_syscall {
			get { return (int) orig_rax; }
			set { orig_rax = value; }
		}

		public string to_string () {
			var builder = new StringBuilder ();

			append_register_value (builder, "rip", rip);
			append_register_value (builder, "rsp", rsp);
			append_register_value (builder, "rbp", rbp);

			append_register_value (builder, "rax", rax);
			append_register_value (builder, "rcx", rcx);
			append_register_value (builder, "rdx", rdx);
			append_register_value (builder, "rbx", rbx);
			append_register_value (builder, "rsi", rsi);
			append_register_value (builder, "rdi", rdi);

			append_register_value (builder, "r8", r8);
			append_register_value (builder, "r9", r9);
			append_register_value (builder, "r10", r10);
			append_register_value (builder, "r11", r11);
			append_register_value (builder, "r12", r12);
			append_register_value (builder, "r13", r13);
			append_register_value (builder, "r14", r14);
			append_register_value (builder, "r15", r15);

			return builder.str;
		}
#elif ARM
		uint32 r[11];
		uint32 fp;
		uint32 ip;
		uint32 sp;
		uint32 lr;
		uint32 pc;
		uint32 cpsr;
		int32 orig_r0;

		public uint64 program_counter {
			get { return pc; }
			set { pc = (uint32) value; }
		}

		public uint64 stack_pointer {
			get { return sp; }
			set { sp = (uint32) value; }
		}

		public int orig_syscall {
			get { return orig_r0; }
			set { orig_r0 = value; }
		}

		public string to_string () {
			var builder = new StringBuilder ();

			append_register_value (builder, "pc", pc);
			append_register_value (builder, "lr", lr);
			append_register_value (builder, "sp", sp);

			for (uint i = 0; i != r.length; i++)
				append_register_value (builder, "r%u".printf (i), r[i]);

			return builder.str;
		}
#elif ARM64
		uint64 x[30];
		uint64 lr;
		uint64 sp;
		uint64 pc;
		uint64 pstate;

		public uint64 get_pc () {
			return pc;
		}

		public uint64 program_counter {
			get { return pc; }
			set { pc = value; }
		}

		public uint64 stack_pointer {
			get { return sp; }
			set { sp = value; }
		}

		public int orig_syscall {
			get { return -1; }
			set {}
		}

		public string to_string () {
			var builder = new StringBuilder ();

			append_register_value (builder, "pc", pc);
			append_register_value (builder, "lr", lr);
			append_register_value (builder, "sp", sp);

			for (uint i = 0; i != x.length; i++)
				append_register_value (builder, "x%u".printf (i), x[i]);

			return builder.str;
		}
#elif MIPS
		uint64 zero;

		uint64 at;

		uint64 v[2];
		uint64 a[4];
		uint64 t[8];
		uint64 s[8];
		uint64 t8;
		uint64 t9;
		uint64 k[2];

		uint64 gp;
		uint64 sp;
		uint64 fp;
		uint64 ra;

		uint64 lo;
		uint64 hi;

		uint64 pc;
		uint64 badvaddr;
		uint64 status;
		uint64 cause;

		uint64 padding[8];

		public uint64 program_counter {
			get { return pc; }
			set { pc = value; }
		}

		public uint64 stack_pointer {
			get { return sp; }
			set { sp = value; }
		}

		public int orig_syscall {
			get { return (int) v[0]; }
			set { v[0] = value; }
		}

		public string to_string () {
			var builder = new StringBuilder ();

			append_register_value (builder, "pc", pc);
			append_register_value (builder, "ra", ra);
			append_register_value (builder, "sp", sp);
			append_register_value (builder, "fp", fp);

			append_register_value (builder, "at", at);
			append_register_value (builder, "gp", gp);

			for (uint i = 0; i != v.length; i++)
				append_register_value (builder, "v%u".printf (i), v[i]);
			for (uint i = 0; i != a.length; i++)
				append_register_value (builder, "a%u".printf (i), a[i]);
			for (uint i = 0; i != t.length; i++)
				append_register_value (builder, "t%u".printf (i), t[i]);
			for (uint i = 0; i != s.length; i++)
				append_register_value (builder, "s%u".printf (i), s[i]);
			append_register_value (builder, "t8", t8);
			append_register_value (builder, "t9", t9);
			for (uint i = 0; i != k.length; i++)
				append_register_value (builder, "k%u".printf (i), k[i]);

			return builder.str;
		}
#endif
	}

	protected struct FPRegs {
#if X86
		long cwd;
		long swd;
		long twd;
		long fip;
		long fcs;
		long foo;
		long fos;
		long st_space[20];
#elif X86_64
		uint16 cwd;
		uint16 swd;
		uint16 ftw;
		uint16 fop;
		uint64 rip;
		uint64 rdp;
		uint mxcsr;
		uint mxcr_mask;
		uint st_space[32];
		uint xmm_space[64];
		uint padding[24];
#elif ARM
		uint8 fpregs[8 * 12];
		uint fpsr;
		uint fpcr;
		uint8 ftype[8];
		uint init_flag;
#elif ARM64
		uint8 vregs[32 * 16];
		uint32 fpsr;
		uint32 fpcr;
		uint64 padding;
#elif MIPS
		ulong fpregs[64];
#endif
	}

	private void append_register_value (StringBuilder builder, string name, uint64 val) {
		if (builder.len != 0)
			builder.append_c ('\n');
		builder.append_c ('\t');
		builder.append_printf ("%3s: %" + ((sizeof (void *) == 8) ? "016" : "08") + uint64.FORMAT_MODIFIER + "x", name, val);
	}

	[CCode (cname = "execve", cheader_filename = "unistd.h")]
	private extern int execve (string pathname,
		[CCode (array_length = false, array_null_terminated = true)]
		string[] argv,
		[CCode (array_length = false, array_null_terminated = true)]
		string[] envp);

	private long ptrace (PtraceRequest request, uint pid = 0, void * addr = null, void * data = null) throws Error {
		errno = 0;
		long res = _ptrace (request, pid, addr, data);
		if (errno != 0)
			throw_ptrace_error (request, pid, errno);
		return res;
	}

	[CCode (cname = "ptrace", cheader_filename = "sys/ptrace.h")]
	private extern long _ptrace (PtraceRequest request, uint pid = 0, void * addr = null, void * data = null);

	[NoReturn]
	private void throw_ptrace_error (PtraceRequest request, uint pid, int err) throws Error {
		switch (err) {
			case Posix.ESRCH:
				throw new Error.PROCESS_NOT_FOUND ("Process not found");
			case Posix.EPERM:
				throw new Error.PERMISSION_DENIED (
					"Unable to access process with pid %u due to system restrictions;" +
					" try `sudo sysctl kernel.yama.ptrace_scope=0`, or run Frida as root",
					pid);
			default:
				throw new Error.NOT_SUPPORTED ("Unable to perform ptrace %s: %s",
					Marshal.enum_to_nick<PtraceRequest> (request),
					strerror (err));
		}
	}

	namespace ChildProcess {
		private async void wait_for_early_signal (uint pid, Posix.Signal sig, Cancellable? cancellable) throws Error, IOError {
			yield wait_for_signals_common (pid, { sig }, suppress_unmatched, cancellable);
		}

		private async void wait_for_signal (uint pid, Posix.Signal sig, Cancellable? cancellable) throws Error, IOError {
			yield wait_for_signals (pid, { sig }, cancellable);
		}

		private async Posix.Signal wait_for_signals (uint pid, Posix.Signal[] sigs, Cancellable? cancellable)
				throws Error, IOError {
			return yield wait_for_signals_common (pid, sigs, pass_through_unmatched, cancellable);
		}

		private async Posix.Signal wait_for_signals_common (uint pid, Posix.Signal[] sigs, UnmatchedStopHandler on_unmatched,
				Cancellable? cancellable) throws Error, IOError {
			while (true) {
				var wr = yield wait_for_next_stop (pid, cancellable);
				wr.check_stopped ();

				if (wr.stop_signal in sigs)
					return wr.stop_signal;

				if (!wr.should_deliver_to_tracee) {
					ptrace (CONT, pid);
					continue;
				}

				on_unmatched (pid, wr);
			}
		}

		private delegate void UnmatchedStopHandler (uint pid, WaitResult wr) throws Error;

		private static void pass_through_unmatched (uint pid, WaitResult wr) throws Error {
			ptrace (CONT, pid, null, (void *) wr.stop_signal);
		}

		private static void suppress_unmatched (uint pid, WaitResult wr) throws Error {
			ptrace (CONT, pid);
		}

		private async Posix.Signal wait_for_next_signal (uint pid, Cancellable? cancellable) throws Error, IOError {
			var wr = yield wait_for_next_stop (pid, cancellable);
			wr.check_stopped ();
			return wr.stop_signal;
		}

		private async WaitResult wait_for_next_stop (uint pid, Cancellable? cancellable) throws Error, IOError {
			var main_context = MainContext.get_thread_default ();

			bool timed_out = false;
			var timeout_source = new TimeoutSource.seconds (5);
			timeout_source.set_callback (() => {
				timed_out = true;
				return Source.REMOVE;
			});
			timeout_source.attach (main_context);

			int status = 0;
			uint[] delays = { 0, 1, 2, 5, 10, 20, 50, 250 };

			try {
				for (uint i = 0; !timed_out && !cancellable.set_error_if_cancelled (); i++) {
					int res = Posix.waitpid ((Posix.pid_t) pid, out status, Posix.WNOHANG);
					if (res == -1)
						throw new Error.NOT_SUPPORTED ("Unable to wait for next stop: %s", strerror (errno));
					if (res != 0)
						break;

					uint delay_ms = (i < delays.length) ? delays[i] : delays[delays.length - 1];

					var delay_source = new TimeoutSource (delay_ms);
					delay_source.set_callback (wait_for_next_stop.callback);
					delay_source.attach (main_context);

					var cancel_source = new CancellableSource (cancellable);
					cancel_source.set_callback (wait_for_next_stop.callback);
					cancel_source.attach (main_context);

					yield;

					cancel_source.destroy ();
					delay_source.destroy ();
				}
			} finally {
				timeout_source.destroy ();
			}

			if (timed_out)
				throw new Error.TIMED_OUT ("Unexpectedly timed out while waiting for stop from process with PID %u", pid);

			return WaitResult (pid, status);
		}
	}

	public enum WaitKind {
		EXITED,
		SIGNALED,
		STOPPED,
		OTHER
	}

	public struct WaitResult {
		public uint pid;
		public int status;

		public WaitKind kind;

		public uint exit_status;

		public Posix.Signal term_signal;

		public Posix.Signal stop_signal;
		public PtraceEvent ptrace_event;
		public bool is_ptrace_event_stop;
		public bool is_syscall_stop;

		public bool should_deliver_to_tracee;

		public WaitResult (uint pid, int status) {
			this.pid = pid;
			this.status = status;

			kind = WaitKind.OTHER;
			exit_status = 0;
			term_signal = 0;
			stop_signal = 0;
			ptrace_event = PtraceEvent.NONE;
			is_ptrace_event_stop = false;
			is_syscall_stop = false;
			should_deliver_to_tracee = false;

			if (PosixStatus.is_exit (status)) {
				kind = WaitKind.EXITED;
				exit_status = PosixStatus.parse_exit_status (status);
			} else if (PosixStatus.is_signaled (status)) {
				kind = WaitKind.SIGNALED;
				term_signal = PosixStatus.parse_termination_signal (status);
			} else if (PosixStatus.is_stopped (status)) {
				kind = WaitKind.STOPPED;

				stop_signal = PosixStatus.parse_stop_signal (status);
				ptrace_event = PosixStatus.ptrace_event (status);

				is_ptrace_event_stop = (stop_signal == Posix.Signal.TRAP) && (ptrace_event != NONE);
				is_syscall_stop = (stop_signal == (Posix.Signal.TRAP | 0x80)) && (ptrace_event == NONE);

				should_deliver_to_tracee = !is_syscall_stop && !is_ptrace_event_stop && stop_signal != Posix.Signal.TRAP;
			}
		}

		public void check_stopped () throws Error {
			switch (kind) {
				case WaitKind.EXITED:
					throw new Error.NOT_SUPPORTED ("Target exited with status %u", exit_status);
				case WaitKind.SIGNALED:
					throw new Error.NOT_SUPPORTED ("Target terminated with signal %u", term_signal);
				case WaitKind.STOPPED:
					return;
				default:
					throw new Error.NOT_SUPPORTED ("Unexpected status: 0x%08x", status);
			}
		}
	}

	namespace PosixStatus {
		[CCode (cname = "WIFEXITED", cheader_filename = "sys/wait.h")]
		private extern bool is_exit (int status);

		[CCode (cname = "WIFSIGNALED", cheader_filename = "sys/wait.h")]
		private extern bool is_signaled (int status);

		[CCode (cname = "WIFSTOPPED", cheader_filename = "sys/wait.h")]
		private extern bool is_stopped (int status);

		[CCode (cname = "WEXITSTATUS", cheader_filename = "sys/wait.h")]
		private extern uint parse_exit_status (int status);

		[CCode (cname = "WTERMSIG", cheader_filename = "sys/wait.h")]
		private extern Posix.Signal parse_termination_signal (int status);

		[CCode (cname = "WSTOPSIG", cheader_filename = "sys/wait.h")]
		private extern Posix.Signal parse_stop_signal (int status);

		private PtraceEvent ptrace_event (int status) {
			return ((uint) status) >> 16;
		}
	}

	public enum PtraceEvent {
		NONE,
		FORK,
		VFORK,
		CLONE,
		EXEC,
		VFORK_DONE,
		EXIT,
		SECCOMP,
	}

	private int tgkill (uint tgid, uint tid, Posix.Signal sig) {
		return Linux.syscall (LinuxSyscall.TGKILL, tgid, tid, sig);
	}

	public extern bool _syscall_satisfies (int syscall_id, LinuxSyscallMask mask);

	private size_t round_size_to_page_size (size_t size) {
		size_t page_size = Gum.query_page_size ();
		return (size + page_size - 1) & ~(page_size - 1);
	}
}
