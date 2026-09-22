[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	/**
	 * Reconstructs a kernel's symbols from the kallsyms tables embedded in its on-disk image,
	 * so a System.map is not needed. The image is a raw arm64 Image (optionally gzip-compressed);
	 * the tables are located by their structure and the names are decoded with the token table,
	 * pairing each name with relative_base + offsets[i] -- the address it was linked for.
	 */
	internal class KallsymsImage {
		public static Gee.List<SymbolInfo> parse (uint8[] image) throws Error {
			uint8[] raw = maybe_gunzip (image);

			var tokens = find_tokens (raw);

			uint num_syms;
			uint table_pos;
			uint64 relative_base;
			bool absolute = find_addresses (raw, out table_pos, out num_syms, out relative_base);

			uint names_pos = find_names (raw, tokens, num_syms);

			var symbols = new Gee.ArrayList<SymbolInfo> ();
			uint p = names_pos;
			for (uint i = 0; i != num_syms; i++) {
				string name;
				p = decode_symbol (raw, p, tokens, out name);
				if (name.length < 2)
					continue;
				uint64 address = absolute
					? read_u64 (raw, table_pos + i * 8)
					: relative_base + (int64) read_i32 (raw, table_pos + i * 4);
				symbols.add (new SymbolInfo () {
					name = name.substring (1),
					offset = address,
					symbol_type = 0xf,
					section = 0x10,
				});
			}
			return symbols;
		}

		private static uint8[] maybe_gunzip (uint8[] image) throws Error {
			if (image.length < 4 || image[0] != 0x1f || image[1] != 0x8b)
				return image;
			try {
				var decompressor = new ZlibDecompressor (ZlibCompressorFormat.GZIP);
				var source = new MemoryInputStream.from_data (image, null);
				var stream = new ConverterInputStream (source, decompressor);
				var output = new MemoryOutputStream.resizable ();
				output.splice (stream, OutputStreamSpliceFlags.CLOSE_TARGET);
				size_t size = output.get_data_size ();
				uint8[] buffer = output.steal_data ();
				buffer.length = (int) size;
				return buffer;
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("Unable to decompress kernel image: %s", e.message);
			}
		}

		/**
		 * kallsyms_token_index is 256 little-endian uint16 cumulative offsets, and the token
		 * table is the 256 NUL-terminated tokens right before it. The index is distinctive:
		 * it starts at 0 and rises by each token's length. A matching base is where every
		 * token's terminator lands, and the tokens then read as mostly-printable BPE fragments.
		 */
		private static string[] find_tokens (uint8[] raw) throws Error {
			uint n = raw.length;
			for (uint i = 0; i + 512 <= n; i++) {
				if (raw[i] != 0 || raw[i + 1] != 0)
					continue;
				var index = new uint16[256];
				uint prev = 0;
				bool monotonic = true;
				for (uint k = 1; k != 256; k++) {
					uint v = raw[i + 2 * k] | (raw[i + 2 * k + 1] << 8);
					uint delta = v - prev;
					if (delta < 1 || delta > 255) {
						monotonic = false;
						break;
					}
					index[k] = (uint16) v;
					prev = v;
				}
				if (!monotonic)
					continue;

				string[]? tokens = reconstruct_tokens (raw, i, index);
				if (tokens != null)
					return tokens;
			}
			throw new Error.NOT_SUPPORTED ("Unable to locate kallsyms token table in kernel image");
		}

		private static string[]? reconstruct_tokens (uint8[] raw, uint index_pos, uint16[] index) {
			uint idx_last = index[255];
			if (index_pos < idx_last + 2)
				return null;
			uint search_lo = (index_pos > idx_last + 4096) ? index_pos - idx_last - 4096 : 0;
			for (uint table_base = index_pos - idx_last - 2; table_base >= search_lo; table_base--) {
				bool aligned = true;
				for (uint k = 1; k != 256; k++) {
					if (raw[table_base + index[k] - 1] != 0) {
						aligned = false;
						break;
					}
				}
				if (aligned) {
					var tokens = new string[256];
					uint printable = 0;
					uint total = 0;
					uint non_empty = 0;
					for (uint k = 0; k != 256; k++) {
						uint start = table_base + index[k];
						uint end = start;
						while (end < index_pos && raw[end] != 0)
							end++;
						tokens[k] = slice_to_string (raw, start, end);
						if (end > start)
							non_empty++;
						for (uint b = start; b != end; b++) {
							total++;
							if (raw[b] >= 0x20 && raw[b] < 0x7f)
								printable++;
						}
					}
					// A real token table is 256 non-trivial byte-pair fragments; a run of
					// mostly-empty tokens is a lookalike index, not the table.
					if (non_empty >= 200 && printable >= (total * 90) / 100)
						return tokens;
				}
				if (table_base == 0)
					break;
			}
			return null;
		}

		/**
		 * The address of each symbol is stored one of two ways. A modern kernel keeps a signed
		 * 32-bit offset per symbol (kallsyms_offsets) and a base to add them to
		 * (kallsyms_relative_base); an older one keeps the absolute 64-bit address per symbol
		 * (kallsyms_addresses). Both are sorted by address, so each is the longest ascending run
		 * of its word size. The relative form is tried first and confirmed by the kernel pointer
		 * that follows it; failing that, the absolute form is located. Returns whether the table
		 * is absolute.
		 */
		private static bool find_addresses (uint8[] raw, out uint table_pos, out uint num_syms,
				out uint64 relative_base) throws Error {
			relative_base = 0;

			uint offsets_words;
			uint offsets_pos = longest_ascending_run (raw, 4, 0, 0x8000000, out offsets_words);
			if (offsets_words >= 1024) {
				uint64 candidate = read_u64 (raw, offsets_pos + offsets_words * 4);
				if (looks_like_kernel_base (candidate)) {
					table_pos = offsets_pos;
					num_syms = offsets_words;
					relative_base = candidate;
					return false;
				}
			}

			uint address_words;
			uint address_pos = longest_ascending_run (raw, 8, KERNEL_VA_MIN, uint64.MAX, out address_words);
			if (address_words >= 1024) {
				table_pos = address_pos;
				num_syms = address_words;
				return true;
			}

			throw new Error.NOT_SUPPORTED ("Unable to locate kallsyms address table in kernel image");
		}

		/**
		 * The byte offset of the longest run of non-decreasing little-endian words (4 or 8 bytes)
		 * whose values lie in [low, high), and its length in words.
		 */
		private static uint longest_ascending_run (uint8[] raw, uint word_size, uint64 low, uint64 high,
				out uint length) {
			uint words = raw.length / word_size;
			uint best_pos = 0;
			uint best_len = 0;
			uint i = 0;
			while (i < words) {
				uint64 v = read_word (raw, i * word_size, word_size);
				if (v < low || v >= high) {
					i++;
					continue;
				}
				uint j = i + 1;
				uint64 prev = v;
				while (j < words) {
					uint64 w = read_word (raw, j * word_size, word_size);
					if (w < low || w >= high || w < prev)
						break;
					prev = w;
					j++;
				}
				if (j - i > best_len) {
					best_len = j - i;
					best_pos = i;
				}
				i = j;
			}
			length = best_len;
			return best_pos * word_size;
		}

		private static uint64 read_word (uint8[] raw, uint pos, uint word_size) {
			return (word_size == 8) ? read_u64 (raw, pos) : (uint64) (uint32) read_i32 (raw, pos);
		}

		private static bool looks_like_kernel_base (uint64 candidate) {
			return (candidate >> 40) == 0xffffff && (candidate & 0xfff) == 0;
		}

		private const uint64 KERNEL_VA_MIN = 0xffffff8000000000;

		/**
		 * kallsyms_names is the run of symbols the offsets index into, each one a length byte
		 * (extended when the high bit is set) and that many token indices, decoding to a type
		 * letter and the name. Its start is found by seeding on a stretch of plausible names,
		 * proving positions reach that seed by walking symbol by symbol, and taking the earliest
		 * such start whose own leading names are short, printable and distinct -- i.e. _text.
		 */
		private static uint find_names (uint8[] raw, string[] tokens, uint num_syms) throws Error {
			uint seed = find_names_seed (raw, tokens);

			uint lo = (seed > 3000000) ? seed - 3000000 : 0;
			var reaches = new bool[seed - lo + 1];
			reaches[seed - lo] = true;
			uint pos = seed;
			while (pos > lo) {
				pos--;
				uint next = pos + symbol_size (raw, pos);
				if (next == seed || (next < seed && reaches[next - lo]))
					reaches[pos - lo] = true;
			}

			// The true start decodes exactly num_syms valid symbols, matching the address table,
			// and the byte after the last is no longer a symbol. A start that is too early keeps
			// decoding past that count; one too late runs out first. This anchors the names to the
			// addresses even when an earlier region also decodes cleanly.
			for (uint start = lo; start <= seed; start++) {
				if (reaches[start - lo] && decodes_full_table (raw, start, tokens, num_syms))
					return start;
			}
			throw new Error.NOT_SUPPORTED ("Unable to locate kallsyms names in kernel image");
		}

		private static bool decodes_full_table (uint8[] raw, uint start, string[] tokens, uint num_syms) {
			uint p = start;
			for (uint i = 0; i != num_syms; i++) {
				string name;
				uint next = try_decode_symbol (raw, p, tokens, out name);
				if (next == 0)
					return false;
				p = next;
			}
			string tail;
			return try_decode_symbol (raw, p, tokens, out tail) == 0;
		}

		/**
		 * A short stretch of names is not enough to seed on: other token-like data (exported
		 * symbol name tables) can decode as a handful of plausible names too. A run this long
		 * only holds together inside the real names table, so it cannot land anywhere else.
		 */
		private static uint find_names_seed (uint8[] raw, string[] tokens) throws Error {
			uint n = raw.length;
			for (uint c = 0; c + 64 < n; c++) {
				string first;
				if (try_decode_symbol (raw, c, tokens, out first) == 0)
					continue;
				if (distinct_clean_names (raw, c, tokens, 200))
					return c;
			}
			throw new Error.NOT_SUPPORTED ("Unable to seed kallsyms names in kernel image");
		}

		private static bool distinct_clean_names (uint8[] raw, uint pos, string[] tokens, uint count) {
			var seen = new Gee.HashSet<string> ();
			uint p = pos;
			for (uint i = 0; i != count; i++) {
				string name;
				uint next = try_decode_symbol (raw, p, tokens, out name);
				if (next == 0 || name.length < 3 || name.length > 80)
					return false;
				seen.add (name);
				p = next;
			}
			return seen.size >= (count * 9) / 10;
		}

		private static uint symbol_size (uint8[] raw, uint pos) {
			uint len = raw[pos];
			uint adv = 1;
			if ((len & 0x80) != 0) {
				len = (len & 0x7f) | (raw[pos + 1] << 7);
				adv = 2;
			}
			return adv + len;
		}

		private static uint decode_symbol (uint8[] raw, uint pos, string[] tokens, out string name) {
			uint len = raw[pos];
			uint p = pos + 1;
			if ((len & 0x80) != 0) {
				len = (len & 0x7f) | (raw[p] << 7);
				p++;
			}
			var builder = new StringBuilder ();
			for (uint i = 0; i != len; i++)
				builder.append (tokens[raw[p + i]]);
			name = builder.str;
			return p + len;
		}

		/**
		 * Decodes a symbol only if it is a plausible name: a type letter followed by printable,
		 * non-space characters. Returns 0 when it is not, so a scan can reject a position without
		 * trusting whatever bytes it landed on.
		 */
		private static uint try_decode_symbol (uint8[] raw, uint pos, string[] tokens, out string name) {
			name = "";
			if (pos >= raw.length)
				return 0;
			uint len = raw[pos];
			uint p = pos + 1;
			if ((len & 0x80) != 0) {
				if (p >= raw.length)
					return 0;
				len = (len & 0x7f) | (raw[p] << 7);
				p++;
			}
			if (len == 0 || len > 300 || p + len > raw.length)
				return 0;
			var builder = new StringBuilder ();
			for (uint i = 0; i != len; i++) {
				unowned string token = tokens[raw[p + i]];
				char* cursor = (char*) token;
				for (char* c = cursor; *c != '\0'; c++) {
					uint8 byte = (uint8) (*c);
					if (byte < 0x21 || byte > 0x7e)
						return 0;
				}
				builder.append (token);
			}
			string decoded = builder.str;
			if (decoded.length < 1 || !is_symbol_type (decoded[0]))
				return 0;
			name = decoded;
			return p + len;
		}

		private static bool is_symbol_type (char c) {
			return c.isalpha () || c == '?' || c == '-';
		}

		private static string slice_to_string (uint8[] raw, uint start, uint end) {
			var builder = new StringBuilder ();
			for (uint i = start; i != end; i++)
				builder.append_c ((char) raw[i]);
			return builder.str;
		}

		private static int32 read_i32 (uint8[] raw, uint pos) {
			return (int32) ((uint32) raw[pos] | ((uint32) raw[pos + 1] << 8)
				| ((uint32) raw[pos + 2] << 16) | ((uint32) raw[pos + 3] << 24));
		}

		private static uint64 read_u64 (uint8[] raw, uint pos) {
			uint64 v = 0;
			for (uint i = 0; i != 8; i++)
				v |= ((uint64) raw[pos + i]) << (int) (8 * i);
			return v;
		}
	}
}
