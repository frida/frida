[CCode (cheader_filename = "xpc/xpc.h", lower_case_cprefix = "xpc_", gir_namespace = "Darwin", gir_version = "1.0")]
namespace Darwin.Xpc {
	[Compact]
	[CCode (cname = "gpointer", ref_function = "xpc_retain", unref_function = "xpc_release")]
	public class Connection {
		public static Connection create (string? name, GCD.DispatchQueue targetq);
		public static Connection create_mach_service (string? name, GCD.DispatchQueue targetq, uint64 flags = 0);

		[CCode (cname = "_frida_xpc_connection_set_event_handler", cheader_filename = "frida-darwin.h")]
		public void set_event_handler (Handler handler);

		public void activate ();
		public void cancel ();

		public void send_message (Object message);
		[CCode (cname = "_frida_xpc_connection_send_message_with_reply", cheader_filename = "frida-darwin.h")]
		public void send_message_with_reply (Object message, GCD.DispatchQueue replyq, owned Handler handler);
	}

	[CCode (cname = "FridaXpcHandler")]
	public delegate void Handler (Object object);

	[Compact]
	[CCode (cname = "gpointer", ref_function = "xpc_retain", unref_function = "xpc_release")]
	public class Object {
		public Type type {
			[CCode (cname = "xpc_get_type")]
			get;
		}

		[CCode (cname = "_frida_xpc_object_to_string", cheader_filename = "frida-darwin.h")]
		public string to_string ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Type {
		public string name {
			get;
		}
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Bool : Object {
		[CCode (cname = "XPC_TYPE_BOOL")]
		public static Type TYPE;

		[CCode (cname = "xpc_bool_create")]
		public Bool (bool val);

		public bool get_value ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Int64 : Object {
		[CCode (cname = "XPC_TYPE_INT64")]
		public static Type TYPE;

		[CCode (cname = "xpc_int64_create")]
		public Int64 (int64 val);

		public int64 get_value ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class UInt64 : Object {
		[CCode (cname = "XPC_TYPE_UINT64")]
		public static Type TYPE;

		[CCode (cname = "xpc_uint64_create")]
		public UInt64 (uint64 val);

		public uint64 get_value ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Data : Object {
		[CCode (cname = "XPC_TYPE_DATA")]
		public static Type TYPE;

		[CCode (cname = "xpc_data_create")]
		public Data (uint8[] bytes);

		public size_t get_length ();

		public void * get_bytes_ptr ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class String : Object {
		[CCode (cname = "XPC_TYPE_STRING")]
		public static Type TYPE;

		[CCode (cname = "xpc_string_create")]
		public String (string val);

		public unowned string get_string_ptr ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Uuid : Object {
		[CCode (cname = "XPC_TYPE_UUID")]
		public static Type TYPE;

		[CCode (array_length = false)]
		public unowned uint8[] get_bytes ();
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Array : Object {
		[CCode (cname = "XPC_TYPE_ARRAY")]
		public static Type TYPE;

		public size_t count {
			get;
		}

		[CCode (cname = "xpc_array_create_empty")]
		public Array ();

		public unowned Object? get_value (size_t index);
		public void set_value (size_t index, Object val);
	}

	[Compact]
	[CCode (cname = "gpointer")]
	public class Dictionary : Object {
		[CCode (cname = "XPC_TYPE_DICTIONARY")]
		public static Type TYPE;

		[CCode (cname = "xpc_dictionary_create_empty")]
		public Dictionary ();

		public bool get_bool (string key);
		public void set_bool (string key, bool val);

		public int64 get_int64 (string key);
		public void set_int64 (string key, int64 val);

		public uint64 get_uint64 (string key);
		public void set_uint64 (string key, uint64 val);

		[CCode (array_length_type = "size_t")]
		public unowned uint8[]? get_data (string key);
		public void set_data (string key, uint8[] bytes);

		public unowned string? get_string (string key);
		public void set_string (string key, string val);

		public unowned Dictionary? get_dictionary (string key);

		public unowned Object? get_value (string key);
		public void set_value (string key, Object val);

		[CCode (cname = "_frida_xpc_dictionary_apply", cheader_filename = "frida-darwin.h")]
		public bool apply (DictionaryApplier applier);

		public Connection? create_connection (string key);
	}

	[CCode (cname = "FridaXpcDictionaryApplier")]
	public delegate bool DictionaryApplier (string key, Object val);

	[Compact]
	[CCode (cname = "gpointer")]
	public class Error : Dictionary {
		[CCode (cname = "XPC_TYPE_ERROR")]
		public static Type TYPE;

		public const string KEY_DESCRIPTION;
	}
}
