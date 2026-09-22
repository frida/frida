namespace Frida {
	private uint linux_major = 0;
	private uint linux_minor = 0;

	public bool check_kernel_version (uint major, uint minor) {
		if (linux_major == 0) {
			var name = Posix.utsname ();
			name.release.scanf ("%u.%u", out linux_major, out linux_minor);
		}

		return (linux_major == major && linux_minor >= minor) || linux_major > major;
	}

	public struct DevId {
		public uint32 major;
		public uint32 minor;

		public DevId (uint32 major, uint32 minor) {
			this.major = major;
			this.minor = minor;
		}

		public uint64 pack () {
			return ((uint64) major << 32) | minor;
		}

		public static DevId unpack (uint64 dev) {
			return DevId ((uint32) (dev >> 32), (uint32) dev);
		}

		public bool equals (DevId other) {
			return major == other.major && minor == other.minor;
		}

		public uint hash () {
			return (uint) (major ^ (minor * 0x9e3779b9));
		}

		public string to_string () {
			return "%x:%x".printf (major, minor);
		}
	}

	public extern unowned LinuxSyscallSignature[] get_syscall_signatures ();

	public extern unowned LinuxSyscallSignature[]? get_compat32_syscall_signatures ();

	public struct LinuxSyscallSignature {
		public int nr;
		public string name;
		public uint8 nargs;
		public LinuxSyscallArg args[6];
	}

	public struct LinuxSyscallArg {
		public string? type;
		public string? name;
	}

	public sealed class PidFileDescriptor : FileDescriptor {
		private uint pid;

		private PidFileDescriptor (int fd, uint pid) {
			base (fd);
			this.pid = pid;
		}

		public static bool is_supported () {
			return check_kernel_version (5, 3);
		}

		public static bool getfd_is_supported () {
			return check_kernel_version (5, 6);
		}

		public static PidFileDescriptor from_pid (uint pid) throws Error {
			int fd = pidfd_open (pid, 0);
			if (fd == -1)
				throw_pidfd_error (pid, errno);
			return new PidFileDescriptor (fd, pid);
		}

		public FileDescriptor getfd (int targetfd) throws Error {
			int fd = pidfd_getfd (handle, targetfd, 0);
			if (fd == -1)
				throw_pidfd_error (pid, errno);
			return new FileDescriptor (fd);
		}

		private static int pidfd_open (uint pid, uint flags) {
			return Linux.syscall (LinuxSyscall.PIDFD_OPEN, pid, flags);
		}

		private static int pidfd_getfd (int pidfd, int targetfd, uint flags) {
			return Linux.syscall (LinuxSyscall.PIDFD_GETFD, pidfd, targetfd, flags);
		}

		[NoReturn]
		private static void throw_pidfd_error (uint pid, int err) throws Error {
			switch (err) {
				case Posix.ESRCH:
					throw new Error.PROCESS_NOT_FOUND ("Process not found");
				case Posix.EPERM:
					throw new Error.PERMISSION_DENIED ("Unable to use pidfd for pid %u: %s", pid, strerror (err));
				default:
					throw new Error.NOT_SUPPORTED ("Unable to use pidfd for pid %u: %s", pid, strerror (err));
			}
		}
	}

	namespace MemoryFileDescriptor {
		public bool is_supported () {
			return check_kernel_version (3, 17);
		}

		public static FileDescriptor from_bytes (string name, Bytes bytes) {
			assert (is_supported ());

			var fd = new FileDescriptor (memfd_create (name, 0));
			unowned uint8[] data = bytes.get_data ();
			ssize_t n = Posix.write (fd.handle, data, data.length);
			assert (n == data.length);
			return fd;
		}

		private int memfd_create (string name, uint flags) {
			return Linux.syscall (LinuxSyscall.MEMFD_CREATE, name, flags);
		}
	}

#if HAVE_LOCAL_BACKEND
	public sealed class BpfObject {
		public Programs programs {
			get;
		}

		public Maps maps {
			get;
		}

		internal Libbpf.Object handle;

		public static BpfObject open (string name, uint8[] blob) throws Error {
			var opts = Libbpf.Object.OpenOpts ();
			opts.sz = sizeof (Libbpf.Object.OpenOpts);
			opts.object_name = name;

			var log = new LibbpfLogScope ();
			var handle = Libbpf.Object.open_mem (blob, opts);
			if (handle == null)
				throw_libbpf_error (errno, log.take ());

			return new BpfObject ((owned) handle);
		}

		private BpfObject (owned Libbpf.Object handle) {
			this.handle = (owned) handle;

			_programs = new Programs (this);
			_maps = new Maps (this);
		}

		public void prepare () throws Error {
			var log = new LibbpfLogScope ();
			check_libbpf_result (handle.prepare (), log.take ());
		}

		public void load () throws Error {
			var log = new LibbpfLogScope ();
			check_libbpf_result (handle.load (), log.take ());
		}

		public sealed class Programs {
			private unowned BpfObject parent;

			internal Programs (BpfObject parent) {
				this.parent = parent;
			}

			public BpfProgram get_by_name (string name) throws Error {
				var p = find_by_name (name);
				if (p == null)
					throw new Error.INVALID_ARGUMENT ("No program named '%s'", name);
				return p;
			}

			public BpfProgram? find_by_name (string name) {
				unowned Libbpf.Program? p = parent.handle.find_program_by_name (name);
				if (p == null)
					return null;
				return new BpfProgram (p, parent);
			}

			public Iterator iterator () {
				return new Iterator (parent);
			}

			public class Iterator {
				private BpfObject parent;
				private BpfProgram? current;

				internal Iterator (BpfObject parent) {
					this.parent = parent;
				}

				public BpfProgram? next_value () {
					unowned Libbpf.Program? p = parent.handle.next_program (current?.handle);
					if (p == null)
						return null;
					current = new BpfProgram (p, parent);
					return current;
				}
			}
		}

		public sealed class Maps {
			private unowned BpfObject parent;

			internal Maps (BpfObject parent) {
				this.parent = parent;
			}

			public BpfMap get_by_name (string name) throws Error {
				var p = find_by_name (name);
				if (p == null)
					throw new Error.INVALID_ARGUMENT ("No map named '%s'", name);
				return p;
			}

			public BpfMap? find_by_name (string name) {
				unowned Libbpf.Map? p = parent.handle.find_map_by_name (name);
				if (p == null)
					return null;
				return new BpfMap (p, parent);
			}

			public Iterator iterator () {
				return new Iterator (parent);
			}

			public class Iterator {
				private BpfObject parent;
				private BpfMap? current;

				internal Iterator (BpfObject parent) {
					this.parent = parent;
				}

				public BpfMap? next_value () {
					unowned Libbpf.Map? p = parent.handle.next_map (current?.handle);
					if (p == null)
						return null;
					current = new BpfMap (p, parent);
					return current;
				}
			}
		}
	}

	public sealed class BpfProgram {
		internal unowned Libbpf.Program handle;
		private BpfObject parent;

		public string name {
			get {
				return handle.name;
			}
		}

		internal BpfProgram (Libbpf.Program handle, BpfObject parent) {
			this.handle = handle;
			this.parent = parent;
		}

		public BpfLink attach () throws Error {
			return parse_attach_result (handle.attach ());
		}

		public BpfLink attach_perf_event (FileDescriptor pfd) throws Error {
			return parse_attach_result (handle.attach_perf_event (pfd.handle));
		}

		private BpfLink parse_attach_result (owned Libbpf.Link? l) throws Error {
			if (l == null)
				throw_libbpf_error (errno);
			return new BpfLink ((owned) l);
		}
	}

	public sealed class BpfMap {
		internal unowned Libbpf.Map handle;
		private BpfObject parent;

		public string name {
			get {
				return handle.name;
			}
		}

		public int fd {
			get {
				return handle.fd;
			}
		}

		public uint32 max_entries {
			get {
				return handle.max_entries;
			}
			set {
				handle.max_entries = value;
			}
		}

		public uint32 key_size {
			get {
				return handle.key_size;
			}
			set {
				handle.key_size = value;
			}
		}

		public uint32 value_size {
			get {
				return handle.value_size;
			}
			set {
				handle.value_size = value;
			}
		}

		internal BpfMap (Libbpf.Map handle, BpfObject parent) {
			this.handle = handle;
			this.parent = parent;
		}

		public void update_u32_u8 (uint32 key, uint8 val) throws Error {
			update_raw ((uint8[]) &key, (uint8[]) &val);
		}

		public void update_u32_u32 (uint32 key, uint32 val) throws Error {
			update_raw ((uint8[]) &key, (uint8[]) &val);
		}

		public void remove_u32 (uint32 key) throws Error {
			remove_raw ((uint8[]) &key);
		}

		public void update_u64_u8 (uint64 key, uint8 val) throws Error {
			update_raw ((uint8[]) &key, (uint8[]) &val);
		}

		public void update_u64_u32 (uint64 key, uint32 val) throws Error {
			update_raw ((uint8[]) &key, (uint8[]) &val);
		}

		public void remove_u64 (uint64 key) throws Error {
			remove_raw ((uint8[]) &key);
		}

		public void foreach_percpu_value<T> (uint8[] key, PercpuValueFunc<T> func) throws Error {
			var ncpus = check_libbpf_result (Libbpf.num_possible_cpus ());
			var percpu_stride = round_up_8 (value_size);

			uint8[] buf = new uint8[ncpus * percpu_stride];
			lookup_raw (key, buf);

			for (int cpu = 0; cpu != ncpus; cpu++) {
				unowned T val = (T) ((uint8 *) buf + (cpu * percpu_stride));
				func (cpu, val);
			}
		}

		public delegate void PercpuValueFunc<T> (uint32 cpu, T val);

		public void lookup_raw (uint8[] key, uint8[] val) throws Error {
			check_libbpf_result (handle.lookup_elem (key, val));
		}

		public void update_raw (uint8[] key, uint8[] val) throws Error {
			check_libbpf_result (handle.update_elem (key, val));
		}

		public void remove_raw (uint8[] key) throws Error {
			check_libbpf_result (handle.delete_elem (key));
		}
	}

	public sealed class BpfLink {
		private Libbpf.Link handle;

		internal BpfLink (owned Libbpf.Link handle) {
			this.handle = (owned) handle;
		}
	}

	private sealed class LibbpfLogScope {
		private static StringBuilder buffer;

		public LibbpfLogScope () {
			if (buffer == null)
				buffer = new StringBuilder ();
			buffer.truncate ();
			Libbpf.set_print (on_message);
		}

		~LibbpfLogScope () {
			Libbpf.set_print (null);
		}

		public string take () {
			return buffer.str.chomp ();
		}

		private static int on_message (Libbpf.PrintLevel level, string format, va_list args) {
			buffer.append_vprintf (format, args);
			return 0;
		}
	}

	private int check_libbpf_result (int result, string details = "") throws Error {
		if (result < 0)
			throw_libbpf_error (result, details);
		return result;
	}

	[NoReturn]
	private void throw_libbpf_error (int err, string details = "") throws Error {
		var message = new StringBuilder (error_message_for_libbpf_error (err));
		if (details.length != 0) {
			message
				.append (":\n")
				.append (details.chomp ());
		}

		if (err.abs () == Posix.EPERM)
			throw new Error.PERMISSION_DENIED ("%s", message.str);
		else
			throw new Error.INVALID_ARGUMENT ("%s", message.str);
	}

	private string error_message_for_libbpf_error (int err) {
		char message[256];
		int result = Libbpf.strerror (err, message);
		assert (result == 0);
		return (string) message;
	}

	public sealed class BpfRingbufReader : Object {
		private BpfMap map;

		private uint8 * consumer_map;
		private uint8 * producer_map;

		private uint8 * producer_page;
		private uint8 * data_base;

		private uint64 * consumer_pos;
		private uint64 * producer_pos;

		public BpfRingbufReader (BpfMap map) throws Error {
			this.map = map;

			var page_size = (size_t) Posix.getpagesize ();
			int fd = map.fd;

			void * mem = Posix.mmap (null, page_size, Posix.PROT_READ | Posix.PROT_WRITE, Posix.MAP_SHARED, fd, 0);
			if (mem == Posix.MAP_FAILED)
				throw_errno ("mmap(consumer) failed");
			consumer_map = mem;

			size_t prod_len = page_size + (2 * map.max_entries);
			mem = Posix.mmap (null, prod_len, Posix.PROT_READ, Posix.MAP_SHARED, fd, (Posix.off_t) page_size);
			if (mem == Posix.MAP_FAILED)
				throw_errno ("mmap(producer+data) failed");
			producer_map = mem;

			producer_page = producer_map;
			data_base = producer_map + page_size;

			consumer_pos = consumer_map;
			producer_pos = producer_page;
		}

		~BpfRingbufReader () {
			var page_size = (size_t) Posix.getpagesize ();

			if (consumer_map != null)
				Posix.munmap (consumer_map, page_size);

			if (producer_map != null)
				Posix.munmap (producer_map, page_size + (2 * map.max_entries));
		}

		public DrainStatus drain (RecordHandler on_record) {
			while (true) {
				uint64 prod = Atomics.load_u64_acquire (producer_pos);
				uint64 cons = Atomics.load_u64_acquire (consumer_pos);

				if (cons >= prod)
					return DRAINED;

				size_t data_size = map.max_entries;
				uint64 mask = data_size - 1;
				uint64 off = cons & mask;
				uint8 * hdrp = data_base + off;

				uint32 hdr_len = Atomics.load_u32_acquire ((uint32 *) hdrp);
				uint32 flags_mask = BpfRingbufFlags.BUSY | BpfRingbufFlags.DISCARD;
				uint32 flags = hdr_len & flags_mask;
				uint32 sample_len = hdr_len & ~flags_mask;

				if ((flags & BpfRingbufFlags.BUSY) != 0)
					return DRAINED;

				assert (sample_len <= data_size - BPF_RINGBUF_HEADER_SIZE);

				var total_len = (uint32) round_up_8 (sample_len + BPF_RINGBUF_HEADER_SIZE);

				if ((flags & BpfRingbufFlags.DISCARD) != 0) {
					Atomics.store_u64_release (consumer_pos, cons + total_len);
					continue;
				}

				unowned uint8[] payload = (uint8[]) (hdrp + BPF_RINGBUF_HEADER_SIZE);
				var action = on_record (payload[:sample_len]);

				Atomics.store_u64_release (consumer_pos, cons + total_len);

				if (action == STOP)
					return STOPPED;
			}
		}

		public enum DrainStatus {
			DRAINED,
			STOPPED,
		}

		public delegate RecordAction RecordHandler (uint8[] payload);

		public enum RecordAction {
			CONTINUE,
			STOP,
		}
	}
#endif

	namespace PerfEvent {
		public FileDescriptor open (PerfEventAttr * attr, int pid, int cpu, int group_fd, uint flags) throws Error {
			int r = Linux.syscall (LinuxSyscall.PERF_EVENT_OPEN, attr, pid, cpu, group_fd, flags);
			if (r == -1)
				throw_errno ("perf_event_open() failed");
			return new FileDescriptor (r);
		}
	}

	private void throw_errno (string message) throws Error {
		throw new Error.NOT_SUPPORTED ("%s (errno=%d: %s)".printf (message, Posix.errno, Posix.strerror (Posix.errno)));
	}

#if HAVE_LOCAL_BACKEND
	private size_t round_up_8 (size_t x) {
		return (x + 7) & ~((size_t) 7);
	}
#endif
}
