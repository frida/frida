[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone.Img4 {
	public async Payload parse_file (File f, Cancellable? cancellable) throws Error, IOError {
		Bytes blob = yield FS.read_all_bytes (f, cancellable);
		return parse (blob);
	}

	public Payload parse (Bytes blob) throws Error {
		BufferReader r = new BufferReader (new Buffer (blob, BIG_ENDIAN));

		if (r.read_uint8 () != Der.SEQUENCE)
			throw new Error.PROTOCOL ("IMG4/IM4P must start with DER SEQUENCE");
		size_t seq_len = Der.read_length (r);
		size_t seq_end = r.offset + seq_len;

		string first_magic = Der.read_string (r);

		if (first_magic == "IMG4") {
			if (r.read_uint8 () != Der.SEQUENCE)
				throw new Error.PROTOCOL ("Expected SEQUENCE for IM4P");
			size_t im4p_len = Der.read_length (r);
			seq_end = r.offset + im4p_len;
			if (Der.read_string (r) != "IM4P")
				throw new Error.PROTOCOL ("Missing IM4P magic");
		} else if (first_magic != "IM4P") {
			throw new Error.PROTOCOL ("Not an IMG4/IM4P container (magic='%s')", first_magic);
		}

		string kind = Der.read_string (r);
		string description = Der.read_string (r);
		Bytes payload_raw = Der.read_octet_string (r);
		Bytes final_data = maybe_decompress_lzfse (payload_raw);

		return new Payload (kind, description, final_data);
	}

	public class Payload : Object {
		public string kind {
			get;
			construct;
		}

		public string description {
			get;
			construct;
		}

		public Bytes data {
			get;
			construct;
		}

		public Payload (string kind, string description, Bytes data) {
			Object (
				kind: kind,
				description: description,
				data: data
			);
		}
	}

	namespace Der {
		private const uint8 OCTET_STRING	= 0x04;
		private const uint8 UTF8_STRING		= 0x0C;
		private const uint8 IA5_STRING		= 0x16;
		private const uint8 SEQUENCE		= 0x30;

		private size_t read_length (BufferReader r) throws Error {
			uint8 b = r.read_uint8 ();
			if ((b & 0x80) == 0)
				return b;

			uint8 nlen = (uint8) (b & 0x7f);
			if (nlen == 0)
				throw new Error.NOT_SUPPORTED ("Indefinite length not supported in IMG4");

			size_t len = 0;
			for (uint8 i = 0; i != nlen; i++)
				len = (len << 8) | r.read_uint8 ();
			return len;
		}

		private Bytes read_octet_string (BufferReader r) throws Error {
			uint8 tag = r.read_uint8 ();
			if (tag != Der.OCTET_STRING)
				throw new Error.PROTOCOL ("Expected OCTET STRING, got tag 0x%02x".printf (tag));
			size_t len = read_length (r);
			return r.read_bytes (len);
		}

		private string read_string (BufferReader r) throws Error {
			uint8 tag = r.read_uint8 ();
			if (tag != Der.IA5_STRING && tag != Der.UTF8_STRING)
				throw new Error.PROTOCOL ("Unexpected string tag 0x%02x", tag);
			size_t len = read_length (r);
			return r.read_fixed_string (len);
		}
	}

	private Bytes maybe_decompress_lzfse (Bytes src) throws Error {
		if (LZFSE.is_lzfse (src))
			return LZFSE.decode (src);
		return src;
	}
}
