[CCode (gir_namespace = "FridaDroidy", gir_version = "1.0")]
namespace Frida.Droidy {
	public sealed class DeviceTracker : Object {
		public signal void device_attached (DeviceDetails details);
		public signal void device_detached (string serial);

		private Client? client;
		private Gee.HashMap<string, DeviceEntry> devices = new Gee.HashMap<string, DeviceEntry> ();
		private Cancellable io_cancellable = new Cancellable ();

		public async void open (Cancellable? cancellable = null) throws Error, IOError {
			client = yield Client.open (cancellable);
			client.message.connect (on_message);

			try {
				try {
					var devices_encoded = yield client.request_data ("host:track-devices-l", cancellable);
					yield update_devices (devices_encoded, cancellable);
				} catch (Error.NOT_SUPPORTED e) {
					client.message.disconnect (on_message);
					client = null;

					client = yield Client.open (cancellable);
					var devices_encoded = yield client.request_data ("host:track-devices", cancellable);
					yield update_devices (devices_encoded, cancellable);
				}
			} catch (GLib.Error e) {
				if (client != null)
					yield client.close (cancellable);

				throw_api_error (e);
			}
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			if (client == null)
				return;

			io_cancellable.cancel ();

			yield client.close (cancellable);
		}

		private void on_message (string devices_encoded) {
			update_devices.begin (devices_encoded, io_cancellable);
		}

		private async void update_devices (string devices_encoded, Cancellable? cancellable) throws IOError {
			var detached = new Gee.ArrayList<DeviceEntry> ();
			var attached = new Gee.ArrayList<DeviceEntry> ();

			var current = new Gee.HashMap<string, string?> ();
			foreach (var line in devices_encoded.split ("\n")) {
				MatchInfo info;
				if (!/^(\S+)\s+(\S+)( (.+))?$/m.match (line, 0, out info)) {
					continue;
				}

				string serial = info.fetch (1);

				string type = info.fetch (2);
				if (type != "device")
					continue;

				string? name = null;
				if (info.get_match_count () == 5) {
					string[] details = info.fetch (4).split (" ");
					foreach (unowned string pair in details) {
						if (pair.has_prefix ("model:")) {
							name = pair.substring (6).replace ("_", " ");
							break;
						}
					}
				}

				current[serial] = name;
			}
			foreach (var e in devices.entries) {
				var serial = e.key;
				if (!current.has_key (serial))
					detached.add (e.value);
			}
			foreach (var entry in current.entries) {
				unowned string serial = entry.key;
				if (!devices.has_key (serial))
					attached.add (new DeviceEntry (serial, entry.value));
			}

			foreach (var entry in detached)
				devices.unset (entry.serial);
			foreach (var entry in attached)
				devices[entry.serial] = entry;

			foreach (var entry in detached) {
				if (entry.announced)
					device_detached (entry.serial);
			}
			foreach (var entry in attached)
				yield announce_device (entry, cancellable);
		}

		private async void announce_device (DeviceEntry entry, Cancellable? cancellable) throws IOError {
			var serial = entry.serial;

			uint port = 0;
			serial.scanf ("emulator-%u", out port);
			if (port != 0) {
				entry.name = "Android Emulator %u".printf (port);
			} else if (entry.name == null) {
				try {
					entry.name = yield detect_name (entry.serial, cancellable);
				} catch (Error e) {
					entry.name = "Android Device";
				}
			}

			var still_attached = devices.has_key (entry.serial);
			if (still_attached) {
				entry.announced = true;
				device_attached (new DeviceDetails (entry.serial, entry.name));
			}
		}

		private async string detect_name (string device_serial, Cancellable? cancellable) throws Error, IOError {
			var output = yield ShellCommand.run ("getprop ro.product.model", device_serial, cancellable);
			return output.chomp ();
		}

		private class DeviceEntry {
			public string serial {
				get;
				private set;
			}

			public string? name {
				get;
				set;
			}

			public bool announced {
				get;
				set;
			}

			public DeviceEntry (string serial, string? name) {
				this.serial = serial;
				this.name = name;
			}
		}
	}

	public sealed class DeviceDetails : Object {
		public string serial {
			get;
			construct;
		}

		public string name {
			get;
			construct;
		}

		public DeviceDetails (string serial, string name) {
			Object (serial: serial, name: name);
		}
	}

	namespace ShellCommand {
		private const int CHUNK_SIZE = 4096;

