[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	private class ConsoleLogHandler : Object, CallbackHandler {
		public signal void output (string message);

		public uint arity {
			get { return 2; }
		}

		private GDB.Client gdb;

		public ConsoleLogHandler (GDB.Client gdb) {
			this.gdb = gdb;
		}

		public async uint64 handle_invocation (uint64[] args, CallFrame frame, Cancellable? cancellable)
				throws Error, IOError {
			var message = args[0];
			var len = (long) args[1];

			Bytes str_bytes = yield gdb.read_byte_array (message, len, cancellable);
			unowned uint8[] str_data = str_bytes.get_data ();
			unowned string str_raw = (string) str_data;
			string str = str_raw.substring (0, len);

			output (str);

			return 0;
		}
	}
}
