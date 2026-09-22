namespace Frida.GDBTest {
	public static void add_tests () {
		GLib.Test.add_func ("/GDB/register-access-uses-thread-suffix", () => {
			var h = new Harness ((h) => register_access_uses_thread_suffix.begin (h as Harness));
			h.run ();
		});
	}

	private static async void register_access_uses_thread_suffix (Harness h) {
		FakeStub? stub = null;
		try {
			stub = yield FakeStub.open ();

			var client = yield ThreadSuffixClient.open (stub.client_stream);

			var thread = client.exception.thread;
			var x0 = yield thread.read_register ("x0");
			assert_true (x0 == 0);

			yield client.close ();
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			if (stub != null)
				stub.stop ();
		}

		h.done ();
	}

	private class FakeStub : Object {
		public IOStream client_stream {
			get;
			private set;
		}

		private SocketService service;
		private Cancellable cancellable = new Cancellable ();

		private const string TARGET_XML =
			"<?xml version=\"1.0\"?>" +
			"<target version=\"1.0\">" +
			"<architecture>aarch64</architecture>" +
			"<feature name=\"org.gnu.gdb.aarch64.core\">" +
			"<reg name=\"x0\" bitsize=\"64\" regnum=\"0\"/>" +
			"<reg name=\"pc\" bitsize=\"64\" regnum=\"32\"/>" +
			"</feature>" +
			"</target>";

		public static async FakeStub open () throws Error, IOError {
			var stub = new FakeStub ();
			yield stub.init ();
			return stub;
		}

		private async void init () throws Error, IOError {
			service = new SocketService ();
			uint16 port;
			try {
				port = service.add_any_inet_port (null);
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			service.incoming.connect ((connection) => {
				serve.begin (connection);
				return true;
			});
			service.start ();

			var socket_client = new SocketClient ();
			try {
				client_stream = yield socket_client.connect_to_host_async ("127.0.0.1", port, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}
		}

		public void stop () {
			cancellable.cancel ();
			service.stop ();
		}

		private async void serve (IOStream connection) {
			var input = connection.get_input_stream ();
			var output = connection.get_output_stream ();
			try {
				string? request;
				while ((request = yield read_packet (input)) != null) {
					yield write_ack (output);
					yield write_packet (output, compute_reply (request));
				}
			} catch (GLib.Error e) {
			}
		}

		private string compute_reply (string request) {
			if (request.has_prefix ("qSupported"))
				return "PacketSize=1000;qXfer:features:read+";
			if (request.has_prefix ("qXfer:features:read:target.xml:"))
				return "l" + TARGET_XML;
			if (request == "qAttached")
				return "1";
			if (request == "?")
				return "T05thread:1;";
			if (request == "qC")
				return "QC1";
			if (request == "QThreadSuffixSupported")
				return "OK";
			if (request[0] == 'p')
				return is_thread_suffixed (request) ? "0000000000000000" : "E03";
			return "";
		}

		private bool is_thread_suffixed (string request) {
			return ";thread:" in request;
		}

		private async string? read_packet (InputStream input) throws GLib.Error {
			while (true) {
				int c = yield read_byte (input);
				if (c < 0)
					return null;
				if (c != '$')
					continue;

				var payload = new StringBuilder ();
				while (true) {
					c = yield read_byte (input);
					if (c < 0)
						return null;
					if (c == '#')
						break;
					payload.append_c ((char) c);
				}
				yield read_byte (input);
				yield read_byte (input);
				return payload.str;
			}
		}

		private async int read_byte (InputStream input) throws GLib.Error {
			uint8 buf[1];
			size_t n;
			yield input.read_all_async (buf, Priority.DEFAULT, cancellable, out n);
			if (n == 0)
				return -1;
			return buf[0];
		}

		private async void write_ack (OutputStream output) throws GLib.Error {
			yield write_all (output, "+".data);
		}

		private async void write_packet (OutputStream output, string payload) throws GLib.Error {
			uint8 checksum = 0;
			for (int i = 0; i != payload.length; i++)
				checksum += (uint8) payload[i];
			var frame = "$%s#%02x".printf (payload, checksum);
			yield write_all (output, frame.data);
		}

		private async void write_all (OutputStream output, uint8[] data) throws GLib.Error {
			size_t written;
			yield output.write_all_async (data, Priority.DEFAULT, cancellable, out written);
		}
	}

	private class ThreadSuffixClient : GDB.Client {
		private ThreadSuffixClient (IOStream stream) {
			Object (stream: stream);
		}

		public static new async ThreadSuffixClient open (IOStream stream, Cancellable? cancellable = null)
				throws Error, IOError {
			var client = new ThreadSuffixClient (stream);
			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}
			return client;
		}

		protected override async void detect_vendor_features (Cancellable? cancellable) throws Error, IOError {
		}

		protected override async void enable_extensions (Cancellable? cancellable) throws Error, IOError {
			yield execute_simple ("QThreadSuffixSupported", cancellable);
			supported_features.add ("thread-suffix");
		}
	}

	private class Harness : Frida.Test.AsyncHarness {
		public Harness (owned Frida.Test.AsyncHarness.TestSequenceFunc func) {
			base ((owned) func);
		}
	}
}
