namespace Frida {
	public sealed class ProcMapsSnapshot : Object {
		public uint32 gen {
			get;
			internal set;
			default = 1;
		}

		private Gee.List<Mapping> maps = new Gee.ArrayList<Mapping> ();

		private const uint64 VM_READ   = 1;
		private const uint64 VM_WRITE  = 2;
		private const uint64 VM_EXEC   = 4;
		private const uint64 VM_SHARED = 8;

		public class Mapping {
			public uint64 start;
			public uint64 end;
			public size_t size {
				get {
					return (size_t) (end - start);
				}
			}
			public uint64 file_offset;

			public bool readable;
			public bool writable;
			public bool executable;
			public bool shared;

			public DevId device;
			public uint64 inode;

			public string path;

			public uint32 start_gen;
			public uint32 end_gen;
		}

		public static ProcMapsSnapshot from_pid (uint32 pid) {
			var snap = new ProcMapsSnapshot ();
			snap.build_from_pid (pid);
			return snap;
		}

		private void build_from_pid (uint32 pid) {
			maps.clear ();

			var it = ProcMapsIter.for_pid (pid);

			while (it.next ()) {
				var m = new Mapping ();

				m.start = it.start_address;
				m.end = it.end_address;
				m.file_offset = it.file_offset;

				string flags = it.flags;
				m.readable = flags[0] == 'r';
				m.writable = flags[1] == 'w';
				m.executable = flags[2] == 'x';
				m.shared = flags[3] == 's';

				m.device = it.device;
				m.inode = it.inode;

				m.path = normalize_path (it.path);

				m.start_gen = 1;
				m.end_gen = 0;

				maps.add (m);
			}
		}

		public void apply_create (uint32 new_gen, uint64 start, uint64 end, uint64 file_offset, uint64 vm_flags, DevId device,
				uint64 inode, string path) {
			end_range_at_gen (start, end, new_gen);

			var m = new Mapping ();

			m.start = start;
			m.end = end;
			m.file_offset = file_offset;

			m.readable = (vm_flags & VM_READ) != 0;
			m.writable = (vm_flags & VM_WRITE) != 0;
			m.executable = (vm_flags & VM_EXEC) != 0;
			m.shared = (vm_flags & VM_SHARED) != 0;

			m.device = device;
			m.inode = inode;

			m.path = path;

			m.start_gen = new_gen;
			m.end_gen = 0;

			insert_sorted (m);
			gen = new_gen;
		}

		public void apply_destroy_range (uint32 new_gen, uint64 start, uint64 end) {
			end_range_at_gen (start, end, new_gen);
			gen = new_gen;
		}

		private void insert_sorted (Mapping m) {
			int i = 0;
			while (i < maps.size && maps[i].start < m.start)
				i++;
			maps.insert (i, m);
		}

		private void end_range_at_gen (uint64 start, uint64 end, uint32 gen) {
			for (int i = maps.size - 1; i >= 0; i--) {
				var m = maps[i];

				bool active = m.end_gen == 0;
				if (!active)
					continue;

				bool disjoint = end <= m.start || start >= m.end;
				if (disjoint)
					continue;

				bool full_cover = start <= m.start && end >= m.end;
				if (full_cover) {
					m.end_gen = gen;
					continue;
				}

				bool chop_head = start <= m.start && end < m.end;
				if (chop_head) {
					var dead = new Mapping ();
					dead.start = m.start;
					dead.end = end;
					dead.file_offset = m.file_offset;
					dead.device = m.device;
					dead.inode = m.inode;
					dead.path = m.path;
					dead.start_gen = m.start_gen;
					dead.end_gen = gen;

					uint64 delta = end - m.start;
					m.start = end;
					m.file_offset += delta;

					maps.insert (i + 1, dead);
					continue;
				}

				bool chop_tail = start > m.start && end >= m.end;
				if (chop_tail) {
					var dead = new Mapping ();
					dead.start = start;
					dead.end = m.end;
					dead.file_offset = m.file_offset + (start - m.start);
					dead.device = m.device;
					dead.inode = m.inode;
					dead.path = m.path;
					dead.start_gen = m.start_gen;
					dead.end_gen = gen;

					m.end = start;

					maps.insert (i + 1, dead);
					continue;
				}

				bool split_middle = start > m.start && end < m.end;
				if (split_middle) {
					var dead = new Mapping ();
					dead.start = start;
					dead.end = end;
					dead.file_offset = m.file_offset + (start - m.start);
					dead.device = m.device;
					dead.inode = m.inode;
					dead.path = m.path;
					dead.start_gen = m.start_gen;
					dead.end_gen = gen;

					var right = new Mapping ();
					right.start = end;
					right.end = m.end;
					right.file_offset = m.file_offset + (end - m.start);
					right.device = m.device;
					right.inode = m.inode;
					right.path = m.path;
					right.start_gen = m.start_gen;
					right.end_gen = 0;

					m.end = start;

					maps.insert (i + 1, dead);
					maps.insert (i + 2, right);
				}
			}
		}

		private static string normalize_path (string path) {
			const string suffix = " (deleted)";
			if (path.has_suffix (suffix))
				return path.substring (0, path.length - suffix.length);
			return path;
		}

		public Gee.Iterator<Mapping> iterator () {
			return maps.iterator ();
		}

