[CCode (cheader_filename = "frida-jni.h", gir_namespace = "Jni", gir_version = "1.0")]
namespace JNI {
	public const int VERSION_1_1;
	public const int VERSION_1_2;
	public const int VERSION_1_4;
	public const int VERSION_1_6;

	[CCode (cname = "jint", cprefix = "JNI_", has_type_id = false)]
	public enum Result {
		OK,
		ERR,
		EDETACHED,
		EVERSION,
		ENOMEM,
		EEXIST,
		EINVAL,
	}

	[CCode (cname = "JavaVMInitArgs", has_destroy_function = false)]
	public struct VMInitArgs {
		public int version;

		[CCode (array_length_cname = "nOptions")]
		public VMOption[] options;

		[CCode (cname = "ignoreUnrecognized")]
		public bool ignore_unrecognized;
	}

	[CCode (cname = "JavaVMOption", has_destroy_function = false)]
	public struct VMOption {
		[CCode (cname = "optionString")]
		public string option_string;

		[CCode (cname = "extraInfo")]
		public void * extra_info;
	}

	[CCode (cname = "JavaVMAttachArgs", has_destroy_function = false)]
	public struct VMAttachArgs {
		public int version;
		public string? name;
		public ObjectRef * group;
	}

	[CCode (cname = "jobject", has_type_id = false)]
	public struct ObjectRef {}

	[CCode (cname = "jclass", has_type_id = false)]
	public struct ClassRef {}

	[CCode (cname = "jstring", has_type_id = false)]
	public struct StringRef {}

	[CCode (cname = "jboolean", has_type_id = false)]
	public enum Boolean {
		FALSE = 0,
		TRUE = 1,
	}

	[CCode (cname = "jthrowable", has_type_id = false)]
	public struct ThrowableRef {}

	[CCode (cname = "jmethodID", has_type_id = false)]
	public struct MethodID {}

	[CCode (cname = "struct JNIInvokeInterface")]
	public class InvokeInterface {
		[CCode (cname = "DestroyJavaVM")]
		public DestroyJavaVMFunc destroy_java_vm;

		[CCode (cname = "AttachCurrentThread")]
		public AttachCurrentThreadFunc attach_current_thread;

		[CCode (cname = "DetachCurrentThread")]
		public DetachCurrentThreadFunc detach_current_thread;

		[CCode (cname = "GetEnv")]
		public GetEnvFunc get_env;

		[CCode (cname = "AttachCurrentThreadAsDaemon")]
		public AttachCurrentThreadFunc attach_current_thread_as_daemon;
	}

	[CCode (has_target = false)]
	public delegate Result DestroyJavaVMFunc (InvokeInterface ** vm);

	[CCode (has_target = false)]
	public delegate Result AttachCurrentThreadFunc (InvokeInterface ** vm, out NativeInterface ** env, VMAttachArgs * args);

	[CCode (has_target = false)]
	public delegate Result DetachCurrentThreadFunc (InvokeInterface ** vm);

	[CCode (has_target = false)]
	public delegate Result GetEnvFunc (InvokeInterface ** vm, out NativeInterface ** env, int version);

	[CCode (cname = "struct JNINativeInterface")]
	public class NativeInterface {
		[CCode (cname = "FindClass")]
		public FindClassFunc find_class;

		[CCode (cname = "ExceptionOccurred")]
		public ExceptionOccurredFunc exception_occurred;

		[CCode (cname = "ExceptionClear")]
		public ExceptionClearFunc exception_clear;

		[CCode (cname = "PushLocalFrame")]
		public PushLocalFrameFunc push_local_frame;

		[CCode (cname = "PopLocalFrame")]
		public PopLocalFrameFunc pop_local_frame;

		[CCode (cname = "NewGlobalRef")]
		public NewGlobalRefFunc new_global_ref;

		[CCode (cname = "DeleteGlobalRef")]
		public DeleteGlobalRefFunc delete_global_ref;

		[CCode (cname = "DeleteLocalRef")]
		public DeleteLocalRefFunc delete_local_ref;

		[CCode (cname = "NewObject")]
		public NewObjectFunc new_object;

		[CCode (cname = "GetObjectClass")]
		public GetObjectClassFunc get_object_class;

		[CCode (cname = "GetMethodID")]
		public GetMethodIDFunc get_method_id;

		[CCode (cname = "CallObjectMethod")]
		public CallObjectMethodFunc call_object_method;

		[CCode (cname = "NewStringUTF")]
		public NewStringUTFFunc new_string_utf;

		[CCode (cname = "GetStringUTFChars")]
		public GetStringUTFCharsFunc get_string_utf_chars;

		[CCode (cname = "ReleaseStringUTFChars")]
		public ReleaseStringUTFCharsFunc release_string_utf_chars;
	}

	[CCode (has_target = false)]
	public delegate ClassRef * FindClassFunc (NativeInterface ** env, string name);

	[CCode (has_target = false)]
	public delegate ThrowableRef * ExceptionOccurredFunc (NativeInterface ** env);

	[CCode (has_target = false)]
	public delegate void ExceptionClearFunc (NativeInterface ** env);

	[CCode (has_target = false)]
	public delegate Result PushLocalFrameFunc (NativeInterface ** env, int capacity);

	[CCode (has_target = false)]
	public delegate ObjectRef * PopLocalFrameFunc (NativeInterface ** env, ObjectRef * result);

	[CCode (has_target = false)]
	public delegate ObjectRef * NewGlobalRefFunc (NativeInterface ** env, ObjectRef * obj);

	[CCode (has_target = false)]
	public delegate void DeleteGlobalRefFunc (NativeInterface ** env, ObjectRef * obj);

	[CCode (has_target = false)]
	public delegate void DeleteLocalRefFunc (NativeInterface ** env, ObjectRef * obj);

	[CCode (has_target = false)]
	public delegate ObjectRef * NewObjectFunc (NativeInterface ** env, ClassRef * klass, MethodID * method, ...);

	[CCode (has_target = false)]
	public delegate ClassRef * GetObjectClassFunc (NativeInterface ** env, ObjectRef * obj);

	[CCode (has_target = false)]
	public delegate MethodID * GetMethodIDFunc (NativeInterface ** env, ClassRef * klass, string name, string sig);

	[CCode (has_target = false)]
	public delegate ObjectRef * CallObjectMethodFunc (NativeInterface ** env, ObjectRef * obj, MethodID * method, ...);

	[CCode (has_target = false)]
	public delegate StringRef * NewStringUTFFunc (NativeInterface ** env, string utf8);

	[CCode (has_target = false)]
	public delegate unowned string GetStringUTFCharsFunc (NativeInterface ** env, StringRef * str, out Boolean is_copy = null);

	[CCode (has_target = false)]
	public delegate void ReleaseStringUTFCharsFunc (NativeInterface ** env, StringRef * str, string chars);
}
