namespace Frida {
	// Bridges to the Frida kernel module over its magic-prctl control channel. The module
	// exposes ptrace-free memory and thread primitives against an arbitrary pid, which the
	// kernel-assisted injector drives instead of ptrace or /proc/<pid>/mem.
	internal sealed class KernelSession : Object {
		public uint pid {
			get;
			construct;
		}

		private const long CONTROL_MAGIC = 0x46524944;
		private const uint64 CONTROL_TOKEN = 0x1d5f9e6b2c7a4038;

		private enum Op {
			PING = 32,
			ALLOC = 64,
			FREE,
			WRITE,
			READ,
			SPAWN,
			CLOAK_THREAD,
			CLOAK_RANGE,
		}

		private struct AllocArgs {
			uint64 size;
			uint32 pid;
			uint32 prot;
		}

		private struct FreeArgs {
			uint64 address;
			uint64 size;
			uint32 pid;
		}

		private struct XferArgs {
			uint64 address;
			uint64 buffer;
			uint64 size;
			uint32 pid;
		}

		private struct SpawnArgs {
			uint64 entry;
			uint64 stack;
			uint64 arg;
			uint64 tls;
			uint32 pid;
		}

		private struct CloakThreadArgs {
			uint32 tgid;
			uint32 tid;
		}

		private struct CloakRangeArgs {
			uint64 start;
			uint64 size;
			uint32 tgid;
		}

		private KernelSession (uint pid) {
			Object (pid: pid);
		}

		public static bool is_available () {
			return control (Op.PING, 0, 0) == CONTROL_MAGIC;
		}

		public static KernelSession open (uint pid) throws Error {
			if (!is_available ())
				throw new Error.NOT_SUPPORTED ("Frida kernel module is not loaded");
			return new KernelSession (pid);
		}

		public uint64 alloc (uint64 size, int prot) throws Error {
			var args = AllocArgs () {
				size = size,
				pid = pid,
				prot = prot,
			};
			long result = control (Op.ALLOC, (ulong) (uintptr) (&args), 0);
			if (result <= 0)
				throw new Error.NOT_SUPPORTED ("Unable to allocate %s bytes in the target", size.to_string ());
			return (uint64) result;
		}

		public void free (uint64 address, uint64 size) throws Error {
			var args = FreeArgs () {
				address = address,
				size = size,
				pid = pid,
			};
			control (Op.FREE, (ulong) (uintptr) (&args), 0);
		}

		public void write_memory (uint64 address, uint8[] data) throws Error {
			var args = XferArgs () {
				address = address,
				buffer = (uint64) (uintptr) (uint8 *) data,
				size = data.length,
				pid = pid,
			};
			long result = control (Op.WRITE, (ulong) (uintptr) (&args), 0);
			if (result < 0 || (uint64) result != data.length)
				throw new Error.NOT_SUPPORTED ("Unable to write %d bytes at 0x%" + uint64.FORMAT_MODIFIER + "x",
					data.length, address);
		}

		public uint8[] read_memory (uint64 address, size_t size) throws Error {
			var result = new uint8[size];
			var args = XferArgs () {
				address = address,
				buffer = (uint64) (uintptr) (uint8 *) result,
				size = size,
				pid = pid,
			};
			long n = control (Op.READ, (ulong) (uintptr) (&args), 0);
			if (n < 0 || (size_t) n != size)
				throw new Error.NOT_SUPPORTED ("Unable to read %s bytes at 0x%" + uint64.FORMAT_MODIFIER + "x",
					size.to_string (), address);
			return result;
		}

		public uint64 read_pointer (uint64 address) throws Error {
			uint8[] raw = read_memory (address, sizeof (uint64));
			return *((uint64 *) raw);
		}