		public Mapping? find_mapping (uint64 addr) {
			return find_mapping_at_gen (addr, gen);
		}

		public Mapping? find_mapping_at_gen (uint64 addr, uint32 gen) {
			int lo = 0;
			int hi = maps.size - 1;

			while (lo <= hi) {
				int mid = lo + ((hi - lo) / 2);
				var m = maps[mid];

				if (addr < m.start)
					hi = mid - 1;
				else if (addr >= m.end)
					lo = mid + 1;
				else {
					for (int i = mid; i >= 0; i--) {
						var c = maps[i];
						if (addr < c.start)
							break;

						bool match = mapping_contains (c, addr) && mapping_alive_at (c, gen);
						if (match)
							return c;
					}

					for (int i = mid + 1; i < maps.size; i++) {
						var c = maps[i];
						if (addr < c.start)
							break;

						bool match = mapping_contains (c, addr) && mapping_alive_at (c, gen);
						if (match)
							return c;
					}

					return null;
				}
			}

			return null;
		}

		private bool mapping_contains (Mapping m, uint64 addr) {
			return addr >= m.start && addr < m.end;
		}

		private bool mapping_alive_at (Mapping m, uint32 gen) {
			bool started = m.start_gen <= gen;
			bool not_ended = m.end_gen == 0 || gen < m.end_gen;
			return started && not_ended;
		}

		public Mapping? find_module_by_path (string path) {
			return find_module_by_path_at_gen (path, gen);
		}

		public Mapping? find_module_by_path_at_gen (string path, uint32 at_gen) {
			Mapping? best_base = null;
			int best_score = int.MIN;

			Mapping? cur_base = null;
			uint cur_total = 0;
			uint cur_exec = 0;

#if ANDROID
			unowned string libc_path = Gum.Process.get_libc_module ().path;
#endif

			for (int i = 0; i < maps.size; i++) {
				var m = maps[i];
				if (!mapping_alive_at (m, at_gen))
					continue;

				unowned string mp = m.path;

				if (mp == "[page size compat]")
					continue;

				if (mp != path) {
					consider_candidate (ref best_base, ref best_score, cur_base, cur_total, cur_exec);
					cur_base = null;
					cur_total = 0;
					cur_exec = 0;
					continue;
				}

#if ANDROID
				if (mp == libc_path && m.shared)
					continue;
#endif

				if (m.file_offset == 0) {
					consider_candidate (ref best_base, ref best_score, cur_base, cur_total, cur_exec);
					cur_base = m;
					cur_total = 0;
					cur_exec = 0;
				}

				if (cur_base != null) {
					cur_total++;
					if (m.executable)
						cur_exec++;
				}
			}

			consider_candidate (ref best_base, ref best_score, cur_base, cur_total, cur_exec);

			return best_base;
		}

		private static void consider_candidate (ref Mapping? best_base, ref int best_score, Mapping? candidate_base, uint total,
				uint exec) {
			if (candidate_base == null)
				return;

			int score = score_candidate (total, exec);
			if (score > best_score) {
				best_score = score;
				best_base = candidate_base;
			}
		}

		private static int score_candidate (uint total, uint exec) {
			int score = (int) total;
			if (exec == 0)
				score = -score;
			return score;
		}
	}

	public sealed class ProcMapsIter {
		private string? contents;
		private MatchInfo? info;
		private uint offset = 0;

		public uint64 start_address {
			get {
				return uint64.parse (info.fetch (1), 16);
			}
		}

		public uint64 end_address {
			get {
				return uint64.parse (info.fetch (2), 16);
			}
		}

		public string flags {
			owned get {
				return info.fetch (3);
			}
		}

		public uint64 file_offset {
			get {
				return uint64.parse (info.fetch (4), 16);
			}
		}

		public uint64 dev {
			get {
				var s = info.fetch (5);
				int colon = s.index_of_char (':');

				uint64 major = uint64.parse (s.substring (0, colon), 16);
				uint64 minor = uint64.parse (s.substring (colon + 1), 16);

				return (major << 32) | minor;
			}
		}

		public DevId device {
			get {
				var s = info.fetch (5);
				int colon = s.index_of_char (':');

				uint32 major = (uint32) uint64.parse (s.substring (0, colon), 16);
				uint32 minor = (uint32) uint64.parse (s.substring (colon + 1), 16);

				return DevId (major, minor);
			}
		}

		public uint64 inode {
			get {
				return uint64.parse (info.fetch (6));
			}
		}

		public string path {
			owned get {
				return info.fetch (7);
			}
		}

		public static ProcMapsIter for_pid (uint pid) {
			return new ProcMapsIter (pid);
		}

		private ProcMapsIter (uint pid) {
			try {
				FileUtils.get_contents ("/proc/%u/maps".printf (pid), out contents);
			} catch (FileError e) {
				return;
			}

			if (!/^([0-9a-f]+)-([0-9a-f]+) (\S{4}) ([0-9a-f]+) ([0-9a-f]{2,}:[0-9a-f]{2,}) (\d+) *([^\n]*)$/m.match (
					contents, 0, out info)) {
				assert_not_reached ();
			}
		}

		public bool next () {
			if (info == null)
				return false;

			if (offset > 0) {
				try {
					info.next ();
				} catch (RegexError e) {
					return false;
				}
			}
			offset++;

			return info.matches ();
		}
	}
}
