[CCode (gir_namespace = "Frida", gir_version = "1.0")]
namespace Frida {
	public extern void init ();
	public extern void init_with_runtime (Runtime runtime);
	public extern void shutdown ();
	public extern void deinit ();
	public extern unowned MainContext get_main_context ();

	public extern void unref (void * obj);

	public extern void version (out uint major, out uint minor, out uint micro, out uint nano);
	public extern unowned string version_string ();

	/**
	 * Selects which main-loop runtime Frida integrates with.
	 */
	public enum Runtime {
		/**
		 * Use GLib's own main loop.
		 */
		GLIB,
		/**
		 * Integrate with a foreign runtime that hosts GLib.
		 */
		OTHER;

		public static Runtime from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<Runtime> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<Runtime> (this);
		}
	}

	/**
	 * Enumerates and keeps track of the devices available to Frida, and is the
	 * usual entry point of the API.
	 *
	 * Create one, then obtain a {@link Device} — for example the local system
	 * via {@link DeviceManager.get_device_by_type} with
	 * {@link DeviceType.LOCAL}, or a USB-connected device.
	 */
	public sealed class DeviceManager : Object, HostSessionHub {
		/**
		 * Emitted when a device is added, such as when a phone is plugged in.
		 *
		 * @param device the device that appeared
		 */
		public signal void added (Device device);
		/**
		 * Emitted when a device is removed, such as when a phone is unplugged.
		 *
		 * @param device the device that went away
		 */
		public signal void removed (Device device);
		/**
		 * Emitted when the set of available devices changes.
		 */
		public signal void changed ();

		/**
		 * Predicate deciding whether a device matches.
		 *
		 * @param device the device to test
		 * @return true if @device matches
		 */
		public delegate bool Predicate (Device device);

		private Promise<bool>? start_request;
		private Promise<bool>? stop_request;

		private HostSessionService? service;
		private Gee.ArrayList<Device> devices = new Gee.ArrayList<Device> ();
		private Gee.ArrayList<DeviceObserverEntry> on_device_added = new Gee.ArrayList<DeviceObserverEntry> ();

#if HAVE_BAREBONE_BACKEND
		private uint next_barebone_device_serial = 1;
#endif

		private Cancellable io_cancellable = new Cancellable ();

		private const string BAREBONE_DEVICE_ID_PREFIX = "barebone@";

		/**
		 * Creates a new device manager with all backends enabled.
		 */
		public DeviceManager () {
			service = new HostSessionService.with_default_backends ();
		}

		/**
		 * Creates a device manager with only the non-local backends enabled, so
		 * the local system is not exposed as a device.
		 */
		public DeviceManager.with_nonlocal_backends_only () {
			service = new HostSessionService.with_nonlocal_backends_only ();
		}

		/**
		 * Creates a device manager with only the socket backend enabled, for
		 * connecting to remote devices over the network.
		 */
		public DeviceManager.with_socket_backend_only () {
			service = new HostSessionService.with_socket_backend_only ();
		}

		/**
		 * Closes the device manager, releasing all resources. The instance must
		 * not be used afterwards.
		 */
		public async void close (Cancellable? cancellable = null) throws IOError {
			yield stop_service (cancellable);
		}

		public void close_sync (Cancellable? cancellable = null) throws IOError {
			try {
				create<CloseTask> ().execute (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private class CloseTask : ManagerTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.close (cancellable);
			}
		}

		/**
		 * Gets the device with the given ID, throwing if it cannot be found.
		 *
		 * @param id the device identifier
		 * @param timeout milliseconds to wait for a match, or 0 to not wait
		 * @return the matching device
		 */
		public async Device get_device_by_id (string id, int timeout = 0, Cancellable? cancellable = null) throws Error, IOError {
			return check_device (yield find_device_by_id (id, timeout, cancellable));
		}

		public Device get_device_by_id_sync (string id, int timeout = 0, Cancellable? cancellable = null) throws Error, IOError {
			return check_device (find_device_by_id_sync (id, timeout, cancellable));
		}

		/**
		 * Gets the first device of the given type, throwing if none is found.
		 *
		 * @param type the kind of device to look for
		 * @param timeout milliseconds to wait for a match, or 0 to not wait
		 * @return the matching device
		 */
		public async Device get_device_by_type (DeviceType type, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			return check_device (yield find_device_by_type (type, timeout, cancellable));
		}

		public Device get_device_by_type_sync (DeviceType type, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			return check_device (find_device_by_type_sync (type, timeout, cancellable));
		}

		/**
		 * Gets the first device accepted by @predicate, throwing if none is
		 * found.
		 *
		 * @param predicate function deciding whether a device matches
		 * @param timeout milliseconds to wait for a match, or 0 to not wait
		 * @return the matching device
		 */
		public async Device get_device (Predicate predicate, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			return check_device (yield find_device (predicate, timeout, cancellable));
		}

		public Device get_device_sync (Predicate predicate, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			return check_device (find_device_sync (predicate, timeout, cancellable));
		}

		private Device check_device (Device? device) throws Error {
			if (device == null)
				throw new Error.INVALID_ARGUMENT ("Device not found");
			return device;
		}

		/**
		 * Finds the device with the given ID.
		 *
		 * @param id the device identifier
		 * @param timeout milliseconds to wait for a match, or 0 to not wait
		 * @return the device, or null if not found
		 */
		public async Device? find_device_by_id (string id, int timeout = 0, Cancellable? cancellable = null) throws Error, IOError {
			return yield find_device ((device) => { return device.id == id; }, timeout, cancellable);
		}

		public Device? find_device_by_id_sync (string id, int timeout = 0, Cancellable? cancellable = null) throws Error, IOError {
			return find_device_sync ((device) => { return device.id == id; }, timeout, cancellable);
		}

		/**
		 * Finds the first device of the given type.
		 *
		 * @param type the kind of device to look for
		 * @param timeout milliseconds to wait for a match, or 0 to not wait
		 * @return the device, or null if not found
		 */
		public async Device? find_device_by_type (DeviceType type, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			return yield find_device ((device) => { return device.dtype == type; }, timeout, cancellable);
		}

		public Device? find_device_by_type_sync (DeviceType type, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			return find_device_sync ((device) => { return device.dtype == type; }, timeout, cancellable);
		}

		/**
		 * Finds the first device accepted by @predicate.
		 *
		 * @param predicate function deciding whether a device matches
		 * @param timeout milliseconds to wait for a match, or 0 to not wait
		 * @return the device, or null if not found
		 */
		public async Device? find_device (Predicate predicate, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			foreach (var device in devices) {
				if (predicate (device))
					return device;
			}

			bool started = start_request != null && start_request.future.ready;
			if (started && timeout == 0)
				return null;

			Device? added_device = null;
			var addition_observer = new DeviceObserverEntry ((device) => {
				if (predicate (device)) {
					added_device = device;
					find_device.callback ();
				}
			});
			on_device_added.add (addition_observer);

			Source? timeout_source = null;
			if (timeout > 0) {
				timeout_source = new TimeoutSource (timeout);
				timeout_source.set_callback (find_device.callback);
				timeout_source.attach (MainContext.get_thread_default ());
			}

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (find_device.callback);
			cancel_source.attach (MainContext.get_thread_default ());

			bool waiting = false;

			if (!started) {
				ensure_service_and_then_call.begin (() => {
						if (waiting && timeout == 0)
							find_device.callback ();
						return false;
					}, io_cancellable);
			}

			waiting = true;
			yield;
			waiting = false;

			cancel_source.destroy ();

			if (timeout_source != null)
				timeout_source.destroy ();

			on_device_added.remove (addition_observer);

			return added_device;
		}

		public Device? find_device_sync (Predicate predicate, int timeout = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<FindDeviceTask> () as FindDeviceTask;
			task.predicate = (device) => {
				return predicate (device);
			};
			task.timeout = timeout;
			return task.execute (cancellable);
		}

		private class FindDeviceTask : ManagerTask<Device?> {
			public Predicate predicate;
			public int timeout;

			protected override async Device? perform_operation () throws Error, IOError {
				return yield parent.find_device (predicate, timeout, cancellable);
			}
		}

		/**
		 * Enumerates the currently available devices.
		 *
		 * @return the available devices
		 */
		public async DeviceList enumerate_devices (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			yield ensure_service (cancellable);

			return new DeviceList (devices.slice (0, devices.size));
		}

		public DeviceList enumerate_devices_sync (Cancellable? cancellable = null) throws Error, IOError {
			return create<EnumerateDevicesTask> ().execute (cancellable);
		}

		private class EnumerateDevicesTask : ManagerTask<DeviceList> {
			protected override async DeviceList perform_operation () throws Error, IOError {
				return yield parent.enumerate_devices (cancellable);
			}
		}

		/**
		 * Adds a device reachable over the network at the given address.
		 *
		 * @param address the host and optional port to connect to
		 * @param options connection options, such as TLS and authentication, or
		 *   null
		 * @return the newly added device
		 */
		public async Device add_remote_device (string address, RemoteDeviceOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
#if HAVE_SOCKET_BACKEND
			check_open ();

			var socket_device = yield get_device ((device) => {
					return device.provider is SocketHostSessionProvider;
				}, 0, cancellable);

			string id = "socket@" + address;

			foreach (var device in devices) {
				if (device.id == id)
					return device;
			}

			unowned string name = address;

			var raw_options = new HostSessionOptions ();
			var opts = raw_options.map;
			opts["address"] = address;
			if (options != null) {
				TlsCertificate? cert = options.certificate;
				if (cert != null)
					opts["certificate"] = cert;

				string? origin = options.origin;
				if (origin != null)
					opts["origin"] = origin;

				string? token = options.token;
				if (token != null)
					opts["token"] = token;

				int interval = options.keepalive_interval;
				if (interval != -1)
					opts["keepalive_interval"] = interval;
			}

			var device = new Device (this, socket_device.provider, id, name, raw_options);
			devices.add (device);
			added (device);
			changed ();

			return device;
#else
			throw new Error.NOT_SUPPORTED ("Socket backend not available");
#endif
		}

		public Device add_remote_device_sync (string address, RemoteDeviceOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<AddRemoteDeviceTask> ();
			task.address = address;
			task.options = options;
			return task.execute (cancellable);
		}

		private class AddRemoteDeviceTask : ManagerTask<Device> {
			public string address;
			public RemoteDeviceOptions? options;

			protected override async Device perform_operation () throws Error, IOError {
				return yield parent.add_remote_device (address, options, cancellable);
			}
		}

		/**
		 * Removes a remote device previously added with
		 * {@link DeviceManager.add_remote_device}.
		 *
		 * @param address the address the device was added with
		 */
		public async void remove_remote_device (string address, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			yield ensure_service (cancellable);

			string id = "socket@" + address;

			foreach (var device in devices) {
				if (device.id == id) {
					yield device._do_close (APPLICATION_REQUESTED, true, cancellable);
					removed (device);
					changed ();
					return;
				}
			}

			throw new Error.INVALID_ARGUMENT ("Device not found");
		}

		public void remove_remote_device_sync (string address, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<RemoveRemoteDeviceTask> ();
			task.address = address;
			task.execute (cancellable);
		}

		private class RemoveRemoteDeviceTask : ManagerTask<void> {
			public string address;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.remove_remote_device (address, cancellable);
			}
		}

		/**
		 * Adds a device backed by the Barebone backend, which talks to a target
		 * that has no operating system of its own, or one that Frida reaches
		 * through a debugger stub or an injected agent.
		 *
		 * @param config how to reach the target, and how to allocate memory in it
		 * @param options how the device presents itself
		 * @return the newly added device
		 */
		public async Device add_barebone_device (BareboneConfig config, BareboneDeviceOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
#if HAVE_BAREBONE_BACKEND
			check_open ();
			yield ensure_service (cancellable);

			string id = options?.id ?? (BAREBONE_DEVICE_ID_PREFIX + (next_barebone_device_serial++).to_string ());

			foreach (var existing in devices) {
				if (existing.id == id)
					throw new Error.INVALID_ARGUMENT ("Device \"%s\" already exists", id);
			}

			var raw_options = new HostSessionOptions ();
			raw_options.map["config"] = config;

			var provider = new BareboneHostSessionProvider ();
			var device = new Device (this, provider, id, options?.name, raw_options, options?.icon);
			devices.add (device);
			added (device);
			changed ();

			return device;
#else
			throw new Error.NOT_SUPPORTED ("Barebone backend not available");
#endif
		}

		public Device add_barebone_device_sync (BareboneConfig config, BareboneDeviceOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<AddBareboneDeviceTask> ();
			task.config = config;
			task.options = options;
			return task.execute (cancellable);
		}

		private class AddBareboneDeviceTask : ManagerTask<Device> {
			public BareboneConfig config;
			public BareboneDeviceOptions? options;

			protected override async Device perform_operation () throws Error, IOError {
				return yield parent.add_barebone_device (config, options, cancellable);
			}
		}

		/**
		 * Removes a Barebone device previously added with
		 * {@link DeviceManager.add_barebone_device}.
		 *
		 * @param device the device that was added
		 */
		public async void remove_barebone_device (Device device, Cancellable? cancellable = null) throws Error, IOError {
#if HAVE_BAREBONE_BACKEND
			check_open ();

			yield ensure_service (cancellable);

			if (!(device.provider is BareboneHostSessionProvider) || !devices.contains (device))
				throw new Error.INVALID_ARGUMENT ("Device not found");

			yield device._do_close (APPLICATION_REQUESTED, true, cancellable);
			removed (device);
			changed ();
#else
			throw new Error.NOT_SUPPORTED ("Barebone backend not available");
#endif
		}

		public void remove_barebone_device_sync (Device device, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<RemoveBareboneDeviceTask> ();
			task.device = device;
			task.execute (cancellable);
		}

		private class RemoveBareboneDeviceTask : ManagerTask<void> {
			public Device device;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.remove_barebone_device (device, cancellable);
			}
		}

		internal void _release_device (Device device) {
			var device_did_exist = devices.remove (device);
			assert (device_did_exist);
		}

		private async void ensure_service (Cancellable? cancellable) throws Error, IOError {
			if (start_request == null) {
				start_request = new Promise<bool> ();
				start_service.begin ();
			}

			try {
				yield start_request.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			} catch (IOError e) {
				cancellable.set_error_if_cancelled ();
				throw new Error.INVALID_OPERATION ("DeviceManager is closing");
			}
		}

		private async void ensure_service_and_then_call (owned SourceFunc callback, Cancellable cancellable) {
			var source = new IdleSource ();
			source.set_callback (ensure_service_and_then_call.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield ensure_service (cancellable);
			} catch (GLib.Error e) {
			}

			callback ();
		}

		private async void start_service () {
			try {
				service.provider_available.connect (on_provider_available);
				service.provider_unavailable.connect (on_provider_unavailable);

				yield service.start (io_cancellable);

				start_request.resolve (true);
			} catch (IOError e) {
				start_request.reject (e);
			}
		}

		private void on_provider_available (HostSessionProvider provider) {
			var device = new Device (this, provider);
			devices.add (device);

			foreach (var observer in on_device_added.to_array ())
				observer.func (device);

			var started = start_request.future.ready;
			if (started) {
				added (device);
				changed ();
			}
		}

		private void on_provider_unavailable (HostSessionProvider provider) {
			var started = start_request.future.ready;

			foreach (var device in devices) {
				if (device.provider == provider) {
					if (started)
						removed (device);
					device._do_close.begin (DEVICE_LOST, false, io_cancellable);
					break;
				}
			}

			if (started)
				changed ();
		}

		private async HostSessionEntry resolve_host_session (string id, Cancellable? cancellable) throws Error, IOError {
			var device = yield get_device_by_id (id, 0, cancellable);
			var session = yield device.get_host_session (cancellable);
			return new HostSessionEntry (device.provider, session);
		}

		private void check_open () throws Error {
			if (stop_request != null)
				throw new Error.INVALID_OPERATION ("Device manager is closed");
		}

		private async void stop_service (Cancellable? cancellable) throws IOError {
			while (stop_request != null) {
				try {
					yield stop_request.future.wait_async (cancellable);
					return;
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					cancellable.set_error_if_cancelled ();
				}
			}
			stop_request = new Promise<bool> ();

			io_cancellable.cancel ();

			try {
				if (start_request != null) {
					try {
						yield ensure_service (cancellable);
					} catch (GLib.Error e) {
						cancellable.set_error_if_cancelled ();
					}

					foreach (var device in devices.to_array ())
						yield device._do_close (APPLICATION_REQUESTED, true, cancellable);
					devices.clear ();

					yield service.stop (cancellable);
					service.provider_available.disconnect (on_provider_available);
					service.provider_unavailable.disconnect (on_provider_unavailable);
				}

				service = null;

				stop_request.resolve (true);
			} catch (IOError e) {
				stop_request.reject (e);
				stop_request = null;
				throw e;
			}
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class ManagerTask<T> : AsyncTask<T> {
			public weak DeviceManager parent {
				get;
				construct;
			}
		}

		private delegate void DeviceObserverFunc (Device device);

		private class DeviceObserverEntry {
			public DeviceObserverFunc func;

			public DeviceObserverEntry (owned DeviceObserverFunc func) {
				this.func = (owned) func;
			}
		}
	}

	/**
	 * An immutable list of {@link Device} objects.
	 */
	public sealed class DeviceList : Object {
		private Gee.List<Device> items;

		internal DeviceList (Gee.List<Device> items) {
			this.items = items;
		}

		/**
		 * Gets the number of devices in the list.
		 *
		 * @return the count
		 */
		public int size () {
			return items.size;
		}

		/**
		 * Gets the device at the given position.
		 *
		 * @param index zero-based position
		 * @return the device
		 */
		public new Device get (int index) {
			return items.get (index);
		}
	}

	/**
	 * Represents a device that Frida can interact with, such as the local
	 * system, a USB-connected phone, or a remote frida-server.
	 *
	 * Obtain one through a {@link DeviceManager}, then use it to spawn or attach
	 * to processes.
	 */
	public sealed class Device : Object {
		/**
		 * Emitted when spawn gating is disabled, either by request or because the host
		 * cancelled it, e.g. a caught process was left unresumed long enough to risk
		 * stalling process creation.
		 *
		 * @param reason why gating was disabled
		 */
		public signal void spawn_gating_disabled (SpawnGatingDisabledReason reason);
		/**
		 * Emitted when a process is spawned while spawn gating is enabled.
		 *
		 * @param spawn details of the pending spawn
		 */
		public signal void spawn_added (Spawn spawn);
		/**
		 * Emitted when a pending spawn is resumed or killed.
		 *
		 * @param spawn the spawn that is no longer pending
		 */
		public signal void spawn_removed (Spawn spawn);
		/**
		 * Emitted when a new child is observed while child gating is enabled.
		 *
		 * @param child details of the child process
		 */
		public signal void child_added (Child child);
		/**
		 * Emitted when a previously added child is resumed or has gone away.
		 *
		 * @param child the child that is no longer pending
		 */
		public signal void child_removed (Child child);
		/**
		 * Emitted when a process crashes.
		 *
		 * @param crash details of the crash
		 */
		public signal void process_crashed (Crash crash);
		/**
		 * Emitted when a spawned process writes to its standard output or error.
		 *
		 * @param pid the process ID
		 * @param fd the file descriptor written to (1 for stdout, 2 for stderr)
		 * @param data the bytes written, or an empty buffer on EOF
		 */
		public signal void output (uint pid, int fd, Bytes data);
		/**
		 * Emitted when an injected library has been unloaded.
		 *
		 * @param id the injection ID returned when the library was injected
		 */
		public signal void uninjected (uint id);
		/**
		 * Emitted while a connection to the device is being established, each
		 * time it reaches a new stage. The connection is made by the first call
		 * that needs one, so this may be emitted from any method that reaches
		 * the device.
		 *
		 * @param status what the connection is busy with, phrased for the user
		 * @param progress how far along the connection is, from 0.0 to 1.0
		 */
		public signal void connecting (string status, double progress);
		/**
		 * Emitted when a connection to the device has been established.
		 */
		public signal void connected ();
		/**
		 * Emitted when the connection to the device is lost.
		 */
		public signal void lost ();

		/**
		 * The device's stable identifier.
		 */
		public string id {
			get {
				if (_id != null)
					return _id;
				return provider.id;
			}
		}

		/**
		 * The device's human-readable name.
		 */
		public string name {
			get {
				if (_name != null)
					return _name;
				return provider.name;
			}
		}

		/**
		 * An icon representing the device, serialized as a variant, or null if
		 * none is available.
		 */
		public Variant? icon {
			get;
			construct;
		}

		/**
		 * The kind of device: local, remote, or USB.
		 */
		public DeviceType dtype {
			get {
				switch (provider.kind) {
					case HostSessionProviderKind.LOCAL:
						return DeviceType.LOCAL;
					case HostSessionProviderKind.REMOTE:
						return DeviceType.REMOTE;
					case HostSessionProviderKind.USB:
						return DeviceType.USB;
					default:
						assert_not_reached ();
				}
			}
		}

		/**
		 * The device's message bus, for exchanging messages with the device.
		 */
		public Bus bus {
			get {
				return _bus;
			}
		}

		private string? _id;
		private string? _name;
		internal HostSessionProvider provider;
		private unowned DeviceManager? manager;

		private HostSessionOptions? host_session_options;
		private Promise<HostSession>? host_session_request;
		private Promise<bool>? close_request;

		internal HostSession? current_host_session;
		private Gee.HashMap<AgentSessionId?, Session> agent_sessions =
			new Gee.HashMap<AgentSessionId?, Session> (AgentSessionId.hash, AgentSessionId.equal);
		private Gee.HashSet<Promise<Session>> pending_attach_requests = new Gee.HashSet<Promise<Session>> ();
		private Gee.HashMap<AgentSessionId?, Promise<bool>> pending_detach_requests =
			new Gee.HashMap<AgentSessionId?, Promise<bool>> (AgentSessionId.hash, AgentSessionId.equal);
		private Bus _bus;

		public delegate bool ProcessPredicate (Process process);

		internal Device (DeviceManager? mgr, HostSessionProvider prov, string? id = null, string? name = null,
				HostSessionOptions? options = null, Variant? icon = null) {
			Object (icon: icon ?? prov.icon);

			_id = id;
			_name = name;
			manager = mgr;
			host_session_options = options;

			assign_provider (prov);
		}

		construct {
			_bus = new Bus (this);
		}

		private void assign_provider (HostSessionProvider prov) {
			provider = prov;
			provider.host_session_detached.connect (on_host_session_detached);
			provider.agent_session_detached.connect (on_agent_session_detached);
		}

		/**
		 * Checks whether the connection to the device has been lost.
		 *
		 * @return true if the device is no longer reachable
		 */
		public bool is_lost () {
			return close_request != null;
		}

		/**
		 * Overrides a device-level option.
		 *
		 * @param name the option name
		 * @param val the value to set
		 */
		public void override_option (string name, Variant val) throws Error {
			Value v;
			switch (val.classify ()) {
				case BOOLEAN:
					v = Value (typeof (bool));
					v.set_boolean (val.get_boolean ());
					break;
				case INT64:
					v = Value (typeof (int64));
					v.set_int64 (val.get_int64 ());
					break;
				case UINT64:
					v = Value (typeof (uint64));
					v.set_uint64 (val.get_uint64 ());
					break;
				case DOUBLE:
					v = Value (typeof (double));
					v.set_double (val.get_double ());
					break;
				case STRING:
					v = Value (typeof (string));
					v.set_string (val.get_string ());
					break;
				default:
					throw new Error.INVALID_ARGUMENT ("Unsupported option type");
			}

			lock (host_session_options) {
				if (host_session_options == null)
					host_session_options = new HostSessionOptions ();
				host_session_options.map[name] = v;
			}
		}

		/**
		 * Queries a set of parameters describing the device and its operating
		 * system.
		 *
		 * @return a table of system parameters keyed by name
		 */
		public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				return yield host_session.query_system_parameters (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public HashTable<string, Variant> query_system_parameters_sync (Cancellable? cancellable = null) throws Error, IOError {
			return create<QuerySystemParametersTask> ().execute (cancellable);
		}

		private class QuerySystemParametersTask : DeviceTask<HashTable<string, Variant>> {
			protected override async HashTable<string, Variant> perform_operation () throws Error, IOError {
				return yield parent.query_system_parameters (cancellable);
			}
		}

		/**
		 * Gets the application currently in the foreground, if any.
		 *
		 * @param options query options shaping the returned details, or null
		 * @return the frontmost application, or null if none
		 */
		public async Application? get_frontmost_application (FrontmostQueryOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			var host_session = yield get_host_session (cancellable);

			try {
				var app = yield host_session.get_frontmost_application (raw_options, cancellable);

				if (app.pid == 0)
					return null;

				return new Application (app.identifier, app.name, app.pid, app.parameters);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public Application? get_frontmost_application_sync (FrontmostQueryOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<GetFrontmostApplicationTask> ();
			task.options = options;
			return task.execute (cancellable);
		}

		private class GetFrontmostApplicationTask : DeviceTask<Application?> {
			public FrontmostQueryOptions? options;

			protected override async Application? perform_operation () throws Error, IOError {
				return yield parent.get_frontmost_application (options, cancellable);
			}
		}

		/**
		 * Enumerates the applications installed on the device.
		 *
		 * @param options query options to scope and shape the results, or null
		 * @return the matching applications
		 */
		public async ApplicationList enumerate_applications (ApplicationQueryOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			var host_session = yield get_host_session (cancellable);

			HostApplicationInfo[] applications;
			try {
				applications = yield host_session.enumerate_applications (raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			var result = new Gee.ArrayList<Application> ();
			foreach (var app in applications)
				result.add (new Application (app.identifier, app.name, app.pid, app.parameters));
			return new ApplicationList (result);
		}

		public ApplicationList enumerate_applications_sync (ApplicationQueryOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<EnumerateApplicationsTask> ();
			task.options = options;
			return task.execute (cancellable);
		}

		private class EnumerateApplicationsTask : DeviceTask<ApplicationList> {
			public ApplicationQueryOptions? options;

			protected override async ApplicationList perform_operation () throws Error, IOError {
				return yield parent.enumerate_applications (options, cancellable);
			}
		}

		/**
		 * Gets the process with the given PID, throwing if it cannot be found.
		 *
		 * @param pid the process ID
		 * @param options matching options, such as a scope or timeout, or null
		 * @return the matching process
		 */
		public async Process get_process_by_pid (uint pid, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return check_process (yield find_process_by_pid (pid, options, cancellable));
		}

		public Process get_process_by_pid_sync (uint pid, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return check_process (find_process_by_pid_sync (pid, options, cancellable));
		}

		/**
		 * Gets the process whose name matches, throwing if none is found.
		 *
		 * @param name the process name to match
		 * @param options matching options, or null
		 * @return the matching process
		 */
		public async Process get_process_by_name (string name, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return check_process (yield find_process_by_name (name, options, cancellable));
		}

		public Process get_process_by_name_sync (string name, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return check_process (find_process_by_name_sync (name, options, cancellable));
		}

		/**
		 * Gets the first process accepted by @predicate, throwing if none is
		 * found.
		 *
		 * @param predicate function deciding whether a process matches
		 * @param options matching options, or null
		 * @return the matching process
		 */
		public async Process get_process (ProcessPredicate predicate, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return check_process (yield find_process (predicate, options, cancellable));
		}

		public Process get_process_sync (ProcessPredicate predicate, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return check_process (find_process_sync (predicate, options, cancellable));
		}

		private Process check_process (Process? process) throws Error {
			if (process == null)
				throw new Error.INVALID_ARGUMENT ("Process not found");
			return process;
		}

		/**
		 * Finds the process with the given PID.
		 *
		 * @param pid the process ID
		 * @param options matching options, or null
		 * @return the process, or null if not found
		 */
		public async Process? find_process_by_pid (uint pid, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return yield find_process ((process) => { return process.pid == pid; }, options, cancellable);
		}

		public Process? find_process_by_pid_sync (uint pid, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			return find_process_sync ((process) => { return process.pid == pid; }, options, cancellable);
		}

		/**
		 * Finds a process whose name matches.
		 *
		 * @param name the process name to match
		 * @param options matching options, or null
		 * @return the matching process, or null if not found
		 */
		public async Process? find_process_by_name (string name, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var folded_name = name.casefold ();
			return yield find_process ((process) => { return process.name.casefold () == folded_name; }, options, cancellable);
		}

		public Process? find_process_by_name_sync (string name, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var folded_name = name.casefold ();
			return find_process_sync ((process) => { return process.name.casefold () == folded_name; }, options, cancellable);
		}

		/**
		 * Finds the first process accepted by @predicate.
		 *
		 * @param predicate function deciding whether a process matches
		 * @param options matching options, or null
		 * @return the matching process, or null if not found
		 */
		public async Process? find_process (ProcessPredicate predicate, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			Process? process = null;
			bool done = false;
			bool waiting = false;
			var main_context = MainContext.get_thread_default ();

			ProcessMatchOptions opts = (options != null) ? options : new ProcessMatchOptions ();
			int timeout = opts.timeout;

			ProcessQueryOptions enumerate_options = new ProcessQueryOptions ();
			enumerate_options.scope = opts.scope;

			Source? timeout_source = null;
			if (timeout > 0) {
				timeout_source = new TimeoutSource (timeout);
				timeout_source.set_callback (() => {
					done = true;
					if (waiting)
						find_process.callback ();
					return false;
				});
				timeout_source.attach (main_context);
			}

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				done = true;
				if (waiting)
					find_process.callback ();
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			try {
				while (!done) {
					var processes = yield enumerate_processes (enumerate_options, cancellable);

					var num_processes = processes.size ();
					for (var i = 0; i != num_processes; i++) {
						var p = processes.get (i);
						if (predicate (p)) {
							process = p;
							break;
						}
					}

					if (process != null || done || timeout == 0)
						break;

					var delay_source = new TimeoutSource (500);
					delay_source.set_callback (find_process.callback);
					delay_source.attach (main_context);

					waiting = true;
					yield;
					waiting = false;

					delay_source.destroy ();
				}
			} finally {
				cancel_source.destroy ();

				if (timeout_source != null)
					timeout_source.destroy ();
			}

			return process;
		}

		public Process? find_process_sync (ProcessPredicate predicate, ProcessMatchOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<FindProcessTask> ();
			task.predicate = (process) => {
				return predicate (process);
			};
			task.options = options;
			return task.execute (cancellable);
		}

		private class FindProcessTask : DeviceTask<Process?> {
			public ProcessPredicate predicate;
			public ProcessMatchOptions? options;

			protected override async Process? perform_operation () throws Error, IOError {
				return yield parent.find_process (predicate, options, cancellable);
			}
		}

		/**
		 * Enumerates the processes running on the device.
		 *
		 * @param options query options to scope and shape the results, or null
		 * @return the matching processes
		 */
		public async ProcessList enumerate_processes (ProcessQueryOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			var host_session = yield get_host_session (cancellable);

			HostProcessInfo[] processes;
			try {
				processes = yield host_session.enumerate_processes (raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			var result = new Gee.ArrayList<Process> ();
			foreach (var p in processes)
				result.add (new Process (p.pid, p.name, p.parameters));
			return new ProcessList (result);
		}

		public ProcessList enumerate_processes_sync (ProcessQueryOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<EnumerateProcessesTask> ();
			task.options = options;
			return task.execute (cancellable);
		}

		private class EnumerateProcessesTask : DeviceTask<ProcessList> {
			public ProcessQueryOptions? options;

			protected override async ProcessList perform_operation () throws Error, IOError {
				return yield parent.enumerate_processes (options, cancellable);
			}
		}

		/**
		 * Enables spawn gating, suspending every newly spawned process until it
		 * is explicitly resumed.
		 *
		 * @param options optional settings, e.g. how much to gate
		 */
		public async void enable_spawn_gating (SpawnGatingOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var scope = (options != null) ? options.scope : SpawnGatingScope.DEFAULT;
			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			var host_session = yield get_host_session (cancellable);

			try {
				yield host_session.enable_spawn_gating_with_options (raw_options, cancellable);
			} catch (GLib.Error e) {
				DBusError.strip_remote_error (e);
				if (scope != DEFAULT || !(e is DBusError.UNKNOWN_METHOD))
					throw_dbus_error (e);

				// Older remote without the scoped call; fall back to the plain one (that's DEFAULT).
				try {
					yield host_session.enable_spawn_gating (cancellable);
				} catch (GLib.Error legacy_error) {
					throw_dbus_error (legacy_error);
				}
			}
		}

		public void enable_spawn_gating_sync (SpawnGatingOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<EnableSpawnGatingTask> ();
			task.options = options;
			task.execute (cancellable);
		}

		private class EnableSpawnGatingTask : DeviceTask<void> {
			public SpawnGatingOptions? options;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.enable_spawn_gating (options, cancellable);
			}
		}

		/**
		 * Disables spawn gating.
		 */
		public async void disable_spawn_gating (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				yield host_session.disable_spawn_gating (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void disable_spawn_gating_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<DisableSpawnGatingTask> ().execute (cancellable);
		}

		private class DisableSpawnGatingTask : DeviceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.disable_spawn_gating (cancellable);
			}
		}

		/**
		 * Enumerates the spawns currently suspended by spawn gating.
		 *
		 * @return the pending spawns
		 */
		public async SpawnList enumerate_pending_spawn (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			HostSpawnInfo[] pending_spawn;
			try {
				pending_spawn = yield host_session.enumerate_pending_spawn (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			var result = new Gee.ArrayList<Spawn> ();
			foreach (var p in pending_spawn)
				result.add (Spawn.from_info (p));
			return new SpawnList (result);
		}

		public SpawnList enumerate_pending_spawn_sync (Cancellable? cancellable = null) throws Error, IOError {
			return create<EnumeratePendingSpawnTask> ().execute (cancellable);
		}

		private class EnumeratePendingSpawnTask : DeviceTask<SpawnList> {
			protected override async SpawnList perform_operation () throws Error, IOError {
				return yield parent.enumerate_pending_spawn (cancellable);
			}
		}

		/**
		 * Enumerates the children currently suspended by child gating.
		 *
		 * @return the pending children
		 */
		public async ChildList enumerate_pending_children (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			HostChildInfo[] pending_children;
			try {
				pending_children = yield host_session.enumerate_pending_children (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			var result = new Gee.ArrayList<Child> ();
			foreach (var p in pending_children)
				result.add (Child.from_info (p));
			return new ChildList (result);
		}

		public ChildList enumerate_pending_children_sync (Cancellable? cancellable = null) throws Error, IOError {
			return create<EnumeratePendingChildrenTask> ().execute (cancellable);
		}

		private class EnumeratePendingChildrenTask : DeviceTask<ChildList> {
			protected override async ChildList perform_operation () throws Error, IOError {
				return yield parent.enumerate_pending_children (cancellable);
			}
		}

		/**
		 * Starts a new process in a suspended state, ready to be resumed once
		 * instrumentation is in place.
		 *
		 * @param program path or identifier of the program to launch
		 * @param options spawn options such as argv, env, and stdio, or null
		 * @return the PID of the newly created process
		 */
		public async uint spawn (string program, SpawnOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			var raw_options = HostSpawnOptions ();
			if (options != null) {
				var argv = options.argv;
				if (argv != null) {
					raw_options.has_argv = true;
					raw_options.argv = argv;
				}

				var envp = options.envp;
				if (envp != null) {
					raw_options.has_envp = true;
					raw_options.envp = envp;
				}

				var env = options.env;
				if (env != null) {
					raw_options.has_env = true;
					raw_options.env = env;
				}

				var cwd = options.cwd;
				if (cwd != null)
					raw_options.cwd = cwd;

				raw_options.stdio = options.stdio;

				raw_options.aux = options.aux;
			}

			var host_session = yield get_host_session (cancellable);

			uint pid;
			try {
				pid = yield host_session.spawn (program, raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			return pid;
		}

		public uint spawn_sync (string program, SpawnOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<SpawnTask> ();
			task.program = program;
			task.options = options;
			return task.execute (cancellable);
		}

		private class SpawnTask : DeviceTask<uint> {
			public string program;
			public SpawnOptions? options;

			protected override async uint perform_operation () throws Error, IOError {
				return yield parent.spawn (program, options, cancellable);
			}
		}

		/**
		 * Writes data to the standard input of a spawned process.
		 *
		 * @param pid the process ID
		 * @param data the bytes to write
		 */
		public async void input (uint pid, Bytes data, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				yield host_session.input (pid, data.get_data (), cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void input_sync (uint pid, Bytes data, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<InputTask> ();
			task.pid = pid;
			task.data = data;
			task.execute (cancellable);
		}

		private class InputTask : DeviceTask<void> {
			public uint pid;
			public Bytes data;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.input (pid, data, cancellable);
			}
		}

		/**
		 * Resumes a process that was spawned in a suspended state, or
		 * suspended via spawn gating.
		 *
		 * @param pid the process ID to resume
		 */
		public async void resume (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				yield host_session.resume (pid, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void resume_sync (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<ResumeTask> ();
			task.pid = pid;
			task.execute (cancellable);
		}

		private class ResumeTask : DeviceTask<void> {
			public uint pid;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.resume (pid, cancellable);
			}
		}

		/**
		 * Terminates a process.
		 *
		 * @param pid the process ID to kill
		 */
		public async void kill (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				yield host_session.kill (pid, cancellable);
			} catch (GLib.Error e) {
				/* The process being killed might be the other end of the connection. */
				if (!(e is IOError.CLOSED))
					throw_dbus_error (e);
			}
		}

		public void kill_sync (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<KillTask> ();
			task.pid = pid;
			task.execute (cancellable);
		}

		private class KillTask : DeviceTask<void> {
			public uint pid;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.kill (pid, cancellable);
			}
		}

		/**
		 * Attaches to a process, giving a {@link Session} through which scripts
		 * can be created and run inside it.
		 *
		 * @param pid the process ID to attach to
		 * @param options session options, such as the realm or persistence
		 *   timeout, or null
		 * @return the new session
		 */
		public async Session attach (uint pid, SessionOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			SessionOptions opts = (options != null) ? options : new SessionOptions ();

			var attach_request = new Promise<Session> ();
			pending_attach_requests.add (attach_request);

			Session session = null;
			try {
				var host_session = yield get_host_session (cancellable);

				var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

				AgentSessionId id;
				try {
					id = yield host_session.attach (pid, raw_options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				try {
					session = new Session (this, pid, id, opts);
					session.active_session = yield provider.link_agent_session (host_session, id, session, cancellable);
					agent_sessions[id] = session;

					attach_request.resolve (session);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			} catch (Error e) {
				attach_request.reject (e);
				throw e;
			} catch (IOError e) {
				attach_request.reject (e);
				throw e;
			} finally {
				pending_attach_requests.remove (attach_request);
			}

			return session;
		}

		public Session attach_sync (uint pid, SessionOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<AttachTask> ();
			task.pid = pid;
			task.options = options;
			return task.execute (cancellable);
		}

		private class AttachTask : DeviceTask<Session> {
			public uint pid;
			public SessionOptions? options;

			protected override async Session perform_operation () throws Error, IOError {
				return yield parent.attach (pid, options, cancellable);
			}
		}

		/**
		 * Injects a shared library from a file into a process.
		 *
		 * @param pid the process ID to inject into
		 * @param path path to the library on the device
		 * @param entrypoint name of the entrypoint function to call
		 * @param data a string passed to the entrypoint
		 * @return an injection ID, later matched by {@link Device.uninjected}
		 */
		public async uint inject_library_file (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				var id = yield host_session.inject_library_file (pid, path, entrypoint, data, cancellable);

				return id.handle;
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public uint inject_library_file_sync (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<InjectLibraryFileTask> ();
			task.pid = pid;
			task.path = path;
			task.entrypoint = entrypoint;
			task.data = data;
			return task.execute (cancellable);
		}

		private class InjectLibraryFileTask : DeviceTask<uint> {
			public uint pid;
			public string path;
			public string entrypoint;
			public string data;

			protected override async uint perform_operation () throws Error, IOError {
				return yield parent.inject_library_file (pid, path, entrypoint, data, cancellable);
			}
		}

		/**
		 * Injects a shared library from an in-memory blob into a process.
		 *
		 * @param pid the process ID to inject into
		 * @param blob the library image
		 * @param entrypoint name of the entrypoint function to call
		 * @param data a string passed to the entrypoint
		 * @return an injection ID, later matched by {@link Device.uninjected}
		 */
		public async uint inject_library_blob (uint pid, Bytes blob, string entrypoint, string data,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			try {
				var id = yield host_session.inject_library_blob (pid, blob.get_data (), entrypoint, data, cancellable);

				return id.handle;
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public uint inject_library_blob_sync (uint pid, Bytes blob, string entrypoint, string data, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<InjectLibraryBlobTask> ();
			task.pid = pid;
			task.blob = blob;
			task.entrypoint = entrypoint;
			task.data = data;
			return task.execute (cancellable);
		}

		private class InjectLibraryBlobTask : DeviceTask<uint> {
			public uint pid;
			public Bytes blob;
			public string entrypoint;
			public string data;

			protected override async uint perform_operation () throws Error, IOError {
				return yield parent.inject_library_blob (pid, blob, entrypoint, data, cancellable);
			}
		}

		/**
		 * Opens a raw communication channel to the given address on the device.
		 *
		 * @param address the address to connect to
		 * @return a stream for reading from and writing to the channel
		 */
		public async IOStream open_channel (string address, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			ChannelId id;
			try {
				id = yield host_session.open_channel (address, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			return yield provider.link_channel (host_session, id, cancellable);
		}

		public IOStream open_channel_sync (string address, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<OpenChannelTask> ();
			task.address = address;
			return task.execute (cancellable);
		}

		private class OpenChannelTask : DeviceTask<IOStream> {
			public string address;

			protected override async IOStream perform_operation () throws Error, IOError {
				return yield parent.open_channel (address, cancellable);
			}
		}

		/**
		 * Opens a service identified by the given address.
		 *
		 * @param address the service address
		 * @return the opened service
		 */
		public async Service open_service (string address, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var host_session = yield get_host_session (cancellable);

			ServiceSessionId id;
			try {
				id = yield host_session.open_service (address, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			var service_session = yield provider.link_service_session (host_session, id, cancellable);

			return new Service (service_session);
		}

		public Service open_service_sync (string address, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<OpenServiceTask> ();
			task.address = address;
			return task.execute (cancellable);
		}

		private class OpenServiceTask : DeviceTask<Service> {
			public string address;

			protected override async Service perform_operation () throws Error, IOError {
				return yield parent.open_service (address, cancellable);
			}
		}

		/**
		 * Removes any persistent pairing record this host has with the device.
		 */
		public async void unpair (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			var pairable = provider as Pairable;
			if (pairable == null)
				throw new Error.NOT_SUPPORTED ("Pairing functionality is not supported by this device");

			yield pairable.unpair (cancellable);
		}

		public void unpair_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<UnpairTask> ().execute (cancellable);
		}

		private class UnpairTask : DeviceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.unpair (cancellable);
			}
		}

		private void check_open () throws Error {
			if (close_request != null)
				throw new Error.INVALID_OPERATION ("Device is gone");
		}

		internal async HostSession get_host_session (Cancellable? cancellable) throws Error, IOError {
			while (host_session_request != null) {
				try {
					return yield host_session_request.future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			host_session_request = new Promise<HostSession> ();

			try {
				HostSessionOptions? opts;
				lock (host_session_options)
					opts = (host_session_options != null) ? host_session_options.copy () : null;

				var session = yield provider.create (manager, opts, (status, progress) => connecting (status, progress), cancellable);
				attach_host_session (session);

				current_host_session = session;
				host_session_request.resolve (session);

				connected ();

				return session;
			} catch (GLib.Error e) {
				host_session_request.reject (e);
				host_session_request = null;

				throw_api_error (e);
			}
		}

		private void on_host_session_detached (HostSession session) {
			if (session != current_host_session)
				return;

			_bus._detach.begin (session);

			detach_host_session (session);

			current_host_session = null;
			host_session_request = null;
		}

		private void attach_host_session (HostSession session) {
			session.spawn_gating_disabled.connect (on_spawn_gating_disabled);
			session.spawn_added.connect (on_spawn_added);
			session.spawn_removed.connect (on_spawn_removed);
			session.child_added.connect (on_child_added);
			session.child_removed.connect (on_child_removed);
			session.process_crashed.connect (on_process_crashed);
			session.output.connect (on_output);
			session.uninjected.connect (on_uninjected);
		}

		private void detach_host_session (HostSession session) {
			session.spawn_gating_disabled.disconnect (on_spawn_gating_disabled);
			session.spawn_added.disconnect (on_spawn_added);
			session.spawn_removed.disconnect (on_spawn_removed);
			session.child_added.disconnect (on_child_added);
			session.child_removed.disconnect (on_child_removed);
			session.process_crashed.disconnect (on_process_crashed);
			session.output.disconnect (on_output);
			session.uninjected.disconnect (on_uninjected);
		}

		internal async void _do_close (SessionDetachReason reason, bool may_block, Cancellable? cancellable) throws IOError {
			while (close_request != null) {
				try {
					yield close_request.future.wait_async (cancellable);
					return;
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					cancellable.set_error_if_cancelled ();
				}
			}
			close_request = new Promise<bool> ();

			try {
				while (!pending_detach_requests.is_empty) {
					var iterator = pending_detach_requests.entries.iterator ();
					iterator.next ();
					var entry = iterator.get ();

					var session_id = entry.key;
					var detach_request = entry.value;

					detach_request.resolve (true);
					pending_detach_requests.unset (session_id);
				}

				while (!pending_attach_requests.is_empty) {
					var iterator = pending_attach_requests.iterator ();
					iterator.next ();
					var attach_request = iterator.get ();
					try {
						yield attach_request.future.wait_async (cancellable);
					} catch (GLib.Error e) {
						cancellable.set_error_if_cancelled ();
					}
				}

				if (host_session_request != null) {
					try {
						yield get_host_session (cancellable);
					} catch (Error e) {
					}
				}

				var no_crash = CrashInfo.empty ();
				foreach (var session in agent_sessions.values.to_array ())
					yield session._do_close (reason, no_crash, may_block, cancellable);
				agent_sessions.clear ();

				provider.host_session_detached.disconnect (on_host_session_detached);
				provider.agent_session_detached.disconnect (on_agent_session_detached);

				if (current_host_session != null) {
					detach_host_session (current_host_session);

					if (may_block) {
						try {
							yield provider.destroy (current_host_session, cancellable);
						} catch (Error e) {
						}
					}

					current_host_session = null;
					host_session_request = null;
				}

				if (manager != null)
					manager._release_device (this);

				lost ();

				close_request.resolve (true);
			} catch (IOError e) {
				close_request.reject (e);
				close_request = null;
				throw e;
			}
		}

		internal async void _release_session (Session session, bool may_block, Cancellable? cancellable) throws IOError {
			AgentSessionId? session_id = null;
			foreach (var entry in agent_sessions.entries) {
				if (entry.value == session) {
					session_id = entry.key;
					break;
				}
			}
			if (session_id == null)
				return;

			agent_sessions.unset (session_id);

			if (may_block) {
				var detach_request = new Promise<bool> ();

				pending_detach_requests[session_id] = detach_request;

				try {
					yield detach_request.future.wait_async (cancellable);
				} catch (Error e) {
					assert_not_reached ();
				}
			}
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			var session = agent_sessions[id];
			if (session != null)
				session._on_detached (reason, crash);

			Promise<bool> detach_request;
			if (pending_detach_requests.unset (id, out detach_request))
				detach_request.resolve (true);
			else if (session != null)
				agent_sessions.unset (id);
		}

		private void on_spawn_gating_disabled (SpawnGatingDisabledReason reason) {
			spawn_gating_disabled (reason);
		}

		private void on_spawn_added (HostSpawnInfo info) {
			spawn_added (Spawn.from_info (info));
		}

		private void on_spawn_removed (HostSpawnInfo info) {
			spawn_removed (Spawn.from_info (info));
		}

		private void on_child_added (HostChildInfo info) {
			child_added (Child.from_info (info));
		}

		private void on_child_removed (HostChildInfo info) {
			child_removed (Child.from_info (info));
		}

		private void on_process_crashed (CrashInfo info) {
			process_crashed (Crash.from_info (info));
		}

		private void on_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, new Bytes (data));
		}

		private void on_uninjected (InjectorPayloadId id) {
			uninjected (id.handle);
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class DeviceTask<T> : AsyncTask<T> {
			public weak Device parent {
				get;
				construct;
			}
		}
	}

	/**
	 * The kind of a {@link Device}.
	 */
	public enum DeviceType {
		/**
		 * The local system.
		 */
		LOCAL,
		/**
		 * A device reached over the network.
		 */
		REMOTE,
		/**
		 * A USB-connected device.
		 */
		USB;

		public static DeviceType from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<DeviceType> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<DeviceType> (this);
		}
	}

	/**
	 * Options for connecting to a remote device with
	 * {@link DeviceManager.add_remote_device}.
	 */
	public sealed class RemoteDeviceOptions : Object {
		/**
		 * TLS certificate to present, for connections that require one.
		 */
		public TlsCertificate? certificate {
			get;
			set;
		}

		/**
		 * Origin to send when connecting, if the server checks it.
		 */
		public string? origin {
			get;
			set;
		}

		/**
		 * Authentication token to present to the server.
		 */
		public string? token {
			get;
			set;
		}

		/**
		 * Interval between keepalive messages, in seconds, or -1 to disable.
		 */
		public int keepalive_interval {
			get;
			set;
			default = -1;
		}
	}

	/**
	 * How a Barebone device presents itself. Pass it to
	 * {@link DeviceManager.add_barebone_device}.
	 */
	public sealed class BareboneDeviceOptions : Object {
		/**
		 * A stable identifier to give the device, or null to generate one. A caller
		 * that derives it from something it persists gets the same id back every
		 * time it adds the device, even across a restart of its own, so anything
		 * that refers to the target by device id stays valid.
		 */
		public string? id {
			get;
			set;
		}

		/**
		 * What to call the device, or null to name it after the backend.
		 */
		public string? name {
			get;
			set;
		}

		/**
		 * Icon to represent the device with, built by {@link icon_from_png} or
		 * {@link icon_from_rgba}, or null to use the backend's own.
		 */
		public Variant? icon {
			get;
			set;
		}
	}

	/**
	 * Builds an icon out of a PNG image, in the shape {@link Device.icon} has.
	 */
	public Variant icon_from_png (uint8[] png, uint16 width, uint16 height) {
		return make_icon ("png", png, width, height);
	}

	/**
	 * Builds an icon out of premultiplied RGBA pixels, in the shape
	 * {@link Device.icon} has.
	 */
	public Variant icon_from_rgba (uint8[] pixels, uint16 width, uint16 height) {
		return make_icon ("rgba", pixels, width, height);
	}

	private Variant make_icon (string format, uint8[] data, uint16 width, uint16 height) {
		var image = new Bytes (data);
		var builder = new VariantBuilder (VariantType.VARDICT);
		builder.add ("{sv}", "format", new Variant.string (format));
		builder.add ("{sv}", "width", new Variant.uint16 (width));
		builder.add ("{sv}", "height", new Variant.uint16 (height));
		builder.add ("{sv}", "image", Variant.new_from_data (new VariantType ("ay"), image.get_data (), true, image));
		return builder.end ();
	}

	/**
	 * Barebone backend configuration. Pass it to
	 * {@link DeviceManager.add_barebone_device} to add a device of your own.
	 *
	 * The default Barebone device, whose ID is 'barebone', is instead configured through the
	 * FRIDA_BAREBONE_CONFIG environment variable, which should point to the filesystem path of a
	 * JSON-encoded configuration file.
	 *
	 * Example JSON configurations:
	 *
	 * 1. Using all defaults:
	 * {
	 *   "connection": {
	 *     "host": "127.0.0.1",
	 *     "port": 3333
	 *   }
	 * }
	 *
	 * 2. Using a physical memory allocator:
	 *  {
	 *    "connection": {
	 *      "host": "127.0.0.1",
	 *      "port": 9000
	 *    },
	 *    "allocator": {
	 *      "mode": "physical",
	 *      "physical_base": "0x8ec9b4000"
	 *    }
	 *  }
	 *
	 * 3. Using target-specific allocation functions:
	 *  {
	 *    "connection": {
	 *      "host": "127.0.0.1",
	 *      "port": 9000
	 *    },
	 *    "allocator": {
	 *      "mode": "target-functions",
	 *      "alloc_function": "0xfffffff007a3c278",
	 *      "free_function": "0xfffffff007a3c338"
	 *    }
	 *  }
	 *
	 * 4. Spelling out the arguments those functions take, for the ones that don't take the
	 *    size first, such as NT's ExAllocatePoolWithTag(pool_type, size, tag):
	 *  {
	 *    "allocator": {
	 *      "mode": "target-functions",
	 *      "alloc_function": "0x804e1000",
	 *      "alloc_arguments": [ "0", "size", "0x64697246" ],
	 *      "free_function": "0x804e2000",
	 *      "free_arguments": [ "address", "0x64697246" ]
	 *    }
	 *  }
	 *
	 * 5. Injecting a remote agent:
	 *  {
	 *    "agent": {
	 *      "path": "/path/to/target/aarch64-unknown-none/release/frida-barebone-agent",
	 *      "transport": {
	 *        "type": "hostlink",
	 *        "qmp": "unix:/path/to/qmp.sock",
	 *        "bus": "frida-vserial.0",
	 *        "mmio": "0xa003e00",
	 *        "irq": 47
	 *      }
	 *    },
	 *    "image": {
	 *      "file": "/path/to/kernelcache.research.iphone12b",
	 *      "base": "0xfffffff007004000"
	 *    }
	 *  }
	 */
	public sealed class BareboneConfig : Object, Json.Serializable {
		public BareboneConnectionConfig connection {
			get;
			set;
			default = new BareboneConnectionConfig ();
		}

		public BareboneAllocatorConfig? allocator {
			get;
			set;
		}

		public BareboneAgentConfig? agent {
			get;
			set;
		}

		public BareboneImageConfig? image {
			get;
			set;
		}

		public BareboneKernelKind kernel {
			get;
			set;
			default = AUTO;
		}

		public void check () throws Error {
			connection.check ();
			if (allocator != null)
				allocator.check ();
			if (agent != null)
				agent.check ();
			if (image != null)
				image.check ();
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "allocator") {
				BareboneAllocatorConfig? allocator = null;
				Type t = typeof (BareboneInvalidAllocatorConfig);
				if (property_node.get_node_type () == Json.NodeType.OBJECT) {
					switch (property_node.get_object ().get_string_member_with_default ("mode", "invalid")) {
					case "physical":
						t = typeof (BarebonePhysicalAllocatorConfig);
						break;
					case "target-functions":
						t = typeof (BareboneTargetFunctionsAllocatorConfig);
						break;
					default:
						break;
					}
					allocator = (BareboneAllocatorConfig) Json.gobject_deserialize (t, property_node);
				} else {
					allocator = new BareboneInvalidAllocatorConfig ();
				}

				var v = Value (t);
				v.set_object (allocator);
				value = v;
				return true;
			}

			if (property_name == "agent") {
				BareboneAgentConfig agent = new BareboneInvalidAgentConfig ();
				if (property_node.get_node_type () == Json.NodeType.OBJECT) {
					Type t = typeof (BareboneInvalidAgentConfig);
					switch (property_node.get_object ().get_string_member_with_default ("type", "invalid")) {
					case "injected":
						t = typeof (BareboneInjectedAgentConfig);
						break;
					case "resident":
						t = typeof (BareboneResidentAgentConfig);
						break;
					default:
						break;
					}
					agent = (BareboneAgentConfig) Json.gobject_deserialize (t, property_node);
				}
				var v = Value (typeof (BareboneAgentConfig));
				v.set_object (agent);
				value = v;
				return true;
			}

			if (property_name == "kernel") {
				var v = Value (typeof (BareboneKernelKind));
				v.set_enum (parse_kernel_kind (property_node.get_string ()));
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}

		private static BareboneKernelKind parse_kernel_kind (string? name) {
			switch (name) {
				case "bare":	return BareboneKernelKind.BARE;
				case "win9x":	return BareboneKernelKind.WIN9X;
				case "winnt":	return BareboneKernelKind.WINNT;
				case "xnu":	return BareboneKernelKind.XNU;
				case "linux":	return BareboneKernelKind.LINUX;
				default:	return BareboneKernelKind.AUTO;
			}
		}
	}

	/**
	 * Selects which bring-up dance the target kernel needs before the agent can be
	 * injected. AUTO infers XNU when an image is configured, and BARE otherwise.
	 */
	public enum BareboneKernelKind {
		AUTO,
		BARE,
		WIN9X,
		WINNT,
		XNU,
		LINUX
	}

	public sealed class BareboneConnectionConfig : Object, Json.Serializable {
		public string host {
			get;
			set;
			default = "127.0.0.1";
		}

		public uint16 port {
			get;
			set;
			default = 3333;
		}

		/**
		 * The local process hosting the stub, when there is one. Used to read guest physical
		 * memory for the VZ flavor, and to instrument the Android emulator's HVF-backed gdbstub
		 * (which is otherwise unusable) for the GDB_REMOTE flavor. Left 0 when the stub is remote.
		 */
		public uint pid {
			get;
			set;
			default = 0;
		}

		public BareboneStubFlavor flavor {
			get;
			set;
			default = GDB_REMOTE;
		}

		public void check () throws Error {
			if ((flavor == VZ || flavor == ANDROID_EMULATOR) && pid == 0)
				throw new Error.INVALID_ARGUMENT (
					"Config for 'connection.pid' is required to reach the stub's hosting process");
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "flavor") {
				var v = Value (typeof (BareboneStubFlavor));
				v.set_enum (parse_stub_flavor (property_node.get_string ()));
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}

		private static BareboneStubFlavor parse_stub_flavor (string? name) {
			switch (name) {
				case "vz":			return BareboneStubFlavor.VZ;
				case "parallels":		return BareboneStubFlavor.PARALLELS;
				case "android-emulator":	return BareboneStubFlavor.ANDROID_EMULATOR;
				default:			return BareboneStubFlavor.GDB_REMOTE;
			}
		}
	}

	/**
	 * Selects which stub the backend is talking to, and thereby what the connection needs.
	 * GDB_REMOTE drives the generic client (QEMU, Corellium, debugserver) over host/port. VZ
	 * drives the Apple Virtualization.framework kernel stub, whose lldb-flavoured quirks the
	 * generic client cannot handle, and reads guest physical memory from the local process at
	 * pid. PARALLELS drives the debug server of Parallels Desktop. ANDROID_EMULATOR speaks the
	 * generic dialect over host/port but its gdbstub is unusable until instrumented, so it too
	 * requires the local process at pid.
	 */
	public enum BareboneStubFlavor {
		GDB_REMOTE,
		VZ,
		PARALLELS,
		ANDROID_EMULATOR
	}

	public abstract class BareboneAllocatorConfig : Object {
		public abstract void check () throws Error;
	}

	public sealed class BareboneInvalidAllocatorConfig : BareboneAllocatorConfig {
		public override void check () throws Error {
			throw new Error.NOT_SUPPORTED ("Config for 'allocator' is invalid");
		}
	}

	public sealed class BarebonePhysicalAllocatorConfig : BareboneAllocatorConfig, Json.Serializable {
		public BareboneMemoryAddress physical_base {
			get;
			set;
		}

		public override void check () throws Error {
			if (physical_base == null)
				throw new Error.NOT_SUPPORTED ("Config for 'allocator.physical_base' is missing");
			physical_base.check ();
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "physical_base") {
				value = BareboneMemoryAddress.deserialize ("allocator.physical_base", property_node);
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	public sealed class BareboneTargetFunctionsAllocatorConfig : BareboneAllocatorConfig, Json.Serializable {
		public BareboneMemoryAddress alloc_function {
			get;
			set;
		}

		public BareboneMemoryAddress free_function {
			get;
			set;
		}

		/** Extra second argument passed to alloc_function. Modern XNU data allocators are
		 * kalloc_data_external(size, flags); QEMU's kalloc(size) ignores it. Defaults to zero. */
		public uint64 alloc_flags {
			get;
			set;
			default = 0;
		}

		/** Full argument list for alloc_function, for allocators that don't take the size first.
		 * NT's ExAllocatePoolWithTag(pool_type, size, tag) is ["0", "size", "0x64697246"].
		 * Defaults to ["size", alloc_flags]. */
		internal Gee.List<BareboneCallArgument>? alloc_arguments {
			get;
			set;
		}

		/** Full argument list for free_function. Defaults to ["address", "size"]. */
		internal Gee.List<BareboneCallArgument>? free_arguments {
			get;
			set;
		}

		/**
		 * Removes any configured arguments for alloc_function, restoring the default of
		 * ["size", alloc_flags].
		 */
		public void clear_alloc_arguments () {
			alloc_arguments = null;
		}

		/**
		 * Adds an argument to pass to alloc_function, for allocators that don't take the size
		 * first. NT's ExAllocatePoolWithTag(pool_type, size, tag) is ["0", "size", "0x64697246"].
		 *
		 * @param argument the argument to add
		 */
		public void add_alloc_argument (BareboneCallArgument argument) {
			if (alloc_arguments == null)
				alloc_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			alloc_arguments.add (argument);
		}

		/**
		 * Invokes @func for each configured alloc_function argument.
		 *
		 * @param func function called with each argument
		 */
		public void enumerate_alloc_arguments (Func<BareboneCallArgument> func) {
			if (alloc_arguments == null)
				return;
			foreach (var argument in alloc_arguments)
				func (argument);
		}

		/**
		 * Removes any configured arguments for free_function, restoring the default of
		 * ["address", "size"].
		 */
		public void clear_free_arguments () {
			free_arguments = null;
		}

		/**
		 * Adds an argument to pass to free_function.
		 *
		 * @param argument the argument to add
		 */
		public void add_free_argument (BareboneCallArgument argument) {
			if (free_arguments == null)
				free_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			free_arguments.add (argument);
		}

		/**
		 * Invokes @func for each configured free_function argument.
		 *
		 * @param func function called with each argument
		 */
		public void enumerate_free_arguments (Func<BareboneCallArgument> func) {
			if (free_arguments == null)
				return;
			foreach (var argument in free_arguments)
				func (argument);
		}

		public override void check () throws Error {
			if (alloc_function == null)
				throw new Error.NOT_SUPPORTED ("Config for 'allocator.alloc_function' is missing");
			alloc_function.check ();

			if (free_function == null)
				throw new Error.NOT_SUPPORTED ("Config for 'allocator.free_function' is missing");
			free_function.check ();

			check_arguments ("allocator.alloc_arguments", alloc_arguments, SIZE);
			check_arguments ("allocator.free_arguments", free_arguments, ADDRESS);
		}

		public Gee.List<BareboneCallArgument> _effective_alloc_arguments () {
			if (alloc_arguments != null)
				return alloc_arguments;

			var arguments = new Gee.ArrayList<BareboneCallArgument> ();
			arguments.add (new BareboneCallArgument (SIZE, 0));
			arguments.add (new BareboneCallArgument (LITERAL, alloc_flags));
			return arguments;
		}

		public Gee.List<BareboneCallArgument> _effective_free_arguments () {
			if (free_arguments != null)
				return free_arguments;

			var arguments = new Gee.ArrayList<BareboneCallArgument> ();
			arguments.add (new BareboneCallArgument (ADDRESS, 0));
			arguments.add (new BareboneCallArgument (SIZE, 0));
			return arguments;
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "alloc-function") {
				value = BareboneMemoryAddress.deserialize ("allocator.alloc_function", property_node);
				return true;
			}

			if (property_name == "free-function") {
				value = BareboneMemoryAddress.deserialize ("allocator.free_function", property_node);
				return true;
			}

			if (property_name == "alloc-arguments" || property_name == "free-arguments") {
				value = deserialize_arguments (property_node);
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}

		private static void check_arguments (string label, Gee.List<BareboneCallArgument>? arguments, BareboneCallArgumentRole required)
				throws Error {
			if (arguments == null)
				return;

			if (arguments.is_empty)
				throw new Error.NOT_SUPPORTED ("Config for '%s' is invalid", label);

			foreach (BareboneCallArgument a in arguments) {
				if (a.role == required)
					return;
			}

			throw new Error.NOT_SUPPORTED ("Config for '%s' must mention '%s'", label,
				(required == SIZE) ? "size" : "address");
		}

		private static Value deserialize_arguments (Json.Node node) {
			Gee.List<BareboneCallArgument>? arguments = null;

			if (node.get_node_type () == Json.NodeType.ARRAY) {
				arguments = new Gee.ArrayList<BareboneCallArgument> ();

				node.get_array ().foreach_element ((array, index, element) => {
					if (arguments == null)
						return;

					BareboneCallArgument? a = BareboneCallArgument.parse (element);
					if (a == null)
						arguments = null;
					else
						arguments.add (a);
				});
			}

			var v = Value (typeof (Gee.List));
			v.set_object (arguments);
			return v;
		}
	}

	/**
	 * One argument in an allocator's argument list: either the requested size, the address being
	 * freed, or a constant the target function needs in that slot.
	 */
	public sealed class BareboneCallArgument : Object {
		public BareboneCallArgumentRole role {
			get;
			construct;
		}

		public uint64 value {
			get;
			construct;
		}

		public BareboneCallArgument (BareboneCallArgumentRole role, uint64 value) {
			Object (role: role, value: value);
		}

		internal static BareboneCallArgument? parse (Json.Node node) {
			if (node.get_value_type () == typeof (string)) {
				unowned string text = node.get_string ();
				if (text == "size")
					return new BareboneCallArgument (SIZE, 0);
				if (text == "address")
					return new BareboneCallArgument (ADDRESS, 0);
			}

			uint64 literal;
			if (!BareboneMemoryAddress.try_deserialize (node, out literal))
				return null;
			return new BareboneCallArgument (LITERAL, literal);
		}
	}

	public enum BareboneCallArgumentRole {
		SIZE,
		ADDRESS,
		LITERAL
	}

	public abstract class BareboneAgentConfig : Object {
		public abstract void check () throws Error;
	}

	public sealed class BareboneInvalidAgentConfig : BareboneAgentConfig {
		public override void check () throws Error {
			throw new Error.NOT_SUPPORTED ("Config for 'agent' is invalid");
		}
	}

	public sealed class BareboneInjectedAgentConfig : BareboneAgentConfig, Json.Serializable {
		public Bytes image {
			get;
			set;
		}

		public BareboneInjectingTransportConfig transport {
			get;
			set;
		}

		public BareboneInjectedAgentConfig.from_bytes (Bytes image, BareboneInjectingTransportConfig transport) {
			Object (image: image, transport: transport);
		}

		public BareboneInjectedAgentConfig.from_file (string path, BareboneInjectingTransportConfig transport) throws Error {
			Object (image: map_agent_file (path), transport: transport);
		}

		public override void check () throws Error {
			if (image == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.image' is missing");
			if (transport == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport' is missing");
			transport.check ();
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "image") {
				Bytes? image = null;
				if (property_node.get_node_type () == Json.NodeType.VALUE) {
					try {
						image = map_agent_file (property_node.get_string ());
					} catch (Error e) {
					}
				}
				var v = Value (typeof (Bytes));
				v.set_boxed (image);
				value = v;
				return true;
			}

			if (property_name == "transport") {
				BareboneInjectingTransportConfig? transport = null;
				if (property_node.get_node_type () == Json.NodeType.OBJECT) {
					Type t = Type.INVALID;
					switch (property_node.get_object ().get_string_member_with_default ("type", "")) {
					case "hostlink":
						t = typeof (BareboneHostlinkTransportConfig);
						break;
					case "vsock":
						t = typeof (BareboneVsockTransportConfig);
						break;
					case "serial":
						t = typeof (BareboneSerialTransportConfig);
						break;
					case "pipe-vsock":
						t = typeof (BareboneVsockPipeTransportConfig);
						break;
					default:
						break;
					}
					if (t != Type.INVALID)
						transport = (BareboneInjectingTransportConfig) Json.gobject_deserialize (t, property_node);
				}
				var v = Value (typeof (BareboneInjectingTransportConfig));
				v.set_object (transport);
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}

		private static Bytes map_agent_file (string path) throws Error {
			try {
				return new MappedFile (path, false).get_bytes ();
			} catch (GLib.Error e) {
				throw new Error.INVALID_ARGUMENT ("%s", e.message);
			}
		}
	}

	public sealed class BareboneResidentAgentConfig : BareboneAgentConfig, Json.Serializable {
		public BareboneResidentTransportConfig transport {
			get;
			set;
		}

		public BareboneResidentAgentConfig (BareboneResidentTransportConfig transport) {
			Object (transport: transport);
		}

		public override void check () throws Error {
			if (transport == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport' is missing");
			transport.check ();
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "transport") {
				BareboneResidentTransportConfig? transport = null;
				if (property_node.get_node_type () == Json.NodeType.OBJECT) {
					Type t = Type.INVALID;
					switch (property_node.get_object ().get_string_member_with_default ("type", "")) {
					case "device":
						t = typeof (BareboneDeviceTransportConfig);
						break;
					case "socket":
						t = typeof (BareboneSocketTransportConfig);
						break;
					default:
						break;
					}
					if (t != Type.INVALID)
						transport = (BareboneResidentTransportConfig) Json.gobject_deserialize (t, property_node);
				}
				var v = Value (typeof (BareboneResidentTransportConfig));
				v.set_object (transport);
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	public abstract class BareboneTransportConfig : Object {
		public abstract void check () throws Error;
	}

	public abstract class BareboneInjectingTransportConfig : BareboneTransportConfig {
	}

	public abstract class BareboneResidentTransportConfig : BareboneTransportConfig {
	}

	public sealed class BareboneHostlinkTransportConfig : BareboneInjectingTransportConfig, Json.Serializable {
		public string qmp {
			get;
			set;
		}

		public string? bus {
			get;
			set;
		}

		public BareboneHostlinkFabric fabric {
			get;
			set;
			default = new BareboneHostlinkPortsFabric ();
		}

		public override void check () throws Error {
			if (qmp == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.qmp' is missing");
			if (!qmp.has_prefix ("unix:"))
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.qmp' must be a UNIX socket for now");
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "fabric") {
				BareboneHostlinkFabric fabric = new BareboneHostlinkPortsFabric ();
				if (property_node.get_node_type () == Json.NodeType.OBJECT) {
					Type t = typeof (BareboneHostlinkPortsFabric);
					switch (property_node.get_object ().get_string_member_with_default ("type", "ports")) {
					case "ecam":
						t = typeof (BareboneHostlinkEcamFabric);
						break;
					case "mmio":
						t = typeof (BareboneHostlinkMmioFabric);
						break;
					default:
						break;
					}
					fabric = (BareboneHostlinkFabric) Json.gobject_deserialize (t, property_node);
				}
				var v = Value (typeof (BareboneHostlinkFabric));
				v.set_object (fabric);
				value = v;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	public abstract class BareboneHostlinkFabric : Object {
	}

	public sealed class BareboneHostlinkEcamFabric : BareboneHostlinkFabric {
		public uint64 ecam {
			get;
			set;
		}

		public BareboneHostlinkEcamFabric (uint64 ecam) {
			Object (ecam: ecam);
		}
	}

	public sealed class BareboneHostlinkPortsFabric : BareboneHostlinkFabric {
	}

	public sealed class BareboneHostlinkMmioFabric : BareboneHostlinkFabric {
	}

	/**
	 * Vsock transport. The agent inside the guest kernel connects out to the host
	 * over AF_VSOCK on `port`. The host can't speak vsock directly from frida-core
	 * (the hypervisor proxies it), so a UNIX-socket bridge is provided by the
	 * embedder (e.g. vphone-cli); frida-core just reads/writes that socket.
	 */
	public sealed class BareboneVsockTransportConfig : BareboneInjectingTransportConfig {
		/** Path to a UNIX socket the embedder has bridged to the guest's hostlink endpoint. */
		public string socket_path {
			get;
			set;
		}

		/** Vsock port the guest agent will connect to (advertised to the agent via its config). */
		public uint port {
			get;
			set;
		}

		public override void check () throws Error {
			if (socket_path == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.socket_path' is missing");
			if (port == 0)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.port' is missing");
		}
	}

	/**
	 * Serial transport. The guest kernel holds a serial port open, and the hypervisor
	 * backs that port with a UNIX socket on the host: Parallels Desktop does this for
	 * `prlctl set <vm> --device-add serial --socket <path>`, once the device has also
	 * been connected with `--device-connect`. Both ends block, so neither polls.
	 *
	 * Note that bytes written while nothing is attached to the socket are dropped
	 * rather than buffered, so the host attaches before the agent is let go.
	 */
	public sealed class BareboneSerialTransportConfig : BareboneInjectingTransportConfig {
		public string path {
			get;
			set;
		}

		public override void check () throws Error {
			if (path == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.path' is missing");
		}
	}

	/**
	 * Hostlink over the Android emulator's pipe-over-vsock bridge: the guest agent
	 * connects out over AF_VSOCK to the connector port and hands it a "pipe:unix:<path>"
	 * request, and the emulator connects that stream to the UNIX socket at `socket_path`,
	 * which frida-core listens on. The qemu-side shim whitelists the path first.
	 */
	public sealed class BareboneVsockPipeTransportConfig : BareboneInjectingTransportConfig {
		/** UNIX socket frida-core listens on and the agent names in its handshake. */
		public string socket_path {
			get;
			set;
		}

		public override void check () throws Error {
			if (socket_path == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.socket_path' is missing");
		}
	}

	/**
	 * An agent already resident in the target, reached through a device node it
	 * exposes. Nothing is injected, so there is no image and no stub.
	 */
	public sealed class BareboneDeviceTransportConfig : BareboneResidentTransportConfig {
		public string path {
			get;
			set;
		}

		public override void check () throws Error {
			if (path == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.path' is missing");
		}
	}

	/**
	 * An agent already resident in the target, reached over a UNIX socket that the
	 * hypervisor has bridged to a port the guest kernel holds open. Nothing is
	 * injected, so there is no image and no stub.
	 */
	public sealed class BareboneSocketTransportConfig : BareboneResidentTransportConfig {
		public string path {
			get;
			set;
		}

		public override void check () throws Error {
			if (path == null)
				throw new Error.NOT_SUPPORTED ("Config for 'agent.transport.path' is missing");
		}
	}

	public sealed class BareboneImageConfig : Object, Json.Serializable {
		public string file {
			get;
			set;
		}

		public BareboneMemoryAddress base {
			get;
			set;
		}

		internal Gee.Map<string, uint64?> symbols {
			get;
			set;
			default = new Gee.HashMap<string, uint64?> ();
		}

		/**
		 * Removes all configured symbols.
		 */
		public void clear_symbols () {
			symbols = new Gee.HashMap<string, uint64?> ();
		}

		/**
		 * Adds a symbol that the image itself doesn't provide.
		 *
		 * @param name the symbol's name
		 * @param address the symbol's address, relative to the image's base
		 */
		public void add_symbol (string name, uint64 address) {
			if (symbols == null)
				symbols = new Gee.HashMap<string, uint64?> ();
			symbols[name] = address;
		}

		/**
		 * Invokes @func for each configured symbol.
		 *
		 * @param func function called with each symbol's name and address
		 */
		public void enumerate_symbols (HFunc<string, uint64?> func) {
			if (symbols == null)
				return;
			foreach (var e in symbols.entries)
				func (e.key, e.value);
		}

		public void check () throws Error {
			if (file == null)
				throw new Error.NOT_SUPPORTED ("Config for 'image.file' is missing");

			if (@base != null)
				@base.check ();

			if (symbols == null)
				throw new Error.NOT_SUPPORTED ("Config for 'image.symbols' is invalid");
		}

		public bool deserialize_property (string property_name, out Value value, ParamSpec pspec, Json.Node property_node) {
			if (property_name == "base") {
				value = BareboneMemoryAddress.deserialize ("image.base", property_node);
				return true;
			}

			if (property_name == "symbols") {
				Gee.Map<string, uint64?> syms = null;

				if (property_node.get_node_type () == Json.NodeType.OBJECT) {
					syms = new Gee.HashMap<string, uint64?> ();

					property_node.get_object ().foreach_member ((obj, name, node) => {
						if (syms == null)
							return;

						uint64 addr;
						if (!BareboneMemoryAddress.try_deserialize (node, out addr)) {
							syms = null;
							return;
						}

						syms[name] = addr;
					});
				}

				value = syms;
				return true;
			}

			value = Value (pspec.value_type);
			return false;
		}
	}

	public abstract class BareboneMemoryAddress : Object {
		public string label {
			get;
			construct;
		}

		public uint64 address {
			get;
			construct;
		}

		public abstract void check () throws Error;

		internal static BareboneMemoryAddress deserialize (string label, Json.Node node) {
			uint64 address;
			if (!try_deserialize (node, out address))
				return new BareboneInvalidMemoryAddress (label);
			return new BareboneNonNullMemoryAddress (label, address);
		}

		internal static bool try_deserialize (Json.Node node, out uint64 address) {
			Type t = node.get_value_type ();

			if (t == typeof (string))
				return uint64.try_parse (node.get_string (), out address, null, 16);

			if (t == typeof (int64)) {
				address = node.get_int ();
				return true;
			}

			address = 0;
			return false;
		}
	}

	public sealed class BareboneInvalidMemoryAddress : BareboneMemoryAddress {
		public BareboneInvalidMemoryAddress (string label) {
			Object (label: label);
		}

		public override void check () throws Error {
			throw new Error.NOT_SUPPORTED ("Config for '%s' is invalid", label);
		}
	}

	public sealed class BareboneNonNullMemoryAddress : BareboneMemoryAddress {
		public BareboneNonNullMemoryAddress (string label, uint64 address) {
			Object (label: label, address: address);
		}

		public override void check () throws Error {
			if (address == 0)
				throw new Error.NOT_SUPPORTED ("Config for '%s' cannot be NULL", label);
		}
	}

	/**
	 * An immutable list of {@link Application} objects.
	 */
	public sealed class ApplicationList : Object {
		private Gee.List<Application> items;

		internal ApplicationList (Gee.List<Application> items) {
			this.items = items;
		}

		/**
		 * Gets the number of applications in the list.
		 *
		 * @return the count
		 */
		public int size () {
			return items.size;
		}

		/**
		 * Gets the application at the given position.
		 *
		 * @param index zero-based position
		 * @return the application
		 */
		public new Application get (int index) {
			return items.get (index);
		}
	}

	/**
	 * Represents an application installed on a device.
	 */
	public sealed class Application : Object {
		/**
		 * The application's identifier, such as its bundle or package ID.
		 */
		public string identifier {
			get;
			construct;
		}

		/**
		 * The application's display name.
		 */
		public string name {
			get;
			construct;
		}

		/**
		 * The PID of the application if it is running, or 0 otherwise.
		 */
		public uint pid {
			get;
			construct;
		}

		/**
		 * Additional parameters describing the application, keyed by name.
		 */
		public HashTable<string, Variant> parameters {
			get;
			construct;
		}

		internal Application (string identifier, string name, uint pid, HashTable<string, Variant> parameters) {
			Object (identifier: identifier, name: name, pid: pid, parameters: parameters);
		}
	}

	/**
	 * An immutable list of {@link Process} objects.
	 */
	public sealed class ProcessList : Object {
		private Gee.List<Process> items;

		internal ProcessList (Gee.List<Process> items) {
			this.items = items;
		}

		/**
		 * Gets the number of processes in the list.
		 *
		 * @return the count
		 */
		public int size () {
			return items.size;
		}

		/**
		 * Gets the process at the given position.
		 *
		 * @param index zero-based position
		 * @return the process
		 */
		public new Process get (int index) {
			return items.get (index);
		}
	}

	/**
	 * Represents a process running on a device.
	 */
	public sealed class Process : Object {
		/**
		 * The process ID.
		 */
		public uint pid {
			get;
			construct;
		}

		/**
		 * The process name.
		 */
		public string name {
			get;
			construct;
		}

		/**
		 * Additional parameters describing the process, keyed by name.
		 */
		public HashTable<string, Variant> parameters {
			get;
			construct;
		}

		internal Process (uint pid, string name, HashTable<string, Variant> parameters) {
			Object (pid: pid, name: name, parameters: parameters);
		}
	}

	/**
	 * Options for scoping and shaping a process query or match.
	 */
	public sealed class ProcessMatchOptions : Object {
		/**
		 * How long to wait for a match, in milliseconds, or 0 to not wait.
		 */
		public int timeout {
			get;
			set;
			default = 0;
		}

		/**
		 * How much detail to include about each process.
		 */
		public Scope scope {
			get;
			set;
			default = MINIMAL;
		}
	}

	/**
	 * Options controlling how a process is spawned with {@link Device.spawn}.
	 */
	public sealed class SpawnOptions : Object {
		/**
		 * The argument vector, replacing the default. The first element is
		 * conventionally the program itself.
		 */
		public string[]? argv {
			get;
			set;
		}

		/**
		 * The complete environment, replacing the default.
		 */
		public string[]? envp {
			get;
			set;
		}

		/**
		 * Environment entries to add to or override in the default environment.
		 */
		public string[]? env {
			get;
			set;
		}

		/**
		 * The working directory to start in.
		 */
		public string? cwd {
			get;
			set;
		}

		/**
		 * How to set up the standard I/O streams of the new process.
		 */
		public Stdio stdio {
			get;
			set;
			default = INHERIT;
		}

		/**
		 * Auxiliary, platform-specific spawn parameters, keyed by name.
		 */
		public HashTable<string, Variant> aux {
			get;
			set;
			default = make_parameters_dict ();
		}
	}

	/**
	 * An immutable list of {@link Spawn} objects.
	 */
	public sealed class SpawnList : Object {
		private Gee.List<Spawn> items;

		internal SpawnList (Gee.List<Spawn> items) {
			this.items = items;
		}

		/**
		 * Gets the number of spawns in the list.
		 *
		 * @return the count
		 */
		public int size () {
			return items.size;
		}

		/**
		 * Gets the spawn at the given position.
		 *
		 * @param index zero-based position
		 * @return the spawn
		 */
		public new Spawn get (int index) {
			return items.get (index);
		}
	}

	/**
	 * Represents a process spawned and held suspended by spawn gating.
	 */
	public sealed class Spawn : Object {
		/**
		 * The process ID of the spawned process.
		 */
		public uint pid {
			get;
			construct;
		}

		/**
		 * The identifier of the program that was spawned, if known.
		 */
		public string? identifier {
			get;
			construct;
		}

		internal Spawn (uint pid, string? identifier) {
			Object (
				pid: pid,
				identifier: identifier
			);
		}

		internal static Spawn from_info (HostSpawnInfo info) {
			var identifier = info.identifier;
			return new Spawn (info.pid, (identifier.length > 0) ? identifier : null);
		}
	}

	/**
	 * An immutable list of {@link Child} objects.
	 */
	public sealed class ChildList : Object {
		private Gee.List<Child> items;

		internal ChildList (Gee.List<Child> items) {
			this.items = items;
		}

		/**
		 * Gets the number of children in the list.
		 *
		 * @return the count
		 */
		public int size () {
			return items.size;
		}

		/**
		 * Gets the child at the given position.
		 *
		 * @param index zero-based position
		 * @return the child
		 */
		public new Child get (int index) {
			return items.get (index);
		}
	}

	/**
	 * Represents a child process observed while child gating is enabled.
	 */
	public sealed class Child : Object {
		/**
		 * The child's process ID.
		 */
		public uint pid {
			get;
			construct;
		}

		/**
		 * The process ID of the parent.
		 */
		public uint parent_pid {
			get;
			construct;
		}

		/**
		 * How the child came to be, such as fork or exec.
		 */
		public ChildOrigin origin {
			get;
			construct;
		}

		/**
		 * The identifier of the program, if known.
		 */
		public string? identifier {
			get;
			construct;
		}

		/**
		 * The path of the program image, if known.
		 */
		public string? path {
			get;
			construct;
		}

		/**
		 * The argument vector the child was launched with, if known.
		 */
		public string[]? argv {
			get;
			construct;
		}

		/**
		 * The environment the child was launched with, if known.
		 */
		public string[]? envp {
			get;
			construct;
		}

		internal Child (uint pid, uint parent_pid, ChildOrigin origin, string? identifier, string? path, string[]? argv,
				string[]? envp) {
			Object (
				pid: pid,
				parent_pid: parent_pid,
				origin: origin,
				identifier: identifier,
				path: path,
				argv: argv,
				envp: envp
			);
		}

		internal static Child from_info (HostChildInfo info) {
			var identifier = info.identifier;
			var path = info.path;
			return new Child (
				info.pid,
				info.parent_pid,
				info.origin,
				(identifier.length > 0) ? identifier : null,
				(path.length > 0) ? path : null,
				info.has_argv ? info.argv : null,
				info.has_envp ? info.envp : null
			);
		}
	}

	/**
	 * Details of a process crash.
	 */
	public sealed class Crash : Object {
		/**
		 * The process ID that crashed.
		 */
		public uint pid {
			get;
			construct;
		}

		/**
		 * The name of the process that crashed.
		 */
		public string process_name {
			get;
			construct;
		}

		/**
		 * A short, human-readable summary of the crash.
		 */
		public string summary {
			get;
			construct;
		}

		/**
		 * The full crash report.
		 */
		public string report {
			get;
			construct;
		}

		/**
		 * Additional parameters describing the crash, keyed by name.
		 */
		public HashTable<string, Variant> parameters {
			get;
			construct;
		}

		internal Crash (uint pid, string process_name, string summary, string report, HashTable<string, Variant> parameters) {
			Object (
				pid: pid,
				process_name: process_name,
				summary: summary,
				report: report,
				parameters: parameters
			);
		}

		internal static Crash? from_info (CrashInfo info) {
			if (info.pid == 0)
				return null;
			return new Crash (
				info.pid,
				info.process_name,
				info.summary,
				info.report,
				info.parameters
			);
		}
	}

	/**
	 * A message bus for exchanging messages with a {@link Device}, obtained via
	 * {@link Device.bus}.
	 */
	public sealed class Bus : Object {
		/**
		 * Emitted when the bus is detached from the device.
		 */
		public signal void detached ();
		/**
		 * Emitted when a message is received from the device.
		 *
		 * @param json the message as a JSON string
		 * @param data binary data accompanying the message, if any
		 */
		public signal void message (string json, Bytes? data);

		private weak Device device;

		private Promise<BusSession>? attach_request;
		private BusSession? active_session;

		private Cancellable io_cancellable = new Cancellable ();

		internal Bus (Device device) {
			this.device = device;
		}

		/**
		 * Checks whether the bus is currently detached.
		 *
		 * @return true if not attached
		 */
		public bool is_detached () {
			return attach_request == null;
		}

		/**
		 * Attaches to the device's message bus, after which messages can be
		 * posted and received.
		 */
		public async void attach (Cancellable? cancellable = null) throws Error, IOError {
			while (attach_request != null) {
				try {
					yield attach_request.future.wait_async (cancellable);
					return;
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			attach_request = new Promise<BusSession> ();

			try {
				var host_session = yield device.get_host_session (cancellable);

				DBusProxy proxy = host_session as DBusProxy;
				if (proxy == null)
					throw new Error.NOT_SUPPORTED ("Bus is not available on this device");

				try {
					active_session = yield proxy.g_connection.get_proxy (null, ObjectPath.BUS_SESSION,
						DO_NOT_LOAD_PROPERTIES, cancellable);
					active_session.message.connect (on_message);

					yield active_session.attach (cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				attach_request.resolve (active_session);
			} catch (GLib.Error e) {
				attach_request.reject (e);
				attach_request = null;

				throw_api_error (e);
			}
		}

		public void attach_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<AttachTask> ().execute (cancellable);
		}

		private class AttachTask : BusTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.attach (cancellable);
			}
		}

		internal async void _detach (HostSession dead_host_session) {
			if (attach_request == null)
				return;

			DBusConnection dead_connection = ((DBusProxy) dead_host_session).g_connection;

			io_cancellable.cancel ();
			io_cancellable = new Cancellable ();

			while (attach_request != null) {
				try {
					var some_session = yield attach_request.future.wait_async (null);
					if (((DBusProxy) some_session).g_connection == dead_connection) {
						some_session.message.disconnect (on_message);
						active_session = null;
						attach_request = null;
					} else {
						return;
					}
				} catch (GLib.Error e) {
				}
			}

			detached ();
		}

		/**
		 * Posts a message to the bus.
		 *
		 * @param json the message as a JSON string
		 * @param data binary data to accompany the message, if any
		 */
		public void post (string json, Bytes? data = null) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_post (json, data);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_post (json, data);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_post (string json, Bytes? data) {
			if (active_session == null)
				return;
			var has_data = data != null;
			var data_param = has_data ? data.get_data () : new uint8[0];
			active_session.post.begin (json, has_data, data_param, io_cancellable);
		}

		private void on_message (string json, bool has_data, uint8[] data) {
			message (json, has_data ? new Bytes (data) : null);
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class BusTask<T> : AsyncTask<T> {
			public weak Bus parent {
				get;
				construct;
			}
		}
	}

	/**
	 * A connection to a device-specific service, opened via
	 * {@link Device.open_service}.
	 */
	public sealed class Service : Object {
		/**
		 * Emitted when the service connection is closed.
		 */
		public signal void close ();
		/**
		 * Emitted when the service sends a message.
		 *
		 * @param message the message
		 */
		public signal void message (Variant message);

		private ServiceSession? session;
		private bool disposed = false;

		internal Service (ServiceSession session) {
			this.session = session;

			session.close.connect (on_close);
			session.message.connect (on_message);
		}

		public override void dispose () {
			if (!disposed) {
				disposed = true;

				MainContext context = get_main_context ();
				if (context.is_owner ()) {
					abandon ();
				} else {
					var source = new IdleSource ();
					source.set_callback (() => {
						abandon ();
						return false;
					});
					source.attach (context);
				}
			}

			base.dispose ();
		}

		~Service () {
			forget_session ();
		}

		private void abandon () {
			var s = session;
			if (s != null) {
				forget_session ();
				s.cancel.begin (null);
			}
		}

		private void forget_session () {
			session.close.disconnect (on_close);
			session.message.disconnect (on_message);
			session = null;
		}

		/**
		 * Checks whether the service connection has been closed.
		 *
		 * @return true if closed
		 */
		public bool is_closed () {
			return session == null;
		}

		/**
		 * Activates the service, completing any handshake needed before
		 * requests can be made.
		 */
		public async void activate (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield session.activate (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void activate_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<ActivateTask> ().execute (cancellable);
		}

		private class ActivateTask : ServiceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.activate (cancellable);
			}
		}

		/**
		 * Cancels the service connection, closing it.
		 */
		public async void cancel (Cancellable? cancellable = null) throws IOError {
			if (session == null)
				return;

			try {
				yield session.cancel (cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					cancellable.set_error_if_cancelled ();
			}
		}

		public void cancel_sync (Cancellable? cancellable = null) throws IOError {
			try {
				create<CancelTask> ().execute (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private class CancelTask : ServiceTask<void> {
			protected override async void perform_operation () throws IOError {
				yield parent.cancel (cancellable);
			}
		}

		/**
		 * Sends a request to the service and waits for its response.
		 *
		 * @param parameters the request parameters
		 * @return the service's response
		 */
		public async Variant request (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				return yield session.request (parameters, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public Variant request_sync (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<RequestTask> () as RequestTask;
			task.parameters = parameters;
			return task.execute (cancellable);
		}

		private class RequestTask : ServiceTask<Variant> {
			public Variant parameters;

			protected override async Variant perform_operation () throws Error, IOError {
				return yield parent.request (parameters, cancellable);
			}
		}

		private void check_open () throws Error {
			if (session == null)
				throw new Error.INVALID_OPERATION ("Session is gone");
		}

		private void on_close () {
			forget_session ();
			close ();
		}

		private void on_message (Variant v) {
			message (v);
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class ServiceTask<T> : AsyncTask<T> {
			public weak Service parent {
				get;
				construct;
			}
		}
	}

	/**
	 * A live connection to a process, obtained via {@link Device.attach},
	 * through which scripts can be created and run.
	 */
	public sealed class Session : Object, AgentMessageSink {
		/**
		 * Emitted when the session is detached from the target process.
		 *
		 * @param reason why the session was detached
		 * @param crash the crash that caused the detach, if any
		 */
		public signal void detached (SessionDetachReason reason, Crash? crash);

		/**
		 * The PID of the attached process.
		 */
		public uint pid {
			get;
			construct;
		}

		/**
		 * How long the session may survive a temporary disconnection before
		 * being torn down, in seconds; 0 means no persistence.
		 */
		public uint persist_timeout {
			get;
			construct;
		}

		private AgentSessionId id;
		private unowned Device device;

		private State state = ATTACHED;
		private Promise<bool> close_request;

		internal AgentSession active_session;
		private AgentSession? obsolete_session;

		private uint last_rx_batch_id = 0;
		private Gee.LinkedList<PendingMessage> pending_messages = new Gee.LinkedList<PendingMessage> ();
		private int next_serial = 1;
		private uint pending_deliveries = 0;
		private Cancellable delivery_cancellable = new Cancellable ();

		private Gee.HashMap<AgentScriptId?, Script> scripts =
			new Gee.HashMap<AgentScriptId?, Script> (AgentScriptId.hash, AgentScriptId.equal);

		private PeerOptions? nice_options;
#if HAVE_NICE
		private Nice.Agent? nice_agent;
		private uint nice_stream_id;
		private uint nice_component_id;
		private SctpConnection? nice_iostream;
		private DBusConnection? nice_connection;
		private uint nice_registration_id;
		private Cancellable? nice_cancellable;

		private MainContext? frida_context;
		private MainContext? dbus_context;
#endif

		private enum State {
			ATTACHED,
			INTERRUPTED,
			DETACHED,
		}

		internal Session (Device device, uint pid, AgentSessionId id, SessionOptions options) {
			Object (pid: pid, persist_timeout: options.persist_timeout);

			this.id = id;
			this.device = device;
		}

		/**
		 * Checks whether the session has been detached from the target.
		 *
		 * @return true if no longer attached
		 */
		public bool is_detached () {
			return state != ATTACHED;
		}

		/**
		 * Detaches from the target process, tearing down all scripts created
		 * through this session.
		 */
		public async void detach (Cancellable? cancellable = null) throws IOError {
			yield _do_close (APPLICATION_REQUESTED, CrashInfo.empty (), true, cancellable);
		}

		public void detach_sync (Cancellable? cancellable = null) throws IOError {
			try {
				create<DetachTask> ().execute (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private class DetachTask : SessionTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.detach (cancellable);
			}
		}

		/**
		 * Resumes a session that was persisted across a disconnection,
		 * reattaching to the target.
		 */
		public async void resume (Cancellable? cancellable = null) throws Error, IOError {
			switch (state) {
				case ATTACHED:
					return;
				case INTERRUPTED:
					break;
				case DETACHED:
					throw new Error.INVALID_OPERATION ("Session is gone");
			}

			DBusConnection old_connection = ((DBusProxy) active_session).g_connection;
			if (old_connection.is_closed ()) {
				var host_session = yield device.get_host_session (cancellable);

				try {
					yield host_session.reattach (id, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				var agent_session = yield device.provider.link_agent_session (host_session, id, this, cancellable);

				begin_migration (agent_session);
			}

			if (nice_options != null) {
				yield do_setup_peer_connection (nice_options, cancellable);
			}

			uint last_tx_batch_id;
			try {
				yield active_session.resume (last_rx_batch_id, cancellable, out last_tx_batch_id);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			if (last_tx_batch_id != 0) {
				PendingMessage? m;
				while ((m = pending_messages.peek ()) != null && m.delivery_attempts > 0 && m.serial <= last_tx_batch_id) {
					pending_messages.poll ();
				}
			}

			delivery_cancellable = new Cancellable ();
			state = ATTACHED;

			maybe_deliver_pending_messages ();
		}

		public void resume_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<ResumeTask> ().execute (cancellable);
		}

		private class ResumeTask : SessionTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.resume (cancellable);
			}
		}

		/**
		 * Enables child gating, so that children of the target process are held
		 * suspended until explicitly resumed.
		 */
		public async void enable_child_gating (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield active_session.enable_child_gating (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void enable_child_gating_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<EnableChildGatingTask> ().execute (cancellable);
		}

		private class EnableChildGatingTask : SessionTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.enable_child_gating (cancellable);
			}
		}

		/**
		 * Disables child gating.
		 */
		public async void disable_child_gating (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield active_session.disable_child_gating (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void disable_child_gating_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<DisableChildGatingTask> ().execute (cancellable);
		}

		private class DisableChildGatingTask : SessionTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.disable_child_gating (cancellable);
			}
		}

		/**
		 * Creates a new script from JavaScript source.
		 *
		 * @param source the script's JavaScript source code
		 * @param options script options such as name and runtime, or null
		 * @return the new script, not yet loaded
		 */
		public async Script create_script (string source, ScriptOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			AgentScriptId script_id;
			try {
				script_id = yield active_session.create_script (source, raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			check_open ();

			var script = new Script (this, script_id);
			scripts[script_id] = script;

			return script;
		}

		public Script create_script_sync (string source, ScriptOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<CreateScriptTask> ();
			task.source = source;
			task.options = options;
			return task.execute (cancellable);
		}

		private class CreateScriptTask : SessionTask<Script> {
			public string source;
			public ScriptOptions? options;

			protected override async Script perform_operation () throws Error, IOError {
				return yield parent.create_script (source, options, cancellable);
			}
		}

		/**
		 * Creates a new script from a precompiled bytecode blob, as produced by
		 * {@link Session.compile_script}.
		 *
		 * @param bytes the compiled script
		 * @param options script options such as name and runtime, or null
		 * @return the new script, not yet loaded
		 */
		public async Script create_script_from_bytes (Bytes bytes, ScriptOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			AgentScriptId script_id;
			try {
				script_id = yield active_session.create_script_from_bytes (bytes.get_data (), raw_options,
					cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			check_open ();

			var script = new Script (this, script_id);
			scripts[script_id] = script;

			return script;
		}

		public Script create_script_from_bytes_sync (Bytes bytes, ScriptOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<CreateScriptFromBytesTask> ();
			task.bytes = bytes;
			task.options = options;
			return task.execute (cancellable);
		}

		private class CreateScriptFromBytesTask : SessionTask<Script> {
			public Bytes bytes;
			public ScriptOptions? options;

			protected override async Script perform_operation () throws Error, IOError {
				return yield parent.create_script_from_bytes (bytes, options, cancellable);
			}
		}

		/**
		 * Compiles JavaScript source to a bytecode blob, which can later be
		 * loaded with {@link Session.create_script_from_bytes}.
		 *
		 * @param source the script's JavaScript source code
		 * @param options script options affecting compilation, or null
		 * @return the compiled bytecode
		 */
		public async Bytes compile_script (string source, ScriptOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			uint8[] data;
			try {
				data = yield active_session.compile_script (source, raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			return new Bytes (data);
		}

		public Bytes compile_script_sync (string source, ScriptOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<CompileScriptTask> ();
			task.source = source;
			task.options = options;
			return task.execute (cancellable);
		}

		private class CompileScriptTask : SessionTask<Bytes> {
			public string source;
			public ScriptOptions? options;

			protected override async Bytes perform_operation () throws Error, IOError {
				return yield parent.compile_script (source, options, cancellable);
			}
		}

		/**
		 * Builds a heap snapshot by running the given script, so that scripts
		 * created later can start from the captured state.
		 *
		 * @param embed_script JavaScript to run to set up the snapshot
		 * @param options snapshot options, or null
		 * @return the serialized snapshot
		 */
		public async Bytes snapshot_script (string embed_script, SnapshotOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			uint8[] data;
			try {
				data = yield active_session.snapshot_script (embed_script, raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			return new Bytes (data);
		}

		public Bytes snapshot_script_sync (string embed_script, SnapshotOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<SnapshotScriptTask> ();
			task.embed_script = embed_script;
			task.options = options;
			return task.execute (cancellable);
		}

		private class SnapshotScriptTask : SessionTask<Bytes> {
			public string embed_script;
			public SnapshotOptions? options;

			protected override async Bytes perform_operation () throws Error, IOError {
				return yield parent.snapshot_script (embed_script, options, cancellable);
			}
		}

		/**
		 * Sets up a peer-to-peer connection to the target, so that subsequent
		 * traffic flows directly rather than through the device's host session.
		 *
		 * @param options peer connection options, such as STUN and relays, or
		 *   null
		 */
		public async void setup_peer_connection (PeerOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			yield do_setup_peer_connection (options, cancellable);
		}

#if HAVE_NICE
		private async void do_setup_peer_connection (PeerOptions? options, Cancellable? cancellable) throws Error, IOError {
			AgentSession server_session = active_session;

			frida_context = get_main_context ();
			dbus_context = yield get_dbus_context ();

			var agent = new Nice.Agent.full (dbus_context, Nice.Compatibility.RFC5245, ICE_TRICKLE);
			agent.set_software ("Frida");
			agent.controlling_mode = true;
			agent.ice_tcp = false;

			uint stream_id = agent.add_stream (1);
			if (stream_id == 0)
				throw new Error.NOT_SUPPORTED ("Unable to add stream");
			uint component_id = 1;
			agent.set_stream_name (stream_id, "application");

			yield PeerConnection.configure_agent (agent, stream_id, component_id, options, cancellable);

			uint8[] cert_der;
			string cert_pem, key_pem;
			yield generate_certificate (out cert_der, out cert_pem, out key_pem);

			TlsCertificate certificate;
			try {
				certificate = new TlsCertificate.from_pem (cert_pem + key_pem, -1);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			var offer = new PeerSessionDescription ();
			offer.session_id = PeerSessionId.generate ();
			agent.get_local_credentials (stream_id, out offer.ice_ufrag, out offer.ice_pwd);
			offer.ice_trickle = true;
			offer.fingerprint = PeerConnection.compute_certificate_fingerprint (cert_der);
			offer.setup = ACTPASS;

			string offer_sdp = offer.to_sdp ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			IOStream stream = null;
			server_session.new_candidates.connect (on_new_candidates);
			server_session.candidate_gathering_done.connect (on_candidate_gathering_done);
			try {
				string answer_sdp;
				try {
					yield server_session.offer_peer_connection (offer_sdp, raw_options, cancellable, out answer_sdp);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				var answer = PeerSessionDescription.parse (answer_sdp);
				agent.set_remote_credentials (stream_id, answer.ice_ufrag, answer.ice_pwd);

				if (nice_agent != null)
					throw new Error.INVALID_OPERATION ("Peer connection already exists");

				nice_agent = agent;
				nice_cancellable = new Cancellable ();
				nice_stream_id = stream_id;
				nice_component_id = component_id;

				var open_request = new Promise<IOStream> ();

				schedule_on_dbus_thread (() => {
					open_peer_connection.begin (server_session, certificate, answer, open_request);
					return false;
				});

				stream = yield open_request.future.wait_async (cancellable);
			} finally {
				server_session.candidate_gathering_done.disconnect (on_candidate_gathering_done);
				server_session.new_candidates.disconnect (on_new_candidates);
			}

			try {
				nice_connection = yield new DBusConnection (stream, null, DELAY_MESSAGE_PROCESSING, null, nice_cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
			nice_connection.on_closed.connect (on_nice_connection_closed);

			try {
				nice_registration_id = nice_connection.register_object (ObjectPath.AGENT_MESSAGE_SINK,
					(AgentMessageSink) this);
			} catch (IOError io_error) {
				assert_not_reached ();
			}

			nice_connection.start_message_processing ();

			AgentSession peer_session;
			try {
				peer_session = yield nice_connection.get_proxy (null, ObjectPath.AGENT_SESSION, DO_NOT_LOAD_PROPERTIES,
					nice_cancellable);
			} catch (IOError e) {
				throw_dbus_error (e);
			}

			try {
				yield server_session.begin_migration (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			begin_migration (peer_session);

			try {
				yield server_session.commit_migration (cancellable);
			} catch (GLib.Error e) {
				cancel_migration (peer_session);
				throw_dbus_error (e);
			}

			nice_options = (options != null) ? options : new PeerOptions ();
		}

		private async void teardown_peer_connection (Cancellable? cancellable) throws IOError {
			Nice.Agent? agent = nice_agent;
			DBusConnection? conn = nice_connection;

			discard_peer_connection ();

			if (conn != null) {
				try {
					yield conn.close (cancellable);
				} catch (GLib.Error e) {
				}
			}

			if (agent != null) {
				schedule_on_dbus_thread (() => {
					agent.close_async.begin ();

					schedule_on_frida_thread (() => {
						teardown_peer_connection.callback ();
						return false;
					});

					return false;
				});
				yield;
			}
		}

		private void discard_peer_connection () {
			nice_cancellable = null;

			if (nice_registration_id != 0) {
				nice_connection.unregister_object (nice_registration_id);
				nice_registration_id = 0;
			}

			if (nice_connection != null) {
				nice_connection.on_closed.disconnect (on_nice_connection_closed);
				nice_connection = null;
			}

			nice_iostream = null;

			nice_component_id = 0;
			nice_stream_id = 0;

			nice_agent = null;
		}

		private async void open_peer_connection (AgentSession server_session, TlsCertificate certificate,
				PeerSessionDescription answer, Promise<IOStream> promise) {
			Nice.Agent agent = nice_agent;
			DtlsConnection? tc = null;
			ulong candidate_handler = 0;
			ulong gathering_handler = 0;
			ulong accept_handler = 0;
			try {
				agent.component_state_changed.connect (on_component_state_changed);

				var pending_candidates = new Gee.ArrayList<string> ();
				candidate_handler = agent.new_candidate_full.connect (candidate => {
					string candidate_sdp = agent.generate_local_candidate_sdp (candidate);
					pending_candidates.add (candidate_sdp);
					if (pending_candidates.size == 1) {
						schedule_on_dbus_thread (() => {
							var stolen_candidates = pending_candidates;
							pending_candidates = new Gee.ArrayList<string> ();

							schedule_on_frida_thread (() => {
								if (nice_agent == null)
									return false;

								server_session.add_candidates.begin (stolen_candidates.to_array (),
									nice_cancellable);

								return false;
							});

							return false;
						});
					}
				});

				gathering_handler = agent.candidate_gathering_done.connect (stream_id => {
					schedule_on_dbus_thread (() => {
						schedule_on_frida_thread (() => {
							if (nice_agent == null)
								return false;
							server_session.notify_candidate_gathering_done.begin (nice_cancellable);
							return false;
						});
						return false;
					});
				});

				if (!agent.gather_candidates (nice_stream_id))
					throw new Error.NOT_SUPPORTED ("Unable to gather local candidates");

				var socket = new PeerSocket (agent, nice_stream_id, nice_component_id);

				if (answer.setup == ACTIVE) {
					var dsc = DtlsServerConnection.new (socket, certificate);
					dsc.authentication_mode = REQUIRED;
					tc = dsc;
				} else {
					tc = DtlsClientConnection.new (socket, null);
					tc.set_certificate (certificate);
				}
				tc.set_database (null);
				accept_handler = tc.accept_certificate.connect ((peer_cert, errors) => {
					return PeerConnection.compute_certificate_fingerprint (peer_cert.certificate.data) == answer.fingerprint;
				});
				yield tc.handshake_async (Priority.DEFAULT, nice_cancellable);

				nice_iostream = new SctpConnection (tc, answer.setup, answer.sctp_port, answer.max_message_size);

				schedule_on_frida_thread (() => {
					promise.resolve (nice_iostream);
					return false;
				});
			} catch (GLib.Error e) {
				string message = (e is IOError.CANCELLED)
					? "Unable to establish peer connection"
					: e.message;
				Error error = new Error.TRANSPORT ("%s", message);
				schedule_on_frida_thread (() => {
					nice_component_id = 0;
					nice_stream_id = 0;
					nice_cancellable = null;
					nice_agent = null;

					promise.reject (error);
					return false;
				});
			} finally {
				if (accept_handler != 0)
					tc.disconnect (accept_handler);
				if (gathering_handler != 0)
					agent.disconnect (gathering_handler);
				if (candidate_handler != 0)
					agent.disconnect (candidate_handler);
			}
		}

		private void on_component_state_changed (uint stream_id, uint component_id, Nice.ComponentState state) {
			if (state == FAILED)
				nice_cancellable.cancel ();
		}

		private void on_new_candidates (string[] candidate_sdps) {
			Nice.Agent? agent = nice_agent;
			if (agent == null)
				return;

			string[] candidate_sdps_copy = candidate_sdps;
			schedule_on_dbus_thread (() => {
				var candidates = new SList<Nice.Candidate> ();
				foreach (unowned string sdp in candidate_sdps_copy) {
					var candidate = agent.parse_remote_candidate_sdp (nice_stream_id, sdp);
					if (candidate != null)
						candidates.append (candidate);
				}

				agent.set_remote_candidates (nice_stream_id, nice_component_id, candidates);

				return false;
			});
		}

		private void on_candidate_gathering_done () {
			Nice.Agent? agent = nice_agent;
			if (agent == null)
				return;

			schedule_on_dbus_thread (() => {
				agent.peer_candidate_gathering_done (nice_stream_id);

				return false;
			});
		}

		private void on_nice_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			handle_nice_connection_closure.begin ();
		}

		private async void handle_nice_connection_closure () {
			try {
				yield teardown_peer_connection (null);
			} catch (IOError e) {
				assert_not_reached ();
			}

			if (persist_timeout != 0) {
				if (state != ATTACHED)
					return;
				state = INTERRUPTED;
				active_session = obsolete_session;
				obsolete_session = null;
				delivery_cancellable.cancel ();
				detached (CONNECTION_TERMINATED, null);
			} else {
				_do_close.begin (CONNECTION_TERMINATED, CrashInfo.empty (), false, null);
			}
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (frida_context);
		}

		private void schedule_on_dbus_thread (owned SourceFunc function) {
			assert (dbus_context != null);

			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (dbus_context);
		}
#else
		private async void do_setup_peer_connection (PeerOptions? options, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Peer-to-peer support not available due to build configuration");
		}

		private async void teardown_peer_connection (Cancellable? cancellable) throws IOError {
		}

		private void discard_peer_connection () {
		}
#endif

		public void setup_peer_connection_sync (PeerOptions? options = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<SetupPeerConnectionTask> ();
			task.options = options;
			task.execute (cancellable);
		}

		private class SetupPeerConnectionTask : SessionTask<void> {
			public PeerOptions? options;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.setup_peer_connection (options, cancellable);
			}
		}

		/**
		 * Joins a portal, making the target reachable through it.
		 *
		 * @param address the portal address to connect to
		 * @param options portal options, such as a token or ACL, or null
		 * @return a membership handle that can later leave the portal
		 */
		public async PortalMembership join_portal (string address, PortalOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_open ();

			var raw_options = (options != null) ? options._serialize () : make_parameters_dict ();

			PortalMembershipId membership_id;
			try {
				membership_id = yield active_session.join_portal (address, raw_options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			return new PortalMembership (this, membership_id);
		}

		public PortalMembership join_portal_sync (string address, PortalOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<JoinPortalTask> ();
			task.address = address;
			task.options = options;
			return task.execute (cancellable);
		}

		private class JoinPortalTask : SessionTask<PortalMembership> {
			public string address;
			public PortalOptions? options;

			protected override async PortalMembership perform_operation () throws Error, IOError {
				return yield parent.join_portal (address, options, cancellable);
			}
		}

		protected async void post_messages (AgentMessage[] messages, uint batch_id,
				Cancellable? cancellable) throws Error, IOError {
			if (state == INTERRUPTED)
				throw new Error.INVALID_OPERATION ("Cannot receive messages while interrupted");

			foreach (var m in messages) {
				switch (m.kind) {
					case SCRIPT: {
						var script = scripts[m.script_id];
						if (script != null)
							script.message (m.text, m.has_data ? new Bytes (m.data) : null);
						break;
					}
					case DEBUGGER:
						var script = scripts[m.script_id];
						if (script != null)
							script.on_debugger_message_from_backend (m.text);
						break;
				}
			}

			last_rx_batch_id = batch_id;
		}

		internal void _post_to_agent (AgentMessageKind kind, AgentScriptId script_id, string text, Bytes? data = null) {
			if (state == DETACHED)
				return;
			pending_messages.offer (new PendingMessage (next_serial++, kind, script_id, text, data));
			maybe_deliver_pending_messages ();
		}

		private void maybe_deliver_pending_messages () {
			if (state != ATTACHED)
				return;

			AgentSession sink = active_session;

			if (pending_messages.is_empty)
				return;

			var batch = new Gee.ArrayList<PendingMessage> ();
			void * items = null;
			int n_items = 0;
			size_t total_size = 0;
			size_t max_size = 4 * 1024 * 1024;
			PendingMessage? m;
			while ((m = pending_messages.peek ()) != null) {
				size_t message_size = m.estimate_size_in_bytes ();
				if (total_size + message_size > max_size && !batch.is_empty)
					break;
				pending_messages.poll ();
				batch.add (m);

				n_items++;
				items = realloc (items, n_items * sizeof (AgentMessage));

				AgentMessage * am = (AgentMessage *) items + n_items - 1;

				am->kind = m.kind;
				am->script_id = m.script_id;

				*((void **) &am->text) = m.text;

				unowned Bytes? data = m.data;
				am->has_data = data != null;
				*((void **) &am->data) = am->has_data ? data.get_data () : null;
				am->data.length = am->has_data ? data.length : 0;

				total_size += message_size;
			}

			if (persist_timeout == 0)
				emit_batch (sink, batch, items);
			else
				deliver_batch.begin (sink, batch, items);
		}

		private void emit_batch (AgentSession sink, Gee.ArrayList<PendingMessage> messages, void * items) {
			unowned AgentMessage[] items_arr = (AgentMessage[]) items;
			items_arr.length = messages.size;

			sink.post_messages.begin (items_arr, 0, delivery_cancellable);

			free (items);
		}

		private async void deliver_batch (AgentSession sink, Gee.ArrayList<PendingMessage> messages, void * items) {
			bool success = false;
			pending_deliveries++;
			try {
				int n = messages.size;

				foreach (var message in messages)
					message.delivery_attempts++;

				unowned AgentMessage[] items_arr = (AgentMessage[]) items;
				items_arr.length = n;

				uint batch_id = messages[n - 1].serial;

				yield sink.post_messages (items_arr, batch_id, delivery_cancellable);

				success = true;
			} catch (GLib.Error e) {
				pending_messages.add_all (messages);
				pending_messages.sort ((a, b) => a.serial - b.serial);
			} finally {
				pending_deliveries--;
				if (pending_deliveries == 0 && success)
					next_serial = 1;

				free (items);
			}
		}

		private class PendingMessage {
			public int serial;
			public AgentMessageKind kind;
			public AgentScriptId script_id;
			public string text;
			public Bytes? data;
			public uint delivery_attempts;

			public PendingMessage (int serial, AgentMessageKind kind, AgentScriptId script_id, string text,
					Bytes? data = null) {
				this.serial = serial;
				this.kind = kind;
				this.script_id = script_id;
				this.text = text;
				this.data = data;
			}

			public size_t estimate_size_in_bytes () {
				return sizeof (AgentMessage) + text.length + 1 + ((data != null) ? data.length : 0);
			}
		}

		internal void _release_script (AgentScriptId script_id) {
			var script_did_exist = scripts.unset (script_id);
			assert (script_did_exist);
		}

		private void check_open () throws Error {
			switch (state) {
				case ATTACHED:
					break;
				case INTERRUPTED:
					throw new Error.INVALID_OPERATION ("Session was interrupted; call resume()");
				case DETACHED:
					throw new Error.INVALID_OPERATION ("Session is gone");
			}
		}

		internal void _on_detached (SessionDetachReason reason, CrashInfo crash) {
			if (persist_timeout != 0 && reason == CONNECTION_TERMINATED) {
				if (state != ATTACHED)
					return;
				state = INTERRUPTED;
				delivery_cancellable.cancel ();
				detached (reason, null);
			} else {
				_do_close.begin (reason, crash, false, null);
			}
		}

		internal async void _do_close (SessionDetachReason reason, CrashInfo crash, bool may_block,
				Cancellable? cancellable) throws IOError {
			while (close_request != null) {
				try {
					yield close_request.future.wait_async (cancellable);
					return;
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					cancellable.set_error_if_cancelled ();
				}
			}
			close_request = new Promise<bool> ();

			state = DETACHED;

			try {
				foreach (var script in scripts.values.to_array ())
					yield script._do_close (may_block, cancellable);

				if (may_block)
					close_session_and_peer_connection.begin (cancellable);
				else
					discard_peer_connection ();

				yield device._release_session (this, may_block, cancellable);

				detached (reason, Crash.from_info (crash));

				close_request.resolve (true);
			} catch (IOError e) {
				close_request.reject (e);
				close_request = null;
				throw e;
			}
		}

		private async void close_session_and_peer_connection (Cancellable? cancellable) throws IOError {
			try {
				yield active_session.close (cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED) {
					discard_peer_connection ();
					return;
				}
			}

			yield teardown_peer_connection (cancellable);
		}

		private void begin_migration (AgentSession new_session) {
			obsolete_session = active_session;
			active_session = new_session;
		}

#if HAVE_NICE
		private void cancel_migration (AgentSession new_session) {
			active_session = obsolete_session;
			obsolete_session = null;
		}
#endif

		public DBusConnection _get_connection () {
			return ((DBusProxy) active_session).g_connection;
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class SessionTask<T> : AsyncTask<T> {
			public weak Session parent {
				get;
				construct;
			}
		}
	}

	/**
	 * A piece of JavaScript loaded into the target process, created through a
	 * {@link Session}.
	 */
	public sealed class Script : Object {
		/**
		 * Emitted when the script has been unloaded and is no longer usable.
		 */
		public signal void destroyed ();
		/**
		 * Emitted when the script sends a message to the host.
		 *
		 * @param json the message as a JSON string
		 * @param data binary data accompanying the message, if any
		 */
		public signal void message (string json, Bytes? data);

		private AgentScriptId id;
		private unowned Session session;

		private Promise<bool> close_request;

		private Gum.InspectorServer? inspector_server;

		internal Script (Session session, AgentScriptId script_id) {
			Object ();

			this.id = script_id;
			this.session = session;
		}

		/**
		 * Checks whether the script has been unloaded.
		 *
		 * @return true if the script is no longer loaded
		 */
		public bool is_destroyed () {
			return close_request != null;
		}

		/**
		 * Loads and starts running the script in the target process.
		 */
		public async void load (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield session.active_session.load_script (id, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void load_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<LoadTask> ().execute (cancellable);
		}

		private class LoadTask : ScriptTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.load (cancellable);
			}
		}

		/**
		 * Interrupts any JavaScript currently executing in the script, leaving it
		 * loaded and able to run again. Does nothing if nothing is executing.
		 */
		public async void interrupt (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield session.active_session.interrupt_script (id, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void interrupt_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<InterruptTask> ().execute (cancellable);
		}

		private class InterruptTask : ScriptTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.interrupt (cancellable);
			}
		}

		/**
		 * Unloads the script from the target process.
		 */
		public async void unload (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			yield _do_close (true, cancellable);
		}

		public void unload_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<UnloadTask> ().execute (cancellable);
		}

		private class UnloadTask : ScriptTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.unload (cancellable);
			}
		}

		/**
		 * Interrupts any JavaScript currently executing and unloads the script,
		 * even if it is stuck in a long-running or infinite operation.
		 */
		public async void terminate (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield session.active_session.terminate_script (id, cancellable);

				yield _do_close (false, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void terminate_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<TerminateTask> ().execute (cancellable);
		}

		private class TerminateTask : ScriptTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.terminate (cancellable);
			}
		}

		/**
		 * Eternalizes the script so it keeps running after the session is
		 * detached, instead of being unloaded.
		 */
		public async void eternalize (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			try {
				yield session.active_session.eternalize_script (id, cancellable);

				yield _do_close (false, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void eternalize_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<EternalizeTask> ().execute (cancellable);
		}

		private class EternalizeTask : ScriptTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.eternalize (cancellable);
			}
		}

		/**
		 * Posts a message to the script, delivered to its `recv` handlers.
		 *
		 * @param json the message as a JSON string
		 * @param data binary data to accompany the message, if any
		 */
		public void post (string json, Bytes? data = null) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_post (json, data);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_post (json, data);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_post (string json, Bytes? data) {
			if (close_request != null)
				return;

			session._post_to_agent (AgentMessageKind.SCRIPT, id, json, data);
		}

		/**
		 * Enables the JavaScript debugger for this script, listening for an
		 * inspector client.
		 *
		 * @param port the TCP port to listen on, or 0 for the default
		 */
		public async void enable_debugger (uint16 port = 0, Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			if (inspector_server != null)
				throw new Error.INVALID_OPERATION ("Debugger is already enabled");

			inspector_server = (port != 0)
				? new Gum.InspectorServer.with_port (port)
				: new Gum.InspectorServer ();
			inspector_server.message.connect (on_debugger_message_from_frontend);

			try {
				yield session.active_session.enable_debugger (id, cancellable);
			} catch (GLib.Error e) {
				inspector_server = null;

				throw_dbus_error (e);
			}

			if (inspector_server != null) {
				try {
					inspector_server.start ();
				} catch (Gum.Error e) {
					inspector_server = null;

					try {
						yield session.active_session.disable_debugger (id, cancellable);
					} catch (GLib.Error e) {
					}

					throw new Error.ADDRESS_IN_USE ("%s", e.message);
				}
			}
		}

		public void enable_debugger_sync (uint16 port = 0, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<EnableScriptDebuggerTask> ();
			task.port = port;
			task.execute (cancellable);
		}

		private class EnableScriptDebuggerTask : ScriptTask<void> {
			public uint16 port;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.enable_debugger (port, cancellable);
			}
		}

		/**
		 * Disables the JavaScript debugger for this script.
		 */
		public async void disable_debugger (Cancellable? cancellable = null) throws Error, IOError {
			check_open ();

			if (inspector_server == null)
				return;

			inspector_server.message.disconnect (on_debugger_message_from_frontend);
			inspector_server.stop ();
			inspector_server = null;

			try {
				yield session.active_session.disable_debugger (id, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void disable_debugger_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<DisableScriptDebuggerTask> ().execute (cancellable);
		}

		private class DisableScriptDebuggerTask : ScriptTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.disable_debugger (cancellable);
			}
		}

		private void on_debugger_message_from_frontend (string message) {
			session._post_to_agent (AgentMessageKind.DEBUGGER, id, message);
		}

		internal void on_debugger_message_from_backend (string message) {
			if (inspector_server != null)
				inspector_server.post_message (message);
		}

		private void check_open () throws Error {
			if (close_request != null)
				throw new Error.INVALID_OPERATION ("Script is destroyed");
		}

		internal async void _do_close (bool may_block, Cancellable? cancellable) throws IOError {
			while (close_request != null) {
				try {
					yield close_request.future.wait_async (cancellable);
					return;
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					cancellable.set_error_if_cancelled ();
				}
			}
			close_request = new Promise<bool> ();

			var parent = session;

			parent._release_script (id);

			if (inspector_server != null) {
				inspector_server.message.disconnect (on_debugger_message_from_frontend);
				inspector_server.stop ();
				inspector_server = null;
			}

			if (may_block) {
				try {
					yield parent.active_session.destroy_script (id, cancellable);
				} catch (GLib.Error e) {
				}
			}

			destroyed ();

			close_request.resolve (true);
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class ScriptTask<T> : AsyncTask<T> {
			public weak Script parent {
				get;
				construct;
			}
		}
	}

	/**
	 * A handle to a portal a session has joined via {@link Session.join_portal}.
	 */
	public sealed class PortalMembership : Object {
		private uint id;
		private Session session;

		internal PortalMembership (Session session, PortalMembershipId membership_id) {
			Object ();

			this.id = membership_id.handle;
			this.session = session;
		}

		/**
		 * Leaves the portal, ending this membership.
		 */
		public async void terminate (Cancellable? cancellable = null) throws Error, IOError {
			try {
				yield session.active_session.leave_portal (PortalMembershipId (id), cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public void terminate_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<TerminateTask> ().execute (cancellable);
		}

		private class TerminateTask : PortalMembershipTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.terminate (cancellable);
			}
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class PortalMembershipTask<T> : AsyncTask<T> {
			public weak PortalMembership parent {
				get;
				construct;
			}
		}
	}

	/**
	 * Injects shared libraries into processes, the lower-level primitive
	 * underlying agent injection.
	 */
	public interface Injector : Object {
		/**
		 * Emitted when an injected library has been unloaded.
		 *
		 * @param id the injection ID returned when the library was injected
		 */
		public signal void uninjected (uint id);

		/**
		 * Creates an injector backed by a helper process, suitable for
		 * injecting into other processes.
		 *
		 * @return the new injector
		 */
		public static Injector new () {
#if HAVE_LOCAL_BACKEND
#if WINDOWS
			var tempdir = new TemporaryDirectory ();
			var helper = new WindowsHelperProcess (tempdir);
			return new Winjector (helper, true, tempdir);
#endif
#if DARWIN
			var tempdir = new TemporaryDirectory ();
			var helper = new DarwinHelperProcess (tempdir);
			return new Fruitjector (helper, true, tempdir);
#endif
#if LINUX
			var tempdir = new TemporaryDirectory ();
			var helper = new LinuxHelperProcess (tempdir);
			return new Linjector (helper, true, tempdir);
#endif
#if FREEBSD
			return new Binjector ();
#endif
#if QNX
			return new Qinjector ();
#endif
#else
			assert_not_reached ();
#endif
		}

		/**
		 * Creates an in-process injector, for injecting into the current
		 * process.
		 *
		 * @return the new injector
		 */
		public static Injector new_inprocess () {
#if HAVE_LOCAL_BACKEND
#if WINDOWS
			var tempdir = new TemporaryDirectory ();
			var helper = new WindowsHelperBackend (PrivilegeLevel.NORMAL);
			return new Winjector (helper, true, tempdir);
#endif
#if DARWIN
			var tempdir = new TemporaryDirectory ();
			var helper = new DarwinHelperBackend ();
			return new Fruitjector (helper, true, tempdir);
#endif
#if LINUX
			var tempdir = new TemporaryDirectory ();
			var helper = new LinuxHelperBackend ();
			return new Linjector (helper, true, tempdir);
#endif
#if FREEBSD
			return new Binjector ();
#endif
#if QNX
			return new Qinjector ();
#endif
#else
			assert_not_reached ();
#endif
		}

		/**
		 * Closes the injector, releasing its resources.
		 */
		public abstract async void close (Cancellable? cancellable = null) throws IOError;

		public void close_sync (Cancellable? cancellable = null) throws IOError {
			try {
				((CloseTask) create<CloseTask> ()).execute (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private class CloseTask : InjectorTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.close (cancellable);
			}
		}

		/**
		 * Injects a shared library from a file into a process.
		 *
		 * @param pid the process ID to inject into
		 * @param path path to the library
		 * @param entrypoint name of the entrypoint function to call
		 * @param data a string passed to the entrypoint
		 * @return an injection ID, later matched by {@link Injector.uninjected}
		 */
		public abstract async uint inject_library_file (uint pid, string path, string entrypoint, string data,
			Cancellable? cancellable = null) throws Error, IOError;

		public uint inject_library_file_sync (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<InjectLibraryFileTask> () as InjectLibraryFileTask;
			task.pid = pid;
			task.path = path;
			task.entrypoint = entrypoint;
			task.data = data;
			return task.execute (cancellable);
		}

		private class InjectLibraryFileTask : InjectorTask<uint> {
			public uint pid;
			public string path;
			public string entrypoint;
			public string data;

			protected override async uint perform_operation () throws Error, IOError {
				return yield parent.inject_library_file (pid, path, entrypoint, data, cancellable);
			}
		}

		/**
		 * Injects a shared library from an in-memory blob into a process.
		 *
		 * @param pid the process ID to inject into
		 * @param blob the library image
		 * @param entrypoint name of the entrypoint function to call
		 * @param data a string passed to the entrypoint
		 * @return an injection ID, later matched by {@link Injector.uninjected}
		 */
		public abstract async uint inject_library_blob (uint pid, Bytes blob, string entrypoint, string data,
			Cancellable? cancellable = null) throws Error, IOError;

		public uint inject_library_blob_sync (uint pid, Bytes blob, string entrypoint, string data,
				Cancellable? cancellable = null) throws Error, IOError {
			var task = create<InjectLibraryBlobTask> () as InjectLibraryBlobTask;
			task.pid = pid;
			task.blob = blob;
			task.entrypoint = entrypoint;
			task.data = data;
			return task.execute (cancellable);
		}

		private class InjectLibraryBlobTask : InjectorTask<uint> {
			public uint pid;
			public Bytes blob;
			public string entrypoint;
			public string data;

			protected override async uint perform_operation () throws Error, IOError {
				return yield parent.inject_library_blob (pid, blob, entrypoint, data, cancellable);
			}
		}

		/**
		 * Stops monitoring an injected library, so its unload is no longer
		 * tracked.
		 *
		 * @param id the injection ID
		 */
		public abstract async void demonitor (uint id, Cancellable? cancellable = null) throws Error, IOError;

		public void demonitor_sync (uint id, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<DemonitorTask> () as DemonitorTask;
			task.id = id;
			task.execute (cancellable);
		}

		private class DemonitorTask : InjectorTask<void> {
			public uint id;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.demonitor (id, cancellable);
			}
		}

		/**
		 * Stops monitoring an injection and clones its state into a new
		 * injection that inherits it.
		 *
		 * @param id the injection ID
		 * @return the new injection ID
		 */
		public abstract async uint demonitor_and_clone_state (uint id, Cancellable? cancellable = null) throws Error, IOError;

		public uint demonitor_and_clone_state_sync (uint id, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<DemonitorAndCloneStateTask> () as DemonitorAndCloneStateTask;
			task.id = id;
			return task.execute (cancellable);
		}

		private class DemonitorAndCloneStateTask : InjectorTask<uint> {
			public uint id;

			protected override async uint perform_operation () throws Error, IOError {
				return yield parent.demonitor_and_clone_state (id, cancellable);
			}
		}

		/**
		 * Recreates the thread used by an injection in the target process, for
		 * example after the process has forked.
		 *
		 * @param pid the process ID
		 * @param id the injection ID
		 */
		public abstract async void recreate_thread (uint pid, uint id, Cancellable? cancellable = null) throws Error, IOError;

		public void recreate_thread_sync (uint pid, uint id, Cancellable? cancellable = null) throws Error, IOError {
			var task = create<RecreateThreadTask> () as RecreateThreadTask;
			task.pid = pid;
			task.id = id;
			task.execute (cancellable);
		}

		private class RecreateThreadTask : InjectorTask<void> {
			public uint pid;
			public uint id;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.recreate_thread (pid, id, cancellable);
			}
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class InjectorTask<T> : AsyncTask<T> {
			public weak Injector parent {
				get;
				construct;
			}
		}
	}

#if !HAVE_EMBEDDED_ASSETS
	internal string helper_path;
	internal string agent_path;
#if COMPILER_BACKEND_INSTALLED_LIBRARY || COMPILER_BACKEND_INSTALLED_EXECUTABLE
	internal string compiler_backend_path;
#endif

	public void _init_asset_paths () {
		var location = AssetLocation.detect ();
		helper_path = location.derive_asset_path ("<arch>", Config.FRIDA_HELPER_NAME);
		agent_path = location.derive_asset_path ("<arch>", Config.FRIDA_AGENT_NAME);
#if COMPILER_BACKEND_INSTALLED_LIBRARY || COMPILER_BACKEND_INSTALLED_EXECUTABLE
		compiler_backend_path = location.derive_plugin_path (Config.FRIDA_COMPILER_BACKEND_NAME);
#endif
	}

	public void _deinit_asset_paths () {
		helper_path = null;
		agent_path = null;
#if COMPILER_BACKEND_INSTALLED_LIBRARY || COMPILER_BACKEND_INSTALLED_EXECUTABLE
		compiler_backend_path = null;
#endif
	}
#endif

#if HAVE_FRIDA_GLIB
	private Mutex gc_mutex;
	private uint gc_generation = 0;
	private bool gc_scheduled = false;
#endif

	public void on_pending_garbage (void * data) {
#if HAVE_FRIDA_GLIB
		gc_mutex.lock ();
		gc_generation++;
		bool already_scheduled = gc_scheduled;
		gc_scheduled = true;
		gc_mutex.unlock ();

		if (already_scheduled)
			return;

		Timeout.add (50, () => {
			gc_mutex.lock ();
			uint generation = gc_generation;
			gc_mutex.unlock ();

			bool collected_everything = Thread.garbage_collect ();

			gc_mutex.lock ();
			bool same_generation = generation == gc_generation;
			bool repeat = !collected_everything || !same_generation;
			if (!repeat)
				gc_scheduled = false;
			gc_mutex.unlock ();

			return repeat;
		});
#endif
	}
}
