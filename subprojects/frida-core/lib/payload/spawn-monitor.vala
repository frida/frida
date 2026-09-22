namespace Frida {
	public sealed class SpawnMonitor : Object, Gum.InvocationListener {
		public weak SpawnHandler handler {
			get;
			construct;
		}

		public MainContext main_context {
			get;
			construct;
		}

		private Mutex mutex;
		private Cond cond;

		public enum OperationStatus {
			QUEUED,
			COMPLETED
		}

#if DARWIN
		private PosixSpawnFunc posix_spawn;
		private PosixSpawnAttrInitFunc posix_spawnattr_init;
		private PosixSpawnAttrDestroyFunc posix_spawnattr_destroy;
		private PosixSpawnAttrGetFlagsFunc posix_spawnattr_getflags;
		private PosixSpawnAttrSetFlagsFunc posix_spawnattr_setflags;

		private void * execve;

		private static Private posix_spawn_caller_is_internal = new Private ();
#endif

		public SpawnMonitor (SpawnHandler handler, MainContext main_context) {
			Object (handler: handler, main_context: main_context);
		}

		construct {
			var interceptor = Gum.Interceptor.obtain ();

#if WINDOWS
			var kernelbase = Gum.Process.find_module_by_name ("kernelbase.dll");
			var create_process_internal = (kernelbase != null) ? kernelbase.find_export_by_name ("CreateProcessInternalW") : 0;
			if (create_process_internal == 0) {
				create_process_internal = Gum.Process.find_module_by_name ("kernel32.dll")
					.find_export_by_name ("CreateProcessInternalW");
			}
			assert (create_process_internal != 0);
			interceptor.attach ((void *) create_process_internal, this);
#else
			var libc = Gum.Process.get_libc_module ();
#if DARWIN
			posix_spawn = (PosixSpawnFunc) libc.find_export_by_name ("posix_spawn");
			posix_spawnattr_init = (PosixSpawnAttrInitFunc) libc.find_export_by_name ("posix_spawnattr_init");
			posix_spawnattr_destroy = (PosixSpawnAttrDestroyFunc) libc.find_export_by_name ("posix_spawnattr_destroy");
			posix_spawnattr_getflags = (PosixSpawnAttrSetFlagsFunc) libc.find_export_by_name ("posix_spawnattr_getflags");
			posix_spawnattr_setflags = (PosixSpawnAttrSetFlagsFunc) libc.find_export_by_name ("posix_spawnattr_setflags");

			execve = (void *) libc.find_export_by_name ("execve");

			interceptor.attach ((void *) posix_spawn, this);

			var replace_options = Gum.ReplaceOptions ();
			replace_options.replacement_data = this;
			interceptor.replace (execve, (void *) replacement_execve, null, replace_options);
#else
			Gum.Address execve = 0;
#if ANDROID
			execve = libc.find_symbol_by_name ("__execve");
#endif
			if (execve == 0)
				execve = libc.find_export_by_name ("execve");
			interceptor.attach ((void *) execve, this);
#endif
#endif
		}

		public override void dispose () {
			var interceptor = Gum.Interceptor.obtain ();

#if DARWIN
			interceptor.revert (execve);
#endif

			interceptor.detach (this);

			base.dispose ();
		}

#if !WINDOWS
		private void on_exec_imminent (HostChildInfo * info) {
			mutex.lock ();

			OperationStatus status = QUEUED;

			var source = new IdleSource ();
			source.set_callback (() => {
				perform_prepare_to_exec.begin (info, &status);
				return false;
			});
			source.attach (main_context);

			while (status != COMPLETED)
				cond.wait (mutex);

			mutex.unlock ();
		}

		private async void perform_prepare_to_exec (HostChildInfo * info, OperationStatus * status) {
			yield handler.prepare_to_exec (info);

			notify_operation_completed (status);
		}

		private void on_exec_cancelled (uint pid) {
			mutex.lock ();

			OperationStatus status = QUEUED;

			var source = new IdleSource ();
			source.set_callback (() => {
				perform_cancel_exec.begin (pid, &status);
				return false;
			});
			source.attach (main_context);

			while (status != COMPLETED)
				cond.wait (mutex);

			mutex.unlock ();
		}

		private async void perform_cancel_exec (uint pid, OperationStatus * status) {
			yield handler.cancel_exec (pid);

			notify_operation_completed (status);
		}
#endif

#if WINDOWS || DARWIN
		private void on_spawn_created (HostChildInfo * info, SpawnStartState start_state) {
			mutex.lock ();

			OperationStatus status = QUEUED;

			var source = new IdleSource ();
			source.set_callback (() => {
				perform_acknowledge_spawn.begin (info, start_state, &status);
				return false;
			});
			source.attach (main_context);

			while (status != COMPLETED)
				cond.wait (mutex);

			mutex.unlock ();
		}

		private async void perform_acknowledge_spawn (HostChildInfo * info, SpawnStartState start_state, OperationStatus * status) {
			yield handler.acknowledge_spawn (info, start_state);

			notify_operation_completed (status);
		}
#endif

#if WINDOWS
		private void on_enter (Gum.InvocationContext context) {
			Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));

			invocation.application_name = (string16?) context.get_nth_argument (1);
			invocation.command_line = (string16?) context.get_nth_argument (2);

			invocation.creation_flags = (uint32) context.get_nth_argument (6);
			context.replace_nth_argument (6, (void *) (invocation.creation_flags | CreateProcessFlags.CREATE_SUSPENDED));

			invocation.environment = context.get_nth_argument (7);

			invocation.process_info = context.get_nth_argument (10);
		}

		private void on_leave (Gum.InvocationContext context) {
			var success = (bool) context.get_return_value ();
			if (!success)
				return;

			Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));

			var pid = invocation.process_info.process_id;
			var parent_pid = get_process_id ();
			var info = HostChildInfo (pid, parent_pid, ChildOrigin.SPAWN);

			string path = null;
			string[] argv;
			try {
				if (invocation.application_name != null)
					path = invocation.application_name.to_utf8 ();

				if (invocation.command_line != null) {
					Shell.parse_argv (invocation.command_line.to_utf8 ().replace ("\\", "\\\\"), out argv);
					if (path == null)
						path = argv[0];
				} else {
					argv = { path };
				}
			} catch (ConvertError e) {
				assert_not_reached ();
			} catch (ShellError e) {
				assert_not_reached ();
			}
			info.path = path;
			info.has_argv = true;
			info.argv = argv;

			string[]? envp = null;
			if (invocation.environment != null) {
				if ((invocation.creation_flags & CreateProcessFlags.CREATE_UNICODE_ENVIRONMENT) != 0)
					envp = _parse_unicode_environment (invocation.environment);
				else
					envp = _parse_ansi_environment (invocation.environment);
				info.has_envp = true;
				info.envp = envp;
			}

			on_spawn_created (&info, SpawnStartState.SUSPENDED);

			if ((invocation.creation_flags & CreateProcessFlags.CREATE_SUSPENDED) == 0)
				_resume_thread (invocation.process_info.thread);
		}

		private struct Invocation {
			public unowned string16? application_name;
			public unowned string16? command_line;

			public uint32 creation_flags;

			public void * environment;

			public CreateProcessInfo * process_info;
		}

		public struct CreateProcessInfo {
			public void * process;
			public void * thread;
			public uint32 process_id;
			public uint32 thread_id;
		}

		[Flags]
		private enum CreateProcessFlags {
			CREATE_SUSPENDED		= 0x00000004,
			CREATE_UNICODE_ENVIRONMENT	= 0x00000400,
		}

		public extern static uint32 _resume_thread (void * thread);
		public extern static string[] _get_environment ();
		public extern static string[] _parse_unicode_environment (void * env);
		public extern static string[] _parse_ansi_environment (void * env);
