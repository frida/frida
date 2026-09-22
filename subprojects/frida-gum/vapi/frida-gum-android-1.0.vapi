[CCode (cheader_filename = "gum/gumandroid.h")]
namespace Gum.Android {
	public LinkerFlavor get_linker_flavor ();

	[CCode (cprefix = "GUM_ANDROID_LINKER_", has_type_id = false)]
	public enum LinkerFlavor {
		NATIVE,
		EMULATED
	}

	public uint get_api_level ();

	public bool is_linker_module_name (string name);
	public unowned Gum.Module? get_linker_module ();
	[CCode (array_length = false, array_null_terminated = true)]
	public unowned string[] get_magic_linker_export_names ();
	public bool try_resolve_magic_export (string module_name, string symbol_name, out Gum.Address result);

	public void enumerate_modules (Gum.FoundModuleFunc func);

	public bool find_unrestricted_dlopen (out GenericDlopenImpl generic_dlopen);
	public bool find_unrestricted_dlsym (out GenericDlsymImpl generic_dlsym);
	public bool find_unrestricted_linker_api (out UnrestrictedLinkerApi api);

	[CCode (cname = "GumGenericDlopenImpl", has_target = false)]
	public delegate void * GenericDlopenImpl (string filename, int flags);
	[CCode (cname = "GumGenericDlsymImpl", has_target = false)]
	public delegate void * GenericDlsymImpl (void * handle, string symbol);

	public struct UnrestrictedLinkerApi {
		public DlopenImpl dlopen;
		public DlsymImpl dlsym;
	}

	[CCode (has_target = false)]
	public delegate void * DlopenImpl (string filename, int flags, void * caller_addr);
	[CCode (has_target = false)]
	public delegate void * DlsymImpl (void * handle, string symbol, string? version, void * caller_addr);
}
