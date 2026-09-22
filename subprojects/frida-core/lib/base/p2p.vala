#if HAVE_NICE
namespace Frida {
	namespace PeerConnection {
		public async void configure_agent (Nice.Agent agent, uint stream_id, uint component_id, PeerOptions? options,
				Cancellable? cancellable) throws Error, IOError {
			if (options == null)
				return;

			string? stun_server = options.stun_server;
			if (stun_server != null) {
				InetSocketAddress? addr;
				try {
					var enumerator = NetworkAddress.parse (stun_server, 3478).enumerate ();
					addr = (InetSocketAddress) yield enumerator.next_async (cancellable);
				} catch (GLib.Error e) {
					throw new Error.INVALID_ARGUMENT ("Invalid STUN server address: %s", e.message);
				}
				if (addr == null)
					throw new Error.INVALID_ARGUMENT ("Invalid STUN server address");
				agent.stun_server = addr.get_address ().to_string ();
				agent.stun_server_port = addr.get_port ();
			}

			var relays = new Gee.ArrayList<Relay> ();
			options.enumerate_relays (relay => {
				relays.add (relay);
			});
			foreach (var relay in relays) {
				InetSocketAddress? addr;
				try {
					var enumerator = NetworkAddress.parse (relay.address, 3478).enumerate ();
					addr = (InetSocketAddress) yield enumerator.next_async (cancellable);
				} catch (GLib.Error e) {
					throw new Error.INVALID_ARGUMENT ("Invalid relay server address: %s", e.message);
				}
				if (addr == null)
					throw new Error.INVALID_ARGUMENT ("Invalid relay server address");
				agent.set_relay_info (stream_id, component_id, addr.get_address ().to_string (),
					addr.get_port (), relay.username, relay.password, relay_kind_to_libnice (relay.kind));
			}
		}

		public string compute_certificate_fingerprint (uint8[] cert_der) {
			var fingerprint = new StringBuilder.sized (128);

			fingerprint.append ("sha-256 ");

			string raw_fingerprint = Checksum.compute_for_data (SHA256, cert_der);
			for (int offset = 0; offset != raw_fingerprint.length; offset += 2) {
				if (offset != 0)
					fingerprint.append_c (':');
				fingerprint.append_c (raw_fingerprint[offset + 0].toupper ());
				fingerprint.append_c (raw_fingerprint[offset + 1].toupper ());
			}

			return fingerprint.str;
		}

		private Nice.RelayType relay_kind_to_libnice (RelayKind kind) {
			switch (kind) {
				case TURN_UDP: return Nice.RelayType.TURN_UDP;
				case TURN_TCP: return Nice.RelayType.TURN_TCP;
				case TURN_TLS: return Nice.RelayType.TURN_TLS;
			}
			assert_not_reached ();
		}
	}

	public class PeerSessionDescription {
		public uint64 session_id = 0;
		public string? ice_ufrag;
		public string? ice_pwd;
		public bool ice_trickle = false;
		public string? fingerprint;
		public PeerSetup setup = HOLDCONN;
		public uint16 sctp_port = 5000;
		public size_t max_message_size = 256 * 1024;

		public static PeerSessionDescription parse (string sdp) throws Error {
			var description = new PeerSessionDescription ();

			foreach (unowned string raw_line in sdp.split ("\n")) {
				string line = raw_line.chomp ();
				if (line.has_prefix ("o=")) {
					string[] tokens = line[2:].split (" ", 6);
					if (tokens.length >= 2)
						description.session_id = uint64.parse (tokens[1]);
				} else if (line.has_prefix ("a=")) {
					string[] tokens = line[2:].split (":", 2);
					if (tokens.length == 2) {
						unowned string attribute = tokens[0];
						unowned string val = tokens[1];
						if (attribute == "ice-ufrag") {
							description.ice_ufrag = val;
						} else if (attribute == "ice-pwd") {
							description.ice_pwd = val;
						} else if (attribute == "ice-options") {
							string[] options = val.split (" ");
							foreach (unowned string option in options) {
								if (option == "trickle")
									description.ice_trickle = true;
							}
						} else if (attribute == "fingerprint") {
							description.fingerprint = val;
						} else if (attribute == "setup") {
							description.setup = PeerSetup.from_nick (val);
						} else if (attribute == "sctp-port") {
							description.sctp_port = (uint16) uint.parse (val);
						} else if (attribute == "max-message-size") {
							description.max_message_size = uint.parse (val);
						}
					}
				}
			}

			description.check ();

			return description;
		}

