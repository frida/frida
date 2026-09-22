[CCode (cheader_filename = "gum/gumwindows.h")]
namespace Gum.Windows {
	public Gum.CpuType query_native_cpu_type ();
	public Gum.CpuType cpu_type_from_pid (uint pid) throws Gum.Error;
}
