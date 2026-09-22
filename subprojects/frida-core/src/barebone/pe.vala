[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public static async uint64 find_export (Machine machine, uint64 image, string wanted, Cancellable? cancellable)
			throws Error, IOError {
		foreach (Export e in yield enumerate_exports (machine, image, cancellable)) {
			if (e.name == wanted)
				return image + e.rva;
		}

		return 0;
	}

	// Read the full export directory once, because its tables and its strings are adjacent. Thus
	// a module with one thousand exports needs one transfer.
	public static async Gee.List<Export> enumerate_exports (Machine machine, uint64 image, Cancellable? cancellable)
			throws Error, IOError {
		var exports = new Gee.ArrayList<Export> ();

		GDB.Client gdb = machine.gdb;

		Buffer headers;
		try {
			headers = gdb.make_buffer (yield gdb.read_byte_array (image, HEADERS_SIZE, cancellable));
		} catch (Error e) {
			return exports;
		}
		if (headers.read_uint16 (0) != DOS_SIGNATURE)
			return exports;
		uint32 pe = headers.read_uint32 (DOS_HEADERS_OFFSET);
		if (pe > MAX_PE_OFFSET || headers.read_uint32 (pe) != PE_SIGNATURE)
			return exports;

		// The two forms of the optional header have different widths before the data directories.
		// Thus the directories start at different offsets.
		size_t directories;
		switch (headers.read_uint16 (pe + OPTIONAL_HEADER_OFFSET)) {
			case PE32_MAGIC:
				directories = pe + PE32_DIRECTORIES_OFFSET;
				break;
			case PE32_PLUS_MAGIC:
				directories = pe + PE32_PLUS_DIRECTORIES_OFFSET;
				break;
			default:
				return exports;
		}

		uint32 directory_rva = headers.read_uint32 (directories);
		uint32 directory_size = headers.read_uint32 (directories + 4);
		if (directory_rva == 0 || directory_size < DIRECTORY_SIZE || directory_size > MAX_DIRECTORY_SIZE)
			return exports;

		Buffer directory = gdb.make_buffer (
			yield read_present_pages (gdb, image + directory_rva, directory_size, cancellable));
		var window = new DirectoryWindow (directory, directory_rva, directory_size);

		uint32 count = directory.read_uint32 (NAME_COUNT_OFFSET);
		uint32 functions = directory.read_uint32 (FUNCTION_TABLE_OFFSET);
		uint32 names = directory.read_uint32 (NAME_TABLE_OFFSET);
		uint32 ordinals = directory.read_uint32 (ORDINAL_TABLE_OFFSET);

		for (uint32 i = 0; i != count; i++) {
			uint32? name_rva = window.read_uint32 (names + i * 4);
			uint32? ordinal = window.read_uint16 (ordinals + i * 2);
			if (name_rva == null || ordinal == null)
				break;

			uint32? function_rva = window.read_uint32 (functions + ordinal * 4);
			if (function_rva == null)
				break;
			if (window.contains (function_rva))
				continue;

			string? name = window.read_string (name_rva);
			if (name == null)
				break;

			exports.add (new Export () {
				name = name,
				rva = function_rva,
			});
		}

		return exports;
	}

	public static async uint32 read_image_size (Machine machine, uint64 image, Cancellable? cancellable)
			throws Error, IOError {
		GDB.Client gdb = machine.gdb;

		Buffer headers;
		try {
			headers = gdb.make_buffer (yield gdb.read_byte_array (image, HEADERS_SIZE, cancellable));
		} catch (Error e) {
			return 0;
		}
		if (headers.read_uint16 (0) != DOS_SIGNATURE)
			return 0;
		uint32 pe = headers.read_uint32 (DOS_HEADERS_OFFSET);
		if (pe > MAX_PE_OFFSET || headers.read_uint32 (pe) != PE_SIGNATURE)
			return 0;

		return headers.read_uint32 (pe + OPTIONAL_HEADER_OFFSET + IMAGE_SIZE_OFFSET);
	}

	// Part of an image can be out of memory. Read one page at a time and let the absent pages
	// read as zero, because no name or table entry has that value.
	private static async Bytes read_present_pages (GDB.Client gdb, uint64 address, size_t size,
			Cancellable? cancellable) throws Error, IOError {
		var result = new uint8[size];

		size_t offset = 0;
		uint present = 0;
		while (offset != size) {
			uint64 cursor = address + offset;
			size_t chunk = size_t.min (size - offset,
				PAGE_SIZE - (size_t) (cursor & (PAGE_SIZE - 1)));

			try {
				unowned uint8[] page = (yield gdb.read_byte_array (cursor, chunk, cancellable))
					.get_data ();
				Memory.copy ((uint8 *) result + offset, page, chunk);
				present++;
			} catch (Error e) {
			}

			offset += chunk;
		}

		if (present == 0)
			throw new Error.NOT_SUPPORTED (
				"Unable to read any page of 0x%" + uint64.FORMAT_MODIFIER + "x", address);

		return new Bytes.take ((owned) result);
	}

	public class Export {
		public string name;
		public uint32 rva;
	}

	// The directory contains all the data that it refers to. Thus an RVA outside the directory
	// shows an incorrect image. An export address is different: it is inside the directory only
	// if it forwards to a different module.
	private class DirectoryWindow {
		private Buffer buffer;
		private uint32 rva;
		private uint32 size;

		public DirectoryWindow (Buffer buffer, uint32 rva, uint32 size) {
			this.buffer = buffer;
			this.rva = rva;
			this.size = size;
		}

		public uint32? read_uint32 (uint32 rva) {
			if (!contains_span (rva, 4))
				return null;
			return buffer.read_uint32 (rva - this.rva);
		}

		public uint32? read_uint16 (uint32 rva) {
			if (!contains_span (rva, 2))
				return null;
			return buffer.read_uint16 (rva - this.rva);
		}

		public string? read_string (uint32 rva) {
			if (!contains (rva))
				return null;

			var text = new StringBuilder ();
			for (uint32 offset = rva - this.rva; offset != size; offset++) {
				char c = (char) buffer.read_uint8 (offset);
				if (c == '\0')
					return text.str;
				text.append_c (c);
			}

			return null;
		}

		public bool contains (uint32 rva) {
			return contains_span (rva, 1);
		}

		private bool contains_span (uint32 rva, uint32 span) {
			return rva >= this.rva && rva + span <= this.rva + size;
		}
	}

	private const size_t PAGE_SIZE = 0x1000;
	private const size_t HEADERS_SIZE = 0x200;
	private const uint16 DOS_SIGNATURE = 0x5a4d;
	private const size_t DOS_HEADERS_OFFSET = 0x3c;
	private const uint32 PE_SIGNATURE = 0x00004550;
	private const size_t OPTIONAL_HEADER_OFFSET = 0x18;
	private const size_t IMAGE_SIZE_OFFSET = 0x38;
	private const uint16 PE32_MAGIC = 0x010b;
	private const uint16 PE32_PLUS_MAGIC = 0x020b;
	private const size_t PE32_DIRECTORIES_OFFSET = 0x78;
	private const size_t PE32_PLUS_DIRECTORIES_OFFSET = 0x88;
	private const size_t MAX_PE_OFFSET = HEADERS_SIZE - PE32_PLUS_DIRECTORIES_OFFSET - 8;

	private const size_t DIRECTORY_SIZE = 0x28;
	private const uint32 MAX_DIRECTORY_SIZE = 1024 * 1024;
	private const size_t NAME_COUNT_OFFSET = 0x18;
	private const size_t FUNCTION_TABLE_OFFSET = 0x1c;
	private const size_t NAME_TABLE_OFFSET = 0x20;
	private const size_t ORDINAL_TABLE_OFFSET = 0x24;
}
