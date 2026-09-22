[CCode (gir_namespace = "FridaSuperSU", gir_version = "1.0")]
namespace Frida.SuperSU {
	public async Process spawn (string working_directory, string[] argv, string[]? envp = null, bool capture_output = false,
			Cancellable? cancellable = null) throws Error, IOError {
		try {
			var connection = yield Connection.open (cancellable);
			yield connection.write_strv (argv, cancellable);
			yield connection.write_strv ((envp != null) ? envp : Environ.get (), cancellable);
			yield connection.write_string (working_directory, cancellable);
			yield connection.write_string ("", cancellable);

			return new Process (connection, capture_output);
		} catch (GLib.Error e) {
			throw new Error.PROTOCOL ("Unable to spawn: %s", e.message);
		}
	}

	public class Process : Object {
		private Connection connection;

		public InputStream output {
			get {
				return output_in;
			}
		}
		private UnixInputStream output_in;
		private UnixOutputStream output_out;

		public int exit_status {
			get {
				return exit_promise.future.value;
			}
		}

		private Promise<int> exit_promise;

		private Cancellable io_cancellable = new Cancellable ();

		internal Process (Connection connection, bool capture_output) {
			this.connection = connection;

			if (capture_output) {
				var fds = new int[2];
				try {
					Unix.open_pipe (fds, 0);
					Unix.set_fd_nonblocking (fds[0], true);
					Unix.set_fd_nonblocking (fds[1], true);
				} catch (GLib.Error e) {
					assert_not_reached ();
				}

				output_in = new UnixInputStream (fds[0], true);
				output_out = new UnixOutputStream (fds[1], true);
			}

			exit_promise = new Promise<int> ();

			read_until_exit.begin ();
		}

		public async void detach (Cancellable? cancellable = null) throws IOError {
			io_cancellable.cancel ();

			yield wait (cancellable);
		}

		public async void wait (Cancellable? cancellable = null) throws IOError {
			try {
				yield exit_promise.future.wait_async (cancellable);
			} catch (Error e) {
			}
		}

		private async void read_until_exit () {
			try {
				bool done = false;
				int status = int.MIN;

				while (!done) {
					var command = yield connection.read_size (io_cancellable);
					switch (command) {
						case 1: {
							var data = yield connection.read_byte_array (io_cancellable);
							if (output_out != null)
								yield output_out.write_bytes_async (data, Priority.DEFAULT, io_cancellable);
							else
								stdout.write (data.get_data ());
							break;
						}

						case 2: {
							var data = yield connection.read_byte_array (io_cancellable);
							if (output_out != null)
								yield output_out.write_bytes_async (data, Priority.DEFAULT, io_cancellable);
							else
								stderr.write (data.get_data ());
							break;
						}

						case 3: {
							done = true;
							var type = yield connection.read_size (io_cancellable);
							if (type == 4)
								status = (int) yield connection.read_ssize (io_cancellable);
							break;
						}

						default:
							done = true;
							break;
					}
				}

				try {
					yield connection.close (null);
				} catch (IOError e) {
				}
				exit_promise.resolve (status);
			} catch (GLib.Error e) {
				try {
					yield connection.close (null);
				} catch (IOError e) {
				}
				exit_promise.reject (e);
			}
		}
	}

	private class Connection : Object, AsyncInitable {
		private SocketConnection? connection;
		private DataInputStream? input;
		private DataOutputStream? output;
		private Socket? socket;

