[CCode (cprefix = "android_", gir_namespace = "Android", gir_version = "1.0")]
namespace Android {
	[Compact]
	[CCode (cheader_filename = "dlfcn.h", cname = "void", free_function = "dlclose")]
	public class Module {
		[CCode (cname = "dlopen")]
		public static Module? open (string filename, DlOpenFlags flags);

		[CCode (cheader_filename = "android/dlext.h", cname = "android_dlopen_ext")]
		public static Module? open_ext (string filename, DlOpenFlags flags, DlExtInfo info);

		[CCode (cname = "dlsym")]
		public void * symbol (string name);

		[CCode (cname = "dlerror")]
		public static unowned string? get_last_error ();
	}

	[CCode (cheader_filename = "dlfcn.h", cname = "dlsym")]
	public void * dlsym (void * handle, string symbol);

	[Flags]
	[CCode (cheader_filename = "dlfcn.h", cname = "int", cprefix = "RTLD_", has_type_id = false)]
	public enum DlOpenFlags {
		LOCAL,
		LAZY,
		NOW,
		NOLOAD,
		GLOBAL,
		NODELETE,
	}

	[CCode (cheader_filename = "dlfcn.h", cname = "RTLD_DEFAULT")]
	public void * RTLD_DEFAULT;

	[CCode (cheader_filename = "dlfcn.h", cname = "RTLD_NEXT")]
	public void * RTLD_NEXT;

	[Flags]
	[CCode (cheader_filename = "android/dlext.h", cprefix = "ANDROID_DLEXT_", has_type_id = false)]
	public enum DlExtFlags {
		RESERVED_ADDRESS,
		RESERVED_ADDRESS_HINT,
		WRITE_RELRO,
		USE_RELRO,
		USE_LIBRARY_FD,
		USE_LIBRARY_FD_OFFSET,
		FORCE_LOAD,
		USE_NAMESPACE,
		RESERVED_ADDRESS_RECURSIVE,
	}

	[CCode (cheader_filename = "android/dlext.h", cname = "android_dlextinfo", has_type_id = false)]
	public struct DlExtInfo {
		public DlExtFlags flags;

		public void * reserved_addr;
		public size_t reserved_size;

		public int relro_fd;

		public int library_fd;
		public int64 library_fd_offset;

		public Namespace * library_namespace;
	}

	[CCode (cheader_filename = "android/dlext.h", cname = "struct android_namespace_t", has_type_id = false)]
	public struct Namespace {
	}

	[CCode (cheader_filename = "signal.h", cname = "NSIG")]
	public const int NSIG;
}
