[CCode (cheader_filename = "quickjs.h", cprefix = "JS", gir_namespace = "QuickJS", gir_version = "1.0")]
namespace QuickJS {
	[Compact]
	[CCode (free_function = "JS_FreeRuntime")]
	public class Runtime {
		[CCode (cname = "JS_NewRuntime")]
		public static Runtime make ();

		[CCode (cname = "JS_GetRuntimeOpaque")]
		public void * get_opaque ();

		[CCode (cname = "JS_SetRuntimeOpaque")]
		public void set_opaque (void * opaque);

		[CCode (cname = "JS_NewClass")]
		public int make_class (ClassID id, ClassDef def);

		[CCode (cname = "JS_SetModuleLoaderFunc")]
		public void set_module_loader_func ([CCode (delegate_target_pos = 2.1)] ModuleNormalizeFunc module_normalize, ModuleLoaderFunc module_loader);

		[CCode (cname = "JS_ExecutePendingJob")]
		public int execute_pending_job (out unowned Context? ctx);
	}

	[CCode (cname = "JS_NewClassID")]
	public ClassID make_class_id (ref ClassID id);

	[CCode (cname = "JSClassID", has_type_id = false)]
	public struct ClassID : uint32 {
	}

	public struct ClassDef {
		public unowned string class_name;
		public ClassFinalizer finalizer;
		public ClassGCMark gc_mark;
		public ClassCall call;
		public ClassExoticMethods * exotic;
	}

	public struct ClassExoticMethods {
		public GetOwnPropertyFunc get_own_property;
		public GetOwnPropertyNamesFunc get_own_property_names;
		public DeletePropertyFunc delete_property;
		public DefineOwnPropertyFunc define_own_property;
		public HasPropertyFunc has_property;
		public GetPropertyFunc get_property;
		public SetPropertyFunc set_property;
	}

	[CCode (has_target = false)]
	public delegate void ClassFinalizer (Runtime rt, Value val);

	[CCode (has_target = false)]
	public delegate void ClassGCMark (Runtime rt, Value val, MarkFunc mark_func);

	[CCode (has_target = false)]
	public delegate Value ClassCall (Context ctx, Value func_obj, Value this_val, [CCode (array_length_pos = 3.1)] Value[] argv,
		int flags);

	[CCode (cname = "JS_MarkFunc", has_target = false)]
	public delegate void MarkFunc (Runtime rt, void * gp);

	[CCode (has_target = false)]
	public delegate int GetOwnPropertyFunc (Context ctx, PropertyDescriptor desc, Value obj, Atom prop);

	[CCode (has_target = false)]
	public delegate int GetOwnPropertyNamesFunc (Context ctx, out PropertyEnum * tab, out uint32 len, Value obj);

	[CCode (has_target = false)]
	public delegate int DeletePropertyFunc (Context ctx, Value obj, Atom prop);

	[CCode (has_target = false)]
	public delegate int DefineOwnPropertyFunc (Context ctx, Value this_obj, Atom prop, Value val, Value getter, Value setter,
		PropertyFlags flags);

	[CCode (has_target = false)]
	public delegate int HasPropertyFunc (Context ctx, Value obj, Atom atom);

	[CCode (has_target = false)]
	public delegate Value GetPropertyFunc (Context ctx, Value obj, Atom atom, Value receiver);

	[CCode (has_target = false)]
	public delegate int SetPropertyFunc (Context ctx, Value obj, Atom atom, Value val, Value receiver, PropertyFlags flags);

	public delegate string * ModuleNormalizeFunc (Context ctx, string base_name, string name);
	public delegate unowned ModuleDef? ModuleLoaderFunc (Context ctx, string name);

	[Compact]
	[CCode (free_function = "JS_FreeContext")]
	public class Context {
		[CCode (cname = "JS_NewContext")]
		public static Context make (Runtime rt);

		[CCode (cname = "JS_GetContextOpaque")]
		public void * get_opaque ();

		[CCode (cname = "JS_SetContextOpaque")]
		public void set_opaque (void * opaque);

		[CCode (cname = "JS_SetClassProto")]
		public void set_class_proto (ClassID class_id, Value obj);

		[CCode (cname = "JS_Eval")]
		public Value eval (string input, size_t input_len, string filename, int flags);

		[CCode (cname = "JS_EvalFunction")]
		public Value eval_function (Value fun_obj);

		[CCode (cname = "JS_Throw")]
		public void throw (Value obj);

		[CCode (cname = "JS_GetException")]
		public Value get_exception ();

		[CCode (cname = "JS_GetGlobalObject")]
		public Value get_global_object ();

		[CCode (cname = "JS_NewAtom")]
		public Atom make_atom (string str);

		[CCode (cname = "JS_NewBool")]
		public Value make_bool (bool v);

		[CCode (cname = "JS_NewInt32")]
		public Value make_int32 (int32 v);

		[CCode (cname = "JS_NewUint32")]
		public Value make_uint32 (uint32 v);

		[CCode (cname = "JS_NewInt64")]
		public Value make_int64 (int64 v);

