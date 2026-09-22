namespace Frida {
	public interface DarwinHelper : Object {
		public signal void output (uint pid, int fd, uint8[] data);
		public signal void gating_cancelled ();
		public signal void spawn_added (HostSpawnInfo info);
		public signal void spawn_removed (HostSpawnInfo info);
		public signal void injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module);
		public signal void uninjected (uint id);
		public signal void process_resumed (uint pid);
		public signal void process_killed (uint pid);

		public abstract uint pid {
			get;
		}

		public abstract async void close (Cancellable? cancellable) throws IOError;

		public abstract async void preload (Cancellable? cancellable) throws Error, IOError;

		public abstract async void enable_spawn_gating (SpawnGatingScope scope, Cancellable? cancellable) throws Error, IOError;
		public abstract async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError;
		public abstract async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError;
		public abstract async uint spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
			UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, Cancellable? cancellable) throws Error, IOError;
		public abstract async void launch (string identifier, HostSpawnOptions options,
			Cancellable? cancellable) throws Error, IOError;
		public abstract async void notify_launch_completed (string identifier, uint pid,
			Cancellable? cancellable) throws Error, IOError;
		public abstract async void notify_exec_completed (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void wait_until_suspended (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void cancel_pending_waits (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError;
		public abstract async void resume (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void kill_process (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void kill_application (string identifier, Cancellable? cancellable) throws Error, IOError;

		public abstract async uint inject_library_file (uint pid, string path, string entrypoint, string data,
			Cancellable? cancellable) throws Error, IOError;
		public abstract async uint inject_library_blob (uint pid, string name, MappedLibraryBlob blob, string entrypoint,
			string data, Cancellable? cancellable) throws Error, IOError;
		public abstract async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError;
		public abstract async uint demonitor_and_clone_injectee_state (uint id, Cancellable? cancellable) throws Error, IOError;
		public abstract async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError;

		public abstract async Future<IOStream> open_pipe_stream (uint remote_pid, Cancellable? cancellable,
			out string remote_address) throws Error, IOError;

		public abstract async MappedLibraryBlob? try_mmap (Bytes blob, Cancellable? cancellable) throws Error, IOError;
	}

	[DBus (name = "re.frida.Helper")]
	public interface DarwinRemoteHelper : Object {
		public signal void output (uint pid, int fd, uint8[] data);
		public signal void gating_cancelled ();
		public signal void spawn_added (HostSpawnInfo info);
		public signal void spawn_removed (HostSpawnInfo info);
		public signal void injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module);
		public signal void uninjected (uint id);
		public signal void process_resumed (uint pid);
		public signal void process_killed (uint pid);

		public abstract async void stop (Cancellable? cancellable) throws GLib.Error;

		public abstract async void enable_spawn_gating (SpawnGatingScope scope, Cancellable? cancellable) throws GLib.Error;
		public abstract async void disable_spawn_gating (Cancellable? cancellable) throws GLib.Error;
		public abstract async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws GLib.Error;
		public abstract async uint spawn (string path, HostSpawnOptions options, Cancellable? cancellable) throws GLib.Error;
		public abstract async uint spawn_with_stdio (string path, HostSpawnOptions options, UnixInputStream stdin_stream,
			UnixOutputStream stdout_stream, UnixOutputStream stderr_stream, Cancellable? cancellable) throws GLib.Error;
		public abstract async void launch (string identifier, HostSpawnOptions options, Cancellable? cancellable) throws GLib.Error;
		public abstract async void notify_launch_completed (string identifier, uint pid,
			Cancellable? cancellable) throws GLib.Error;
		public abstract async void notify_exec_completed (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void wait_until_suspended (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void cancel_pending_waits (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void input (uint pid, uint8[] data, Cancellable? cancellable) throws GLib.Error;
		public abstract async void resume (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void kill_process (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void kill_application (string identifier, Cancellable? cancellable) throws GLib.Error;

		public abstract async uint inject_library_file (uint pid, string path, string entrypoint, string data,
			Cancellable? cancellable) throws GLib.Error;
		public abstract async uint inject_library_blob (uint pid, string name, MappedLibraryBlob blob, string entrypoint,
			string data, Cancellable? cancellable) throws GLib.Error;
		public abstract async void demonitor (uint id, Cancellable? cancellable) throws GLib.Error;
		public abstract async uint demonitor_and_clone_injectee_state (uint id, Cancellable? cancellable) throws GLib.Error;
		public abstract async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws GLib.Error;

		public abstract async void transfer_socket (uint pid, GLib.Socket sock, Cancellable? cancellable,
			out string remote_address) throws GLib.Error;
	}

	public struct PipeEndpoints {
		public string local_address {
			get;
			private set;
		}

		public string remote_address {
			get;
			private set;
		}

		public PipeEndpoints (string local_address, string remote_address) {
			this.local_address = local_address;
			this.remote_address = remote_address;
		}
	}

	public struct DarwinModuleDetails {
		public uint64 mach_header_address {
			get;
			private set;
		}

		public string uuid {
			get;
			private set;
		}

		public string path {
			get;
			private set;
		}

		public DarwinModuleDetails (uint64 mach_header_address, string uuid, string path) {
			this.mach_header_address = mach_header_address;
			this.uuid = uuid;
			this.path = path;
		}
	}

	namespace ObjectPath {
		public const string HELPER = "/re/frida/Helper";
		public const string SYSTEM_SESSION_PROVIDER = "/re/frida/SystemSessionProvider";
	}
}
