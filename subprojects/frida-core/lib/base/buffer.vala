namespace Frida {
	public sealed class BufferBuilder : Object {
		public ByteOrder byte_order {
			get;
			construct;
		}

		public uint pointer_size {
			get;
			construct;
		}

		public size_t offset {
			get {
				return cursor;
			}
		}

		private ByteArray buffer = new ByteArray ();
		private size_t cursor = 0;

		private uint64 base_address = 0;
		private Gee.List<LabelRef>? label_refs;
		private Gee.Map<string, uint>? label_defs;

		public BufferBuilder (ByteOrder byte_order = HOST, uint pointer_size = (uint) sizeof (size_t)) {
			Object (
				byte_order: byte_order,
				pointer_size: pointer_size
			);
		}

		public unowned BufferBuilder seek (size_t offset) {
			if (buffer.len < offset) {
				size_t n = offset - buffer.len;
				Memory.set (get_pointer (offset - n, n), 0, n);
			}
			cursor = offset;
			return this;
		}

		public unowned BufferBuilder skip (size_t n) {
			seek (cursor + n);
			return this;
		}

		public unowned BufferBuilder align (size_t n) {
			size_t remainder = cursor % n;
			if (remainder != 0)
				skip (n - remainder);
			return this;
		}

		public unowned BufferBuilder append_pointer (uint64 val) {
			write_pointer (cursor, val);
			cursor += pointer_size;
			return this;
		}

		public unowned BufferBuilder append_pointer_to_label (string name) {
			if (label_refs == null)
				label_refs = new Gee.ArrayList<LabelRef> ();
			label_refs.add (new LabelRef (name, cursor));
			return skip (pointer_size);
		}

		public unowned BufferBuilder append_pointer_to_label_if (bool present, string name) {
			if (present)
				append_pointer_to_label (name);
			else
				append_pointer (0);
			return this;
		}

		public unowned BufferBuilder append_label (string name) throws Error {
			if (label_defs == null)
				label_defs = new Gee.HashMap<string, uint> ();
			if (label_defs.has_key (name))
				throw new Error.INVALID_ARGUMENT ("Label '%s' already exists", name);
			label_defs[name] = (uint) cursor;
			return this;
		}

		public unowned BufferBuilder append_size (uint64 val) {
			return append_pointer (val);
		}

		public unowned BufferBuilder append_int8 (int8 val) {
			write_int8 (cursor, val);
			cursor += (uint) sizeof (int8);
			return this;
		}

		public unowned BufferBuilder append_uint8 (uint8 val) {
			write_uint8 (cursor, val);
			cursor += (uint) sizeof (uint8);
			return this;
		}

		public unowned BufferBuilder append_int16 (int16 val) {
			write_int16 (cursor, val);
			cursor += (uint) sizeof (int16);
			return this;
		}

		public unowned BufferBuilder append_uint16 (uint16 val) {
			write_uint16 (cursor, val);
			cursor += (uint) sizeof (uint16);
			return this;
		}

		public unowned BufferBuilder append_int32 (int32 val) {
			write_int32 (cursor, val);
			cursor += (uint) sizeof (int32);
			return this;
		}

		public unowned BufferBuilder append_uint32 (uint32 val) {
			write_uint32 (cursor, val);
			cursor += (uint) sizeof (uint32);
			return this;
		}

		public unowned BufferBuilder append_int64 (int64 val) {
			write_int64 (cursor, val);
			cursor += (uint) sizeof (int64);
			return this;
		}

		public unowned BufferBuilder append_uint64 (uint64 val) {
			write_uint64 (cursor, val);
			cursor += (uint) sizeof (uint64);
			return this;
		}

		public unowned BufferBuilder append_float (float val) {
			write_float (cursor, val);
			cursor += (uint) sizeof (float);
			return this;
		}

		public unowned BufferBuilder append_double (double val) {
			write_double (cursor, val);
			cursor += (uint) sizeof (double);
			return this;
		}

		public unowned BufferBuilder append_string (string val, StringTerminator terminator = NUL) {
			uint size = val.length;
			if (terminator == NUL)
				size++;
			Memory.copy (get_pointer (cursor, size), val, size);
			cursor += size;
			return this;
		}

		public unowned BufferBuilder append_bytes (Bytes bytes) {
			return append_data (bytes.get_data ());
		}

		public unowned BufferBuilder append_data (uint8[] data) {
			write_data (cursor, data);
			cursor += data.length;
			return this;
		}

		public unowned BufferBuilder write_pointer (size_t offset, uint64 val) {
			if (pointer_size == 4)
				write_uint32 (offset, (uint32) val);
			else
				write_uint64 (offset, val);
			return this;
		}

		public unowned BufferBuilder write_size (size_t offset, uint64 val) {
			return write_pointer (offset, val);
		}

		public unowned BufferBuilder write_int8 (size_t offset, int8 val) {
			*((int8 *) get_pointer (offset, sizeof (int8))) = val;
			return this;
		}

		public unowned BufferBuilder write_uint8 (size_t offset, uint8 val) {
			*get_pointer (offset, sizeof (uint8)) = val;
			return this;
		}

		public unowned BufferBuilder write_int16 (size_t offset, int16 val) {
			int16 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((int16 *) get_pointer (offset, sizeof (int16))) = target_val;
			return this;
		}

		public unowned BufferBuilder write_uint16 (size_t offset, uint16 val) {
			uint16 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((uint16 *) get_pointer (offset, sizeof (uint16))) = target_val;
			return this;
		}

		public unowned BufferBuilder write_int32 (size_t offset, int32 val) {
			int32 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((int32 *) get_pointer (offset, sizeof (int32))) = target_val;
			return this;
		}

		public unowned BufferBuilder write_uint32 (size_t offset, uint32 val) {
			uint32 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((uint32 *) get_pointer (offset, sizeof (uint32))) = target_val;
			return this;
		}

		public unowned BufferBuilder write_int64 (size_t offset, int64 val) {
			int64 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((int64 *) get_pointer (offset, sizeof (int64))) = target_val;
			return this;
		}

		public unowned BufferBuilder write_uint64 (size_t offset, uint64 val) {
			uint64 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((uint64 *) get_pointer (offset, sizeof (uint64))) = target_val;
			return this;
		}

		public unowned BufferBuilder write_float (size_t offset, float val) {
			return write_uint32 (offset, *((uint32 *) &val));
		}

		public unowned BufferBuilder write_double (size_t offset, double val) {
			return write_uint64 (offset, *((uint64 *) &val));
		}

		public unowned BufferBuilder write_string (size_t offset, string val) {
			uint size = val.length + 1;
			Memory.copy (get_pointer (offset, size), val, size);
			return this;
		}

		public unowned BufferBuilder write_bytes (size_t offset, Bytes bytes) {
			return write_data (offset, bytes.get_data ());
		}

		public unowned BufferBuilder write_data (size_t offset, uint8[] data) {
			Memory.copy (get_pointer (offset, data.length), data, data.length);
			return this;
		}

		private uint8 * get_pointer (size_t offset, size_t n) {
			size_t minimum_size = offset + n;
			if (buffer.len < minimum_size)
				buffer.set_size ((uint) minimum_size);

			return (uint8 *) buffer.data + offset;
		}

		public Bytes try_build (uint64 base_address = 0) throws Error {
			this.base_address = base_address;

			if (label_refs != null) {
				foreach (LabelRef r in label_refs)
					write_pointer (r.offset, address_of (r.name));
			}

			return ByteArray.free_to_bytes ((owned) buffer);
		}

		public Bytes build (uint64 base_address = 0) {
			try {
				return try_build (base_address);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		public uint64 address_of (string label) throws Error {
			if (label_defs == null || !label_defs.has_key (label))
				throw new Error.INVALID_OPERATION ("Label '%s' not defined", label);
			size_t offset = label_defs[label];
			return base_address + offset;
		}

		private class LabelRef {
			public string name;
			public size_t offset;

			public LabelRef (string name, size_t offset) {
				this.name = name;
				this.offset = offset;
			}
		}
	}

	public enum StringTerminator {
		NONE,
		NUL
	}

	public sealed class Buffer : Object {
		public Bytes bytes {
			get {
				if (_bytes == null)
					_bytes = new Bytes.static (_data);
				return _bytes;
			}
		}

		public size_t size {
			get {
				return _data.length;
			}
		}

		public ByteOrder byte_order {
			get;
			construct;
		}

		public uint pointer_size {
			get;
			construct;
		}

		private Bytes? _bytes;
		private unowned uint8[] _data;

		public Buffer (Bytes bytes, ByteOrder byte_order = HOST, uint pointer_size = (uint) sizeof (size_t)) {
			Object (
				byte_order: byte_order,
				pointer_size: pointer_size
			);

			reset_bytes (bytes);
		}

		public Buffer.from_data (uint8[] data, ByteOrder byte_order = HOST, uint pointer_size = (uint) sizeof (size_t)) {
			Object (
				byte_order: byte_order,
				pointer_size: pointer_size
			);

			reset_data (data);
		}

		public void reset_bytes (Bytes bytes) {
			_bytes = bytes;
			_data = bytes.get_data ();
		}

		public void reset_data (uint8[] data) {
			_bytes = null;
			_data = data;
		}

		public unowned uint8[] peek_data () {
			return _data;
		}

		public uint64 read_pointer (size_t offset) {
			return (pointer_size == 4)
				? read_uint32 (offset)
				: read_uint64 (offset);
		}

		public void write_pointer (size_t offset, uint64 val) {
			if (pointer_size == 4)
				write_uint32 (offset, (uint32) val);
			else
				write_uint64 (offset, val);
		}

		public int8 read_int8 (size_t offset) {
			return *((int8 *) get_pointer (offset, sizeof (int8)));
		}

		public unowned Buffer write_int8 (size_t offset, int8 val) {
			*((int8 *) get_pointer (offset, sizeof (int8))) = val;
			return this;
		}

		public uint8 read_uint8 (size_t offset) {
			return *get_pointer (offset, sizeof (uint8));
		}

		public unowned Buffer write_uint8 (size_t offset, uint8 val) {
			*((uint8 *) get_pointer (offset, sizeof (uint8))) = val;
			return this;
		}

		public int16 read_int16 (size_t offset) {
			int16 val = *((int16 *) get_pointer (offset, sizeof (int16)));
			return (byte_order == BIG_ENDIAN)
				? int16.from_big_endian (val)
				: int16.from_little_endian (val);
		}

		public unowned Buffer write_int16 (size_t offset, int16 val) {
			int16 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((int16 *) get_pointer (offset, sizeof (int16))) = target_val;
			return this;
		}

		public uint16 read_uint16 (size_t offset) {
			uint16 val = *((uint16 *) get_pointer (offset, sizeof (uint16)));
			return (byte_order == BIG_ENDIAN)
				? uint16.from_big_endian (val)
				: uint16.from_little_endian (val);
		}

		public unowned Buffer write_uint16 (size_t offset, uint16 val) {
			uint16 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((uint16 *) get_pointer (offset, sizeof (uint16))) = target_val;
			return this;
		}

		public int32 read_int32 (size_t offset) {
			int32 val = *((int32 *) get_pointer (offset, sizeof (int32)));
			return (byte_order == BIG_ENDIAN)
				? int32.from_big_endian (val)
				: int32.from_little_endian (val);
		}

		public unowned Buffer write_int32 (size_t offset, int32 val) {
			int32 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((int32 *) get_pointer (offset, sizeof (int32))) = target_val;
			return this;
		}

		public uint32 read_uint32 (size_t offset) {
			uint32 val = *((uint32 *) get_pointer (offset, sizeof (uint32)));
			return (byte_order == BIG_ENDIAN)
				? uint32.from_big_endian (val)
				: uint32.from_little_endian (val);
		}

		public unowned Buffer write_uint32 (size_t offset, uint32 val) {
			uint32 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((uint32 *) get_pointer (offset, sizeof (uint32))) = target_val;
			return this;
		}

		public int64 read_int64 (size_t offset) {
			int64 val = *((int64 *) get_pointer (offset, sizeof (int64)));
			return (byte_order == BIG_ENDIAN)
				? int64.from_big_endian (val)
				: int64.from_little_endian (val);
		}

		public unowned Buffer write_int64 (size_t offset, int64 val) {
			int64 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((int64 *) get_pointer (offset, sizeof (int64))) = target_val;
			return this;
		}

		public uint64 read_uint64 (size_t offset) {
			uint64 val = *((uint64 *) get_pointer (offset, sizeof (uint64)));
			return (byte_order == BIG_ENDIAN)
				? uint64.from_big_endian (val)
				: uint64.from_little_endian (val);
		}

		public unowned Buffer write_uint64 (size_t offset, uint64 val) {
			uint64 target_val = (byte_order == BIG_ENDIAN)
				? val.to_big_endian ()
				: val.to_little_endian ();
			*((uint64 *) get_pointer (offset, sizeof (uint64))) = target_val;
			return this;
		}

		public float read_float (size_t offset) {
			uint32 bits = read_uint32 (offset);
			return *((float *) &bits);
		}

		public double read_double (size_t offset) {
			uint64 bits = read_uint64 (offset);
			return *((double *) &bits);
		}

		public unowned string read_string (size_t offset) throws Error {
			string * start = (string *) get_pointer (offset, sizeof (char));
			size_t max_length = _data.length - offset;
			string * end = memchr (start, 0, max_length);
			if (end == null)
				throw new Error.PROTOCOL ("Missing null character");
			return start;
		}

		[CCode (cname = "memchr", cheader_filename = "string.h")]
		private extern static string * memchr (string * s, int c, size_t n);

		public string read_fixed_string (size_t offset, size_t size) throws Error {
			string * start = (string *) get_pointer (offset, size);
			size_t max_length = size_t.min (size, _data.length - offset);
			string * end = memchr (start, 0, max_length);
			size_t n;
			if (end != null)
				n = end - start;
			else
				n = size;
			return start->substring (0, (long) n);
		}

		public unowned Buffer write_string (size_t offset, string val) {
			uint size = val.length + 1;
			Memory.copy (get_pointer (offset, size), val, size);
			return this;
		}

		public Bytes read_bytes (size_t offset, size_t size) {
			if (_bytes == null)
				return make_bytes_with_owner ((uint8 *) _data + offset, _data.length - size, this);
			return _bytes[offset:offset + size];
		}

		public unowned uint8[] read_data (size_t offset, size_t size) {
			unowned uint8 * ptr = get_pointer (offset, size);
			return ((uint8[]) ptr)[:size];
		}

		public unowned Buffer write_bytes (size_t offset, Bytes bytes) {
			size_t size = bytes.get_size ();
			Memory.copy (get_pointer (offset, size), bytes.get_data (), size);
			return this;
		}

		private uint8 * get_pointer (size_t offset, size_t n) {
			size_t minimum_size = offset + n;
			assert (_data.length >= minimum_size);
			return (uint8 *) _data + offset;
		}
	}

	public sealed class BufferReader {
		public Buffer buffer {
			get;
		}

		public size_t offset {
			get;
		}

		public size_t available {
			get {
				return _size - _offset;
			}
		}

		private size_t _size;

		public BufferReader (Buffer buffer) {
			reset (buffer);
		}

		public void reset (Buffer buffer) {
			_buffer = buffer;
			_size = buffer.size;
			_offset = 0;
		}

		public void reset_at (Buffer buffer, size_t offset) throws Error {
			_buffer = buffer;
			_size = buffer.size;
			seek (offset);
		}

		public unowned BufferReader seek (size_t offset) throws Error {
			if (offset > _size)
				throw new Error.PROTOCOL ("Malformed buffer: truncated");
			_offset = offset;
			return this;
		}

		public unowned BufferReader skip (size_t n) throws Error {
			check_available (n);
			_offset += n;
			return this;
		}

		public unowned BufferReader align (size_t alignment) throws Error {
			size_t mask = alignment - 1;
			size_t rem = _offset & mask;
			if (rem != 0)
				skip (alignment - rem);
			return this;
		}

		public uint64 peek_pointer () throws Error {
			var pointer_size = _buffer.pointer_size;
			check_available (pointer_size);
			return _buffer.read_pointer (_offset);
		}

		public uint64 read_pointer () throws Error {
			var pointer_size = _buffer.pointer_size;
			check_available (pointer_size);
			var ptr = _buffer.read_pointer (_offset);
			_offset += pointer_size;
			return ptr;
		}

		public int8 peek_int8 () throws Error {
			check_available (sizeof (int8));
			return _buffer.read_int8 (_offset);
		}

		public int8 read_int8 () throws Error {
			check_available (sizeof (int8));
			var val = _buffer.read_int8 (_offset);
			_offset += sizeof (int8);
			return val;
		}

		public uint8 peek_uint8 () throws Error {
			check_available (sizeof (uint8));
			return _buffer.read_uint8 (_offset);
		}

		public uint8 read_uint8 () throws Error {
			check_available (sizeof (uint8));
			var val = _buffer.read_uint8 (_offset);
			_offset += sizeof (uint8);
			return val;
		}

		public int16 peek_int16 () throws Error {
			check_available (sizeof (int16));
			return _buffer.read_int16 (_offset);
		}

		public int16 read_int16 () throws Error {
			check_available (sizeof (int16));
			var val = _buffer.read_int16 (_offset);
			_offset += sizeof (int16);
			return val;
		}

		public uint16 peek_uint16 () throws Error {
			check_available (sizeof (uint16));
			return _buffer.read_uint16 (_offset);
		}

		public uint16 read_uint16 () throws Error {
			check_available (sizeof (uint16));
			var val = _buffer.read_uint16 (_offset);
			_offset += sizeof (uint16);
			return val;
		}

		public int32 peek_int32 () throws Error {
			check_available (sizeof (int32));
			return _buffer.read_int32 (_offset);
		}

		public int32 read_int32 () throws Error {
			check_available (sizeof (int32));
			var val = _buffer.read_int32 (_offset);
			_offset += sizeof (int32);
			return val;
		}

		public uint32 peek_uint32 () throws Error {
			check_available (sizeof (uint32));
			return _buffer.read_uint32 (_offset);
		}

		public uint32 read_uint32 () throws Error {
			check_available (sizeof (uint32));
			var val = _buffer.read_uint32 (_offset);
			_offset += sizeof (uint32);
			return val;
		}

		public int64 peek_int64 () throws Error {
			check_available (sizeof (int64));
			return _buffer.read_int64 (_offset);
		}

		public int64 read_int64 () throws Error {
			check_available (sizeof (int64));
			var val = _buffer.read_int64 (_offset);
			_offset += sizeof (int64);
			return val;
		}

		public uint64 peek_uint64 () throws Error {
			check_available (sizeof (uint64));
			return _buffer.read_uint64 (_offset);
		}

		public uint64 read_uint64 () throws Error {
			check_available (sizeof (uint64));
			var val = _buffer.read_uint64 (_offset);
			_offset += sizeof (uint64);
			return val;
		}

		public float peek_float () throws Error {
			check_available (sizeof (float));
			return _buffer.read_float (_offset);
		}

		public float read_float () throws Error {
			check_available (sizeof (float));
			var val = _buffer.read_float (_offset);
			_offset += sizeof (float);
			return val;
		}

		public double peek_double () throws Error {
			check_available (sizeof (double));
			return _buffer.read_double (_offset);
		}

		public double read_double () throws Error {
			check_available (sizeof (double));
			var val = _buffer.read_double (_offset);
			_offset += sizeof (double);
			return val;
		}

		public unowned string peek_string () throws Error {
			check_available (1);
			return _buffer.read_string (_offset);
		}

		public unowned string read_string () throws Error {
			check_available (1);
			unowned string val = _buffer.read_string (_offset);
			_offset += val.length + 1;
			return val;
		}

		public string peek_fixed_string (size_t size) throws Error {
			check_available (size);
			return _buffer.read_fixed_string (_offset, size);
		}

		public string read_fixed_string (size_t size) throws Error {
			check_available (size);
			var val = _buffer.read_fixed_string (_offset, size);
			_offset += size;
			return val;
		}

		public unowned uint8[] peek_data (size_t size) throws Error {
			check_available (size);
			return _buffer.read_data (_offset, size);
		}

		public unowned uint8[] read_data (size_t size) throws Error {
			check_available (size);
			unowned uint8[] slice = _buffer.read_data (_offset, size);
			_offset += size;
			return slice;
		}

		public Bytes read_bytes (size_t size) throws Error {
			check_available (size);
			var val = _buffer.read_bytes (_offset, size);
			_offset += size;
			return val;
		}

		public unowned uint8[] read_remaining_data () throws Error {
			return read_data (available);
		}

		private void check_available (size_t n) throws Error {
			if (available < n)
				throw new Error.PROTOCOL ("Malformed buffer: truncated");
		}
	}

	public extern Bytes make_bytes_with_owner<T> (void * data, size_t size, owned T? owner = null);
}