		public static async string run (string command, string device_serial, Cancellable? cancellable = null) throws Error, IOError {
			var client = yield Client.open (cancellable);

			try {
				yield client.request ("host:transport:" + device_serial, cancellable);
				yield client.request_protocol_change ("shell:" + command, cancellable);

				var input = client.stream.get_input_stream ();
				var buf = new uint8[CHUNK_SIZE];
				var offset = 0;
				while (true) {
					var capacity = buf.length - offset;
					if (capacity < CHUNK_SIZE)
						buf.resize (buf.length + CHUNK_SIZE - capacity);

					ssize_t n;
					try {
						n = yield input.read_async (buf[offset:buf.length - 1], Priority.DEFAULT, cancellable);
					} catch (IOError e) {
						throw new Error.TRANSPORT ("%s", e.message);
					}

					if (n == 0)
						break;

					offset += (int) n;
				}
				buf[offset] = '\0';

				char * chars = buf;
				return (string) chars;
			} finally {
				client.close.begin (cancellable);
			}
		}
	}

	public sealed class ShellSession : Object {
		public signal void output (StdioPipe pipe, Bytes bytes);
		public signal void closed ();

		private IOStream stream;
		private BufferedInputStream input_stream;
		private OutputStream output_stream;
		private Cancellable io_cancellable = new Cancellable ();

		private State state = CLOSED;
		private string session_id = Uuid.string_random ();
		private Gee.Queue<PendingCommand> pending_commands = new Gee.ArrayQueue<PendingCommand> ();

		private ByteArray pending_output = new ByteArray ();
		private bool writing = false;

		private enum State {
			CLOSED,
			IDLE,
			BUSY
		}

		private const uint32 MAX_PAYLOAD_SIZE = 1024 * 1024;

		public async void open (string device_serial, Cancellable? cancellable = null) throws Error, IOError {
			assert (state == CLOSED);

			var client = yield Client.open (cancellable);

			try {
				yield client.request ("host:transport:" + device_serial, cancellable);
				yield client.request_protocol_change ("shell,v2,raw:", cancellable);

				stream = client.stream;
				input_stream = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
					"base-stream", stream.get_input_stream (),
					"close-base-stream", false,
					"buffer-size", 128 * 1024);
				output_stream = stream.get_output_stream ();

				state = IDLE;

				process_incoming_packets.begin ();
			} catch (GLib.Error e) {
				yield client.close (cancellable);

				throw_api_error (e);
			}
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			if (state == CLOSED)
				return;

			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (close.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield stream.close_async (Priority.DEFAULT, cancellable);
			} catch (IOError e) {
			}
		}

		public async void check_call (string command, Cancellable? cancellable) throws Error, IOError {
			var result = yield run (command, cancellable);
			if (result.status != 0)
				throw new Error.INVALID_ARGUMENT ("Shell command failed (%s)", command);
		}

		public async string check_output (string command, Cancellable? cancellable) throws Error, IOError {
			var result = yield run (command, cancellable);
			if (result.status != 0)
				throw new Error.INVALID_ARGUMENT ("Shell command failed (%s)", command);
			return result.stdout_text;
		}

		public async ShellCommandResult run (string command, Cancellable? cancellable) throws Error, IOError {
			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Shell session is closed");

			var pending = new PendingCommand (command, run.callback);
			schedule (pending);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				pending.complete_with_error (new IOError.CANCELLED ("Operation was cancelled"));
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			yield;

			cancel_source.destroy ();

			cancellable.set_error_if_cancelled ();

			GLib.Error? e = pending.error;
			if (e != null) {
				if (e is Error)
					throw (Error) e;
				if (e is IOError.CANCELLED)
					throw (IOError) e;
				throw new Error.TRANSPORT ("%s", e.message);
			}

			return pending.result;
		}

		public void send_command (string command) throws Error {
			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Shell session is closed");

			string full_command = command + "\n";
			write_packet (new Packet (STDIN, new Bytes (full_command.data)));
		}

		private void schedule (PendingCommand command) {
			pending_commands.offer (command);
			maybe_write_next_command ();
		}

		private void move_to_next_command () {
			pending_commands.poll ();
			state = IDLE;
			maybe_write_next_command ();
		}

		private void maybe_write_next_command () {
			if (state != IDLE)
				return;

			PendingCommand? c = pending_commands.peek ();
			if (c == null)
				return;

			state = BUSY;

			string command = "%s; echo -n x$?%s 1>&2\n".printf (c.command, session_id);
			write_packet (new Packet (STDIN, new Bytes (command.data)));
		}

