[CCode (gir_namespace = "FridaAXML", gir_version = "1.0")]
namespace Frida.AXML {
	public static ElementTree read (InputStream stream) throws Error {
		try {
			var input = new DataInputStream (stream);
			input.byte_order = LITTLE_ENDIAN;

			var type = input.read_uint16 ();
			var header_size = input.read_uint16 ();
			var binary_size = input.read_uint32 ();

			if (type != ChunkType.XML)
				throw new Error.INVALID_ARGUMENT ("Not Android Binary XML");

			StringPool? pool = null;
			ResourceMap? resource_map = null;

			var namespaces = new Queue<Namespace> ();
			var root = new ElementTree ();
			var tree = new Queue<ElementTree> ();
			tree.push_head (root);

			while (input.tell () < binary_size) {
				var offset = input.tell ();

				type = input.read_uint16 ();
				header_size = input.read_uint16 ();
				var size = input.read_uint32 ();

				switch (type) {
					case ChunkType.STRING_POOL:
						pool = new StringPool.with_stream (input);
						break;
					case ChunkType.RESOURCE_MAP:
						resource_map = new ResourceMap.with_stream (input, size);
						break;
					case ChunkType.START_NAMESPACE:
						namespaces.push_head (new Namespace.with_stream (input));
						break;
					case ChunkType.START_ELEMENT: {
						var e = new ElementTree ();
						var start_element = new StartElement.with_stream (input, pool);
						e.name = pool.get_string (start_element.name);
						foreach (var attribute in start_element.attributes)
							e.set_attribute (attribute.get_name (), attribute);
						tree.peek_head ().add_child (e);
						tree.push_head (e);
						break;
					}
					case ChunkType.END_ELEMENT:
						tree.pop_head ();
						break;
					case ChunkType.END_NAMESPACE:
						if (namespaces.pop_head () == null)
							throw new Error.INVALID_ARGUMENT ("Mismatched namespaces");
						break;
					default:
						throw new Error.NOT_SUPPORTED ("Type not recognized: %#x", type);
				}

				input.seek (offset + size, SeekType.SET);
			}

			return root.get_child (0);
		} catch (GLib.Error e) {
			if (e is Error)
				throw (Error) e;
			throw new Error.INVALID_ARGUMENT ("%s", e.message);
		}
	}

	public sealed class ElementTree : Object {
		public string name {
			get;
			set;
		}

		private Gee.HashMap<string, Attribute> attributes = new Gee.HashMap<string, Attribute> ();

		private Gee.ArrayList<ElementTree> children = new Gee.ArrayList<ElementTree> ();

		public Attribute? get_attribute (string name) {
			return attributes[name];
		}

		public void set_attribute (string name, Attribute? value) {
			attributes[name] = value;
		}

		public void add_child (ElementTree child) {
			children.add (child);
		}

		public ElementTree? get_child (int i) {
			if (i >= children.size)
				return null;
			return children[i];
		}

		public string to_string (int depth = 0) {
			var b = new StringBuilder ();

			for (int i = 0; i != depth; i++)
				b.append ("\t");
			b.append_printf ("<%s", name);
			foreach (var attribute in attributes) {
				b.append_printf (" %s=\"%s\"", attribute.key, attribute.value.get_value ().to_string ());
			}
			b.append (">\n");

			foreach (var child in children)
				b.append (child.to_string (depth + 1));

			for (int i = 0; i != depth; i++)
				b.append ("\t");
			b.append_printf ("</%s>", name);
			if (depth != 0)
				b.append ("\n");

			return b.str;
		}
	}

	private class EndElement : Object {
		public uint32 line;
		public uint32 comment;
		public uint32 namespace;
		public uint32 name;

		public EndElement.with_stream (DataInputStream input) throws IOError {
			line = input.read_uint32 ();
			comment = input.read_uint32 ();
			namespace = input.read_uint32 ();
			name = input.read_uint32 ();
		}
	}

	public sealed class ResourceValue : Object {
		private uint16 size;
		private uint8 unused;
		private ResourceType type;
		private uint32 d;
		private float f;
		private StringPool pool;

		internal ResourceValue.with_stream (DataInputStream input, StringPool string_pool) throws IOError {
			size = input.read_uint16 ();
			unused = input.read_byte ();
			type = (ResourceType) input.read_byte ();
			d = input.read_uint32 ();
			f = *(float *) &d;
			pool = string_pool;
		}

		public string to_string () {
			switch (type) {
				case REFERENCE:
					return "@0x%x".printf (d);
				case STRING:
					return pool.get_string (d);
				case FLOAT:
					return "%f".printf (f);
				case INT_DEC:
					return "%ud".printf (d);
				case INT_HEX:
					return "0x%x".printf (d);
				case BOOL:
					return (d != 0) ? "true" : "false";
				case NULL:
				default:
					return "NULL";
			}
		}
	}