		// entry/stack/arg/tls seed the new thread's PC/SP/x0 and TPIDR_EL0. The thread joins
		// the target's thread group; tls must point at a private block the caller prepared.
		public uint spawn_thread (uint64 entry, uint64 stack, uint64 arg, uint64 tls) throws Error {
			var args = SpawnArgs () {
				entry = entry,
				stack = stack,
				arg = arg,
				tls = tls,
				pid = pid,
			};
			long result = control (Op.SPAWN, (ulong) (uintptr) (&args), 0);
			if (result <= 0)
				throw new Error.NOT_SUPPORTED ("Unable to spawn a thread in the target");
			return (uint) result;
		}

		// Hide the loader's own footprint (which Gum doesn't track and so wouldn't self-cloak):
		// the bootstrap thread plus the region/stack/tls we mapped for it.
		public void cloak_thread (uint tid) {
			var args = CloakThreadArgs () { tgid = pid, tid = tid };
			control (Op.CLOAK_THREAD, (ulong) (uintptr) (&args), 0);
		}

		public void cloak_range (uint64 address, uint64 size) {
			var args = CloakRangeArgs () { start = address, size = size, tgid = pid };
			control (Op.CLOAK_RANGE, (ulong) (uintptr) (&args), 0);
		}

		// prctl (FRIDA_CONTROL_MAGIC, op, arg2, arg3, FRIDA_CONTROL_TOKEN). Absent the token
		// the module falls through to the real prctl, so probing the magic looks like a stock
		// kernel. We bind syscall() with a 64-bit return since the libc prctl() wrapper — and
		// the vapi's syscall() — would truncate an allocated address.
		private static long control (Op op, ulong arg2, ulong arg3) {
			return syscall ((long) LinuxSyscall.PRCTL, CONTROL_MAGIC, (ulong) op, arg2, arg3, CONTROL_TOKEN);
		}

		[CCode (cname = "syscall", cheader_filename = "unistd.h,sys/syscall.h")]
		private extern static long syscall (long number, ...);
	}

	// Injects frida-agent.so using the kernel module's ptrace-free primitives: stage the loader +
	// context in a fresh region, then spawn a bootstrap thread straight into it. The loader hands
	// off to a real pthread it creates, so the only libc call on the kernel-spawned thread is
	// pthread_create — which we give a private, isolated bionic TLS so it never touches the
	// target's live per-thread state.
	internal sealed class KernelInjectSession : Object {
		public uint pid {
			get;
			construct;
		}

		private KernelSession kernel;

		private const int PROT_RW = 3;
		private const int PROT_RWX = 7;
		private const uint64 STACK_SIZE = 128 * 1024;
		private const uint64 TLS_SIZE = 8 * 1024;
		private const uint SETTLE_TIMEOUT_SECONDS = 10;

		private struct RegionLayout {
			public size_t entry_offset;
			public size_t libc_offset;
			public size_t entrypoint_offset;
			public size_t data_offset;
			public size_t fallback_offset;
			public size_t context_offset;
			public size_t page_size;
			public size_t total;

			public static RegionLayout compute (InjectSpec spec, string data, string fallback_address) {
				size_t loader_size = Frida.Data.HelperBackend.get_loader_bin_blob ().data.length;
				size_t entrypoint_size = make_cstring (spec.entrypoint).length;
				size_t data_size = make_cstring (data).length;
				size_t fallback_size = make_cstring (fallback_address).length;
				size_t page_size = Gum.query_page_size ();

				var l = RegionLayout ();
				l.entry_offset = (size_t) align_up (loader_size, 16);
				size_t o = (size_t) align_up (l.entry_offset + 256, 16);
				l.libc_offset = o; o += sizeof (HelperLibcApi);
				l.entrypoint_offset = o; o += entrypoint_size;
				l.data_offset = o; o += data_size;
				l.fallback_offset = o; o += fallback_size;
				l.context_offset = (size_t) align_up (o, page_size);
				l.page_size = page_size;
				l.total = l.context_offset + page_size;
				return l;
			}
		}

		private KernelInjectSession (uint pid, KernelSession kernel) {
			Object (pid: pid);
			this.kernel = kernel;
		}

