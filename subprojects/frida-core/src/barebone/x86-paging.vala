[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	internal class X86PageTables : Object {
		public GDB.Client gdb {
			get;
			construct;
		}

		public PhysicalMemory? physical_memory {
			get;
			set;
		}

		// Fixed for the kernel we are attached to.
		private MMUParameters? cached_parameters;

		private const size_t PAGE_SIZE = 4096;

		private const Gum.PageProtection MAPPING_PROTECTION =
			Gum.PageProtection.READ | Gum.PageProtection.WRITE | Gum.PageProtection.EXECUTE;

		private const uint64 PRESENT_BIT = 1ULL << 0;
		private const uint64 WRITABLE_BIT = 1ULL << 1;
		private const uint64 LARGE_PAGE_BIT = 1ULL << 7;
		private const uint64 NX_BIT = 1ULL << 63;

		private const uint64 LEGACY_ADDRESS_MASK = 0xfffff000ULL;
		private const uint64 EXTENDED_ADDRESS_MASK = 0x000ffffffffff000ULL;

		private const uint64 LEGACY_LARGE_PAGE_ADDRESS_MASK = 0xffc00000ULL;
		private const uint64 PSE36_ADDRESS_MASK = 0x001fe000ULL;
		private const uint PSE36_ADDRESS_SHIFT = 19;

		public X86PageTables (GDB.Client gdb) {
			Object (gdb: gdb);
		}

		public async Gee.List<RangeDetails> collect_ranges (Cancellable? cancellable) throws Error, IOError {
			var result = new Gee.ArrayList<RangeDetails> ();

			MMUParameters p = yield load_parameters (cancellable);
			if (!p.paging_enabled) {
				result.add (new RangeDetails (0, 0, 1ULL << 32, READ | WRITE | EXECUTE, MappingType.UNKNOWN));
				return result;
			}

			bool was_running = yield begin_access (cancellable);
			GLib.Error? failure = null;
			try {
				yield collect_ranges_in_table (p.root_table, 0, 0, READ | WRITE | EXECUTE, p, result, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield end_access (was_running, cancellable);
			throw_if_failed (failure);

			return result;
		}

		private async void collect_ranges_in_table (uint64 table_pa, uint level, uint64 upper_bits,
				Gum.PageProtection inherited, MMUParameters p, Gee.List<RangeDetails> ranges,
				Cancellable? cancellable) throws Error, IOError {
			Level l = p.levels[level];
			Buffer entries = yield read_buffer (table_pa, l.num_entries * p.entry_size, cancellable);

			for (uint i = 0; i != l.num_entries; i++) {
				uint64 entry = read_entry (entries, i * p.entry_size, p);
				if ((entry & PRESENT_BIT) == 0)
					continue;

				uint64 va = upper_bits | ((uint64) i << l.shift);
				Gum.PageProtection prot = inherited & protection_from_entry (entry, level, p);

				if (is_leaf_entry (entry, level, p)) {
					ranges.add (new RangeDetails (canonicalize (va, p), leaf_address (entry, level, p),
						1ULL << l.shift, prot, MappingType.UNKNOWN));
					continue;
				}

				yield collect_ranges_in_table (table_address (entry, p), level + 1, va, prot, p, ranges,
					cancellable);
			}
		}

		public async uint64 translate (uint64 va, Cancellable? cancellable) throws Error, IOError {
			MMUParameters p = yield load_parameters (cancellable);
			if (!p.paging_enabled)
				return va;

			bool was_running = yield begin_access (cancellable);
			uint64 pa = 0;
			GLib.Error? failure = null;
			try {
				LeafEntry leaf = yield find_leaf_entry (va, p, cancellable);
				uint64 page_mask = (1ULL << p.levels[leaf.level].shift) - 1;

				pa = leaf_address (leaf.value, leaf.level, p) | (va & page_mask);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield end_access (was_running, cancellable);
			throw_if_failed (failure);

			return pa;
		}

		public async void protect (uint64 va, size_t size, Gum.PageProtection prot, Cancellable? cancellable)
				throws Error, IOError {
			MMUParameters p = yield load_parameters (cancellable);
			if (!p.paging_enabled)
				throw new Error.INVALID_OPERATION ("Unable to change protection while paging is disabled");

			uint64 start_va = page_start (va, PAGE_SIZE);
			uint64 end_va = round_address_up (va + size, PAGE_SIZE);

			bool was_running = yield begin_access (cancellable);
			GLib.Error? failure = null;
			try {
				var relaxed_slots = new Gee.HashSet<uint64?> (Numeric.uint64_hash, Numeric.uint64_equal);
				Level leaf = p.levels[p.leaf_level];

				uint64 page_va = start_va;
				while (page_va != end_va) {
					LeafEntry mapping = yield find_leaf_entry (page_va, p, cancellable);
					if (mapping.level != p.leaf_level) {
						page_va = skip_large_page (page_va, end_va, mapping, prot, p);
						continue;
					}

					uint index = (uint) ((page_va >> leaf.shift) & (leaf.num_entries - 1));
					uint64 remaining_here = (uint64) (leaf.num_entries - index) * PAGE_SIZE;
					uint64 chunk_end = uint64.min (end_va, page_va + remaining_here);
					uint num_pages = (uint) ((chunk_end - page_va) / PAGE_SIZE);

					yield relax_parent_entries (page_va, prot, p, relaxed_slots, cancellable);

					uint64 first_slot = mapping.slot_pa;
					Buffer current = yield read_buffer (first_slot, num_pages * p.entry_size,
						cancellable);

					var builder = gdb.make_buffer_builder ();
					bool changed = false;
					for (uint i = 0; i != num_pages; i++) {
						uint64 entry = read_entry (current, i * p.entry_size, p);
						uint64 updated = apply_protection_bits (entry, prot, p);
						changed = changed || updated != entry;
						if (p.entry_size == 8)
							builder.append_uint64 (updated);
						else
							builder.append_uint32 ((uint32) updated);
					}
					if (changed)
						yield write_buffer (first_slot, builder.build (), cancellable);

					page_va = chunk_end;
				}
			} catch (GLib.Error e) {
				failure = e;
			}
			yield end_access (was_running, cancellable);
			throw_if_failed (failure);
		}

		// You cannot divide a large page here. Thus it is a problem only if it gives less than the
		// request. Windows maps its kernel pool this way, and other data shares the mapping.
		private static uint64 skip_large_page (uint64 page_va, uint64 end_va, LeafEntry mapping,
				Gum.PageProtection prot, MMUParameters p) throws Error {
			if (!grants (mapping.value, prot, p)) {
				throw new Error.NOT_SUPPORTED (
					"Unable to change protection of the large page mapping at 0x%" +
					uint64.FORMAT_MODIFIER + "x", page_va);
			}

			uint64 span = (uint64) 1 << p.levels[mapping.level].shift;
			return uint64.min (end_va, round_address_up (page_va + 1, (size_t) span));
		}

		private static bool grants (uint64 entry, Gum.PageProtection prot, MMUParameters p) {
			if ((prot & Gum.PageProtection.WRITE) != 0 && (entry & WRITABLE_BIT) == 0)
				return false;
			if ((prot & Gum.PageProtection.EXECUTE) != 0 && p.nx_enabled && (entry & NX_BIT) != 0)
				return false;
			return true;
		}

		private async LeafEntry find_leaf_entry (uint64 va, MMUParameters p, Cancellable? cancellable)
				throws Error, IOError {
			uint64 table_pa = p.root_table;
			for (uint level = 0; ; level++) {
				uint64 slot_pa = slot_address (va, table_pa, level, p);
				uint64 entry = yield read_entry_at (slot_pa, p, cancellable);

				if ((entry & PRESENT_BIT) == 0) {
					throw new Error.NOT_SUPPORTED ("Address 0x%" + uint64.FORMAT_MODIFIER + "x is not mapped",
						va);
				}

				if (is_leaf_entry (entry, level, p))
					return { slot_pa, entry, level };

				table_pa = table_address (entry, p);
			}
		}

		private struct LeafEntry {
			public uint64 slot_pa;
			public uint64 value;
			public uint level;
		}

		private async void relax_parent_entries (uint64 va, Gum.PageProtection prot, MMUParameters p,
				Gee.HashSet<uint64?> already_relaxed, Cancellable? cancellable) throws Error, IOError {
			uint64 table_pa = p.root_table;
			for (uint level = 0; level != p.leaf_level; level++) {
				uint64 slot_pa = slot_address (va, table_pa, level, p);
				uint64 entry = yield read_entry_at (slot_pa, p, cancellable);

				if (!already_relaxed.contains (slot_pa) && p.levels[level].has_protection_bits) {
					uint64 new_entry = entry;
					if ((prot & Gum.PageProtection.WRITE) != 0)
						new_entry |= WRITABLE_BIT;
					if ((prot & Gum.PageProtection.EXECUTE) != 0 && p.nx_enabled)
						new_entry &= ~NX_BIT;

					if (new_entry != entry) {
						yield write_entry (slot_pa, new_entry, p, cancellable);
						entry = new_entry;
					}
					already_relaxed.add (slot_pa);
				}

				table_pa = table_address (entry, p);
			}
		}

		public async Allocation map (Gee.List<uint64?> physical_addresses, Cancellable? cancellable)
				throws Error, IOError {
			MMUParameters p = yield load_parameters (cancellable);
			if (!p.paging_enabled)
				throw new Error.NOT_SUPPORTED ("Paging is disabled");

			var run = new Run (physical_addresses.size);

			bool was_running = yield begin_access (cancellable);
			Allocation? allocation = null;
			GLib.Error? failure = null;
			try {
				if (yield scan_from_the_top (run, p, cancellable))
					allocation = yield occupy (run, physical_addresses, p, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield end_access (was_running, cancellable);
			throw_if_failed (failure);

			if (allocation == null)
				throw new Error.NOT_SUPPORTED ("Unable to find a free run of page table entries");

			return allocation;
		}

		private async bool scan_from_the_top (Run run, MMUParameters p, Cancellable? cancellable)
				throws Error, IOError {
			uint num_entries = p.levels[0].num_entries;

			for (uint first = num_entries - 1; first >= num_entries / 2; first--) {
				run.reset ();

				if (yield scan_for_run (run, p.root_table, 0, 0, first, p, cancellable))
					return true;
			}

			return false;
		}

		// A run has to be contiguous in virtual address space but not in the tables
		// describing it, so anything longer than a table holds is gathered a segment
		// at a time and a gap anywhere above the leaves starts the search over.
		private async bool scan_for_run (Run run, uint64 table_pa, uint level, uint64 upper_bits,
				uint first, MMUParameters p, Cancellable? cancellable) throws Error, IOError {
			Level l = p.levels[level];
			bool at_leaf_level = level == p.leaf_level;

			Buffer entries = yield read_buffer (table_pa, l.num_entries * p.entry_size, cancellable);

			for (uint i = first; i != l.num_entries; i++) {
				uint64 entry = read_entry (entries, i * p.entry_size, p);
				uint64 prefix = upper_bits | ((uint64) i << l.shift);

				if (!at_leaf_level) {
					bool usable = (entry & PRESENT_BIT) != 0
						&& !is_leaf_entry (entry, level, p)
						&& (protection_from_entry (entry, level, p) & MAPPING_PROTECTION)
							== MAPPING_PROTECTION;
					if (!usable) {
						run.reset ();
						continue;
					}

					if (yield scan_for_run (run, table_address (entry, p), level + 1, prefix, 0, p,
							cancellable))
						return true;
					continue;
				}

				uint64 va = canonicalize (prefix, p);
				if ((entry & PRESENT_BIT) != 0 || va == 0) {
					run.reset ();
					continue;
				}

				run.take (va, table_pa + ((uint64) i * p.entry_size), p.entry_size);
				if (run.is_complete ())
					return true;
			}

			return false;
		}

		private async Allocation occupy (Run run, Gee.List<uint64?> physical_addresses, MMUParameters p,
				Cancellable? cancellable) throws Error, IOError {
			var displaced = new Gee.ArrayList<DisplacedEntries> ();

			uint next_page = 0;
			foreach (RunSegment segment in run.segments) {
				var builder = gdb.make_buffer_builder ();
				for (uint i = 0; i != segment.num_entries; i++) {
					uint64 pa = physical_addresses[(int) (next_page + i)];
					uint64 entry = apply_protection_bits (pa | PRESENT_BIT, MAPPING_PROTECTION, p);
					if (p.entry_size == 8)
						builder.append_uint64 (entry);
					else
						builder.append_uint32 ((uint32) entry);
				}
				Bytes new_entries = builder.build ();

				Bytes old_entries = (yield read_buffer (segment.slot_pa, new_entries.get_size (),
					cancellable)).bytes;
				yield write_buffer (segment.slot_pa, new_entries, cancellable);

				displaced.add (new DisplacedEntries (segment.slot_pa, old_entries));
				next_page += segment.num_entries;
			}

			return new EntryAllocation (run.va, run.count * PAGE_SIZE, displaced, this);
		}

		private async void restore_entries (Gee.List<DisplacedEntries> displaced, Cancellable? cancellable)
				throws Error, IOError {
			bool was_running = yield begin_access (cancellable);
			GLib.Error? failure = null;
			try {
				foreach (DisplacedEntries d in displaced)
					yield write_buffer (d.slot_pa, d.entries, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield end_access (was_running, cancellable);
			throw_if_failed (failure);
		}

		private class Run {
			public uint needed;
			public uint64 va;
			public uint count;
			public Gee.ArrayList<RunSegment> segments = new Gee.ArrayList<RunSegment> ();

			private uint64 next_va;
			private size_t entry_size;

			public Run (uint needed) {
				this.needed = needed;
			}

			public void reset () {
				va = 0;
				count = 0;
				next_va = 0;
				segments.clear ();
			}

			public void take (uint64 page_va, uint64 slot_pa, size_t entry_size) {
				if (count != 0 && page_va != next_va)
					reset ();

				this.entry_size = entry_size;

				if (count == 0) {
					va = page_va;
					segments.add (new RunSegment (slot_pa));
				} else {
					RunSegment last = segments[segments.size - 1];
					if (slot_pa == last.slot_pa + (last.num_entries * entry_size))
						last.num_entries++;
					else
						segments.add (new RunSegment (slot_pa));
				}

				count++;
				next_va = page_va + PAGE_SIZE;
			}

			public bool is_complete () {
				return count == needed;
			}
		}

		private class RunSegment {
			public uint64 slot_pa;
			public uint num_entries = 1;

			public RunSegment (uint64 slot_pa) {
				this.slot_pa = slot_pa;
			}
		}

		private class DisplacedEntries {
			public uint64 slot_pa;
			public Bytes entries;

			public DisplacedEntries (uint64 slot_pa, Bytes entries) {
				this.slot_pa = slot_pa;
				this.entries = entries;
			}
		}

		private class EntryAllocation : Object, Allocation {
			public uint64 virtual_address {
				get { return va; }
			}

			public size_t size {
				get { return allocated_size; }
			}

			private uint64 va;
			private size_t allocated_size;
			private Gee.List<DisplacedEntries> displaced;
			private X86PageTables page_tables;

			public EntryAllocation (uint64 va, size_t allocated_size, Gee.List<DisplacedEntries> displaced,
					X86PageTables page_tables) {
				this.va = va;
				this.allocated_size = allocated_size;
				this.displaced = displaced;
				this.page_tables = page_tables;
			}

			public async void deallocate (Cancellable? cancellable) throws Error, IOError {
				yield page_tables.restore_entries (displaced, cancellable);
			}
		}

		private static uint64 slot_address (uint64 va, uint64 table_pa, uint level, MMUParameters p) {
			Level l = p.levels[level];
			uint index = (uint) ((va >> l.shift) & (l.num_entries - 1));
			return table_pa + ((uint64) index * p.entry_size);
		}

		private static bool is_leaf_entry (uint64 entry, uint level, MMUParameters p) {
			if (level == p.leaf_level)
				return true;
			return p.levels[level].may_hold_large_page && (entry & LARGE_PAGE_BIT) != 0;
		}

		private static uint64 table_address (uint64 entry, MMUParameters p) {
			return entry & ((p.entry_size == 8) ? EXTENDED_ADDRESS_MASK : LEGACY_ADDRESS_MASK);
		}

		private static uint64 leaf_address (uint64 entry, uint level, MMUParameters p) {
			if (p.entry_size == 8)
				return entry & EXTENDED_ADDRESS_MASK & ~((1ULL << p.levels[level].shift) - 1);

			if (level == p.leaf_level)
				return entry & LEGACY_ADDRESS_MASK;

			uint64 low_bits = entry & LEGACY_LARGE_PAGE_ADDRESS_MASK;
			uint64 pse36_high_bits = (entry & PSE36_ADDRESS_MASK) << PSE36_ADDRESS_SHIFT;
			return low_bits | pse36_high_bits;
		}

		private static uint64 canonicalize (uint64 va, MMUParameters p) {
			if (p.canonical_bits == 0)
				return va;
			uint64 sign = 1ULL << (p.canonical_bits - 1);
			return (va ^ sign) - sign;
		}

		// What a single entry contributes is a ceiling, not the effective right: the bits are ANDed
		// across levels.
		private static Gum.PageProtection protection_from_entry (uint64 entry, uint level, MMUParameters p) {
			if (!p.levels[level].has_protection_bits)
				return READ | WRITE | EXECUTE;

			Gum.PageProtection prot = READ;
			if ((entry & WRITABLE_BIT) != 0)
				prot |= WRITE;
			if (!p.nx_enabled || (entry & NX_BIT) == 0)
				prot |= EXECUTE;
			return prot;
		}

		private static uint64 apply_protection_bits (uint64 entry, Gum.PageProtection prot, MMUParameters p) {
			uint64 result = entry;

			if ((prot & Gum.PageProtection.WRITE) != 0)
				result |= WRITABLE_BIT;
			else
				result &= ~WRITABLE_BIT;

			if (p.nx_enabled) {
				if ((prot & Gum.PageProtection.EXECUTE) != 0)
					result &= ~NX_BIT;
				else
					result |= NX_BIT;
			}

			return result;
		}

		private async uint64 read_entry_at (uint64 slot_pa, MMUParameters p, Cancellable? cancellable)
				throws Error, IOError {
			Buffer buf = yield read_buffer (slot_pa, p.entry_size, cancellable);
			return read_entry (buf, 0, p);
		}

		private async void write_entry (uint64 slot_pa, uint64 entry, MMUParameters p, Cancellable? cancellable)
				throws Error, IOError {
			Buffer buf = gdb.make_buffer (new Bytes (new uint8[p.entry_size]));
			if (p.entry_size == 8)
				buf.write_uint64 (0, entry);
			else
				buf.write_uint32 (0, (uint32) entry);

			yield write_buffer (slot_pa, buf.bytes, cancellable);
		}

		private static uint64 read_entry (Buffer entries, size_t offset, MMUParameters p) {
			return (p.entry_size == 8) ? entries.read_uint64 (offset) : entries.read_uint32 (offset);
		}

		private async bool begin_access (Cancellable? cancellable) throws Error, IOError {
			if (physical_memory != null)
				return false;

			return yield enter_physical_addressing (gdb, cancellable);
		}

		private async void end_access (bool was_running, Cancellable? cancellable) throws Error, IOError {
			if (physical_memory != null)
				return;

			yield leave_physical_addressing (gdb, was_running, cancellable);
		}

		private async Buffer read_buffer (uint64 pa, size_t size, Cancellable? cancellable) throws Error, IOError {
			if (physical_memory != null && physical_memory.contains (pa))
				return gdb.make_buffer (new Bytes (physical_memory.read (pa, size)));
			return yield gdb.read_buffer (pa, size, cancellable);
		}

		private async void write_buffer (uint64 pa, Bytes data, Cancellable? cancellable) throws Error, IOError {
			if (physical_memory != null && physical_memory.contains (pa)) {
				physical_memory.write (pa, data.get_data ());
				return;
			}
			yield gdb.write_byte_array (pa, data, cancellable);
		}

		private async MMUParameters load_parameters (Cancellable? cancellable) throws Error, IOError {
			if (cached_parameters == null)
				cached_parameters = yield MMUParameters.load (gdb, cancellable);
			return cached_parameters;
		}

		private class MMUParameters {
			public bool paging_enabled;
			public bool nx_enabled;
			public uint canonical_bits;
			public size_t entry_size;
			public uint64 root_table;
			public Level[] levels;

			public uint leaf_level {
				get { return levels.length - 1; }
			}

			private const uint64 CR0_PG = 1ULL << 31;
			private const uint64 CR3_PAE_ROOT_MASK = ~0x1fULL;
			private const uint64 CR4_PSE = 1ULL << 4;
			private const uint64 CR4_PAE = 1ULL << 5;
			private const uint64 CR4_LA57 = 1ULL << 12;
			private const uint64 EFER_LMA = 1ULL << 10;
			private const uint64 EFER_NXE = 1ULL << 11;

			public static async MMUParameters load (GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
				ControlRegisters regs = yield ControlRegisters.read (gdb, cancellable);

				var parameters = new MMUParameters ();

				parameters.paging_enabled = (regs.cr0 & CR0_PG) != 0;

				bool pae = (regs.cr4 & CR4_PAE) != 0;
				parameters.nx_enabled = pae && (regs.efer & EFER_NXE) != 0;

				if ((regs.efer & EFER_LMA) != 0)
					parameters.adopt_long_mode ((regs.cr4 & CR4_LA57) != 0, regs.cr3);
				else if (pae)
					parameters.adopt_pae (regs.cr3);
				else
					parameters.adopt_legacy ((regs.cr4 & CR4_PSE) != 0, regs.cr3);

				return parameters;
			}

			private void adopt_long_mode (bool la57, uint64 cr3) {
				var levels = new Gee.ArrayList<Level> ();
				if (la57)
					levels.add (new Level (48, 512, false, true));
				levels.add (new Level (39, 512, false, true));
				levels.add (new Level (30, 512, true, true));
				levels.add (new Level (21, 512, true, true));
				levels.add (new Level (12, 512, false, true));

				this.levels = levels.to_array ();
				canonical_bits = la57 ? 57 : 48;
				entry_size = 8;
				root_table = cr3 & EXTENDED_ADDRESS_MASK;
			}

			// A page-directory-pointer entry has reserved bits where the others keep RW and NX.
			private void adopt_pae (uint64 cr3) {
				levels = {
					new Level (30, 4, false, false),
					new Level (21, 512, true, true),
					new Level (12, 512, false, true)
				};
				entry_size = 8;
				root_table = cr3 & CR3_PAE_ROOT_MASK;
			}

			private void adopt_legacy (bool pse_enabled, uint64 cr3) {
				levels = {
					new Level (22, 1024, pse_enabled, true),
					new Level (12, 1024, false, true)
				};
				entry_size = 4;
				root_table = cr3 & LEGACY_ADDRESS_MASK;
			}
		}

		private class Level {
			public uint shift;
			public uint num_entries;
			public bool may_hold_large_page;
			public bool has_protection_bits;

			public Level (uint shift, uint num_entries, bool may_hold_large_page, bool has_protection_bits) {
				this.shift = shift;
				this.num_entries = num_entries;
				this.may_hold_large_page = may_hold_large_page;
				this.has_protection_bits = has_protection_bits;
			}
		}

		private class ControlRegisters {
			public uint64 cr0;
			public uint64 cr3;
			public uint64 cr4;
			public uint64 efer;

			public static async ControlRegisters read (GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
				var regs = new ControlRegisters ();

				if (yield regs.try_read_from_registers (gdb, cancellable))
					return regs;

				yield regs.read_from_monitor (gdb, cancellable);

				return regs;
			}

			private async bool try_read_from_registers (GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
				GDB.Exception? exception = gdb.exception;
				if (exception == null)
					throw new Error.INVALID_OPERATION ("Unable to query in current state");
				GDB.Thread thread = exception.thread;

				try {
					cr0 = yield thread.read_register ("cr0", cancellable);
					cr3 = yield thread.read_register ("cr3", cancellable);
					cr4 = yield thread.read_register ("cr4", cancellable);
				} catch (Error e) {
					return false;
				}

				try {
					efer = yield thread.read_register ("efer", cancellable);
				} catch (Error e) {
				}

				return true;
			}

			private async void read_from_monitor (GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
				string dump = yield gdb.run_remote_command ("info registers", cancellable);

				bool found_cr3 = false;
				foreach (string token in dump.split_set (" \t\r\n")) {
					string[] parts = token.split ("=");
					if (parts.length != 2)
						continue;
					uint64 val = uint64.parse (parts[1], 16);

					switch (parts[0]) {
						case "CR0":
							cr0 = val;
							break;
						case "CR3":
							cr3 = val;
							found_cr3 = true;
							break;
						case "CR4":
							cr4 = val;
							break;
						case "EFER":
							efer = val;
							break;
						default:
							break;
					}
				}

				if (!found_cr3)
					throw new Error.NOT_SUPPORTED ("Unable to determine the target's control registers");
			}
		}
	}
}
