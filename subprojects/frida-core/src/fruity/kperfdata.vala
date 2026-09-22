[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public delegate void KperfdataRecordVisitor (KdBuf rec) throws Error;

	public sealed class KperfdataStreamParser : Object {
		private ByteArray stash = new ByteArray ();
		private size_t cursor = 0;

		private bool have_header = false;
		private uint32 thread_count = 0;
		private size_t records_offset = 0;

		private const uint32 RAW_VERSION2 = 0x55aa0200u;

		private const size_t HEADER_FIXED_SIZE = 0x120;
		private const size_t THREADMAP_ENTRY_SIZE = 0x20;
		private const size_t PAGE_SIZE = 0x1000;

		private const size_t KD_BUF_SIZE = 0x40;

		public void reset () {
			stash.set_size (0);
			cursor = 0;

			have_header = false;
			thread_count = 0;
			records_offset = 0;
		}

		public void push (Bytes chunk, KperfdataRecordVisitor visitor) throws Error {
			stash.append (chunk.get_data ());

			var stash_bytes = new Bytes.static (stash.data);
			parse_bytes (stash_bytes, visitor);

			compact ();
		}

		private void parse_bytes (Bytes bytes, KperfdataRecordVisitor visitor) throws Error {
			var buf = new Frida.Buffer (bytes, LITTLE_ENDIAN);
			var r = new Frida.BufferReader (buf);

			while (true) {
				if (!have_header) {
					if (r.available < 8)
						return;

					size_t header_start = r.offset;

					uint32 magic = r.read_uint32 ();
					if (magic != RAW_VERSION2)
						throw new Error.PROTOCOL ("Unknown kperfdata magic 0x%x", magic);

					thread_count = r.read_uint32 ();

					uint64 threadmaps_size_64 = thread_count * THREADMAP_ENTRY_SIZE;
					size_t header_size = (size_t) (HEADER_FIXED_SIZE + threadmaps_size_64);
					size_t aligned = align_up (header_size, PAGE_SIZE);

					records_offset = aligned;

					if ((r.offset - 8) != header_start)
						throw new Error.PROTOCOL ("Internal offset bookkeeping error");

					if ((stash.len - header_start) < records_offset) {
						r.skip (header_start - r.offset);
						return;
					}

					cursor = header_start + records_offset;
					have_header = true;

					r.skip (cursor - r.offset);
					continue;
				}

				if (r.offset < cursor) {
					if ((cursor - r.offset) > r.available)
						return;

					r.skip (cursor - r.offset);
				}

				if (r.available < KD_BUF_SIZE)
					return;

				KdBuf rec = KdBuf ();

				rec.timestamp = r.read_uint64 ();

				rec.arg1 = r.read_uint64 ();
				rec.arg2 = r.read_uint64 ();
				rec.arg3 = r.read_uint64 ();
				rec.arg4 = r.read_uint64 ();
				rec.arg5 = r.read_uint64 ();

				rec.debugid = r.read_uint32 ();
				rec.cpuid = r.read_uint32 ();

				rec.unused = r.read_uint64 ();

				visitor (rec);

				cursor = r.offset;
			}
		}

		private void compact () {
			if (cursor == 0)
				return;

			if (cursor >= stash.len) {
				stash.set_size (0);
				cursor = 0;
				return;
			}

			size_t remaining = stash.len - cursor;
			Memory.move (stash.data, (uint8 *) stash.data + cursor, remaining);
			stash.set_size ((uint) remaining);
			cursor = 0;
		}

		private static size_t align_up (size_t val, size_t alignment) {
			size_t mask = alignment - 1;
			return (val + mask) & ~mask;
		}
	}

	public struct KdBuf {
		public uint64 timestamp;

		public uint64 arg1;
		public uint64 arg2;
		public uint64 arg3;
		public uint64 arg4;
		public uint64 arg5;

		public uint32 debugid;
		public uint32 cpuid;
		public uint64 unused;

		public KdebugCode kcode {
			get {
				return KdebugCode (debugid);
			}
		}

		public string to_string () {
			var kc = kcode;

			return (
				"ts=0x%" + uint64.FORMAT_MODIFIER + "x "
				+ "arg1=0x%" + uint64.FORMAT_MODIFIER + "x "
				+ "arg2=0x%" + uint64.FORMAT_MODIFIER + "x "
				+ "arg3=0x%" + uint64.FORMAT_MODIFIER + "x "
				+ "arg4=0x%" + uint64.FORMAT_MODIFIER + "x "
				+ "arg5=0x%" + uint64.FORMAT_MODIFIER + "x "
				+ "debugid=0x%08x "
				+ "klass=%s subclass=%u code=%u func=%s "
				+ "cpuid=%u "
				+ "unused=0x%" + uint64.FORMAT_MODIFIER + "x"
			).printf (
				timestamp,
				arg1,
				arg2,
				arg3,
				arg4,
				arg5,
				debugid,
				kc.klass.to_nick (),
				kc.subclass,
				kc.code,
				kc.func_qual.to_nick (),
				cpuid,
				unused
			);
		}
	}
}
