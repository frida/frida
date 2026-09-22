[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public class NSObject {
		public virtual uint hash () {
			return (uint) this;
		}

		public virtual bool is_equal_to (NSObject other) {
			return other == this;
		}

		public string to_string (uint level = 0) {
			var s = new StringBuilder.sized (256);
			append_indent (s, level);
			append_content (s, level);
			return s.str;
		}

		protected virtual void append_content (StringBuilder s, uint level) {
			s.append ("NSObject {}");
		}

		protected void append_indent (StringBuilder s, uint level) {
			for (uint i = 0; i != level; i++)
				s.append ("  ");
		}

		public static uint hash_func (NSObject val) {
			return val.hash ();
		}

		public static bool equal_func (NSObject a, NSObject b) {
			return a.is_equal_to (b);
		}
	}

	public sealed class NSNumber : NSObject {
		public Kind kind {
			get;
			private set;
		}

		public bool boolean {
			get;
			private set;
		}

		public int64 integer {
			get;
			private set;
		}

		public double number {
			get;
			private set;
		}

		public enum Kind {
			BOOLEAN,
			INTEGER,
			FLOAT,
			DOUBLE,
		}

		public NSNumber.from_boolean (bool val) {
			kind = BOOLEAN;
			boolean = val;
			integer = val ? 1 : 0;
			number = val ? 1.0 : 0.0;
		}

		public NSNumber.from_integer (int64 val) {
			kind = INTEGER;
			boolean = (val != 0) ? true : false;
			integer = val;
			number = val;
		}

		public NSNumber.from_float (float val) {
			kind = FLOAT;
			boolean = (val != 0.0f) ? true : false;
			integer = (int64) val;
			number = val;
		}

		public NSNumber.from_double (double val) {
			kind = DOUBLE;
			boolean = (val != 0.0) ? true : false;
			integer = (int64) val;
			number = val;
		}

		public override uint hash () {
			return (uint) integer;
		}

		public override bool is_equal_to (NSObject other) {
			var other_number = other as NSNumber;
			if (other_number == null)
				return false;

			if (other_number.kind != kind)
				return false;

			switch (kind) {
				case BOOLEAN:
					return other_number.boolean == boolean;
				case INTEGER:
					return other_number.integer == integer;
				case FLOAT:
				case DOUBLE:
					return other_number.number == number;
			}

			return false;
		}

		protected override void append_content (StringBuilder s, uint level) {
			switch (kind) {
				case BOOLEAN:
					s.append (boolean ? "true" : "false");
					break;
				case INTEGER:
					s.append_printf ("%" + int64.FORMAT, integer);
					break;
				case FLOAT:
				case DOUBLE:
					s.append_printf ("%f", number);
					break;
			}
		}
	}

	public sealed class NSString : NSObject {
		public string str {
			get;
			private set;
		}

		public NSString (string str) {
			this.str = str;
		}

		public override uint hash () {
			return str.hash ();
		}

		public override bool is_equal_to (NSObject other) {
			var other_string = other as NSString;
			if (other_string == null)
				return false;
			return other_string.str == str;
		}

		protected override void append_content (StringBuilder s, uint level) {
			s
				.append_c ('"')
				.append (str.replace ("\"", "\\\""))
				.append_c ('"');
		}
	}

	public sealed class NSData : NSObject {
		public Bytes bytes {
			get;
			private set;
		}

		public NSData (Bytes bytes) {
			this.bytes = bytes;
		}

		public override uint hash () {
			return bytes.hash ();
		}

		public override bool is_equal_to (NSObject other) {
			var other_data = other as NSData;
			if (other_data == null)
				return false;
			return other_data.bytes.compare (bytes) == 0;
		}

		protected override void append_content (StringBuilder s, uint level) {
			s.append_printf ("NSData { size = %" + size_t.FORMAT + " }", bytes.get_size ());
		}
	}

	public sealed class NSDictionary : NSObject {
		public int size {
			get {
				return storage.size;
			}
		}

		public Gee.Set<Gee.Map.Entry<string, NSObject>> entries {
			owned get {
				return storage.entries;
			}
		}

		public Gee.Iterable<string> keys {
			owned get {
				return storage.keys;
			}
		}

		public Gee.Iterable<NSObject> values {
			owned get {
				return storage.values;
			}
		}

		private Gee.HashMap<string, NSObject> storage;

		public NSDictionary (Gee.HashMap<string, NSObject>? storage = null) {
			this.storage = (storage != null) ? storage : new Gee.HashMap<string, NSObject> ();
		}

		public unowned T get_value<T> (string key) throws Error {
			unowned T? val;
			if (!get_optional_value<T> (key, out val))
				throw new Error.PROTOCOL ("Expected dictionary to contain “%s”", key);
			return val;
		}

		public bool get_optional_value<T> (string key, out unowned T? val) throws Error {
			val = null;

			NSObject? opaque_obj = storage[key];
			if (opaque_obj == null)
				return false;

			Type expected_type = typeof (T);
			Type actual_type = Type.from_instance (opaque_obj);
			if (!actual_type.is_a (expected_type)) {
				throw new Error.PROTOCOL ("Expected “%s” to be a %s but got %s",
					key, expected_type.name (), actual_type.name ());
			}

			val = (T) opaque_obj;
			return true;
		}

		public void set_value (string key, NSObject val) {
			storage[key] = val;
		}

		protected override void append_content (StringBuilder s, uint level) {
			if (storage.size == 0) {
				s.append ("{}");
				return;
			}

			s.append ("{\n");
			foreach (var e in entries) {
				append_indent (s, level + 1);
				s
					.append (e.key)
					.append (": ");
				e.value.append_content (s, level + 1);
				s.append (",\n");
			}
			append_indent (s, level);
			s.append_c ('}');
		}
	}

	public sealed class NSDictionaryRaw : NSObject {
		public int size {
			get {
				return storage.size;
			}
		}

		public Gee.Set<Gee.Map.Entry<NSObject, NSObject>> entries {
			owned get {
				return storage.entries;
			}
		}

		public Gee.Iterable<NSObject> keys {
			owned get {
				return storage.keys;
			}
		}

		public Gee.Iterable<NSObject> values {
			owned get {
				return storage.values;
			}
		}

		private Gee.HashMap<NSObject, NSObject> storage;

		public NSDictionaryRaw (Gee.HashMap<NSObject, NSObject>? storage = null) {
			this.storage = (storage != null)
				? storage
				: new Gee.HashMap<NSObject, NSObject> (NSObject.hash_func, NSObject.equal_func);
		}

		protected override void append_content (StringBuilder s, uint level) {
			if (storage.size == 0) {
				s.append ("{}");
				return;
			}

			s.append ("{\n");
			foreach (var e in entries) {
				append_indent (s, level + 1);
				e.key.append_content (s, level + 1);
				s.append (": ");
				e.value.append_content (s, level + 1);
				s.append (",\n");
			}
			append_indent (s, level);
			s.append_c ('}');
		}
	}

	public sealed class NSArray : NSObject {
		public int length {
			get {
				return storage.size;
			}
		}

		public Gee.Iterable<NSObject> elements {
			owned get {
				return storage;
			}
		}

		private Gee.ArrayList<NSObject> storage;

		public NSArray (Gee.ArrayList<NSObject>? storage = null) {
			this.storage = (storage != null) ? storage : new Gee.ArrayList<NSObject> (NSObject.equal_func);
		}

		public void add_object (NSObject obj) {
			storage.add (obj);
		}

		protected override void append_content (StringBuilder s, uint level) {
			if (storage.size == 0) {
				s.append ("[]");
				return;
			}

			s.append ("[\n");
			foreach (var e in elements) {
				append_indent (s, level + 1);
				e.append_content (s, level + 1);
				s.append (",\n");
			}
			append_indent (s, level);
			s.append_c (']');
		}
	}

	public sealed class NSSet : NSObject {
		public Gee.Iterable<NSObject> items {
			owned get {
				return storage;
			}
		}

		private Gee.HashSet<NSObject> storage;

		public NSSet (Gee.HashSet<NSObject>? storage = null) {
			this.storage = (storage != null) ? storage : new Gee.HashSet<NSObject> (NSObject.hash_func);
		}

		public void add_object (NSObject obj) {
			storage.add (obj);
		}

		protected override void append_content (StringBuilder s, uint level) {
			if (storage.size == 0) {
				s.append ("{}");
				return;
			}

			s.append ("{\n");
			foreach (var e in items) {
				append_indent (s, level + 1);
				e.append_content (s, level + 1);
				s.append (",\n");
			}
			append_indent (s, level);
			s.append_c ('}');
		}
	}

	public sealed class NSDate : NSObject {
		public double time {
			get;
			private set;
		}

		private const int64 MAC_EPOCH_DELTA_FROM_UNIX = 978307200LL;

		public NSDate (double time) {
			this.time = time;
		}

		public DateTime to_date_time () {
			int64 whole_seconds = (int64) time;
			return new DateTime.from_unix_utc (MAC_EPOCH_DELTA_FROM_UNIX + whole_seconds)
				.add_seconds (time - (double) whole_seconds);
		}

		protected override void append_content (StringBuilder s, uint level) {
			s.append_printf ("NSDate { time = %f }", time);
		}
	}

	public sealed class NSError : NSObject {
		public NSString domain {
			get;
			private set;
		}

		public int64 code {
			get;
			private set;
		}

		public NSDictionary user_info {
			get;
			private set;
		}

		public NSError (NSString domain, int64 code, NSDictionary user_info) {
			this.domain = domain;
			this.code = code;
			this.user_info = user_info;
		}

		protected override void append_content (StringBuilder s, uint level) {
			s.append ("NSError { domain = ");
			domain.append_content (s, level);
			s.append_printf (", code = %" + uint64.FORMAT_MODIFIER + "d, user_info = ", code);

			if (user_info.size == 0) {
				user_info.append_content (s, level);
				s.append (" }");
				return;
			}

			s.append ("{\n");
			append_indent (s, level + 1);
			s.append ("domain: ");
			domain.append_content (s, level + 1);
			s.append (",\n");

			append_indent (s, level + 1);
			s.append_printf ("code: %" + uint64.FORMAT_MODIFIER + "d,\n", code);

			append_indent (s, level + 1);
			s.append ("user_info: ");
			user_info.append_content (s, level + 1);
			s.append_c ('\n');

			append_indent (s, level);
			s.append_c ('}');
		}
	}

	public sealed class DTTapMessage : NSObject {
		public NSDictionary plist {
			get;
			private set;
		}

		public DTTapMessage (NSDictionary plist) {
			this.plist = plist;
		}

		protected override void append_content (StringBuilder s, uint level) {
			if (plist.size == 0) {
				s.append ("DTTapMessage { plist: ");
				plist.append_content (s, level);
				s.append (" }");
				return;
			}

			s.append ("DTTapMessage {\n");

			append_indent (s, level + 1);
			s.append ("plist: ");
			plist.append_content (s, level + 1);
			s.append_c ('\n');

			append_indent (s, level);
			s.append_c ('}');
		}
	}

	namespace NSKeyedArchive {
		private Gee.HashMap<Type, EncodeFunc> encoders;
		private Gee.HashMap<string, DecodeFunc> decoders;

		private const string[] DICTIONARY_CLASS = { "NSDictionary", "NSObject" };
		private const string[] ARRAY_CLASS = { "NSArray", "NSObject" };
		private const string[] SET_CLASS = { "NSSet", "NSObject" };

		[CCode (has_target = false)]
		private delegate PlistUid EncodeFunc (NSObject instance, EncodingContext ctx);

		[CCode (has_target = false)]
		private delegate NSObject DecodeFunc (PlistDict instance, DecodingContext ctx) throws Error, PlistError;

		public static uint8[] encode (NSObject? obj) {
			if (obj == null)
				return new uint8[0];

			ensure_encoders_registered ();

			var objects = new PlistArray ();
			objects.add_string ("$null");

			var ctx = new EncodingContext (objects);

			var top = new PlistDict ();
			top.set_uid ("root", encode_value (obj, ctx));

			var plist = new Plist ();
			plist.set_integer ("$version", 100000);
			plist.set_array ("$objects", objects);
			plist.set_string ("$archiver", "NSKeyedArchiver");
			plist.set_dict ("$top", top);

			return plist.to_binary ();
		}

		private static PlistUid encode_value (NSObject? obj, EncodingContext ctx) {
			if (obj == null)
				return new PlistUid (0);

			var type = Type.from_instance (obj);
			var encode_object = encoders[type];
			if (encode_object == null)
				critical ("Missing NSKeyedArchive encoder for type “%s”", type.name ());

			return encode_object (obj, ctx);
		}

		public static NSObject? decode (uint8[] data) throws Error {
			ensure_decoders_registered ();

			try {
				var plist = new Plist.from_binary (data);

				var ctx = new DecodingContext (plist.get_array ("$objects"));

				return decode_value (plist.get_dict ("$top").get_uid ("root"), ctx);
			} catch (PlistError e) {
				throw new Error.PROTOCOL ("%s", e.message);
			}
		}

		private static NSObject? decode_value (PlistUid index, DecodingContext ctx) throws Error, PlistError {
			var uid = index.uid;
			if (uid == 0)
				return null;

			var objects = ctx.objects;

			Value * val = objects.get_value ((int) uid);
			Type t = val.type ();

			if (t == typeof (bool))
				return new NSNumber.from_boolean (val.get_boolean ());

			if (t == typeof (int64))
				return new NSNumber.from_integer (val.get_int64 ());

			if (t == typeof (float))
				return new NSNumber.from_float (val.get_float ());

			if (t == typeof (double))
				return new NSNumber.from_double (val.get_double ());

			if (t == typeof (string))
				return new NSString (val.get_string ());

			if (t == typeof (Bytes))
				return new NSData ((Bytes) val.get_boxed ());

			if (t == typeof (PlistDict)) {
				var instance = (PlistDict) val.get_object ();
				var klass = objects.get_dict ((int) instance.get_uid ("$class").uid);
				var decode = get_decoder (klass);
				return decode (instance, ctx);
			}

			throw new Error.NOT_SUPPORTED ("Unsupported NSKeyedArchive type: %s", val.type_name ());
		}

		private static DecodeFunc get_decoder (PlistDict klass) throws Error, PlistError {
			var hierarchy = klass.get_array ("$classes");

			int n = hierarchy.length;
			for (int i = 0; i != n; i++) {
				var name = hierarchy.get_string (i);
				var decoder = decoders[name];
				if (decoder != null)
					return decoder;
			}

			throw new Error.NOT_SUPPORTED ("Missing NSKeyedArchive decoder for type “%s”", klass.get_string ("$classname"));
		}

		private static void ensure_encoders_registered () {
			if (encoders != null)
				return;

			encoders = new Gee.HashMap<Type, EncodeFunc> ();
			encoders[typeof (NSNumber)] = encode_number;
			encoders[typeof (NSString)] = encode_string;
			encoders[typeof (NSDictionary)] = encode_dictionary;
			encoders[typeof (NSArray)] = encode_array;
			encoders[typeof (NSSet)] = encode_set;
		}

		private static void ensure_decoders_registered () {
			if (decoders != null)
				return;

			decoders = new Gee.HashMap<string, DecodeFunc> ();
			decoders["NSDictionary"] = decode_dictionary;
			decoders["NSArray"] = decode_array;
			decoders["NSDate"] = decode_date;
			decoders["NSError"] = decode_error;
			decoders["DTTapMessage"] = decode_tap_message;
			decoders["DTSysmonTapMessage"] = decode_tap_message;
			decoders["DTActivityTraceTapMessage"] = decode_tap_message;
			decoders["DTTapStatusMessage"] = decode_tap_message;
			decoders["DTTapHeartbeatMessage"] = decode_tap_message;
			decoders["DTKTraceTapMessage"] = decode_tap_message;
		}

		private static PlistUid encode_number (NSObject instance, EncodingContext ctx) {
			var n = (NSNumber) instance;
			switch (n.kind) {
				case BOOLEAN:
					return encode_boolean (n.boolean, ctx);
				case INTEGER:
					return encode_integer (n.integer, ctx);
				case FLOAT:
					return encode_float (n.integer, ctx);
				case DOUBLE:
					return encode_double (n.integer, ctx);
			}
			assert_not_reached ();
		}

		private static PlistUid encode_boolean (bool val, EncodingContext ctx) {
			var uid = ctx.find_existing_object (e => e.holds (typeof (bool)) && e.get_boolean () == val);
			if (uid != null)
				return uid;

			var objects = ctx.objects;
			uid = new PlistUid (objects.length);
			objects.add_boolean (val);
			return uid;
		}

		private static PlistUid encode_integer (int64 val, EncodingContext ctx) {
			var uid = ctx.find_existing_object (e => e.holds (typeof (int64)) && e.get_int64 () == val);
			if (uid != null)
				return uid;

			var objects = ctx.objects;
			uid = new PlistUid (objects.length);
			objects.add_integer (val);
			return uid;
		}

		private static PlistUid encode_float (float val, EncodingContext ctx) {
			var uid = ctx.find_existing_object (e => e.holds (typeof (float)) && e.get_float () == val);
			if (uid != null)
				return uid;

			var objects = ctx.objects;
			uid = new PlistUid (objects.length);
			objects.add_float (val);
			return uid;
		}

		private static PlistUid encode_double (double val, EncodingContext ctx) {
			var uid = ctx.find_existing_object (e => e.holds (typeof (double)) && e.get_double () == val);
			if (uid != null)
				return uid;

			var objects = ctx.objects;
			uid = new PlistUid (objects.length);
			objects.add_double (val);
			return uid;
		}

		private static PlistUid encode_string (NSObject instance, EncodingContext ctx) {
			string str = ((NSString) instance).str;

			var uid = ctx.find_existing_object (e => e.holds (typeof (string)) && e.get_string () == str);
			if (uid != null)
				return uid;

			var objects = ctx.objects;
			uid = new PlistUid (objects.length);
			objects.add_string (str);
			return uid;
		}

		private static PlistUid encode_dictionary (NSObject instance, EncodingContext ctx) {
			NSDictionary dict = (NSDictionary) instance;

			var object = new PlistDict ();
			var uid = ctx.add_object (object);

			var keys = new PlistArray ();
			var objs = new PlistArray ();
			foreach (var entry in dict.entries) {
				var key = encode_value (new NSString (entry.key), ctx);
				var obj = encode_value (entry.value, ctx);

				keys.add_uid (key);
				objs.add_uid (obj);
			}
			object.set_array ("NS.keys", keys);
			object.set_array ("NS.objects", objs);
			object.set_uid ("$class", ctx.get_class (DICTIONARY_CLASS));

			return uid;
		}

		private static NSObject decode_dictionary (PlistDict instance, DecodingContext ctx) throws Error, PlistError {
			var keys = instance.get_array ("NS.keys");
			var objs = instance.get_array ("NS.objects");

			int n = keys.length;

			var string_keys = new Gee.ArrayList<string> ();
			for (int i = 0; i != n; i++) {
				var key = decode_value (keys.get_uid (i), ctx) as NSString;
				if (key is NSString)
					string_keys.add (key.str);
				else
					break;
			}

			if (string_keys.size == n) {
				var storage = new Gee.HashMap<string, NSObject> ();

				for (int i = 0; i != n; i++)
					storage[string_keys[i]] = decode_value (objs.get_uid (i), ctx);

				return new NSDictionary (storage);
			} else {
				var storage = new Gee.HashMap<NSObject, NSObject> (NSObject.hash_func, NSObject.equal_func);

				for (int i = 0; i != n; i++) {
					var key = decode_value (keys.get_uid (i), ctx);
					var obj = decode_value (objs.get_uid (i), ctx);

					storage[key] = obj;
				}

				return new NSDictionaryRaw (storage);
			}
		}

		private static PlistUid encode_array (NSObject instance, EncodingContext ctx) {
			NSArray array = (NSArray) instance;

			var object = new PlistDict ();
			var uid = ctx.add_object (object);

			var objs = new PlistArray ();
			foreach (var element in array.elements)
				objs.add_uid (encode_value (element, ctx));
			object.set_array ("NS.objects", objs);
			object.set_uid ("$class", ctx.get_class (ARRAY_CLASS));

			return uid;
		}

		private static PlistUid encode_set (NSObject instance, EncodingContext ctx) {
			NSSet set = (NSSet) instance;

			var object = new PlistDict ();
			var uid = ctx.add_object (object);

			var objs = new PlistArray ();
			foreach (var element in set.items)
				objs.add_uid (encode_value (element, ctx));
			object.set_array ("NS.objects", objs);
			object.set_uid ("$class", ctx.get_class (SET_CLASS));

			return uid;
		}

		private static NSObject decode_array (PlistDict instance, DecodingContext ctx) throws Error, PlistError {
			var objs = instance.get_array ("NS.objects");

			var storage = new Gee.ArrayList<NSObject> (NSObject.equal_func);

			var n = objs.length;
			for (int i = 0; i != n; i++) {
				var obj = decode_value (objs.get_uid (i), ctx);

				storage.add (obj);
			}

			return new NSArray (storage);
		}

		private static NSObject decode_date (PlistDict instance, DecodingContext ctx) throws Error, PlistError {
			var time = instance.get_double ("NS.time");

			return new NSDate (time);
		}

		private static NSObject decode_error (PlistDict instance, DecodingContext ctx) throws Error, PlistError {
			NSString? domain = decode_value (instance.get_uid ("NSDomain"), ctx) as NSString;
			if (domain == null)
				throw new Error.PROTOCOL ("Malformed NSError");

			int64 code = instance.get_integer ("NSCode");

			NSObject? user_info = decode_value (instance.get_uid ("NSUserInfo"), ctx);
			if (user_info != null && !(user_info is NSDictionary))
				throw new Error.PROTOCOL ("Malformed NSError");

			return new NSError (domain, code, (NSDictionary) user_info);
		}

		private static NSObject decode_tap_message (PlistDict instance, DecodingContext ctx) throws Error, PlistError {
			return new DTTapMessage ((NSDictionary) decode_value (instance.get_uid ("DTTapMessagePlist"), ctx));
		}

		private sealed class EncodingContext {
			public PlistArray objects;

			private Gee.HashMap<string, PlistUid> classes = new Gee.HashMap<string, PlistUid> ();

			public delegate void AddObjectFunc (PlistArray objects);

			public EncodingContext (PlistArray objects) {
				this.objects = objects;
			}

			public PlistUid? find_existing_object (Gee.Predicate<Value *> predicate) {
				int64 uid = 0;
				foreach (var e in objects.elements) {
					if (uid > 0 && predicate (e))
						return new PlistUid (uid);
					uid++;
				}

				return null;
			}

			public PlistUid add_object (PlistDict obj) {
				var uid = new PlistUid (objects.length);
				objects.add_dict (obj);
				return uid;
			}

			public PlistUid get_class (string[] description) {
				var canonical_name = description[0];

				var uid = classes[canonical_name];
				if (uid != null)
					return uid;

				var spec = new PlistDict ();

				var hierarchy = new PlistArray ();
				foreach (var name in description)
					hierarchy.add_string (name);
				spec.set_array ("$classes", hierarchy);

				spec.set_string ("$classname", canonical_name);

				uid = add_object (spec);
				classes[canonical_name] = uid;

				return uid;
			}
		}

		private sealed class DecodingContext {
			public PlistArray objects;

			public DecodingContext (PlistArray objects) {
				this.objects = objects;
			}
		}
	}
}
