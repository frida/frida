[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public interface TunnelConnection : Object {
		public signal void closed ();

		public abstract NetworkStack tunnel_netstack {
			get;
		}

		public abstract InetAddress remote_address {
			get;
		}

		public abstract uint16 remote_rsd_port {
			get;
		}

		public abstract async void close (Cancellable? cancellable) throws IOError;

		protected static Bytes make_handshake_request (size_t mtu) {
			string body = Json.to_string (
				new Json.Builder ()
				.begin_object ()
					.set_member_name ("type")
					.add_string_value ("clientHandshakeRequest")
					.set_member_name ("mtu")
					.add_int_value (mtu)
				.end_object ()
				.get_root (), false);
			return make_request (body.data);
		}

		protected static Bytes make_request (uint8[] body) {
			return new BufferBuilder (BIG_ENDIAN)
				.append_string ("CDTunnel", StringTerminator.NONE)
				.append_uint16 ((uint16) body.length)
				.append_data (body)
				.build ();
		}
	}

	public sealed class TunnelParameters {
		public InetAddress address;
		public uint16 mtu;
		public InetAddress server_address;
		public uint16 server_rsd_port;

		public static TunnelParameters from_json (JsonObjectReader reader) throws Error {
			reader.read_member ("clientParameters");

			reader.read_member ("address");
			string address = reader.get_string_value ();
			reader.end_member ();

			reader.read_member ("mtu");
			uint16 mtu = reader.get_uint16_value ();
			reader.end_member ();

			reader.end_member ();

			reader.read_member ("serverAddress");
			string server_address = reader.get_string_value ();
			reader.end_member ();

			reader.read_member ("serverRSDPort");
			uint16 server_rsd_port = reader.get_uint16_value ();
			reader.end_member ();

			return new TunnelParameters () {
				address = new InetAddress.from_string (address),
				mtu = (uint16) mtu,
				server_address = new InetAddress.from_string (server_address),
				server_rsd_port = server_rsd_port,
			};
		}
	}

	public sealed class TcpTunnelConnection : Object, TunnelConnection, AsyncInitable {
		public IOStream stream {
			get;
			construct;
		}

		public NetworkStack tunnel_netstack {
			get {
				return _tunnel_netstack;
			}
		}

		public InetAddress remote_address {
			get {
				return tunnel_params.server_address;
			}
		}

		public uint16 remote_rsd_port {
			get {
				return tunnel_params.server_rsd_port;
			}
		}

		private Promise<bool> close_request = new Promise<bool> ();

		private TunnelParameters tunnel_params;
		private Bytes flush_trigger_datagram;
		private VirtualNetworkStack _tunnel_netstack;

		private BufferedInputStream input;
		private OutputStream output;

		private Gee.Queue<Bytes> pending_output = new Gee.ArrayQueue<Bytes> ();
		private bool writing = false;

		private Cancellable io_cancellable = new Cancellable ();

		private const size_t PREFERRED_MTU = 16000;
		private const size_t REMOTEPAIRINGDEVICED_DEFER_THRESHOLD = 8192;
		private const string PSK_IDENTITY = "com.apple.CoreDevice.TunnelService.Identity";

		public static async TcpTunnelConnection open (InetSocketAddress address, NetworkStack netstack, TunnelKey local_keypair,
				TunnelKey remote_pubkey, Cancellable? cancellable = null) throws Error, IOError {
			var raw_stream = yield netstack.open_tcp_connection (address, cancellable);

			var tls_stream = yield TlsPskClientStream.open (raw_stream, PSK_IDENTITY,
				get_raw_private_key (local_keypair.handle), cancellable);

			var connection = new TcpTunnelConnection (tls_stream);

			try {
				yield connection.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return connection;
		}

		public static async TcpTunnelConnection open_stream (IOStream stream, Cancellable? cancellable = null)
				throws Error, IOError {
			var connection = new TcpTunnelConnection (stream);

			try {
				yield connection.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return connection;
		}

		private TcpTunnelConnection (IOStream stream) {
			Object (stream: stream);
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			input = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
				"base-stream", stream.get_input_stream (),
				"close-base-stream", false,
				"buffer-size", 128 * 1024);
			output = stream.get_output_stream ();

			post (make_handshake_request (PREFERRED_MTU));

			tunnel_params = TunnelParameters.from_json (yield read_message (cancellable));
			flush_trigger_datagram = make_minimal_dummy_ipv6_datagram (tunnel_params.address, tunnel_params.server_address);

			_tunnel_netstack = new VirtualNetworkStack (null, tunnel_params.address, tunnel_params.mtu);
			_tunnel_netstack.outgoing_datagrams.connect (post_batch);

			process_incoming_messages.begin ();

			return true;
		}

		public async void close (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			try {
				yield close_request.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private async void process_incoming_messages () {
			try {
				while (true) {
					var pending_input = new Gee.ArrayQueue<Bytes> ();
					while (true) {
						var size = peek_datagram_size ();
						if (size == -1)
							break;

						var datagram = new uint8[size];
						input.read (datagram, io_cancellable);

						pending_input.offer (new Bytes.take ((owned) datagram));
					}

					if (pending_input.size != 0) {
						try {
							yield _tunnel_netstack.handle_incoming_datagrams (pending_input);
						} catch (Error e) {
						}

						var source = new IdleSource ();
						source.set_callback (process_incoming_messages.callback);
						source.attach (MainContext.get_thread_default ());
						yield;
					}

					ssize_t n;
					try {
						n = yield input.fill_async (-1, Priority.DEFAULT, io_cancellable);
					} catch (GLib.Error e) {
						throw new Error.TRANSPORT ("Connection closed");
					}

					if (n == 0)
						throw new Error.TRANSPORT ("Connection closed");
				}
			} catch (GLib.Error e) {
			}

			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (process_incoming_messages.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield stream.close_async ();
			} catch (GLib.Error e) {
			}

			if (_tunnel_netstack != null)
				_tunnel_netstack.stop ();

			close_request.resolve (true);

			closed ();
		}

		private void post (Bytes datagram) {
			var batch = new Gee.ArrayList<Bytes> ();
			batch.add (datagram);
			post_batch (batch);
		}

		private void post_batch (Gee.Collection<Bytes> datagrams) {
			foreach (var d in datagrams)
				pending_output.offer (d);

			if (!writing) {
				writing = true;
				process_pending_output.begin ();
			}
		}

		private async void process_pending_output () {
			while (!pending_output.is_empty) {
				size_t size = 0;
				foreach (var d in pending_output)
					size += d.get_size ();

				var batch = new ByteArray.sized ((uint) size);
				foreach (var d in pending_output)
					batch.append (d.get_data ());

				pending_output = new Gee.ArrayQueue<Bytes> ();

				size_t bytes_written;
				try {
					yield output.write_all_async (batch.data, Priority.DEFAULT, io_cancellable, out bytes_written);
				} catch (GLib.Error e) {
					break;
				}

				if (pending_output.is_empty && size > REMOTEPAIRINGDEVICED_DEFER_THRESHOLD) {
					try {
						yield output.write_all_async (flush_trigger_datagram.get_data (), Priority.DEFAULT,
							io_cancellable, null);
					} catch (GLib.Error e) {
					}
				}
			}

			writing = false;
		}

		private async JsonObjectReader read_message (Cancellable? cancellable) throws Error, IOError {
			size_t header_size = 10;
			if (input.get_available () < header_size)
				yield fill_until_n_bytes_available (header_size, cancellable);

			uint8 raw_magic[8];
			input.peek (raw_magic);
			string magic = ((string) raw_magic).make_valid (raw_magic.length);
			if (magic != "CDTunnel")
				throw new Error.PROTOCOL ("Invalid message magic: '%s'", magic);

			uint16 body_size = 0;
			unowned uint8[] size_buf = ((uint8[]) &body_size)[:2];
			input.peek (size_buf, raw_magic.length);
			body_size = uint16.from_big_endian (body_size);

			size_t full_size = header_size + body_size;
			if (input.get_available () < full_size)
				yield fill_until_n_bytes_available (full_size, cancellable);

			var body = new uint8[body_size + 1];
			input.peek (body[:body_size], header_size);
			body.length = body_size;

			input.skip (full_size, cancellable);

			unowned string json = (string) body;
			if (!json.validate ())
				throw new Error.PROTOCOL ("Invalid UTF-8");

			return new JsonObjectReader (json);
		}

		private ssize_t peek_datagram_size () {
			ssize_t header_size = 40;
			if (input.get_available () < header_size)
				return -1;

			uint16 payload_size = 0;
			unowned uint8[] size_buf = ((uint8[]) &payload_size)[:2];
			input.peek (size_buf, 4);
			payload_size = uint16.from_big_endian (payload_size);

			ssize_t full_size = header_size + payload_size;
			if (input.get_available () < full_size)
				return -1;

			return full_size;
		}

		private async void fill_until_n_bytes_available (size_t minimum, Cancellable? cancellable) throws Error, IOError {
			size_t available = input.get_available ();
			while (available < minimum) {
				if (input.get_buffer_size () < minimum)
					input.set_buffer_size (minimum);

				ssize_t n;
				try {
					n = yield input.fill_async ((ssize_t) (input.get_buffer_size () - available), Priority.DEFAULT,
						cancellable);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("Connection closed");
				}

				if (n == 0)
					throw new Error.TRANSPORT ("Connection closed");

				available += n;
			}
		}

		private static Bytes make_minimal_dummy_ipv6_datagram (InetAddress sender, InetAddress recipient) {
			assert (sender.get_native_size () == 16 && recipient.get_native_size () == 16);

			uint8 hop_limit = 1;
			unowned uint8[] src = sender.to_bytes ();
			unowned uint8[] dst = recipient.to_bytes ();

			return new BufferBuilder (BIG_ENDIAN)
				.append_uint32 (0x60000000)
				.append_uint16 (0)
				.append_uint8 (59)
				.append_uint8 (hop_limit)
				.append_data (src[:16])
				.append_data (dst[:16])
				.build ();
		}
	}

	public sealed class QuicTunnelConnection : Object, TunnelConnection, AsyncInitable {
		public InetSocketAddress address {
			get;
			construct;
		}

		public NetworkStack netstack {
			get;
			construct;
		}

		public NetworkStack tunnel_netstack {
			get {
				return _tunnel_netstack;
			}
		}

		public TunnelKey local_keypair {
			get;
			construct;
		}

		public TunnelKey remote_pubkey {
			get;
			construct;
		}

		public InetAddress remote_address {
			get {
				return tunnel_params.server_address;
			}
		}

		public uint16 remote_rsd_port {
			get {
				return tunnel_params.server_rsd_port;
			}
		}

		private State state = ACTIVE;
		private Promise<bool> establish_request = new Promise<bool> ();
		private Promise<bool> close_request = new Promise<bool> ();

		private Stream? control_stream;
		private TunnelParameters tunnel_params;
		private VirtualNetworkStack? _tunnel_netstack;

		private Gee.Map<int64?, Stream> streams = new Gee.HashMap<int64?, Stream> (Numeric.int64_hash, Numeric.int64_equal);
		private Gee.Queue<Bytes> pending_input = new Gee.ArrayQueue<Bytes> ();
		private bool input_flush_scheduled = false;
		private bool input_flush_happening = false;
		private Gee.Queue<Bytes> pending_output = new Gee.ArrayQueue<Bytes> ();

		private DatagramBasedSource? rx_source;
		private uint8[] rx_buf = new uint8[MAX_UDP_PAYLOAD_SIZE];
		private uint8[] tx_buf = new uint8[MAX_UDP_PAYLOAD_SIZE];
		private Source? write_idle;
		private Source? expiry_timer;

		private UdpSocket? socket;
		private uint8[] raw_local_address;
		private NGTcp2.Connection? connection;
		private NGTcp2.Crypto.ConnectionRef connection_ref;
		private OpenSSL.SSLContext ssl_ctx;
		private OpenSSL.SSL ssl;

		private MainContext main_context;

		private Cancellable io_cancellable = new Cancellable ();

		private enum State {
			ACTIVE,
			CLOSE_SCHEDULED,
			CLOSE_WRITTEN,
		}

		private const string ALPN = "\x1bRemotePairingTunnelProtocol";

		private const size_t NETWORK_MTU = 1500;

		private const size_t ETHERNET_HEADER_SIZE = 14;
		private const size_t IPV6_HEADER_SIZE = 40;
		private const size_t UDP_HEADER_SIZE = 8;
		private const size_t QUIC_HEADER_MAX_SIZE = 38;

		private const size_t MAX_UDP_PAYLOAD_SIZE = NETWORK_MTU - ETHERNET_HEADER_SIZE - IPV6_HEADER_SIZE - UDP_HEADER_SIZE;
		private const size_t PREFERRED_MTU = MAX_UDP_PAYLOAD_SIZE - QUIC_HEADER_MAX_SIZE;

		private const size_t MAX_QUIC_DATAGRAM_SIZE = 14000;
		private const NGTcp2.Duration KEEP_ALIVE_TIMEOUT = 15ULL * NGTcp2.SECONDS;

		public static async QuicTunnelConnection open (InetSocketAddress address, NetworkStack netstack, TunnelKey local_keypair,
				TunnelKey remote_pubkey, Cancellable? cancellable = null) throws Error, IOError {
			var connection = new QuicTunnelConnection (address, netstack, local_keypair, remote_pubkey);

			try {
				yield connection.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return connection;
		}

		private QuicTunnelConnection (InetSocketAddress address, NetworkStack netstack, TunnelKey local_keypair,
				TunnelKey remote_pubkey) {
			Object (
				address: address,
				netstack: netstack,
				local_keypair: local_keypair,
				remote_pubkey: remote_pubkey
			);
		}

		construct {
			connection_ref.get_conn = conn_ref => {
				QuicTunnelConnection * self = conn_ref.user_data;
				return self->connection;
			};
			connection_ref.user_data = this;

			ssl_ctx = new OpenSSL.SSLContext (OpenSSL.SSLMethod.tls_client ());
#if !HAVE_NGTCP2_CRYPTO_OSSL
			NGTcp2.Crypto.Quictls.configure_client_context (ssl_ctx);
#endif
			ssl_ctx.use_certificate (make_certificate (local_keypair.handle));
			ssl_ctx.use_private_key (local_keypair.handle);

			ssl = new OpenSSL.SSL (ssl_ctx);
#if HAVE_NGTCP2_CRYPTO_OSSL
			NGTcp2.Crypto.Ossl.configure_client_session (ssl);
#endif
			ssl.set_app_data (&connection_ref);
			ssl.set_connect_state ();
			ssl.set_alpn_protos (ALPN.data);
#if !HAVE_NGTCP2_CRYPTO_OSSL
			ssl.set_quic_transport_version (OpenSSL.TLSExtensionType.quic_transport_parameters);
#endif

			main_context = MainContext.ref_thread_default ();
		}

		public override void dispose () {
			perform_teardown ();

			base.dispose ();
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			socket = netstack.create_udp_socket ();
			socket.bind ((InetSocketAddress) Object.new (typeof (InetSocketAddress),
				address: netstack.listener_ip,
				scope_id: netstack.scope_id
			));
			socket.socket_connect (address, cancellable);

			raw_local_address = address_to_native (socket.get_local_address ());
			uint8[] raw_remote_address = address_to_native (address);

			var dcid = make_connection_id (NGTcp2.MIN_INITIAL_DCIDLEN);
			var scid = make_connection_id (NGTcp2.MIN_INITIAL_DCIDLEN);

			var path = NGTcp2.Path () {
				local = NGTcp2.Address () { addr = raw_local_address },
				remote = NGTcp2.Address () { addr = raw_remote_address },
			};

			var callbacks = NGTcp2.Callbacks () {
				get_new_connection_id = on_get_new_connection_id,
				extend_max_local_streams_bidi = (conn, max_streams, user_data) => {
					QuicTunnelConnection * self = user_data;
					return self->on_extend_max_local_streams_bidi (max_streams);
				},
				stream_close = (conn, flags, stream_id, app_error_code, user_data, stream_user_data) => {
					QuicTunnelConnection * self = user_data;
					return self->on_stream_close (flags, stream_id, app_error_code);
				},
				recv_stream_data = (conn, flags, stream_id, offset, data, user_data, stream_user_data) => {
					QuicTunnelConnection * self = user_data;
					return self->on_recv_stream_data (flags, stream_id, offset, data);
				},
				recv_datagram = (conn, flags, data, user_data) => {
					QuicTunnelConnection * self = user_data;
					return self->on_recv_datagram (flags, data);
				},
				rand = on_rand,
				client_initial = NGTcp2.Crypto.client_initial_cb,
				recv_crypto_data = NGTcp2.Crypto.recv_crypto_data_cb,
				encrypt = NGTcp2.Crypto.encrypt_cb,
				decrypt = NGTcp2.Crypto.decrypt_cb,
				hp_mask = NGTcp2.Crypto.hp_mask_cb,
				recv_retry = NGTcp2.Crypto.recv_retry_cb,
				update_key = NGTcp2.Crypto.update_key_cb,
				delete_crypto_aead_ctx = NGTcp2.Crypto.delete_crypto_aead_ctx_cb,
				delete_crypto_cipher_ctx = NGTcp2.Crypto.delete_crypto_cipher_ctx_cb,
				get_path_challenge_data = NGTcp2.Crypto.get_path_challenge_data_cb,
				version_negotiation = NGTcp2.Crypto.version_negotiation_cb,
			};

			var settings = NGTcp2.Settings.make_default ();
			settings.initial_ts = make_timestamp ();
			settings.max_tx_udp_payload_size = MAX_UDP_PAYLOAD_SIZE;
			settings.no_tx_udp_payload_size_shaping = true;
			settings.handshake_timeout = 5ULL * NGTcp2.SECONDS;

			var transport_params = NGTcp2.TransportParams.make_default ();
			transport_params.max_datagram_frame_size = MAX_QUIC_DATAGRAM_SIZE;
			transport_params.max_idle_timeout = 30ULL * NGTcp2.SECONDS;
			transport_params.initial_max_data = 1048576;
			transport_params.initial_max_stream_data_bidi_local = 1048576;

			NGTcp2.Connection.make_client (out connection, dcid, scid, path, NGTcp2.ProtocolVersion.V1, callbacks,
				settings, transport_params, null, this);
			connection.set_tls_native_handle (ssl);
			connection.set_keep_alive_timeout (KEEP_ALIVE_TIMEOUT);

			rx_source = socket.datagram_based.create_source (IOCondition.IN, io_cancellable);
			rx_source.set_callback (on_socket_readable);
			rx_source.attach (main_context);

			process_pending_writes ();

			yield establish_request.future.wait_async (cancellable);

			return true;
		}

		private void on_control_stream_opened () {
			var zeroed_padding_packet = new uint8[PREFERRED_MTU];
			send_datagram (new Bytes.take ((owned) zeroed_padding_packet));

			control_stream.send (make_handshake_request (PREFERRED_MTU).get_data ());
		}

		private void on_control_stream_response (string json) throws Error {
			tunnel_params = TunnelParameters.from_json (new JsonObjectReader (json));

			_tunnel_netstack = new VirtualNetworkStack (null, tunnel_params.address, tunnel_params.mtu);
			_tunnel_netstack.outgoing_datagrams.connect (send_datagrams);

			establish_request.resolve (true);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (connection == null)
				return;

			state = CLOSE_SCHEDULED;
			process_pending_writes ();

			try {
				yield close_request.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private void perform_teardown () {
			if (close_request.future.ready)
				return;

			connection = null;
			socket = null;

			io_cancellable.cancel ();

			if (rx_source != null) {
				rx_source.destroy ();
				rx_source = null;
			}

			if (write_idle != null) {
				write_idle.destroy ();
				write_idle = null;
			}

			if (expiry_timer != null) {
				expiry_timer.destroy ();
				expiry_timer = null;
			}

			if (_tunnel_netstack != null)
				_tunnel_netstack.stop ();

			close_request.resolve (true);

			closed ();
		}

		private void on_stream_data_available (Stream stream, uint8[] data, out size_t consumed) {
			if (stream != control_stream || establish_request.future.ready) {
				consumed = data.length;
				return;
			}

			consumed = 0;

			if (data.length < 12)
				return;

			var buf = new Buffer (new Bytes.static (data), BIG_ENDIAN);

			try {
				string magic = buf.read_fixed_string (0, 8);
				if (magic != "CDTunnel")
					throw new Error.PROTOCOL ("Invalid magic");

				size_t body_size = buf.read_uint16 (8);
				size_t body_available = data.length - 10;
				if (body_available < body_size)
					return;

				var raw_json = new uint8[body_size + 1];
				Memory.copy (raw_json, data + 10, body_size);

				unowned string json = (string) raw_json;
				if (!json.validate ())
					throw new Error.PROTOCOL ("Invalid UTF-8");

				on_control_stream_response (json);

				consumed = 10 + body_size;
			} catch (Error e) {
				if (!establish_request.future.ready)
					establish_request.reject (e);
			}
		}

		private void send_datagram (Bytes datagram) {
			var batch = new Gee.ArrayList<Bytes> ();
			batch.add (datagram);
			send_datagrams (batch);
		}

		private void send_datagrams (Gee.Collection<Bytes> datagrams) {
			foreach (var d in datagrams)
				pending_output.offer (d);
			process_pending_writes ();
		}

		private bool on_socket_readable (DatagramBased datagram_based, IOCondition condition) {
			try {
				InetSocketAddress remote_address;
				size_t n = Udp.recv (rx_buf, socket.datagram_based, io_cancellable, out remote_address);

				uint8[] raw_remote_address = address_to_native (remote_address);

				var path = NGTcp2.Path () {
					local = NGTcp2.Address () { addr = raw_local_address },
					remote = NGTcp2.Address () { addr = raw_remote_address },
				};

				unowned uint8[] data = rx_buf[:n];

				var res = connection.read_packet (path, null, data, make_timestamp ());
				if (res == NGTcp2.ErrorCode.DRAINING)
					perform_teardown ();
			} catch (GLib.Error e) {
				return Source.REMOVE;
			} finally {
				process_pending_writes ();
			}

			return Source.CONTINUE;
		}

		private void process_pending_writes () {
			if (connection == null || write_idle != null)
				return;

			var source = new IdleSource ();
			source.set_callback (() => {
				write_idle = null;
				do_process_pending_writes ();
				return Source.REMOVE;
			});
			source.attach (main_context);
			write_idle = source;
		}

		private void do_process_pending_writes () {
			var ts = make_timestamp ();

			var pi = NGTcp2.PacketInfo ();
			Gee.Iterator<Stream> stream_iter = streams.values.iterator ();
			while (true) {
				ssize_t n = -1;

				if (state == CLOSE_SCHEDULED) {
					var error = NGTcp2.ConnectionError.application (0);
					n = connection.write_connection_close (null, &pi, tx_buf, error, ts);
					state = CLOSE_WRITTEN;
				} else {
					Bytes? datagram = pending_output.peek ();
					if (datagram != null) {
						int accepted = -1;
						n = connection.write_datagram (null, null, tx_buf, &accepted, NGTcp2.WriteDatagramFlags.MORE, 0,
							datagram.get_data (), ts);
						if (accepted > 0)
							pending_output.poll ();
					} else {
						Stream? stream = null;
						unowned uint8[]? data = null;
						NGTcp2.WriteStreamFlags stream_flags = MORE;

						while (stream == null && stream_iter.next ()) {
							Stream s = stream_iter.get ();
							uint64 len = s.tx_buf.len;
							uint64 limit = 0;

							if (len != 0 && (limit = connection.get_max_stream_data_left (s.id)) != 0) {
								stream = s;
								data = s.tx_buf.data[:(int) uint64.min (len, limit)];
								break;
							}
						}

						ssize_t datalen = 0;
						n = connection.write_stream (null, &pi, tx_buf, &datalen, stream_flags,
							(stream != null) ? stream.id : -1, data, ts);
						if (datalen > 0)
							stream.tx_buf.remove_range (0, (uint) datalen);
					}
				}

				if (n == 0)
					break;
				if (n == NGTcp2.ErrorCode.WRITE_MORE)
					continue;
				if (n == NGTcp2.ErrorCode.CLOSING) {
					perform_teardown ();
					break;
				}
				if (n < 0)
					break;

				try {
					Udp.send (tx_buf[:n], socket.datagram_based, io_cancellable);
				} catch (GLib.Error e) {
					continue;
				}
			}

			if (expiry_timer != null) {
				expiry_timer.destroy ();
				expiry_timer = null;
			}

			if (close_request.future.ready)
				return;

			NGTcp2.Timestamp expiry = connection.get_expiry ();
			if (expiry == uint64.MAX)
				return;

			NGTcp2.Timestamp now = make_timestamp ();

			uint delta_msec;
			if (expiry > now) {
				uint64 delta_nsec = expiry - now;
				delta_msec = (uint) (delta_nsec / 1000000ULL);
			} else {
				delta_msec = 1;
			}

			var source = new TimeoutSource (delta_msec);
			source.set_callback (on_expiry);
			source.attach (main_context);
			expiry_timer = source;
		}

		private bool on_expiry () {
			int res = connection.handle_expiry (make_timestamp ());
			if (res != 0) {
				perform_teardown ();
				return Source.REMOVE;
			}

			process_pending_writes ();

			return Source.REMOVE;
		}

		private static int on_get_new_connection_id (NGTcp2.Connection conn, out NGTcp2.ConnectionID cid, uint8[] token,
				size_t cidlen, void * user_data) {
			cid = make_connection_id (cidlen);

			OpenSSL.Rng.generate (token[:NGTcp2.STATELESS_RESET_TOKENLEN]);

			return 0;
		}

		private int on_extend_max_local_streams_bidi (uint64 max_streams) {
			if (control_stream == null) {
				control_stream = open_bidi_stream ();

				var source = new IdleSource ();
				source.set_callback (() => {
					on_control_stream_opened ();
					return Source.REMOVE;
				});
				source.attach (main_context);
			}

			return 0;
		}

		private int on_stream_close (uint32 flags, int64 stream_id, uint64 app_error_code) {
			if (!establish_request.future.ready) {
				establish_request.reject (new Error.TRANSPORT ("Connection closed early with QUIC app error code %" +
					uint64.FORMAT_MODIFIER + "u", app_error_code));
			}

			perform_teardown ();

			return 0;
		}

		private int on_recv_stream_data (uint32 flags, int64 stream_id, uint64 offset, uint8[] data) {
			Stream? stream = streams[stream_id];
			if (stream != null)
				stream.on_recv (data);

			return 0;
		}

		private int on_recv_datagram (uint32 flags, uint8[] data) {
			pending_input.offer (new Bytes (data));

			if (!input_flush_scheduled) {
				var source = new IdleSource ();
				source.set_callback (() => {
					input_flush_scheduled = false;
					flush_pending_input.begin ();
					return Source.REMOVE;
				});
				source.attach (MainContext.get_thread_default ());
				input_flush_scheduled = true;
			}

			return 0;
		}

		private async void flush_pending_input () {
			if (input_flush_happening)
				return;
			input_flush_happening = true;

			var datagrams = pending_input;
			pending_input = new Gee.ArrayQueue<Bytes> ();

			try {
				yield _tunnel_netstack.handle_incoming_datagrams (datagrams);
			} catch (GLib.Error e) {
			}

			input_flush_happening = false;
		}

		private static void on_rand (uint8[] dest, NGTcp2.RNGContext rand_ctx) {
			OpenSSL.Rng.generate (dest);
		}

		private static uint8[] address_to_native (SocketAddress address) {
			var size = address.get_native_size ();
			var buf = new uint8[size];
			try {
				address.to_native (buf, size);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
			return buf;
		}

		private static NGTcp2.ConnectionID make_connection_id (size_t len) {
			var cid = NGTcp2.ConnectionID () {
				datalen = len,
			};

			NGTcp2.ConnectionID * mutable_cid = &cid;
			OpenSSL.Rng.generate (mutable_cid->data[:len]);

			return cid;
		}

		private static NGTcp2.Timestamp make_timestamp () {
			return get_monotonic_time () * NGTcp2.MICROSECONDS;
		}

		private static OpenSSL.X509 make_certificate (OpenSSL.Envelope.Key keypair) {
			var cert = new OpenSSL.X509 ();
			cert.get_serial_number ().set_uint64 (1);
			cert.get_not_before ().adjust (0);
			cert.get_not_after ().adjust (5260000);

			unowned OpenSSL.X509.Name name = cert.get_subject_name ();
			cert.set_issuer_name (name);
			cert.set_pubkey (keypair);

			var mc = new OpenSSL.Envelope.MessageDigestContext ();
			mc.digest_sign_init (null, null, null, keypair);
			cert.sign_ctx (mc);

			return cert;
		}

		private Stream open_bidi_stream () {
			int64 id;
			connection.open_bidi_stream (out id, null);

			var stream = new Stream (this, id);
			streams[id] = stream;

			return stream;
		}

		private class Stream {
			public int64 id;

			private weak QuicTunnelConnection parent;

			public ByteArray rx_buf = new ByteArray.sized (256);
			public ByteArray tx_buf = new ByteArray.sized (128);

			public Stream (QuicTunnelConnection parent, int64 id) {
				this.parent = parent;
				this.id = id;
			}

			public void send (uint8[] data) {
				tx_buf.append (data);
				parent.process_pending_writes ();
			}

			public void on_recv (uint8[] data) {
				rx_buf.append (data);

				size_t consumed;
				parent.on_stream_data_available (this, rx_buf.data, out consumed);

				if (consumed != 0)
					rx_buf.remove_range (0, (uint) consumed);
			}
		}
	}

	public sealed class TunnelKey {
		public OpenSSL.Envelope.Key handle;

		public TunnelKey (owned OpenSSL.Envelope.Key handle) {
			this.handle = (owned) handle;
		}
	}
}
