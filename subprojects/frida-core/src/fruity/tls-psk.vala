[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class TlsPskClientStream : IOStream, AsyncInitable {
		public IOStream base_stream {
			get;
			construct;
		}

		public string psk_identity {
			get;
			construct;
		}

		public Bytes psk {
			get;
			construct;
		}

		public override InputStream input_stream {
			get {
				return _input_stream;
			}
		}

		public override OutputStream output_stream {
			get {
				return _output_stream;
			}
		}

		private OpenSSL.SSLContext ssl_ctx;
		private OpenSSL.SSL ssl;
		private void * rbio;
		private void * wbio;

		private PollableInputStream base_pollable_input;
		private PollableOutputStream base_pollable_output;

		private TlsPskInputStream _input_stream;
		private TlsPskOutputStream _output_stream;

		private const int IO_CHUNK_SIZE = 64 * 1024;
		private const string CIPHER_LIST =
			"PSK-AES128-GCM-SHA256:PSK-AES256-GCM-SHA384:PSK-AES256-CBC-SHA384:PSK-AES128-CBC-SHA256";

		public static async TlsPskClientStream open (IOStream base_stream, string psk_identity, Bytes psk,
				Cancellable? cancellable) throws Error, IOError {
			var stream = new TlsPskClientStream (base_stream, psk_identity, psk);

			try {
				yield stream.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return stream;
		}

		private TlsPskClientStream (IOStream base_stream, string psk_identity, Bytes psk) {
			Object (base_stream: base_stream, psk_identity: psk_identity, psk: psk);
		}

		construct {
			var raw_input = base_stream.get_input_stream ();
			var raw_output = base_stream.get_output_stream ();
			assert (raw_input is PollableInputStream && raw_output is PollableOutputStream);
			base_pollable_input = (PollableInputStream) raw_input;
			base_pollable_output = (PollableOutputStream) raw_output;

			_input_stream = new TlsPskInputStream (this);
			_output_stream = new TlsPskOutputStream (this);

			ssl_ctx = new OpenSSL.SSLContext (OpenSSL.SSLMethod.tls_client ());

			ssl = new OpenSSL.SSL (ssl_ctx);
			ssl.set_app_data (this);
			ssl.set_connect_state ();
			ssl.set_cipher_list (CIPHER_LIST);
			ssl.set_psk_client_callback ((ssl, hint, identity, psk) => {
				TlsPskClientStream * self = ssl.get_app_data ();

				unowned uint8[] ident_bytes = self->psk_identity.data;
				Memory.copy (identity, ident_bytes, ident_bytes.length);

				unowned uint8[] key = self->psk.get_data ();
				Memory.copy (psk, key, key.length);

				return key.length;
			});

			rbio = OpenSSL.BasicIO.raw_new (OpenSSL.BasicIOMethod.memory ());
			wbio = OpenSSL.BasicIO.raw_new (OpenSSL.BasicIOMethod.memory ());
			ssl.set_bio (rbio, wbio);
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			while (true) {
				int ret = ssl.do_handshake ();

				yield handshake_drain_wbio (cancellable);

				if (ret == 1)
					return true;

				int err = ssl.get_error (ret);
				if (err == OpenSSL.SSLErrorCode.WANT_READ) {
					if (!yield handshake_feed_rbio (cancellable))
						throw new Error.TRANSPORT ("Connection closed during TLS handshake");
				} else if (err != OpenSSL.SSLErrorCode.WANT_WRITE) {
					throw new Error.PROTOCOL ("TLS handshake failed: %s", openssl_error_string ());
				}
			}
		}

		private async void handshake_drain_wbio (Cancellable? cancellable) throws Error, IOError {
			while (OpenSSL.BasicIO.raw_ctrl_pending (wbio) > 0) {
				var buf = new uint8[IO_CHUNK_SIZE];
				int n = OpenSSL.BasicIO.raw_read (wbio, buf);
				try {
					yield ((OutputStream) base_pollable_output).write_all_async (buf[0:n], Priority.DEFAULT,
						cancellable, null);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("%s", e.message);
				}
			}
		}

		private async bool handshake_feed_rbio (Cancellable? cancellable) throws Error, IOError {
			var buf = new uint8[IO_CHUNK_SIZE];
			ssize_t n;
			try {
				n = yield ((InputStream) base_pollable_input).read_async (buf, Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}
			if (n == 0)
				return false;
			OpenSSL.BasicIO.raw_write (rbio, buf[:n]);
			return true;
		}

		internal ssize_t do_read_nonblocking (uint8[] buffer) throws GLib.Error {
			while (true) {
				int n = ssl.read (buffer);
				if (n > 0)
					return n;

				int err = ssl.get_error (n);
				if (err == OpenSSL.SSLErrorCode.ZERO_RETURN)
					return 0;
				if (err != OpenSSL.SSLErrorCode.WANT_READ)
					throw new IOError.FAILED ("TLS read error: %s", openssl_error_string ());

				var tmp = new uint8[IO_CHUNK_SIZE];
				ssize_t r = base_pollable_input.read_nonblocking (tmp);
				if (r == 0)
					return 0;
				OpenSSL.BasicIO.raw_write (rbio, tmp[:r]);
			}
		}

		internal ssize_t do_read_blocking (uint8[] buffer, Cancellable? cancellable) throws IOError {
			while (true) {
				int n = ssl.read (buffer);
				if (n > 0)
					return n;

				int err = ssl.get_error (n);
				if (err == OpenSSL.SSLErrorCode.ZERO_RETURN)
					return 0;
				if (err != OpenSSL.SSLErrorCode.WANT_READ)
					throw new IOError.FAILED ("TLS read error: %s", openssl_error_string ());

				var tmp = new uint8[IO_CHUNK_SIZE];
				ssize_t r = ((InputStream) base_pollable_input).read (tmp, cancellable);
				if (r == 0)
					return 0;
				OpenSSL.BasicIO.raw_write (rbio, tmp[:r]);
			}
		}

		internal bool do_is_readable () {
			return ssl.pending () > 0 || base_pollable_input.is_readable ();
		}

		internal PollableSource do_create_input_source (Cancellable? cancellable) {
			return new PollableSource.full (_input_stream,
				base_pollable_input.create_source (cancellable), cancellable);
		}

		internal ssize_t do_write_nonblocking (uint8[] buffer) throws GLib.Error {
			if (!flush_wbio_nonblocking ())
				throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");

			int n = ssl.write (buffer);
			if (n <= 0) {
				int err = ssl.get_error (n);
				if (err == OpenSSL.SSLErrorCode.WANT_WRITE || err == OpenSSL.SSLErrorCode.WANT_READ)
					throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
				throw new IOError.FAILED ("TLS write error: %s", openssl_error_string ());
			}

			flush_wbio_nonblocking ();

			return n;
		}

		internal ssize_t do_write_blocking (uint8[] buffer, Cancellable? cancellable) throws IOError {
			try {
				flush_wbio_blocking (cancellable);
			} catch (GLib.Error e) {
				throw new IOError.FAILED ("%s", e.message);
			}

			int n = ssl.write (buffer);
			if (n <= 0)
				throw new IOError.FAILED ("TLS write error: %s", openssl_error_string ());

			try {
				flush_wbio_blocking (cancellable);
			} catch (GLib.Error e) {
				throw new IOError.FAILED ("%s", e.message);
			}

			return n;
		}

		internal bool do_is_writable () {
			return base_pollable_output.is_writable ();
		}

		internal PollableSource do_create_output_source (Cancellable? cancellable) {
			return new PollableSource.full (_output_stream, base_pollable_output.create_source (cancellable), cancellable);
		}

		private bool flush_wbio_nonblocking () throws GLib.Error {
			while (true) {
				unowned uint8[] data;
				long len = OpenSSL.BasicIO.raw_get_mem_data (wbio, out data);
				if (len == 0)
					return true;
				data.length = (int) len;

				ssize_t written;
				try {
					written = base_pollable_output.write_nonblocking (data);
				} catch (IOError.WOULD_BLOCK e) {
					return false;
				}

				var scratch = new uint8[written];
				OpenSSL.BasicIO.raw_read (wbio, scratch);
			}
		}

		private void flush_wbio_blocking (Cancellable? cancellable) throws GLib.Error {
			while (true) {
				unowned uint8[] data;
				long len = OpenSSL.BasicIO.raw_get_mem_data (wbio, out data);
				if (len == 0)
					return;
				data.length = (int) len;

				size_t written;
				((OutputStream) base_pollable_output).write_all (data, out written, cancellable);

				var scratch = new uint8[written];
				OpenSSL.BasicIO.raw_read (wbio, scratch);
			}
		}

		private static string openssl_error_string () {
			var buf = new char[256];
			OpenSSL.Error.error_string_n (OpenSSL.Error.get_error (), buf);
			return (string) buf;
		}

		private class TlsPskInputStream : InputStream, PollableInputStream {
			public weak TlsPskClientStream parent {
				get;
				construct;
			}

			public TlsPskInputStream (TlsPskClientStream parent) {
				Object (parent: parent);
			}

			public override ssize_t read (uint8[] buffer, Cancellable? cancellable = null) throws IOError {
				return parent.do_read_blocking (buffer, cancellable);
			}

			public override bool close (Cancellable? cancellable = null) throws IOError {
				return true;
			}

			public bool can_poll () {
				return true;
			}

			public bool is_readable () {
				return parent.do_is_readable ();
			}

			public PollableSource create_source (Cancellable? cancellable) {
				return parent.do_create_input_source (cancellable);
			}

			public ssize_t read_nonblocking_fn (uint8[] buffer) throws GLib.Error {
				return parent.do_read_nonblocking (buffer);
			}
		}

		private class TlsPskOutputStream : OutputStream, PollableOutputStream {
			public weak TlsPskClientStream parent {
				get;
				construct;
			}

			public TlsPskOutputStream (TlsPskClientStream parent) {
				Object (parent: parent);
			}

			public override ssize_t write (uint8[] buffer, Cancellable? cancellable = null) throws IOError {
				return parent.do_write_blocking (buffer, cancellable);
			}

			public override bool close (Cancellable? cancellable = null) throws IOError {
				return true;
			}

			public override bool flush (Cancellable? cancellable = null) throws GLib.Error {
				return true;
			}

			public bool can_poll () {
				return true;
			}

			public bool is_writable () {
				return parent.do_is_writable ();
			}

			public PollableSource create_source (Cancellable? cancellable) {
				return parent.do_create_output_source (cancellable);
			}

			public ssize_t write_nonblocking_fn (uint8[]? buffer) throws GLib.Error {
				return parent.do_write_nonblocking (buffer);
			}

			public PollableReturn writev_nonblocking_fn (OutputVector[] vectors, out size_t bytes_written) throws GLib.Error {
				assert_not_reached ();
			}
		}
	}
}
