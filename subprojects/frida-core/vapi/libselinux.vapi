[CCode (cprefix = "", lower_case_cprefix = "", cheader_filename = "selinux/selinux.h")]
namespace SELinux {
	public int setfilecon (string path, string con);
	public int fsetfilecon (int fd, string con);
}
