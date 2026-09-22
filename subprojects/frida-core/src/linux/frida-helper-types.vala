namespace Frida {
	public interface LinuxHelper : Object {
		public signal void output (uint pid, int fd, uint8[] data);
		public signal void uninjected (uint id);

		public abstract async void close (Cancellable? cancellable) throws IOError;

		public abstract async uint spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
			UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, Cancellable? cancellable) throws Error, IOError;
		public abstract async void prepare_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void await_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void await_syscall (uint pid, LinuxSyscallMask mask, Cancellable? cancellable) throws Error, IOError;
		public abstract async void resume_syscall (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError;
		public abstract async void resume (uint pid, Cancellable? cancellable) throws Error, IOError;
		public abstract async void kill (uint pid, Cancellable? cancellable) throws Error, IOError;

		public abstract async void inject_library (uint pid, UnixInputStream library_so, string entrypoint, string data,
			AgentFeatures features, uint id, Cancellable? cancellable) throws Error, IOError;
		public abstract async IOStream request_control_channel (uint id, Cancellable? cancellable) throws Error, IOError;
		public abstract async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError;
		public abstract async void demonitor_and_clone_injectee_state (uint id, uint clone_id, AgentFeatures features,
			Cancellable? cancellable) throws Error, IOError;
		public abstract async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError;
	}

	[Flags]
	public enum LinuxSyscallMask {
		RESTART,
		IOCTL,
		READ,
		POLL_LIKE,
		WAIT,
		SIGWAIT,
		FUTEX,
		ACCEPT,
		RECV,
	}

	[Flags]
	public enum AgentFeatures {
		CONTROL_CHANNEL,
	}

	[DBus (name = "re.frida.Helper")]
	public interface LinuxRemoteHelper : Object {
		public signal void output (uint pid, int fd, uint8[] data);
		public signal void uninjected (uint id);

		public abstract async void stop (Cancellable? cancellable) throws GLib.Error;

		public abstract async uint spawn (string path, HostSpawnOptions options, Cancellable? cancellable) throws GLib.Error;
		public abstract async uint spawn_with_stdio (string path, HostSpawnOptions options, UnixInputStream stdin_stream,
			UnixOutputStream stdout_stream, UnixOutputStream stderr_stream, Cancellable? cancellable) throws GLib.Error;
		public abstract async void prepare_exec_transition (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void await_exec_transition (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void await_syscall (uint pid, LinuxSyscallMask mask, Cancellable? cancellable) throws GLib.Error;
		public abstract async void resume_syscall (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void input (uint pid, uint8[] data, Cancellable? cancellable) throws GLib.Error;
		public abstract async void resume (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void kill (uint pid, Cancellable? cancellable) throws GLib.Error;

		public abstract async void inject_library (uint pid, UnixInputStream library_so, string entrypoint, string data,
			AgentFeatures features, uint id, Cancellable? cancellable) throws GLib.Error;
		public abstract async Socket request_control_channel (uint id, Cancellable? cancellable) throws GLib.Error;
		public abstract async void demonitor (uint id, Cancellable? cancellable) throws GLib.Error;
		public abstract async void demonitor_and_clone_injectee_state (uint id, uint clone_id, AgentFeatures features,
			Cancellable? cancellable) throws GLib.Error;
		public abstract async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws GLib.Error;
	}

	public struct PathTemplate {
		public string str {
			get;
			private set;
		}

		public PathTemplate (string str) {
			this.str = str;
		}

		public string expand (string arch) {
			try {
				return /<arch>/.replace_literal (str, -1, 0, arch);
			} catch (RegexError e) {
				assert_not_reached ();
			}
		}
	}

	namespace ObjectPath {
		public const string HELPER = "/re/frida/Helper";
	}
}
