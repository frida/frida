[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	private sealed class CModule : Object {
		public signal void console_output (string message);

		public Gee.List<Export> exports {
			get;
			default = new Gee.ArrayList<Export> ();
		}

		public class Export {
			public string name;
			public uint64 address;

			internal Export (string name, uint64 address) {
				this.name = name;
				this.address = address;
			}
		}

		private Gum.ElfModule elf;
		private Allocation allocation;
		private Callback console_log_callback;

		public async CModule.from_blob (Bytes blob, Machine machine, Allocator allocator, Cancellable? cancellable)
				throws Error, IOError {
			try {
				elf = new Gum.ElfModule.from_blob (blob);
			} catch (Gum.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			var raw_elf = new Bytes.static (elf.get_file_data ());

			size_t page_size = yield machine.query_page_size (cancellable);

			allocation = yield inject_elf (elf, raw_elf, page_size, machine, allocator, uploaded => {}, cancellable);

			uint64 base_va = allocation.virtual_address;
			uint64 console_log_trap = 0;
			elf.enumerate_symbols (e => {
				if (e.name == "")
					return true;

				if (e.name == "_console_log") {
					console_log_trap = base_va + e.address;
					return true;
				}

				exports.add (new Export (e.name, base_va + e.address));

				return true;
			});

			if (console_log_trap != 0) {
				var handler = new ConsoleLogHandler (machine.gdb);
				handler.output.connect (on_console_output);
				console_log_callback = yield new Callback (console_log_trap, handler, machine, cancellable);
			}
		}

		private void on_console_output (string message) {
			console_output (message);
		}
	}
}
