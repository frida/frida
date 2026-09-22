[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.DebuggerMappings {
	internal async void spray_pages (GDB.Client gdb, uint64 address, size_t size, size_t page_size, Cancellable? cancellable)
			throws Error, IOError {
		int n_pages = (int) (size / page_size);
		if (n_pages == 0)
			throw new Error.INVALID_ARGUMENT ("Expected one or more pages");

		var builder = gdb.make_packet_builder_sized (64);
		Future<bool>? last_write = null;

		for (int i = 0; i < n_pages; i += 2) {
			bool is_last = (i + 2 >= n_pages);

			uint64 last_byte_addr = address + ((uint64) i * page_size) + page_size - 1;
			size_t write_count = is_last ? 1 : 2;

			builder
				.append_c ('M')
				.append_address (last_byte_addr)
				.append_c (',')
				.append_size (write_count)
				.append_c (':')
				.append_escaped ((write_count == 2) ? "0000" : "00");

			var request = new Promise<bool> ();
			gdb.perform_execute.begin (builder.build (), cancellable, request);

			builder.reset ();

			if (is_last)
				last_write = request.future;
		}

		yield last_write.wait_async (cancellable);
	}

	internal async void handle_page_plan (GDB.Client gdb, uint64 address, size_t size, size_t page_size, Cancellable? cancellable)
			throws Error, IOError {
		var plan = new BufferReader (yield gdb.read_buffer (address, size, cancellable));

		var n_blocks = plan.read_uint32 ();
		if (n_blocks == 0)
			return;

		var builder = gdb.make_packet_builder_sized (64);
		Future<bool>? last_write = null;

		for (uint32 i = 0; i != n_blocks; i++) {
			uint64 start = plan.read_pointer ();
			uint32 count = plan.read_uint32 ();

			for (size_t j = 0; j < count; j += 2) {
				bool is_last = (j + 2 >= count) && (i == n_blocks - 1);

				uint64 last_byte_addr = start + ((uint64) j * page_size) + page_size - 1;
				size_t write_count = size_t.min (2, count - j);

				builder
					.append_c ('M')
					.append_address (last_byte_addr)
					.append_c (',')
					.append_size (write_count)
					.append_c (':')
					.append_hexbyte (plan.read_uint8 ());

				if (write_count == 2)
					builder.append_hexbyte (plan.read_uint8 ());

				var request = new Promise<bool> ();
				gdb.perform_execute.begin (builder.build (), cancellable, request);

				builder.reset ();

				if (is_last)
					last_write = request.future;
			}
		}

		yield last_write.wait_async (cancellable);
	}
}
