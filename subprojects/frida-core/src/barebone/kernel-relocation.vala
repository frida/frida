[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	/**
	 * Translates static kernelcache addresses to their runtime locations.
	 *
	 * The on-device kernel collection slides each fileset entry independently (the SPTM loader
	 * scatters com.apple.kernel and every kext to unrelated runtime addresses), so a single KASLR
	 * slide cannot map a static address to its runtime location. Each entry's in-memory Mach-O
	 * header carries the rebased per-segment vmaddrs; we pair them with the static segment table
	 * (same header, same order) to translate any static address, kernel or kext.
	 */
	public sealed class KernelRelocation : Object {
		public Gee.List<Segment> segments {
			get;
			default = new Gee.ArrayList<Segment> ();
		}

		public uint64 reference_base {
			get;
			private set;
		}

		private Gee.List<Entry> entries = new Gee.ArrayList<Entry> ();

		private const uint64 COLLECTION_WINDOW = 0x8000000;
		private const uint64 CHUNK_SIZE = 1024 * 1024;
		private const uint64 SUMMARY_HEADER_SIZE = 0x10;
		private const uint64 SUMMARY_SIZE = 0x88;
		private const size_t SUMMARY_LOAD_ADDRESS = 0x50;
		private const uint32 MH_MAGIC_64 = 0xfeedfacfU;
		private const uint32 MH_FILESET = 0xc;
		private const uint32 CPU_TYPE_ARM64 = 0x0100000cU;
		private const uint32 LC_SEGMENT_64 = 0x19;
		private const uint32 LC_FILESET_ENTRY = 0x80000035U;

		public static async KernelRelocation compute (Machine machine, Bytes kernelcache_blob,
				uint64 loaded_kext_summaries, Cancellable? cancellable) throws Error, IOError {
			var gdb = machine.gdb;
			Buffer image = gdb.make_buffer (kernelcache_blob);
			Gee.List<FilesetEntry> static_entries = parse_fileset_entries (image);

			Gee.List<SegmentInfo> static_segments = parse_segments (image, 0);
			uint64 preferred_base = static_segments[0].vmaddr;

			uint64 collection_header = yield find_collection_header (machine, cancellable);
			Buffer collection = yield read_mach_header (gdb, collection_header, cancellable);
			var runtime_segments = parse_segments (collection, 0);
			var header_locator = new KernelRelocation ();
			pair_segments (header_locator, static_segments, runtime_segments);

			var reloc = new KernelRelocation ();
			FilesetEntry kernel = find_entry (static_entries, "com.apple.kernel");
			yield add_fileset_entry (reloc, gdb, image, kernel, header_locator.translate (kernel.vmaddr), cancellable);
			reloc.reference_base = reloc.translate (kernel.vmaddr);

			if (loaded_kext_summaries != 0) {
				var runtime_headers = yield read_loaded_kext_headers (gdb, reloc,
					preferred_base + loaded_kext_summaries, cancellable);
				foreach (var entry in static_entries) {
					uint64? runtime_header = runtime_headers[entry.name];
					if (runtime_header != null)
						yield add_fileset_entry (reloc, gdb, image, entry, runtime_header, cancellable);
				}
			}

			pair_segments (reloc, static_segments, runtime_segments);

			return reloc;
		}

		private static void pair_segments (KernelRelocation reloc, Gee.List<SegmentInfo> static_segments,
				Gee.List<SegmentInfo> runtime_segments) {
			for (int i = 0; i != static_segments.size; i++) {
				reloc.entries.add (new Entry () {
					static_base = static_segments[i].vmaddr,
					size = static_segments[i].vmsize,
					runtime_base = runtime_segments[i].vmaddr
				});
			}
		}

		private static FilesetEntry find_entry (Gee.List<FilesetEntry> entries, string name) throws Error {
			foreach (var entry in entries) {
				if (entry.name == name)
					return entry;
			}
			throw new Error.NOT_SUPPORTED ("Kernelcache is missing the %s fileset entry", name);
		}

		private static async void add_fileset_entry (KernelRelocation reloc, GDB.Client gdb, Buffer image,
				FilesetEntry entry, uint64 runtime_header, Cancellable? cancellable) throws Error, IOError {
			Buffer runtime_image = yield read_mach_header (gdb, runtime_header, cancellable);
			var static_segments = parse_segments (image, (size_t) entry.fileoff);
			var runtime_segments = parse_segments (runtime_image, 0);
			pair_segments (reloc, static_segments, runtime_segments);

			for (int i = 0; i != int.min (static_segments.size, runtime_segments.size); i++) {
				reloc.segments.add (new Segment () {
					address = runtime_segments[i].vmaddr,
					size = static_segments[i].vmsize,
					protection = static_segments[i].protection
				});
			}
		}

		/**
		 * The runtime kext metadata embedded in the collection header is unusable: the loader clobbers the
		 * fileset-entry vmaddrs and repacks the kext mach-headers away from their static positions. The
		 * gLoadedKextSummaries array, on the other hand, is purpose-built for debuggers — it pairs each
		 * loaded kext's bundle id with the runtime address of its mach-header, giving us a reliable
		 * name -> runtime-header map from which to translate that kext's independently scattered segments.
		 */
		private static async Gee.Map<string, uint64?> read_loaded_kext_headers (GDB.Client gdb,
				KernelRelocation reloc, uint64 loaded_kext_summaries, Cancellable? cancellable) throws Error, IOError {
			uint64 list = (yield gdb.read_buffer (reloc.translate (loaded_kext_summaries), 8, cancellable))
				.read_uint64 (0);
			uint32 count = (yield gdb.read_buffer (list, 16, cancellable)).read_uint32 (8);

			var result = new Gee.HashMap<string, uint64?> ();
			for (uint32 i = 0; i != count; i++) {
				Buffer summary = yield gdb.read_buffer (list + SUMMARY_HEADER_SIZE + (uint64) i * SUMMARY_SIZE,
					(size_t) SUMMARY_SIZE, cancellable);
				result[read_lc_string (summary, 0)] = summary.read_uint64 (SUMMARY_LOAD_ADDRESS);
			}
			return result;
		}

		public uint64 translate (uint64 static_address) throws Error {
			foreach (var e in entries) {
				if (static_address >= e.static_base && static_address < e.static_base + e.size)
					return e.runtime_base + (static_address - e.static_base);
			}
			throw new Error.INVALID_ARGUMENT ("Address 0x%" + uint64.FORMAT_MODIFIER + "x is outside the kernelcache",
				static_address);
		}

		public uint32 runtime_offset (uint64 static_address) throws Error {
			return (uint32) (translate (static_address) - reference_base);
		}

		private static async Buffer read_mach_header (GDB.Client gdb, uint64 address, Cancellable? cancellable)
				throws Error, IOError {
			Buffer head = yield gdb.read_buffer (address, 32, cancellable);
			return yield gdb.read_buffer (address, 32 + head.read_uint32 (20), cancellable);
		}

		/**
		 * Reads of unmapped memory wedge the VZ stub indefinitely, so the collection header cannot be
		 * located by blind-scanning down from a kernel pointer: scattered segments leave unmapped gaps
		 * in between. We instead walk the kernel's own page tables for the readable ranges and probe
		 * only the first 16 bytes of each — every byte touched is guaranteed mapped.
		 */
		private static async uint64 find_collection_header (Machine machine, Cancellable? cancellable)
				throws Error, IOError {
			var gdb = machine.gdb;

			uint64 vbar = yield gdb.exception.thread.read_register ("vbar_el1", cancellable);

			var range_bases = new Gee.ArrayList<uint64?> ();
			var range_sizes = new Gee.ArrayList<uint64?> ();
			yield machine.enumerate_ranges (Gum.PageProtection.READ, details => {
				if (details.base_va < vbar - COLLECTION_WINDOW || details.base_va > vbar + COLLECTION_WINDOW)
					return true;
				range_bases.add (details.base_va);
				range_sizes.add (details.size);
				return true;
			}, cancellable);

			size_t page_size = yield machine.query_page_size (cancellable);

			for (int i = 0; i != range_bases.size; i++) {
				uint64 found = yield find_header_in (machine, range_bases[i], range_sizes[i], page_size,
					cancellable);
				if (found != 0)
					return found;
			}

			throw new Error.NOT_SUPPORTED ("Unable to locate the runtime kernel collection header");
		}

		private static async uint64 find_header_in (Machine machine, uint64 base_va, uint64 size,
				size_t page_size, Cancellable? cancellable) throws Error, IOError {
			var gdb = machine.gdb;

			for (uint64 offset = 0; offset + 16 <= size; offset += CHUNK_SIZE) {
				size_t chunk = (size_t) uint64.min (CHUNK_SIZE, size - offset);

				Bytes taken;
				try {
					taken = yield machine.read_virtual (base_va + offset, chunk, cancellable);
				} catch (Error e) {
					continue;
				}

				Buffer pages = gdb.make_buffer (taken);
				for (size_t at = 0; at + 16 <= chunk; at += page_size) {
					if (pages.read_uint32 (at) == MH_MAGIC_64 && pages.read_uint32 (at + 4) == CPU_TYPE_ARM64
							&& pages.read_uint32 (at + 12) == MH_FILESET)
						return base_va + offset + at;
				}
			}

			return 0;
		}

		private static Gee.List<FilesetEntry> parse_fileset_entries (Buffer image) throws Error {
			var result = new Gee.ArrayList<FilesetEntry> ();
			uint32 ncmds = image.read_uint32 (16);
			size_t off = 32;
			for (uint32 i = 0; i != ncmds; i++) {
				uint32 cmd = image.read_uint32 (off);
				uint32 cmdsize = image.read_uint32 (off + 4);
				if (cmd == LC_FILESET_ENTRY) {
					result.add (new FilesetEntry () {
						name = read_lc_string (image, off + image.read_uint32 (off + 24)),
						vmaddr = image.read_uint64 (off + 8),
						fileoff = image.read_uint64 (off + 16)
					});
				}
				off += cmdsize;
			}
			return result;
		}

		private static Gee.List<SegmentInfo> parse_segments (Buffer image, size_t header_offset) throws Error {
			var result = new Gee.ArrayList<SegmentInfo> ();
			uint32 ncmds = image.read_uint32 (header_offset + 16);
			size_t off = header_offset + 32;
			for (uint32 i = 0; i != ncmds; i++) {
				uint32 cmd = image.read_uint32 (off);
				uint32 cmdsize = image.read_uint32 (off + 4);
				if (cmd == LC_SEGMENT_64) {
					result.add (new SegmentInfo () {
						vmaddr = image.read_uint64 (off + 24),
						vmsize = image.read_uint64 (off + 32),
						protection = image.read_uint32 (off + 60)
					});
				}
				off += cmdsize;
			}
			return result;
		}

		private static string read_lc_string (Buffer image, size_t offset) throws Error {
			var builder = new StringBuilder ();
			for (size_t i = 0; ; i++) {
				uint8 c = image.read_uint8 (offset + i);
				if (c == 0)
					break;
				builder.append_c ((char) c);
			}
			return builder.str;
		}

		private class FilesetEntry {
			public string name;
			public uint64 vmaddr;
			public uint64 fileoff;
		}

		private class SegmentInfo {
			public uint64 vmaddr;
			public uint64 vmsize;
			public uint32 protection;
		}

		public class Segment : Object {
			public uint64 address;
			public uint64 size;
			public uint32 protection;
		}

		private class Entry {
			public uint64 static_base;
			public uint64 size;
			public uint64 runtime_base;
		}
	}
}
