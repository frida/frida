[CCode (cheader_filename = "bpf/libbpf.h", cprefix = "libbpf_", gir_namespace = "Libbpf", gir_version = "1.0")]
namespace Libbpf {
	[Compact]
	[CCode (cname = "struct bpf_object", cprefix = "bpf_object__", free_function = "bpf_object__close")]
	public class Object {
		public static Object open_mem (uint8[] buf, OpenOpts opts);

		[Compact]
		[CCode (cname = "struct bpf_object_open_opts")]
		public struct OpenOpts {
			public size_t sz;
			public unowned string? object_name;
			public bool relaxed_maps;
			public unowned string? pin_root_path;
			public unowned string? kconfig;
			public unowned string? btf_custom_path;
			[CCode (array_length_cname = "kernel_log_size")]
			public unowned char[] kernel_log_buf;
			public uint32 kernel_log_level;
			public unowned string? bpf_token_path;
		}

		public int prepare ();
		public int load ();

		public unowned Program? find_program_by_name (string name);
		public unowned Program? next_program (Program? prog = null);

		public unowned Map? find_map_by_name (string name);
		public int find_map_fd_by_name (string name);
		public unowned Map? next_map (Map? map = null);
	}

	[Compact]
	[CCode (cname = "struct bpf_program", cprefix = "bpf_program__", free_function = "")]
	public class Program {
		public string name {
			[CCode (cname = "bpf_program__name")]
			get;
		}

		public ProgramType ptype {
			[CCode (cname = "bpf_program__type")]
			get;
		}

		public string section_name {
			[CCode (cname = "bpf_program__section_name")]
			get;
		}

		public bool autoload {
			[CCode (cname = "bpf_program__autoload")]
			get;
			set;
		}

		public bool autoattach {
			[CCode (cname = "bpf_program__autoattach")]
			get;
			set;
		}

		public int fd {
			[CCode (cname = "bpf_program__fd")]
			get;
		}

		public Link? attach ();
		public Link? attach_perf_event (int pfd);
	}

	[Compact]
	[CCode (cname = "struct bpf_map", cprefix = "bpf_map__", free_function = "")]
	public class Map {
		public string name {
			[CCode (cname = "bpf_map__name")]
			get;
		}

		public MapType mtype {
			[CCode (cname = "bpf_map__type")]
			get;
		}

		public bool autocreate {
			[CCode (cname = "bpf_map__autocreate")]
			get;
			set;
		}

		public bool autoattach {
			[CCode (cname = "bpf_map__autoattach")]
			get;
			set;
		}

		public int fd {
			[CCode (cname = "bpf_map__fd")]
			get;
		}

		public uint32 max_entries {
			[CCode (cname = "bpf_map__max_entries")]
			get;
			set;
		}

		public uint32 key_size {
			[CCode (cname = "bpf_map__key_size")]
			get;
			set;
		}

		public uint32 value_size {
			[CCode (cname = "bpf_map__value_size")]
			get;
			set;
		}

		public int reuse_fd (int fd);

		public int lookup_elem (uint8[] key, uint8[] val, uint64 flags = 0);
		public int update_elem (uint8[] key, uint8[] val, uint64 flags = 0);
		public int delete_elem (uint8[] key, uint64 flags = 0);
	}

	[Compact]
	[CCode (cname = "struct bpf_link", cprefix = "bpf_link__", free_function = "bpf_link__destroy")]
	public class Link {
		public string fd {
			[CCode (cname = "bpf_link__fd")]
			get;
		}

		public void disconnect ();
		public int detach ();
	}

	[CCode (cname = "enum bpf_prog_type", cprefix = "BPF_PROG_TYPE_", has_type_id = false)]
	public enum ProgramType {
		UNSPEC,
		SOCKET_FILTER,
		KPROBE,
		SCHED_CLS,
		SCHED_ACT,
		TRACEPOINT,
		XDP,
		PERF_EVENT,
		CGROUP_SKB,
		CGROUP_SOCK,
		LWT_IN,
		LWT_OUT,
		LWT_XMIT,
		SOCK_OPS,
		SK_SKB,
		CGROUP_DEVICE,
		SK_MSG,
		RAW_TRACEPOINT,
		CGROUP_SOCK_ADDR,
		LWT_SEG6LOCAL,
		LIRC_MODE2,
		SK_REUSEPORT,
		FLOW_DISSECTOR,
		CGROUP_SYSCTL,
		RAW_TRACEPOINT_WRITABLE,
		CGROUP_SOCKOPT,
		TRACING,
		STRUCT_OPS,
		EXT,
		LSM,
		SK_LOOKUP,
		SYSCALL,
		NETFILTER;

		[CCode (cname = "libbpf_bpf_prog_type_str")]
		public unowned string? to_string ();
	}

	[CCode (cname = "enum bpf_map_type", cprefix = "BPF_MAP_TYPE_", has_type_id = false)]
	public enum MapType {
		UNSPEC,
		HASH,
		ARRAY,
		PROG_ARRAY,
		PERF_EVENT_ARRAY,
		PERCPU_HASH,
		PERCPU_ARRAY,
		STACK_TRACE,
		CGROUP_ARRAY,
		LRU_HASH,
		LRU_PERCPU_HASH,
		LPM_TRIE,
		ARRAY_OF_MAPS,
		HASH_OF_MAPS,
		DEVMAP,
		SOCKMAP,
		CPUMAP,
		XSKMAP,
		SOCKHASH,
		CGROUP_STORAGE_DEPRECATED,
		REUSEPORT_SOCKARRAY,
		PERCPU_CGROUP_STORAGE_DEPRECATED,
		QUEUE,
		STACK,
		SK_STORAGE,
		DEVMAP_HASH,
		STRUCT_OPS,
		RINGBUF,
		INODE_STORAGE,
		TASK_STORAGE,
		BLOOM_FILTER,
		USER_RINGBUF,
		CGRP_STORAGE,
		ARENA,
		INSN_ARRAY;

		[CCode (cname = "libbpf_bpf_map_type_str")]
		public unowned string? to_string ();
	}

	public int num_possible_cpus ();

	public int strerror (int err, char[] buf);

	public PrintFn? set_print (PrintFn? fn);

	[CCode (cname = "libbpf_print_fn_t", has_target = false)]
	public delegate int PrintFn (PrintLevel level, string format, va_list args);

	[CCode (cname = "enum libbpf_print_level", cprefix = "LIBBPF_", has_type_id = false)]
	public enum PrintLevel {
		WARN,
		INFO,
		DEBUG
	}
}
