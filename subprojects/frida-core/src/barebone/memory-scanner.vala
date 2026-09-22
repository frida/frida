[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public class MatchPattern {
		public size_t size;
		public Gee.List<MatchToken> tokens = new Gee.ArrayList<MatchToken> ();

		public bool matches (uint8[] data, size_t offset) {
			size_t cursor = offset;
			foreach (MatchToken t in tokens) {
				if (!t.matches (data, cursor))
					return false;
				cursor += t.size;
			}

			return true;
		}

		public MatchPattern.from_string (string pattern) throws Error {
			string[] parts = pattern.replace (" ", "").split (":", 2);

			unowned string match_str = parts[0];
			uint len = match_str.length;
			if (len % 2 != 0)
				throw_invalid_pattern ();

			unowned string? mask_str = (parts.length == 2) ? parts[1] : null;
			bool has_mask = mask_str != null;
			if (has_mask && mask_str.length != match_str.length)
				throw_invalid_pattern ();

			MatchToken? token = null;
			for (uint i = 0; i != len; i += 2) {
				uint8 mask = has_mask
					? ((parse_xdigit_value (mask_str[i + 0]) << 4) | parse_xdigit_value (mask_str[i + 1]))
					: 0xff;

				uint8 upper;
				if (match_str[i + 0] == '?') {
					upper = 4;
					mask &= 0x0f;
				} else {
					upper = parse_xdigit_value (match_str[i + 0]);
				}

				uint8 lower;
				if (match_str[i + 1] == '?') {
					lower = 2;
					mask &= 0xf0;
				} else {
					lower = parse_xdigit_value (match_str[i + 1]);
				}

				uint8 val = (upper << 4) | lower;

				switch (mask) {
					case 0xff:
						if (token == null || token.kind != EXACT)
							token = push_token (EXACT);
						token.append (val);
						break;
					case 0x00:
						if (token == null || token.kind != WILDCARD)
							token = push_token (WILDCARD);
						token.append (val);
						break;
					default:
						if (token == null || token.kind != MASK)
							token = push_token (MASK);
						token.append_with_mask (val, mask);
						break;
				}
			}

			if (tokens.is_empty)
				throw_invalid_pattern ();
			if (!tokens.any_match (t => t.kind != WILDCARD))
				throw_invalid_pattern ();

			foreach (MatchToken t in tokens)
				size += t.size;
		}

		private MatchToken push_token (MatchToken.Kind kind) {
			var t = new MatchToken (kind);
			tokens.add (t);
			return t;
		}

		private static uint8 parse_xdigit_value (char ch) throws Error {
			int v = ch.xdigit_value ();
			if (v == -1)
				throw_invalid_pattern ();
			return (uint8) v;
		}

		[NoReturn]
		private static void throw_invalid_pattern () throws Error {
			throw new Error.INVALID_ARGUMENT ("Invalid pattern");
		}
	}

	public class MatchToken {
		public Kind kind;
		public ByteArray? values;
		public ByteArray? masks;
		public size_t size;

		public enum Kind {
			EXACT,
			WILDCARD,
			MASK
		}

		public MatchToken (Kind kind) {
			this.kind = kind;
		}

		public void append (uint8 val) {
			if (kind != WILDCARD) {
				if (values == null)
					values = new ByteArray ();
				values.append ({ val });
			}

			size++;
		}

		public void append_with_mask (uint8 val, uint8 mask) {
			append (val);

			if (masks == null)
				masks = new ByteArray ();
			masks.append ({ mask });
		}

		public bool matches (uint8[] data, size_t offset) {
			unowned uint8[]? values = (this.values != null) ? this.values.data : null;
			unowned uint8[]? masks = (this.masks != null) ? this.masks.data : null;

			for (size_t i = 0; i != size; i++) {
				switch (kind) {
					case EXACT:
						if (data[offset + i] != values[i])
							return false;
						break;
					case WILDCARD:
						break;
					case MASK:
						if (((data[offset + i] ^ values[i]) & masks[i]) != 0)
							return false;
						break;
				}
			}

			return true;
		}
	}

	// Machines with no scanner in the guest match here. This reads the memory instead of running
	// code in the guest.
	public void find_pattern_matches (MatchPattern pattern, uint8[] haystack, uint max_matches,
			FoundPatternFunc func) {
		if (haystack.length < pattern.size)
			return;

		uint found = 0;
		size_t last = haystack.length - pattern.size;
		for (size_t offset = 0; offset <= last; offset++) {
			if (!pattern.matches (haystack, offset))
				continue;

			func (offset);

			found++;
			if (found == max_matches)
				return;
		}
	}

	public delegate void FoundPatternFunc (size_t offset);

	public void append_memory_scanner_data (BufferBuilder builder, Gee.List<Gum.MemoryRange?> ranges, MatchPattern pattern,
			uint max_matches, out size_t data_size) {
		var start_offset = builder.offset;
		var pointer_size = builder.pointer_size;

		try {
			builder
				.append_label ("search-parameters")
				.append_pointer_to_label ("ranges")
				.append_size (ranges.size)
				.append_pointer_to_label ("tokens")
				.append_size (pattern.tokens.size);
			builder
				.append_label ("search-results")
				.append_pointer_to_label ("matches")
				.append_size (max_matches);
			builder.append_label ("ranges");
			foreach (Gum.MemoryRange r in ranges) {
				builder
					.append_pointer (r.base_address)
					.append_size (r.size);
			}
			builder.append_label ("tokens");
			uint i = 0;
			foreach (MatchToken t in pattern.tokens) {
				builder
					.append_size (t.kind)
					.append_pointer_to_label_if (t.values != null, "t%u.values".printf (i))
					.append_pointer_to_label_if (t.masks != null, "t%u.masks".printf (i))
					.append_size (t.size);
				i++;
			}
			i = 0;
			foreach (MatchToken t in pattern.tokens) {
				if (t.values != null) {
					builder
						.append_label ("t%u.values".printf (i))
						.append_data (t.values.data);
				}
				if (t.masks != null) {
					builder
						.append_label ("t%u.masks".printf (i))
						.append_data (t.masks.data);
				}
				i++;
			}
			builder
				.align (pointer_size)
				.append_label ("matches");
		} catch (Error e) {
			assert_not_reached ();
		}

		data_size = (builder.offset - start_offset) + (max_matches * pointer_size);
	}
}
