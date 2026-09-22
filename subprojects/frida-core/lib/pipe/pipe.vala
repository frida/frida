namespace Frida {
	public sealed class PipeTransport : Object {
		public string local_address {
			get;
			construct;
		}

		public string remote_address {
			get;
			construct;
		}

		public void * _backend;

		public PipeTransport () throws Error {
			string local_address, remote_address;
			var backend = _create_backend (out local_address, out remote_address);
			Object (local_address: local_address, remote_address: remote_address);
			_backend = backend;
		}

		~PipeTransport () {
			_destroy_backend (_backend);
		}

		public extern static void set_temp_directory (string path);

		public extern static void * _create_backend (out string local_address, out string remote_address) throws Error;
		public extern static void _destroy_backend (void * backend);
	}

	namespace Pipe {
		public Future<IOStream> open (string address, Cancellable? cancellable) {
#if WINDOWS
			return WindowsPipe.open (address, cancellable);
#elif MACOS || IOS || TVOS
			return DarwinPipe.open (address, cancellable);
#else
			return UnixPipe.open (address, cancellable);
#endif
		}
	}

#if WINDOWS
	public sealed class WindowsPipe : IOStream {
		public string address {
			get;
			construct;
		}

		public void * backend {
			get;
			construct;
		}

		public MainContext main_context {
			get;
			construct;
		}

		public override InputStream input_stream {
			get {
				return input;
			}
		}

		public override OutputStream output_stream {
			get {
				return output;
			}
		}

		private InputStream input;
		private OutputStream output;

		public static Future<WindowsPipe> open (string address, Cancellable? cancellable) {
			var promise = new Promise<WindowsPipe> ();

			try {
				var pipe = new WindowsPipe (address);
				promise.resolve (pipe);
			} catch (IOError e) {
				promise.reject (e);
			}

			return promise.future;
		}

		public WindowsPipe (string address) throws IOError {
			var backend = _create_backend (address);

			Object (
				address: address,
				backend: backend,
				main_context: MainContext.get_thread_default ()
			);
		}

		construct {
			input = _make_input_stream (backend);
			output = _make_output_stream (backend);
		}

		~WindowsPipe () {
			_destroy_backend (backend);
		}

		public override bool close (Cancellable? cancellable = null) throws IOError {
			return _close_backend (backend);
		}

		protected extern static void * _create_backend (string address) throws IOError;
		protected extern static void _destroy_backend (void * backend);
		protected extern static bool _close_backend (void * backend) throws IOError;

		protected extern static InputStream _make_input_stream (void * backend);
		protected extern static OutputStream _make_output_stream (void * backend);
	}
#elif MACOS || IOS || TVOS
	namespace DarwinPipe {
		public static Future<IOStream> open (string address, Cancellable? cancellable) {
			var promise = new Promise<IOStream> ();

			try {
				var fd = _consume_stashed_file_descriptor (address);
				IOStream stream;
				if (Gum.Darwin.query_hardened ()) {
					var input = new UnixInputStream (fd, true);
					var output = new UnixOutputStream (fd, false);
					stream = new SimpleIOStream (input, output);
				} else {
					var socket = new Socket.from_fd (fd);
					stream = SocketConnection.factory_create_connection (socket);
				}
				promise.resolve (stream);
			} catch (GLib.Error e) {
				promise.reject (e);
			}

			return promise.future;
		}

		public extern int _consume_stashed_file_descriptor (string address) throws Error;
	}
#else
	namespace UnixPipe {
		public static Future<SocketConnection> open (string address, Cancellable? cancellable) {
			var promise = new Promise<SocketConnection> ();

			MatchInfo info;
			bool valid_address = /^pipe:role=(.+?),path=(.+?)$/.match (address, 0, out info);
			assert (valid_address);
			string role = info.fetch (1);
			string path = info.fetch (2);

			try {
				UnixSocketAddressType type = UnixSocketAddress.abstract_names_supported ()
					? UnixSocketAddressType.ABSTRACT
					: UnixSocketAddressType.PATH;
				var server_address = new UnixSocketAddress.with_type (path, -1, type);

				if (role == "server") {
					var socket = new Socket (SocketFamily.UNIX, SocketType.STREAM, SocketProtocol.DEFAULT);
					UnixSocket.tune_buffer_sizes (socket.get_fd ());
					socket.bind (server_address, true);
					socket.listen ();

					Posix.chmod (path, Posix.S_IRUSR | Posix.S_IWUSR | Posix.S_IRGRP | Posix.S_IWGRP | Posix.S_IROTH | Posix.S_IWOTH);
#if ANDROID
					SELinux.setfilecon (path, "u:object_r:frida_file:s0");
#endif

					establish_server.begin (socket, server_address, promise, cancellable);
				} else {
					establish_client.begin (server_address, promise, cancellable);
				}
			} catch (GLib.Error e) {
				promise.reject (e);
			}

			return promise.future;
		}

		private async void establish_server (Socket socket, UnixSocketAddress address, Promise<SocketConnection> promise,
				Cancellable? cancellable) {
			var listener = new SocketListener ();
			try {
				listener.add_socket (socket, null);

				var connection = yield listener.accept_async (cancellable);
				promise.resolve (connection);
			} catch (GLib.Error e) {
				promise.reject (e);
			} finally {
				if (address.get_address_type () == PATH)
					Posix.unlink (address.get_path ());
				listener.close ();
			}
		}

		private async void establish_client (UnixSocketAddress address, Promise<SocketConnection> promise, Cancellable? cancellable) {
			var client = new SocketClient ();
			try {
				var connection = yield client.connect_async (address, cancellable);
				UnixSocket.tune_buffer_sizes (connection.get_socket ().get_fd ());
				promise.resolve (connection);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}
	}
#endif
}