		public string to_sdp () {
			return string.join ("\r\n",
				"v=0",
				("o=- %" + uint64.FORMAT_MODIFIER + "u 2 IN IP4 127.0.0.1").printf (session_id),
				"s=-",
				"t=0 0",
				"a=group:BUNDLE 0",
				"a=extmap-allow-mixed",
				"a=msid-semantic: WMS",
				"m=application 9 UDP/DTLS/SCTP webrtc-datachannel",
				"c=IN IP4 0.0.0.0",
				"a=ice-ufrag:" + ice_ufrag,
				"a=ice-pwd:" + ice_pwd,
				"a=ice-options:trickle",
				"a=fingerprint:" + fingerprint,
				"a=setup:" + setup.to_nick (),
				"a=mid:0",
				("a=sctp-port:%" + uint16.FORMAT_MODIFIER + "u").printf (sctp_port),
				("a=max-message-size:%" + size_t.FORMAT_MODIFIER + "u").printf (max_message_size)
			) + "\r\n";
		}

		private void check () throws Error {
			if (session_id == 0 || ice_ufrag == null || ice_pwd == null || !ice_trickle || fingerprint == null ||
					setup == HOLDCONN) {
				throw new Error.NOT_SUPPORTED ("Unsupported session configuration");
			}
		}
	}

	namespace PeerSessionId {
		public uint64 generate () {
			return ((uint64) Random.next_int ()) << 32 | (uint64) Random.next_int ();
		}
	}

	public enum PeerSetup {
		ACTIVE,
		PASSIVE,
		ACTPASS,
		HOLDCONN;

