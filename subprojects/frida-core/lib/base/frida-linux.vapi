namespace Frida {
	[CCode (cheader_filename = "dlfcn.h", cname = "dlopen")]
	public void * dlopen (string filename, int flags);

	[CCode (cheader_filename = "dlfcn.h", cname = "dlclose")]
	public int dlclose (void * handle);

	[CCode (cheader_filename = "dlfcn.h", cname = "dlsym")]
	public void * dlsym (void * handle, string symbol);

	[CCode (cheader_filename = "dlfcn.h", cname = "dlerror")]
	public unowned string dlerror ();

	[CCode (cheader_filename = "sys/mman.h", cname = "MAP_ANONYMOUS")]
	public const int MAP_ANONYMOUS;

	[CCode (cheader_filename = "frida-linux-bpf.h", cprefix = "FRIDA_BPF_RINGBUF_", has_type_id = false)]
	[Flags]
	public enum BpfRingbufFlags {
		BUSY,
		DISCARD,
	}

	[CCode (cheader_filename = "frida-linux-bpf.h")]
	public const uint32 BPF_RINGBUF_HEADER_SIZE;

	[CCode (cheader_filename = "frida-linux-perf-event.h")]
	public const int PERF_EVENT_TYPE_SOFTWARE;

	[CCode (cheader_filename = "frida-linux-perf-event.h")]
	public const int PERF_EVENT_COUNT_SW_CPU_CLOCK;

	[CCode (cheader_filename = "frida-linux-perf-event.h", has_type_id = false)]
	public struct PerfEventAttr {
		public PerfEventType event_type;
		public uint32 size;
		public uint64 config;

		public uint64 sample_period;

		public uint64 sample_type;
		public uint64 read_format;

		public uint64 flags;

		public uint32 wakeup_events;
		public uint32 bp_type;

		public uint64 config1;
		public uint64 config2;
	}

	[CCode (cheader_filename = "frida-linux-perf-event.h", has_type_id = false)]
	public enum PerfEventType {
		HARDWARE,
		SOFTWARE,
	}
}
