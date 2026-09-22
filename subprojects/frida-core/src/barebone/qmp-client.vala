[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class QmpClient : Object, AsyncInitable {
		public signal void event (string name, Json.Node? data);

		private string? hostlink_port;
		private bool hostlink_is_open = false;
		private SourceFunc? hostlink_open_handler;

		public string address {
			get;
			construct;
		}

		public uint16 port {
			get;
			construct;
		}

		private SocketConnection connection;
		private DataInputStream input;
		private OutputStream output;

		private bool is_connected = false;
		private Promise<bool> close_request = new Promise<bool> ();

		private Gee.Map<uint, Promise<Json.Node>> pending_requests = new Gee.HashMap<uint, Promise<Json.Node>> ();
		private uint next_request_id = 1;

		private Cancellable io_cancellable = new Cancellable ();

		private const int REQUEST_TIMEOUT_MS = 30000;
		private const uint UNPLUG_MAX_ATTEMPTS = 40;
		private const uint UNPLUG_INTERVAL_MS = 500;

		public QmpClient (string? address = null, uint16 port = 0) {
			Object (
				address: address ?? "localhost",
				port: port != 0 ? port : 4444
			);
		}

		public static async QmpClient open (string? address = null, uint16 port = 0, Cancellable? cancellable = null)
				throws Error, IOError {
			var client = new QmpClient (address, port);

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return client;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			SocketConnectable connectable;
			if (address.has_prefix ("unix:")) {
				connectable = new UnixSocketAddress.with_type (address.substring (5), -1,
					UnixSocketAddressType.PATH);
			} else {
				connectable = parse_socket_address (address, port, "localhost", 4444);
			}

			try {
				var client = new SocketClient ();
				connection = yield client.connect_async (connectable, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to connect to QMP server: %s", e.message);
			}

			var socket = connection.socket;
			if (socket.get_family () != UNIX)
				Tcp.enable_nodelay (socket);

			input = new DataInputStream (connection.get_input_stream ());
			input.set_newline_type (DataStreamNewlineType.LF);
			output = connection.get_output_stream ();

			bool started_message_processing = false;
			try {
				is_connected = true;

				string? greeting_line = yield input.read_line_async (Priority.DEFAULT, cancellable);
				if (greeting_line == null)
					throw new Error.TRANSPORT ("Connection closed during QMP greeting");

				process_incoming_messages.begin ();
				started_message_processing = true;

				yield execute_command ("qmp_capabilities", null, cancellable);

				return true;
			} catch (GLib.Error e) {
				is_connected = false;
				if (!started_message_processing)
					close_request.resolve (true);

				if (e is Error)
					throw_api_error (e);
				else
					throw new Error.TRANSPORT ("%s", e.message);
			}
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			io_cancellable.cancel ();

			try {
				yield close_request.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		public async void wait_until_hostlink_is_open (Cancellable? cancellable) throws Error, IOError {
			while (!hostlink_is_open) {
				hostlink_open_handler = wait_until_hostlink_is_open.callback;
				yield;
			}
		}

		private void on_event (string name, Json.Node? data) {
			if (name != "VSERPORT_CHANGE" || data == null)
				return;

			var details = data.get_object ();
			if (details.get_string_member_with_default ("id", "") != hostlink_port)
				return;
			if (!details.get_boolean_member_with_default ("open", false))
				return;

			hostlink_is_open = true;

			if (hostlink_open_handler != null) {
				var handler = (owned) hostlink_open_handler;
				hostlink_open_handler = null;
				handler ();
			}
		}

		public async Hostlink open_hostlink (string? preferred_bus = null, Cancellable? cancellable = null)
				throws Error, IOError {
			uint64 mmio = 0;
			uint irq = 0;
			string bus = preferred_bus;
			if (bus == null) {
				mmio = (uint64) yield get_qom_property_int ("/machine", "hostlink-mmio", cancellable);
				irq = (uint) yield get_qom_property_int ("/machine", "hostlink-irq", cancellable);
				bus = yield get_qom_property_string ("/machine", "hostlink-bus", cancellable);
			}

			if (bus != null && mmio == 0)
				mmio = yield resolve_mmio_base (bus, cancellable);

			string chardev = "vserial0";
			string device = "hostlink.port";
			yield unplug_leftover_hostlink (chardev, device, cancellable);

			hostlink_port = device;
			event.connect (on_event);

			var connection = yield plug_hostlink (chardev, device, bus, cancellable);

			return new Hostlink () {
				connection = connection,
				mmio = mmio,
				irq = irq,
			};
		}

		private async uint64 resolve_mmio_base (string bus, Cancellable? cancellable) throws IOError {
			var controller = bus[:bus.last_index_of (".")];

			string parent;
			try {
				parent = yield get_qom_property_string ("/machine/peripheral/" + controller, "parent_bus", cancellable);
			} catch (Error e) {
				return 0;
			}

			var marker = "virtio-mmio-bus.";
			var at = parent.index_of (marker);
			if (at == -1)
				return 0;

			return VIRT_MMIO_BASE + (uint64) int.parse (parent[at + marker.length:]) * VIRT_MMIO_STRIDE;
		}

		private const uint64 VIRT_MMIO_BASE = 0x0a000000;
		private const uint64 VIRT_MMIO_STRIDE = 0x200;

		private async void unplug_leftover_hostlink (string chardev, string device,
				Cancellable? cancellable) throws IOError {
			try {
				yield delete_device (device, cancellable);
			} catch (Error e) {
				return;
			}

			for (uint attempt = 0; attempt != UNPLUG_MAX_ATTEMPTS; attempt++) {
				try {
					yield remove_chardev (chardev, cancellable);
					return;
				} catch (Error e) {
					if (!("busy" in e.message))
						return;
				}

				var timeout = new TimeoutSource (UNPLUG_INTERVAL_MS);
				timeout.set_callback (unplug_leftover_hostlink.callback);
				timeout.attach (MainContext.get_thread_default ());
				yield;
				timeout.destroy ();
			}
		}

		private async void delete_device (string id, Cancellable? cancellable) throws Error, IOError {
			var args = new Json.Builder ();
			args
				.begin_object ()
					.set_member_name ("id")
					.add_string_value (id)
				.end_object ();
			yield execute_command ("device_del", args.get_root (), cancellable);
		}

		private async void remove_chardev (string id, Cancellable? cancellable) throws Error, IOError {
			var args = new Json.Builder ();
			args
				.begin_object ()
					.set_member_name ("id")
					.add_string_value (id)
				.end_object ();
			yield execute_command ("chardev-remove", args.get_root (), cancellable);
		}

		private async SocketConnection plug_hostlink (string chardev, string device, string bus,
				Cancellable? cancellable) throws Error, IOError {
			string socket_path = Path.build_filename (Environment.get_tmp_dir (),
				"frida-hl-" + Uuid.string_random ().substring (0, 8) + ".sock");

			yield add_listening_chardev (chardev, socket_path, cancellable);
			yield add_serial_port (chardev, bus, "re.frida.hostlink", device, 1, cancellable);

			var client = new SocketClient ();
			try {
				return yield client.connect_async (new UnixSocketAddress (socket_path), cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to connect to %s: %s", socket_path, e.message);
			}
		}

		public class Hostlink {
			public SocketConnection connection;
			public uint64 mmio;
			public uint irq;
		}

		private async int64 get_qom_property_int (string path, string property, Cancellable? cancellable) throws Error, IOError {
			var val = yield get_qom_property (path, property, cancellable);
			if (val.get_value_type () != typeof (int64))
				throw new Error.PROTOCOL ("Expected '%s' property on %s to be an integer", property, path);
			return val.get_int ();
		}

		private async string get_qom_property_string (string path, string property, Cancellable? cancellable)
				throws Error, IOError {
			var val = yield get_qom_property (path, property, cancellable);
			if (val.get_value_type () != typeof (string))
				throw new Error.PROTOCOL ("Expected '%s' property on %s to be a string", property, path);
			return val.get_string ();
		}

		private async Json.Node get_qom_property (string path, string property, Cancellable? cancellable) throws Error, IOError {
			var args = new Json.Builder ();
			args
				.begin_object ()
					.set_member_name ("path")
					.add_string_value (path)
					.set_member_name ("property")
					.add_string_value (property)
				.end_object ();
			return yield execute_command ("qom-get", args.get_root (), cancellable);
		}

		private async void add_listening_chardev (string id, string path, Cancellable? cancellable)
				throws Error, IOError {
			var args = new Json.Builder ();
			args
				.begin_object ()
					.set_member_name ("id")
					.add_string_value (id)
					.set_member_name ("backend")
					.begin_object ()
						.set_member_name ("type")
						.add_string_value ("socket")
						.set_member_name ("data")
						.begin_object ()
							.set_member_name ("server")
							.add_boolean_value (true)
							.set_member_name ("wait")
							.add_boolean_value (false)
							.set_member_name ("addr")
							.begin_object ()
								.set_member_name ("type")
								.add_string_value ("unix")
								.set_member_name ("data")
								.begin_object ()
									.set_member_name ("path")
									.add_string_value (path)
								.end_object ()
							.end_object ()
						.end_object ()
					.end_object ()
				.end_object ();
			yield execute_command ("chardev-add", args.get_root (), cancellable);
		}

		private async void add_serial_port (string chardev, string bus, string name, string id, uint nr, Cancellable? cancellable)
				throws Error, IOError {
			var args = new Json.Builder ();
			args
				.begin_object ()
					.set_member_name ("driver")
					.add_string_value ("virtserialport")
					.set_member_name ("chardev")
					.add_string_value (chardev)
					.set_member_name ("bus")
					.add_string_value (bus)
					.set_member_name ("name")
					.add_string_value (name)
					.set_member_name ("id")
					.add_string_value (id)
					.set_member_name ("nr")
					.add_int_value ((int64) nr)
				.end_object ();
			yield execute_command ("device_add", args.get_root (), cancellable);
		}

		public async Json.Node execute_command (string command, Json.Node? arguments = null, Cancellable? cancellable = null)
				throws Error, IOError {
			check_connected ();

			Request request = begin_request (command, arguments);

			try {
				yield output.write_all_async (request.json.data, Priority.DEFAULT, cancellable, null);
			} catch (GLib.Error e) {
				cancel_request (request);
				throw new Error.TRANSPORT ("%s", e.message);
			}

			return yield join_request (request, cancellable);
		}

		private Request begin_request (string command, Json.Node? arguments) {
			uint id = next_request_id++;

			var promise = new Promise<Json.Node> ();
			pending_requests[id] = promise;

			string json = build_request (command, id, arguments);

			return new Request () {
				promise = promise,
				json = json,
				id = id,
			};
		}

		private static string build_request (string command, uint id, Json.Node? arguments) {
			var b = new Json.Builder ();
			b
				.begin_object ()
				.set_member_name ("execute")
				.add_string_value (command);

			if (arguments != null) {
				b
					.set_member_name ("arguments")
					.add_value (arguments);
			}

			b
				.set_member_name ("id")
				.add_int_value ((int64) id);

			Json.Node message = b.end_object ().get_root ();

			return Json.to_string (message, false) + "\n";
		}

		private async Json.Node join_request (Request request, Cancellable? cancellable) throws Error, IOError {
			var timeout_source = new TimeoutSource (REQUEST_TIMEOUT_MS);
			timeout_source.set_callback (() => {
				Promise<Json.Node>? p;
				if (pending_requests.unset (request.id, out p))
					p.reject (new Error.TIMED_OUT ("QMP command timed out"));
				return Source.REMOVE;
			});
			timeout_source.attach (MainContext.get_thread_default ());

			try {
				return yield request.promise.future.wait_async (cancellable);
			} finally {
				timeout_source.destroy ();
			}
		}

		private void cancel_request (Request r) {
			pending_requests.unset (r.id);
		}

		private class Request {
			public Promise<Json.Node> promise;
			public string json;
			public uint id;
		}

		private void check_connected () throws Error {
			if (!is_connected)
				throw new Error.INVALID_OPERATION ("QMP client is not connected");
		}

		private async void process_incoming_messages () {
			try {
				while (is_connected) {
					string? line = yield input.read_line_async (Priority.DEFAULT, io_cancellable);
					if (line == null)
						break;

					handle_message (Json.from_string (line));
				}
			} catch (GLib.Error e) {
			} finally {
				is_connected = false;

				foreach (var promise in pending_requests.values)
					promise.reject (new Error.TRANSPORT ("QMP connection closed"));
				pending_requests.clear ();
			}

			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (process_incoming_messages.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield connection.close_async ();
			} catch (GLib.Error e) {
			}

			close_request.resolve (true);
		}

		private void handle_message (owned Json.Node message) throws Error {
			var r = make_json_reader_taking_node ((owned) message);

			bool is_response = r.read_member ("id");
			uint id = 0;
			if (is_response) {
				id = (uint) r.get_int_value ();
				GLib.Error? e = r.get_error ();
				if (e != null)
					throw new Error.PROTOCOL ("Malformed message: %s", e.message);
			}
			r.end_member ();

			if (is_response)
				handle_response (id, r);
			else
				handle_event (r);
		}

		private void handle_response (uint request_id, Json.Reader r) throws Error {
			Promise<Json.Node>? promise;
			if (!pending_requests.unset (request_id, out promise))
				return;

			if (r.read_member ("error")) {
				r.read_member ("desc");
				string? description = r.get_string_value ();
				if (description == null)
					throw new Error.PROTOCOL ("Malformed message: %s", r.get_error ().message);
				r.end_member ();

				promise.reject (new Error.NOT_SUPPORTED ("%s", description));
				return;
			}
			r.end_member ();

			r.read_member ("return");
			promise.resolve (r.get_current_node ());
		}

		private void handle_event (Json.Reader r) throws Error {
			r.read_member ("event");
			string? name = r.get_string_value ();
			if (name == null)
				throw new Error.PROTOCOL ("Malformed event message: missing 'event' property");
			r.end_member ();

			Json.Node? data = null;
			if (r.read_member ("data")) {
				if (!r.is_object ())
					throw new Error.PROTOCOL ("Malformed event message: 'data' must be an object");
				data = r.get_current_node ();
			}
			r.end_member ();

			event (name, data);
		}
	}
}
