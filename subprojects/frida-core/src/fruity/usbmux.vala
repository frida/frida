[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class UsbmuxClient : Object, AsyncInitable {
		public signal void device_attached (UsbmuxDevice device);
		public signal void device_detached (UsbmuxDevice device);

		public SocketConnection? connection {
			get;
			private set;
		}

		private Gee.Map<uint, UsbmuxDevice> devices = new Gee.HashMap<uint, UsbmuxDevice> ();

		private InputStream? input;
		private OutputStream? output;
		private Cancellable io_cancellable = new Cancellable ();

		private bool is_processing_messages;
		private uint last_tag = 1;
		private uint mode_switch_tag;
		private Gee.ArrayList<PendingResponse> pending_responses = new Gee.ArrayList<PendingResponse> ();

		private enum QueryType {
			REGULAR,
			MODE_SWITCH
		}

		private const uint16 USBMUXD_DEFAULT_SERVER_PORT = 27015;
		private const uint USBMUX_PROTOCOL_VERSION = 1;
		private const uint32 MAX_MESSAGE_SIZE = 128 * 1024;

		public static async UsbmuxClient open (Cancellable? cancellable = null) throws UsbmuxError, IOError {
			var client = new UsbmuxClient ();

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_local_error (e);
			}

			return client;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws UsbmuxError, IOError {
			assert (!is_processing_messages);

			SocketConnectable? connectable = null;
			string? env_socket_address = Environment.get_variable ("USBMUXD_SOCKET_ADDRESS");
			if (env_socket_address != null) {
				if (env_socket_address.has_prefix ("UNIX:")) {
#if !WINDOWS
					connectable = new UnixSocketAddress (env_socket_address[5:]);
#endif
				} else {
					try {
						connectable = NetworkAddress.parse (env_socket_address, USBMUXD_DEFAULT_SERVER_PORT);
					} catch (GLib.Error e) {
					}
				}
			}

			if (connectable == null) {
#if WINDOWS
				connectable = new InetSocketAddress (new InetAddress.loopback (SocketFamily.IPV4),
					USBMUXD_DEFAULT_SERVER_PORT);
#else
				connectable = new UnixSocketAddress ("/var/run/usbmuxd");
#endif
			}

			try {
				var client = new SocketClient ();
				connection = yield client.connect_async (connectable, cancellable);

				var socket = connection.socket;
				if (socket.get_family () != UNIX)
					Tcp.enable_nodelay (socket);

				input = connection.get_input_stream ();
				output = connection.get_output_stream ();

				is_processing_messages = true;

				process_incoming_messages.begin ();
			} catch (GLib.Error e) {
				throw new UsbmuxError.DAEMON_NOT_RUNNING ("%s", e.message);
			}

			return true;
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			if (is_processing_messages) {
				is_processing_messages = false;

				io_cancellable.cancel ();

				var source = new IdleSource ();
				source.set_priority (Priority.LOW);
				source.set_callback (close.callback);
				source.attach (MainContext.get_thread_default ());
				yield;
			}

			try {
				var conn = this.connection;
				if (conn != null)
					yield conn.close_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
			}
			connection = null;
			input = null;
			output = null;
		}

		public async void enable_listen_mode (Cancellable? cancellable = null) throws UsbmuxError, IOError {
			assert (is_processing_messages);

			var request = create_request ("Listen");
			request.set_integer ("kLibUSBMuxVersion", 3);
			request.set_integer ("ConnType", 0);

			var response = yield query (request, REGULAR, cancellable);
			try {
				if (response.get_string ("MessageType") != "Result")
					throw new UsbmuxError.PROTOCOL ("Unexpected response message type");

				var result = (int) response.get_integer ("Number");
				if (result != ResultCode.SUCCESS)
					throw new UsbmuxError.PROTOCOL ("Unexpected result while trying to enable listen mode: %d", result);
			} catch (PlistError e) {
				throw new UsbmuxError.PROTOCOL ("Unexpected response: %s", e.message);
			}
		}

		public async void connect_to_port (uint device_id, uint16 port, Cancellable? cancellable = null)
				throws UsbmuxError, IOError {
			assert (is_processing_messages);

			var request = create_request ("Connect");
			request.set_integer ("DeviceID", device_id);
			request.set_integer ("PortNumber", port.to_big_endian ());

			var response = yield query (request, MODE_SWITCH, cancellable);
			try {
				if (response.get_string ("MessageType") != "Result")
					throw new UsbmuxError.PROTOCOL ("Unexpected response message type");

				var result = (int) response.get_integer ("Number");
				switch (result) {
					case ResultCode.SUCCESS:
						break;
					case ResultCode.CONNECTION_REFUSED:
						throw new UsbmuxError.CONNECTION_REFUSED ("Unable to connect (connection refused)");
					case ResultCode.INVALID_REQUEST:
						throw new UsbmuxError.INVALID_ARGUMENT ("Unable to connect (invalid argument)");
					default:
						throw new UsbmuxError.PROTOCOL ("Unable to connect (error code: %d)", result);
				}
			} catch (PlistError e) {
				throw new UsbmuxError.PROTOCOL ("Unexpected response: %s", e.message);
			}
		}

		public async Plist read_pair_record (string udid, Cancellable? cancellable = null) throws UsbmuxError, IOError {
			var request = create_request ("ReadPairRecord");
			request.set_string ("PairRecordID", udid);

			var response = yield query (request, REGULAR, cancellable);
			try {
				check_pair_response (response, "ReadPairRecord");

				var raw_record = response.get_bytes ("PairRecordData");
				return new Plist.from_data (raw_record.get_data ());
			} catch (PlistError e) {
				throw new UsbmuxError.PROTOCOL ("Unexpected ReadPairRecord response: %s", e.message);
			}
		}

		public async void delete_pair_record (string udid, Cancellable? cancellable = null) throws UsbmuxError, IOError {
			var request = create_request ("DeletePairRecord");
			request.set_string ("PairRecordID", udid);

			var response = yield query (request, REGULAR, cancellable);
			check_pair_response (response, "DeletePairRecord");
		}

		private static void check_pair_response (Plist response, string operation) throws UsbmuxError {
			if (!response.has ("MessageType"))
				return;

			try {
				if (response.get_string ("MessageType") != "Result")
					throw new UsbmuxError.PROTOCOL ("Unexpected %s response", operation);
				var result = (int) response.get_integer ("Number");
				if (result == ResultCode.NOT_FOUND)
					throw new UsbmuxError.INVALID_ARGUMENT ("Pair record not found");
				if (result != 0)
					throw new UsbmuxError.PROTOCOL ("Unexpected %s result: %d", operation, result);
			} catch (PlistError e) {
				throw new UsbmuxError.PROTOCOL ("Unexpected response: %s", e.message);
			}
		}

		private async Plist query (Plist request, QueryType query_type, Cancellable? cancellable) throws UsbmuxError, IOError {
			uint32 tag = last_tag++;

			if (query_type == MODE_SWITCH)
				mode_switch_tag = tag;

			var body_xml = request.to_xml ();
			unowned uint8[] body = ((uint8[]) body_xml)[0:body_xml.length];

			var msg = create_message (MessageType.PROPERTY_LIST, tag, body);
			var pending = new PendingResponse (tag, query.callback);
			pending_responses.add (pending);
			write_message.begin (msg);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				if (pending_responses.remove (pending))
					query.callback ();
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			yield;

			cancel_source.destroy ();

			cancellable.set_error_if_cancelled ();

			return pending.get_response ();
		}

		private Plist create_request (string message_type) {
			var request = new Plist ();
			request.set_string ("ClientVersionString", "usbmuxd-509");
			request.set_string ("ProgName", "Xcode");
			request.set_string ("BundleID", "com.apple.dt.Xcode");
#if !WINDOWS
			request.set_integer ("ProcessID", Posix.getpid ());
#endif
			request.set_string ("MessageType", message_type);
			return request;
		}

		private async void process_incoming_messages () {
			while (is_processing_messages) {
				try {
					var msg = yield read_message ();
					dispatch_message (msg);
				} catch (GLib.Error error) {
					foreach (var pending_response in pending_responses)
						pending_response.complete_with_error (error);
					is_processing_messages = false;
				}
			}
		}

		private void dispatch_message (UsbmuxClient.Message msg) throws UsbmuxError {
			if (msg.type != MessageType.PROPERTY_LIST)
				throw new UsbmuxError.PROTOCOL ("Unexpected message type %u, was expecting a property list", (uint) msg.type);
			else if (msg.body_size == 0)
				throw new UsbmuxError.PROTOCOL ("Unexpected message with empty body");

			unowned string body_xml = (string) msg.body;
			try {
				var body = new Plist.from_xml (body_xml);
				if (msg.tag != 0) {
					handle_response_message (msg.tag, body);
				} else {
					var message_type = body.get_string ("MessageType");
					if (message_type == "Attached") {
						var props = body.get_dict ("Properties");

						ConnectionType connection_type = USB;
						if (props.has ("ConnectionType") && props.get_string ("ConnectionType") == "Network")
							connection_type = NETWORK;

						var device_id = (uint) body.get_integer ("DeviceID");

						int product_id = (connection_type == USB) ? (int) props.get_integer ("ProductID") : -1;

						string udid = props.get_string ("SerialNumber");
						if (udid.length == 24)
							udid = udid[:8] + "-" + udid[8:];

						InetSocketAddress? network_address = null;
						if (connection_type == NETWORK)
							network_address = parse_network_address (props.get_bytes ("NetworkAddress"));

						var device =
							new UsbmuxDevice (connection_type, device_id, product_id, udid, network_address);
						devices[device_id] = device;
						device_attached (device);
					} else if (message_type == "Detached") {
						UsbmuxDevice? device;
						if (devices.unset ((uint) body.get_integer ("DeviceID"), out device))
							device_detached (device);
					} else {
						throw new UsbmuxError.PROTOCOL ("Unexpected message type: %s", message_type);
					}
				}
			} catch (PlistError e) {
				throw new UsbmuxError.PROTOCOL ("Malformed usbmux message body: %s", e.message);
			}
		}

		private void handle_response_message (uint32 tag, Plist response) throws UsbmuxError {
			PendingResponse match = null;
			foreach (var pending in pending_responses) {
				if (pending.tag == tag) {
					match = pending;
					break;
				}
			}
			bool query_was_cancelled = match == null;
			if (query_was_cancelled)
				return;

			pending_responses.remove (match);
			match.complete_with_response (response);

			if (tag == mode_switch_tag) {
				int64 result;
				try {
					result = response.get_integer ("Number");
				} catch (PlistError e) {
					throw new UsbmuxError.PROTOCOL ("Malformed response: %s", e.message);
				}
				if (result == ResultCode.SUCCESS)
					is_processing_messages = false;
				else
					mode_switch_tag = 0;
			}
		}

		private uint8[] create_message (MessageType type, uint32 tag, uint8[]? body = null) {
			uint body_size = 0;
			if (body != null)
				body_size = body.length;

			uint8[] blob = new uint8[16 + body_size];

			uint32 * p = (void *) blob;
			p[0] = blob.length.to_little_endian ();
			p[1] = USBMUX_PROTOCOL_VERSION.to_little_endian ();
			p[2] = ((uint) type).to_little_endian ();
			p[3] = tag.to_little_endian ();

			if (body_size != 0) {
				uint8 * blob_start = (void *) blob;
				Memory.copy (blob_start + 16, body, body_size);
			}

			return blob;
		}

		private async Message read_message () throws GLib.Error {
			var header_buf = new uint8[16];
			yield read (header_buf);
			var header = (uint32 *) header_buf;

			uint32 size = uint32.from_little_endian (header[0]);
			MessageType type = (MessageType) uint32.from_little_endian (header[2]);
			uint32 tag = uint32.from_little_endian (header[3]);

			if (size < 16 || size > MAX_MESSAGE_SIZE)
				throw new UsbmuxError.PROTOCOL ("Invalid message size");

			var body_size = size - 16;
			var msg = new Message (type, tag, body_size);

			if (body_size > 0) {
				unowned uint8[] body_buf = ((uint8 []) msg.body)[0:body_size];
				yield read (body_buf);
			}

			return msg;
		}

		private async void read (uint8[] buffer) throws GLib.Error {
			size_t bytes_read;
			yield input.read_all_async (buffer, Priority.DEFAULT, io_cancellable, out bytes_read);
			if (bytes_read == 0)
				throw new IOError.CONNECTION_CLOSED ("Connection closed");
		}

		private async void write_message (uint8[] blob) throws GLib.Error {
			size_t bytes_written;
			yield output.write_all_async (blob, Priority.DEFAULT, io_cancellable, out bytes_written);
		}

		private static InetSocketAddress parse_network_address (Bytes bytes) throws UsbmuxError {
			uint8[] data = bytes.get_data ();
			if (data.length < 8)
				throw new UsbmuxError.PROTOCOL ("Invalid network address");

			uint8 raw_family = data[1];
			switch (raw_family) {
				case 0x02: {
					/*
					 *    \
					 *    | struct sockaddr_in {
					 *  0 |   __uint8_t       sin_len;
					 *  1 |   sa_family_t     sin_family;
					 *  2 |   in_port_t       sin_port;
					 *  4 |   struct  in_addr sin_addr;
					 *  8 |   char            sin_zero[8];
					 * 16 | };
					 *    /
					 */

					uint16 port = uint16.from_big_endian (*((uint16 *) ((uint8 *) data + 2)));
					var address = new InetAddress.from_bytes (data[4:8], IPV4);

					return new InetSocketAddress (address, port);
				}
				case 0x1e: {
					/*
					 *     \ struct sockaddr_in6 {
					 *  0  |   __uint8_t       sin6_len;
					 *  1  |   sa_family_t     sin6_family;
					 *  2  |   in_port_t       sin6_port;
					 *  4  |   __uint32_t      sin6_flowinfo;
					 *  8  |   struct in6_addr sin6_addr;
					 * 24  |   __uint32_t      sin6_scope_id;
					 * 28  | };
					 *     /
					 */

					if (data.length < 28)
						throw new UsbmuxError.PROTOCOL ("Invalid network address");

					uint16 port = uint16.from_big_endian (*((uint16 *) ((uint8 *) data + 2)));
					uint32 flowinfo = uint32.from_big_endian (*((uint32 *) ((uint8 *) data + 4)));
					var address = new InetAddress.from_bytes (data[8:24], IPV6);
					uint32 scope_id = uint32.from_little_endian (*((uint32 *) ((uint8 *) data + 24)));

					return (InetSocketAddress) Object.new (typeof (InetSocketAddress),
						address: address,
						port: port,
						flowinfo: flowinfo,
						scope_id: scope_id
					);
				}
				default:
					throw new UsbmuxError.PROTOCOL ("Unsupported address family: 0x%02x", raw_family);
			}
		}

		private static void throw_local_error (GLib.Error e) throws UsbmuxError, IOError {
			if (e is UsbmuxError)
				throw (UsbmuxError) e;

			if (e is IOError)
				throw (IOError) e;

			assert_not_reached ();
		}

		private enum MessageType {
			RESULT		= 1,
			CONNECT		= 2,
			LISTEN		= 3,
			DEVICE_ATTACHED	= 4,
			DEVICE_DETACHED	= 5,
			PROPERTY_LIST	= 8
		}

		private enum ResultCode {
			PROTOCOL_ERROR      = -1,
			SUCCESS		    = 0,
			NOT_FOUND           = 2,
			CONNECTION_REFUSED  = 3,
			INVALID_REQUEST	    = 5
		}

		private class Message {
			public MessageType type;
			public uint32 tag;
			public uint8 * body;
			public uint32 body_size;

			public Message (MessageType type, uint32 tag, uint32 body_size) {
				this.type = type;
				this.tag = tag;
				this.body = malloc (body_size + 1);
				this.body[body_size] = 0;
				this.body_size = body_size;
			}

			~Message () {
				free (body);
			}
		}

		private class PendingResponse {
			public uint32 tag {
				get;
				private set;
			}

			private SourceFunc? handler;

			private Plist? response;
			private GLib.Error? error;

			public PendingResponse (uint32 tag, owned SourceFunc handler) {
				this.tag = tag;
				this.handler = (owned) handler;
			}

			public void complete_with_response (Plist? response) {
				this.response = response;
				handler ();
				handler = null;
			}

			public void complete_with_error (GLib.Error error) {
				this.error = error;
				handler ();
				handler = null;
			}

			public Plist get_response () throws UsbmuxError, IOError {
				if (response == null) {
					if (error is UsbmuxError)
						throw (UsbmuxError) error;
					else if (error is IOError)
						throw (IOError) error;
					else
						assert_not_reached ();
				}

				return response;
			}
		}
	}

	public errordomain UsbmuxError {
		DAEMON_NOT_RUNNING,
		CONNECTION_REFUSED,
		INVALID_ARGUMENT,
		PROTOCOL
	}

	public sealed class UsbmuxDevice : Object {
		public ConnectionType connection_type {
			get;
			construct;
		}

		public uint id {
			get;
			construct;
		}

		public int product_id {
			get;
			construct;
		}

		public string udid {
			get;
			construct;
		}

		public InetSocketAddress? network_address {
			get;
			construct;
		}

		public UsbmuxDevice (ConnectionType connection_type, uint id, int product_id, string udid,
				InetSocketAddress? network_address) {
			Object (
				connection_type: connection_type,
				id: id,
				product_id: product_id,
				udid: udid,
				network_address: network_address
			);
		}
	}
}
