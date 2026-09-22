[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class MacOSCoreDeviceBackend : Object, Backend {
		private Gee.Map<string, MacOSCoreDeviceTransport> transports = new Gee.HashMap<string, MacOSCoreDeviceTransport> ();

		private Promise<bool> all_current_devices_listed = new Promise<bool> ();
		private Promise<bool> browse_request = new Promise<bool> ();

		private XpcClient? pairingd;
		private Darwin.GCD.DispatchQueue queue =
			new Darwin.GCD.DispatchQueue ("re.frida.fruity.remotepairing", Darwin.GCD.DispatchQueueAttr.SERIAL);

		public async void start (Cancellable? cancellable) throws IOError {
			pairingd = XpcClient.make_for_mach_service ("com.apple.CoreDevice.remotepairingd", queue);
			pairingd.notify["state"].connect (on_state_changed);
			pairingd.message.connect (on_message);

			do_browse.begin ();

			try {
				yield all_current_devices_listed.future.wait_async (cancellable);
			} catch (Error e) {
			}
		}

		private async void do_browse () {
			try {
				var r = new PairingdRequest ("RemotePairing.BrowseRequest");
				r.body.set_bool ("currentDevicesOnly", false);
				yield pairingd.request (r.message, null);
				browse_request.resolve (true);
			} catch (GLib.Error e) {
				browse_request.reject (e);
			}
		}

		public async void stop (Cancellable? cancellable) throws IOError {
		}

		private void on_state_changed (Object obj, ParamSpec pspec) {
			if (pairingd.state == CLOSED && !all_current_devices_listed.future.ready) {
				all_current_devices_listed.reject (
					new Error.TRANSPORT ("Connection closed while waiting for initial device list"));
			}
		}

		private void on_message (Darwin.Xpc.Object obj) {
			var reader = new XpcObjectReader (obj);
			try {
				reader.read_member ("mangledTypeName");
				if (reader.get_string_value () == "RemotePairing.ServiceEvent") {
					reader
						.end_member ()
						.read_member ("value");
					if (reader.try_read_member ("deviceFound")) {
						reader
							.read_member ("_0")
							.read_member ("deviceInfo");

						var udid = reader.read_member ("udid").get_string_value ();
						reader.end_member ();
						if (transports.has_key (udid))
							return;

						var device_info = (Darwin.Xpc.Dictionary) reader.current_object;

						var pairing_device = new XpcClient (device_info.create_connection ("endpoint"), queue);

						on_device_found (udid, pairing_device, device_info);
					} else if (reader.try_read_member ("allCurrentDevicesListed")) {
						all_current_devices_listed.resolve (true);
					}
				}
			} catch (Error e) {
			}
		}

		private void on_device_found (string udid, XpcClient pairing_device, Darwin.Xpc.Dictionary device_info) throws Error {
			var transport = new MacOSCoreDeviceTransport (udid, pairing_device, device_info);
			transport.closed.connect (on_transport_closed);
			transports[udid] = transport;
			transport_attached (transport);
		}

		private void on_transport_closed (MacOSCoreDeviceTransport transport) {
			transport_detached (transport);
			transports.unset (transport.udid);
		}
	}

	public sealed class MacOSCoreDeviceTransport : Object, Transport {
		public signal void closed ();

		public XpcClient pairing_device {
			get;
			construct;
		}

		public ConnectionType connection_type {
			get {
				return _connection_type;
			}
		}

		public string udid {
			get {
				return _udid;
			}
		}

		public string? name {
			get {
				return _name;
			}
		}

		public Variant? icon {
			get {
				return null;
			}
		}

		public UsbmuxDevice? usbmux_device {
			get {
				return null;
			}
		}

		private ConnectionType _connection_type;
		private string _udid;
		private string _name;
		private string _auth_state;
		private Bytes? _remote_unlock_host_key;

		private Promise<Tunnel>? tunnel_request;

		private Cancellable io_cancellable = new Cancellable ();

		public MacOSCoreDeviceTransport (string udid, XpcClient pairing_device, Darwin.Xpc.Dictionary device_info) throws Error {
			Object (pairing_device: pairing_device);
			_udid = udid;
			update_device_info (device_info);
		}

		construct {
			pairing_device.notify["state"].connect (on_state_changed);
			pairing_device.message.connect (on_message);
		}

		private void update_device_info (Darwin.Xpc.Dictionary device_info) throws Error {
			_connection_type = (device_info.get_value ("untrustedRSDDeviceInfo") != null)
				? ConnectionType.USB
				: ConnectionType.NETWORK;

			var reader = new XpcObjectReader (device_info);
			_name = reader.read_member ("name").get_string_value ();
			reader.end_member ();

			_auth_state = reader.read_member ("authState").read_member ("rawCase").get_string_value ();
			reader
				.end_member ()
				.end_member ();

			if (reader.has_member ("remoteUnlockHostKey")) {
				_remote_unlock_host_key = new Bytes (reader.read_member ("remoteUnlockHostKey").get_data_value ());
				reader.end_member ();
			} else {
				_remote_unlock_host_key = null;
			}
		}

		private void on_state_changed (Object obj, ParamSpec pspec) {
			if (pairing_device.state == CLOSED) {
				io_cancellable.cancel ();

				closed ();
			}
		}

		private void on_message (Darwin.Xpc.Object obj) {
			var reader = new XpcObjectReader (obj);
			try {
				reader.read_member ("mangledTypeName");
				if (reader.get_string_value () == "RemotePairing.ServiceEvent") {
					reader
						.end_member ()
						.read_member ("value");
					if (reader.try_read_member ("deviceFound")) {
						reader
							.read_member ("_0")
							.read_member ("deviceInfo");
						var device_info = (Darwin.Xpc.Dictionary)
							reader.get_object_value (Darwin.Xpc.Dictionary.TYPE);
						update_device_info (device_info);
					} else if (reader.try_read_member ("tunnelUsageAssertionsInvalidated")) {
						reader.read_member ("assertionIdentifiers");
						var ids = new Gee.ArrayList<Bytes> ();
						size_t n = reader.count_elements ();
						for (size_t i = 0; i != n; i++) {
							reader.read_element (i);
							ids.add (new Bytes (reader.get_uuid_value ()));
							reader.end_element ();
						}
						on_tunnel_usage_assertions_invalidated.begin (ids);
					}
				}
			} catch (Error e) {
			}
		}

		private async void on_tunnel_usage_assertions_invalidated (Gee.List<Bytes> ids) {
			if (tunnel_request == null)
				return;

			try {
				var tunnel = (MacOSTunnel) yield tunnel_request.future.wait_async (io_cancellable);
				unowned uint8[] tunnel_assertion_id = tunnel.assertion_identifier.get_bytes ();
				foreach (var id in ids) {
					if (Memory.cmp (id.get_data (), tunnel_assertion_id, id.get_size ()) == 0) {
						tunnel_request = null;
						return;
					}
				}
			} catch (GLib.Error e) {
			}
		}

		public async Tunnel? find_tunnel (UsbmuxDevice? device, Cancellable? cancellable) throws Error, IOError {
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
				yield ensure_paired (cancellable);

				var tunnel = new MacOSTunnel (pairing_device, _remote_unlock_host_key);
				yield tunnel.attach (cancellable);

				tunnel_request.resolve (tunnel);

				return tunnel;
			} catch (GLib.Error e) {
				tunnel_request.reject (e);
				tunnel_request = null;

				throw_api_error (e);
			}
		}

		private async void ensure_paired (Cancellable? cancellable) throws Error, IOError {
			if (_auth_state == "authenticated")
				return;

			var r = new PairingdRequest ("RemotePairing.InitiatePairingCommand");
			r.body.set_bool ("requireNonInteractive", false);
			var response = yield pairing_device.request (r.message, cancellable);
			var reader = new XpcObjectReader (response);
			reader.check_nserror (() => Error.PERMISSION_DENIED);
		}
	}

	public sealed class MacOSTunnel : Object, Tunnel {
		public DiscoveryService discovery {
			get {
				return _discovery;
			}
		}

		public int64 opened_at {
			get {
				return _opened_at;
			}
		}

		public Bytes? remote_unlock_host_key {
			get {
				return _remote_unlock_host_key;
			}
		}

		public Darwin.Xpc.Uuid? assertion_identifier {
			get {
				return _assertion_identifier;
			}
		}

		private XpcClient pairing_device;
		private Bytes? _remote_unlock_host_key;
		private Darwin.Xpc.Uuid? _assertion_identifier;
		private InetAddress? tunnel_device_address;
		private DiscoveryService? _discovery;
		private int64 _opened_at = -1;

		public MacOSTunnel (XpcClient pairing_device, Bytes? remote_unlock_host_key) {
			this.pairing_device = pairing_device;
			_remote_unlock_host_key = remote_unlock_host_key;
		}

		public async void close (Cancellable? cancellable) throws IOError {
			var r = new PairingdRequest ("RemotePairing.ReleaseAssertionRequest");
			r.body.set_value ("assertionIdentifier", _assertion_identifier);
			try {
				yield pairing_device.request (r.message, cancellable);
			} catch (Error e) {
			}
		}

		public async void attach (Cancellable? cancellable) throws Error, IOError {
			var r = new PairingdRequest ("RemotePairing.CreateAssertionCommand");
			r.body.set_int64 ("flags", 0);
			var response = yield pairing_device.request (r.message, cancellable);

			_opened_at = get_monotonic_time ();

			var reader = new XpcObjectReader (response);
			reader.check_nserror (() => Error.NOT_SUPPORTED);
			reader.read_member ("response");

			_assertion_identifier = (Darwin.Xpc.Uuid) reader
				.read_member ("assertionIdentifier")
				.get_object_value (Darwin.Xpc.Uuid.TYPE);
			reader.end_member ();

			string tunnel_ip_address = reader
				.read_member ("info")
				.read_member ("tunnelIPAddress")
				.get_string_value ();
			tunnel_device_address = new InetAddress.from_string (tunnel_ip_address);

			_discovery = yield locate_discovery_service (tunnel_device_address, cancellable);
		}

		public async IOStream open_tcp_connection (uint16 port, Cancellable? cancellable) throws Error, IOError {
			SocketConnection connection;
			try {
				var service_address = new InetSocketAddress (tunnel_device_address, port);

				var client = new SocketClient ();
				connection = yield client.connect_async (service_address, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CONNECTION_REFUSED)
					throw new Error.SERVER_NOT_RUNNING ("%s", e.message);
				throw new Error.TRANSPORT ("%s", e.message);
			}

			Tcp.enable_nodelay (connection.socket);

			return connection;
		}

		private static async DiscoveryService locate_discovery_service (InetAddress tunnel_device_address, Cancellable? cancellable)
				throws Error, IOError {
			var main_context = MainContext.get_thread_default ();

			var path_buf = new char[4096];
			unowned string path = (string) path_buf;

			uint delays[] = { 0, 50, 250 };
			for (uint attempts = 0; attempts != delays.length; attempts++) {
				uint delay = delays[attempts];
				if (delay != 0) {
					var timeout_source = new TimeoutSource (delay);
					timeout_source.set_callback (locate_discovery_service.callback);
					timeout_source.attach (main_context);

					var cancel_source = new CancellableSource (cancellable);
					cancel_source.set_callback (locate_discovery_service.callback);
					cancel_source.attach (main_context);

					yield;

					cancel_source.destroy ();
					timeout_source.destroy ();

					if (cancellable.is_cancelled ())
						break;
				}

				foreach (var item in XNU.query_active_tcp_connections ()) {
					if (item.family != IPV6)
						continue;
					if (!item.foreign_address.equal (tunnel_device_address))
						continue;
					if (Darwin.XNU.proc_pidpath (item.effective_pid, path_buf) <= 0)
						continue;
					if (path != "/usr/libexec/remoted")
						continue;

					try {
						var connectable = new InetSocketAddress (tunnel_device_address, item.foreign_port);

						var sc = new SocketClient ();
						SocketConnection connection = yield sc.connect_async (connectable, cancellable);
						Tcp.enable_nodelay (connection.socket);

						return yield DiscoveryService.open (connection, cancellable);
					} catch (GLib.Error e) {
					}
				}
			}

			throw new Error.NOT_SUPPORTED ("Unable to detect RSD port");
		}
	}

	private sealed class PairingdRequest {
		public Darwin.Xpc.Dictionary message = new Darwin.Xpc.Dictionary ();
		public Darwin.Xpc.Dictionary body = new Darwin.Xpc.Dictionary ();

		public PairingdRequest (string name) {
			message.set_string ("mangledTypeName", name);
			message.set_value ("value", body);
		}
	}

	namespace XNU {
		public PcbList query_active_tcp_connections () {
			size_t size = 0;
			Darwin.XNU.sysctlbyname ("net.inet.tcp.pcblist_n", null, &size);

			var pcbs = new uint8[size];
			Darwin.XNU.sysctlbyname ("net.inet.tcp.pcblist_n", pcbs, &size);

			return new PcbList (pcbs);
		}

		public sealed class PcbList {
			private uint8[] pcbs;

			internal PcbList (owned uint8[] pcbs) {
				this.pcbs = (owned) pcbs;
			}

			public Iterator iterator () {
				return new Iterator (this);
			}

			public sealed class Iterator {
				private PcbList list;
				private InetItem * cursor;

				internal Iterator (PcbList list) {
					this.list = list;

					var gen = (Darwin.XNU.InetPcbGeneration *) list.pcbs;
					cursor = (InetItem *) ((uint8 *) list.pcbs + gen->length);
				}

				public Item? next_value () {
					InetPcb * pcb = null;
					while (true) {
						if (cursor->length == 24)
							return null;

						switch (cursor->kind) {
							case InetItemKind.PCB:
								pcb = (InetPcb *) cursor;
								break;
							case InetItemKind.SOCKET:
								var item = new Item (*pcb, *((InetSocket *) cursor));
								advance ();
								return item;
						}

						advance ();
					}
				}

				private void advance () {
					uint32 l = cursor->length;
					if (l % 8 != 0)
						l += 8 - (l % 8);
					cursor = (InetItem *) ((uint8 *) cursor + l);
				}
			}

			public sealed class Item {
				public SocketFamily family {
					get {
						return ((pcb.version_flag & Darwin.XNU.InetVersionFlags.IPV6) != 0)
							? SocketFamily.IPV6
							: SocketFamily.IPV4;
					}
				}

				public InetAddress local_address {
					get {
						if (cached_local_address == null)
							cached_local_address = parse_address (pcb.local_address);
						return cached_local_address;
					}
				}

				public uint16 local_port {
					get {
						return uint16.from_big_endian (pcb.local_port);
					}
				}

				public InetAddress foreign_address {
					get {
						if (cached_foreign_address == null)
							cached_foreign_address = parse_address (pcb.foreign_address);
						return cached_foreign_address;
					}
				}

				public uint16 foreign_port {
					get {
						return uint16.from_big_endian (pcb.foreign_port);
					}
				}

				public int32 effective_pid {
					get {
						return sock.effective_pid;
					}
				}

				private InetPcb pcb;
				private InetSocket sock;
				private InetAddress? cached_local_address;
				private InetAddress? cached_foreign_address;

				public Item (InetPcb pcb, InetSocket sock) {
					this.pcb = pcb;
					this.sock = sock;
				}

				private InetAddress parse_address (uint8[] bytes) {
					if ((pcb.version_flag & Darwin.XNU.InetVersionFlags.IPV6) != 0)
						return new InetAddress.from_bytes (bytes, IPV6);
					var addr = (Darwin.XNU.InetAddr4in6 *) bytes;
					return new InetAddress.from_bytes ((uint8[]) &addr->addr4.s_addr, IPV4);
				}
			}
		}

		[SimpleType]
		public struct InetItem {
			public uint32 length;
			public uint32 kind;
		}

		public enum InetItemKind {
			SOCKET	= 0x001,
			PCB	= 0x010,
		}

		[SimpleType]
		public struct InetPcb {
			public uint32 length;
			public uint32 kind;

			public uint64 inpp;
			public uint16 foreign_port;
			public uint16 local_port;
			public uint32 per_protocol_pcb_low;
			public uint32 per_protocol_pcb_high;
			public uint32 generation_count_low;
			public uint32 generation_count_high;
			public int flags;
			public uint32 flow;
			public uint8 version_flag;
			public uint8 ip_ttl;
			public uint8 ip_protocol;
			public uint8 padding;
			public uint8 foreign_address[16];
			public uint8 local_address[16];
			public InetDepend4 depend4;
			public InetDepend6 depend6;
			public uint32 flowhash;
			public uint32 flags2;
		}

		[SimpleType]
		public struct InetSocket {
			public uint32 length;
			public uint32 kind;

			public uint64 so;
			public int16 type;
			public uint16 options_low;
			public uint16 options_high;
			public int16 linger;
			public int16 state;
			public uint16 pcb[4];
			public uint16 protocol_low;
			public uint16 protocol_high;
			public uint16 family_low;
			public uint16 family_high;
			public int16 qlen;
			public int16 incqlen;
			public int16 qlimit;
			public int16 timeo;
			public uint16 error;
			public int32 pgid;
			public uint32 oobmark;
			public uint32 uid;
			public int32 last_pid;
			public int32 effective_pid;
			public uint64 gencnt;
			public uint32 flags;
			public uint32 flags1;
			public int32 usecount;
			public int32 retaincnt;
			public uint32 filter_flags;
		}

		[SimpleType]
		public struct InetDepend4 {
			public uint8 ip_tos;
		}

		[SimpleType]
		public struct InetDepend6 {
			public uint8 hlim;
			public int checksum;
			public uint16 interface_index;
			public int16 hops;
		}
	}
}
