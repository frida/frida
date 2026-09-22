[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class PlistServiceClient : Object {
		public signal void closed ();

		public IOStream stream {
			get {
				return _stream;
			}
			set {
				_stream = value;
				input = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
					"base-stream", stream.get_input_stream (),
					"close-base-stream", false,
					"buffer-size", 128 * 1024);
				output = stream.get_output_stream ();
			}
		}

		private IOStream _stream;
		private BufferedInputStream input;
		private OutputStream output;
		private Cancellable io_cancellable = new Cancellable ();

		private State state = OPEN;

		private ByteArray pending_output = new ByteArray ();
		private bool writing = false;

		private enum State {
			OPEN,
			CLOSED
		}

		private const uint32 MAX_MESSAGE_SIZE = 100 * 1024 * 1024;

		public PlistServiceClient (IOStream stream) {
			Object (stream: stream);
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
		}

		public async Plist query (Plist request, Cancellable? cancellable) throws PlistServiceError, IOError {
			write_message (request);
			return yield read_message (cancellable);
		}

		public void write_message (Plist message) {
			uint8[] message_data = message.to_binary ();

			uint offset = pending_output.len;
			pending_output.set_size ((uint) (offset + sizeof (uint32) + message_data.length));

			uint8 * blob = (uint8 *) pending_output.data + offset;

			uint32 * size = blob;
			*size = message_data.length.to_big_endian ();

			Memory.copy (blob + 4, message_data, message_data.length);

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

		public async Plist read_message (Cancellable? cancellable) throws PlistServiceError, IOError {
			var messages = yield read_messages (1, cancellable);
			return messages[0];
		}

		public async Gee.List<Plist> read_messages (size_t limit, Cancellable? cancellable) throws PlistServiceError, IOError {
			var result = new Gee.ArrayList<Plist> ();

			do {
				size_t header_size = sizeof (uint32);
				if (input.get_available () < header_size) {
					if (result.is_empty)
						yield fill_until_n_bytes_available (header_size, cancellable);
					else
						break;
				}

				uint32 message_size = 0;
				unowned uint8[] size_buf = ((uint8[]) &message_size)[0:4];
				input.peek (size_buf);
				message_size = uint32.from_big_endian (message_size);
				if (message_size < 1 || message_size > MAX_MESSAGE_SIZE)
					throw new PlistServiceError.PROTOCOL ("Invalid message size");

				size_t frame_size = header_size + message_size;
				if (input.get_available () < frame_size) {
					if (result.is_empty)
						yield fill_until_n_bytes_available (frame_size, cancellable);
					else
						break;
				}

				var message_buf = new uint8[message_size + 1];
				unowned uint8[] message_data = message_buf[0:message_size];
				input.peek (message_data, header_size);

				input.skip (frame_size, cancellable);

				Plist message;
				try {
					unowned string message_str = (string) message_buf;
					if (message_str.has_prefix ("bplist"))
						message = new Plist.from_binary (message_data);
					else
						message = new Plist.from_xml (message_str);
				} catch (PlistError e) {
					throw new PlistServiceError.PROTOCOL ("Malformed message: %s", e.message);
				}

				result.add (message);
			} while (limit == 0 || result.size != limit);

			return result;
		}

		private async void fill_until_n_bytes_available (size_t minimum,
				Cancellable? cancellable) throws PlistServiceError, IOError {
			size_t available = input.get_available ();
			while (available < minimum) {
				if (input.get_buffer_size () < minimum)
					input.set_buffer_size (minimum);

				ssize_t n;
				try {
					n = yield input.fill_async ((ssize_t) (input.get_buffer_size () - available), Priority.DEFAULT,
						cancellable);
				} catch (GLib.Error e) {
					ensure_closed ();
					throw new PlistServiceError.CONNECTION_CLOSED ("%s", e.message);
				}

				if (n == 0) {
					ensure_closed ();
					throw new PlistServiceError.CONNECTION_CLOSED ("Connection closed");
				}

				available += n;
			}
		}

		private async void process_pending_output () {
			while (pending_output.len > 0) {
				uint8[] batch = pending_output.steal ();

				size_t bytes_written;
				try {
					yield output.write_all_async (batch, Priority.DEFAULT, io_cancellable, out bytes_written);
				} catch (GLib.Error e) {
					ensure_closed ();
					break;
				}
			}

			writing = false;
		}

		private void ensure_closed () {
			if (state == CLOSED)
				return;
			state = CLOSED;
			closed ();
		}
	}

	public errordomain PlistServiceError {
		CONNECTION_CLOSED,
		PROTOCOL
	}
}
