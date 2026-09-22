[CCode (lower_case_cprefix = "", gir_namespace = "Darwin", gir_version = "1.0")]
namespace Darwin.XNU {
	[CCode (cname = "kern_return_t", cheader_filename = "mach/mach_error.h", cprefix = "KERN_", has_type_id = false)]
	public enum KernReturn {
		SUCCESS,
		INVALID_ADDRESS,
		PROTECTION_FAILURE,
		NO_SPACE,
		INVALID_ARGUMENT,
		FAILURE,
		RESOURCE_SHORTAGE,
		NOT_RECEIVER,
		NO_ACCESS,
		MEMORY_FAILURE,
		MEMORY_ERROR,
		ALREADY_IN_SET,
		NOT_IN_SET,
		NAME_EXISTS,
		ABORTED,
		INVALID_NAME,
		INVALID_TASK,
		INVALID_RIGHT,
		INVALID_VALUE,
		UREFS_OVERFLOW,
		INVALID_CAPABILITY,
		RIGHT_EXISTS,
		INVALID_HOST,
		MEMORY_PRESENT,
		MEMORY_DATA_MOVED,
		MEMORY_RESTART_COPY,
		INVALID_PROCESSOR_SET,
		POLICY_LIMIT,
		INVALID_POLICY,
		INVALID_OBJECT,
		ALREADY_WAITING,
		DEFAULT_SET,
		EXCEPTION_PROTECTED,
		INVALID_LEDGER,
		INVALID_MEMORY_CONTROL,
		INVALID_SECURITY,
		NOT_DEPRESSED,
		TERMINATED,
		LOCK_SET_DESTROYED,
		LOCK_UNSTABLE,
		LOCK_OWNED,
		LOCK_OWNED_SELF,
		SEMAPHORE_DESTROYED,
		RPC_SERVER_TERMINATED,
		RPC_TERMINATE_ORPHAN,
		RPC_CONTINUE_ORPHAN,
		NOT_SUPPORTED,
		NODE_DOWN,
		NOT_WAITING,
		OPERATION_TIMED_OUT,
		CODESIGN_ERROR,
		POLICY_STATIC,
		INSUFFICIENT_BUFFER_SIZE,
		DENIED,
		MISSING_KC,
		INVALID_KC,
		NOT_FOUND,
		RETURN_MAX,
	}

	[CCode (cheader_filename = "mach/mach_error.h")]
	public static unowned string mach_error_string (KernReturn kr);

	[CCode (cname = "mach_port_t", has_type_id = false)]
	public struct MachPort : uint {
		[CCode (cname = "MACH_PORT_NULL")]
		public const MachPort NULL;
	}

	[CCode (cheader_filename = "libproc.h")]
	public int proc_pidpath (int pid, char[] buffer);

	[CCode (cheader_filename = "sys/sysctl.h")]
	public int sysctlbyname (string name, void * oldp, size_t * oldlenp, void * newp = null, size_t newlen = 0);

	[CCode (cheader_filename = "util.h")]
	public int openpty (out int amaster, out int aslave, [CCode (array_length=false, array_null_terminated=true)] char[] name,
		Posix.termios? termp, winsize? winp);

	[CCode (cname = "struct winsize", has_type_id = false, cheader_filename = "sys/ttycom.h", destroy_function = "")]
	public struct winsize {
		public ushort ws_row;
		public ushort ws_col;
		public ushort ws_xpixel;
		public ushort ws_ypixel;
	}

	[CCode (cname = "struct xinpgen", cheader_filename = "netinet/in_pcb.h")]
	public struct InetPcbGeneration {
		[CCode (cname = "xig_len")]
		public int32 length;
		[CCode (cname = "xig_count")]
		public uint count;
		[CCode (cname = "xig_gen")]
		public uint64 generation_count;
		[CCode (cname = "xig_sogen")]
		public uint64 socket_generation_count;
	}

	[CCode (cname = "struct in_addr_4in6", cheader_filename = "netinet/in_pcb.h")]
	public struct InetAddr4in6 {
		[CCode (cname = "ia46_addr4")]
		public Posix.InAddr addr4;
	}

	[CCode (cprefix = "INP_", cheader_filename = "netinet/in_pcb.h")]
	public enum InetVersionFlags {
		IPV4,
		IPV6,
		V4MAPPEDV6,
	}

	[CCode (cheader_filename = "fcntl.h")]
	public const int F_FULLFSYNC;
	[CCode (cheader_filename = "fcntl.h")]
	public const int F_SETNOSIGPIPE;
}
