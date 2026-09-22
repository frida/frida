public sealed class Frida.SymbolResolver : Object {
	private Gee.Map<uint, ProcMapsSnapshot> cache = new Gee.HashMap<uint, ProcMapsSnapshot> ();

	private Gee.Map<uint, Gee.Map<uint32, PendingMapEvent>> pending = new Gee.HashMap<uint, Gee.Map<uint32, PendingMapEvent>> ();
	private Gee.Set<uint> incomplete = new Gee.HashSet<uint> ();

	private class PendingMapEvent {
		public bool is_create;

		public uint32 gen;
		public uint64 start;
		public uint64 end;

		public uint64 file_offset;
		public uint64 vm_flags;
		public DevId device;
		public uint64 inode;
		public string? path;
	}

	private const uint MAX_PENDING = 256;

	public void resolve_addresses (uint pid, uint32 gen, uint64[] addresses, AddressResolved on_symbol, ModulesReady on_modules)
			throws Error {
		var snap = get_snapshot (pid);

		var used_index = new Gee.HashMap<string, uint> ();
		var used_modules = new Gee.ArrayList<string> ();

		foreach (var addr in addresses) {
			ProcMapsSnapshot.Mapping? m = snap.find_mapping_at_gen (addr, gen);
			if (m == null || m.path == "") {
				on_symbol (addr, uint32.MAX, 0);
				continue;
			}

			uint mod_idx;
			if (!used_index.has_key (m.path)) {
				mod_idx = used_modules.size;
				used_index[m.path] = mod_idx;
				used_modules.add (m.path);
			} else {
				mod_idx = used_index[m.path];
			}

			uint64 rel64 = (addr - m.start) + m.file_offset;
			uint32 rel32 = (rel64 <= uint32.MAX) ? (uint32) rel64 : 0;

			on_symbol (addr, mod_idx, rel32);
		}

		on_modules (used_modules);
	}

	public delegate void AddressResolved (uint64 address, uint module_index, uint32 rel);
	public delegate void ModulesReady (Gee.List<string> modules);

	private ProcMapsSnapshot get_snapshot (uint pid) throws Error {
		var s = cache[pid];
		if (s == null)
			return refresh_snapshot (pid);
		return s;
	}

	public ProcMapsSnapshot refresh_snapshot (uint pid) throws Error {
		var s = ProcMapsSnapshot.from_pid (pid);
		cache[pid] = s;

		pending.unset (pid);
		incomplete.remove (pid);

		return s;
	}

	public void apply_map_create (uint32 pid, uint32 gen, uint64 start, uint64 end, uint64 file_offset, uint64 vm_flags, DevId device,
			uint64 inode, string? path) {
		var snap = cache[pid];
		if (snap == null)
			return;

		if (gen <= snap.gen)
			return;

		var q = pending[pid];
		if (q == null) {
			q = new Gee.HashMap<uint32, PendingMapEvent> ();
			pending[pid] = q;
		}

		var ev = new PendingMapEvent ();
		ev.is_create = true;
		ev.gen = gen;
		ev.start = start;
		ev.end = end;
		ev.file_offset = file_offset;
		ev.vm_flags = vm_flags;
		ev.device = device;
		ev.inode = inode;
		ev.path = path;

		q[gen] = ev;

		drain_map_events (pid, snap, q);
	}

	public void apply_map_destroy_range (uint32 pid, uint32 gen, uint64 start, uint64 end) {
		var snap = cache[pid];
		if (snap == null)
			return;

		if (gen <= snap.gen)
			return;

		var q = pending[pid];
		if (q == null) {
			q = new Gee.HashMap<uint32, PendingMapEvent> ();
			pending[pid] = q;
		}

		var ev = new PendingMapEvent ();
		ev.is_create = false;
		ev.gen = gen;
		ev.start = start;
		ev.end = end;

		q[gen] = ev;

		drain_map_events (pid, snap, q);
	}

	private void drain_map_events (uint32 pid, ProcMapsSnapshot snap, Gee.Map<uint32, PendingMapEvent> q) {
		if (q.size > MAX_PENDING) {
			incomplete.add (pid);
			q.clear ();
			return;
		}

		while (true) {
			uint32 next_gen = snap.gen + 1;

			PendingMapEvent? ev = q[next_gen];
			if (ev == null)
				break;

			q.unset (next_gen);

			if (ev.is_create) {
				string path = make_path (ev.device, ev.inode, ev.path);
				snap.apply_create (next_gen, ev.start, ev.end, ev.file_offset, ev.vm_flags, ev.device, ev.inode, path);
			} else {
				snap.apply_destroy_range (next_gen, ev.start, ev.end);
			}
		}
	}

	public void invalidate (uint32 pid) {
		cache.unset (pid);
	}

	public void clear () {
		cache.clear ();
	}

	private static string make_path (DevId device, uint64 inode, string? path) {
		if (path != null)
			return path;

		return ("<%x:%x:%" + uint64.FORMAT_MODIFIER + "u>").printf (device.major, device.minor, inode);
	}
}