		[CCode (cname = "JS_NewBigInt64")]
		public Value make_bigint64 (int64 v);

		[CCode (cname = "JS_NewBigUint64")]
		public Value make_biguint64 (uint64 v);

		[CCode (cname = "JS_NewFloat64")]
		public Value make_float64 (double d);

		[CCode (cname = "JS_NewString")]
		public Value make_string (string str);

		[CCode (cname = "JS_NewObject")]
		public Value make_object ();

		[CCode (cname = "JS_NewObjectProtoClass")]
		public Value make_object_with_proto_and_class (Value proto, ClassID class_id);

		[CCode (cname = "JS_NewObjectClass")]
		public Value make_object_class (ClassID class_id);

		[CCode (cname = "JS_NewObjectProto")]
		public Value make_object_proto (Value proto);

		[CCode (cname = "JS_NewError")]
		public Value make_error ();

		[CCode (cname = "JS_NewArray")]
		public Value make_array ();

		[CCode (cname = "JS_NewCFunction")]
		public Value make_cfunction (CFunction func, string name, int length);

		[CCode (cname = "JS_NewCFunction2")]
		public Value make_cfunction2 (CFunction func, string name, int length, CFunctionEnum cproto, int magic);

		[CCode (cname = "JS_NewCFunctionData")]
		public Value make_cfunction_data (CFunctionData func, int length, int magic, [CCode (array_length_pos = 3.1)] Value[] data);

		[CCode (cname = "JS_NewPromiseCapability")]
		public Value make_promise ([CCode (array_length = false, array_null_terminated = false)] Value[] resolving_funcs);

		[CCode (cname = "JS_NewArrayBufferCopy")]
		public Value make_array_buffer ([CCode (array_length_type = "size_t")] uint8[] data);

		[CCode (cname = "JS_NewArrayBuffer")]
		public Value make_array_buffer_with_free_func ([CCode (array_length_type = "size_t")] owned uint8[] data,
			FreeArrayBufferDataFunc free_func, bool is_shared);

		[CCode (cname = "JS_DupValue")]
		public Value dup_value (Value v);

		[CCode (cname = "JS_FreeValue")]
		public void free_value (Value v);

		[CCode (cname = "JS_DupAtom")]
		public Atom dup_atom (Atom v);

		[CCode (cname = "JS_FreeAtom")]
		public void free_atom (Atom v);

		[CCode (cname = "JS_FreeCString")]
		public void free_cstring (string * ptr);

		[CCode (cname = "js_malloc")]
		public void * malloc (size_t size);

		[CCode (cname = "js_free")]
		public void free (void * ptr);

		[CCode (cname = "js_strdup")]
		public string * strdup (string str);
	}

	[CCode (has_target = false)]
	public delegate Value CFunction (Context ctx, Value this_val, [CCode (array_length_pos = 2.1)] Value[] argv);

	[CCode (has_target = false)]
	public delegate Value CFunctionData (Context ctx, Value this_val, [CCode (array_length_pos = 2.1)] Value[] argv, int magic,
		[CCode (array_length = false, array_null_terminated = false)] Value[] data);

	[CCode (cprefix = "JS_CFUNC_", has_type_id = false)]
	public enum CFunctionEnum {
		generic,
		generic_magic,
		constructor,
		constructor_magic,
		constructor_or_func,
		constructor_or_func_magic,
		f_f,
		f_f_f,
		getter,
		setter,
		getter_magic,
		setter_magic,
		iterator_next,
	}

	[CCode (instance_pos = 1.1)]
	public delegate void FreeArrayBufferDataFunc (Runtime rt, void * ptr);

	[CCode (has_type_id = false, default_value = "JS_UNDEFINED")]
	public struct Value : uint64 {
		[CCode (cname = "JS_IsNull")]
		public bool is_null ();

		[CCode (cname = "JS_IsUndefined")]
		public bool is_undefined ();

		[CCode (cname = "JS_IsException")]
		public bool is_exception ();

		[CCode (cname = "JS_IsString")]
		public bool is_string ();

		[CCode (cname = "JS_IsObject")]
		public bool is_object ();

		[CCode (cname = "JS_IsError", instance_pos = 1.1)]
		public bool is_error (Context ctx);

		[CCode (cname = "JS_IsArray", instance_pos = 1.1)]
		public bool is_array (Context ctx);

		[CCode (cname = "JS_IsFunction", instance_pos = 1.1)]
		public bool is_function (Context ctx);

		[CCode (cname = "JS_VALUE_GET_PTR")]
		public void * get_ptr ();

		[CCode (cname = "JS_ToCString", instance_pos = 1.1)]
		public unowned string * to_cstring (Context ctx);

		[CCode (cname = "JS_ToBool", instance_pos = 1.1)]
		public int to_bool (Context ctx);

		[CCode (cname = "JS_ToInt32", instance_pos = 2.1)]
		public int to_int32 (Context ctx, out int32 res);

		[CCode (cname = "JS_ToUint32", instance_pos = 2.1)]
		public int to_uint32 (Context ctx, out uint32 res);