#elif DARWIN
		private void on_enter (Gum.InvocationContext context) {
			var caller_is_internal = (bool) posix_spawn_caller_is_internal.get ();
			if (caller_is_internal)
				return;

			Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));

			invocation.pid = context.get_nth_argument (0);
			if (invocation.pid == null) {
				invocation.pid = &invocation.pid_storage;
				context.replace_nth_argument (0, invocation.pid);
			}

			invocation.path = (string?) context.get_nth_argument (1);

			posix_spawnattr_init (&invocation.attr_storage);

			posix_spawnattr_t * attr = context.get_nth_argument (3);
			if (attr == null) {
				attr = &invocation.attr_storage;
				context.replace_nth_argument (3, attr);
			}
			invocation.attr = attr;

			posix_spawnattr_getflags (attr, out invocation.flags);
			posix_spawnattr_setflags (attr, invocation.flags | PosixSpawnFlags.START_SUSPENDED);

			invocation.argv = parse_strv ((string **) context.get_nth_argument (4));

			invocation.envp = parse_strv ((string **) context.get_nth_argument (5));

			if ((invocation.flags & PosixSpawnFlags.SETEXEC) != 0) {
				var pid = Posix.getpid ();
				var parent_pid = pid;
				var info = HostChildInfo (pid, parent_pid, ChildOrigin.EXEC);
				fill_child_info_path_argv_and_envp (ref info, invocation.path, invocation.argv, invocation.envp);

				on_exec_imminent (&info);
			}
		}

		private void on_leave (Gum.InvocationContext context) {
			var caller_is_internal = (bool) posix_spawn_caller_is_internal.get ();
			if (caller_is_internal)
				return;

			Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));

			int result = (int) context.get_return_value ();

			if ((invocation.flags & PosixSpawnFlags.SETEXEC) != 0) {
				on_exec_cancelled (Posix.getpid ());
			} else if (result == 0) {
				var pid = *(invocation.pid);
				var parent_pid = Posix.getpid ();
				var info = HostChildInfo (pid, parent_pid, ChildOrigin.SPAWN);
				fill_child_info_path_argv_and_envp (ref info, invocation.path, invocation.argv, invocation.envp);

				SpawnStartState start_state = ((invocation.flags & PosixSpawnFlags.START_SUSPENDED) != 0)
					? SpawnStartState.SUSPENDED
					: SpawnStartState.RUNNING;

				on_spawn_created (&info, start_state);
			}

			posix_spawnattr_destroy (&invocation.attr_storage);
		}

		private static int replacement_execve (string? path, string ** argv, string ** envp) {
			unowned Gum.InvocationContext context = Gum.Interceptor.get_current_invocation ();
			var monitor = (SpawnMonitor) context.get_replacement_data ();

			return monitor.handle_execve (path, argv, envp);
		}

		private int handle_execve (string? path, string ** argv, string ** envp) {
			var pid = Posix.getpid ();
			var parent_pid = pid;
			var info = HostChildInfo (pid, parent_pid, ChildOrigin.EXEC);
			fill_child_info_path_argv_and_envp (ref info, path, parse_strv (argv), parse_strv (envp));

			on_exec_imminent (&info);

			Pid resulting_pid;

			posix_spawnattr_t attr;
			posix_spawnattr_init (&attr);
			posix_spawnattr_setflags (&attr, PosixSpawnFlags.SETEXEC | PosixSpawnFlags.START_SUSPENDED);

			posix_spawn_caller_is_internal.set ((void *) true);

			var result = posix_spawn (out resulting_pid, path, null, &attr, argv, envp);
			var spawn_errno = Posix.errno;

			posix_spawn_caller_is_internal.set ((void *) false);

			posix_spawnattr_destroy (&attr);

			on_exec_cancelled (pid);

			Posix.errno = spawn_errno;

			return result;
		}

		private struct Invocation {
			public Posix.pid_t * pid;
			public Posix.pid_t pid_storage;
			public unowned string? path;
			public posix_spawnattr_t * attr;
			public posix_spawnattr_t attr_storage;
			public uint16 flags;
			public unowned string[]? argv;
			public unowned string[]? envp;
		}

		[CCode (has_target = false)]
		private delegate int PosixSpawnFunc (out Pid pid, string path, void * file_actions, posix_spawnattr_t * attr, string ** argv, string ** envp);

		[CCode (has_target = false)]
		private delegate int PosixSpawnAttrInitFunc (posix_spawnattr_t * attr);

		[CCode (has_target = false)]
		private delegate int PosixSpawnAttrDestroyFunc (posix_spawnattr_t * attr);

		[CCode (has_target = false)]
		private delegate int PosixSpawnAttrGetFlagsFunc (posix_spawnattr_t * attr, out uint16 flags);

		[CCode (has_target = false)]
		private delegate int PosixSpawnAttrSetFlagsFunc (posix_spawnattr_t * attr, uint16 flags);

		[SimpleType]
		[IntegerType (rank = 9)]
		[CCode (cname = "posix_spawnattr_t", cheader_filename = "spawn.h", has_type_id = false)]
		private struct posix_spawnattr_t : size_t {
		}

		[Flags]
		private enum PosixSpawnFlags {
			SETEXEC		= 0x0040,
			START_SUSPENDED	= 0x0080,
		}