		public static bool is_available () {
			return KernelSession.is_available ();
		}

		public static async KernelInjectSession open (uint pid, Cancellable? cancellable) throws Error, IOError {
			return new KernelInjectSession (pid, KernelSession.open (pid));
		}

		public async RemoteAgent inject (InjectSpec spec, Cancellable? cancellable) throws Error, IOError {
			var maps = ProcMapsSnapshot.from_pid (pid);
			var libc = RemoteLibcApi.resolve (maps);

			string data = spec.data;

			string fallback_address = make_fallback_address ();
			var region = RegionLayout.compute (spec, data, fallback_address);

			Future<RemoteAgent> future_agent = establish_connection (spec, fallback_address, cancellable);

			uint64 region_base = kernel.alloc (region.total, PROT_RWX);
			uint64 stack_base = kernel.alloc (STACK_SIZE, PROT_RW);
			uint64 tls_base;
			uint64 tls = synthesize_tls (out tls_base);

			write_region (region_base, region, spec, data, fallback_address, libc);

			kernel.cloak_range (region_base, region.total);
			kernel.cloak_range (stack_base, STACK_SIZE);
			kernel.cloak_range (tls_base, TLS_SIZE);

			uint64 entry = region_base + region.entry_offset;
			uint64 context = region_base + region.context_offset;
			uint64 stack_top = (stack_base + STACK_SIZE - 16) & ~((uint64) 15);

			uint tid = kernel.spawn_thread (entry, stack_top, context, tls);
			kernel.cloak_thread (tid);

			return yield await_agent (future_agent, cancellable);
		}

		private void write_region (uint64 region_base, RegionLayout l, InjectSpec spec, string data,
				string fallback_address, RemoteLibcApi libc) throws Error {
			kernel.write_memory (region_base, Frida.Data.HelperBackend.get_loader_bin_blob ().data);
			kernel.write_memory (region_base + l.entry_offset, build_bootstrap_code (region_base, l));

			var ctx = HelperLoaderContext ();
			ctx.ctrlfds[0] = -1;
			ctx.ctrlfds[1] = -1;
			ctx.agent_entrypoint = (string *) (region_base + l.entrypoint_offset);
			ctx.agent_data = (string *) (region_base + l.data_offset);
			ctx.fallback_address = (string *) (region_base + l.fallback_offset);
			ctx.libc = (HelperLibcApi *) (region_base + l.libc_offset);

			kernel.write_memory (region_base + l.context_offset, (uint8[]) &ctx);
			kernel.write_memory (region_base + l.libc_offset, (uint8[]) &libc.table);
			kernel.write_memory (region_base + l.entrypoint_offset, make_cstring (spec.entrypoint));
			kernel.write_memory (region_base + l.data_offset, make_cstring (data));
			kernel.write_memory (region_base + l.fallback_offset, make_cstring (fallback_address));
		}

		private uint8[] build_bootstrap_code (uint64 region_base, RegionLayout l) throws Error {
#if ARM64
			var buffer = new uint8[64];
			var writer = new Gum.Arm64Writer ((void *) buffer);
			writer.pc = region_base + l.entry_offset;
			// x0 already holds the loader context (passed as the spawn argument). Call frida_load,
			// then terminate this bootstrap thread; the agent runs on the pthread the loader spawned.
			if (!writer.put_bl_imm ((Gum.Address) region_base))
				throw new Error.NOT_SUPPORTED ("Loader is out of branch range of the bootstrap");
			writer.put_instruction ((uint32) 0xd2800ba8); // movz x8, #93 (__NR_exit)
			writer.put_instruction ((uint32) 0xd4000001); // svc #0
			writer.flush ();
			return buffer[:writer.offset ()];
#else
			throw new Error.NOT_SUPPORTED ("Kernel-assisted injection is only implemented for arm64");
#endif
		}

