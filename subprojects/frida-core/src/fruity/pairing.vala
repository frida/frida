[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	using OpenSSL;
	using OpenSSL.Envelope;

	public sealed class PairingService : Object, AsyncInitable {
		public const string DNS_SD_NAME = "_remotepairing._tcp.local";

		private signal void events_received (ObjectReader events);

		public PairingTransport transport {
			get;
			construct;
		}

		public PairingStore store {
			get;
			construct;
		}

		public DeviceOptions device_options {
			get;
			private set;
		}

		public DeviceInfo? device_info {
			get;
			private set;
		}

		public PairingPeer? established_peer {
			get;
			private set;
		}

		private Gee.Queue<Promise<ObjectReader>> requests = new Gee.ArrayQueue<Promise<ObjectReader>> ();
		private uint64 next_control_sequence_number = 0;
		private uint64 next_encrypted_sequence_number = 0;

		private ChaCha20Poly1305? client_cipher;
		private ChaCha20Poly1305? server_cipher;

		public static async PairingService open (PairingTransport transport, PairingStore store, Cancellable? cancellable = null)
				throws Error, IOError {
			var service = new PairingService (transport, store);

			try {
				yield service.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return service;
		}

		private PairingService (PairingTransport transport, PairingStore store) {
			Object (transport: transport, store: store);
		}

		construct {
			transport.close.connect (on_close);
			transport.message.connect (on_message);
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			yield transport.open (cancellable);

			yield attempt_pair_verify (cancellable);

			PairingPeer? peer = yield verify_manual_pairing (cancellable);
			if (peer == null) {
				if (!device_options.allows_pair_setup)
					throw new Error.NOT_SUPPORTED ("Device not paired and pairing not allowed on current transport");
				peer = yield setup_manual_pairing (cancellable);
			}

			if (peer.remote_unlock_host_key == null) {
				bool peer_modified = false;
				try {
					peer.remote_unlock_host_key = yield create_remote_unlock_key (cancellable);
					peer_modified = true;
				} catch (Error e) {
					if (e is Error.INVALID_OPERATION) {
						store.forget_peer (peer);
						throw e;
					}
					if (!(e is Error.NOT_SUPPORTED))
						throw e;
				}
				if (peer_modified) {
					try {
						store.save_peer (peer);
					} catch (Error e) {
					}
				}
			}

			established_peer = peer;

			return true;
		}

		public void close () {
			transport.cancel ();
		}

#if !MACOS
		public async TunnelConnection open_tunnel (InetAddress device_address, NetworkStack netstack,
				Cancellable? cancellable = null) throws Error, IOError {
			string? protocol = Environment.get_variable ("FRIDA_FRUITY_TUNNEL_PROTOCOL");
			if (protocol == null)
				protocol = "tcp";

			Key local_keypair;
			uint8[] key;
			if (protocol == "quic") {
				local_keypair = make_keypair (RSA);
				key = key_to_der (local_keypair);
			} else {
				local_keypair = make_keypair (ED25519);
				key = get_raw_private_key (local_keypair).get_data ();
			}

			string request = Json.to_string (
				new Json.Builder ()
				.begin_object ()
					.set_member_name ("request")
					.begin_object ()
						.set_member_name ("_0")
						.begin_object ()
							.set_member_name ("createListener")
							.begin_object ()
								.set_member_name ("transportProtocolType")
								.add_string_value (protocol)
								.set_member_name ("key")
								.add_string_value (Base64.encode (key))
							.end_object ()
						.end_object ()
					.end_object ()
				.end_object ()
				.get_root (), false);

			string response = yield request_encrypted (request, cancellable);

			Json.Reader reader;
			try {
				reader = make_json_reader (response);
			} catch (GLib.Error e) {
				throw new Error.PROTOCOL ("Invalid response JSON");
			}

			reader.read_member ("response");
			reader.read_member ("_1");
			reader.read_member ("createListener");

			if (!reader.read_member ("devicePublicKey"))
				throw new Error.NOT_SUPPORTED ("Unsupported tunnel service");
			string? device_pubkey = reader.get_string_value ();
			reader.end_member ();

			reader.read_member ("port");
			uint16 port = (uint16) reader.get_int_value ();
			reader.end_member ();

			GLib.Error? error = reader.get_error ();
			if (error != null)
				throw new Error.PROTOCOL ("Invalid response: %s", error.message);

			Key remote_pubkey = key_from_der (Base64.decode (device_pubkey));

			var tunnel_endpoint = (InetSocketAddress) Object.new (typeof (InetSocketAddress),
				address: device_address,
				port: port,
				scope_id: netstack.scope_id
			);

			if (protocol == "quic") {
				return yield QuicTunnelConnection.open (
					tunnel_endpoint,
					netstack,
					new TunnelKey ((owned) local_keypair),
					new TunnelKey ((owned) remote_pubkey),
					cancellable);
			} else {
				return yield TcpTunnelConnection.open (
					tunnel_endpoint,
					netstack,
					new TunnelKey ((owned) local_keypair),
					new TunnelKey ((owned) remote_pubkey),
					cancellable);
			}
		}
#endif

		private async void attempt_pair_verify (Cancellable? cancellable) throws Error, IOError {
			Bytes payload = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("request")
					.begin_dictionary ()
						.set_member_name ("_0")
						.begin_dictionary ()
							.set_member_name ("handshake")
							.begin_dictionary ()
								.set_member_name ("_0")
								.begin_dictionary ()
									.set_member_name ("wireProtocolVersion")
									.add_int64_value (19)
									.set_member_name ("hostOptions")
									.begin_dictionary ()
										.set_member_name ("attemptPairVerify")
										.add_bool_value (true)
									.end_dictionary ()
								.end_dictionary ()
							.end_dictionary ()
						.end_dictionary ()
					.end_dictionary ()
				.end_dictionary ()
				.build ();

			ObjectReader response = yield request_plain (payload, cancellable);

			response
				.read_member ("response")
				.read_member ("_1")
				.read_member ("handshake")
				.read_member ("_0");

			response.read_member ("deviceOptions");

			bool allows_pair_setup = response.read_member ("allowsPairSetup").get_bool_value ();
			response.end_member ();

			bool allows_pinless_pairing = response.read_member ("allowsPinlessPairing").get_bool_value ();
			response.end_member ();

			bool allows_promptless_automation_pairing_upgrade =
				response.read_member ("allowsPromptlessAutomationPairingUpgrade").get_bool_value ();
			response.end_member ();

			bool allows_sharing_sensitive_info = response.read_member ("allowsSharingSensitiveInfo").get_bool_value ();
			response.end_member ();

			bool allows_incoming_tunnel_connections =
				response.read_member ("allowsIncomingTunnelConnections").get_bool_value ();
			response.end_member ();

			device_options = new DeviceOptions () {
				allows_pair_setup = allows_pair_setup,
				allows_pinless_pairing = allows_pinless_pairing,
				allows_promptless_automation_pairing_upgrade = allows_promptless_automation_pairing_upgrade,
				allows_sharing_sensitive_info = allows_sharing_sensitive_info,
				allows_incoming_tunnel_connections = allows_incoming_tunnel_connections,
			};

			if (response.has_member ("peerDeviceInfo")) {
				response.read_member ("peerDeviceInfo");

				string name = response.read_member ("name").get_string_value ();
				response.end_member ();

				string model = response.read_member ("model").get_string_value ();
				response.end_member ();

				string udid = response.read_member ("udid").get_string_value ();
				response.end_member ();

				uint64 ecid = response.read_member ("ecid").get_uint64_value ();
				response.end_member ();

				Plist kvs;
				try {
					kvs = new Plist.from_binary (response.read_member ("deviceKVSData").get_data_value ().get_data ());
					response.end_member ();
				} catch (PlistError e) {
					throw new Error.PROTOCOL ("%s", e.message);
				}

				device_info = new DeviceInfo () {
					name = name,
					model = model,
					udid = udid,
					ecid = ecid,
					kvs = kvs,
				};
			}
		}

		private async PairingPeer? verify_manual_pairing (Cancellable? cancellable) throws Error, IOError {
			Key host_keypair = make_keypair (X25519);
			uint8[] raw_host_pubkey = get_raw_public_key (host_keypair).get_data ();

			Bytes start_params = new PairingParamsBuilder ()
				.add_state (1)
				.add_public_key (host_keypair)
				.build ();

			Bytes start_payload = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("kind")
					.add_string_value ("verifyManualPairing")
					.set_member_name ("startNewSession")
					.add_bool_value (true)
					.set_member_name ("data")
					.add_data_value (start_params)
				.end_dictionary ()
				.build ();

			var start_response = yield request_pairing_data (start_payload, cancellable);
			if (start_response.has_member ("error")) {
				yield notify_pair_verify_failed (cancellable);
				return null;
			}
			uint8[] raw_device_pubkey = start_response.read_member ("public-key").get_data_value ().get_data ();
			start_response.end_member ();
			var device_pubkey = new Key.from_raw_public_key (X25519, null, raw_device_pubkey);

			Bytes shared_key = derive_shared_key (host_keypair, device_pubkey);

			Bytes operation_key = derive_chacha_key (shared_key,
				"Pair-Verify-Encrypt-Info",
				"Pair-Verify-Encrypt-Salt");

			var cipher = new ChaCha20Poly1305 (operation_key);

			var start_inner_response = new VariantReader (PairingParamsParser.parse (cipher.decrypt (
				new Bytes.static ("\x00\x00\x00\x00PV-Msg02".data[:12]),
				start_response.read_member ("encrypted-data").get_data_value ())));
			string peer_identifier = start_inner_response
				.read_member ("identifier")
				.get_uuid_value ();
			PairingPeer? peer = store.find_peer_by_identifier (peer_identifier);
			if (peer == null) {
				yield notify_pair_verify_failed (cancellable);
				return null;
			}
			// TODO: Verify signature using peer's public key.

			unowned string host_identifier = store.self_identity.identifier;

			var message = new ByteArray.sized (100);
			message.append (raw_host_pubkey);
			message.append (host_identifier.data);
			message.append (raw_device_pubkey);
			Bytes signature = compute_message_signature (ByteArray.free_to_bytes ((owned) message), store.self_identity.key);

			Bytes inner_params = new PairingParamsBuilder ()
				.add_identifier (host_identifier)
				.add_signature (signature)
				.build ();

			Bytes outer_params = new PairingParamsBuilder ()
				.add_state (3)
				.add_encrypted_data (
					cipher.encrypt (
						new Bytes.static ("\x00\x00\x00\x00PV-Msg03".data[:12]),
						inner_params))
				.build ();

			Bytes finish_payload = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("kind")
					.add_string_value ("verifyManualPairing")
					.set_member_name ("startNewSession")
					.add_bool_value (false)
					.set_member_name ("data")
					.add_data_value (outer_params)
				.end_dictionary ()
				.build ();

			ObjectReader finish_response = yield request_pairing_data (finish_payload, cancellable);
			if (finish_response.has_member ("error")) {
				yield notify_pair_verify_failed (cancellable);
				return null;
			}

			setup_main_encryption_keys (shared_key);

			return peer;
		}

		private async void notify_pair_verify_failed (Cancellable? cancellable) throws Error, IOError {
			yield post_plain (transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("event")
					.begin_dictionary ()
						.set_member_name ("_0")
						.begin_dictionary ()
							.set_member_name ("pairVerifyFailed")
							.begin_dictionary ()
							.end_dictionary ()
						.end_dictionary ()
					.end_dictionary ()
				.end_dictionary ()
				.build (), cancellable);
		}

		private async PairingPeer setup_manual_pairing (Cancellable? cancellable) throws Error, IOError {
			Bytes start_params = new PairingParamsBuilder ()
				.add_method (0)
				.add_state (1)
				.build ();

			Bytes start_payload = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("kind")
					.add_string_value ("setupManualPairing")
					.set_member_name ("startNewSession")
					.add_bool_value (true)
					.set_member_name ("sendingHost")
					.add_string_value (Environment.get_host_name ())
					.set_member_name ("data")
					.add_data_value (start_params)
				.end_dictionary ()
				.build ();

			var start_response = yield request_pairing_data (start_payload, cancellable);
			if (start_response.has_member ("retry-delay")) {
				uint16 retry_delay = start_response.read_member ("retry-delay").get_uint16_value ();
				throw new Error.INVALID_OPERATION ("Rate limit exceeded, try again in %u seconds", retry_delay);
			}

			Bytes remote_pubkey = start_response.read_member ("public-key").get_data_value ();
			start_response.end_member ();

			Bytes salt = start_response.read_member ("salt").get_data_value ();
			start_response.end_member ();

			var srp_session = new SRPClientSession ("Pair-Setup", "000000");
			srp_session.process (remote_pubkey, salt);
			Bytes shared_key = srp_session.key;

			Bytes verify_params = new PairingParamsBuilder ()
				.add_state (3)
				.add_raw_public_key (srp_session.public_key)
				.add_proof (srp_session.key_proof)
				.build ();

			Bytes verify_payload = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("kind")
					.add_string_value ("setupManualPairing")
					.set_member_name ("startNewSession")
					.add_bool_value (false)
					.set_member_name ("sendingHost")
					.add_string_value (Environment.get_host_name ())
					.set_member_name ("data")
					.add_data_value (verify_params)
				.end_dictionary ()
				.build ();

			var verify_response = yield request_pairing_data (verify_payload, cancellable);
			Bytes remote_proof = verify_response.read_member ("proof").get_data_value ();

			srp_session.verify_proof (remote_proof);

			Bytes operation_key = derive_chacha_key (shared_key,
				"Pair-Setup-Encrypt-Info",
				"Pair-Setup-Encrypt-Salt");

			var cipher = new ChaCha20Poly1305 (operation_key);

			Bytes signing_key = derive_chacha_key (shared_key,
				"Pair-Setup-Controller-Sign-Info",
				"Pair-Setup-Controller-Sign-Salt");

			unowned PairingIdentity self_identity = store.self_identity;
			Bytes self_identity_pubkey = get_raw_public_key (self_identity.key);

			var message = new ByteArray.sized (100);
			message.append (signing_key.get_data ());
			message.append (self_identity.identifier.data);
			message.append (self_identity_pubkey.get_data ());
			Bytes signature = compute_message_signature (ByteArray.free_to_bytes ((owned) message), self_identity.key);

			Bytes self_info = new OpackBuilder ()
				.begin_dictionary ()
					.set_member_name ("name")
					.add_string_value (Environment.get_host_name ())
					.set_member_name ("accountID")
					.add_string_value (self_identity.identifier)
					.set_member_name ("remotepairing_serial_number")
					.add_string_value ("AAAAAAAAAAAA")
					.set_member_name ("altIRK")
					.add_data_value (self_identity.irk)
					.set_member_name ("model")
					.add_string_value ("computer-model")
					.set_member_name ("mac")
					.add_data_value (new Bytes ({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }))
					.set_member_name ("btAddr")
					.add_string_value ("11:22:33:44:55:66")
				.end_dictionary ()
				.build ();

			Bytes inner_params = new PairingParamsBuilder ()
				.add_identifier (self_identity.identifier)
				.add_raw_public_key (self_identity_pubkey)
				.add_signature (signature)
				.add_info (self_info)
				.build ();

			Bytes outer_params = new PairingParamsBuilder ()
				.add_state (5)
				.add_encrypted_data (
					cipher.encrypt (
						new Bytes.static ("\x00\x00\x00\x00PS-Msg05".data[:12]),
						inner_params))
				.build ();

			Bytes finish_payload = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("kind")
					.add_string_value ("setupManualPairing")
					.set_member_name ("startNewSession")
					.add_bool_value (false)
					.set_member_name ("sendingHost")
					.add_string_value (Environment.get_host_name ())
					.set_member_name ("data")
					.add_data_value (outer_params)
				.end_dictionary ()
				.build ();

			var outer_finish_response = yield request_pairing_data (finish_payload, cancellable);
			var inner_finish_response = new VariantReader (PairingParamsParser.parse (
				cipher.decrypt (new Bytes.static ("\x00\x00\x00\x00PS-Msg06".data[:12]),
					outer_finish_response.read_member ("encrypted-data").get_data_value ())));

			string peer_identifier = inner_finish_response.read_member ("identifier").get_string_value ();
			inner_finish_response.end_member ();
			Bytes peer_pubkey = inner_finish_response.read_member ("public-key").get_data_value ();
			inner_finish_response.end_member ();
			Bytes peer_info = inner_finish_response.read_member ("info").get_data_value ();
			inner_finish_response.end_member ();

			PairingPeer peer = store.add_peer (peer_identifier, peer_pubkey, peer_info);

			setup_main_encryption_keys (shared_key);

			return peer;
		}

		private async Bytes? create_remote_unlock_key (Cancellable? cancellable) throws Error, IOError {
			string request = Json.to_string (
				new Json.Builder ()
				.begin_object ()
					.set_member_name ("request")
					.begin_object ()
						.set_member_name ("_0")
						.begin_object ()
							.set_member_name ("createRemoteUnlockKey")
							.begin_object ()
							.end_object ()
						.end_object ()
					.end_object ()
				.end_object ()
				.get_root (), false);

			string response = yield request_encrypted (request, cancellable);

			Json.Reader reader;
			try {
				reader = make_json_reader (response);
			} catch (GLib.Error e) {
				throw new Error.PROTOCOL ("Invalid createRemoteUnlockKey response JSON");
			}

			reader.read_member ("response");
			reader.read_member ("_1");

			if (reader.read_member ("errorExtended")) {
				reader.read_member ("_0");

				reader.read_member ("domain");
				unowned string? domain = reader.get_string_value ();
				reader.end_member ();

				reader.read_member ("code");
				int64 code = reader.get_int_value ();
				reader.end_member ();

				reader.read_member ("userInfo");
				reader.read_member ("NSLocalizedDescription");
				unowned string? description = reader.get_string_value ();

				if (domain == null || description == null)
					throw new Error.PROTOCOL ("Invalid createRemoteUnlockKey error response");

				if (domain == "com.apple.CoreDevice.ControlChannelConnectionError" && code == 2)
					throw new Error.INVALID_OPERATION ("%s", description);

				throw new Error.NOT_SUPPORTED ("%s", description);
			}
			reader.end_member ();

			reader.read_member ("createRemoteUnlockKey");
			reader.read_member ("hostKey");
			unowned string? key = reader.get_string_value ();
			if (key == null)
				throw new Error.PROTOCOL ("Malformed createRemoteUnlockKey response");
			return new Bytes (Base64.decode (key));
		}

		private void setup_main_encryption_keys (Bytes shared_key) {
			client_cipher = new ChaCha20Poly1305 (derive_chacha_key (shared_key, "ClientEncrypt-main"));
			server_cipher = new ChaCha20Poly1305 (derive_chacha_key (shared_key, "ServerEncrypt-main"));
		}

		private async ObjectReader request_pairing_data (Bytes payload, Cancellable? cancellable) throws Error, IOError {
			Bytes wrapper = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("event")
					.begin_dictionary ()
						.set_member_name ("_0")
						.begin_dictionary ()
							.set_member_name ("pairingData")
							.begin_dictionary ()
								.set_member_name ("_0")
								.add_raw_value (payload)
							.end_dictionary ()
						.end_dictionary ()
					.end_dictionary ()
				.end_dictionary ()
				.build ();

			var promise = new Promise<ObjectReader> ();
			var pairing_handler = events_received.connect (reader => {
				try {
					reader
						.read_member ("plain")
						.read_member ("_0")
						.read_member ("event")
						.read_member ("_0");

					if (reader.has_member ("pairingData") || reader.has_member ("pairingRejectedWithError"))
						promise.resolve (reader);
				} catch (Error e) {
					promise.reject (e);
					return;
				}
			});

			ObjectReader response = null;
			try {
				yield post_plain (wrapper, cancellable);
				response = yield promise.future.wait_async (cancellable);
			} finally {
				disconnect (pairing_handler);
			}

			if (response.has_member ("pairingRejectedWithError")) {
				string description = response
					.read_member ("pairingRejectedWithError")
					.read_member ("wrappedError")
					.read_member ("userInfo")
					.read_member ("NSLocalizedDescription")
					.get_string_value ();
				throw new Error.PROTOCOL ("%s", description);
			}

			Bytes raw_data = response
				.read_member ("pairingData")
				.read_member ("_0")
				.read_member ("data")
				.get_data_value ();
			Variant data = PairingParamsParser.parse (raw_data);
			return new VariantReader (data);
		}

		private async ObjectReader request_plain (Bytes payload, Cancellable? cancellable) throws Error, IOError {
			uint64 seqno = next_control_sequence_number++;
			var promise = new Promise<ObjectReader> ();
			requests.offer (promise);

			try {
				yield post_plain_with_sequence_number (seqno, payload, cancellable);
			} catch (GLib.Error e) {
				if (requests.remove (promise))
					promise.reject (e);
			}

			ObjectReader response = yield promise.future.wait_async (cancellable);

			return response
				.read_member ("plain")
				.read_member ("_0");
		}

		private async void post_plain (Bytes payload, Cancellable? cancellable) throws Error, IOError {
			uint64 seqno = next_control_sequence_number++;
			yield post_plain_with_sequence_number (seqno, payload, cancellable);
		}

		private async void post_plain_with_sequence_number (uint64 seqno, Bytes payload, Cancellable? cancellable)
				throws Error, IOError {
			transport.post (transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("sequenceNumber")
					.add_uint64_value (seqno)
					.set_member_name ("originatedBy")
					.add_string_value ("host")
					.set_member_name ("message")
					.begin_dictionary ()
						.set_member_name ("plain")
						.begin_dictionary ()
							.set_member_name ("_0")
							.add_raw_value (payload)
						.end_dictionary ()
					.end_dictionary ()
				.end_dictionary ()
				.build ());
		}

		private async string request_encrypted (string json, Cancellable? cancellable) throws Error, IOError {
			uint64 seqno = next_control_sequence_number++;
			var promise = new Promise<ObjectReader> ();
			requests.offer (promise);

			Bytes iv = new BufferBuilder (LITTLE_ENDIAN)
				.append_uint64 (next_encrypted_sequence_number++)
				.append_uint32 (0)
				.build ();

			Bytes raw_request = transport.make_object_builder ()
				.begin_dictionary ()
					.set_member_name ("sequenceNumber")
					.add_uint64_value (seqno)
					.set_member_name ("originatedBy")
					.add_string_value ("host")
					.set_member_name ("message")
					.begin_dictionary ()
						.set_member_name ("streamEncrypted")
						.begin_dictionary ()
							.set_member_name ("_0")
							.add_data_value (client_cipher.encrypt (iv, new Bytes.static (json.data)))
						.end_dictionary ()
					.end_dictionary ()
				.end_dictionary ()
				.build ();

			transport.post (raw_request);

			ObjectReader response = yield promise.future.wait_async (cancellable);

			Bytes encrypted_response = response
				.read_member ("streamEncrypted")
				.read_member ("_0")
				.get_data_value ();

			Bytes decrypted_response = server_cipher.decrypt (iv, encrypted_response);

			unowned string s = (string) decrypted_response.get_data ();
			if (!s.validate ((ssize_t) decrypted_response.get_size ()))
				throw new Error.PROTOCOL ("Invalid UTF-8");

			return s;
		}

		private void on_close (Error? error) {
			var e = (error != null)
				? error
				: new Error.TRANSPORT ("Connection closed while waiting for response");
			foreach (Promise<ObjectReader> promise in requests)
				promise.reject (e);
			requests.clear ();
		}

		private void on_message (ObjectReader reader) {
			try {
				string origin = reader.read_member ("originatedBy").get_string_value ();
				if (origin != "device")
					return;
				reader.end_member ();

				reader.read_member ("message");

				bool is_event = false;
				if (reader.has_member ("plain")) {
					is_event = reader
						.read_member ("plain")
						.read_member ("_0")
						.has_member ("event");
					reader
						.end_member ()
						.end_member ();
				}

				if (is_event) {
					events_received (reader);
				} else {
					var request = requests.poll ();
					if (request != null)
						request.resolve (reader);
				}
			} catch (Error e) {
			}
		}

		private static uint8[] key_to_der (Key key) {
			var sink = new BasicIO (BasicIOMethod.memory ());
			key.to_der (sink);
			unowned uint8[] der_data = get_basic_io_content (sink);
			uint8[] der_data_owned = der_data;
			return der_data_owned;
		}

		private static Key key_from_der (uint8[] der) throws Error {
			var source = new BasicIO.from_static_memory_buffer (der);
			Key? key = new Key.from_der (source);
			if (key == null)
				throw new Error.PROTOCOL ("Invalid key");
			return key;
		}

		private static unowned uint8[] get_basic_io_content (BasicIO bio) {
			unowned uint8[] data;
			long n = bio.get_mem_data (out data);
			data.length = (int) n;
			return data;
		}

		private static Bytes derive_shared_key (Key local_keypair, Key remote_pubkey) {
			var ctx = new KeyContext.for_key (local_keypair);
			ctx.derive_init ();
			ctx.derive_set_peer (remote_pubkey);

			size_t size = 0;
			ctx.derive (null, ref size);

			var shared_key = new uint8[size];
			ctx.derive (shared_key, ref size);

			return new Bytes.take ((owned) shared_key);
		}

		private static Bytes derive_chacha_key (Bytes shared_key, string info, string? salt = null) {
			var kdf = KeyDerivationFunction.fetch (null, KeyDerivationAlgorithm.HKDF);

			var kdf_ctx = new KeyDerivationContext (kdf);

			size_t return_size = OpenSSL.ParamReturnSize.UNMODIFIED;

			OpenSSL.Param kdf_params[] = {
				{ KeyDerivationParameter.DIGEST, UTF8_STRING, OpenSSL.ShortName.sha512.data, return_size },
				{ KeyDerivationParameter.KEY, OCTET_STRING, shared_key.get_data (), return_size },
				{ KeyDerivationParameter.INFO, OCTET_STRING, info.data, return_size },
				{ (salt != null) ? KeyDerivationParameter.SALT : null, OCTET_STRING, (salt != null) ? salt.data : null,
					return_size },
				{ null, INTEGER, null, return_size },
			};

			var derived_key = new uint8[32];
			kdf_ctx.derive (derived_key, kdf_params);

			return new Bytes.take ((owned) derived_key);
		}

		private static Bytes compute_message_signature (Bytes message, Key key) {
			var ctx = new MessageDigestContext ();
			ctx.digest_sign_init (null, null, null, key);

			unowned uint8[] data = message.get_data ();

			size_t size = 0;
			ctx.digest_sign (null, ref size, data);

			var signature = new uint8[size];
			ctx.digest_sign (signature, ref size, data);

			return new Bytes.take ((owned) signature);
		}

		private class ChaCha20Poly1305 {
			private Bytes key;

			private Cipher cipher = Cipher.fetch (null, OpenSSL.ShortName.chacha20_poly1305);
			private CipherContext? cached_ctx;

			private const size_t TAG_SIZE = 16;

			public ChaCha20Poly1305 (Bytes key) {
				this.key = key;
			}

			public Bytes encrypt (Bytes iv, Bytes message) {
				size_t cleartext_size = message.get_size ();
				var buf = new uint8[cleartext_size + TAG_SIZE];

				unowned CipherContext ctx = get_context ();
				cached_ctx.encrypt_init (cipher, key.get_data (), iv.get_data ());

				int size = buf.length;
				ctx.encrypt_update (buf, ref size, message.get_data ());

				int extra_size = buf.length - size;
				ctx.encrypt_final (buf[size:], ref extra_size);
				assert (extra_size == 0);

				ctx.ctrl (AEAD_GET_TAG, (int) TAG_SIZE, (void *) buf[size:]);

				return new Bytes.take ((owned) buf);
			}

			public Bytes decrypt (Bytes iv, Bytes message) throws Error {
				size_t message_size = message.get_size ();
				if (message_size < 1 + TAG_SIZE)
					throw new Error.PROTOCOL ("Encrypted message is too short");
				unowned uint8[] message_data = message.get_data ();

				var buf = new uint8[message_size];

				unowned CipherContext ctx = get_context ();
				cached_ctx.decrypt_init (cipher, key.get_data (), iv.get_data ());

				int size = (int) message_size;
				int res = ctx.decrypt_update (buf, ref size, message_data);
				if (res != 1)
					throw new Error.PROTOCOL ("Failed to decrypt: %d", res);

				int extra_size = buf.length - size;
				res = ctx.decrypt_final (buf[size:], ref extra_size);
				if (res != 1)
					throw new Error.PROTOCOL ("Failed to decrypt: %d", res);
				assert (extra_size == 0);

				size_t cleartext_size = message_size - TAG_SIZE;
				buf[cleartext_size] = 0;
				buf.length = (int) cleartext_size;

				return new Bytes.take ((owned) buf);
			}

			private unowned CipherContext get_context () {
				if (cached_ctx == null)
					cached_ctx = new CipherContext ();
				else
					cached_ctx.reset ();
				return cached_ctx;
			}
		}

		private class SRPClientSession {
			public Bytes public_key {
				owned get {
					var buf = new uint8[local_pubkey.num_bytes ()];
					local_pubkey.to_big_endian (buf);
					return new Bytes.take ((owned) buf);
				}
			}

			public Bytes key {
				get {
					return _key;
				}
			}

			public Bytes key_proof {
				get {
					return _key_proof;
				}
			}

			private string username;
			private string password;

			private BigNumber prime = BigNumber.get_rfc3526_prime_3072 ();
			private BigNumber generator;
			private BigNumber multiplier;

			private BigNumber local_privkey;
			private BigNumber local_pubkey;

			private BigNumber? remote_pubkey;
			private Bytes? salt;

			private BigNumber? password_hash;
			private BigNumber? password_verifier;

			private BigNumber? common_secret;
			private BigNumber? premaster_secret;
			private Bytes? _key;
			private Bytes? _key_proof;
			private Bytes? _key_proof_hash;

			private BigNumberContext bn_ctx = new BigNumberContext.secure ();

			public SRPClientSession (string username, string password) {
				this.username = username;
				this.password = password;

				uint8 raw_gen = 5;
				generator = new BigNumber.from_native ((uint8[]) &raw_gen);
				multiplier = new HashBuilder ()
					.add_number_padded (prime)
					.add_number_padded (generator)
					.build_number ();

				uint8 raw_local_privkey[128];
				Rng.generate (raw_local_privkey);
				local_privkey = new BigNumber.from_big_endian (raw_local_privkey);

				local_pubkey = new BigNumber ();
				BigNumber.mod_exp (local_pubkey, generator, local_privkey, prime, bn_ctx);
			}

			public void process (Bytes raw_remote_pubkey, Bytes salt) throws Error {
				remote_pubkey = new BigNumber.from_big_endian (raw_remote_pubkey.get_data ());
				var rem = new BigNumber ();
				BigNumber.mod (rem, remote_pubkey, prime, bn_ctx);
				if (rem.is_zero ())
					throw new Error.INVALID_ARGUMENT ("Malformed remote public key");

				this.salt = salt;

				password_hash = compute_password_hash (salt);
				password_verifier = compute_password_verifier (password_hash);

				common_secret = compute_common_secret (remote_pubkey);
				premaster_secret = compute_premaster_secret (common_secret, remote_pubkey, password_hash,
					password_verifier);
				_key = compute_session_key (premaster_secret);
				_key_proof = compute_session_key_proof (_key, remote_pubkey, salt);
				_key_proof_hash = compute_session_key_proof_hash (_key_proof, _key);
			}

			public void verify_proof (Bytes proof) throws Error {
				size_t size = proof.get_size ();
				if (size != _key_proof_hash.get_size ())
					throw new Error.INVALID_ARGUMENT ("Invalid proof size");

				if (Crypto.memcmp (proof.get_data (), _key_proof_hash.get_data (), size) != 0)
					throw new Error.INVALID_ARGUMENT ("Invalid proof");
			}

			private BigNumber compute_password_hash (Bytes salt) {
				return new HashBuilder ()
					.add_bytes (salt)
					.add_bytes (new HashBuilder ()
						.add_string (username)
						.add_string (":")
						.add_string (password)
						.build_digest ())
					.build_number ();
			}

			private BigNumber compute_password_verifier (BigNumber password_hash) {
				var verifier = new BigNumber ();
				BigNumber.mod_exp (verifier, generator, password_hash, prime, bn_ctx);
				return verifier;
			}

			private BigNumber compute_common_secret (BigNumber remote_pubkey) {
				return new HashBuilder ()
					.add_number_padded (local_pubkey)
					.add_number_padded (remote_pubkey)
					.build_number ();
			}

			private BigNumber compute_premaster_secret (BigNumber common_secret, BigNumber remote_pubkey,
					BigNumber password_hash, BigNumber password_verifier) {
				var val = new BigNumber ();

				BigNumber.mul (val, multiplier, password_verifier, bn_ctx);
				var baze = new BigNumber ();
				BigNumber.sub (baze, remote_pubkey, val);

				var exp = new BigNumber ();
				BigNumber.mul (val, common_secret, password_hash, bn_ctx);
				BigNumber.add (exp, local_privkey, val);

				BigNumber.mod_exp (val, baze, exp, prime, bn_ctx);

				return val;
			}

			private static Bytes compute_session_key (BigNumber premaster_secret) {
				return new HashBuilder ()
					.add_number (premaster_secret)
					.build_digest ();
			}

			private Bytes compute_session_key_proof (Bytes session_key, BigNumber remote_pubkey, Bytes salt) {
				Bytes prime_hash = new HashBuilder ().add_number (prime).build_digest ();
				Bytes generator_hash = new HashBuilder ().add_number (generator).build_digest ();
				uint8 prime_and_generator_xored[64];
				unowned uint8[] left = prime_hash.get_data ();
				unowned uint8[] right = generator_hash.get_data ();
				for (var i = 0; i != prime_and_generator_xored.length; i++)
					prime_and_generator_xored[i] = left[i] ^ right[i];

				return new HashBuilder ()
					.add_data (prime_and_generator_xored)
					.add_bytes (new HashBuilder ().add_string (username).build_digest ())
					.add_bytes (salt)
					.add_number (local_pubkey)
					.add_number (remote_pubkey)
					.add_bytes (session_key)
					.build_digest ();
			}

			private Bytes compute_session_key_proof_hash (Bytes key_proof, Bytes key) {
				return new HashBuilder ()
					.add_number (local_pubkey)
					.add_bytes (key_proof)
					.add_bytes (key)
					.build_digest ();
			}

			private class HashBuilder {
				private Checksum checksum = new Checksum (SHA512);

				public unowned HashBuilder add_number (BigNumber val) {
					var buf = new uint8[val.num_bytes ()];
					val.to_big_endian (buf);
					return add_data (buf);
				}

				public unowned HashBuilder add_number_padded (BigNumber val) {
					uint8 buf[384];
					val.to_big_endian_padded (buf);
					return add_data (buf);
				}

				public unowned HashBuilder add_string (string val) {
					return add_data (val.data);
				}

				public unowned HashBuilder add_bytes (Bytes val) {
					return add_data (val.get_data ());
				}

				public unowned HashBuilder add_data (uint8[] val) {
					checksum.update (val, val.length);
					return this;
				}

				public Bytes build_digest () {
					var buf = new uint8[64];
					size_t len = buf.length;
					checksum.get_digest (buf, ref len);
					return new Bytes.take ((owned) buf);
				}

				public BigNumber build_number () {
					uint8 buf[64];
					size_t len = buf.length;
					checksum.get_digest (buf, ref len);
					return new BigNumber.from_big_endian (buf);
				}
			}
		}
	}

	public interface PairingTransport : Object {
		public signal void close (Error? error);
		public signal void message (ObjectReader reader);

		public abstract async void open (Cancellable? cancellable) throws Error, IOError;
		public abstract void cancel ();

		public abstract ObjectBuilder make_object_builder ();
		public abstract void post (Bytes message);
	}

	public sealed class XpcPairingTransport : Object, PairingTransport {
		public IOStream stream {
			get;
			construct;
		}

		private XpcConnection connection;

		private Cancellable io_cancellable = new Cancellable ();

		public XpcPairingTransport (IOStream stream) {
			Object (stream: stream);
		}

		construct {
			connection = new XpcConnection (stream);
			connection.close.connect (on_close);
			connection.message.connect (on_message);
		}

		public async void open (Cancellable? cancellable) throws Error, IOError {
			connection.activate ();

			yield connection.wait_until_ready (cancellable);
		}

		public void cancel () {
			io_cancellable.cancel ();

			connection.cancel ();
		}

		public ObjectBuilder make_object_builder () {
			return new XpcObjectBuilder ();
		}

		public void post (Bytes msg) {
			connection.post.begin (
				new XpcBodyBuilder ()
					.begin_dictionary ()
						.set_member_name ("mangledTypeName")
						.add_string_value ("RemotePairing.ControlChannelMessageEnvelope")
						.set_member_name ("value")
						.add_raw_value (msg)
					.end_dictionary ()
					.build (),
				io_cancellable);
		}

		private void on_close (Error? error) {
			close (error);
		}

		private void on_message (XpcMessage msg) {
			if (msg.body == null)
				return;

			var reader = new VariantReader (msg.body);
			try {
				string type_name = reader.read_member ("mangledTypeName").get_string_value ();
				if (type_name != "RemotePairingDevice.ControlChannelMessageEnvelope")
					return;
				reader.end_member ();

				reader.read_member ("value");

				message (reader);
			} catch (Error e) {
			}
		}
	}

	public sealed class PlainPairingTransport : Object, PairingTransport {
		public IOStream stream {
			get;
			construct;
		}

		private BufferedInputStream input;
		private OutputStream output;

		private ByteArray pending_output = new ByteArray ();
		private bool writing = false;

		private Cancellable io_cancellable = new Cancellable ();

		public PlainPairingTransport (IOStream stream) {
			Object (stream: stream);
		}

		construct {
			input = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
				"base-stream", stream.get_input_stream (),
				"close-base-stream", false,
				"buffer-size", 128 * 1024);
			output = stream.get_output_stream ();
		}

		public async void open (Cancellable? cancellable) throws Error, IOError {
			process_incoming_messages.begin ();
		}

		public void cancel () {
			io_cancellable.cancel ();
		}

		public ObjectBuilder make_object_builder () {
			return new JsonObjectBuilder ();
		}

		public void post (Bytes msg) {
			Bytes raw_msg = new BufferBuilder (BIG_ENDIAN)
				.append_string ("RPPairing", StringTerminator.NONE)
				.append_uint16 ((uint16) msg.get_size ())
				.append_bytes (msg)
				.build ();
			pending_output.append (raw_msg.get_data ());

			if (!writing) {
				writing = true;

				var source = new IdleSource ();
				source.set_callback (() => {
					process_pending_output.begin ();
					return false;
				});
				source.attach (MainContext.get_thread_default ());
			}
		}

		private async void process_incoming_messages () {
			try {
				while (true) {
					size_t header_size = 11;
					if (input.get_available () < header_size)
						yield fill_until_n_bytes_available (header_size);

					uint8 raw_magic[9];
					input.peek (raw_magic);
					string magic = ((string) raw_magic).make_valid (raw_magic.length);
					if (magic != "RPPairing")
						throw new Error.PROTOCOL ("Invalid message magic: '%s'", magic);

					uint16 body_size = 0;
					unowned uint8[] size_buf = ((uint8[]) &body_size)[:2];
					input.peek (size_buf, raw_magic.length);
					body_size = uint16.from_big_endian (body_size);
					if (body_size < 2)
						throw new Error.PROTOCOL ("Invalid message size");

					size_t full_size = header_size + body_size;
					if (input.get_available () < full_size)
						yield fill_until_n_bytes_available (full_size);

					var raw_json = new uint8[body_size + 1];
					input.peek (raw_json[:body_size], header_size);

					unowned string json = (string) raw_json;
					if (!json.validate ())
						throw new Error.PROTOCOL ("Invalid UTF-8");

					var reader = new JsonObjectReader (json);

					message (reader);

					input.skip (full_size, io_cancellable);
				}
			} catch (GLib.Error e) {
			}

			close (null);
		}

		private async void process_pending_output () {
			while (pending_output.len > 0) {
				uint8[] batch = pending_output.steal ();

				size_t bytes_written;
				try {
					yield output.write_all_async (batch, Priority.DEFAULT, io_cancellable, out bytes_written);
				} catch (GLib.Error e) {
					break;
				}
			}

			writing = false;
		}

		private async void fill_until_n_bytes_available (size_t minimum) throws Error, IOError {
			size_t available = input.get_available ();
			while (available < minimum) {
				if (input.get_buffer_size () < minimum)
					input.set_buffer_size (minimum);

				ssize_t n;
				try {
					n = yield input.fill_async ((ssize_t) (input.get_buffer_size () - available), Priority.DEFAULT,
						io_cancellable);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("Connection closed");
				}

				if (n == 0)
					throw new Error.TRANSPORT ("Connection closed");

				available += n;
			}
		}
	}

	public sealed class PairingStore {
		public PairingIdentity self_identity {
			get {
				return _self_identity;
			}
		}

		public Gee.Iterable<PairingPeer> peers {
			get {
				return _peers;
			}
		}

		private PairingIdentity _self_identity;
		private Gee.List<PairingPeer> _peers;

		public PairingStore () {
			_self_identity = try_load_identity ();
			if (_self_identity == null) {
				_self_identity = PairingIdentity.make ();
				try {
					save_identity (_self_identity);
				} catch (GLib.Error e) {
				}
			}

			_peers = load_peers ();
		}

		public PairingPeer add_peer (string identifier, Bytes public_key, Bytes info) throws Error {
			var r = new VariantReader (OpackParser.parse (info));

			Bytes irk = r.read_member ("altIRK").get_data_value ();
			r.end_member ();

			unowned string name = r.read_member ("name").get_string_value ();
			r.end_member ();

			unowned string model = r.read_member ("model").get_string_value ();
			r.end_member ();

			unowned string udid = r.read_member ("remotepairing_udid").get_string_value ();
			r.end_member ();

			var peer = new PairingPeer () {
				identifier = identifier,
				public_key = public_key,
				irk = irk,
				name = name,
				model = model,
				udid = udid,
				info = info,
				remote_unlock_host_key = null,
			};
			_peers.add (peer);

			try {
				save_peer (peer);
			} catch (Error e) {
			}

			return peer;
		}

		public PairingPeer? find_peer_by_identifier (string identifier) {
			return peers.first_match (p => p.identifier == identifier);
		}

		public PairingPeer? find_peer_matching_service (PairingServiceDetails service) {
			var mac = OpenSSL.Envelope.MessageAuthCode.fetch (null, OpenSSL.ShortName.siphash);

			size_t hash_size = 8;
			size_t return_size = OpenSSL.ParamReturnSize.UNMODIFIED;
			OpenSSL.Param mac_params[] = {
				{ OpenSSL.Envelope.MessageAuthParameter.SIZE, UNSIGNED_INTEGER,
					(uint8[]) &hash_size, return_size },
				{ null, INTEGER, null, return_size },
			};

			foreach (var peer in _peers) {
				var ctx = new OpenSSL.Envelope.MessageAuthCodeContext (mac);
				ctx.init (peer.irk.get_data (), mac_params);
				ctx.update (service.identifier.data);
				uint8 output[8];
				size_t outlen = 0;
				ctx.final (output, out outlen);

				uint8 tag[6];
				for (uint i = 0; i != 6; i++)
					tag[i] = output[5 - i];

				if (Memory.cmp (tag, service.auth_tag.get_data (), service.auth_tag.get_size ()) == 0)
					return peer;
			}

			return null;
		}

		private static PairingIdentity? try_load_identity () {
			try {
				var plist = new Plist.from_data (query_self_identity_location ().load_bytes ().get_data ());
				return new PairingIdentity () {
					identifier = plist.get_string ("identifier"),
					key = new Key.from_raw_private_key (ED25519, null, plist.get_bytes ("privateKey").get_data ()),
					irk = plist.get_bytes ("irk"),
				};
			} catch (GLib.Error e) {
				return null;
			}
		}

		private static void save_identity (PairingIdentity identity) throws Error {
			var plist = new Plist ();
			plist.set_string ("identifier", identity.identifier);
			plist.set_bytes ("publicKey", get_raw_public_key (identity.key));
			plist.set_bytes ("privateKey", get_raw_private_key (identity.key));
			plist.set_bytes ("irk", identity.irk);
			save_plist (plist, query_self_identity_location ());
		}

		private static Gee.List<PairingPeer> load_peers () {
			var peers = new Gee.ArrayList<PairingPeer> ();

			try {
				var enumerator = query_peers_location ().enumerate_children (FileAttribute.STANDARD_NAME, FileQueryInfoFlags.NONE);
				File? child;
				while (enumerator.iterate (null, out child) && child != null) {
					var plist = new Plist.from_data (child.load_bytes ().get_data ());

					var info = plist.get_bytes ("info");
					var r = new VariantReader (OpackParser.parse (info));
					unowned string udid = r.read_member ("remotepairing_udid").get_string_value ();

					Bytes? remote_unlock_host_key = null;
					if (plist.has ("remoteUnlockHostKey"))
						remote_unlock_host_key = plist.get_bytes ("remoteUnlockHostKey");

					peers.add (new PairingPeer () {
						identifier = plist.get_string ("identifier"),
						public_key = plist.get_bytes ("publicKey"),
						irk = plist.get_bytes ("irk"),
						name = plist.get_string ("name"),
						model = plist.get_string ("model"),
						udid = udid,
						info = info,
						remote_unlock_host_key = remote_unlock_host_key,
					});
				}
			} catch (GLib.Error e) {
			}

			return peers;
		}

		public void save_peer (PairingPeer peer) throws Error {
			var plist = new Plist ();
			plist.set_string ("identifier", peer.identifier);
			plist.set_bytes ("publicKey", peer.public_key);
			plist.set_bytes ("irk", peer.irk);
			plist.set_string ("name", peer.name);
			plist.set_string ("model", peer.model);
			plist.set_bytes ("info", peer.info);
			if (peer.remote_unlock_host_key != null)
				plist.set_bytes ("remoteUnlockHostKey", peer.remote_unlock_host_key);

			save_plist (plist, query_peer_location (peer));
		}

		public void forget_peer (PairingPeer peer) {
			try {
				query_peer_location (peer).delete ();
			} catch (GLib.Error e) {
			}

			_peers.remove (peer);
		}

		private static File query_self_identity_location () {
			return query_base_location ().get_child ("self-identity.plist");
		}

		private static File query_peer_location (PairingPeer peer) {
			return query_peers_location ().get_child (peer.identifier + ".plist");
		}

		private static File query_peers_location () {
			return query_base_location ().get_child ("peers");
		}

		private static File query_base_location () {
			return File.new_build_filename (Environment.get_user_config_dir (), "frida");
		}

		private static void save_plist (Plist plist, File location) throws Error {
			try {
				location.get_parent ().make_directory_with_parents ();
			} catch (GLib.Error e) {
			}
			try {
				location.replace_contents (plist.to_binary (), null, false, PRIVATE | REPLACE_DESTINATION, null);
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
		}
	}

	public sealed class PairingIdentity {
		public string identifier;
		public Key key;
		public Bytes irk;

		public static PairingIdentity make () {
			uint8 raw_irk[16];
			Rng.generate (raw_irk);

			return new PairingIdentity () {
				identifier = make_host_identifier (),
				key = make_keypair (ED25519),
				irk = new Bytes (raw_irk),
			};
		}
	}

	public sealed class PairingPeer {
		public string identifier;
		public Bytes public_key;
		public Bytes irk;
		public string name;
		public string model;
		public string udid;
		public Bytes info;
		public Bytes? remote_unlock_host_key;
	}

	public sealed class PairingServiceMetadata {
		public string identifier;
		public Bytes auth_tag;

		public static PairingServiceMetadata from_txt_record (Gee.Iterable<string> record) throws Error {
			string? identifier = null;
			Bytes? auth_tag = null;
			foreach (string item in record) {
				string[] tokens = item.split ("=", 2);
				if (tokens.length != 2)
					continue;

				unowned string key = tokens[0];
				unowned string val = tokens[1];
				if (key == "identifier")
					identifier = val;
				else if (key == "authTag")
					auth_tag = new Bytes (Base64.decode (val));
			}
			if (identifier == null || auth_tag == null)
				throw new Error.PROTOCOL ("Missing TXT metadata");

			return new PairingServiceMetadata () {
				identifier = identifier,
				auth_tag = auth_tag,
			};
		}
	}

	public sealed class DeviceOptions {
		public bool allows_pair_setup;
		public bool allows_pinless_pairing;
		public bool allows_promptless_automation_pairing_upgrade;
		public bool allows_sharing_sensitive_info;
		public bool allows_incoming_tunnel_connections;
	}

	public sealed class DeviceInfo {
		public string name;
		public string model;
		public string udid;
		public uint64 ecid;
		public Plist kvs;
	}

	private sealed class PairingParamsBuilder {
		private BufferBuilder builder = new BufferBuilder (LITTLE_ENDIAN);

		public unowned PairingParamsBuilder add_method (uint8 method) {
			begin_param (METHOD, 1)
				.append_uint8 (method);

			return this;
		}

		public unowned PairingParamsBuilder add_identifier (string identifier) {
			begin_param (IDENTIFIER, identifier.data.length)
				.append_data (identifier.data);

			return this;
		}

		public unowned PairingParamsBuilder add_public_key (Key key) {
			return add_raw_public_key (get_raw_public_key (key));
		}

		public unowned PairingParamsBuilder add_raw_public_key (Bytes key) {
			return add_blob (PUBLIC_KEY, key);
		}

		public unowned PairingParamsBuilder add_proof (Bytes proof) {
			return add_blob (PROOF, proof);
		}

		public unowned PairingParamsBuilder add_encrypted_data (Bytes bytes) {
			return add_blob (ENCRYPTED_DATA, bytes);
		}

		public unowned PairingParamsBuilder add_state (uint8 state) {
			begin_param (STATE, 1)
				.append_uint8 (state);

			return this;
		}

		public unowned PairingParamsBuilder add_signature (Bytes signature) {
			return add_blob (SIGNATURE, signature);
		}

		public unowned PairingParamsBuilder add_info (Bytes info) {
			return add_blob (INFO, info);
		}

		private unowned PairingParamsBuilder add_blob (PairingParamType type, Bytes blob) {
			unowned uint8[] data = blob.get_data ();

			uint cursor = 0;
			do {
				uint n = uint.min (data.length - cursor, uint8.MAX);
				begin_param (type, n)
					.append_data (data[cursor:cursor + n]);
				cursor += n;
			} while (cursor != data.length);

			return this;
		}

		private unowned BufferBuilder begin_param (PairingParamType type, size_t size) {
			return builder
				.append_uint8 (type)
				.append_uint8 ((uint8) size);
		}

		public Bytes build () {
			return builder.build ();
		}
	}

	private sealed class PairingParamsParser {
		private BufferReader reader;
		private EnumClass param_type_class;

		public static Variant parse (Bytes pairing_params) throws Error {
			var parser = new PairingParamsParser (pairing_params);
			return parser.read_params ();
		}

		private PairingParamsParser (Bytes bytes) {
			reader = new BufferReader (new Buffer (bytes, LITTLE_ENDIAN));
			param_type_class = (EnumClass) typeof (PairingParamType).class_ref ();
		}

		private Variant read_params () throws Error {
			var byte_array = new VariantType.array (VariantType.BYTE);

			var parameters = new Gee.HashMap<string, Variant> ();
			while (reader.available != 0) {
				var raw_type = reader.read_uint8 ();
				unowned EnumValue? type_enum_val = param_type_class.get_value (raw_type);
				if (type_enum_val == null)
					throw new Error.INVALID_ARGUMENT ("Unsupported pairing parameter type (0x%x)", raw_type);
				var type = (PairingParamType) raw_type;
				unowned string key = type_enum_val.value_nick;

				var val_size = reader.read_uint8 ();
				Variant val;
				switch (type) {
					case IDENTIFIER:
						val = new Variant.string (reader.read_fixed_string (val_size));
						break;
					case STATE:
					case ERROR:
						if (val_size != 1)
							throw new Error.INVALID_ARGUMENT ("Invalid value for '%s': size=%u", key, val_size);
						val = new Variant.byte (reader.read_uint8 ());
						break;
					case RETRY_DELAY: {
						uint16 delay;
						switch (val_size) {
							case 1:
								delay = reader.read_uint8 ();
								break;
							case 2:
								delay = reader.read_uint16 ();
								break;
							default:
								throw new Error.INVALID_ARGUMENT ("Invalid value for 'retry-delay'");
						}
						val = new Variant.uint16 (delay);
						break;
					}
					default: {
						Bytes val_bytes = reader.read_bytes (val_size);
						var val_bytes_copy = new Bytes (val_bytes.get_data ());
						val = Variant.new_from_data (byte_array, val_bytes_copy.get_data (), true, val_bytes_copy);
						break;
					}
				}

				Variant? existing_val = parameters[key];
				if (existing_val != null) {
					if (!existing_val.is_of_type (byte_array))
						throw new Error.INVALID_ARGUMENT ("Unable to merge '%s' keys: unsupported type", key);
					Bytes part1 = existing_val.get_data_as_bytes ();
					Bytes part2 = val.get_data_as_bytes ();
					var combined = new ByteArray.sized ((uint) (part1.get_size () + part2.get_size ()));
					combined.append (part1.get_data ());
					combined.append (part2.get_data ());
					val = Variant.new_from_data (byte_array, combined.data, true, (owned) combined);
				}

				parameters[key] = val;
			}

			var builder = new VariantBuilder (VariantType.VARDICT);
			foreach (var e in parameters.entries)
				builder.add ("{sv}", e.key, e.value);
			return builder.end ();
		}
	}

	private enum PairingParamType {
		METHOD,
		IDENTIFIER,
		SALT,
		PUBLIC_KEY,
		PROOF,
		ENCRYPTED_DATA,
		STATE,
		ERROR,
		RETRY_DELAY /* = 8 */,
		SIGNATURE = 10,
		INFO = 17,
	}

	private OpenSSL.Envelope.Key make_keypair (OpenSSL.Envelope.KeyType type) {
		var ctx = new OpenSSL.Envelope.KeyContext.for_key_type (type);
		ctx.keygen_init ();

		OpenSSL.Envelope.Key? keypair = null;
		ctx.keygen (ref keypair);

		return keypair;
	}

	private Bytes get_raw_public_key (OpenSSL.Envelope.Key key) {
		size_t size = 0;
		key.get_raw_public_key (null, ref size);

		var result = new uint8[size];
		key.get_raw_public_key (result, ref size);

		return new Bytes.take ((owned) result);
	}

	private Bytes get_raw_private_key (OpenSSL.Envelope.Key key) {
		size_t size = 0;
		key.get_raw_private_key (null, ref size);

		var result = new uint8[size];
		key.get_raw_private_key (result, ref size);

		return new Bytes.take ((owned) result);
	}
}
