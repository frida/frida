[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class LockdownClient : Object {
		public signal void closed ();

		public PlistServiceClient service {
			get;
			construct;
		}

		private UsbmuxDevice? usbmux_device;
		private Plist? pair_record;
		private string? host_id;
		private string? system_buid;
		private TlsCertificate? tls_certificate;

		private Promise<bool>? pending_service_query;

		private const uint16 LOCKDOWN_PORT = 62078;

		public LockdownClient (IOStream stream) {
			Object (service: new PlistServiceClient (stream));
		}

		construct {
			service.closed.connect (on_service_closed);
		}

		public static async LockdownClient open (UsbmuxDevice device, Cancellable? cancellable = null)
				throws LockdownError, IOError {
			try {
				var usbmux = yield UsbmuxClient.open (cancellable);

				Plist pair_record;
				try {
					pair_record = yield usbmux.read_pair_record (device.udid, cancellable);
				} catch (UsbmuxError e) {
					if (e is UsbmuxError.INVALID_ARGUMENT)
						throw new LockdownError.NOT_PAIRED ("Not paired");
					throw e;
				}

				string? host_id = null;
				string? system_buid = null;
				TlsCertificate? tls_certificate = null;
				try {
					host_id = pair_record.get_string ("HostID");
					system_buid = pair_record.get_string ("SystemBUID");

					var cert = pair_record.get_bytes_as_string ("HostCertificate");
					var key = pair_record.get_bytes_as_string ("HostPrivateKey");
					tls_certificate = new TlsCertificate.from_pem (string.join ("\n", cert, key), -1);
				} catch (GLib.Error e) {
				}

				yield usbmux.connect_to_port (device.id, LOCKDOWN_PORT, cancellable);

				var client = new LockdownClient (usbmux.connection);
				client.usbmux_device = device;
				client.pair_record = pair_record;
				client.host_id = host_id;
				client.system_buid = system_buid;
				client.tls_certificate = tls_certificate;

				yield client.query_type (cancellable);

				return client;
			} catch (UsbmuxError e) {
				throw new LockdownError.UNSUPPORTED ("%s", e.message);
			}
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			yield service.close (cancellable);
		}

		private void on_service_closed () {
			closed ();
		}

		public async void start_session (Cancellable? cancellable) throws LockdownError, IOError {
			if (tls_certificate == null)
				throw new LockdownError.UNSUPPORTED ("Incomplete pair record");

			try {
				var request = create_request ("StartSession");
				request.set_string ("HostID", host_id);
				request.set_string ("SystemBUID", system_buid);

				var response = yield service.query (request, cancellable);
				if (response.has ("Error")) {
					var error = response.get_string ("Error");
					if (error == "InvalidHostID")
						yield unpair (cancellable);
					throw new LockdownError.PROTOCOL ("Unexpected response: %s", error);
				}

				if (response.get_boolean ("EnableSessionSSL"))
					service.stream = yield start_tls (service.stream, cancellable);
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} catch (PlistError e) {
				throw error_from_plist (e);
			}
		}

		private async TlsConnection start_tls (IOStream stream, Cancellable? cancellable) throws LockdownError, IOError {
			try {
				var server_identity = new NetworkAddress ("apple.com", 62078);
				var connection = TlsClientConnection.new (stream, server_identity);
				connection.set_database (null);
				connection.accept_certificate.connect (on_accept_certificate);

				connection.set_certificate (tls_certificate);

				yield connection.handshake_async (Priority.DEFAULT, cancellable);

				return connection;
			} catch (GLib.Error e) {
				throw new LockdownError.PROTOCOL ("%s", e.message);
			}
		}

		private bool on_accept_certificate (TlsCertificate peer_cert, TlsCertificateFlags errors) {
			return true;
		}

		public async Plist get_value (string? domain, string? key, Cancellable? cancellable = null) throws LockdownError, IOError {
			try {
				var request = create_request ("GetValue");
				if (domain != null)
					request.set_string ("Domain", domain);
				if (key != null)
					request.set_string ("Key", key);

				return yield service.query (request, cancellable);
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			}
		}

		public async IOStream start_service (string name_with_options, Cancellable? cancellable = null) throws LockdownError, IOError {
			var tokens = name_with_options.split ("?", 2);
			unowned string name = tokens[0];
			bool tls_handshake_only = false;
			if (tokens.length > 1) {
				unowned string options = tokens[1];
				tls_handshake_only = options == "tls=handshake-only";
			}

			Plist request = create_request ("StartService");
			request.set_string ("Service", name);

			Plist? response = null;
			while (pending_service_query != null) {
				var future = pending_service_query.future;
				try {
					yield future.wait_async (cancellable);
				} catch (GLib.Error e) {
				}
				cancellable.set_error_if_cancelled ();
			}
			pending_service_query = new Promise<bool> ();
			try {
				response = yield service.query (request, cancellable);
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} finally {
				pending_service_query = null;
			}

			try {
				if (response.has ("Error")) {
					var error = response.get_string ("Error");
					if (error == "InvalidService")
						throw new LockdownError.INVALID_SERVICE ("Service '%s' not found", name);
					else
						throw new LockdownError.PROTOCOL ("Unexpected response: %s", error);
				}

				bool enable_encryption = response.has ("EnableServiceSSL") && response.get_boolean ("EnableServiceSSL");

				var client = yield UsbmuxClient.open (cancellable);
				yield client.connect_to_port (usbmux_device.id, (uint16) response.get_integer ("Port"), cancellable);

				SocketConnection raw_connection = client.connection;
				IOStream stream = raw_connection;

				if (enable_encryption) {
					var tls_connection = yield start_tls (raw_connection, cancellable);

					if (tls_handshake_only) {
						/*
						 * In this case we assume that communication should be cleartext after the handshake.
						 *
						 * Also, because TlsConnection closes its base stream once destroyed, and because it holds a strong
						 * ref on the base stream, we cannot return the base stream here and still keep the TlsConnection
						 * instance alive. And attaching it as data to the base stream would create a reference loop.
						 *
						 * So instead we get the underlying Socket and create a new SocketConnection for the Socket, where
						 * we keep the TlsConnection and its base stream alive by attaching it as data.
						 */
						stream = Object.new (typeof (SocketConnection), "socket", raw_connection.socket) as IOStream;
						stream.set_data ("tls-connection", tls_connection);
					} else {
						stream = tls_connection;
					}
				}

				return stream;
			} catch (PlistError e) {
				throw error_from_plist (e);
			} catch (UsbmuxError e) {
				throw new LockdownError.UNSUPPORTED ("%s", e.message);
			}
		}

		public async void unpair (Cancellable? cancellable = null) throws LockdownError, IOError {
			var request = create_request ("Unpair");

			var record = pair_record.clone ();
			record.remove ("RootPrivateKey");
			record.remove ("HostPrivateKey");
			request.set_dict ("PairRecord", record);

			Plist response;
			try {
				response = yield service.query (request, cancellable);
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			}
			if (response.has ("Error")) {
				try {
					var error = response.get_string ("Error");
					if (error != "InvalidHostID")
						throw new LockdownError.PROTOCOL ("Unexpected response: %s", error);
				} catch (Fruity.PlistError e) {
					throw new LockdownError.PROTOCOL ("%s", e.message);
				}
			}

			try {
				var usbmux = yield UsbmuxClient.open (cancellable);
				yield usbmux.delete_pair_record (usbmux_device.udid, cancellable);
			} catch (UsbmuxError e) {
				if (!(e is UsbmuxError.INVALID_ARGUMENT))
					throw new LockdownError.PROTOCOL ("%s", e.message);
			}
		}

		private async string query_type (Cancellable? cancellable) throws LockdownError, IOError {
			try {
				var response = yield service.query (create_request ("QueryType"), cancellable);

				return response.get_string ("Type");
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} catch (PlistError e) {
				throw error_from_plist (e);
			}
		}

		private static Plist create_request (string request_type) {
			var request = new Plist ();
			request.set_string ("Request", request_type);
			request.set_string ("Label", "Xcode");
			request.set_string ("ProtocolVersion", "2");
			return request;
		}

		private static LockdownError error_from_service (PlistServiceError e) {
			if (e is PlistServiceError.CONNECTION_CLOSED)
				return new LockdownError.CONNECTION_CLOSED ("%s", e.message);
			return new LockdownError.PROTOCOL ("%s", e.message);
		}

		private static LockdownError error_from_plist (PlistError e) {
			return new LockdownError.PROTOCOL ("Unexpected response: %s", e.message);
		}
	}

	public errordomain LockdownError {
		CONNECTION_CLOSED,
		INVALID_SERVICE,
		NOT_PAIRED,
		UNSUPPORTED,
		PROTOCOL
	}
}