		private async void process_incoming_packets () {
			while (true) {
				try {
					var packet = yield read_packet ();

					dispatch_packet (packet);
				} catch (GLib.Error e) {
					state = CLOSED;

					foreach (PendingCommand c in pending_commands)
						c.complete_with_error (e);
					pending_commands.clear ();

					closed ();

					return;
				}
			}
		}

		private void dispatch_packet (Packet packet) throws Error {
			PendingCommand? c = pending_commands.peek ();
			if (c != null) {
				switch (packet.id) {
					case STDOUT: {
						c.stdout_buffer.append (packet.payload.get_data ());
						break;
					}
					case STDERR: {
						c.stderr_buffer.append (packet.payload.get_data ());

						unowned uint8[] data = c.stderr_buffer.data;

						int minimum_status_size = 2;

						unowned uint8[] term = session_id.data;
						int term_start_offset = data.length - term.length;

						if (data.length >= minimum_status_size + term.length &&
								Memory.cmp ((uint8 *) data + term_start_offset, term, term.length) == 0) {
							int offset = term_start_offset - minimum_status_size;
							while (data[offset] != 'x') {
								offset--;
								if (offset == -1)
									throw new Error.PROTOCOL ("Malformed reply");
							}
							data[term_start_offset] = 0;
							unowned string status_str = (string) ((uint8 *) data + offset + 1);

							uint8 status;
							try {
								uint64 val;
								uint64.from_string (status_str, out val, 10, uint8.MIN, uint8.MAX);
								status = (uint8) val;
							} catch (NumberParserError e) {
								throw new Error.PROTOCOL ("Malformed reply");
							}

							c.stderr_buffer.set_size (offset);

							c.complete_with_status (status);
							move_to_next_command ();
						}

						break;
					}
					default: {
						c.complete_with_error (new Error.PROTOCOL ("Unexpected reply"));
						move_to_next_command ();
						break;
					}
				}
			} else {
				switch (packet.id) {
					case STDOUT:
						output (STDOUT, packet.payload);
						break;
					case STDERR:
						output (STDERR, packet.payload);
						break;
					default:
						break;
				}
			}
		}

		private async Packet read_packet () throws GLib.Error {
			size_t header_size = sizeof (uint8) + sizeof (uint32);
			if (input_stream.get_available () < header_size)
				yield fill_until_n_bytes_available (header_size, io_cancellable);

			uint8 id = 0;
			unowned uint8[] id_buf = ((uint8[]) &id)[0:1];
			input_stream.peek (id_buf);

			uint32 payload_size = 0;
			unowned uint8[] payload_size_buf = ((uint8[]) &payload_size)[0:4];
			input_stream.peek (payload_size_buf, sizeof (uint8));

			payload_size = uint32.from_little_endian (payload_size);
			if (payload_size < 1 || payload_size > MAX_PAYLOAD_SIZE)
				throw new Error.PROTOCOL ("Invalid message size");

			size_t frame_size = header_size + payload_size;
			if (input_stream.get_available () < frame_size)
				yield fill_until_n_bytes_available (frame_size, io_cancellable);

			var payload_buf = new uint8[payload_size + 1];
			payload_buf.length = (int) payload_size;
			input_stream.peek (payload_buf, header_size);

			input_stream.skip (frame_size, io_cancellable);

			return new Packet ((PacketId) id, new Bytes.take ((owned) payload_buf));
		}

