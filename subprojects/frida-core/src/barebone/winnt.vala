[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public static async WinNtLayout collect_winnt_layout (Machine machine, Cancellable? cancellable)
			throws Error, IOError {
		var modules = new Gee.ArrayList<ModuleInfo> ();
		var symbols = new Gee.ArrayList<SymbolInfo> ();

		Shape shape = Shape.of (machine.gdb);

		Anchors anchors = yield find_anchors (machine, shape, cancellable);

		foreach (LoadedModule module in yield read_loaded_modules (machine, anchors.module_list, shape, cancellable)) {
			modules.add (new ModuleInfo () {
				name = module.name,
				version = "",
				offset = module.base_address,
				size = module.size,
			});

			yield add_export_symbols (machine, module, symbols, cancellable);
		}

		if (anchors.process_list_head != 0) {
			symbols.add (new SymbolInfo () {
				name = PROCESS_LIST_HEAD,
				offset = anchors.process_list_head,
				symbol_type = 0xf,
				section = 0x10,
			});
		}

		return new WinNtLayout (modules, symbols);
	}

	public sealed class WinNtLayout : Object {
		public Gee.List<ModuleInfo> modules {
			get;
			construct;
		}

		public Gee.List<SymbolInfo> symbols {
			get;
			construct;
		}

		public WinNtLayout (Gee.List<ModuleInfo> modules, Gee.List<SymbolInfo> symbols) {
			Object (modules: modules, symbols: symbols);
		}
	}

	private static async Anchors find_anchors (Machine machine, Shape shape, Cancellable? cancellable)
			throws Error, IOError {
		if (shape.machine_type == IMAGE_FILE_MACHINE_ARM64)
			return yield read_anchors_from_kernel (machine, shape, cancellable);

		uint64 version_block = yield find_version_block (machine, shape, cancellable);
		uint64 module_list = yield read_loaded_module_list (machine, version_block, shape, cancellable);
		uint64 process_list_head = yield read_process_list_head (machine, version_block, shape, cancellable);

		return new Anchors () {
			module_list = module_list,
			process_list_head = process_list_head,
		};
	}

	private class Anchors {
		public uint64 module_list;
		public uint64 process_list_head;
	}

	private static async Anchors read_anchors_from_kernel (Machine machine, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		uint64 kernel = yield find_kernel_image (machine, shape, cancellable);

		uint64 module_list = yield find_export (machine, kernel, LOADED_MODULE_LIST, cancellable);
		if (!is_kernel_address (module_list, shape))
			throw new Error.NOT_SUPPORTED ("Unable to find the loaded module list");
		uint64 process_list_head = yield find_process_list_head (machine, kernel, shape, cancellable);

		return new Anchors () {
			module_list = module_list,
			process_list_head = process_list_head,
		};
	}

	private static async uint64 find_kernel_image (Machine machine, Shape shape, Cancellable? cancellable)
			throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		for (uint attempt = 0; attempt != MAX_CATCH_ATTEMPTS; attempt++) {
			if (yield stopped_in_kernel_mode (gdb, cancellable)) {
				uint64 region = yield gdb.exception.thread.read_register (KERNEL_PCR_REGISTER, cancellable);
				if (is_kernel_address (region, shape)) {
					uint64 image = yield find_image_below_pointers (machine, region, shape, cancellable);
					if (image != 0)
						return image;
				}
			}

			yield catch_processor_again (gdb, cancellable);
		}

		throw new Error.NOT_SUPPORTED ("Unable to find the kernel image");
	}

	private static async uint64 find_image_below_pointers (Machine machine, uint64 region, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;
		Buffer page = gdb.make_buffer (yield gdb.read_byte_array (region, PCR_SCAN_SIZE, cancellable));

		var visited = new Gee.HashSet<uint64?> ((n) => (uint) (*(uint64 *) n), (a, b) => *(uint64 *) a == *(uint64 *) b);
		uint budget = MAX_IMAGE_PROBES;
		for (size_t offset = 0; offset != PCR_SCAN_SIZE; offset += shape.pointer_size) {
			uint64 candidate = read_pointer (page, offset, shape);
			if (!is_kernel_address (candidate, shape))
				continue;

			uint64 image = candidate - (candidate % KERNEL_IMAGE_ALIGNMENT);
			for (uint step = 0; step != MAX_IMAGE_STEPS && budget != 0; step++, image -= KERNEL_IMAGE_ALIGNMENT) {
				if (!visited.add (image))
					break;
				budget--;

				uint64 module_list;
				try {
					module_list = yield find_export (machine, image, LOADED_MODULE_LIST,
						cancellable);
				} catch (Error e) {
					continue;
				}

				if (module_list != 0)
					return image;
			}
		}

		return 0;
	}

	private static async uint64 find_process_list_head (Machine machine, uint64 kernel, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 holder = yield find_export (machine, kernel, INITIAL_PROCESS, cancellable);
		if (!is_kernel_address (holder, shape))
			return 0;

		uint64 process = read_pointer (gdb.make_buffer (yield gdb.read_byte_array (holder, shape.pointer_size,
			cancellable)), 0, shape);
		if (!is_kernel_address (process, shape))
			return 0;

		uint32 image_size = yield read_image_size (machine, kernel, cancellable);
		Buffer body = gdb.make_buffer (yield gdb.read_byte_array (process, PROCESS_SCAN_SIZE, cancellable));

		for (size_t offset = 0; offset != PROCESS_SCAN_SIZE - (2 * (size_t) shape.pointer_size);
				offset += shape.pointer_size) {
			uint64 node = process + offset;
			if (!(yield links_back_to (machine, node, read_pointer (body, offset, shape), shape, cancellable)))
				continue;

			uint64 head = yield walk_to_image (machine, node, kernel, image_size, shape, cancellable);
			if (head != 0)
				return head;
		}

		return 0;
	}

	private static async bool links_back_to (Machine machine, uint64 node, uint64 forward, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		if (!is_kernel_address (forward, shape))
			return false;

		GDB.Client gdb = machine.gdb;
		Buffer neighbour;
		try {
			neighbour = gdb.make_buffer (yield gdb.read_byte_array (forward, 2 * shape.pointer_size,
				cancellable));
		} catch (Error e) {
			return false;
		}

		return read_pointer (neighbour, shape.pointer_size, shape) == node;
	}

	private static async uint64 walk_to_image (Machine machine, uint64 node, uint64 image, uint32 image_size,
			Shape shape, Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 entry = node;
		for (uint step = 0; step != MAX_PROCESSES; step++) {
			Buffer links;
			try {
				links = gdb.make_buffer (yield gdb.read_byte_array (entry, shape.pointer_size, cancellable));
			} catch (Error e) {
				return 0;
			}

			entry = read_pointer (links, 0, shape);
			if (!is_kernel_address (entry, shape))
				return 0;
			if (entry == node)
				return 0;
			if (entry >= image && entry < image + image_size)
				return entry;
		}

		return 0;
	}

	// The processor control region points to the block that a kernel debugger uses. That block
	// gives the kernel and its module list, which is sufficient to find the other data.
	private static async uint64 find_version_block (Machine machine, Shape shape, Cancellable? cancellable)
			throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 pcr_base = yield find_processor_control_region (machine, shape, cancellable);

		Buffer pcr = gdb.make_buffer (yield gdb.read_byte_array (pcr_base,
			shape.version_block + shape.pointer_size, cancellable));
		uint64 version_block = read_pointer (pcr, shape.version_block, shape);
		if (!is_kernel_address (version_block, shape))
			throw new Error.NOT_SUPPORTED ("Unable to find the kernel debugger version block");

		Buffer v = gdb.make_buffer (yield gdb.read_byte_array (version_block, VERSION_BLOCK_SIZE, cancellable));
		if (v.read_uint16 (MACHINE_TYPE_OFFSET) != shape.machine_type)
			throw new Error.NOT_SUPPORTED ("Kernel is not the architecture the stub reports");

		return version_block;
	}

	// A 32-bit kernel keeps this block at a constant address. A 64-bit kernel selects the address
	// and points GS to it in kernel mode. In user mode GS points to the block of the current
	// thread, because the processor exchanges the two values.
	private static async uint64 find_processor_control_region (Machine machine, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		if (shape.pointer_size == 4) {
			if (yield points_at_itself (machine, PCR_BASE, shape, cancellable))
				return PCR_BASE;
			throw new Error.NOT_SUPPORTED ("Unable to find the processor control region");
		}

		for (uint attempt = 0; attempt != MAX_CATCH_ATTEMPTS; attempt++) {
			GDB.Thread thread = gdb.exception.thread;
			foreach (string name in new string[] { "gs_base", "k_gs_base" }) {
				uint64 candidate;
				try {
					candidate = yield thread.read_register (name, cancellable);
				} catch (Error e) {
					continue;
				}
				if (yield points_at_itself (machine, candidate, shape, cancellable))
					return candidate;
			}

			yield catch_processor_again (gdb, cancellable);
		}

		throw new Error.NOT_SUPPORTED ("Unable to find the processor control region");
	}

	// You can read only the value that GS holds now. Thus continue the guest and try again.
	private static async void catch_processor_again (GDB.Client gdb, Cancellable? cancellable)
			throws Error, IOError {
		yield gdb.continue (cancellable);

		var source = new TimeoutSource (CATCH_INTERVAL_MS);
		source.set_callback (catch_processor_again.callback);
		source.attach (MainContext.get_thread_default ());
		yield;

		yield gdb.stop (cancellable);
	}

	private const uint MAX_CATCH_ATTEMPTS = 20;
	private const uint CATCH_INTERVAL_MS = 20;

	// A processor control region starts with its own address. Use this to identify a candidate.
	private static async bool points_at_itself (Machine machine, uint64 candidate, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		if (!is_kernel_address (candidate, shape))
			return false;

		GDB.Client gdb = machine.gdb;
		Buffer head;
		try {
			head = gdb.make_buffer (yield gdb.read_byte_array (candidate + shape.self,
				shape.pointer_size, cancellable));
		} catch (Error e) {
			return false;
		}

		return read_pointer (head, 0, shape) == candidate;
	}

	// Both kernels use the 64-bit form of this structure, and a 32-bit kernel extends the sign of
	// the pointers in it.
	private static async uint64 read_loaded_module_list (Machine machine, uint64 version_block, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 list = read_pointer (gdb.make_buffer (yield gdb.read_byte_array (
			version_block + LOADED_MODULE_LIST_OFFSET, shape.pointer_size, cancellable)), 0, shape);
		if (!is_kernel_address (list, shape))
			throw new Error.NOT_SUPPORTED ("Unable to find the loaded module list");

		return list;
	}

	// The kernel gives the addresses that a debugger needs here, and no module exports them.
	private static async uint64 read_process_list_head (Machine machine, uint64 version_block, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 data_list = read_pointer (gdb.make_buffer (yield gdb.read_byte_array (
			version_block + DEBUGGER_DATA_LIST_OFFSET, shape.pointer_size, cancellable)), 0, shape);
		if (!is_kernel_address (data_list, shape))
			return 0;

		uint64 block = read_pointer (gdb.make_buffer (yield gdb.read_byte_array (data_list, shape.pointer_size,
			cancellable)), 0, shape);
		if (!is_kernel_address (block, shape))
			return 0;

		uint64 head = read_pointer (gdb.make_buffer (yield gdb.read_byte_array (
			block + PROCESS_LIST_HEAD_OFFSET, shape.pointer_size, cancellable)), 0, shape);
		if (!is_kernel_address (head, shape))
			return 0;

		return head;
	}

	private static async Gee.List<LoadedModule> read_loaded_modules (Machine machine, uint64 head, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		var modules = new Gee.ArrayList<LoadedModule> ();

		GDB.Client gdb = machine.gdb;
		uint64 entry = read_pointer (gdb.make_buffer (yield gdb.read_byte_array (head, shape.pointer_size,
			cancellable)), 0, shape);
		var visited = new Gee.HashSet<uint64?> ((n) => (uint) (*(uint64 *) n), (a, b) => *(uint64 *) a == *(uint64 *) b);
		while (entry != head && is_kernel_address (entry, shape) && !visited.contains (entry)) {
			visited.add (entry);

			Buffer e = gdb.make_buffer (yield gdb.read_byte_array (entry, shape.table_entry_size, cancellable));

			uint64 base_address = read_pointer (e, shape.dll_base, shape);
			if (is_kernel_address (base_address, shape)) {
				modules.add (new LoadedModule () {
					base_address = base_address,
					size = e.read_uint32 (shape.image_size),
					name = yield read_unicode_string (machine, e, shape.base_name, shape, cancellable),
				});
			}

			entry = read_pointer (e, FORWARD_LINK_OFFSET, shape);
		}

		return modules;
	}

	private class LoadedModule {
		public uint64 base_address;
		public uint32 size;
		public string name;
	}

	private static async void add_export_symbols (Machine machine, LoadedModule module,
			Gee.List<SymbolInfo> symbols, Cancellable? cancellable) throws Error, IOError {
		Gee.List<Export> exports;
		try {
			exports = yield enumerate_exports (machine, module.base_address, cancellable);
		} catch (Error e) {
			return;
		}

		foreach (Export e in exports) {
			symbols.add (new SymbolInfo () {
				name = e.name,
				offset = module.base_address + e.rva,
				symbol_type = 0xf,
				section = 0x10,
			});
		}
	}

	private static async string read_unicode_string (Machine machine, Buffer owner, size_t offset, Shape shape,
			Cancellable? cancellable) throws Error, IOError {
		uint16 length = owner.read_uint16 (offset);
		uint64 buffer = read_pointer (owner, offset + shape.name_buffer, shape);
		if (length == 0 || length > MAX_NAME_SIZE || !is_kernel_address (buffer, shape))
			return "";

		Bytes raw = yield machine.gdb.read_byte_array (buffer, length, cancellable);
		try {
			return convert ((string) raw.get_data (), length, "UTF-8", "UTF-16LE");
		} catch (ConvertError e) {
			return "";
		}
	}

	internal static async bool stopped_in_kernel_mode (GDB.Client gdb, Cancellable? cancellable)
			throws Error, IOError {
		GDB.Thread thread = gdb.exception.thread;

		if (gdb.arch == GDB.TargetArch.ARM64) {
			uint64 state = yield thread.read_register ("cpsr", cancellable);
			return ((state >> EXCEPTION_LEVEL_SHIFT) & EXCEPTION_LEVEL_MASK) == KERNEL_EXCEPTION_LEVEL;
		}

		uint64 cs = yield thread.read_register ("cs", cancellable);
		return (cs & RING_MASK) == 0;
	}

	private static uint64 read_pointer (Buffer buf, size_t offset, Shape shape) {
		return (shape.pointer_size == 8) ? buf.read_uint64 (offset) : buf.read_uint32 (offset);
	}

	private static bool is_kernel_address (uint64 address, Shape shape) {
		if (shape.pointer_size == 8)
			return address >= KERNEL_SPACE_BASE_64;
		return address >= KERNEL_SPACE_BASE && address <= uint32.MAX;
	}

	private class Shape {
		public uint pointer_size;
		public uint16 machine_type;
		public size_t self;
		public size_t version_block;
		public size_t table_entry_size;
		public size_t dll_base;
		public size_t image_size;
		public size_t base_name;
		public size_t name_buffer;

		public static Shape of (GDB.Client gdb) {
			Shape shape = for_pointer_size (gdb.pointer_size);
			if (gdb.arch == GDB.TargetArch.ARM64)
				shape.machine_type = IMAGE_FILE_MACHINE_ARM64;
			return shape;
		}

		private static Shape for_pointer_size (uint pointer_size) {
			if (pointer_size == 8) {
				return new Shape () {
					pointer_size = 8,
					machine_type = IMAGE_FILE_MACHINE_AMD64,
					self = 0x18,
					version_block = 0x108,
					table_entry_size = 0x68,
					dll_base = 0x30,
					image_size = 0x40,
					base_name = 0x58,
					name_buffer = 0x08,
				};
			}

			return new Shape () {
				pointer_size = 4,
				machine_type = IMAGE_FILE_MACHINE_I386,
				self = 0x1c,
				version_block = 0x34,
				table_entry_size = 0x34,
				dll_base = 0x18,
				image_size = 0x20,
				base_name = 0x2c,
				name_buffer = 0x04,
			};
		}
	}

	private const uint64 PCR_BASE = 0xffdff000;

	private const size_t VERSION_BLOCK_SIZE = 0x28;
	private const size_t MACHINE_TYPE_OFFSET = 0x08;
	private const size_t LOADED_MODULE_LIST_OFFSET = 0x18;
	private const size_t DEBUGGER_DATA_LIST_OFFSET = 0x20;
	private const uint16 IMAGE_FILE_MACHINE_I386 = 0x014c;
	private const uint16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;
	private const uint16 IMAGE_FILE_MACHINE_ARM64 = 0xaa64;

	private const string KERNEL_PCR_REGISTER = "x18";
	private const size_t PCR_SCAN_SIZE = 0x1000;
	private const uint64 KERNEL_IMAGE_ALIGNMENT = 0x10000;
	private const uint MAX_IMAGE_STEPS = 512;
	private const uint MAX_IMAGE_PROBES = 4096;

	private const string LOADED_MODULE_LIST = "PsLoadedModuleList";
	private const string INITIAL_PROCESS = "PsInitialSystemProcess";
	private const size_t PROCESS_SCAN_SIZE = 0x800;
	private const uint MAX_PROCESSES = 512;

	private const uint64 RING_MASK = 3;
	private const uint EXCEPTION_LEVEL_SHIFT = 2;
	private const uint64 EXCEPTION_LEVEL_MASK = 3;
	private const uint64 KERNEL_EXCEPTION_LEVEL = 1;

	public const string PROCESS_LIST_HEAD = "PsActiveProcessHead";
	private const size_t PROCESS_LIST_HEAD_OFFSET = 0x50;

	private const size_t FORWARD_LINK_OFFSET = 0x00;
	private const uint16 MAX_NAME_SIZE = 0x200;

	private const uint64 KERNEL_SPACE_BASE = 0x80000000;
	private const uint64 KERNEL_SPACE_BASE_64 = 0xffff800000000000;
}