	public sealed class Attribute : Object {
		private uint32 namespace;
		private uint32 name;
		private uint32 unused;
		private ResourceValue value;
		private StringPool pool;

		internal Attribute.with_stream (DataInputStream input, StringPool string_pool) throws IOError {
			namespace = input.read_uint32 ();
			name = input.read_uint32 ();
			unused = input.read_uint32 ();
			value = new ResourceValue.with_stream (input, string_pool);
			pool = string_pool;
		}

		public string? get_name () {
			return pool.get_string (name);
		}

		public ResourceValue get_value () {
			return value;
		}
	}

	private class StartElement {
		public uint32 line;
		public uint32 comment;
		public uint32 namespace;
		public uint32 name;
		public uint32 flags;
		public uint16 unused0;
		public uint16 unused1;
		public uint16 unused2;
		public Gee.ArrayList<Attribute> attributes = new Gee.ArrayList<Attribute> ();

		public StartElement.with_stream (DataInputStream input, StringPool pool) throws IOError {
			line = input.read_uint32 ();
			comment = input.read_uint32 ();
			namespace = input.read_uint32 ();
			name = input.read_uint32 ();
			flags = input.read_uint32 ();
			var attribute_count = input.read_uint16 ();
			unused0 = input.read_uint16 ();
			unused1 = input.read_uint16 ();
			unused2 = input.read_uint16 ();

			for (uint16 i = 0; i != attribute_count; i++)
				attributes.add (new Attribute.with_stream (input, pool));
		}
	}

	private class Namespace {
		public uint32 line;
		public uint32 comment;
		public uint32 prefix;
		public uint32 uri;

		public Namespace.with_stream (DataInputStream input) throws IOError {
			line = input.read_uint32 ();
			comment = input.read_uint32 ();
			prefix = input.read_uint32 ();
			uri = input.read_uint32 ();
		}
	}

	private sealed class ResourceMap {
		private Gee.ArrayList<uint32> resources = new Gee.ArrayList<uint32> ();

		public ResourceMap.with_stream (DataInputStream input, uint32 size) throws IOError {
			for (uint32 i = 0; i < size / 4; i++)
				resources.add (input.read_uint32 ());
		}
	}

	private sealed class StringPool {
		private uint32 flags;
		private Gee.ArrayList<string> strings = new Gee.ArrayList<string> ();

		public StringPool.with_stream (DataInputStream input) throws GLib.Error {
			var string_count = input.read_uint32 ();
			// Ignore the style_count
			input.read_uint32 ();
			flags = input.read_uint32 ();
			var strings_offset = input.read_uint32 ();
			// Ignore the styles_offset
			input.read_uint32 ();

			var offsets = new uint32[string_count];
			for (uint32 i = 0; i != string_count; i++)
				offsets[i] = input.read_uint32 ();

			var previous_position = input.tell ();

			for (uint32 i = 0; i != string_count; i++) {
				var offset = offsets[i];
				input.seek (strings_offset + 8 + offset, SeekType.SET);

				if ((flags & FLAG_UTF8) != 0) {
					// Ignore UTF-16LE encoded length
					uint32 n = input.read_byte ();
					if ((n & 0x80) != 0) {
						n = ((n & 0x7f) << 8) | input.read_byte ();
					}

					// Read UTF-8 encoded length
					n = input.read_byte ();
					if ((n & 0x80) != 0) {
						n = ((n & 0x7f) << 8) | input.read_byte ();
					}

					var string_data = new uint8[n];
					input.read (string_data);
					strings.add ((string) string_data);
				} else {
					// If >0x7fff, stored as a big-endian ut32
					uint32 n = input.read_uint16 ();
					if ((n & 0x8000) != 0) {
						n |= ((n & 0x7fff) << 16) | input.read_uint16 ();
					}

					// Size of UTF-16LE without NULL
					n *= 2;

					var string_data = new uint8[n];
					input.read (string_data);
					strings.add (convert ((string) string_data, n, "UTF-8", "UTF-16LE"));
				}
			}

			input.seek (previous_position, SeekType.SET);
		}

		public string? get_string (uint32 i) {
			if (i >= strings.size)
				return null;
			return strings[(int) i];
		}
	}

	private enum ChunkType {
		STRING_POOL	= 0x0001,
		XML		= 0x0003,
		START_NAMESPACE	= 0x0100,
		END_NAMESPACE	= 0x0101,
		START_ELEMENT	= 0x0102,
		END_ELEMENT	= 0x0103,
		RESOURCE_MAP	= 0x0180
	}

	private enum ResourceType {
		NULL		= 0x00,
		REFERENCE	= 0x01,
		STRING		= 0x03,
		FLOAT		= 0x04,
		INT_DEC		= 0x10,
		INT_HEX		= 0x11,
		BOOL		= 0x12
	}

	private const uint32 FLAG_UTF8 = 1 << 8;
}
