[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone.LZFSE {
	public bool is_lzfse (Bytes buf) {
		if (buf.get_size () < 4)
			return false;
		unowned uint8[] d = buf.get_data ();
		return d[0] == 'b' && d[1] == 'v' && d[2] == 'x';
	}

	public Bytes decode (Bytes compressed) throws Error {
		size_t est_max = compressed.get_size () * 15;
		uint8[] dst_buf = new uint8[est_max];

		size_t written = decode_buffer (dst_buf, compressed.get_data ());
		if (written == 0)
			throw new Error.PROTOCOL ("liblzfse returned 0 bytes");

		return new Bytes (dst_buf[:written]);
	}

	[CCode (cheader_filename = "lzfse.h", cname = "lzfse_decode_buffer")]
	private extern size_t decode_buffer ([CCode (array_length_type = "size_t")] uint8[] dst, [CCode (array_length_type = "size_t")] uint8[] src, void * scratch = null);
}
