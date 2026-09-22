[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class InstallationProxyClient : Object, AsyncInitable {
		public Device device {
			get;
			construct;
		}

		private PlistServiceClient service;

		private InstallationProxyClient (Device device) {
			Object (device: device);
		}

		public static async InstallationProxyClient open (Device device, Cancellable? cancellable = null) throws Error, IOError {
			var client = new InstallationProxyClient (device);

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return client;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var stream = yield device.open_lockdown_service ("com.apple.mobile.installation_proxy", cancellable);

			service = new PlistServiceClient (stream);

			return true;
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			yield service.close (cancellable);
		}

		public async Gee.ArrayList<ApplicationDetails> browse (Cancellable? cancellable = null) throws Error, IOError {
			try {
				var result = new Gee.ArrayList<ApplicationDetails> ();

				var request = make_request ("Browse");

				request.set_dict ("ClientOptions", make_client_options ());

				service.write_message (request);
				string status = "";
				do {
					var response = yield service.read_message (cancellable);

					status = response.get_string ("Status");
					if (status == "BrowsingApplications") {
						var entries = response.get_array ("CurrentList");
						var length = entries.length;
						for (int i = 0; i != length; i++) {
							PlistDict app = entries.get_dict (i);
							if (is_springboard_visible_app (app))
								result.add (parse_application_details (app));
						}
					}
				} while (status != "Complete");

				return result;
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} catch (PlistError e) {
				throw error_from_plist (e);
			}
		}

		public async Gee.HashMap<string, ApplicationDetails> lookup (PlistDict query, Cancellable? cancellable = null)
				throws Error, IOError {
			try {
				var result = new Gee.HashMap<string, ApplicationDetails> ();

				var request = make_request ("Lookup");

				var options = make_client_options ();
				request.set_dict ("ClientOptions", options);
				foreach (var key in query.keys) {
					var val = query.get_value (key);
					Value? val_copy = Value (val.type ());
					val.copy (ref val_copy);
					options.set_value (key, (owned) val_copy);
				}

				service.write_message (request);
				string status = "";
				do {
					var response = yield service.read_message (cancellable);

					var result_dict = response.get_dict ("LookupResult");
					foreach (var identifier in result_dict.keys)
						result[identifier] = parse_application_details (result_dict.get_dict (identifier));

					status = response.get_string ("Status");
				} while (status != "Complete");

				return result;
			} catch (PlistServiceError e) {
				throw error_from_service (e);
			} catch (PlistError e) {
				throw error_from_plist (e);
			}
		}

		private static Plist make_request (string command) {
			var request = new Plist ();
			request.set_string ("Command", command);
			return request;
		}

		private static PlistDict make_client_options () {
			var options = new PlistDict ();

			var attributes = new PlistArray ();
			options.set_array ("ReturnAttributes", attributes);
			attributes.add_string ("ApplicationType");
			attributes.add_string ("IsAppClip");
			attributes.add_string ("SBAppTags");
			attributes.add_string ("CFBundleIdentifier");
			attributes.add_string ("CFBundleDisplayName");
			attributes.add_string ("CFBundleShortVersionString");
			attributes.add_string ("CFBundleVersion");
			attributes.add_string ("Path");
			attributes.add_string ("Container");
			attributes.add_string ("GroupContainers");
			attributes.add_string ("Entitlements");

			return options;
		}

		private static bool is_springboard_visible_app (PlistDict details) {
			try {
				unowned string application_type = details.get_string ("ApplicationType");
				if (application_type == "Hidden")
					return false;

				if (details.has ("IsAppClip") && details.get_boolean ("IsAppClip"))
					return false;

				if (details.has ("SBAppTags")) {
					PlistArray tags = details.get_array ("SBAppTags");
					int n = tags.length;
					for (int i = 0; i != n; i++) {
						unowned string tag = tags.get_string (i);
						if (tag == "hidden" || tag == "SBInternalAppTag" || tag == "watch-companion")
							return false;
					}
				}

				return true;
			} catch (PlistError e) {
				assert_not_reached ();
			}
		}

		private static ApplicationDetails parse_application_details (PlistDict details) throws PlistError {
			unowned string identifier = details.get_string ("CFBundleIdentifier");
			unowned string name = details.get_string ("CFBundleDisplayName");
			unowned string? version = details.has ("CFBundleShortVersionString") ? details.get_string ("CFBundleShortVersionString") : null;
			unowned string? build = details.has ("CFBundleVersion") ? details.get_string ("CFBundleVersion") : null;
			unowned string path = details.get_string ("Path");

			var containers = new Gee.HashMap<string, string> ();
			if (details.has ("Container"))
				containers["data"] = details.get_string ("Container");
			if (details.has ("GroupContainers")) {
				foreach (var entry in details.get_dict ("GroupContainers").entries) {
					unowned string group = entry.key;
					Value * value = entry.value;
					if (value->holds (typeof (string)))
						containers[group] = (string) *value;
				}
			}

			bool debuggable = false;
			if (details.has ("Entitlements")) {
				var entitlements = details.get_dict ("Entitlements");
				debuggable = entitlements.has ("get-task-allow") && entitlements.get_boolean ("get-task-allow");
			}

			return new ApplicationDetails (identifier, name, version, build, path, containers, debuggable);
		}

		private static Error error_from_service (PlistServiceError e) {
			return new Error.PROTOCOL ("%s", e.message);
		}

		private static Error error_from_plist (PlistError e) {
			return new Error.PROTOCOL ("Unexpected response: %s", e.message);
		}
	}

	public sealed class ApplicationDetails : Object {
		public string identifier {
			get;
			construct;
		}

		public string name {
			get;
			construct;
		}

		public string? version {
			get;
			construct;
		}

		public string? build {
			get;
			construct;
		}

		public string path {
			get;
			construct;
		}

		public Gee.Map<string, string> containers {
			get;
			construct;
		}

		public bool debuggable {
			get;
			construct;
		}

		public ApplicationDetails (string identifier, string name, string? version, string? build, string path,
				Gee.Map<string, string> containers, bool debuggable) {
			Object (
				identifier: identifier,
				name: name,
				version: version,
				build: build,
				path: path,
				containers: containers,
				debuggable: debuggable
			);
		}
	}
}