#else
		private void on_enter (Gum.InvocationContext context) {
			Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));
			invocation.pid = Posix.getpid ();

			var parent_pid = invocation.pid;
			var info = HostChildInfo (invocation.pid, parent_pid, ChildOrigin.EXEC);
			unowned string? path = (string?) context.get_nth_argument (0);
			var argv = parse_strv ((string **) context.get_nth_argument (1));
			var envp = parse_strv ((string **) context.get_nth_argument (2));
			fill_child_info_path_argv_and_envp (ref info, path, argv, envp);

			on_exec_imminent (&info);
		}

		private void on_leave (Gum.InvocationContext context) {
			Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));
			on_exec_cancelled (invocation.pid);
		}

		private struct Invocation {
			public uint pid;
		}
#endif

#if !WINDOWS
		private static void fill_child_info_path_argv_and_envp (ref HostChildInfo info, string? path, string[]? argv, string[]? envp) {
			if (path != null)
				info.path = path;

			if (argv != null) {
				info.has_argv = true;
				info.argv = argv;
			}

			if (envp != null) {
				info.has_envp = true;
				info.envp = envp;
			}
		}

		private unowned string[]? parse_strv (string ** strv) {
			if (strv == null)
				return null;

			unowned string[] elements = (string[]) strv;
			return elements[0:strv_length (elements)];
		}
#endif

		private void notify_operation_completed (OperationStatus * status) {
			mutex.lock ();
			*status = COMPLETED;
			cond.broadcast ();
			mutex.unlock ();
		}
	}

	public interface SpawnHandler : Object {
		public abstract async void prepare_to_exec (HostChildInfo * info);
		public abstract async void cancel_exec (uint pid);
		public abstract async void acknowledge_spawn (HostChildInfo * info, SpawnStartState start_state);
	}
}