		[CCode (cname = "JS_ToFloat64", instance_pos = 2.1)]
		public int to_float64 (Context ctx, out double res);

		[CCode (cname = "JS_GetArrayBuffer", instance_pos = -1.1, array_length_type = "size_t")]
		public unowned uint8[]? get_array_buffer (Context ctx);

		[CCode (cname = "JS_GetTypedArrayBuffer", instance_pos = 1.1)]
		public Value get_typed_array_buffer (Context ctx, size_t * byte_offset = null, size_t * byte_length = null,
			size_t * bytes_per_element = null);

		[CCode (cname = "JS_GetProperty", instance_pos = 1.1)]
		public Value get_property (Context ctx, Atom prop);

		[CCode (cname = "JS_GetPropertyStr", instance_pos = 1.1)]
		public Value get_property_str (Context ctx, string prop);

		[CCode (cname = "JS_GetPropertyUint32", instance_pos = 1.1)]
		public Value get_property_uint32 (Context ctx, uint32 idx);

		[CCode (cname = "JS_SetProperty", instance_pos = 1.1)]
		public void set_property (Context ctx, Atom prop, Value val);

		[CCode (cname = "JS_SetPropertyStr", instance_pos = 1.1)]
		public void set_property_str (Context ctx, string prop, Value val);

		[CCode (cname = "JS_SetPropertyUint32", instance_pos = 1.1)]
		public void set_property_uint32 (Context ctx, uint32 idx, Value val);

		[CCode (cname = "JS_SetPropertyInt64", instance_pos = 1.1)]
		public void set_property_int64 (Context ctx, int64 idx, Value val);

		[CCode (cname = "JS_DefineProperty", instance_pos = 1.1)]
		public void define_property (Context ctx, Atom prop, Value val, Value getter, Value setter, PropertyFlags flags);

		[CCode (cname = "JS_DefinePropertyGetSet", instance_pos = 1.1)]
		public void define_property_get_set (Context ctx, Atom prop, Value getter, Value setter, PropertyFlags flags);

		[CCode (cname = "JS_GetOwnPropertyNames", instance_pos = 3.1)]
		public int get_own_property_names (Context ctx, out PropertyEnum * tab, out uint32 len, GetPropertyNamesFlags flags);

		[CCode (cname = "JS_Call", instance_pos = 1.1)]
		public Value call (Context ctx, Value this_obj, [CCode (array_length_pos = 2.1)] Value[] argv);

		[CCode (cname = "JS_SetConstructor", instance_pos = 1.1)]
		public void set_constructor (Context ctx, Value proto);

		[CCode (cname = "JS_GetPrototype", instance_pos = 1.1)]
		public Value get_prototype (Context ctx);

		[CCode (cname = "JS_GetOpaque")]
		public void * get_opaque (ClassID class_id);

		[CCode (cname = "JS_GetAnyOpaque")]
		public void * get_any_opaque (out ClassID class_id);

		[CCode (cname = "JS_SetOpaque")]
		public void set_opaque (void * opaque);
	}

	[CCode (has_type_id = false, default_value = "JS_ATOM_NULL")]
	public struct Atom : uint32 {
		[CCode (cname = "JS_AtomToCString", instance_pos = 1.1)]
		public unowned string * to_cstring (Context ctx);
	}

	[CCode (has_type_id = false)]
	public struct PropertyEnum {
		public bool is_enumerable;
		public Atom atom;
	}

	[CCode (has_type_id = false)]
	public struct PropertyDescriptor {
		public PropertyFlags flags;
		public Value value;
		public Value getter;
		public Value setter;
	}

	[CCode (cprefix = "JS_GPN_", has_type_id = false)]
	[Flags]
	public enum GetPropertyNamesFlags {
		STRING_MASK,
		SYMBOL_MASK,
		PRIVATE_MASK,
		ENUM_ONLY,
		SET_ENUM,
	}

	[CCode (cname = "int", cprefix = "JS_PROP_", has_type_id = false)]
	[Flags]
	public enum PropertyFlags {
		CONFIGURABLE,
		WRITABLE,
		ENUMERABLE,
		C_W_E,
		HAS_CONFIGURABLE,
		HAS_WRITABLE,
		HAS_ENUMERABLE,
		HAS_GET,
		HAS_SET,
		HAS_VALUE,
		THROW,
		THROW_STRICT,
	}

	[CCode (cname = "JS_UNDEFINED")]
	public Value Undefined;

	[CCode (cname = "JS_NULL")]
	public Value Null;

	[CCode (cname = "JS_EXCEPTION")]
	public Value Exception;

	[CCode (cprefix = "JS_EVAL_TYPE_", has_type_id = false)]
	public enum EvalType {
		GLOBAL,
		MODULE,
		DIRECT,
		INDIRECT,
	}

	[CCode (cprefix = "JS_EVAL_FLAG_", has_type_id = false)]
	[Flags]
	public enum EvalFlag {
		STRICT,
		STRIP,
		COMPILE_ONLY,
		BACKTRACE_BARRIER,
	}

	[Compact]
	public class ModuleDef {
	}
}
