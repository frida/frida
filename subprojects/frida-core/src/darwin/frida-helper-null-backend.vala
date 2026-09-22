namespace Frida {
	public sealed class DarwinHelperBackend : Object, DarwinHelper {
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
				return true;
			}
		}

		public async void close (Cancellable? cancellable) throws IOError {
		}

		public async void preload (Cancellable? cancellable) throws Error, IOError {
		}

		public async void enable_spawn_gating (SpawnGatingScope scope, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
				UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void launch (string identifier, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void notify_launch_completed (string identifier, uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void notify_exec_completed (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void wait_until_suspended (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void cancel_pending_waits (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void kill_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void kill_application (string identifier, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			throw_not_supported ();
		}

		public async uint inject_library_blob (uint pid, string name, MappedLibraryBlob blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void prepare_target (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint demonitor_and_clone_injectee_state (uint id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async Future<IOStream> open_pipe_stream (uint remote_pid, Cancellable? cancellable, out string remote_address)
				throws Error, IOError {
			throw_not_supported ();
		}

		public async MappedLibraryBlob? try_mmap (Bytes blob, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public static void make_pipe_endpoint_from_socket (uint pid, uint task, GLib.Socket sock, out string address) throws Error {
			throw_not_supported ();
		}

		public static uint task_for_pid (uint pid) throws Error {
			throw_not_supported ();
		}

		public static void deallocate_port (uint port) {
		}
	}

	[NoReturn]
	private static void throw_not_supported () throws Error {
		throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
	}
}
