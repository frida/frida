[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class OpackBuilder {
		protected BufferBuilder builder = new BufferBuilder (LITTLE_ENDIAN);
		private Gee.Deque<Scope> scopes = new Gee.ArrayQueue<Scope> ();

		public OpackBuilder () {
			push_scope (new Scope (ROOT));
		}

		public unowned OpackBuilder begin_dictionary () {
			begin_value ();

			size_t type_offset = builder.offset;
			builder.append_uint8 (0x00);

			push_scope (new CollectionScope (type_offset));

			return this;
		}

		public unowned OpackBuilder set_member_name (string name) {
			return add_string_value (name);
		}

		public unowned OpackBuilder end_dictionary () {
			CollectionScope scope = pop_scope ();

			size_t n = scope.num_values / 2;
			if (n < 0xf) {
				builder.write_uint8 (scope.type_offset, 0xe0 | n);
			} else {
				builder
					.write_uint8 (scope.type_offset, 0xef)
					.append_uint8 (0x03);
			}

			return this;
		}

		public unowned OpackBuilder add_string_value (string val) {
			begin_value ();

			size_t len = val.length;

			if (len > uint32.MAX) {
				builder
					.append_uint8 (0x6f)
					.append_string (val, StringTerminator.NUL);

				return this;
			}

			if (len <= 0x20)
				builder.append_uint8 ((uint8) (0x40 + len));
			else if (len <= uint8.MAX)
				builder.append_uint8 (0x61).append_uint8 ((uint8) len);
			else if (len <= uint16.MAX)
				builder.append_uint8 (0x62).append_uint16 ((uint16) len);
			else if (len <= 0xffffff)
				builder.append_uint8 (0x63).append_uint8 ((uint8) (len & 0xff)).append_uint16 ((uint16) (len >> 8));
			else
				builder.append_uint8 (0x64).append_uint32 ((uint32) len);

			builder.append_string (val, StringTerminator.NONE);

			return this;
		}

		public unowned OpackBuilder add_data_value (Bytes val) {
			begin_value ();

			size_t size = val.get_size ();
			if (size <= 0x20)
				builder.append_uint8 ((uint8) (0x70 + size));
			else if (size <= uint8.MAX)
				builder.append_uint8 (0x91).append_uint8 ((uint8) size);
			else if (size <= uint16.MAX)
				builder.append_uint8 (0x92).append_uint16 ((uint16) size);
			else if (size <= 0xffffff)
				builder.append_uint8 (0x93).append_uint8 ((uint8) (size & 0xff)).append_uint16 ((uint16) (size >> 8));
			else
				builder.append_uint8 (0x94).append_uint32 ((uint32) size);

			builder.append_bytes (val);

			return this;
		}

		private unowned OpackBuilder begin_value () {
			peek_scope ().num_values++;
			return this;
		}

		public Bytes build () {
			return builder.build ();
		}

		private void push_scope (Scope scope) {
			scopes.offer_tail (scope);
		}

		private Scope peek_scope () {
			return scopes.peek_tail ();
		}

		private T pop_scope<T> () {
			return (T) scopes.poll_tail ();
		}

		private class Scope {
			public Kind kind;
			public size_t num_values = 0;

			public enum Kind {
				ROOT,
				COLLECTION,
			}

			public Scope (Kind kind) {
				this.kind = kind;
			}
		}

		private class CollectionScope : Scope {
			public size_t type_offset;

			public CollectionScope (size_t type_offset) {
				base (COLLECTION);
				this.type_offset = type_offset;
			}
		}
	}

	public sealed class OpackParser {
		private BufferReader reader;

		[Flags]
		private enum ValueFlags {
			ALLOW_TERMINATOR = 1 << 0,
		}

		public static Variant parse (Bytes opack) throws Error {
			var parser = new OpackParser (opack);
			return parser.read_value ();
		}

		private OpackParser (Bytes opack) {
			reader = new BufferReader (new Buffer (opack, LITTLE_ENDIAN));
		}

		private Variant? read_value (ValueFlags flags = 0) throws Error {
			uint8 v = reader.read_uint8 ();
			uint8 top = v >> 4;
			uint8 bottom = v & 0b1111;
			switch (top) {
				case 0:
					switch (bottom) {
						case 1:
							return true;
						case 2:
							return false;
						case 3:
							if ((flags & ValueFlags.ALLOW_TERMINATOR) == 0)
								throw new Error.PROTOCOL ("Unexpected OPACK terminator");
							return null;
					}
					if (bottom < 8)
						throw new Error.NOT_SUPPORTED ("Unsupported OPACK type 0x%02x", v);
					return (int64) (v - 8);
				case 1:
				case 2:
					return (int64) (v - 8);
				case 3:
					switch (bottom) {
						case 0: return (int64) reader.read_int8 ();
						case 1: return (int64) reader.read_int16 ();
						case 2: return (int64) reader.read_int32 ();
						case 3: return (int64) reader.read_int64 ();
					}
					throw new Error.NOT_SUPPORTED ("Unsupported OPACK type 0x%02x", v);
				case 4:
				case 5:
				case 6:
					return read_string (v - 0x40);
				case 7:
				case 8:
				case 9:
					return read_data (v - 0x70);
				case 0xe:
					return read_dictionary (bottom);
				default:
					throw new Error.NOT_SUPPORTED ("Unsupported OPACK type 0x%02x", v);
			}
		}

		private Variant read_string (size_t len) throws Error {
			if (len == 0x2f)
				return reader.read_string ();
			len = read_variable_length (len);
			return reader.read_fixed_string (len);
		}

		private Variant read_data (size_t len) throws Error {
			len = read_variable_length (len);
			var bytes = reader.read_bytes (len);
			return Variant.new_from_data (new VariantType.array (VariantType.BYTE), bytes.get_data (), true, bytes);
		}

		private size_t read_variable_length (size_t len) throws Error {
			if (len <= 0x20)
				return len;

			switch (len) {
				case 0x21:
					return reader.read_uint8 ();
				case 0x22:
					return reader.read_uint16 ();
				case 0x23:
					uint32 bottom = reader.read_uint8 ();
					uint32 top = reader.read_uint16 ();
					return top << 8 | bottom;
				case 0x24:
					return reader.read_uint32 ();
				default:
					throw new Error.NOT_SUPPORTED ("Unsupported OPACK length: 0x%zx", len);
			}
		}

		private Variant read_dictionary (size_t n) throws Error {
			var builder = new VariantBuilder (VariantType.VARDICT);

			bool has_terminator = n == 0xf;

			size_t i = 0;
			while (true) {
				if (!has_terminator && i == n)
					break;

				var key = read_value (has_terminator ? ValueFlags.ALLOW_TERMINATOR : 0);
				if (key == null)
					break;
				if (!key.is_of_type (VariantType.STRING))
					throw new Error.PROTOCOL ("Unsupported OPACK dictionary key type");
				var val = read_value ();
				builder.add_value (new Variant.dict_entry (key, new Variant.variant (val)));

				i++;
			}

			return builder.end ();
		}
	}
}