		// Give the bootstrap thread a private, isolated bionic TLS so pthread_create's stack-guard
		// check and errno access land in our own block, never the target's live per-thread state.
		// Slot indices per bionic tls_defines.h (arm64): BIONIC_TLS=-1, THREAD_ID=1, STACK_GUARD=5,
		// each at tp + index * 8. The guard check is self-consistent within our block, so any stable
		// value passes and we never read the target's real canary.
		private uint64 synthesize_tls (out uint64 block_base) throws Error {
			uint64 block = kernel.alloc (TLS_SIZE, PROT_RW);
			block_base = block;
			uint64 tp = block + (TLS_SIZE / 2);
			uint64 pthread_scratch = block + (TLS_SIZE / 4);
			uint64 bionic_scratch = block + (3 * TLS_SIZE / 4);

			write_slot (tp, -1, bionic_scratch);
			write_slot (tp, 1, pthread_scratch);
			write_slot (tp, 5, 0x1122334455667788);

			return tp;
		}

		private void write_slot (uint64 tp, int index, uint64 value) throws Error {
			uint8[] raw = new uint8[8];
			*((uint64 *) raw) = value;
			kernel.write_memory ((uint64) ((int64) tp + (int64) index * 8), raw);
		}

		private Future<RemoteAgent> establish_connection (InjectSpec spec, string fallback_address,
				Cancellable? cancellable) throws Error {
			var promise = new Promise<RemoteAgent> ();

			var server_address = new UnixSocketAddress.with_type (fallback_address, -1, UnixSocketAddressType.ABSTRACT);
			Socket server_socket;
			try {
				var socket = new Socket (SocketFamily.UNIX, SocketType.STREAM, SocketProtocol.DEFAULT);
				socket.bind (server_address, true);
				socket.listen ();
				server_socket = socket;
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}

			accept_agent.begin (server_socket, spec, promise, cancellable);

			return promise.future;
		}

		private async void accept_agent (Socket server_socket, InjectSpec spec, Promise<RemoteAgent> promise,
				Cancellable? cancellable) {
			var listener = new SocketListener ();
			try {
				listener.add_socket (server_socket, null);

				var connection = (UnixConnection) yield listener.accept_async (cancellable);
				var agent = yield RemoteAgent.start (FROM_SCRATCH, spec, pid, new BootstrapResult (), connection, null,
					cancellable);
				promise.resolve (agent);
			} catch (Error e) {
				promise.reject (e);
			} catch (IOError e) {
				promise.reject (e);
			} catch (GLib.Error e) {
				promise.reject (new Error.TRANSPORT ("%s", e.message));
			} finally {
				listener.close ();
			}
		}

		private async RemoteAgent await_agent (Future<RemoteAgent> future_agent, Cancellable? cancellable)
				throws Error, IOError {
			var settle_cancellable = new Cancellable ();
			var main_context = MainContext.get_thread_default ();

			var timeout_source = new TimeoutSource.seconds (SETTLE_TIMEOUT_SECONDS);
			timeout_source.set_callback (() => {
				settle_cancellable.cancel ();
				return Source.REMOVE;
			});
			timeout_source.attach (main_context);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				settle_cancellable.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (main_context);

			RemoteAgent? agent = null;
			try {
				agent = yield future_agent.wait_async (settle_cancellable);
			} catch (IOError e) {
				cancellable.set_error_if_cancelled ();
				throw new Error.PROCESS_NOT_RESPONDING ("Timed out waiting for the injected agent to connect back");
			} finally {
				cancel_source.destroy ();
				timeout_source.destroy ();
			}

			agent.ack ();

			return agent;
		}

		private static string make_fallback_address () {
			return "/frida-" + Uuid.string_random ();
		}

		private static uint8[] make_cstring (string str) {
			unowned uint8[] bytes = str.data;
			var result = new uint8[bytes.length + 1];
			Memory.copy (result, bytes, bytes.length);
			return result;
		}

		private static uint64 align_up (uint64 value, size_t alignment) {
			return (value + (alignment - 1)) & ~((uint64) alignment - 1);
		}
	}
}
