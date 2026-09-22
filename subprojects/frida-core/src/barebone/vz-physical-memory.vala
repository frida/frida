[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	/**
	 * Direct host access to the guest's physical RAM, bypassing the kernel GDB stub.
	 *
	 * Apple's Virtualization daemon maps the whole guest RAM into its own address space and
	 * records the layout in the Hv::Vm singleton's vector<HvCore::SharedRamEntry>. We acquire
	 * the daemon's task port, find Hv::Vm by its vtable, walk that vector to build a guest-PA
	 * -> host-VA map, and then read and write guest physical memory with a single mach_vm call
	 * instead of thousands of RSP packets. The stub is left to do what only it can: drive the
	 * vCPUs.
	 */
	public sealed class VzPhysicalMemory : Object, PhysicalMemory {
		private Gum.DarwinPort task;
		private uint64 instance_address;
		private Gee.ArrayList<SharedRamEntry?> entries = new Gee.ArrayList<SharedRamEntry?> ();

		private const uint64 VPTR_MASK = 0x00007ffffffffff8;
		private const uint64 PTR_MASK = 0x00ffffffffffffff;
		private const uint64 GUEST_RAM_CHUNK_SIZE = 64 * 1024 * 1024;
		private const uint64 MAX_HEAP_ADDRESS = 0x400000000;
		private const uint64 SCAN_WINDOW = 16 * 1024 * 1024;
		private const uint64 SHARED_RAM_VECTOR_OFFSET = 0x30;
		private const uint64 SHARED_RAM_ENTRY_SIZE = 32;
		private const uint64 MAX_SHARED_RAM_ENTRIES = 1024;
		private const uint64 MIN_HOST_VA = 0x10000000;
		private const uint64 MAPPING_ALIGNMENT = 0x4000;
		private const uint MAX_DISCOVERY_ATTEMPTS = 10;

		public static VzPhysicalMemory open (uint pid) throws Error {
			var memory = new VzPhysicalMemory ();

			if (Gum.Darwin.task_for_pid (Gum.Darwin.mach_task_self (), (int) pid, out memory.task) != Gum.Darwin.Status.SUCCESS) {
				throw new Error.PERMISSION_DENIED (
					"Unable to acquire the task port for the Virtualization daemon (pid %u); " +
					"frida-core needs root or the com.apple.system-task-ports entitlement", pid);
			}

			for (uint attempt = 0; true; attempt++) {
				try {
					memory.discover ();
					return memory;
				} catch (Error e) {
					if (attempt == MAX_DISCOVERY_ATTEMPTS - 1)
						throw e;
				}
			}
		}

		private void discover () throws Error {
			uint64 vptr = (resolve_hv_vm_vtable () + 16) & VPTR_MASK;
			foreach (uint64 instance in find_vptr_candidates (vptr)) {
				if (try_load_shared_ram (instance))
					return;
			}
			throw new Error.NOT_SUPPORTED ("Unable to locate a populated Hv::Vm guest-RAM map in the daemon");
		}

		private bool try_load_shared_ram (uint64 instance) {
			var loaded = read_shared_ram (instance);
			if (loaded == null)
				return false;
			instance_address = instance;
			entries = loaded;
			return true;
		}

		// Validate the vector by shape, not size: the daemon reclaims guest RAM while the VM is idle
		// so a real Hv::Vm can momentarily expose only the low-memory entry. Page-aligned sizes and
		// plausible host VAs are what separate the real map from a random vtable-shaped false match.
		// Non-throwing on purpose: a candidate vptr may point at anything, so every failed daemon read
		// just means "not this one" — surfacing it as an exception only invites error-propagation faults.
		private Gee.ArrayList<SharedRamEntry?>? read_shared_ram (uint64 instance) {
			uint64 begin, end;
			if (!try_read_u64 (instance + SHARED_RAM_VECTOR_OFFSET, out begin))
				return null;
			if (!try_read_u64 (instance + SHARED_RAM_VECTOR_OFFSET + 8, out end))
				return null;
			begin &= PTR_MASK;
			end &= PTR_MASK;
			if (end <= begin || (end - begin) % SHARED_RAM_ENTRY_SIZE != 0)
				return null;
			if ((end - begin) / SHARED_RAM_ENTRY_SIZE > MAX_SHARED_RAM_ENTRIES)
				return null;

			var result = new Gee.ArrayList<SharedRamEntry?> ();
			for (uint64 cursor = begin; cursor < end; cursor += SHARED_RAM_ENTRY_SIZE) {
				uint64 host_va, guest_pa, size;
				if (!try_read_u64 (cursor, out host_va))
					return null;
				if (!try_read_u64 (cursor + 8, out guest_pa))
					return null;
				if (!try_read_u64 (cursor + 16, out size))
					return null;
				var entry = SharedRamEntry () { host_va = host_va & PTR_MASK, guest_pa = guest_pa, size = size };
				if (entry.size == 0 || entry.size % MAPPING_ALIGNMENT != 0)
					return null;
				// Device/MMIO regions appear with no host backing; keep only the host-backed RAM ranges.
				if (entry.host_va == 0)
					continue;
				if (entry.host_va < MIN_HOST_VA)
					return null;
				result.add (entry);
			}
			return result;
		}

		private void refresh () {
			var loaded = read_shared_ram (instance_address);
			if (loaded != null)
				entries = loaded;
		}

		private static uint64 resolve_hv_vm_vtable () throws Error {
			Gum.Module module;
			try {
				module = Gum.Module.load ("/System/Library/Frameworks/Hypervisor.framework/Hypervisor");
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("Unable to load Hypervisor.framework: %s", e.message);
			}

			Gum.Address vtable = module.find_symbol_by_name ("_ZTVN2Hv2VmE");
			if (vtable == 0)
				throw new Error.NOT_SUPPORTED ("Unable to resolve the Hv::Vm vtable symbol");
			return vtable;
		}

		private Gee.List<uint64?> find_vptr_candidates (uint64 vptr) {
			var candidates = new Gee.ArrayList<uint64?> ();
			Gum.Darwin.enumerate_ranges (task, READ, details => {
				// Hv::Vm lives in the daemon's regular malloc heap; the high guest-RAM-mapped regions
				// up at tens of GB block mach_vm_read while the VM is running, so stay below them.
				if (details.range.base_address < MAX_HEAP_ADDRESS && details.range.size < GUEST_RAM_CHUNK_SIZE)
					scan_span_for_vptr (details.range.base_address, details.range.base_address + details.range.size, vptr, candidates);
				return true;
			});
			return candidates;
		}

		// find_pointers scans this process's own memory, so mirror each daemon window into a local
		// buffer and let gum's tiled scanner sweep it for the masked vptr, then map hits back to the
		// daemon address.
		private void scan_span_for_vptr (uint64 start, uint64 end, uint64 vptr, Gee.List<uint64?> candidates) {
			size_t[] values = { (size_t) vptr };
			for (uint64 pos = start; pos < end; pos += SCAN_WINDOW) {
				size_t length = (size_t) uint64.min (SCAN_WINDOW, end - pos);
				uint8[]? window = Gum.Darwin.read (task, pos, length);
				if (window == null)
					continue;
				Gum.Address window_base = (Gum.Address) (uintptr) (void *) window;
				Gum.MemoryRange[] ranges = { { window_base, window.length } };
				var matches = Gum.Memory.find_pointers (ranges, values, (size_t) VPTR_MASK);
				for (uint i = 0; i != matches.length; i++)
					candidates.add (pos + (matches.index (i).address - window_base));
			}
		}

		public uint8[] read (uint64 pa, size_t size) throws Error {
			SharedRamEntry entry = entry_for (pa, size);
			uint8[]? data = Gum.Darwin.read (task, entry.host_va + (pa - entry.guest_pa), size);
			if (data == null)
				throw new Error.INVALID_ARGUMENT ("Unable to read guest physical memory at PA 0x%" + uint64.FORMAT_MODIFIER + "x", pa);
			return data;
		}

		public void write (uint64 pa, uint8[] data) throws Error {
			SharedRamEntry entry = entry_for (pa, data.length);
			if (!Gum.Darwin.write (task, entry.host_va + (pa - entry.guest_pa), data))
				throw new Error.INVALID_ARGUMENT ("Unable to write guest physical memory at PA 0x%" + uint64.FORMAT_MODIFIER + "x", pa);
		}

		public bool contains (uint64 pa) {
			if (!is_backed (pa))
				refresh ();
			return is_backed (pa);
		}

		private bool is_backed (uint64 pa) {
			foreach (var entry in entries) {
				if (pa >= entry.guest_pa && pa < entry.guest_pa + entry.size)
					return true;
			}
			return false;
		}

		private SharedRamEntry entry_for (uint64 pa, size_t length) throws Error {
			SharedRamEntry? entry = covering_entry (pa, length);
			if (entry == null) {
				refresh ();
				entry = covering_entry (pa, length);
			}
			if (entry == null)
				throw new Error.INVALID_ARGUMENT ("Guest PA 0x%" + uint64.FORMAT_MODIFIER + "x is not backed by guest RAM", pa);
			return entry;
		}

		private SharedRamEntry? covering_entry (uint64 pa, size_t length) {
			foreach (var entry in entries) {
				if (pa >= entry.guest_pa && pa + length <= entry.guest_pa + entry.size)
					return entry;
			}
			return null;
		}

		private bool try_read_u64 (uint64 address, out uint64 value) {
			value = 0;
			uint8[]? buffer = Gum.Darwin.read (task, address, 8);
			if (buffer == null || buffer.length < 8)
				return false;
			uint64 v = 0;
			for (int i = 7; i >= 0; i--)
				v = (v << 8) | buffer[i];
			value = v;
			return true;
		}

		private struct SharedRamEntry {
			public uint64 host_va;
			public uint64 guest_pa;
			public uint64 size;
		}
	}
}
