namespace Frida {
	public interface WindowsHelper : Object {
		public signal void uninjected (uint id);

		public abstract async void close (Cancellable? cancellable) throws IOError;

		public abstract async void inject_library_file (uint pid, PathTemplate path_template, string entrypoint, string data,
			string[] dependencies, uint id, Cancellable? cancellable) throws Error, IOError;
	}

	[DBus (name = "re.frida.Helper")]
	public interface WindowsRemoteHelper : Object {
		public signal void uninjected (uint id);

		public abstract async void stop (Cancellable? cancellable) throws GLib.Error;

		public abstract async bool can_handle_target (uint pid, Cancellable? cancellable) throws GLib.Error;
		public abstract async void inject_library_file (uint pid, PathTemplate path_template, string entrypoint, string data,
			string[] dependencies, uint id, Cancellable? cancellable) throws GLib.Error;
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

	public enum PrivilegeLevel {
		NORMAL,
		ELEVATED
	}

	namespace ObjectPath {
		public const string HELPER = "/re/frida/Helper";
	}
}
