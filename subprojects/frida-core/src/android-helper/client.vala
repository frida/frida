namespace Frida {
	public sealed class AndroidHelperClient : Object {
		public AndroidHelperTransport transport {
			get;
			construct;
		}

		public AndroidHelperClient (AndroidHelperTransport transport) {
			Object (transport: transport);
		}

		public async HostApplicationInfo get_frontmost_application (FrontmostQueryOptions options,
				Cancellable? cancellable) throws Error, IOError {
			var stanza = new Json.Builder ();
			stanza
				.begin_array ()
				.add_string_value ("get-frontmost-application")
				.add_string_value (options.scope.to_nick ())
				.end_array ();

			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			if (response.is_null ())
				return HostApplicationInfo.empty ();

			Json.Reader reader = make_json_reader_taking_node ((owned) response);

			string? identifier = null;
			string? name = null;
			uint pid = 0;
			HashTable<string, Variant> parameters = make_parameters_dict ();

			if (reader.read_element (0)) {
				identifier = reader.get_string_value ();
				reader.end_element ();
			}

			if (reader.read_element (1)) {
				name = reader.get_string_value ();
				reader.end_element ();
			}

			if (reader.read_element (2)) {
				pid = (uint) reader.get_int_value ();
				reader.end_element ();
			}

			if (reader.read_element (3)) {
				if (reader.is_object ())
					add_parameters_from_json (parameters, reader);
				reader.end_element ();
			}

			GLib.Error? error = reader.get_error ();
			if (error != null)
				throw new Error.PROTOCOL ("%s", error.message);

			return HostApplicationInfo (identifier, name, pid, (owned) parameters);
		}

		public async HostApplicationInfo[] enumerate_applications (ApplicationQueryOptions options, Cancellable? cancellable)
				throws Error, IOError {
			var stanza = new Json.Builder ();
			stanza
				.begin_array ()
				.add_string_value ("enumerate-applications");

			stanza.begin_array ();
			options.enumerate_selected_identifiers (identifier => stanza.add_string_value (identifier));
			stanza.end_array ();

			stanza
				.add_string_value (options.scope.to_nick ())
				.end_array ();

			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			Json.Reader reader = make_json_reader_taking_node ((owned) response);

			int num_apps = reader.count_elements ();
			if (num_apps == -1)
				throw new Error.PROTOCOL ("Invalid response from helper service");

			var result = new HostApplicationInfo[0];

			for (int i = 0; i != num_apps; i++) {
				reader.read_element (i);

				string? identifier = null;
				string? name = null;
				uint pid = 0;
				HashTable<string, Variant> parameters = make_parameters_dict ();

				if (reader.read_element (0)) {
					identifier = reader.get_string_value ();
					reader.end_element ();
				}

				if (reader.read_element (1)) {
					name = reader.get_string_value ();
					reader.end_element ();
				}

				if (reader.read_element (2)) {
					pid = (uint) reader.get_int_value ();
					reader.end_element ();
				}

				if (reader.read_element (3)) {
					if (reader.is_object ())
						add_parameters_from_json (parameters, reader);
					reader.end_element ();
				}

				GLib.Error? error = reader.get_error ();
				if (error != null)
					throw new Error.PROTOCOL ("%s", error.message);

				result += HostApplicationInfo (identifier, name, pid, (owned) parameters);

				reader.end_element ();
			}

			return result;
		}

		public async HostProcessInfo[] enumerate_processes (ProcessQueryOptions options, Cancellable? cancellable)
				throws Error, IOError {
			var stanza = new Json.Builder ();
			stanza
				.begin_array ()
				.add_string_value ("enumerate-processes");

			stanza.begin_array ();
			options.enumerate_selected_pids (pid => stanza.add_int_value (pid));
			stanza.end_array ();

			stanza
				.add_string_value (options.scope.to_nick ())
				.end_array ();

			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			Json.Reader reader = make_json_reader_taking_node ((owned) response);

			int num_processes = reader.count_elements ();
			if (num_processes == -1)
				throw new Error.PROTOCOL ("Invalid response from helper service");

			var result = new HostProcessInfo[0];

			for (int i = 0; i != num_processes; i++) {
				reader.read_element (i);

				uint pid = 0;
				string? name = null;
				HashTable<string, Variant> parameters = make_parameters_dict ();

				if (reader.read_element (0)) {
					pid = (uint) reader.get_int_value ();
					reader.end_element ();
				}

				if (reader.read_element (1)) {
					name = reader.get_string_value ();
					reader.end_element ();
				}

				if (reader.read_element (2)) {
					if (reader.is_object ())
						add_parameters_from_json (parameters, reader);
					reader.end_element ();
				}

				GLib.Error? error = reader.get_error ();
				if (error != null)
					throw new Error.PROTOCOL ("%s", error.message);

				result += HostProcessInfo (pid, name, (owned) parameters);

				reader.end_element ();
			}

			return result;
		}

		public async string get_process_name (string package, int uid, Cancellable? cancellable) throws Error, IOError {
			var stanza = new Json.Builder ();
			stanza
				.begin_array ()
				.add_string_value ("get-process-name")
				.add_string_value (package)
				.add_int_value (uid)
				.end_array ();

			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			var r = make_json_reader_taking_node ((owned) response);
			parse_envelope_or_throw (r);

			r.read_element (1);
			string? process_name = r.get_string_value ();
			if (process_name == null)
				throw new Error.PROTOCOL ("Malformed response");
			return process_name;
		}

		public async void start_package (string package, PackageEntrypoint entrypoint, Cancellable? cancellable)
				throws Error, IOError {
			if (entrypoint is DefaultActivityEntrypoint) {
				var stanza = new Json.Builder ();
				stanza
					.begin_array ()
					.add_string_value ("start-activity")
					.add_string_value (package)
					.add_null_value ()
					.add_int_value (entrypoint.uid)
					.end_array ();
				yield request_ok (stanza, cancellable);
				return;
			}

			if (entrypoint is ActivityEntrypoint) {
				var e = (ActivityEntrypoint) entrypoint;

				var stanza = new Json.Builder ();
				stanza
					.begin_array ()
					.add_string_value ("start-activity")
					.add_string_value (package)
					.add_string_value (e.activity)
					.add_int_value (entrypoint.uid)
					.end_array ();
				yield request_ok (stanza, cancellable);
				return;
			}

			if (entrypoint is BroadcastReceiverEntrypoint) {
				var e = (BroadcastReceiverEntrypoint) entrypoint;

				var stanza = new Json.Builder ();
				stanza
					.begin_array ()
					.add_string_value ("send-broadcast")
					.add_string_value (package)
					.add_string_value (e.receiver)
					.add_string_value (e.action)
					.add_int_value (entrypoint.uid)
					.end_array ();
				yield request_ok (stanza, cancellable);
				return;
			}

			assert_not_reached ();
		}

		public async void stop_package (string package, int uid, Cancellable? cancellable) throws Error, IOError {
			var stanza = new Json.Builder ();
			stanza
				.begin_array ()
				.add_string_value ("stop-package")
				.add_string_value (package)
				.add_int_value (uid)
				.end_array ();

			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			var r = make_json_reader_taking_node ((owned) response);
			parse_envelope_or_throw (r);
		}

		public async bool try_stop_package_by_pid (uint pid, Cancellable? cancellable) throws Error, IOError {
			var stanza = new Json.Builder ();
			stanza
				.begin_array ()
				.add_string_value ("try-stop-package-by-pid")
				.add_int_value ((int) pid)
				.end_array ();

			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			var r = make_json_reader_taking_node ((owned) response);
			parse_envelope_or_throw (r);

			r.read_element (1);
			var val = r.get_value ();
			if (val == null || val.get_value_type () != typeof (bool))
				throw new Error.PROTOCOL ("Malformed response");
			return r.get_boolean_value ();
		}

		private async void request_ok (Json.Builder stanza, Cancellable? cancellable) throws Error, IOError {
			Json.Node response = yield transport.request (stanza.get_root (), cancellable);

			var r = make_json_reader_taking_node ((owned) response);
			parse_envelope_or_throw (r);
		}

		private void parse_envelope_or_throw (Json.Reader r) throws Error {
			r.read_element (0);
			string? kind = r.get_string_value ();
			if (kind == null)
				throw new Error.PROTOCOL ("Malformed response");
			r.end_element ();

			if (kind == "ok")
				return;

			if (kind == "error") {
				r.read_element (1);
				string? code = r.get_string_value ();
				if (code == null)
					throw new Error.PROTOCOL ("Malformed response");
				r.end_element ();

				r.read_element (2);
				string? message = r.get_string_value ();
				if (message == null)
					throw new Error.PROTOCOL ("Malformed response");
				r.end_element ();

				if (code == "INVALID_ARGUMENT")
					throw new Error.INVALID_ARGUMENT ("%s", message);

				throw new Error.NOT_SUPPORTED ("%s", message);
			}

			throw new Error.PROTOCOL ("Malformed response");
		}

		private static void add_parameters_from_json (HashTable<string, Variant> parameters, Json.Reader reader) throws Error {
			foreach (string name in reader.list_members ()) {
				reader.read_member (name);

				if (reader.is_value ()) {
					Json.Node val = reader.get_value ();

					parameters[name] = variant_from_json_value (val);
				} else if (reader.is_array ()) {
					int length = reader.count_elements ();
					if (length == 0)
						throw new Error.PROTOCOL ("Unexpected JSON array element shape");

					if (name == "$icons") {
						var icons = new VariantBuilder (new VariantType.array (VariantType.VARDICT));

						for (int i = 0; i != length; i++) {
							reader.read_element (i);

							string? png_str = reader.get_string_value ();
							if (png_str == null)
								throw new Error.PROTOCOL ("Unexpected JSON icon type");

							var png = new Bytes.take (Base64.decode (png_str));

							icons.open (VariantType.VARDICT);
							icons.add ("{sv}", "format", new Variant.string ("png"));
							icons.add ("{sv}", "image", Variant.new_from_data (new VariantType ("ay"),
								png.get_data (), true, png));
							icons.close ();

							reader.end_element ();
						}

						parameters["icons"] = icons.end ();
					} else {
						reader.read_element (0);
						if (!reader.is_value ())
							throw new Error.PROTOCOL ("Unexpected JSON array element type");
						Variant first_element = variant_from_json_value (reader.get_value ());
						reader.end_element ();

						var builder = new VariantBuilder (new VariantType.array (first_element.get_type ()));
						builder.add_value (first_element);

						for (int i = 1; i != length; i++) {
							reader.read_element (i);
							if (!reader.is_value ())
								throw new Error.PROTOCOL ("Unexpected JSON array element type");
							builder.add_value (variant_from_json_value (reader.get_value ()));
							reader.end_element ();
						}

						parameters[name] = builder.end ();
					}
				} else {
					throw new Error.PROTOCOL ("Unexpected JSON type");
				}

				reader.end_member ();
			}
		}

		private static Variant variant_from_json_value (Json.Node node) throws Error {
			Type type = node.get_value_type ();

			if (type == typeof (string))
				return new Variant.string (node.get_string ());

			if (type == typeof (int64))
				return new Variant.int64 (node.get_int ());

			if (type == typeof (bool))
				return new Variant.boolean (node.get_boolean ());

			throw new Error.PROTOCOL ("Unexpected JSON type: %s", type.name ());
		}
	}

	public interface AndroidHelperTransport : Object {
		public abstract async void close (Cancellable? cancellable) throws IOError;
		public abstract async Json.Node request (Json.Node stanza, Cancellable? cancellable) throws Error, IOError;
	}

	public sealed class AndroidHelperStreamTransport : Object, AndroidHelperTransport {
		public signal void closed ();

		public IOStream stream {
			get;
			construct;
		}

		private BufferedInputStream input;
		private OutputStream output;
		private Cancellable io_cancellable = new Cancellable ();

		private State state = CLOSED;
		private Gee.Queue<Request> pending_requests = new Gee.ArrayQueue<Request> ();

		private ByteArray pending_output = new ByteArray ();
		private bool writing = false;

		private enum State {
			CLOSED,
			OPEN
		}

		private const uint32 MAX_RESPONSE_SIZE = 100 * 1024 * 1024;

		public AndroidHelperStreamTransport (IOStream stream) {
			Object (stream: stream);
		}

		construct {
			input = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
				"base-stream", stream.get_input_stream (),
				"close-base-stream", false,
				"buffer-size", 128 * 1024);
			output = stream.get_output_stream ();

			state = OPEN;

			process_incoming_responses.begin ();
		}

		public async void close (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (close.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield stream.close_async (Priority.DEFAULT, cancellable);
			} catch (IOError e) {
			}

			ensure_closed ();
		}

		private void ensure_closed () {
			if (state == CLOSED)
				return;
			state = CLOSED;
			closed ();
		}

		public async Json.Node request (Json.Node stanza, Cancellable? cancellable) throws Error, IOError {
			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Helper client is closed");

			var r = new Request (request.callback);
			pending_requests.offer (r);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				r.complete_with_error (new IOError.CANCELLED ("Operation was cancelled"));
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			write_request (stanza);

			yield;

			cancel_source.destroy ();

			cancellable.set_error_if_cancelled ();

			GLib.Error? e = r.error;
			if (e != null) {
				if (e is Error)
					throw (Error) e;
				if (e is IOError.CANCELLED)
					throw (IOError) e;
				throw new Error.TRANSPORT ("%s", e.message);
			}

			return r.response;
		}

		private void write_request (Json.Node request) {
			string request_str = Json.to_string (request, false);
			uint8[] request_data = request_str.data;

			uint offset = pending_output.len;
			pending_output.set_size ((uint) (offset + sizeof (uint32) + request_data.length));

			uint8 * blob = (uint8 *) pending_output.data + offset;

			uint32 * size = blob;
			*size = request_data.length.to_big_endian ();

			Memory.copy (blob + 4, request_data, request_data.length);

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

		private async void process_incoming_responses () {
			while (true) {
				try {
					Json.Node response = yield read_response ();

					Request? request = pending_requests.poll ();
					if (request == null)
						throw new Error.PROTOCOL ("Unexpected response");

					request.complete_with_response (response);
				} catch (GLib.Error e) {
					foreach (Request r in pending_requests)
						r.complete_with_error (e);
					pending_requests.clear ();

					ensure_closed ();

					return;
				}
			}
		}

		private async Json.Node read_response () throws GLib.Error {
			size_t header_size = sizeof (uint32);
			if (input.get_available () < header_size)
				yield fill_until_n_bytes_available (header_size, io_cancellable);

			uint32 response_size = 0;
			unowned uint8[] response_size_buf = ((uint8[]) &response_size)[0:4];
			input.peek (response_size_buf);

			response_size = uint32.from_big_endian (response_size);
			if (response_size < 1 || response_size > MAX_RESPONSE_SIZE)
				throw new Error.PROTOCOL ("Invalid response size");

			size_t frame_size = header_size + response_size;
			if (input.get_available () < frame_size)
				yield fill_until_n_bytes_available (frame_size, io_cancellable);

			var response_data = new uint8[response_size + 1];
			response_data.length = (int) response_size;
			input.peek (response_data, header_size);

			input.skip (frame_size, io_cancellable);

			return Json.from_string ((string) response_data);
		}

		private async void fill_until_n_bytes_available (size_t minimum, Cancellable? cancellable) throws GLib.Error {
			size_t available = input.get_available ();
			while (available < minimum) {
				if (input.get_buffer_size () < minimum)
					input.set_buffer_size (minimum);

				ssize_t n = yield input.fill_async ((ssize_t) (input.get_buffer_size () - available),
					Priority.DEFAULT, cancellable);
				if (n == 0)
					throw new IOError.CONNECTION_CLOSED ("Connection closed");

				available += n;
			}
		}

		private class Request {
			private SourceFunc? handler;

			public Json.Node? response;
			public GLib.Error? error;

			public Request (owned SourceFunc handler) {
				this.handler = (owned) handler;
			}

			public void complete_with_response (Json.Node r) {
				if (handler == null)
					return;

				response = r;

				handler ();
				handler = null;
			}

			public void complete_with_error (GLib.Error e) {
				if (handler == null)
					return;

				error = e;

				handler ();
				handler = null;
			}
		}
	}

	public abstract class PackageEntrypoint : Object {
		public int uid {
			get;
			set;
		}

		public static PackageEntrypoint parse (string package, HostSpawnOptions options) throws Error {
			PackageEntrypoint? entrypoint = null;

			HashTable<string, Variant> aux = options.aux;

			Variant? activity_value = aux["activity"];
			if (activity_value != null) {
				if (!activity_value.is_of_type (VariantType.STRING))
					throw new Error.INVALID_ARGUMENT ("The 'activity' option must be a string");
				string activity = canonicalize_class_name (activity_value.get_string (), package);

				if (aux.contains ("action")) {
					throw new Error.INVALID_ARGUMENT (
						"The 'action' option should only be specified when a 'receiver' is specified");
				}

				entrypoint = new ActivityEntrypoint (activity);
			}

			Variant? receiver_value = aux["receiver"];
			if (receiver_value != null) {
				if (!receiver_value.is_of_type (VariantType.STRING))
					throw new Error.INVALID_ARGUMENT ("The 'receiver' option must be a string");
				string receiver = canonicalize_class_name (receiver_value.get_string (), package);

				if (entrypoint != null) {
					throw new Error.INVALID_ARGUMENT (
						"Only one of 'activity' or 'receiver' (with 'action') may be specified");
				}

				Variant? action_value = aux["action"];
				if (action_value == null)
					throw new Error.INVALID_ARGUMENT ("The 'action' option is required when 'receiver' is specified");
				if (!action_value.is_of_type (VariantType.STRING))
					throw new Error.INVALID_ARGUMENT ("The 'action' option must be a string");
				string action = action_value.get_string ();

				entrypoint = new BroadcastReceiverEntrypoint (receiver, action);
			}

			if (entrypoint == null)
				entrypoint = new DefaultActivityEntrypoint ();

			Variant? uid_value = aux["uid"];
			if (uid_value != null) {
				if (!uid_value.is_of_type (VariantType.INT64))
					throw new Error.INVALID_ARGUMENT ("The 'uid' option must be an integer");
				entrypoint.uid = (int) uid_value.get_int64 ();
			}

			return entrypoint;
		}
	}

	public sealed class DefaultActivityEntrypoint : PackageEntrypoint {
		public DefaultActivityEntrypoint () {
			Object ();
		}
	}

	public sealed class ActivityEntrypoint : PackageEntrypoint {
		public string activity {
			get;
			construct;
		}

		public ActivityEntrypoint (string activity) {
			Object (activity: activity);
		}
	}

	public sealed class BroadcastReceiverEntrypoint : PackageEntrypoint {
		public string receiver {
			get;
			construct;
		}

		public string action {
			get;
			construct;
		}

		public BroadcastReceiverEntrypoint (string receiver, string action) {
			Object (receiver: receiver, action: action);
		}
	}

	private static string canonicalize_class_name (string klass, string package) {
		var result = new StringBuilder (klass);

		if (klass.has_prefix (".")) {
			result.prepend (package);
		} else if (klass.index_of (".") == -1) {
			result.prepend_c ('.');
			result.prepend (package);
		}

		return result.str;
	}
}
