[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public static async Win9xLayout collect_win9x_layout (Machine machine, Cancellable? cancellable)
			throws Error, IOError {
		var modules = new Gee.ArrayList<ModuleInfo> ();
		var symbols = new Gee.ArrayList<SymbolInfo> ();

		var blocks = yield find_descriptor_blocks (machine, cancellable);
		if (blocks.is_empty) {
			throw new Error.NOT_SUPPORTED (
				"Unable to find any VxD; the guest is probably still booting, as none of its " +
				"memory is paged in yet");
		}

		var images = yield read_loader_images (machine, blocks, cancellable);

		yield add_kernel32_symbols (machine, symbols, cancellable);

		foreach (DeviceDescriptorBlock ddb in blocks) {
			unowned string[]? names = known_service_names (ddb.name);

			uint64 lowest = uint64.MAX;
			uint64 highest = 0;

			var addresses = yield read_service_table (machine, ddb, cancellable);
			for (int ordinal = 0; ordinal != addresses.size; ordinal++) {
				uint64 address = addresses[ordinal];
				if (!is_implemented (address))
					continue;

				symbols.add (new SymbolInfo () {
					name = (names != null && ordinal < names.length)
						? names[ordinal]
						: "%s_service_%d".printf (ddb.name, ordinal),
					offset = address,
					symbol_type = 0xf,
					section = 0x10,
				});

				lowest = uint64.min (lowest, address);
				highest = uint64.max (highest, address);
			}

			uint64 image_base = 0;
			uint32 image_size = 0;
			LoaderImage? image = find_loader_image (images, ddb.address);
			if (image != null) {
				image_base = image.base_address;
				image_size = image.size;
			} else if (highest != 0) {
				image_base = lowest;
				image_size = (uint32) (highest - lowest);
			} else {
				image_base = ddb.address;
				image_size = (uint32) DDB_SIZE;
			}

			modules.add (new ModuleInfo () {
				name = "%s.VXD".printf (ddb.name),
				version = "",
				offset = image_base,
				size = image_size,
			});
		}

		return new Win9xLayout (modules, symbols);
	}

	public sealed class Win9xLayout : Object {
		public Gee.List<ModuleInfo> modules {
			get;
			construct;
		}

		public Gee.List<SymbolInfo> symbols {
			get;
			construct;
		}

		public Win9xLayout (Gee.List<ModuleInfo> modules, Gee.List<SymbolInfo> symbols) {
			Object (modules: modules, symbols: symbols);
		}
	}

	// Every VxD is on VMM's chain, including the ones that export no services and would
	// therefore be indistinguishable from noise when sweeping. Sweep only far enough to
	// find VMM itself, then follow it.
	private static async Gee.List<DeviceDescriptorBlock> find_descriptor_blocks (Machine machine,
			Cancellable? cancellable) throws Error, IOError {
		var blocks = new Gee.ArrayList<DeviceDescriptorBlock> ();

		uint64 address = yield find_kernel_descriptor_block (machine, cancellable);
		var visited = new Gee.HashSet<uint64?> ((n) => (uint) (*(uint64 *) n), (a, b) => *(uint64 *) a == *(uint64 *) b);
		while (is_arena_address (address) && !visited.contains (address)) {
			visited.add (address);

			Bytes raw;
			try {
				raw = yield machine.gdb.read_byte_array (address, DDB_SIZE, cancellable);
			} catch (Error e) {
				break;
			}

			Buffer buf = machine.gdb.make_buffer (raw);
			DeviceDescriptorBlock? ddb = DeviceDescriptorBlock.parse_linked (buf, address);
			if (ddb == null)
				break;
			blocks.add (ddb);

			address = buf.read_uint32 (NEXT_OFFSET);
		}

		return blocks;
	}

	// VMM is the first VxD in the system arena, thus examine only the start of it.
	private static async uint64 find_kernel_descriptor_block (Machine machine, Cancellable? cancellable)
			throws Error, IOError {
		foreach (Gum.MemoryRange run in yield find_resident_runs (machine, SYSTEM_ARENA_BASE,
				VMM_SEARCH_SPAN, cancellable)) {
			foreach (DeviceDescriptorBlock candidate in yield harvest_candidates (machine, run.base_address,
					run.size, cancellable)) {
				if (candidate.name == "VMM" && yield candidate.is_credible (machine, cancellable))
					return candidate.address;
			}
		}

		return 0;
	}

	// Windows maps the page tables into all contexts, thus one read gives the present pages. The
	// machine would walk the full address space, which costs more than the sweep.
	private static async Gee.List<Gum.MemoryRange?> find_resident_runs (Machine machine, uint64 base_va,
			uint64 span, Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		Buffer entries;
		try {
			entries = gdb.make_buffer (yield gdb.read_byte_array (
				PAGE_TABLES_BASE + (base_va >> 22) * PAGE_SIZE, PAGE_SIZE, cancellable));
		} catch (Error e) {
			return yield collect_runs_from_machine (machine, base_va, span, cancellable);
		}

		var runs = new Gee.ArrayList<Gum.MemoryRange?> ();
		uint64 run_start = 0;
		uint64 run_size = 0;
		for (uint64 offset = 0; offset != span; offset += PAGE_SIZE) {
			size_t index = (size_t) (((base_va + offset) >> 12) & 0x3ff);
			bool present = (entries.read_uint32 (index * 4) & 1) != 0;

			if (present) {
				if (run_size == 0)
					run_start = base_va + offset;
				run_size += PAGE_SIZE;
			} else if (run_size != 0) {
				runs.add (Gum.MemoryRange () { base_address = run_start, size = (size_t) run_size });
				run_size = 0;
			}
		}
		if (run_size != 0)
			runs.add (Gum.MemoryRange () { base_address = run_start, size = (size_t) run_size });

		return runs;
	}

	private static async Gee.List<Gum.MemoryRange?> collect_runs_from_machine (Machine machine, uint64 base_va,
			uint64 span, Cancellable? cancellable) throws Error, IOError {
		uint64 limit = base_va + span;

		var runs = new Gee.ArrayList<Gum.MemoryRange?> ();
		yield machine.enumerate_ranges (Gum.PageProtection.READ, r => {
			if (r.base_va >= base_va && r.base_va < limit)
				runs.add (Gum.MemoryRange () { base_address = r.base_va, size = (size_t) r.size });
			return true;
		}, cancellable);

		return runs;
	}

	private static async Gee.List<DeviceDescriptorBlock> harvest_candidates (Machine machine, uint64 base_va,
			uint64 size, Cancellable? cancellable) throws Error, IOError {
		var candidates = new Gee.ArrayList<DeviceDescriptorBlock> ();

		GDB.Client gdb = machine.gdb;
		for (uint64 offset = 0; offset < size; offset += SCAN_CHUNK_SIZE) {
			uint64 chunk_start = base_va + offset;
			uint64 remaining = size - offset;
			size_t readable_size = (size_t) uint64.min (SCAN_CHUNK_SIZE + STRADDLE_ALLOWANCE, remaining);
			size_t claimed_size = (size_t) uint64.min (SCAN_CHUNK_SIZE, remaining);

			Bytes chunk;
			try {
				chunk = yield gdb.read_byte_array (chunk_start, readable_size, cancellable);
			} catch (Error e) {
				continue;
			}

			Buffer buf = gdb.make_buffer (chunk);
			for (size_t pos = 0; pos < claimed_size && pos + DDB_SIZE <= readable_size; pos += 4) {
				DeviceDescriptorBlock? ddb = DeviceDescriptorBlock.parse (buf, pos, chunk_start + pos);
				if (ddb != null)
					candidates.add (ddb);
			}
		}

		return candidates;
	}

	// VXDLDR knows the object each dynamically loaded VxD was scattered into; the one holding
	// the descriptor block is the VxD proper. Statically linked VxDs live inside VMM32.VXD and
	// have no image of their own to ask about.
	private static async Gee.List<LoaderImage> read_loader_images (Machine machine,
			Gee.List<DeviceDescriptorBlock> blocks, Cancellable? cancellable) throws Error, IOError {
		var images = new Gee.ArrayList<LoaderImage> ();

		DeviceDescriptorBlock? loader = null;
		foreach (DeviceDescriptorBlock ddb in blocks) {
			if (ddb.name == "VXDLDR")
				loader = ddb;
		}
		if (loader == null || loader.service_count <= GET_DEVICE_LIST_ORDINAL)
			return images;

		GDB.Client gdb = machine.gdb;
		uint64 getter = gdb.make_buffer (yield gdb.read_byte_array (
			loader.service_table + GET_DEVICE_LIST_ORDINAL * 4, 4, cancellable)).read_uint32 (0);

		Buffer code = gdb.make_buffer (yield gdb.read_byte_array (getter, 5, cancellable));
		if (code.read_uint8 (0) != LOAD_EAX_ABSOLUTE)
			return images;

		uint64 head = gdb.make_buffer (yield gdb.read_byte_array (code.read_uint32 (1), 4, cancellable))
			.read_uint32 (0);

		uint64 record = head;
		var visited = new Gee.HashSet<uint64?> ((n) => (uint) (*(uint64 *) n), (a, b) => *(uint64 *) a == *(uint64 *) b);
		while (is_arena_address (record) && !visited.contains (record)) {
			visited.add (record);

			Buffer r = gdb.make_buffer (yield gdb.read_byte_array (record, LOADER_RECORD_SIZE, cancellable));
			uint64 ddb_address = r.read_uint32 (LOADER_DDB_OFFSET);
			uint objects = r.read_uint8 (LOADER_OBJECT_COUNT_OFFSET);
			uint64 table = r.read_uint32 (LOADER_OBJECT_TABLE_OFFSET);

			if (objects != 0 && is_arena_address (table)) {
				Buffer entries = gdb.make_buffer (yield gdb.read_byte_array (table,
					objects * LOADER_OBJECT_SIZE, cancellable));
				for (uint i = 0; i != objects; i++) {
					uint64 object_base = entries.read_uint32 (i * LOADER_OBJECT_SIZE);
					uint32 object_size = entries.read_uint32 (i * LOADER_OBJECT_SIZE + 4);
					if (ddb_address >= object_base && ddb_address < object_base + object_size) {
						images.add (new LoaderImage () {
							ddb = ddb_address,
							base_address = object_base,
							size = object_size,
						});
						break;
					}
				}
			}

			record = r.read_uint32 (NEXT_OFFSET);
		}

		return images;
	}

	private static LoaderImage? find_loader_image (Gee.List<LoaderImage> images, uint64 ddb) {
		foreach (LoaderImage image in images) {
			if (image.ddb == ddb)
				return image;
		}
		return null;
	}

	private class LoaderImage {
		public uint64 ddb;
		public uint64 base_address;
		public uint32 size;
	}

	// Two things only KERNEL32 knows: the value it XORs process ids with, and the table its
	// modules are named through. Read both out of its own code rather than hardcoding them.
	private static async void add_kernel32_symbols (Machine machine, Gee.List<SymbolInfo> symbols,
			Cancellable? cancellable) throws Error, IOError {
		uint64 kernel32 = yield find_kernel32 (machine, cancellable);
		if (kernel32 == 0)
			return;

		symbols.add (make_symbol (KERNEL32_BASE, kernel32));

		uint64 obfuscator = yield find_process_id_obfuscator (machine, kernel32, cancellable);
		if (obfuscator != 0)
			symbols.add (make_symbol (PROCESS_ID_OBFUSCATOR, obfuscator));

		uint64 table = yield find_module_table (machine, kernel32, cancellable);
		if (table != 0)
			symbols.add (make_symbol (MODULE_TABLE, table));
	}

	private static SymbolInfo make_symbol (string name, uint64 address) {
		return new SymbolInfo () {
			name = name,
			offset = address,
			symbol_type = 0xf,
			section = 0x10,
		};
	}

	// GetCurrentProcessId() pushes the process database pointer and tail-calls a helper whose
	// first instruction loads the obfuscator.
	private static async uint64 find_process_id_obfuscator (Machine machine, uint64 kernel32,
			Cancellable? cancellable) throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 getter = yield find_export (machine, kernel32, "GetCurrentProcessId", cancellable);
		if (getter == 0)
			return 0;

		Buffer code = gdb.make_buffer (yield gdb.read_byte_array (getter, 12, cancellable));
		if (code.read_uint8 (0) != LOAD_EAX_ABSOLUTE || code.read_uint8 (7) != CALL_RELATIVE)
			return 0;
		uint64 helper = getter + 12 + code.read_uint32 (8);

		Buffer body = gdb.make_buffer (yield gdb.read_byte_array (helper, 5, cancellable));
		if (body.read_uint8 (0) != LOAD_EAX_ABSOLUTE)
			return 0;

		return body.read_uint32 (1);
	}

	// GetModuleFileNameA() reaches the name through the module table, indexing it by the word
	// at MODREF+0x10, which is the one instruction pair worth recognising.
	private static async uint64 find_module_table (Machine machine, uint64 kernel32, Cancellable? cancellable)
			throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		uint64 thunk = yield find_export (machine, kernel32, "GetModuleFileNameA", cancellable);
		if (thunk == 0)
			return 0;

		Buffer prologue = gdb.make_buffer (yield gdb.read_byte_array (thunk, 0x40, cancellable));
		uint64 body = 0;
		for (size_t i = 0; i != 0x40 - 5; i++) {
			if (prologue.read_uint8 (i) == JUMP_RELATIVE) {
				body = thunk + i + 5 + prologue.read_uint32 (i + 1);
				break;
			}
		}
		if (body == 0)
			return 0;

		Buffer code = gdb.make_buffer (yield gdb.read_byte_array (body, 0x100, cancellable));
		for (size_t i = 0; i != 0x100 - INDEX_MODULE_TABLE.length - 4; i++) {
			bool matched = true;
			for (size_t j = 0; j != INDEX_MODULE_TABLE.length; j++) {
				if (code.read_uint8 (i + j) != INDEX_MODULE_TABLE[j]) {
					matched = false;
					break;
				}
			}
			if (matched)
				return code.read_uint32 (i + INDEX_MODULE_TABLE.length);
		}

		return 0;
	}

	private static async uint64 find_kernel32 (Machine machine, Cancellable? cancellable) throws Error, IOError {
		for (uint64 candidate = KERNEL32_SEARCH_BASE; candidate < KERNEL32_SEARCH_LIMIT;
				candidate += IMAGE_ALIGNMENT) {
			if ((yield find_export (machine, candidate, "GetCurrentProcessId", cancellable)) != 0)
				return candidate;
		}

		return 0;
	}

	private static async Gee.List<uint64?> read_service_table (Machine machine, DeviceDescriptorBlock ddb,
			Cancellable? cancellable) throws Error, IOError {
		Bytes raw = yield machine.gdb.read_byte_array (ddb.service_table, ddb.service_count * 4, cancellable);
		Buffer buf = machine.gdb.make_buffer (raw);

		var addresses = new Gee.ArrayList<uint64?> ();
		for (uint i = 0; i != ddb.service_count; i++)
			addresses.add (buf.read_uint32 (i * 4));
		return addresses;
	}

	private class DeviceDescriptorBlock {
		public string name;
		public uint64 address;
		public uint64 service_table;
		public uint32 service_count;

		public static DeviceDescriptorBlock? parse_linked (Buffer buf, uint64 address) {
			string? name = parse_name (buf, NAME_OFFSET);
			if (name == null)
				return null;

			uint32 count = buf.read_uint32 (SERVICE_TABLE_SIZE_OFFSET);
			uint32 table = buf.read_uint32 (SERVICE_TABLE_PTR_OFFSET);
			if (count > MAX_SERVICES || !is_arena_address (table))
				count = 0;

			return new DeviceDescriptorBlock () {
				name = name,
				address = address,
				service_table = table,
				service_count = count,
			};
		}

		public static DeviceDescriptorBlock? parse (Buffer buf, size_t offset, uint64 address) {
			string? name = parse_name (buf, offset + NAME_OFFSET);
			if (name == null)
				return null;

			uint32 count = buf.read_uint32 (offset + SERVICE_TABLE_SIZE_OFFSET);
			if (count == 0 || count > MAX_SERVICES)
				return null;

			uint32 table = buf.read_uint32 (offset + SERVICE_TABLE_PTR_OFFSET);
			if (!is_arena_address (table))
				return null;

			uint32 control_proc = buf.read_uint32 (offset + CONTROL_PROC_OFFSET);
			if (control_proc != 0 && !is_arena_address (control_proc))
				return null;

			return new DeviceDescriptorBlock () {
				name = name,
				address = address,
				service_table = table,
				service_count = count,
			};
		}

		public async bool is_credible (Machine machine, Cancellable? cancellable) throws Error, IOError {
			Bytes first_entry;
			try {
				first_entry = yield machine.gdb.read_byte_array (service_table, 4, cancellable);
			} catch (Error e) {
				return false;
			}
			return is_arena_address (machine.gdb.make_buffer (first_entry).read_uint32 (0));
		}

		private static string? parse_name (Buffer buf, size_t offset) {
			var name = new StringBuilder ();
			bool padding_reached = false;
			for (size_t i = 0; i != NAME_SIZE; i++) {
				char c = (char) buf.read_uint8 (offset + i);
				if (c == ' ') {
					padding_reached = true;
					continue;
				}
				if (padding_reached)
					return null;
				if (!c.isalpha () && !c.isdigit () && c != '_' && c != '$')
					return null;
				name.append_c (c);
			}
			if (name.len == 0)
				return null;
			return name.str;
		}

		private const size_t CONTROL_PROC_OFFSET = 0x18;
		private const size_t NAME_OFFSET = 0x0c;
		private const size_t NAME_SIZE = 8;
		private const size_t SERVICE_TABLE_PTR_OFFSET = 0x30;
		private const size_t SERVICE_TABLE_SIZE_OFFSET = 0x34;
	}

	private static unowned string[]? known_service_names (string vxd) {
		if (vxd == "VMM")
			return VMM_SERVICE_NAMES;
		if (vxd == "VPICD")
			return VPICD_SERVICE_NAMES;
		if (vxd == "VWIN32")
			return VWIN32_SERVICE_NAMES;
		if (vxd == "IFSMGR")
			return IFSMGR_SERVICE_NAMES;
		return null;
	}

	private static bool is_arena_address (uint64 address) {
		return address >= SYSTEM_ARENA_BASE && address < SYSTEM_ARENA_LIMIT;
	}

	private static bool is_implemented (uint64 service_address) {
		return service_address != 0;
	}

	private const size_t NEXT_OFFSET = 0x00;
	public const string KERNEL32_BASE = "KERNEL32_Base";
	public const string PROCESS_ID_OBFUSCATOR = "KERNEL32_ProcessIdObfuscator";
	public const string MODULE_TABLE = "KERNEL32_ModuleTable";

	// movsx ecx, WORD PTR [eax+0x10] ; mov eax, ds:<table>
	private const uint8[] INDEX_MODULE_TABLE = { 0x0f, 0xbf, 0x48, 0x10, 0xa1 };
	private const uint8 JUMP_RELATIVE = 0xe9;

	private const uint64 KERNEL32_SEARCH_BASE = 0xbff00000;
	private const uint64 KERNEL32_SEARCH_LIMIT = 0xc0000000;
	private const uint64 IMAGE_ALIGNMENT = 0x10000;
	private const uint8 CALL_RELATIVE = 0xe8;

	private const uint GET_DEVICE_LIST_ORDINAL = 5;
	private const uint8 LOAD_EAX_ABSOLUTE = 0xa1;
	private const size_t LOADER_RECORD_SIZE = 0x24;
	private const size_t LOADER_DDB_OFFSET = 0x05;
	private const size_t LOADER_OBJECT_COUNT_OFFSET = 0x13;
	private const size_t LOADER_OBJECT_TABLE_OFFSET = 0x17;
	private const size_t LOADER_OBJECT_SIZE = 0x10;
	private const size_t DDB_SIZE = 0x38;
	private const size_t STRADDLE_ALLOWANCE = DDB_SIZE;

	private const uint64 SYSTEM_ARENA_BASE = 0xc0000000;
	private const uint64 SYSTEM_ARENA_LIMIT = 0xc4000000;

	private const uint32 MAX_SERVICES = 0x400;

	// The descriptor block is in the first pages, and the sweep reads one page at a time.
	private const uint64 VMM_SEARCH_SPAN = 1024 * 1024;
	private const uint64 PAGE_TABLES_BASE = 0xff800000;
	private const size_t SCAN_CHUNK_SIZE = 256 * 1024;
}
