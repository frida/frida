[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class SpringboardServicesClient : Object, AsyncInitable {
		public Device device {
			get;
			construct;
		}

		private PlistServiceClient service;

		private SpringboardServicesClient (Device device) {
			Object (device: device);
		}

		public static async SpringboardServicesClient open (Device device, Cancellable? cancellable = null) throws Error, IOError {
			var client = new SpringboardServicesClient (device);

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return client;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var stream = yield device.open_lockdown_service ("com.apple.springboardservices", cancellable);

			service = new PlistServiceClient (stream);

			return true;
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			yield service.close (cancellable);
		}

		public async Bytes get_icon_png_data (string bundle_id, Cancellable? cancellable = null) throws Error, IOError {
			try {
				var request = make_request ("getIconPNGData");
				request.set_string ("bundleId", bundle_id);

				var response = yield service.query (request, cancellable);
				if (response.has ("Error"))
					throw new Error.INVALID_ARGUMENT ("%s", response.get_string ("Error"));

				return response.get_bytes ("pngData");
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} catch (PlistError e) {
				throw error_from_plist (e);
			}
		}

		public async Gee.HashMap<string, Bytes> get_icon_png_data_batch (string[] bundle_ids, Cancellable? cancellable = null)
				throws Error, IOError {
			try {
				foreach (unowned string bundle_id in bundle_ids) {
					var request = make_request ("getIconPNGData");
					request.set_string ("bundleId", bundle_id);
					service.write_message (request);
				}

				var result = new Gee.HashMap<string, Bytes> ();
				uint offset = 0;
				do {
					foreach (Plist response in yield service.read_messages (0, cancellable)) {
						if (response.has ("Error")) {
							throw new Error.INVALID_ARGUMENT ("%s",
								response.get_string ("Error"));
						}

						result[bundle_ids[offset]] = response.get_bytes ("pngData");

						offset++;
						if (offset == bundle_ids.length)
							break;
					}
				} while (offset != bundle_ids.length);
				return result;
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} catch (PlistError e) {
				throw error_from_plist (e);
			}
		}

		private static Plist make_request (string command) {
			var request = new Plist ();
			request.set_string ("command", command);
			return request;
		}

		private static Error error_from_service (PlistServiceError e) {
			return new Error.PROTOCOL ("%s", e.message);
		}

		private static Error error_from_plist (PlistError e) {
			return new Error.PROTOCOL ("Unexpected response: %s", e.message);
		}
	}
}
