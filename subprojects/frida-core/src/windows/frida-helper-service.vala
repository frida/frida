namespace Frida {
	public int main (string[] args) {
		HelperMode mode = HelperMode.SERVICE;

		if (args.length > 1) {
			var mode_str = args[1].up ();
			switch (mode_str) {
				case "MANAGER":	    mode = HelperMode.MANAGER;	  break;
				case "STANDALONE":  mode = HelperMode.STANDALONE; break;
				case "SERVICE":	    mode = HelperMode.SERVICE;	  break;
				default:					  return 1;
			}
		}

		if (mode == HelperMode.MANAGER) {
			if (args.length != 4)
				return 1;
			PrivilegeLevel level;
			var level_str = args[2].up ();
			switch (level_str) {
				case "NORMAL":   level = PrivilegeLevel.NORMAL;   break;
				case "ELEVATED": level = PrivilegeLevel.ELEVATED; break;
				default:					  return 1;
			}
			var parent_address = args[3];

			var manager = new HelperManager (parent_address, level);
			return manager.run ();
		}

		string? svcname = (args.length > 2) ? args[2] : null;

		HelperService service;
		if (mode == HelperMode.STANDALONE)
			service = new StandaloneHelperService (svcname);
		else
			service = new ManagedHelperService (svcname);
		service.run ();

		return 0;
	}

	private enum HelperMode {
		MANAGER,
		STANDALONE,
		SERVICE
	}

	private sealed class HelperManager : Object, WindowsRemoteHelper {
		public string parent_address {
			get;
			construct;
		}

		public PrivilegeLevel level {
			get;
			construct;
		}

		private MainLoop loop = new MainLoop ();
		private int run_result = 0;

		private DBusConnection connection;
		private uint registration_id;
		private Gee.Collection<ServiceConnection> helpers = new Gee.ArrayList<ServiceConnection> ();
		private void * context;

		public HelperManager (string parent_address, PrivilegeLevel level) {
			Object (parent_address: parent_address, level: level);
		}

		public int run () {
			Idle.add (() => {
				start.begin ();
				return false;
			});

			loop.run ();

			return run_result;
		}

		private async void shutdown () {
			if (connection != null) {
				if (registration_id != 0)
					connection.unregister_object (registration_id);
				connection.on_closed.disconnect (on_connection_closed);
				try {
					yield connection.close ();
				} catch (GLib.Error connection_error) {
				}
				connection = null;
			}

			if (context != null)
				stop_services (context);
			loop.quit ();
		}

		private async void start () {
			try {
				var archs = new Gee.ArrayList<string> ();
				if (Gum.Windows.query_native_cpu_type () == ARM64)
					archs.add ("arm64");
				if (Gum.Windows.query_native_cpu_type () != IA32)
					archs.add ("x86_64");
				archs.add ("x86");

				string basename = HelperService.make_svcname_base ();

				foreach (string arch in archs) {
					var helper = new ServiceConnection (basename + arch);
					helpers.add (helper);
				}

				context = start_services (basename, archs.to_array (), level);

				foreach (var helper in helpers) {
					yield helper.open ();
					helper.proxy.uninjected.connect (on_uninjected);
				}

				var stream_request = Pipe.open (parent_address, null);
				var stream = yield stream_request.wait_async (null);

				connection = yield new DBusConnection (stream, null, DELAY_MESSAGE_PROCESSING);
				connection.on_closed.connect (on_connection_closed);

				WindowsRemoteHelper helper = this;
				registration_id = connection.register_object (ObjectPath.HELPER, helper);

				connection.start_message_processing ();
			} catch (GLib.Error e) {
				printerr ("Unable to start: %s\n", e.message);
				run_result = 1;
				shutdown.begin ();
			}
		}

		public async void stop (Cancellable? cancellable) throws Error, IOError {
			foreach (var helper in helpers) {
				try {
					yield helper.proxy.stop (cancellable);
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED)
						throw (IOError) e;
				}
			}

			Timeout.add (20, () => {
				shutdown.begin ();
				return false;
			});
		}

		public async bool can_handle_target (uint pid, Cancellable? cancellable) throws Error, IOError {
			return true;
		}

		public async void inject_library_file (uint pid, PathTemplate path_template, string entrypoint, string data,
				string[] dependencies, uint id, Cancellable? cancellable) throws Error, IOError {
			try {
				foreach (var helper in helpers) {
					if (yield helper.proxy.can_handle_target (pid, cancellable)) {
						yield helper.proxy.inject_library_file (pid, path_template, entrypoint, data, dependencies,
							id, cancellable);
						return;
					}
				}
				throw new Error.NOT_SUPPORTED ("Missing helper able to handle the given target");
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		private void on_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			stop.begin (null);
		}

		private void on_uninjected (uint id) {
			uninjected (id);
		}

		private class ServiceConnection {
			public WindowsRemoteHelper proxy {
				get;
				private set;
			}

			private string name;
			private Future<IOStream> stream_request;
			private DBusConnection connection;

			public ServiceConnection (string name) {
				this.name = name;
				this.stream_request = Pipe.open ("pipe:role=server,name=" + name, null);
			}

			public async void open () throws Error {
				try {
					var stream = yield this.stream_request.wait_async (null);

					connection = yield new DBusConnection (stream, null, NONE);
				} catch (GLib.Error e) {
					throw new Error.PERMISSION_DENIED ("%s", e.message);
				}

				try {
					proxy = yield connection.get_proxy (null, ObjectPath.HELPER, DO_NOT_LOAD_PROPERTIES);
				} catch (IOError e) {
					throw new Error.PROTOCOL ("%s", e.message);
				}
			}
		}

		private extern static void * start_services (string service_basename, string[] archs, PrivilegeLevel level);
		private extern static void stop_services (void * context);
	}

	private abstract class HelperService : Object, WindowsRemoteHelper {
		public PrivilegeLevel level {
			get;
			construct;
		}

		public string? svcname {
			get;
			construct;
		}

		private DBusConnection connection;
		private uint registration_id;

		private WindowsHelperBackend backend;

		construct {
			backend = new WindowsHelperBackend (level);
			backend.uninjected.connect (on_backend_uninjected);

			Idle.add (() => {
				start.begin ();
				return false;
			});
		}

		public abstract void run ();

		protected abstract void shutdown ();

		private async void start () {
			try {
				var stream_request = Pipe.open ("pipe:role=client,name=" + (svcname ?? derive_svcname_for_self ()), null);
				var stream = yield stream_request.wait_async (null);

				connection = yield new DBusConnection (stream, null, DELAY_MESSAGE_PROCESSING);
				connection.on_closed.connect (on_connection_closed);

				WindowsRemoteHelper helper = this;
				registration_id = connection.register_object (ObjectPath.HELPER, helper);

				connection.start_message_processing ();
			} catch (GLib.Error e) {
				printerr ("Unable to start: %s\n", e.message);
				shutdown ();
			}
		}

		public async void stop (Cancellable? cancellable) throws Error, IOError {
			Timeout.add (20, () => {
				do_stop.begin ();
				return false;
			});
		}

		private async void do_stop () {
			connection.unregister_object (registration_id);
			connection.on_closed.disconnect (on_connection_closed);
			try {
				yield connection.close ();
			} catch (GLib.Error connection_error) {
			}

			try {
				yield backend.close (null);
			} catch (IOError e) {
				assert_not_reached ();
			}

			shutdown ();
		}

		private void on_connection_closed (bool remote_peer_vanished, GLib.Error? error) {
			do_stop.begin ();
		}

		public async bool can_handle_target (uint pid, Cancellable? cancellable) throws Error, IOError {
			return cpu_type_from_pid (pid) == Gum.NATIVE_CPU;
		}

		public async void inject_library_file (uint pid, PathTemplate path_template, string entrypoint, string data,
				string[] dependencies, uint id, Cancellable? cancellable) throws Error, IOError {
			yield backend.inject_library_file (pid, path_template, entrypoint, data, dependencies, id, cancellable);
		}

		private void on_backend_uninjected (uint id) {
			uninjected (id);
		}

		public extern static string make_svcname_base ();
		public extern static string derive_basename ();
		public extern static string derive_svcname_for_self ();
	}

	private sealed class StandaloneHelperService : HelperService {
		private MainLoop loop;

		public StandaloneHelperService (string? svcname) {
			Object (level: PrivilegeLevel.NORMAL, svcname: svcname);
		}

		public override void run () {
			loop = new MainLoop ();
			loop.run ();
		}

		public override void shutdown () {
			Idle.add (() => {
				loop.quit ();
				return false;
			});
		}
	}

	private sealed class ManagedHelperService : HelperService {
		public ManagedHelperService (string? svcname) {
			Object (level: PrivilegeLevel.ELEVATED, svcname: svcname);
		}

		public override void run () {
			enter_dispatcher_and_main_loop ();
		}

		public override void shutdown () {
		}

		private extern static void enter_dispatcher_and_main_loop ();
	}
}
