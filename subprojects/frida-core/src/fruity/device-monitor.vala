[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class DeviceMonitor : Object {
		public signal void device_attached (Device device);
		public signal void device_detached (Device device);

		private State state = CREATED;
		private Gee.List<Backend> backends = new Gee.ArrayList<Backend> ();
		private Gee.Map<string, Device> devices = new Gee.HashMap<string, Device> ();

#if !MACOS
		private PairingStore pairing_store = new PairingStore ();
#endif

		private enum State {
			CREATED,
			STARTING,
			STARTED,
			STOPPED,
		}

		private delegate void NotifyCompleteFunc ();

		construct {
#if MACOS
			add_backend (new UsbmuxBackend ());
			add_backend (new MacOSCoreDeviceBackend ());
#else
			add_backend (new UsbmuxBackend (pairing_store));
			add_backend (new PortableCoreDeviceBackend (pairing_store));
#endif
		}

		public async void start (Cancellable? cancellable = null) throws IOError {
			state = STARTING;

			var remaining = backends.size + 1;

			NotifyCompleteFunc on_complete = () => {
				remaining--;
				if (remaining == 0)
					start.callback ();
			};

			foreach (var backend in backends)
				do_start.begin (backend, cancellable, on_complete);

			var source = new IdleSource ();
			source.set_callback (() => {
				on_complete ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());

			yield;

			on_complete = null;

#if !MACOS
			var b = (PortableCoreDeviceBackend) backends.first_match (b => b is PortableCoreDeviceBackend);
			if (b != null && b.supports_modeswitch)
				yield b.activate_modeswitch_support (cancellable);
#endif

			state = STARTED;

			foreach (var device in devices.values)
				device_attached (device);
		}

		public async void stop (Cancellable? cancellable = null) throws IOError {
			var remaining = backends.size + 1;

			NotifyCompleteFunc on_complete = () => {
				remaining--;
				if (remaining == 0)
					stop.callback ();
			};

			foreach (var backend in backends)
				do_stop.begin (backend, cancellable, on_complete);

			var source = new IdleSource ();
			source.set_callback (() => {
				on_complete ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());

			yield;

			on_complete = null;

			foreach (var device in devices.values)
				device.close ();
			devices.clear ();

			state = STOPPED;
		}

		private async void do_start (Backend backend, Cancellable? cancellable, NotifyCompleteFunc on_complete) {
			try {
				yield backend.start (cancellable);
			} catch (IOError e) {
			}

			on_complete ();
		}

		private async void do_stop (Backend backend, Cancellable? cancellable, NotifyCompleteFunc on_complete) {
			try {
				yield backend.stop (cancellable);
			} catch (IOError e) {
			}

			on_complete ();
		}

		private void add_backend (Backend backend) {
			backends.add (backend);
			backend.transport_attached.connect (on_transport_attached);
			backend.transport_detached.connect (on_transport_detached);
		}

		private void on_transport_attached (Transport transport) {
			unowned string udid = transport.udid;

			var device = devices[udid];
			if (device == null) {
				device = new Device ();
				devices[udid] = device;
			}

			device.transports.add (transport);

			if (state != STARTING && device.transports.size == 1)
				device_attached (device);
		}

		private void on_transport_detached (Transport transport) {
			unowned string udid = transport.udid;

			var device = devices[udid];
			device.transports.remove (transport);
			if (device.transports.is_empty) {
				devices.unset (udid);

				if (state != STARTING)
					device_detached (device);
			}
		}
	}

	public sealed class Device : Object, HostChannelProvider {
		public ConnectionType connection_type {
			get {
				return (transports.first_match (t => t.connection_type == USB) != null)
					? ConnectionType.USB
					: ConnectionType.NETWORK;
			}
		}

		public string udid {
			get {
				foreach (var transport in transports)
					return transport.udid;
				assert_not_reached ();
			}
		}

		public string name {
			get {
				var transport = transports.first_match (t => t.name != null);
				if (transport == null)
					return "iOS Device";
				return transport.name;
			}
		}

		public Variant? icon {
			get {
				var transport = transports.first_match (t => t.icon != null);
				if (transport == null)
					return null;
				return transport.icon;
			}
		}

		public Gee.Set<Transport> transports {
			get;
			default = new Gee.TreeSet<Transport> (compare_transports);
		}

		private const string[] LOCKDOWN_SERVICES_WITHOUT_ESCROW_BAG_SUPPORT = {
			"com.apple.accessibility.axAuditDaemon.remoteserver",
			"com.apple.afc",
			"com.apple.companion_proxy",
			"com.apple.crashreportcopymobile",
			"com.apple.GPUTools.MobileService",
			"com.apple.idamd",
			"com.apple.PurpleReverseProxy.Conn",
			"com.apple.streaming_zip_conduit",
			"com.apple.webinspector",
		};

		internal void close () {
			transports.clear ();
		}

		public UsbmuxDevice? find_usbmux_device () {
			var t = transports.first_match (t => t.usbmux_device != null);
			return (t != null) ? t.usbmux_device : null;
		}

		private UsbmuxTransport get_usbmux_transport () throws Error {
			var t = transports.first_match (t => t is UsbmuxTransport);
			if (t == null)
				throw new Error.NOT_SUPPORTED ("USB connection not available");
			return (UsbmuxTransport) t;
		}

		public async Tunnel? find_tunnel (Cancellable? cancellable) throws Error, IOError {
			var usbmux_device = find_usbmux_device ();

			var transports_to_try = new Gee.ArrayList<Transport> ();
			transports_to_try.add_all_iterator (transports.filter (t => t.usbmux_device == null));
			transports_to_try.add_all_iterator (transports.filter (t => t.usbmux_device != null));

			foreach (var transport in transports_to_try) {
				Tunnel? tunnel = yield transport.find_tunnel (usbmux_device, cancellable);
				if (tunnel != null)
					return tunnel;
			}

			return null;
		}

		public async LockdownClient get_lockdown_client (Cancellable? cancellable) throws Error, IOError {
			var stream = yield open_lockdown_service ("", cancellable);
			return new LockdownClient (stream);
		}

		public async IOStream open_lockdown_service (string service_name, Cancellable? cancellable) throws Error, IOError {
			var tunnel = yield find_tunnel (cancellable);
			if (tunnel != null) {
				ServiceInfo? service_info = null;
				bool needs_checkin = service_name == "";
				try {
					service_info = tunnel.discovery.get_service (
						(service_name == "") ? "com.apple.mobile.lockdown.remote.trusted" : service_name);
				} catch (Error e) {
					if (!(e is Error.NOT_SUPPORTED))
						throw e;
				}
				if (service_info == null) {
					service_info = tunnel.discovery.get_service (service_name + ".shim.remote");
					needs_checkin = true;
				}

				var stream = yield tunnel.open_tcp_connection (service_info.port, cancellable);

				if (needs_checkin) {
					var service = new PlistServiceClient (stream);

					var checkin = new Plist ();
					checkin.set_string ("Request", "RSDCheckin");
					checkin.set_string ("Label", "Xcode");
					checkin.set_string ("ProtocolVersion", "2");
					unowned Bytes? key = tunnel.remote_unlock_host_key;
					if (key != null && lockdown_service_supports_escrow_bag (service_name))
						checkin.set_bytes ("EscrowBag", key);

					try {
						yield service.query (checkin, cancellable);

						var result = yield service.read_message (cancellable);
						if (result.has ("Error")) {
							var error_type = result.get_string ("Error");
							if (error_type == "ServiceProhibited")
								throw new Error.PERMISSION_DENIED ("Service prohibited");
							throw new Error.NOT_SUPPORTED ("%s", error_type);
						}
					} catch (PlistServiceError e) {
						throw new Error.PROTOCOL ("%s", e.message);
					} catch (PlistError e) {
						throw new Error.PROTOCOL ("%s", e.message);
					}
				}

				return stream;
			}

			return yield get_usbmux_transport ().open_lockdown_service (service_name, cancellable);
		}

		// FIXME: Replace with `element in array`-check once Vala compiler bug has been fixed so generated C code is warning-free.
		private static bool lockdown_service_supports_escrow_bag (string name) {
			foreach (unowned string s in LOCKDOWN_SERVICES_WITHOUT_ESCROW_BAG_SUPPORT) {
				if (s == name)
					return false;
			}
			return true;
		}

		public async IOStream open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			string[] tokens = address.split (":", 2);
			unowned string protocol = tokens[0];
			unowned string location = tokens[1];

			if (protocol == "tcp") {
				var channel = yield open_tcp_channel (location, ALLOW_ANY_TRANSPORT, cancellable);
				return channel.stream;
			}

			if (protocol == "lockdown")
				return yield open_lockdown_service (location, cancellable);

			throw new Error.NOT_SUPPORTED ("Unsupported channel address");
		}

		public async TcpChannel open_tcp_channel (string location, OpenTcpChannelFlags flags, Cancellable? cancellable)
				throws Error, IOError {
			var usbmux_device = find_usbmux_device ();
			var tunnel = yield find_tunnel (cancellable);

			uint16 port;
			ulong raw_port;
			if (ulong.try_parse (location, out raw_port)) {
				if (raw_port == 0 || raw_port > uint16.MAX)
					throw new Error.INVALID_ARGUMENT ("Invalid TCP port");
				port = (uint16) raw_port;
			} else {
				if (tunnel == null)
					throw new Error.NOT_SUPPORTED ("Unable to resolve port name; tunnel not available");
				if ((flags & OpenTcpChannelFlags.ALLOW_TUNNEL) == 0)
					throw new Error.NOT_SUPPORTED ("Connection to tunnel service not allowed by flags");
				var service_info = tunnel.discovery.get_service (location);
				port = service_info.port;
			}

			Error? pending_error = null;

			if ((flags & OpenTcpChannelFlags.ALLOW_TUNNEL) != 0 && tunnel != null) {
				try {
					var stream = yield tunnel.open_tcp_connection (port, cancellable);
					return new TcpChannel () { stream = stream, kind = TUNNEL };
				} catch (Error e) {
					if (e is Error.SERVER_NOT_RUNNING || e is Error.TRANSPORT)
						pending_error = e;
					else
						throw e;
				}
			}

			if ((flags & OpenTcpChannelFlags.ALLOW_USBMUX) != 0 && usbmux_device != null) {
				if (usbmux_device.connection_type == USB) {
					UsbmuxClient client = null;
					try {
						client = yield UsbmuxClient.open (cancellable);

						yield client.connect_to_port (usbmux_device.id, port, cancellable);

						return new TcpChannel () { stream = client.connection, kind = USBMUX };
					} catch (GLib.Error e) {
						if (client != null)
							client.close.begin ();

						if (e is UsbmuxError.CONNECTION_REFUSED)
							throw new Error.SERVER_NOT_RUNNING ("%s", e.message);

						throw new Error.TRANSPORT ("%s", e.message);
					}
				}

				InetSocketAddress device_address = usbmux_device.network_address;
				var target_address = (InetSocketAddress) Object.new (typeof (InetSocketAddress),
					address: device_address.address,
					port: port,
					flowinfo: device_address.flowinfo,
					scope_id: device_address.scope_id
				);

				var client = new SocketClient ();
				try {
					var connection = yield client.connect_async (target_address, cancellable);

					Tcp.enable_nodelay (connection.socket);

					return new TcpChannel () { stream = connection, kind = USBMUX };
				} catch (GLib.Error e) {
					if (e is IOError.CONNECTION_REFUSED)
						throw new Error.SERVER_NOT_RUNNING ("%s", e.message);

					throw new Error.TRANSPORT ("%s", e.message);
				}
			}

			if (pending_error != null)
				throw pending_error;
			throw new Error.TRANSPORT ("No viable transport found");
		}

		private static int compare_transports (Transport a, Transport b) {
			int res = score_transport (b) - score_transport (a);
			if (res != 0)
				return res;

			if (a == b)
				return 0;

			uintptr ap = (uintptr) (void *) a;
			uintptr bp = (uintptr) (void *) b;

			return (ap < bp) ? -1 : 1;
		}

		private static int score_transport (Transport t) {
			int score = 0;
			if (t.connection_type == USB)
				score++;
			if (t.usbmux_device != null)
				score++;
			return score;
		}
	}

	public class TcpChannel {
		public IOStream stream;
		public Kind kind;

		public enum Kind {
			USBMUX,
			TUNNEL
		}
	}

	[Flags]
	public enum OpenTcpChannelFlags {
		ALLOW_USBMUX,
		ALLOW_TUNNEL,
		ALLOW_ANY_TRANSPORT = ALLOW_USBMUX | ALLOW_TUNNEL,
	}

	public interface Transport : Object {
		public abstract ConnectionType connection_type {
			get;
		}

		public abstract string udid {
			get;
		}

		public abstract string? name {
			get;
		}

		public abstract Variant? icon {
			get;
		}

		public abstract UsbmuxDevice? usbmux_device {
			get;
		}

		public abstract async Tunnel? find_tunnel (UsbmuxDevice? device, Cancellable? cancellable) throws Error, IOError;
	}

	public enum ConnectionType {
		USB,
		NETWORK
	}

	public interface Tunnel : Object {
		public abstract DiscoveryService discovery {
			get;
		}

		public abstract int64 opened_at {
			get;
		}

		public abstract Bytes? remote_unlock_host_key {
			get;
		}

		public abstract async void close (Cancellable? cancellable) throws IOError;
		public abstract async IOStream open_tcp_connection (uint16 port, Cancellable? cancellable) throws Error, IOError;
	}

	public interface Backend : Object {
		public signal void transport_attached (Transport transport);
		public signal void transport_detached (Transport transport);

		public abstract async void start (Cancellable? cancellable) throws IOError;
		public abstract async void stop (Cancellable? cancellable) throws IOError;
	}

	private sealed class UsbmuxBackend : Object, Backend {
#if !MACOS
		public PairingStore pairing_store {
			get;
			construct;
		}
#endif

		public bool available {
			get {
				return usbmux != null;
			}
		}

		private Gee.Map<UsbmuxDevice, UsbmuxTransport> transports = new Gee.HashMap<UsbmuxDevice, UsbmuxTransport> ();

		private UsbmuxClient? usbmux;

		private Promise<bool> start_request;
		private Cancellable start_cancellable;
		private SourceFunc on_start_completed;

		private Cancellable io_cancellable = new Cancellable ();

#if !MACOS
		public UsbmuxBackend (PairingStore pairing_store) {
			Object (pairing_store: pairing_store);
		}
#endif

		public async void start (Cancellable? cancellable) throws IOError {
			start_request = new Promise<bool> ();
			start_cancellable = new Cancellable ();
			on_start_completed = start.callback;

			var main_context = MainContext.get_thread_default ();

			var timeout_source = new TimeoutSource (500);
			timeout_source.set_callback (start.callback);
			timeout_source.attach (main_context);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (start.callback);
			cancel_source.attach (main_context);

			do_start.begin ();

			yield;

			cancel_source.destroy ();
			timeout_source.destroy ();
			on_start_completed = null;
		}

		private async void do_start () {
			bool success = yield try_open_usbmux_client ();
			if (success) {
				/* Perform a dummy-request to flush out any pending device attach notifications. */
				try {
					yield usbmux.connect_to_port (uint.MAX, 0, start_cancellable);
					assert_not_reached ();
				} catch (GLib.Error expected_error) {
					if (expected_error.code == IOError.CONNECTION_CLOSED) {
						/* Deal with usbmuxd closing the connection when receiving commands in the wrong state. */
						usbmux.close.begin (null);

						success = yield try_open_usbmux_client ();
						if (success) {
							UsbmuxClient flush_client = null;
							try {
								flush_client = yield UsbmuxClient.open (start_cancellable);
								try {
									yield flush_client.connect_to_port (uint.MAX, 0, start_cancellable);
									assert_not_reached ();
								} catch (GLib.Error expected_error) {
								}
							} catch (GLib.Error e) {
								success = false;
							}

							if (flush_client != null)
								flush_client.close.begin (null);

							if (!success && usbmux != null) {
								usbmux.close.begin (null);
								usbmux = null;
							}
						}
					}
				}
			}

			start_request.resolve (success);

			if (on_start_completed != null)
				on_start_completed ();
		}

		private async bool try_open_usbmux_client () {
			bool success = true;

			try {
				usbmux = yield UsbmuxClient.open (start_cancellable);
				usbmux.device_attached.connect (on_device_attached);
				usbmux.device_detached.connect (on_device_detached);

				yield usbmux.enable_listen_mode (start_cancellable);
			} catch (GLib.Error e) {
				success = false;
			}

			if (!success && usbmux != null) {
				usbmux.close.begin (null);
				usbmux = null;
			}

			return success;
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();
			start_cancellable.cancel ();

			try {
				yield start_request.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}

			if (usbmux != null) {
				yield usbmux.close (cancellable);
				usbmux = null;
			}

			transports.clear ();
		}

		private void on_device_attached (UsbmuxDevice device) {
			add_transport.begin (device);
		}

		private void on_device_detached (UsbmuxDevice device) {
			remove_transport (device);
		}

		private async void add_transport (UsbmuxDevice device) {
#if MACOS
			var transport = new UsbmuxTransport (device);
#else
			var transport = new UsbmuxTransport (device, pairing_store);
#endif
			transports[device] = transport;

			string? name = null;
			Variant? icon = null;
			if (device.connection_type == USB) {
				bool got_details = false;
				for (int i = 1; !got_details && transports.has_key (device); i++) {
					try {
						_extract_details_for_device (device.product_id, device.udid, out name, out icon);
						got_details = true;
					} catch (Error e) {
						if (i != 20) {
							var main_context = MainContext.get_thread_default ();

							var delay_source = new TimeoutSource.seconds (1);
							delay_source.set_callback (add_transport.callback);
							delay_source.attach (main_context);

							var cancel_source = new CancellableSource (io_cancellable);
							cancel_source.set_callback (add_transport.callback);
							cancel_source.attach (main_context);

							yield;

							cancel_source.destroy ();
							delay_source.destroy ();

							if (io_cancellable.is_cancelled ())
								return;
						} else {
							break;
						}
					}
				}
				if (!transports.has_key (device))
					return;
				if (!got_details) {
					remove_transport (device);
					return;
				}
			} else {
				name = "iOS Device [%s]".printf (device.network_address.address.to_string ());
			}
			transport._name = (owned) name;
			transport._icon = (owned) icon;

			transport_attached (transport);
		}

		private void remove_transport (UsbmuxDevice device) {
			UsbmuxTransport transport;
			transports.unset (device, out transport);
			transport_detached (transport);
		}

		public extern static void _extract_details_for_device (int product_id, string udid, out string name,
			out Variant? icon) throws Error;
	}

	private sealed class UsbmuxTransport : Object, Transport {
		public UsbmuxDevice device {
			get;
			construct;
		}

#if !MACOS
		public PairingStore pairing_store {
			get;
			construct;
		}
#endif

		public ConnectionType connection_type {
			get {
				return device.connection_type;
			}
		}

		public string udid {
			get {
				return device.udid;
			}
		}

		public string? name {
			get {
				return _name;
			}
		}

		public Variant? icon {
			get {
				return _icon;
			}
		}

		public UsbmuxDevice? usbmux_device {
			get {
				return device;
			}
		}

		internal string _name;
		internal Variant? _icon;

#if !MACOS
		private Promise<Tunnel?>? tunnel_request;
#endif

		private Gee.Queue<UsbmuxLockdownServiceRequest> lockdown_service_requests =
			new Gee.ArrayQueue<UsbmuxLockdownServiceRequest> ();
		private LockdownClient? cached_lockdown_client;

#if MACOS
		public UsbmuxTransport (UsbmuxDevice device) {
			Object (device: device);
		}
#else
		public UsbmuxTransport (UsbmuxDevice device, PairingStore store) {
			Object (device: device, pairing_store: store);
		}
#endif

		public async Tunnel? find_tunnel (UsbmuxDevice? device, Cancellable? cancellable) throws Error, IOError {
#if !MACOS
			while (tunnel_request != null) {
				try {
					return yield tunnel_request.future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			tunnel_request = new Promise<Tunnel> ();

			try {
				IOStream? stream = null;
				try {
					stream = yield open_lockdown_service ("com.apple.internal.devicecompute.CoreDeviceProxy", cancellable);
				} catch (Error e) {
					if (!(e is Error.NOT_SUPPORTED))
						throw e;
				}

				UsbmuxTunnel? tunnel = null;
				if (stream != null) {
					tunnel = new UsbmuxTunnel (stream, pairing_store);
					tunnel.lost.connect (on_tunnel_lost);
					try {
						yield tunnel.open (cancellable);
					} catch (Error e) {
						if (e is Error.NOT_SUPPORTED)
							tunnel = null;
						else
							throw e;
					}
				}

				tunnel_request.resolve (tunnel);

				return tunnel;
			} catch (GLib.Error e) {
				tunnel_request.reject (e);
				tunnel_request = null;

				throw_api_error (e);
			}
#else
			return null;
#endif
		}

#if !MACOS
		private void on_tunnel_lost () {
			if (tunnel_request.future.ready)
				tunnel_request = null;
		}
#endif

		public async IOStream open_lockdown_service (string service_name, Cancellable? cancellable) throws Error, IOError {
			if (service_name == "") {
				var client = yield open_usbmux_lockdown_client (cancellable);
				return client.service.stream;
			}

			var request = new UsbmuxLockdownServiceRequest (service_name, cancellable);
			bool first_request = lockdown_service_requests.is_empty;
			lockdown_service_requests.offer (request);

			if (first_request)
				process_lockdown_service_requests.begin ();

			return yield request.promise.future.wait_async (cancellable);
		}

		private async void process_lockdown_service_requests () {
			UsbmuxLockdownServiceRequest? req;
			bool already_invalidated = false;
			while ((req = lockdown_service_requests.peek ()) != null) {
				try {
					if (cached_lockdown_client == null)
						cached_lockdown_client = yield open_usbmux_lockdown_client (req.cancellable);
					var stream = yield cached_lockdown_client.start_service (req.service_name, req.cancellable);
					req.promise.resolve (stream);
				} catch (GLib.Error e) {
					if (e is LockdownError.CONNECTION_CLOSED && cached_lockdown_client != null &&
							!already_invalidated) {
						cached_lockdown_client = null;
						already_invalidated = true;
						continue;
					}
					req.promise.reject ((e is LockdownError.INVALID_SERVICE)
						? (Error) new Error.NOT_SUPPORTED ("%s", e.message)
						: (Error) new Error.TRANSPORT ("%s", e.message));
				}

				lockdown_service_requests.poll ();
			}
		}

		private async LockdownClient open_usbmux_lockdown_client (Cancellable? cancellable) throws Error, IOError {
			try {
				var client = yield LockdownClient.open (device, cancellable);
				yield client.start_session (cancellable);
				return client;
			} catch (LockdownError e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
		}

		private class UsbmuxLockdownServiceRequest {
			public string service_name;
			public Cancellable? cancellable;
			public Promise<IOStream> promise = new Promise<IOStream> ();

			public UsbmuxLockdownServiceRequest (string service_name, Cancellable? cancellable) {
				this.service_name = service_name;
				this.cancellable = cancellable;
			}
		}
	}

	public interface FruitFinder : Object {
		public static FruitFinder make_default () {
#if LINUX && !ANDROID
			return new LinuxFruitFinder ();
#else
			return new NullFruitFinder ();
#endif
		}

		public abstract string? udid_from_iface (string ifname) throws Error;
	}

	public sealed class NullFruitFinder : Object, FruitFinder {
		public string? udid_from_iface (string ifname) throws Error {
			return null;
		}
	}

	public interface PairingBrowser : Object {
		public const string SERVICE_NAME = "_remotepairing._tcp.local";

		public signal void service_discovered (PairingServiceDetails service);

		public static PairingBrowser make_default () {
#if WINDOWS
			return new WindowsPairingBrowser ();
#elif LINUX && !ANDROID
			return new LinuxPairingBrowser ();
#else
			return new NullPairingBrowser ();
#endif
		}

		public abstract async void start (Cancellable? cancellable) throws IOError;
		public abstract async void stop (Cancellable? cancellable) throws IOError;
	}

	public sealed class NullPairingBrowser : Object, PairingBrowser {
		public async void start (Cancellable? cancellable) throws IOError {
		}

		public async void stop (Cancellable? cancellable) throws IOError {
		}
	}

	public class PairingServiceDetails {
		public string identifier;
		public Bytes auth_tag;
		public InetSocketAddress endpoint;
		public InetSocketAddress interface_address;
	}

}
