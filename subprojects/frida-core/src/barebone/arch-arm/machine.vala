[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class ArmMachine : Object, Machine {
		public override GDB.Client gdb {
			get;
			set;
		}

		public override string llvm_target {
			get { return "armv7a-none-eabi"; }
		}

		public override string llvm_code_model {
			get { return "small"; }
		}

		private const uint NUM_ARGS_IN_REGS = 4;

		private const uint64 THUMB_BIT = 1ULL;

		private const uint64 CPSR_THUMB_BIT = 1ULL << 5;
		private const uint64 CPSR_MODE_MASK = 0x1f;
		private const uint64 CPSR_MODE_USER = 0x10;

		private const uint64 TTBCR_EAE = 1ULL << 31;
		private const uint64 TTBCR_N_MASK = 7;
		private const uint64 SCTLR_M = 1ULL << 0;
		private const uint64 SCTLR_AFE = 1ULL << 29;

		private const uint L1_INDEX_BITS = 12;
		private const uint L1_NUM_ENTRIES = 1 << L1_INDEX_BITS;
		private const uint L1_ENTRY_SIZE = 4;
		private const uint L1_SHIFT = 20;
		private const uint L1_ALIGN_BITS = 14;
		private const uint KERNEL_FIRST_L1_INDEX = L1_NUM_ENTRIES / 2;
		private const uint32 L1_KIND_MASK = 3;
		private const uint32 L1_KIND_FAULT = 0;
		private const uint32 L1_KIND_TABLE = 1;
		private const uint32 L1_KIND_SECTION = 2;
		private const uint32 L2_TABLE_MASK = 0xfffffc00U;

		private const uint32 SECTION_MASK = 0xfff00000U;
		private const uint32 SUPERSECTION_MASK = 0xff000000U;
		private const size_t SECTION_SIZE = 1024 * 1024;
		private const size_t SUPERSECTION_SIZE = 16 * 1024 * 1024;
		private const uint32 SECTION_SUPER = 1U << 18;
		private const uint32 SECTION_XN = 1U << 4;
		private const uint SECTION_AP_SHIFT = 10;
		private const uint SECTION_APX_SHIFT = 15;

		private const uint L2_NUM_ENTRIES = 256;
		private const uint L2_ENTRY_SIZE = 4;
		private const uint L2_SHIFT = 12;
		private const uint32 L2_KIND_MASK = 3;
		private const uint32 L2_KIND_FAULT = 0;
		private const uint32 L2_KIND_LARGE = 1;
		private const uint32 LARGE_PAGE_MASK = 0xffff0000U;
		private const uint32 SMALL_PAGE_MASK = 0xfffff000U;
		private const size_t LARGE_PAGE_SIZE = 64 * 1024;
		private const size_t SMALL_PAGE_SIZE = 4096;
		private const uint32 LARGE_PAGE_XN = 1U << 15;
		private const uint32 SMALL_PAGE_XN = 1U;
		private const uint PAGE_AP_SHIFT = 4;
		private const uint PAGE_APX_SHIFT = 9;
		private const uint AP_LOW_MASK = 3;
		private const uint AP_HIGH_SHIFT = 2;
		private const uint AP_READ_WRITE = 3;
		private const uint AP_READ_ONLY = 6;
		private const uint AFE_AP_READ_WRITE = 3;
		private const uint AFE_AP_READ_ONLY = 7;

		private const uint32 SMALL_PAGE_MAPPED = (3 << PAGE_AP_SHIFT) | (1 << 3) | (1 << 2) | 2;

		public uint64 call_landing_zone = 0;

		// Every process on ARM carries its own copy of the kernel's top-level table, and a
		// mapping made after that copy reaches it only when somebody faults on it. A debugger
		// never does, so the kernel's own table is what its addresses are resolved through.
		public uint64 kernel_page_table = 0;
		private uint64 kernel_page_table_pa = 0;

		public ArmMachine (GDB.Client gdb) {
			Object (gdb: gdb);
		}

		public async size_t query_page_size (Cancellable? cancellable) throws Error, IOError {
			return 4096;
		}

		public async uint query_exception_level (Cancellable? cancellable) throws Error, IOError {
			GDB.Exception? exception = gdb.exception;
			if (exception == null)
				throw new Error.INVALID_OPERATION ("Unable to query in current state");
			GDB.Thread thread = exception.thread;

			var cpsr = yield thread.read_register ("cpsr", cancellable);

			return ((cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) ? 0 : 1;
		}

		public async void enumerate_ranges (Gum.PageProtection prot, FoundRangeFunc func, Cancellable? cancellable)
				throws Error, IOError {
			Gee.List<RangeDetails> ranges = yield collect_ranges_using_mmu (cancellable);
			foreach (RangeDetails r in coalesce_ranges (ranges)) {
				if ((r.protection & prot) != prot)
					continue;
				if (!func (r))
					return;
			}
		}

		private async Gee.List<RangeDetails> collect_ranges_using_mmu (Cancellable? cancellable) throws Error, IOError {
			var result = new Gee.ArrayList<RangeDetails> ();

			MMUParameters p = yield MMUParameters.load (gdb, cancellable);

			bool was_running = yield enter_physical_addressing (gdb, cancellable);
			GLib.Error? failure = null;
			try {
				uint boundary = p.split_index;
				yield collect_ranges_in_l1 (p.tt0, 0, boundary, p, result, cancellable);
				if (boundary != L1_NUM_ENTRIES)
					yield collect_ranges_in_l1 (p.tt1, boundary, L1_NUM_ENTRIES, p, result, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield leave_physical_addressing (gdb, was_running, cancellable);
			throw_if_failed (failure);

			return result;
		}

		private async void collect_ranges_in_l1 (uint64 table_address, uint first_index, uint end_index, MMUParameters p,
				Gee.List<RangeDetails> ranges, Cancellable? cancellable) throws Error, IOError {
			Buffer entries = yield read_physical_buffer (table_address + (first_index * L1_ENTRY_SIZE),
				(end_index - first_index) * L1_ENTRY_SIZE, cancellable);

			for (uint i = first_index; i != end_index; i++) {
				uint32 raw = entries.read_uint32 ((i - first_index) * L1_ENTRY_SIZE);
				uint64 va = (uint64) i << L1_SHIFT;

				uint32 kind = raw & L1_KIND_MASK;
				if (kind == L1_KIND_TABLE) {
					yield collect_ranges_in_l2 (raw & L2_TABLE_MASK, va, p, ranges, cancellable);
				} else if (kind == L1_KIND_SECTION) {
					bool is_supersection = (raw & SECTION_SUPER) != 0;
					uint64 pa = is_supersection ? (raw & SUPERSECTION_MASK) : (raw & SECTION_MASK);
					size_t size = is_supersection ? SUPERSECTION_SIZE : SECTION_SIZE;
					uint ap = ((raw >> SECTION_AP_SHIFT) & AP_LOW_MASK)
						| (((raw >> SECTION_APX_SHIFT) & 1) << AP_HIGH_SHIFT);
					bool xn = (raw & SECTION_XN) != 0;

					ranges.add (new RangeDetails (va, pa, size, protection_from_ap (ap, xn, p.afe),
						MappingType.UNKNOWN));
				}
			}
		}

		private async void collect_ranges_in_l2 (uint64 table_address, uint64 base_va, MMUParameters p,
				Gee.List<RangeDetails> ranges, Cancellable? cancellable) throws Error, IOError {
			Buffer entries = yield read_physical_buffer (table_address, L2_NUM_ENTRIES * L2_ENTRY_SIZE, cancellable);

			for (uint i = 0; i != L2_NUM_ENTRIES; i++) {
				uint32 raw = entries.read_uint32 (i * L2_ENTRY_SIZE);
				if ((raw & L2_KIND_MASK) == L2_KIND_FAULT)
					continue;

				bool is_large = (raw & L2_KIND_MASK) == L2_KIND_LARGE;
				uint64 pa = is_large ? (raw & LARGE_PAGE_MASK) : (raw & SMALL_PAGE_MASK);
				size_t size = is_large ? LARGE_PAGE_SIZE : SMALL_PAGE_SIZE;
				uint ap = ((raw >> PAGE_AP_SHIFT) & AP_LOW_MASK)
					| (((raw >> PAGE_APX_SHIFT) & 1) << AP_HIGH_SHIFT);
				bool xn = is_large ? ((raw & LARGE_PAGE_XN) != 0) : ((raw & SMALL_PAGE_XN) != 0);

				ranges.add (new RangeDetails (base_va | ((uint64) i << L2_SHIFT), pa, size,
					protection_from_ap (ap, xn, p.afe), MappingType.UNKNOWN));
			}
		}

		private static Gum.PageProtection protection_from_ap (uint ap, bool xn, bool afe) {
			Gum.PageProtection prot;

			if (afe) {
				if ((ap & 1) == 0)
					return NO_ACCESS;
				prot = ((ap & 4) != 0)
					? Gum.PageProtection.READ
					: (Gum.PageProtection.READ | Gum.PageProtection.WRITE);
			} else if (ap == 0) {
				return NO_ACCESS;
			} else if (ap <= 3) {
				prot = Gum.PageProtection.READ | Gum.PageProtection.WRITE;
			} else {
				prot = Gum.PageProtection.READ;
			}

			if (!xn)
				prot |= Gum.PageProtection.EXECUTE;

			return prot;
		}

		public override async void write_virtual (uint64 va, uint8[] data, Cancellable? cancellable)
				throws Error, IOError {
			if (kernel_page_table == 0) {
				yield gdb.write_byte_array (va, new Bytes (data), cancellable);
				return;
			}

			MMUParameters p = yield MMUParameters.load (gdb, cancellable);

			bool was_running = yield enter_physical_addressing (gdb, cancellable);
			GLib.Error? failure = null;
			bool written = false;
			try {
				yield locate_kernel_page_table (p, cancellable);

				if (kernel_page_table_pa != 0)
					written = yield write_through_kernel_table (va, data, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield leave_physical_addressing (gdb, was_running, cancellable);
			throw_if_failed (failure);

			if (!written)
				yield gdb.write_byte_array (va, new Bytes (data), cancellable);
		}

		private async bool write_through_kernel_table (uint64 va, uint8[] data, Cancellable? cancellable)
				throws Error, IOError {
			size_t offset = 0;
			while (offset < data.length) {
				uint64 cur_va = va + offset;
				uint64? pa = yield translate_in_table (kernel_page_table_pa, cur_va, cancellable);
				if (pa == null)
					return offset != 0;

				size_t chunk = size_t.min (SMALL_PAGE_SIZE - (size_t) (cur_va & (SMALL_PAGE_SIZE - 1)),
					data.length - offset);
				yield write_physical_buffer (pa, new Bytes (data[offset : offset + chunk]), cancellable);
				offset += chunk;
			}

			return true;
		}

		private async void locate_kernel_page_table (MMUParameters p, Cancellable? cancellable)
				throws Error, IOError {
			if (kernel_page_table == 0 || kernel_page_table_pa != 0)
				return;

			uint64? table = yield translate_in_table (p.l1_table_for (kernel_page_table), kernel_page_table,
				cancellable);
			if (table != null)
				kernel_page_table_pa = table;
		}

		public override async uint64 translate_address (uint64 va, Cancellable? cancellable) throws Error, IOError {
			MMUParameters p = yield MMUParameters.load (gdb, cancellable);

			bool was_running = yield enter_physical_addressing (gdb, cancellable);
			uint64? pa = null;
			GLib.Error? failure = null;
			try {
				yield locate_kernel_page_table (p, cancellable);

				pa = yield translate_in_table (yield table_describing (va, p, cancellable), va, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield leave_physical_addressing (gdb, was_running, cancellable);
			throw_if_failed (failure);

			if (pa == null)
				throw new Error.INVALID_ARGUMENT ("Address 0x%x is not mapped", (uint) va);

			return pa;
		}

		private async uint64 table_describing (uint64 va, MMUParameters p, Cancellable? cancellable)
				throws Error, IOError {
			if (kernel_page_table_pa != 0) {
				uint64 slot = kernel_page_table_pa + (((va >> L1_SHIFT) & (L1_NUM_ENTRIES - 1)) * L1_ENTRY_SIZE);
				uint32 entry = (yield read_physical_buffer (slot, L1_ENTRY_SIZE, cancellable)).read_uint32 (0);
				if ((entry & L1_KIND_MASK) != L1_KIND_FAULT)
					return kernel_page_table_pa;
			}

			return p.l1_table_for (va);
		}

		private async uint64? translate_in_table (uint64 table_pa, uint64 va, Cancellable? cancellable)
				throws Error, IOError {
			uint64 l1_slot = table_pa + (((va >> L1_SHIFT) & (L1_NUM_ENTRIES - 1)) * L1_ENTRY_SIZE);
			uint32 l1 = (yield read_physical_buffer (l1_slot, L1_ENTRY_SIZE, cancellable)).read_uint32 (0);

			uint32 kind = l1 & L1_KIND_MASK;
			if (kind == L1_KIND_SECTION) {
				if ((l1 & SECTION_SUPER) != 0)
					return (l1 & SUPERSECTION_MASK) | (va & 0xffffff);
				return (l1 & SECTION_MASK) | (va & 0xfffff);
			}
			if (kind != L1_KIND_TABLE)
				return null;

			uint64 l2_slot = (l1 & L2_TABLE_MASK)
				+ (((va >> L2_SHIFT) & (L2_NUM_ENTRIES - 1)) * L2_ENTRY_SIZE);
			uint32 l2 = (yield read_physical_buffer (l2_slot, L2_ENTRY_SIZE, cancellable)).read_uint32 (0);

			uint32 leaf = l2 & L2_KIND_MASK;
			if (leaf == L2_KIND_FAULT)
				return null;
			if (leaf == L2_KIND_LARGE)
				return (l2 & LARGE_PAGE_MASK) | (va & 0xffff);
			return (l2 & SMALL_PAGE_MASK) | (va & 0xfff);
		}

		private async Buffer read_physical_buffer (uint64 pa, size_t size, Cancellable? cancellable) throws Error, IOError {
			return yield gdb.read_buffer (pa, size, cancellable);
		}

		private class MMUParameters {
			public uint64 tt0;
			public uint64 tt1;
			public uint n;
			public bool afe;

			public static async MMUParameters load (GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
				GDB.Exception? exception = gdb.exception;
				if (exception == null)
					throw new Error.INVALID_OPERATION ("Unable to query in current state");
				GDB.Thread thread = exception.thread;

				uint64 ttbcr = yield thread.read_register ("ttbcr", cancellable);
				if ((ttbcr & TTBCR_EAE) != 0)
					throw new Error.NOT_SUPPORTED ("LPAE translation tables are not supported; please open a PR");

				uint64 sctlr;
				try {
					sctlr = yield thread.read_register ("sctlr", cancellable);
				} catch (GLib.Error e) {
					sctlr = yield thread.read_register ("sctlr_el1", cancellable);
				}
				if ((sctlr & SCTLR_M) == 0)
					throw new Error.NOT_SUPPORTED ("The MMU is off");

				var p = new MMUParameters ();
				p.n = (uint) (ttbcr & TTBCR_N_MASK);
				p.afe = (sctlr & SCTLR_AFE) != 0;
				p.tt0 = (yield thread.read_register ("ttbr0", cancellable))
					& ~(((uint64) 1 << (L1_ALIGN_BITS - p.n)) - 1);
				p.tt1 = (yield thread.read_register ("ttbr1", cancellable))
					& ~(((uint64) 1 << L1_ALIGN_BITS) - 1);

				return p;
			}

			public uint split_index {
				get { return (n != 0) ? (1u << (L1_INDEX_BITS - n)) : L1_NUM_ENTRIES; }
			}

			public uint64 l1_table_for (uint64 va) {
				return ((va >> L1_SHIFT) < split_index) ? tt0 : tt1;
			}
		}

		public async Allocation allocate_pages (Gee.List<uint64?> physical_addresses, Cancellable? cancellable)
				throws Error, IOError {
			MMUParameters p = yield MMUParameters.load (gdb, cancellable);

			uint num_pages = physical_addresses.size;

			bool was_running = yield enter_physical_addressing (gdb, cancellable);
			var run = new Run (num_pages);

			Allocation? allocation = null;
			GLib.Error? failure = null;
			try {
				if (yield scan_for_run (run, p, cancellable))
					allocation = yield occupy (run, physical_addresses, cancellable);
			} catch (GLib.Error e) {
				failure = e;
			}
			yield leave_physical_addressing (gdb, was_running, cancellable);
			throw_if_failed (failure);

			if (allocation == null)
				throw new Error.NOT_SUPPORTED ("Unable to find a free run of page table entries");

			return allocation;
		}

		private async bool scan_for_run (Run run, MMUParameters p, Cancellable? cancellable) throws Error, IOError {
			uint boundary = p.split_index;

			if (boundary != L1_NUM_ENTRIES) {
				if (yield scan_for_run_in_l1 (run, p.tt1, boundary, L1_NUM_ENTRIES, p, cancellable))
					return true;
			} else {
				if (yield scan_for_run_in_l1 (run, p.tt0, KERNEL_FIRST_L1_INDEX, L1_NUM_ENTRIES, p,
						cancellable))
					return true;
			}

			run.reset ();

			return yield scan_for_run_in_l1 (run, p.tt0, 0, boundary, p, cancellable);
		}

		private async bool scan_for_run_in_l1 (Run run, uint64 table_address, uint first_index, uint end_index,
				MMUParameters p, Cancellable? cancellable) throws Error, IOError {
			Buffer l1 = yield read_physical_buffer (table_address + (first_index * L1_ENTRY_SIZE),
				(end_index - first_index) * L1_ENTRY_SIZE, cancellable);

			for (uint i = first_index; i != end_index; i++) {
				uint32 raw = l1.read_uint32 ((i - first_index) * L1_ENTRY_SIZE);
				if ((raw & L1_KIND_MASK) != L1_KIND_TABLE) {
					run.reset ();
					continue;
				}

				uint64 table_pa = raw & L2_TABLE_MASK;
				Buffer l2 = yield read_physical_buffer (table_pa, L2_NUM_ENTRIES * L2_ENTRY_SIZE, cancellable);

				for (uint j = 0; j != L2_NUM_ENTRIES; j++) {
					uint64 va = ((uint64) i << L1_SHIFT) | ((uint64) j << L2_SHIFT);
					if ((l2.read_uint32 (j * L2_ENTRY_SIZE) & L2_KIND_MASK) != L2_KIND_FAULT || va == 0) {
						run.reset ();
						continue;
					}

					run.take (va, table_pa + ((uint64) j * L2_ENTRY_SIZE));
					if (run.is_complete ())
						return true;
				}
			}

			return false;
		}

		private async Allocation occupy (Run run, Gee.List<uint64?> physical_addresses, Cancellable? cancellable)
				throws Error, IOError {
			var displaced = new Gee.ArrayList<DisplacedEntries> ();

			uint next_page = 0;
			foreach (RunSegment segment in run.segments) {
				var builder = gdb.make_buffer_builder ();
				for (uint i = 0; i != segment.num_entries; i++)
					builder.append_uint32 ((uint32) (physical_addresses[(int) (next_page + i)] | SMALL_PAGE_MAPPED));
				Bytes new_entries = builder.build ();

				Bytes old_entries = (yield read_physical_buffer (segment.slot_pa, new_entries.get_size (),
					cancellable)).bytes;
				yield write_physical_buffer (segment.slot_pa, new_entries, cancellable);

				displaced.add (new DisplacedEntries (segment.slot_pa, old_entries));
				next_page += segment.num_entries;
			}

			return new PageTableAllocation (run.va, run.count * SMALL_PAGE_SIZE, displaced, this);
		}

		public async void protect_pages (uint64 virtual_address, size_t size, Gum.PageProtection prot,
				Cancellable? cancellable) throws Error, IOError {
			MMUParameters p = yield MMUParameters.load (gdb, cancellable);

			uint64 start_va = page_start (virtual_address, SMALL_PAGE_SIZE);
			uint64 end_va = round_address_up (virtual_address + size, SMALL_PAGE_SIZE);

			bool was_running = yield enter_physical_addressing (gdb, cancellable);
			GLib.Error? failure = null;
			try {
				yield locate_kernel_page_table (p, cancellable);

				uint64 page_va = start_va;
				while (page_va < end_va) {
					uint64 l1_slot = (yield table_describing (page_va, p, cancellable))
						+ (((page_va >> L1_SHIFT) & (L1_NUM_ENTRIES - 1)) * L1_ENTRY_SIZE);
					uint32 l1_entry = (yield read_physical_buffer (l1_slot, L1_ENTRY_SIZE, cancellable))
						.read_uint32 (0);

					if ((l1_entry & L1_KIND_MASK) != L1_KIND_TABLE) {
						page_va = yield skip_section (page_va, end_va, l1_entry, prot, p, cancellable);
						continue;
					}

					uint64 table_pa = l1_entry & L2_TABLE_MASK;
					uint first_index = (uint) ((page_va >> L2_SHIFT) & (L2_NUM_ENTRIES - 1));
					uint64 remaining_here = (uint64) (L2_NUM_ENTRIES - first_index) * SMALL_PAGE_SIZE;
					uint64 chunk_end = uint64.min (end_va, page_va + remaining_here);
					uint num_pages = (uint) ((chunk_end - page_va) / SMALL_PAGE_SIZE);

					uint64 first_slot = table_pa + (first_index * L2_ENTRY_SIZE);
					Buffer current = yield read_physical_buffer (first_slot, num_pages * L2_ENTRY_SIZE,
						cancellable);

					var builder = gdb.make_buffer_builder ();
					bool changed = false;
					for (uint i = 0; i != num_pages; i++) {
						uint32 entry = current.read_uint32 (i * L2_ENTRY_SIZE);
						uint32 updated = apply_protection_to_page (entry, prot, p);
						changed = changed || updated != entry;
						builder.append_uint32 (updated);
					}
					if (changed)
						yield write_physical_buffer (first_slot, builder.build (), cancellable);

					page_va = chunk_end;
				}
			} catch (GLib.Error e) {
				failure = e;
			}
			yield leave_physical_addressing (gdb, was_running, cancellable);
			throw_if_failed (failure);
		}

		private async uint64 skip_section (uint64 page_va, uint64 end_va, uint32 l1_entry, Gum.PageProtection prot,
				MMUParameters p, Cancellable? cancellable) throws Error, IOError {
			if ((l1_entry & L1_KIND_MASK) != L1_KIND_SECTION) {
				throw new Error.NOT_SUPPORTED ("Unable to change protection of the unmapped page at 0x%"
					+ uint64.FORMAT_MODIFIER + "x", page_va);
			}

			uint ap = ((l1_entry >> SECTION_AP_SHIFT) & AP_LOW_MASK)
				| (((l1_entry >> SECTION_APX_SHIFT) & 1) << AP_HIGH_SHIFT);
			Gum.PageProtection current = protection_from_ap (ap, (l1_entry & SECTION_XN) != 0, p.afe);
			if ((current & prot) != prot) {
				throw new Error.NOT_SUPPORTED ("Unable to change protection of the section mapping at 0x%"
					+ uint64.FORMAT_MODIFIER + "x", page_va);
			}

			size_t span = ((l1_entry & SECTION_SUPER) != 0) ? SUPERSECTION_SIZE : SECTION_SIZE;
			return uint64.min (end_va, round_address_up (page_va + 1, span));
		}

		private static uint32 apply_protection_to_page (uint32 entry, Gum.PageProtection prot, MMUParameters p) {
			if ((entry & L2_KIND_MASK) == L2_KIND_FAULT)
				return entry;

			uint ap = (prot & Gum.PageProtection.WRITE) != 0 ? AP_READ_WRITE : AP_READ_ONLY;
			if (p.afe)
				ap = (prot & Gum.PageProtection.WRITE) != 0 ? AFE_AP_READ_WRITE : AFE_AP_READ_ONLY;

			uint32 updated = entry & ~(uint32) ((AP_LOW_MASK << PAGE_AP_SHIFT) | (1 << PAGE_APX_SHIFT));
			updated |= (ap & AP_LOW_MASK) << PAGE_AP_SHIFT;
			updated |= ((ap >> AP_HIGH_SHIFT) & 1) << PAGE_APX_SHIFT;

			if ((entry & L2_KIND_MASK) == L2_KIND_LARGE) {
				updated &= ~(uint32) LARGE_PAGE_XN;
				if ((prot & Gum.PageProtection.EXECUTE) == 0)
					updated |= LARGE_PAGE_XN;
			} else {
				updated &= ~(uint32) SMALL_PAGE_XN;
				if ((prot & Gum.PageProtection.EXECUTE) == 0)
					updated |= SMALL_PAGE_XN;
			}

			return updated;
		}

		private async void write_physical_buffer (uint64 pa, Bytes data, Cancellable? cancellable)
				throws Error, IOError {
			yield gdb.write_byte_array (pa, data, cancellable);
		}

		private class Run {
			public uint needed;
			public uint64 va;
			public uint count;
			public Gee.ArrayList<RunSegment> segments = new Gee.ArrayList<RunSegment> ();

			private uint64 next_va;

			public Run (uint needed) {
				this.needed = needed;
			}

			public void reset () {
				va = 0;
				count = 0;
				next_va = 0;
				segments.clear ();
			}

			public void take (uint64 page_va, uint64 slot_pa) {
				if (count != 0 && page_va != next_va)
					reset ();

				if (count == 0) {
					va = page_va;
					segments.add (new RunSegment (slot_pa));
				} else {
					RunSegment last = segments[segments.size - 1];
					if (slot_pa == last.slot_pa + (last.num_entries * L2_ENTRY_SIZE))
						last.num_entries++;
					else
						segments.add (new RunSegment (slot_pa));
				}

				count++;
				next_va = page_va + SMALL_PAGE_SIZE;
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

		private class PageTableAllocation : Object, Allocation {
			public uint64 virtual_address {
				get { return va; }
			}

			public size_t size {
				get { return allocated_size; }
			}

			private uint64 va;
			private size_t allocated_size;
			private Gee.List<DisplacedEntries> displaced;
			private weak ArmMachine machine;

			public PageTableAllocation (uint64 va, size_t allocated_size, Gee.List<DisplacedEntries> displaced,
					ArmMachine machine) {
				this.va = va;
				this.allocated_size = allocated_size;
				this.displaced = displaced;
				this.machine = machine;
			}

			public async void deallocate (Cancellable? cancellable) throws Error, IOError {
				bool was_running = yield enter_physical_addressing (machine.gdb, cancellable);
				GLib.Error? failure = null;
				try {
					foreach (DisplacedEntries d in displaced)
						yield machine.write_physical_buffer (d.slot_pa, d.entries, cancellable);
				} catch (GLib.Error e) {
					failure = e;
				}
				yield leave_physical_addressing (machine.gdb, was_running, cancellable);
				throw_if_failed (failure);
			}
		}

		public async Gee.List<uint64?> scan_ranges (Gee.List<Gum.MemoryRange?> ranges, MatchPattern pattern, uint max_matches,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public void apply_relocation (Gum.ElfRelocationDetails r, uint64 base_va, Buffer relocated) throws Error {
			Gum.ElfArmRelocation type = (Gum.ElfArmRelocation) r.type;
			size_t offset = (size_t) r.address;
			switch (type) {
				case ABS32:
				case RELATIVE:
					relocated.write_uint32 (offset, (uint32) (base_va + relocated.read_uint32 (offset)));
					break;
				case REL32:
					var diff = (int64) (base_va + r.symbol.address + relocated.read_uint32 (offset))
						- (int64) (base_va + r.address);
					relocated.write_int32 (offset, (int32) diff);
					break;
				default:
					throw new Error.NOT_SUPPORTED ("Unsupported relocation type: %s",
						Marshal.enum_to_nick<Gum.ElfArmRelocation> (type));
			}
		}

		public async uint64 invoke (uint64 impl, uint64[] args, Cancellable? cancellable) throws Error, IOError {
			if (args.length > NUM_ARGS_IN_REGS)
				throw new Error.NOT_SUPPORTED ("Unsupported number of arguments; please open a PR");

			bool was_running = gdb.state != STOPPED;
			if (was_running)
				yield gdb.stop (cancellable);

			GDB.Thread thread = gdb.exception.thread;
			Gee.Map<string, Variant> saved_regs = yield thread.read_registers (cancellable);

			var regs = new Gee.HashMap<string, Variant> ();
			regs.set_all (saved_regs);

			uint64 landing_zone = (call_landing_zone != 0) ? call_landing_zone : saved_regs["pc"].get_uint64 ();

			regs["pc"] = address_from_funcptr (impl);
			regs["lr"] = landing_zone;
			regs["cpsr"] = instruction_set_of (impl, saved_regs["cpsr"].get_uint64 ());

			for (uint i = 0; i != args.length; i++)
				regs["r%u".printf (i)] = args[i];

			yield thread.write_registers (regs, cancellable);

			GDB.Breakpoint bp = yield gdb.add_breakpoint (SOFT, address_from_funcptr (landing_zone),
				breakpoint_size_from_funcptr (landing_zone), cancellable);
			GDB.Exception ex = null;
			do {
				ex = yield gdb.continue_until_exception (cancellable);
			} while (ex.breakpoint != bp);
			yield bp.remove (cancellable);

			GDB.Thread landed = ex.thread;
			uint64 retval = yield landed.read_register ("r0", cancellable);

			yield landed.write_registers (saved_regs, cancellable);

			if (was_running)
				yield gdb.continue (cancellable);

			return retval;
		}

		private static uint64 instruction_set_of (uint64 funcptr, uint64 cpsr) {
			if ((funcptr & THUMB_BIT) != 0)
				return cpsr | CPSR_THUMB_BIT;
			return cpsr & ~CPSR_THUMB_BIT;
		}

		public async CallFrame load_call_frame (GDB.Thread thread, uint arity, Cancellable? cancellable) throws Error, IOError {
			var regs = yield thread.read_registers (cancellable);

			Buffer? stack = null;
			uint64 original_sp = regs["sp"].get_uint64 ();
			if (arity > NUM_ARGS_IN_REGS)
				stack = yield gdb.read_buffer (original_sp, (arity - NUM_ARGS_IN_REGS) * 4, cancellable);

			return new ArmCallFrame (thread, regs, stack, original_sp);
		}

		private class ArmCallFrame : Object, CallFrame {
			public uint64 return_address {
				get { return regs["lr"].get_uint64 (); }
			}

			public Gee.Map<string, Variant> registers {
				get { return regs; }
			}

			private GDB.Thread thread;

			private Gee.Map<string, Variant> regs;

			private Buffer? stack;
			private uint64 original_sp;
			private State stack_state = PRISTINE;

			private enum State {
				PRISTINE,
				MODIFIED
			}

			public ArmCallFrame (GDB.Thread thread, Gee.Map<string, Variant> regs, Buffer? stack, uint64 original_sp) {
				this.thread = thread;

				this.regs = regs;

				this.stack = stack;
				this.original_sp = original_sp;
			}

			public uint64 get_nth_argument (uint n) {
				if (n < NUM_ARGS_IN_REGS)
					return regs["r%u".printf (n)].get_uint64 ();

				size_t offset;
				if (try_get_stack_offset_of_nth_argument (n, out offset))
					return stack.read_uint32 (offset);

				return uint64.MAX;
			}

			public void replace_nth_argument (uint n, uint64 val) {
				if (n < NUM_ARGS_IN_REGS) {
					regs["r%u".printf (n)] = val;
					invalidate_regs ();
					return;
				}

				size_t offset;
				if (try_get_stack_offset_of_nth_argument (n, out offset)) {
					stack.write_uint32 (offset, (uint32) val);
					invalidate_stack ();
				}
			}

			private bool try_get_stack_offset_of_nth_argument (uint n, out size_t offset) {
				offset = 0;

				if (stack == null || n < NUM_ARGS_IN_REGS)
					return false;
				size_t start = (n - NUM_ARGS_IN_REGS) * 4;
				size_t end = start + 4;
				if (end > stack.bytes.get_size ())
					return false;

				offset = start;
				return true;
			}

			public uint64 get_return_value () {
				return regs["r0"].get_uint64 ();
			}

			public void replace_return_value (uint64 retval) {
				regs["r0"] = retval;
				invalidate_regs ();
			}

			public void force_return () {
				regs["pc"] = return_address;
				invalidate_regs ();
			}

			private void invalidate_regs () {
				regs.set_data ("dirty", true);
			}

			private void invalidate_stack () {
				stack_state = MODIFIED;
			}

			public async void commit (Cancellable? cancellable) throws Error, IOError {
				if (regs.get_data<bool> ("dirty"))
					yield thread.write_registers (regs, cancellable);

				if (stack_state == MODIFIED)
					yield thread.client.write_byte_array (original_sp, stack.bytes, cancellable);
			}
		}

		private class ArmInlineHook : Object, InlineHook {
			private State state = DISABLED;
			private uint64 target;
			private Bytes old_target_code;
			private Bytes new_target_code;
			private Allocation allocation;
			private GDB.Client gdb;

			private enum State {
				DISABLED,
				ENABLED,
				DESTROYED
			}

			public ArmInlineHook (uint64 target, Bytes old_target_code, Bytes new_target_code, Allocation allocation,
					GDB.Client gdb) {
				this.target = target;
				this.old_target_code = old_target_code;
				this.new_target_code = new_target_code;
				this.allocation = allocation;
				this.gdb = gdb;
			}

			public async void destroy (Cancellable? cancellable) throws Error, IOError {
				if (state == DESTROYED)
					return;

				bool was_running = gdb.state != STOPPED;
				if (was_running)
					yield gdb.stop (cancellable);

				yield disable (cancellable);
				yield allocation.deallocate (cancellable);
				state = DESTROYED;

				if (was_running)
					yield gdb.continue (cancellable);
			}

			public async void enable (Cancellable? cancellable) throws Error, IOError {
				if (state == ENABLED)
					return;
				if (state != DISABLED)
					throw new Error.INVALID_OPERATION ("Invalid operation");
				yield gdb.write_byte_array (target, new_target_code, cancellable);
				state = ENABLED;
			}

			public async void disable (Cancellable? cancellable) throws Error, IOError {
				if (state != ENABLED)
					return;
				yield gdb.write_byte_array (target, old_target_code, cancellable);
				state = DISABLED;
			}
		}

		public uint64 address_from_funcptr (uint64 ptr) {
			return ptr & ~THUMB_BIT;
		}

		public size_t breakpoint_size_from_funcptr (uint64 ptr) {
			return ((ptr & THUMB_BIT) != 0) ? 2 : 4;
		}

		public async InlineHook create_inline_hook (uint64 target, uint64 handler, Allocator allocator,
				Cancellable? cancellable) throws Error, IOError {
			uint64 target_address = address_from_funcptr (target);
			bool target_is_thumb = (target & THUMB_BIT) != 0;

			size_t page_size = allocator.page_size;
			var allocation = yield allocator.allocate (page_size, page_size, cancellable);
			uint64 code_va = allocation.virtual_address;

			var scratch_buf = new uint8[512];

			Bytes old_target_code, trampoline_code, new_target_code;
			if (target_is_thumb) {
				var tw = new Gum.ThumbWriter (scratch_buf);
				tw.flush_on_destroy = false;
				tw.pc = code_va | THUMB_BIT;

				void * on_invoke_label = tw.code;

				uint64 on_enter_trampoline = tw.pc;
				emit_thumb_prolog (tw);

				tw.put_call_address_with_arguments (handler, 1,
					Gum.ArgType.REGISTER, Gum.ArmReg.SP);

				emit_thumb_epilog (tw, on_invoke_label);

				size_t redirect_size = tw.can_branch_directly_between (target_address, on_enter_trampoline) ? 4 : 8;

				old_target_code = yield gdb.read_byte_array (target_address, redirect_size, cancellable);

				tw.put_label (on_invoke_label);
				var rl = new Gum.ThumbRelocator (old_target_code.get_data (), tw);
				rl.input_pc = target_address;
				uint reloc_bytes = 0;
				do
					reloc_bytes = rl.read_one ();
				while (reloc_bytes < redirect_size);
				rl.write_all ();
				if (!rl.eoi)
					tw.put_branch_address ((target_address + reloc_bytes) | THUMB_BIT);
				tw.flush ();
				trampoline_code = new Bytes (scratch_buf[:tw.offset ()]);

				tw.reset (scratch_buf);
				tw.pc = target_address | THUMB_BIT;
				tw.put_branch_address (on_enter_trampoline);
				tw.flush ();
				new_target_code = new Bytes (scratch_buf[:tw.offset ()]);
			} else {
				var aw = new Gum.ArmWriter (scratch_buf);
				aw.flush_on_destroy = false;
				aw.pc = code_va;

				void * on_invoke_label = aw.code;

				uint64 on_enter_trampoline = aw.pc;
				emit_arm_prolog (aw);

				aw.put_call_address_with_arguments (handler, 1,
					Gum.ArgType.REGISTER, Gum.ArmReg.SP);

				emit_arm_epilog (aw, on_invoke_label);

				size_t redirect_size = aw.can_branch_directly_between (target_address, on_enter_trampoline) ? 4 : 8;

				old_target_code = yield gdb.read_byte_array (target_address, redirect_size, cancellable);

				aw.put_label (on_invoke_label);
				var rl = new Gum.ArmRelocator (old_target_code.get_data (), aw);
				rl.input_pc = target_address;
				uint reloc_bytes = 0;
				do
					reloc_bytes = rl.read_one ();
				while (reloc_bytes < redirect_size);
				rl.write_all ();
				if (!rl.eoi)
					aw.put_branch_address (target_address + reloc_bytes);
				aw.flush ();
				trampoline_code = new Bytes (scratch_buf[:aw.offset ()]);

				aw.reset (scratch_buf);
				aw.pc = target_address;
				aw.put_branch_address (on_enter_trampoline);
				aw.flush ();
				new_target_code = new Bytes (scratch_buf[:aw.offset ()]);
			}

			yield gdb.write_byte_array (code_va, trampoline_code, cancellable);

			return new ArmInlineHook (target_address, old_target_code, new_target_code, allocation, gdb);
		}

		private static void emit_thumb_prolog (Gum.ThumbWriter tw) {
			tw.put_push_regs (9,
				Gum.ArmReg.R0, Gum.ArmReg.R1, Gum.ArmReg.R2,
				Gum.ArmReg.R3, Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R6, Gum.ArmReg.R7, Gum.ArmReg.LR);

			tw.put_mov_reg_cpsr (Gum.ArmReg.R5);
			tw.put_add_reg_reg_imm (Gum.ArmReg.R4, Gum.ArmReg.SP, 9 * 4);

			tw.put_sub_reg_imm (Gum.ArmReg.SP, 4);
			tw.put_vpush_range (Gum.ArmReg.Q8, Gum.ArmReg.Q15);
			tw.put_vpush_range (Gum.ArmReg.Q0, Gum.ArmReg.Q7);

			tw.put_push_regs (7,
				Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R8, Gum.ArmReg.R9, Gum.ArmReg.R10, Gum.ArmReg.R11, Gum.ArmReg.R12);

			tw.put_sub_reg_imm (Gum.ArmReg.SP, 3 * 4);
		}

		private static void emit_thumb_epilog (Gum.ThumbWriter tw, void * next_hop_label) {
			tw.put_add_reg_imm (Gum.ArmReg.SP, 3 * 4);

			tw.put_pop_regs (7,
				Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R8, Gum.ArmReg.R9, Gum.ArmReg.R10, Gum.ArmReg.R11, Gum.ArmReg.R12);

			tw.put_vpop_range (Gum.ArmReg.Q0, Gum.ArmReg.Q7);
			tw.put_vpop_range (Gum.ArmReg.Q8, Gum.ArmReg.Q15);
			tw.put_add_reg_imm (Gum.ArmReg.SP, 4);

			tw.put_mov_cpsr_reg (Gum.ArmReg.R5);

			tw.put_pop_regs (9,
				Gum.ArmReg.R0, Gum.ArmReg.R1, Gum.ArmReg.R2,
				Gum.ArmReg.R3, Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R6, Gum.ArmReg.R7, Gum.ArmReg.LR);

			tw.put_b_label (next_hop_label);
		}

		private static void emit_arm_prolog (Gum.ArmWriter aw) {
			aw.put_push_regs (9,
				Gum.ArmReg.R0, Gum.ArmReg.R1, Gum.ArmReg.R2,
				Gum.ArmReg.R3, Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R6, Gum.ArmReg.R7, Gum.ArmReg.LR);

			aw.put_mov_reg_cpsr (Gum.ArmReg.R5);
			aw.put_add_reg_reg_imm (Gum.ArmReg.R4, Gum.ArmReg.SP, 9 * 4);

			aw.put_sub_reg_u16 (Gum.ArmReg.SP, 4);
			aw.put_vpush_range (Gum.ArmReg.Q8, Gum.ArmReg.Q15);
			aw.put_vpush_range (Gum.ArmReg.Q0, Gum.ArmReg.Q7);

			aw.put_push_regs (7,
				Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R8, Gum.ArmReg.R9, Gum.ArmReg.R10, Gum.ArmReg.R11, Gum.ArmReg.R12);

			aw.put_sub_reg_u16 (Gum.ArmReg.SP, 3 * 4);
		}

		private static void emit_arm_epilog (Gum.ArmWriter aw, void * next_hop_label) {
			aw.put_add_reg_u16 (Gum.ArmReg.SP, 3 * 4);

			aw.put_pop_regs (7,
				Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R8, Gum.ArmReg.R9, Gum.ArmReg.R10, Gum.ArmReg.R11, Gum.ArmReg.R12);

			aw.put_vpop_range (Gum.ArmReg.Q0, Gum.ArmReg.Q7);
			aw.put_vpop_range (Gum.ArmReg.Q8, Gum.ArmReg.Q15);
			aw.put_add_reg_u16 (Gum.ArmReg.SP, 4);

			aw.put_mov_cpsr_reg (Gum.ArmReg.R5);

			aw.put_pop_regs (9,
				Gum.ArmReg.R0, Gum.ArmReg.R1, Gum.ArmReg.R2,
				Gum.ArmReg.R3, Gum.ArmReg.R4, Gum.ArmReg.R5,
				Gum.ArmReg.R6, Gum.ArmReg.R7, Gum.ArmReg.LR);

			aw.put_b_label (next_hop_label);
		}
	}
}