		public static async Connection open (Cancellable? cancellable = null) throws Error, IOError {
			var connection = new Connection ();

			try {
				yield connection.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return connection;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			string address = "eu.chainfire.supersu";
			while (true) {
				string? redirect = yield establish (address, cancellable);
				if (redirect == null)
					break;

				address = redirect;
			}

			return true;
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (connection != null) {
				try {
					yield connection.close_async (Priority.DEFAULT, cancellable);
				} catch (IOError e) {
					if (e is IOError.CANCELLED)
						throw (IOError) e;
				}
				connection = null;
			}
			input = null;
			output = null;
		}

		private async string? establish (string address, Cancellable? cancellable) throws Error, IOError {
			try {
				var client = new SocketClient ();
				connection = yield client.connect_async (new UnixSocketAddress.with_type (address, -1, ABSTRACT),
					cancellable);

				input = new DataInputStream (connection.get_input_stream ());
				input.set_byte_order (DataStreamByteOrder.HOST_ENDIAN);

				output = new DataOutputStream (connection.get_output_stream ());
				output.set_byte_order (DataStreamByteOrder.HOST_ENDIAN);

				socket = connection.get_socket ();

				write_size (0);
				yield write_credentials (cancellable);

				var redirect = yield read_string (cancellable);
				if (redirect.length > 0)
					yield close (cancellable);

				return redirect.length > 0 ? redirect : null;
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("SuperSU is not installed (%s)".printf (e.message));
			}
		}

		public async string read_string (Cancellable? cancellable) throws GLib.Error {
			var size = yield read_size (cancellable);
			if (size == 0)
				return "";

			var data_buf = new uint8[size + 1];
			size_t bytes_read;
			yield input.read_all_async (data_buf[0:size], Priority.DEFAULT, cancellable, out bytes_read);
			if (bytes_read != size)
				throw new IOError.FAILED ("Unable to read string");
			data_buf[size] = 0;

			char * v = data_buf;
			return (string) v;
		}

		public async void write_string (string str, Cancellable? cancellable) throws GLib.Error {
			write_size (str.length);

			if (str.length > 0) {
				unowned uint8[] buf = (uint8[]) str;
				yield output.write_all_async (buf[0:str.length], Priority.DEFAULT, cancellable, null);
			}
		}

		public async void write_strv (string[] strv, Cancellable? cancellable) throws GLib.Error {
			write_size (strv.length);
			foreach (string s in strv)
				yield write_string (s, cancellable);
		}

		public async Bytes read_byte_array (Cancellable? cancellable) throws GLib.Error {
			var size = yield read_size (cancellable);
			if (size == 0)
				return new Bytes (new uint8[0]);

			var data = yield input.read_bytes_async (size, Priority.DEFAULT, cancellable);
			if (data.length != size)
				throw new IOError.FAILED ("Unable to read byte array");

			return data;
		}

		public async size_t read_size (Cancellable? cancellable) throws GLib.Error {
			yield prepare_to_read (sizeof (uint32), cancellable);

			return input.read_uint32 ();
		}

		public async ssize_t read_ssize (Cancellable? cancellable) throws GLib.Error {
			yield prepare_to_read (sizeof (int32), cancellable);

			return input.read_int32 ();
		}

		public void write_size (size_t size) throws GLib.Error {
			output.put_uint32 ((uint32) size);
		}

		private async void prepare_to_read (size_t required, Cancellable? cancellable) throws GLib.Error {
			while (true) {
				size_t available = input.get_available ();
				if (available >= required)
					return;
				ssize_t n = yield input.fill_async ((ssize_t) (required - available), Priority.DEFAULT, cancellable);
				if (n == 0)
					throw new Error.TRANSPORT ("Disconnected");
			}
		}

		private async void write_credentials (Cancellable? cancellable) throws GLib.Error {
			yield output.flush_async (Priority.DEFAULT, cancellable);

			var parameters = new MemoryOutputStream.resizable ();
			var p = new DataOutputStream (parameters);
			p.set_byte_order (DataStreamByteOrder.HOST_ENDIAN);
			p.put_uint32 (Posix.getpid ());
			p.put_uint32 ((uint32) Posix.getuid ());
			p.put_uint32 ((uint32) Posix.getgid ());

			var vector = OutputVector ();
			vector.buffer = parameters.data;
			vector.size = parameters.data_size;

			var vectors = new OutputVector[] { vector };
			var messages = new SocketControlMessage[] { new UnixCredentialsMessage () };
			socket.send_message (null, vectors, messages, SocketMsgFlags.NONE);
		}
	}
}