		public static PeerSetup from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<PeerSetup> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<PeerSetup> (this);
		}
	}

	public sealed class PeerSocket : Object, DatagramBased {
		public Nice.Agent agent {
			get;
			construct;
		}

		public uint stream_id {
			get;
			construct;
		}

		public uint component_id {
			get;
			construct;
		}

		public MainContext? main_context {
			get;
			construct;
		}

		public IOCondition pending_io {
			get {
				mutex.lock ();
				IOCondition result = _pending_io;
				mutex.unlock ();
				return result;
			}
		}

		private Nice.ComponentState component_state;
		private RecvState recv_state = NOT_RECEIVING;
		private Gee.Queue<Bytes> recv_queue = new Gee.ArrayQueue<Bytes> ();
		private IOCondition _pending_io = 0;
		private Mutex mutex = Mutex ();
		private Cond cond = Cond ();

		private Gee.Map<unowned Source, IOCondition> sources = new Gee.HashMap<unowned Source, IOCondition> ();

		private enum RecvState {
			NOT_RECEIVING,
			RECEIVING
		}

		public PeerSocket (Nice.Agent agent, uint stream_id, uint component_id) {
			Object (
				agent: agent,
				stream_id: stream_id,
				component_id: component_id,
				main_context: MainContext.get_thread_default ()
			);
		}

		construct {
			component_state = agent.get_component_state (stream_id, component_id);
			agent.component_state_changed.connect (on_component_state_changed);
		}

		public virtual int datagram_receive_messages (InputMessage[] messages, int flags, int64 timeout,
				Cancellable? cancellable) throws GLib.Error {
			if (flags != 0)
				throw new IOError.NOT_SUPPORTED ("Flags not supported");

			int64 deadline;
			prepare_for_io (timeout, cancellable, out deadline);

			int received = 0;
			GLib.Error? io_error = null;
			ulong cancellation_handler = 0;

			while (received != messages.length && io_error == null) {
				mutex.lock ();
				Bytes? bytes = recv_queue.poll ();
				update_pending_io ();
				mutex.unlock ();

				if (bytes != null) {
					messages[received].bytes_received = 0;
					messages[received].flags = 0;

					uint8 * data = bytes.get_data ();
					size_t remaining = bytes.get_size ();
					foreach (unowned InputVector vector in messages[received].vectors) {
						size_t n = size_t.min (remaining, vector.size);
						if (n == 0)
							break;
						Memory.copy (vector.buffer, data, n);
						data += n;
						remaining -= n;
						messages[received].bytes_received += n;
					}

					received++;
				} else {
					if (received > 0)
						break;

					if (deadline == 0) {
						io_error = new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
					} else if (deadline != -1 && get_monotonic_time () >= deadline) {
						io_error = new IOError.TIMED_OUT ("Timed out");
					} else {
						if (cancellable != null && cancellation_handler == 0) {
							cancellation_handler = cancellable.connect (() => {
								mutex.lock ();
								cond.broadcast ();
								mutex.unlock ();
							});
						}

						mutex.lock ();
						while (recv_queue.is_empty && !cancellable.is_cancelled ()) {
							if (deadline != -1) {
								if (!cond.wait_until (mutex, deadline)) {
									io_error = new IOError.TIMED_OUT ("Timed out");
									break;
								}
							} else {
								cond.wait (mutex);
							}
						}
						mutex.unlock ();
					}
				}
			}

			if (cancellation_handler != 0)
				cancellable.disconnect (cancellation_handler);

			if (received == 0 && io_error != null)
				throw io_error;

			return received;
		}

		public virtual int datagram_send_messages (OutputMessage[] messages, int flags, int64 timeout,
				Cancellable? cancellable) throws GLib.Error {
			if (flags != 0)
				throw new IOError.NOT_SUPPORTED ("Flags not supported");

			int64 deadline;
			prepare_for_io (timeout, cancellable, out deadline);

			var nice_messages = new Nice.OutputMessage[messages.length];
			for (var i = 0; i != messages.length; i++) {
				nice_messages[i].buffers = messages[i].vectors;
				nice_messages[i].n_buffers = (int) messages[i].num_vectors;
			}

			int sent = 0;
			GLib.Error? io_error = null;
			ulong cancellation_handler = 0;

			while (sent != nice_messages.length && io_error == null) {
				try {
					int n = agent.send_messages_nonblocking (stream_id, component_id, nice_messages[sent:],
						cancellable);
					sent += n;
				} catch (GLib.Error e) {
					if (sent > 0)
						break;

					if (e is IOError.WOULD_BLOCK && deadline != 0) {
						if (deadline != -1 && get_monotonic_time () >= deadline) {
							io_error = new IOError.TIMED_OUT ("Timed out");
							break;
						}

						if (cancellable != null && cancellation_handler == 0) {
							cancellation_handler = cancellable.connect (() => {
								mutex.lock ();
								cond.broadcast ();
								mutex.unlock ();
							});
						}

						int64 ten_msec_from_now = get_monotonic_time () + 10000;
						mutex.lock ();
						while (!cancellable.is_cancelled ()) {
							if (!cond.wait_until (mutex, (deadline != -1)
									? int64.min (ten_msec_from_now, deadline)
									: ten_msec_from_now)) {
								break;
							}
						}
						mutex.unlock ();
					} else {
						io_error = e;
					}
				}
			}

			if (cancellation_handler != 0)
				cancellable.disconnect (cancellation_handler);

			if (sent == 0 && io_error != null)
				throw io_error;

			foreach (var message in messages)
				message.bytes_sent = 0;
			for (int i = 0; i < sent; i++) {
				foreach (var vector in messages[i].vectors)
					messages[i].bytes_sent += (uint) vector.size;
			}

			return sent;
		}

		public virtual DatagramBasedSource datagram_create_source (IOCondition condition, Cancellable? cancellable) {
			return new PeerSocketSource (this, condition, cancellable);
		}

		public virtual IOCondition datagram_condition_check (IOCondition condition) {
			assert_not_reached ();
		}

		public virtual bool datagram_condition_wait (IOCondition condition, int64 timeout,
				Cancellable? cancellable) throws GLib.Error {
			assert_not_reached ();
		}

		public void register_source (Source source, IOCondition condition) {
			mutex.lock ();
			sources[source] = condition | IOCondition.ERR | IOCondition.HUP;
			mutex.unlock ();
		}

		public void unregister_source (Source source) {
			mutex.lock ();
			sources.unset (source);
			mutex.unlock ();
		}

		private void on_component_state_changed (uint stream_id, uint component_id, Nice.ComponentState state) {
			if (stream_id != this.stream_id || component_id != this.component_id)
				return;

			mutex.lock ();
			component_state = state;
			update_pending_io ();
			cond.broadcast ();
			mutex.unlock ();
		}

		private void on_recv (Nice.Agent agent, uint stream_id, uint component_id, uint8[] data) {
			var packet = new Bytes (data);

			mutex.lock ();
			recv_queue.offer (packet);
			update_pending_io ();
			cond.broadcast ();
			mutex.unlock ();
		}

		private void update_pending_io () {
			IOCondition condition = 0;

			if (!recv_queue.is_empty)
				condition |= IOCondition.IN;

			switch (component_state) {
				case CONNECTED:
				case READY:
					condition |= IOCondition.OUT;
					break;
				case FAILED:
					condition |= IOCondition.ERR;
					break;
				default:
					break;
			}

			if (condition == _pending_io)
				return;

			_pending_io = condition;

			foreach (var entry in sources.entries) {
				unowned Source source = entry.key;
				IOCondition c = entry.value;
				if ((_pending_io & c) != 0)
					source.set_ready_time (0);
			}

			notify_property ("pending-io");
		}

		private void prepare_for_io (int64 timeout, Cancellable? cancellable, out int64 deadline) throws IOError {
			mutex.lock ();

			if (recv_state == NOT_RECEIVING) {
				recv_state = RECEIVING;
				mutex.unlock ();
				agent.attach_recv (stream_id, component_id, main_context, on_recv);
				mutex.lock ();
			}

			Nice.ComponentState current_state = component_state;
			bool timed_out = false;

			if (timeout != 0) {
				ulong cancellation_handler = 0;

				deadline = (timeout != -1)
					? get_monotonic_time () + timeout
					: -1;

				while (component_state != CONNECTED && component_state != READY && component_state != FAILED) {
					if (cancellable != null && cancellation_handler == 0) {
						mutex.unlock ();
						cancellation_handler = cancellable.connect (() => {
							mutex.lock ();
							cond.broadcast ();
							mutex.unlock ();
						});
						mutex.lock ();
					}

					if (cancellable.is_cancelled ())
						break;

					if (deadline != -1) {
						if (!cond.wait_until (mutex, deadline)) {
							timed_out = true;
							break;
						}
					} else {
						cond.wait (mutex);
					}
				}

				if (cancellation_handler != 0) {
					mutex.unlock ();
					cancellable.disconnect (cancellation_handler);
					mutex.lock ();
				}

				current_state = component_state;
			} else {
				deadline = 0;
			}

			mutex.unlock ();

			cancellable.set_error_if_cancelled ();

			if (current_state != CONNECTED && current_state != READY) {
				if (timed_out) {
					throw new IOError.TIMED_OUT ("Timed out");
				} else {
					if (timeout == 0 && current_state != FAILED)
						throw new IOError.WOULD_BLOCK ("Operation would block");
					else
						throw new IOError.HOST_UNREACHABLE ("Unable to send");
				}
			}
		}
	}

	private class PeerSocketSource : DatagramBasedSource {
		public PeerSocket socket;
		public IOCondition condition;
		public Cancellable? cancellable;

		public PeerSocketSource (PeerSocket socket, IOCondition condition, Cancellable? cancellable) {
			this.socket = socket;
			this.condition = condition;
			this.cancellable = cancellable;

			socket.register_source (this, condition);
		}

		~PeerSocketSource () {
			socket.unregister_source (this);
		}

		protected override bool prepare (out int timeout) {
			timeout = -1;
			return (socket.pending_io & condition) != 0;
		}

		protected override bool check () {
			return (socket.pending_io & condition) != 0;
		}

		protected override bool dispatch (SourceFunc? callback) {
			set_ready_time (-1);

			if (callback == null)
				return Source.REMOVE;

			DatagramBasedSourceFunc f = (DatagramBasedSourceFunc) callback;
			return f (socket, socket.pending_io);
		}
	}

	public sealed class SctpConnection : VirtualStream {
		public DatagramBased transport_socket {
			get;
			construct;
		}

		public PeerSetup setup {
			get;
			construct;
		}

		public uint16 port {
			get;
			construct;
		}

		public size_t max_message_size {
			get;
			construct;
		}

		private DatagramBasedSource transport_source;
		private uint8[] transport_buffer = new uint8[65536];

		private void * sctp_socket;
		private SctpTimerSource sctp_source;
		private uint16 stream_id;
		private ByteArray dcep_message = new ByteArray ();

		public SctpConnection (DatagramBased transport_socket, PeerSetup setup, uint16 port, size_t max_message_size) {
			Object (
				transport_socket: transport_socket,
				setup: setup,
				port: port,
				max_message_size: max_message_size
			);
		}

		static construct {
			_initialize_sctp_backend ();
		}

		protected extern static void _initialize_sctp_backend ();

		construct {
			sctp_socket = _create_sctp_socket ();
			_connect_sctp_socket (sctp_socket, port);

			transport_source = transport_socket.create_source (IOCondition.IN, io_cancellable);
			transport_source.set_callback (on_transport_socket_readable);
			transport_source.attach (main_context);

			sctp_source = new SctpTimerSource ();
			sctp_source.attach (main_context);
		}

		protected extern void * _create_sctp_socket ();

		protected extern void _connect_sctp_socket (void * sock, uint16 port);

		protected override VirtualStream.State query_initial_state () {
			return CREATED;
		}

		protected override IOCondition query_events () {
			return _query_sctp_socket_events (sctp_socket);
		}

		protected override void update_pending_io () {
			base.update_pending_io ();
			sctp_source.invalidate ();
		}

		protected extern static IOCondition _query_sctp_socket_events (void * sock);

		protected override void handle_close () {
			sctp_source.destroy ();
			transport_source.destroy ();

			_close (sctp_socket);
			sctp_socket = null;
		}

		public extern static void _close (void * sock);

		public override void shutdown_read () throws IOError {
			_shutdown (sctp_socket, READ);
		}

		public override void shutdown_write () throws IOError {
			_shutdown (sctp_socket, WRITE);
		}

		public extern static void _shutdown (void * sock, SctpShutdownType type) throws IOError;

		public override ssize_t read (uint8[] buffer) throws IOError {
			ssize_t n = -1;

			try {
				uint16 stream_id;
				PayloadProtocolId protocol_id;
				SctpMessageFlags msg_flags;

				n = _recv (sctp_socket, buffer, out stream_id, out protocol_id, out msg_flags);

				if (protocol_id == WEBRTC_DCEP) {
					dcep_message.append (buffer[0:n]);
					if ((msg_flags & SctpMessageFlags.END_OF_RECORD) != 0) {
						handle_dcep_message (stream_id, dcep_message.steal ());
					}
					throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
				} else if (protocol_id == NONE || state != OPEN) {
					throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
				}
			} finally {
				update_pending_io ();
			}

			return n;
		}

		public override ssize_t write (uint8[] buffer) throws IOError {
			if (state != OPEN)
				throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");

			ssize_t n = ssize_t.min (buffer.length, (ssize_t) max_message_size);

			try {
				return _send (sctp_socket, stream_id, WEBRTC_BINARY, buffer[0:n]);
			} finally {
				update_pending_io ();
			}
		}

		private bool on_transport_socket_readable (DatagramBased datagram_based, IOCondition condition) {
			var v = InputVector ();
			v.buffer = transport_buffer;
			v.size = transport_buffer.length;

			InputVector[] vectors = { v };

			var m = InputMessage ();
			m.vectors = vectors;
			m.num_vectors = vectors.length;

			InputMessage[] messages = { m };

			try {
				transport_socket.receive_messages (messages, 0, 0, io_cancellable);

				unowned uint8[] data = (uint8[]) v.buffer;
				data.length = (int) messages[0].bytes_received;

				_handle_transport_packet (data);
			} catch (GLib.Error e) {
				return Source.REMOVE;
			}

			return Source.CONTINUE;
		}

		protected extern void _handle_transport_packet (uint8[] data);

		protected int _emit_transport_packet (uint8[] data) {
			try {
				Udp.send (data, transport_socket, io_cancellable);
				return 0;
			} catch (GLib.Error e) {
				return -1;
			}
		}

		protected void _on_sctp_socket_events_changed () {
			update_pending_io ();

			if (state == CREATED && setup == ACTIVE && (pending_io & IOCondition.OUT) != 0) {
				stream_id = 1;

				uint8[] open_message = {
					DcepMessageType.DATA_CHANNEL_OPEN,
					/* Channel Type: DATA_CHANNEL_RELIABLE */ 0x00,
					/* Priority */ 0x00, 0x00,
					/* Reliability */ 0x00, 0x00, 0x00, 0x00,
					/* Label Length */ 0x00, 0x07,
					/* Protocol Length */ 0x00, 0x00,
					/* Label: "session" */ 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e
				};
				try {
					_send (sctp_socket, stream_id, WEBRTC_DCEP, open_message);
					state = OPENING;
				} catch (IOError e) {
				}
			}
		}

		protected extern static ssize_t _recv (void * sock, uint8[] buffer, out uint16 stream_id,
			out PayloadProtocolId protocol_id, out SctpMessageFlags message_flags) throws IOError;

		protected extern static ssize_t _send (void * sock, uint16 stream_id, PayloadProtocolId protocol_id,
			uint8[] data) throws IOError;

		private void handle_dcep_message (uint16 stream_id, uint8[] message) throws IOError {
			DcepMessageType type = (DcepMessageType) message[0];

			switch (type) {
				case DATA_CHANNEL_OPEN: {
					if (state != CREATED || setup == ACTIVE)
						return;

					this.stream_id = stream_id;

					uint8[] reply = { DcepMessageType.DATA_CHANNEL_ACK };
					_send (sctp_socket, stream_id, WEBRTC_DCEP, reply);

					state = OPEN;

					break;
				}
				case DATA_CHANNEL_ACK:
					if (state != OPENING)
						return;

					state = OPEN;

					break;
			}
		}
	}

	protected enum SctpShutdownType {
		READ = 1,
		WRITE,
		READ_WRITE
	}

	[Flags]
	protected enum SctpMessageFlags {
		END_OF_RECORD,
		NOTIFICATION,
	}

	protected enum PayloadProtocolId {
		NONE = 0,
		WEBRTC_DCEP = 50,
		WEBRTC_STRING,
		WEBRTC_BINARY_PARTIAL,
		WEBRTC_BINARY,
		WEBRTC_STRING_PARTIAL,
		WEBRTC_STRING_EMPTY,
		WEBRTC_BINARY_EMPTY
	}

	protected enum DcepMessageType {
		DATA_CHANNEL_OPEN = 0x03,
		DATA_CHANNEL_ACK = 0x02
	}

	private class SctpTimerSource : Source {
		private static int64 last_process_time = -1;

		public void invalidate () {
			set_ready_time (0);
		}

		protected override bool prepare (out int timeout) {
			return update_timer_status (out timeout);
		}

		protected override bool check () {
			return update_timer_status ();
		}

		private bool update_timer_status (out int timeout = null) {
			int64 now = get_monotonic_time ();

			if (last_process_time == -1)
				last_process_time = now;

			int next_timeout = _get_timeout ();
			if (next_timeout == -1) {
				last_process_time = -1;
				timeout = -1;
				return false;
			}

			int64 next_wakeup_time = last_process_time + next_timeout;

			bool ready = now >= next_wakeup_time;

			timeout = (int) int64.max (next_wakeup_time - now, 0);

			return ready;
		}

		protected override bool dispatch (SourceFunc? callback) {
			set_ready_time (-1);

			bool result = Source.CONTINUE;

			int64 now = get_monotonic_time ();
			int64 elapsed_usec = now - last_process_time;
			uint32 elapsed_msec = (uint32) (elapsed_usec / 1000);

			_process_timers (elapsed_msec);

			last_process_time = now;

			if (callback != null)
				result = callback ();

			return result;
		}

		protected extern static int _get_timeout ();
		protected extern static void _process_timers (uint32 elapsed_msec);
	}

	public async void generate_certificate (out uint8[] cert_der, out string cert_pem, out string key_pem) {
		var caller_context = MainContext.ref_thread_default ();

		Bytes? result_cert_der = null;
		string? result_cert_pem = null;
		string? result_key_pem = null;

		new Thread<bool> ("frida-generate-certificate", () => {
			uint8[] local_cert_der;
			string local_cert_pem;
			string local_key_pem;
			_generate_certificate (out local_cert_der, out local_cert_pem, out local_key_pem);

			result_cert_der = new Bytes.take ((owned) local_cert_der);
			result_cert_pem = (owned) local_cert_pem;
			result_key_pem = (owned) local_key_pem;

			var idle_source = new IdleSource ();
			idle_source.set_callback (generate_certificate.callback);
			idle_source.attach (caller_context);

			return true;
		});

		yield;

		cert_der = Bytes.unref_to_data ((owned) result_cert_der);
		cert_pem = (owned) result_cert_pem;
		key_pem = (owned) result_key_pem;
	}

	public extern void _generate_certificate (out uint8[] cert_der, out string cert_pem, out string key_pem);
}
#endif
