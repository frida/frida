namespace Frida {
	public interface ObjectBuilder : Object {
		public abstract unowned ObjectBuilder begin_dictionary ();
		public abstract unowned ObjectBuilder set_member_name (string name);
		public abstract unowned ObjectBuilder end_dictionary ();

		public abstract unowned ObjectBuilder begin_array ();
		public abstract unowned ObjectBuilder end_array ();

		public abstract unowned ObjectBuilder add_null_value ();
		public abstract unowned ObjectBuilder add_bool_value (bool val);
		public abstract unowned ObjectBuilder add_int64_value (int64 val);
		public abstract unowned ObjectBuilder add_uint64_value (uint64 val);
		public abstract unowned ObjectBuilder add_data_value (Bytes val);
		public abstract unowned ObjectBuilder add_string_value (string val);
		public abstract unowned ObjectBuilder add_uuid_value (uint8[] val);
		public abstract unowned ObjectBuilder add_raw_value (Bytes val);

		public abstract Bytes build ();
	}

	public interface ObjectReader : Object {
		public abstract bool has_member (string name) throws Error;
		public abstract unowned ObjectReader read_member (string name) throws Error;
		public abstract unowned ObjectReader end_member ();

		public abstract uint count_elements () throws Error;
		public abstract unowned ObjectReader read_element (uint index) throws Error;
		public abstract unowned ObjectReader end_element () throws Error;

		public abstract bool get_bool_value () throws Error;
		public abstract uint8 get_uint8_value () throws Error;
		public abstract uint16 get_uint16_value () throws Error;
		public abstract int64 get_int64_value () throws Error;
		public abstract uint64 get_uint64_value () throws Error;
		public abstract Bytes get_data_value () throws Error;
		public abstract unowned string get_string_value () throws Error;
		public abstract unowned string get_uuid_value () throws Error;
	}

	public sealed class VariantReader : Object, ObjectReader {
		public Variant root_object {
			get {
				return scopes.peek_head ().val;
			}
		}

		public Variant current_object {
			get {
				return scopes.peek_tail ().val;
			}
		}

		private Gee.Deque<Scope> scopes = new Gee.ArrayQueue<Scope> ();

		public VariantReader (Variant v) {
			push_scope (v);
		}

		public string[] list_members () throws Error {
			var scope = peek_scope ();
			if (scope.dict == null)
				throw new Error.PROTOCOL ("Dictionary expected, but at %s", scope.val.print (true));

			var members = new Gee.ArrayList<string> ();
			foreach (var item in scope.val) {
				string k;
				Variant v;
				item.get ("{sv}", out k, out v);

				members.add (k);
			}
			return members.to_array ();
		}

		public bool has_member (string name) throws Error {
			var scope = peek_scope ();
			if (scope.dict == null)
				throw new Error.PROTOCOL ("Dictionary expected, but at %s", scope.val.print (true));
			return scope.dict.contains (name);
		}

		public unowned ObjectReader read_member (string name) throws Error {
			var scope = peek_scope ();
			if (scope.dict == null)
				throw new Error.PROTOCOL ("Dictionary expected, but at %s", scope.val.print (true));

			Variant? v = scope.dict.lookup_value (name, null);
			if (v == null)
				throw new Error.PROTOCOL ("Key '%s' not found in dictionary: %s", name, scope.val.print (true));

			push_scope (v);

			return this;
		}

		public unowned ObjectReader end_member () {
			pop_scope ();

			return this;
		}

		public uint count_elements () throws Error {
			var scope = peek_scope ();
			scope.check_array ();
			return (uint) scope.val.n_children ();
		}

		public unowned ObjectReader read_element (uint index) throws Error {
			var scope = peek_scope ();
			scope.check_array ();
			push_scope (scope.val.get_child_value (index).get_variant ());

			return this;
		}

		public unowned ObjectReader end_element () throws Error {
			pop_scope ();

			return this;
		}

		public bool get_bool_value () throws Error {
			return peek_scope ().get_value (VariantType.BOOLEAN).get_boolean ();
		}

		public uint8 get_uint8_value () throws Error {
			return peek_scope ().get_value (VariantType.BYTE).get_byte ();
		}

		public uint16 get_uint16_value () throws Error {
			return peek_scope ().get_value (VariantType.UINT16).get_uint16 ();
		}

		public int64 get_int64_value () throws Error {
			return peek_scope ().get_value (VariantType.INT64).get_int64 ();
		}

		public uint64 get_uint64_value () throws Error {
			return peek_scope ().get_value (VariantType.UINT64).get_uint64 ();
		}

		public Bytes get_data_value () throws Error {
			return peek_scope ().get_value (new VariantType.array (VariantType.BYTE)).get_data_as_bytes ();
		}

		public unowned string get_string_value () throws Error {
			return peek_scope ().get_value (VariantType.STRING).get_string ();
		}

		public unowned string get_uuid_value () throws Error {
			return peek_scope ().get_value (VariantType.STRING).get_string (); // TODO: Use a tuple to avoid ambiguity.
		}

		private void push_scope (Variant v) {
			scopes.offer_tail (new Scope (v));
		}

		private Scope peek_scope () {
			return scopes.peek_tail ();
		}

		private Scope pop_scope () {
			return scopes.poll_tail ();
		}

		private class Scope {
			public Variant val;
			public VariantDict? dict;
			public bool is_array = false;

			public Scope (Variant v) {
				val = v;

				VariantType t = v.get_type ();
				if (t.equal (VariantType.VARDICT))
					dict = new VariantDict (v);
				else if (t.is_subtype_of (VariantType.ARRAY))
					is_array = true;
			}

			public Variant get_value (VariantType expected_type) throws Error {
				if (!val.get_type ().equal (expected_type)) {
					throw new Error.PROTOCOL ("Expected type '%s', got '%s'",
						(string) expected_type.peek_string (),
						(string) val.get_type ().peek_string ());
				}

				return val;
			}

			public void check_array () throws Error {
				if (!is_array)
					throw new Error.PROTOCOL ("Array expected, but at %s", val.print (true));
			}
		}
	}

	public sealed class JsonObjectBuilder : Object, ObjectBuilder {
		private Json.Builder builder = new Json.Builder ();
		private Gee.Map<string, Bytes> raw_values = new Gee.HashMap<string, Bytes> ();

		public unowned ObjectBuilder begin_dictionary () {
			builder.begin_object ();
			return this;
		}

		public unowned ObjectBuilder set_member_name (string name) {
			builder.set_member_name (name);
			return this;
		}

		public unowned ObjectBuilder end_dictionary () {
			builder.end_object ();
			return this;
		}

		public unowned ObjectBuilder begin_array () {
			builder.begin_array ();
			return this;
		}

		public unowned ObjectBuilder end_array () {
			builder.end_array ();
			return this;
		}

		public unowned ObjectBuilder add_null_value () {
			builder.add_null_value ();
			return this;
		}

		public unowned ObjectBuilder add_bool_value (bool val) {
			builder.add_boolean_value (val);
			return this;
		}

		public unowned ObjectBuilder add_int64_value (int64 val) {
			builder.add_int_value (val);
			return this;
		}

		public unowned ObjectBuilder add_uint64_value (uint64 val) {
			builder.add_int_value ((int64) val);
			return this;
		}

		public unowned ObjectBuilder add_data_value (Bytes val) {
			builder.add_string_value (Base64.encode (val.get_data ()));
			return this;
		}

		public unowned ObjectBuilder add_string_value (string val) {
			builder.add_string_value (val);
			return this;
		}

		public unowned ObjectBuilder add_uuid_value (uint8[] val) {
			assert_not_reached ();
		}

		public unowned ObjectBuilder add_raw_value (Bytes val) {
			string uuid = Uuid.string_random ();
			builder.add_string_value (uuid);
			raw_values[uuid] = val;
			return this;
		}

		public Bytes build () {
			string json = Json.to_string (builder.get_root (), false);

			foreach (var e in raw_values.entries) {
				unowned string uuid = e.key;
				Bytes val = e.value;

				unowned string raw_str = (string) val.get_data ();
				string str = raw_str[:(long) val.get_size ()];

				json = json.replace ("\"" + uuid + "\"", str);
			}

			return new Bytes (json.data);
		}
	}

	public sealed class JsonObjectReader : Object, ObjectReader {
		private Json.Reader reader;

		public JsonObjectReader (string json) throws Error {
			try {
				reader = make_json_reader (json);
			} catch (GLib.Error e) {
				throw new Error.INVALID_ARGUMENT ("%s", e.message);
			}
		}

		public bool has_member (string name) throws Error {
			bool result = reader.read_member (name);
			reader.end_member ();
			return result;
		}

		public unowned ObjectReader read_member (string name) throws Error {
			if (!reader.read_member (name))
				throw_dict_access_error ();
			return this;
		}

		public unowned ObjectReader end_member () {
			reader.end_member ();
			return this;
		}

		[NoReturn]
		private void throw_dict_access_error () throws Error {
			GLib.Error e = reader.get_error ();
			reader.end_member ();
			throw new Error.PROTOCOL ("%s", e.message);
		}

		public uint count_elements () throws Error {
			int n = reader.count_elements ();
			if (n == -1)
				throw_array_access_error ();
			return n;
		}

		public unowned ObjectReader read_element (uint index) throws Error {
			if (!reader.read_element (index)) {
				GLib.Error e = reader.get_error ();
				reader.end_element ();
				throw new Error.PROTOCOL ("%s", e.message);
			}
			return this;
		}

		public unowned ObjectReader end_element () throws Error {
			reader.end_element ();
			return this;
		}

		[NoReturn]
		private void throw_array_access_error () throws Error {
			GLib.Error e = reader.get_error ();
			reader.end_element ();
			throw new Error.PROTOCOL ("%s", e.message);
		}

		public bool get_bool_value () throws Error {
			bool v = reader.get_boolean_value ();
			if (!v)
				maybe_throw_value_access_error ();
			return v;
		}

		public uint8 get_uint8_value () throws Error {
			int64 v = get_int64_value ();
			if (v < 0 || v > uint8.MAX)
				throw new Error.PROTOCOL ("Invalid uint8");
			return (uint8) v;
		}

		public uint16 get_uint16_value () throws Error {
			int64 v = get_int64_value ();
			if (v < 0 || v > uint16.MAX)
				throw new Error.PROTOCOL ("Invalid uint16");
			return (uint16) v;
		}

		public int64 get_int64_value () throws Error {
			int64 v = reader.get_int_value ();
			if (v == 0)
				maybe_throw_value_access_error ();
			return v;
		}

		public uint64 get_uint64_value () throws Error {
			int64 v = get_int64_value ();
			if (v < 0)
				throw new Error.PROTOCOL ("Invalid uint64");
			return v;
		}

		public Bytes get_data_value () throws Error {
			return new Bytes (Base64.decode (get_string_value ()));
		}

		public unowned string get_string_value () throws Error {
			unowned string? v = reader.get_string_value ();
			if (v == null)
				maybe_throw_value_access_error ();
			return v;
		}

		public unowned string get_uuid_value () throws Error {
			return get_string_value ();
		}

		private void maybe_throw_value_access_error () throws Error {
			GLib.Error? e = reader.get_error ();
			if (e == null)
				return;
			reader.end_member ();
			throw new Error.PROTOCOL ("%s", e.message);
		}
	}

	/*
	 * Constructs a Json.Reader for the given JSON string, working around the
	 * json_node_copy() bug in unpatched json-glib (GNOME bugzilla #774684).
	 *
	 * Json.Reader's set_root() copies the supplied node via json_node_copy().
	 * For object/array nodes that copy is a thin shell that shares the original
	 * JsonObject/JsonArray via refcount, but the members inside the shared
	 * container have their parent pointers set to the *original* root, not the
	 * shell. If the original is then dropped, json_reader_end_member()'s walk
	 * via json_node_get_parent() touches a freed node — which on builds with
	 * glib checks enabled (e.g. Fedora) trips JSON_NODE_IS_VALID and aborts.
	 *
	 * We keep the original root alive by attaching it to the reader as object
	 * data; the destroy notify drops our reference when the reader is finalized.
	 */
	public Json.Reader make_json_reader (string json) throws GLib.Error {
		return make_json_reader_taking_node (Json.from_string (json));
	}

	public Json.Reader make_json_reader_taking_node (owned Json.Node root) {
		var reader = new Json.Reader (root);
		reader.set_data<Json.Node> ("frida-json-root", (owned) root);
		return reader;
	}
}
