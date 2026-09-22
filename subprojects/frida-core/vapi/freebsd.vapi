[CCode (lower_case_cprefix = "", gir_namespace = "FreeBSD", gir_version = "1.0")]
namespace FreeBSD {
	[CCode (cheader_filename = "libutil.h")]
	public int openpty (out int amaster, out int aslave, [CCode (array_length=false, array_null_terminated=true)] char[] name,
		Posix.termios? termp, winsize? winp);

	[CCode (cname = "struct winsize", has_type_id = false, cheader_filename = "sys/ttycom.h", destroy_function = "")]
	public struct winsize {
		public ushort ws_row;
		public ushort ws_col;
		public ushort ws_xpixel;
		public ushort ws_ypixel;
	}
}