		private void write_packet (Packet packet) {
			size_t header_size = sizeof (uint8) + sizeof (uint32);
			size_t payload_size = packet.payload.get_size ();

			uint offset = pending_output.len;
			pending_output.set_size ((uint) (offset + header_size + payload_size));

			uint8 * packet_buf = (uint8 *) pending_output.data + offset;
			packet_buf[0] = packet.id;
			*((uint32 *) (packet_buf + 1)) = ((uint32) payload_size).to_little_endian ();
			Memory.copy (packet_buf + header_size, packet.payload.get_data (), payload_size);

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
					yield output_stream.write_all_async (batch, Priority.DEFAULT, io_cancellable, out bytes_written);
				} catch (GLib.Error e) {
					break;
				}
			}

			writing = false;
		}

		private async void fill_until_n_bytes_available (size_t minimum, Cancellable? cancellable) throws GLib.Error {
			size_t available = input_stream.get_available ();
			while (available < minimum) {
				if (input_stream.get_buffer_size () < minimum)
					input_stream.set_buffer_size (minimum);

				ssize_t n = yield input_stream.fill_async ((ssize_t) (input_stream.get_buffer_size () - available),
					Priority.DEFAULT, cancellable);
				if (n == 0)
					throw new IOError.CONNECTION_CLOSED ("Connection closed");

				available += n;
			}
		}

		private class PendingCommand {
			public string command;
			private SourceFunc? handler;

			public ShellCommandResult? result;
			public GLib.Error? error;

			public ByteArray stdout_buffer = new ByteArray ();
			public ByteArray stderr_buffer = new ByteArray ();

			public PendingCommand (string command, owned SourceFunc handler) {
				this.command = command;
				this.handler = (owned) handler;
			}

			public void complete_with_status (uint8 status) {
				if (handler == null)
					return;

				stdout_buffer.append ({ 0 });
				uint8[] stdout_terminated = stdout_buffer.steal ();
				stdout_terminated.length--;

				stderr_buffer.append ({ 0 });
				uint8[] stderr_terminated = stderr_buffer.steal ();
				stderr_terminated.length--;

				Bytes stdout_bytes = new Bytes.take ((owned) stdout_terminated);
				Bytes stderr_bytes = new Bytes.take ((owned) stderr_terminated);

				result = new ShellCommandResult (status, stdout_bytes, stderr_bytes);

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

		private enum PacketId {
			STDIN,
			STDOUT,
			STDERR,
			EXIT,

			CLOSE_STDIN,

			WINDOW_SIZE_CHANGE
		}

		private class Packet {
			public PacketId id;
			public Bytes payload;

			public Packet (PacketId id, Bytes payload) {
				this.id = id;
				this.payload = payload;
			}
		}
	}

	public enum StdioPipe {
		STDOUT,
		STDERR
	}

	public sealed class ShellCommandResult : Object {
		public uint8 status {
			get;
			construct;
		}

		public unowned string stdout_text {
			get {
				return (string) stdout_bytes.get_data ();
			}
		}

		public Bytes stdout_bytes {
			get;
			construct;
		}

		public unowned string stderr_text {
			get {
				return (string) stderr_bytes.get_data ();
			}
		}

		public Bytes stderr_bytes {
			get;
			construct;
		}

		public ShellCommandResult (uint8 status, Bytes stdout_bytes, Bytes stderr_bytes) {
			Object (
				status: status,
				stdout_bytes: stdout_bytes,
				stderr_bytes: stderr_bytes
			);
		}
	}

	namespace FileSync {
		private const size_t MAX_DATA_SIZE = 65536;

		public static async void send (InputStream content, FileMetadata metadata, string remote_path, string device_serial,
				Cancellable? cancellable = null) throws Error, IOError {
			var client = yield Client.open (cancellable);

			try {
				yield client.request ("host:transport:" + device_serial, cancellable);

				var session = yield client.request_sync_session (cancellable);

				var cmd_buf = new MemoryOutputStream.resizable ();
				var cmd = new DataOutputStream (cmd_buf);
				cmd.byte_order = LITTLE_ENDIAN;

				string raw_mode = "%u".printf (metadata.mode);

				cmd.put_string ("SEND");
				cmd.put_uint32 (remote_path.length + 1 + raw_mode.length);
				cmd.put_string (remote_path);
				cmd.put_string (",");
				cmd.put_string (raw_mode);

				while (true) {
					Bytes chunk = yield content.read_bytes_async (MAX_DATA_SIZE, Priority.DEFAULT, cancellable);
					size_t size = chunk.get_size ();
					if (size == 0)
						break;

					cmd.put_string ("DATA");
					cmd.put_uint32 ((uint32) size);
					cmd.write_bytes (chunk);

					cmd_buf.close ();
					yield session.send (cmd_buf.steal_as_bytes (), cancellable);

					cmd_buf = new MemoryOutputStream.resizable ();
					cmd = new DataOutputStream (cmd_buf);
					cmd.byte_order = LITTLE_ENDIAN;
				}

				cmd.put_string ("DONE");
				cmd.put_uint32 ((uint32) metadata.time_modified.to_unix ());

				cmd_buf.close ();
				yield session.finish (cmd_buf.steal_as_bytes (), cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			} finally {
				client.close.begin (cancellable);
			}
		}
	}

	public sealed class FileMetadata : Object {
		public uint32 mode {
			get;
			set;
			default = 0100644;
		}

		public DateTime time_modified {
			get;
			set;
			default = new DateTime.now_local ();
		}
	}

	public sealed class JDWPTracker : Object {
		public signal void debugger_attached (uint pid);
		public signal void debugger_detached (uint pid);

		private Client? client;
		private Gee.HashSet<uint> debugger_pids = new Gee.HashSet<uint> ();
		private Cancellable io_cancellable = new Cancellable ();

		public async void open (string device_serial, Cancellable? cancellable = null) throws Error, IOError {
			client = yield Client.open (cancellable);
			client.message.connect (on_message);

			try {
				yield client.request ("host:transport:" + device_serial, cancellable);
				string? pids_encoded = yield client.request_data ("track-jdwp", cancellable);
				update_pids (pids_encoded, cancellable);
			} catch (GLib.Error e) {
				yield client.close (cancellable);

				throw_api_error (e);
			}
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			if (client == null)
				return;

			io_cancellable.cancel ();

			yield client.close (cancellable);
		}

		private void on_message (string pids_encoded) {
			try {
				update_pids (pids_encoded, io_cancellable);
			} catch (IOError e) {
			}
		}

		private void update_pids (string pids_encoded, Cancellable? cancellable) throws IOError {
			var detached = new Gee.ArrayList<uint> ();
			var attached = new Gee.ArrayList<uint> ();

			var current = new Gee.HashSet<uint> ();
			foreach (var line in pids_encoded.chomp ().split ("\n")) {
				uint pid = uint.parse (line);
				current.add (pid);
			}
			foreach (var pid in debugger_pids) {
				if (!current.contains (pid))
					detached.add (pid);
			}
			foreach (var pid in current) {
				if (!debugger_pids.contains (pid))
					attached.add (pid);
			}

			foreach (var pid in detached) {
				debugger_pids.remove (pid);
				debugger_detached (pid);
			}
			foreach (var pid in attached) {
				debugger_pids.add (pid);
				debugger_attached (pid);
			}
		}
	}

	public sealed class Client : Object {
		public signal void closed ();
		public signal void message (string payload);

		public IOStream stream {
			get;
			construct;
		}
		private InputStream input;
		private OutputStream output;
		private Cancellable io_cancellable = new Cancellable ();

		protected bool is_processing_messages;
		private Gee.ArrayQueue<PendingResponse> pending_responses = new Gee.ArrayQueue<PendingResponse> ();

		public enum RequestType {
			COMMAND,
			SYNC,
			DATA,
			PROTOCOL_CHANGE
		}

		private const uint16 ADB_SERVER_DEFAULT_PORT = 5037;
		private const size_t MAX_MESSAGE_LENGTH = 65536;

		public static async Client open (Cancellable? cancellable = null) throws Error, IOError {
			SocketConnectable? connectable = null;
			string? env_socket_address = Environment.get_variable ("ADB_SERVER_SOCKET");
			string? env_server_address = Environment.get_variable ("ANDROID_ADB_SERVER_ADDRESS");
			string? env_server_port = Environment.get_variable ("ANDROID_ADB_SERVER_PORT");
			if (env_socket_address != null) {
				string[] tokens = env_socket_address.split (":", 2);
				if (tokens.length == 2) {
					unowned string socket_type = tokens[0];
					unowned string socket_address = tokens[1];
					if (socket_type == "tcp") {
						try {
							connectable = NetworkAddress.parse (socket_address, ADB_SERVER_DEFAULT_PORT);
						} catch (GLib.Error e) {
						}
					}
#if !WINDOWS
					else if (socket_type == "local" || socket_type == "localfilesystem") {
						connectable = new UnixSocketAddress (socket_address);
					} else if (socket_type == "localabstract" && UnixSocketAddress.abstract_names_supported ()) {
						connectable = new UnixSocketAddress.with_type (socket_address, socket_address.length,
							ABSTRACT_PADDED);
					}
#endif
				}
			} else if (env_server_address != null && env_server_port != null) {
				connectable = new NetworkAddress (
					env_server_address,
					(uint16) uint.parse (env_server_port)
				);
			} else if (env_server_address != null) {
				connectable = new NetworkAddress (
					env_server_address,
					ADB_SERVER_DEFAULT_PORT
				);
			} else if (env_server_port != null) {
				connectable = new NetworkAddress.loopback ((uint16) uint.parse (env_server_port));
			}

			if (connectable == null)
				connectable = new NetworkAddress.loopback (ADB_SERVER_DEFAULT_PORT);

			IOStream stream;
			try {
				var client = new SocketClient ();
				var connection = yield client.connect_async (connectable, cancellable);

				Tcp.enable_nodelay (connection.socket);

				stream = connection;
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			return new Client (stream);
		}

		public Client (IOStream stream) {
			Object (stream: stream);
		}

		construct {
			input = stream.get_input_stream ();
			output = stream.get_output_stream ();

			is_processing_messages = true;

			process_incoming_messages.begin ();
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
				yield this.stream.close_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
			}
		}

		public async void request (string message, Cancellable? cancellable = null) throws Error, IOError {
			yield request_with_type (message, RequestType.COMMAND, cancellable);
		}

		public async string request_data (string message, Cancellable? cancellable = null) throws Error, IOError {
			return yield request_with_type (message, RequestType.DATA, cancellable);
		}

		public async void request_protocol_change (string message, Cancellable? cancellable = null) throws Error, IOError {
			yield request_with_type (message, RequestType.PROTOCOL_CHANGE, cancellable);
		}

		public async SyncSession request_sync_session (Cancellable? cancellable = null) throws Error, IOError {
			yield request ("sync:", cancellable);

			var session = new SyncSession (this);

			PendingResponse pending = null;
			pending = new PendingResponse (RequestType.SYNC, () => {
				session._end (pending.error);
			});
			pending_responses.offer_tail (pending);

			return session;
		}

		private async string? request_with_type (string message, RequestType request_type, Cancellable? cancellable)
				throws Error, IOError {
			Bytes response_bytes = yield request_with_bytes (new Bytes (message.data), request_type, cancellable);
			if (response_bytes == null)
				return null;
			return (string) response_bytes.get_data ();
		}

		public async Bytes? request_with_bytes (Bytes message, RequestType request_type, Cancellable? cancellable) throws Error, IOError {
			bool waiting = false;

			var pending = new PendingResponse (request_type, () => {
				if (waiting)
					request_with_bytes.callback ();
			});
			pending_responses.offer_tail (pending);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				pending.complete_with_error (new IOError.CANCELLED ("Operation was cancelled"));
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			try {
				size_t bytes_written;
				try {
					if (request_type == SYNC) {
						yield output.write_all_async (message.get_data (), Priority.DEFAULT, cancellable, out bytes_written);
					} else {
						var message_size = message.get_size ();
						var message_buf = new uint8[4 + message_size];
						var length_str = "%04x".printf (message.length);
						Memory.copy (message_buf, length_str, 4);
						Memory.copy ((uint8 *) message_buf + 4, message.get_data (), message_size);

						yield output.write_all_async (message_buf, Priority.DEFAULT, cancellable, out bytes_written);
					}
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("Unable to write message: %s", e.message);
				}

				if (!pending.completed) {
					waiting = true;
					yield;
					waiting = false;
				}
			} finally {
				cancel_source.destroy ();
			}

			cancellable.set_error_if_cancelled ();

			if (pending.error != null)
				throw_api_error (pending.error);

			return pending.result;
		}

		internal async void write_subcommand_chunk (Bytes chunk, Cancellable? cancellable) throws Error, IOError {
			try {
				size_t bytes_written;
				yield output.write_all_async (chunk.get_data (), Priority.DEFAULT, cancellable, out bytes_written);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to write subcommand chunk: %s", e.message);
			}
		}

		private async void process_incoming_messages () {
			while (is_processing_messages) {
				try {
					var command_or_length = yield read_fixed_string (4);
					switch (command_or_length) {
						case "OKAY":
						case "FAIL":
							var pending = pending_responses.poll_head ();
							if (pending != null) {
								var success = command_or_length == "OKAY";
								if (success) {
									Bytes? result;
									if (pending.request_type == RequestType.DATA)
										result = yield read_bytes ();
									else
										result = null;
									pending.complete_with_result (result);

									if (pending.request_type == RequestType.PROTOCOL_CHANGE) {
										is_processing_messages = false;
										return;
									}
								} else {
									var error_message = yield read_string (pending.request_type);
									pending.complete_with_error (
										new Error.NOT_SUPPORTED (error_message));
								}
							} else {
								throw new Error.PROTOCOL ("Reply to unknown request");
							}
							break;
						case "SYNC":
						case "CNXN":
						case "AUTH":
						case "OPEN":
						case "CLSE":
						case "WRTE":
							throw new Error.PROTOCOL ("Unexpected command");

						default:
							var length = parse_length (command_or_length);
							var payload = yield read_fixed_string (length);
							message (payload);
							break;
					}
				} catch (Error e) {
					foreach (var pending_response in pending_responses)
						pending_response.complete_with_error (e);
					is_processing_messages = false;
				}
			}
		}

		private async string read_string (RequestType type) throws Error {
			size_t length;
			if (type == SYNC) {
				length = yield read_u32 ();
			} else {
				var length_str = yield read_fixed_string (4);
				length = parse_length (length_str);
			}

			return yield read_fixed_string (length);
		}

		private async string read_fixed_string (size_t length) throws Error {
			var buf = new uint8[length + 1];
			size_t bytes_read;
			try {
				yield input.read_all_async (buf[0:length], Priority.DEFAULT, io_cancellable, out bytes_read);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to read string: %s", e.message);
			}
			if (bytes_read == 0)
				closed ();
			if (bytes_read != length)
				throw new Error.TRANSPORT ("Unable to read string");
			buf[length] = '\0';
			char * chars = buf;
			return (string) chars;
		}

		private async uint32 read_u32 () throws Error {
			uint32 result = 0;

			unowned uint8[] buf = (uint8[]) &result;
			size_t bytes_read;
			try {
				yield input.read_all_async (buf[0:sizeof (uint32)], Priority.DEFAULT, io_cancellable, out bytes_read);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to read: %s", e.message);
			}
			if (bytes_read != sizeof (uint32))
				throw new Error.TRANSPORT ("Unable to read");

			return result;
		}

		private async Bytes read_bytes () throws Error {
			var length_str = yield read_fixed_string (4);
			var length = parse_length (length_str);

			var buf = new uint8[length + 1];
			if (length > 0) {
				size_t bytes_read;
				try {
					yield input.read_all_async (buf[0:length], Priority.DEFAULT, io_cancellable, out bytes_read);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("Unable to read: %s", e.message);
				}
				if (bytes_read != length)
					throw new Error.TRANSPORT ("Unable to read");
			}
			buf[length] = '\0';
			buf.length = (int) length;

			return new Bytes.take ((owned) buf);
		}

		private size_t parse_length (string str) throws Error {
			int length = 0;
			str.scanf ("%04x", out length);
			if (length < 0 || length > MAX_MESSAGE_LENGTH)
				throw new Error.PROTOCOL ("Invalid message length");
			return length;
		}

		private class PendingResponse {
			public delegate void CompletionHandler ();
			private CompletionHandler? handler;

			public RequestType request_type {
				get;
				private set;
			}

			public bool completed {
				get;
				private set;
			}

			public Bytes? result {
				get;
				private set;
			}

			public GLib.Error? error {
				get;
				private set;
			}

			public PendingResponse (RequestType request_type, owned CompletionHandler handler) {
				this.request_type = request_type;
				this.handler = (owned) handler;
			}

			public void complete_with_result (Bytes? val) {
				if (handler == null)
					return;
				completed = true;
				result = val;
				handler ();
				handler = null;
			}

			public void complete_with_error (GLib.Error e) {
				if (handler == null)
					return;
				completed = true;
				error = e;
				handler ();
				handler = null;
			}
		}
	}

	public sealed class SyncSession : Object {
		public Client client {
			get;
			construct;
		}

		private Promise<bool> io_request = new Promise<bool> ();

		public SyncSession (Client client) {
			Object (client: client);
		}

		public async void send (Bytes chunk, Cancellable? cancellable = null) throws Error, IOError {
			var future = io_request.future;
			if (future.ready) {
				if (future.error != null)
					throw (Error) future.error;
				else
					throw new Error.PROTOCOL ("Sync session terminated unexpectedly");
			}

			yield client.write_subcommand_chunk (chunk, cancellable);
		}

		public async void finish (Bytes chunk, Cancellable? cancellable = null) throws Error, IOError {
			yield send (chunk, cancellable);

			yield io_request.future.wait_async (cancellable);
		}

		internal void _end (GLib.Error? error) {
			if (error == null)
				io_request.resolve (true);
			else
				io_request.reject (error);
		}
	}
}
