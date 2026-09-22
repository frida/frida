[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	/**
	 * What differs between the kernels we inject into: how the target is brought to a
	 * quiescent state beforehand, and how it is let go afterwards.
	 */
	internal interface KernelFlavor : Object {
		public abstract async void prepare (Cancellable? cancellable) throws Error, IOError;
		public abstract async void settle (Cancellable? cancellable) throws Error, IOError;

		public abstract bool stays_attached { get; }
	}

	internal sealed class BareKernelFlavor : Object, KernelFlavor {
		public bool stays_attached {
			get { return true; }
		}

		private Machine machine;

		public BareKernelFlavor (Machine machine) {
			this.machine = machine;
		}

		public async void prepare (Cancellable? cancellable) throws Error, IOError {
		}

		public async void settle (Cancellable? cancellable) throws Error, IOError {
			yield machine.gdb.continue (cancellable);
		}
	}

	internal sealed class XnuKernelFlavor : Object, KernelFlavor {
		public bool stays_attached {
			get { return !has_left; }
		}

		private bool has_left = false;
		private Machine machine;
		private uint64 kernel_base;
		private SymbolInfo thread_block;
		private SymbolInfo? panic;

		public XnuKernelFlavor (Machine machine, uint64 kernel_base, Gee.Map<string, SymbolInfo> symbols)
				throws Error {
			this.machine = machine;
			this.kernel_base = kernel_base;

			thread_block = symbols["thread_block"];
			if (thread_block == null)
				throw new Error.NOT_SUPPORTED ("Missing symbol for thread_block");

			panic = symbols["panic"];
		}

		public async void prepare (Cancellable? cancellable) throws Error, IOError {
			uint64 thread_block_address = kernel_base + thread_block.offset;

			var arm64 = machine as Arm64Machine;
			if (panic != null && arm64 != null)
				arm64.call_landing_zone = kernel_base + panic.offset;
			var arm = machine as ArmMachine;
			if (panic != null && arm != null)
				arm.call_landing_zone = kernel_base + panic.offset;

			yield machine.enter_exception_level (1, 1000, cancellable);

			yield run_until_thread_block (thread_block_address, cancellable);

			if (arm64 != null)
				yield arm64.learn_permission_templates (thread_block_address, cancellable);
		}

		public async void settle (Cancellable? cancellable) throws Error, IOError {
			GDB.Client gdb = machine.gdb;

			// The vphone research kernel panics on any synchronous exception taken while a
			// debugger is attached, which the worker hits in the allocator during gum_init.
			var arm64 = machine as Arm64Machine;
			bool leaving_is_safe = arm64 != null && arm64.physical_memory != null
				&& Environment.get_variable ("FRIDA_BAREBONE_STAY") == null;
			if (leaving_is_safe) {
				yield gdb.detach (cancellable);
				has_left = true;
			} else {
				yield gdb.continue (cancellable);
			}
		}

		private async void run_until_thread_block (uint64 address, Cancellable? cancellable) throws Error, IOError {
			GDB.Client gdb = machine.gdb;
			var bp = yield gdb.add_breakpoint (SOFT, address, 4, cancellable);

			GDB.Breakpoint? hit = null;
			do {
				var exception = yield gdb.continue_until_exception (cancellable);
				hit = exception.breakpoint;
			} while (hit != bp);

			yield bp.remove (cancellable);
		}
	}

	internal sealed class LinuxKernelFlavor : Object, KernelFlavor {
		public bool stays_attached {
			get { return true; }
		}

		private Machine machine;
		private uint64 kernel_base;
		private Allocator allocator;
		private SymbolInfo schedule;
		private SymbolInfo? panic;
		private Gee.Map<string, SymbolInfo> symbols;
		private Allocation? current_probe_stub;

		public LinuxKernelFlavor (Machine machine, uint64 kernel_base, Allocator allocator,
				Gee.Map<string, SymbolInfo> symbols) throws Error {
			this.machine = machine;
			this.kernel_base = kernel_base;
			this.allocator = allocator;
			this.symbols = symbols;

			schedule = symbols["schedule"];
			if (schedule == null)
				throw new Error.NOT_SUPPORTED ("Missing symbol for schedule");

			panic = symbols["panic"];
		}

		public async void prepare (Cancellable? cancellable) throws Error, IOError {
			uint64 schedule_address = kernel_base + schedule.offset;

			var arm64 = machine as Arm64Machine;
			if (panic != null && arm64 != null)
				arm64.call_landing_zone = kernel_base + panic.offset;
			var arm = machine as ArmMachine;
			if (panic != null && arm != null)
				arm.call_landing_zone = kernel_base + panic.offset;

			var ia32 = machine as IA32Machine;
			if (ia32 != null)
				ia32.arguments_in_registers = LINUX_REGISTER_ARGUMENTS;

			if (arm64 != null && !arm64.mmu_registers_available) {
				arm64.set_memory_ro = symbol_address ("set_memory_ro");
				arm64.set_memory_rw = symbol_address ("set_memory_rw");
				arm64.set_memory_x = symbol_address ("set_memory_x");
				arm64.set_memory_nx = symbol_address ("set_memory_nx");
				if (arm64.set_memory_x == 0)
					throw new Error.NOT_SUPPORTED ("Missing set_memory_* symbols for kernel-API page protection");
			}

			yield machine.enter_exception_level (1, 1000, cancellable);

			yield run_until_schedule (schedule_address, cancellable);

			// Kernels predating execmem allocate with module_alloc, which reschedules. A task caught
			// entering the scheduler is already marked for sleep and would be dequeued there and
			// never resumed, so it is forced back to runnable first; the borrowed task then merely
			// sees a spurious wakeup once released.
			bool predates_execmem = arm64 != null && symbols["execmem_alloc"] == null;
			if (predates_execmem)
				yield keep_current_runnable (cancellable);

			// learn_permission_templates walks the page tables, which needs the MMU registers; the
			// kernel-API protection path does not use permission templates.
			if (arm64 != null && arm64.mmu_registers_available)
				yield arm64.learn_permission_templates (schedule_address, cancellable);
		}

		// The task caught at schedule() has set itself for sleep; a call made in its context that
		// then sleeps (module_alloc, kmalloc reclaim) would be dequeued there and never resumed, so
		// it is woken back to runnable first. current lives in SP_EL0, which the stub does not expose
		// as a register, so it is read by invoking a two-instruction stub that moves it into x0.
		private async void keep_current_runnable (Cancellable? cancellable) throws Error, IOError {
			uint64 wake_up_process = symbol_address ("wake_up_process");
			if (wake_up_process == 0)
				return;

			uint64 current = yield read_current_task (cancellable);
			if (current < TASK_VA_MIN || current >= TASK_VA_MAX)
				return;

			yield machine.invoke (wake_up_process, { current }, cancellable);
		}

		private async uint64 read_current_task (Cancellable? cancellable) throws Error, IOError {
			current_probe_stub = yield allocator.allocate (8, 8, cancellable);
			uint64 stub = current_probe_stub.virtual_address;
			var code = new uint8[8];
			write_u32 (code, 0, 0xd5384100u);	// mrs x0, sp_el0
			write_u32 (code, 4, 0xd65f03c0u);	// ret
			yield machine.write_virtual (stub, code, cancellable);

			return yield machine.invoke (stub, {}, cancellable);
		}

		private static void write_u32 (uint8[] buffer, uint offset, uint32 value) {
			buffer[offset + 0] = (uint8) (value >> 0);
			buffer[offset + 1] = (uint8) (value >> 8);
			buffer[offset + 2] = (uint8) (value >> 16);
			buffer[offset + 3] = (uint8) (value >> 24);
		}

		private uint64 symbol_address (string name) {
			var sym = symbols[name];
			return (sym != null) ? kernel_base + sym.offset : 0;
		}

		public async void settle (Cancellable? cancellable) throws Error, IOError {
			yield machine.gdb.continue (cancellable);
		}

		// A task passing voluntarily through the scheduler is a context the agent can be injected
		// from: sleepable, on a stack of its own, holding nothing. The CPU idle loop reaches the
		// scheduler with interrupts masked, and injecting from the idle task deadlocks a sleeping
		// callee (module_alloc, execmem_alloc), so those hits are skipped and a real task awaited.
		private async void run_until_schedule (uint64 address, Cancellable? cancellable) throws Error, IOError {
			GDB.Client gdb = machine.gdb;
			var bp = yield gdb.add_breakpoint (SOFT, address, 4, cancellable);

			SymbolInfo? system_state = symbols["system_state"];
			uint64 system_state_address = (system_state != null) ? kernel_base + system_state.offset : 0;

			GDB.Exception? exception = null;
			bool ready = false;
			do {
				exception = yield gdb.continue_until_exception (cancellable);
				ready = false;
				if (exception.breakpoint != bp)
					continue;
				if (yield interrupts_masked (exception.thread, cancellable))
					continue;
				ready = yield system_is_running (system_state_address, cancellable);
			} while (!ready);

			yield bp.remove (cancellable);
		}

		private async bool system_is_running (uint64 system_state_address, Cancellable? cancellable)
				throws Error, IOError {
			if (system_state_address == 0)
				return true;
			var data = (yield machine.read_virtual (system_state_address, 4, cancellable)).get_data ();
			uint32 state = 0;
			for (uint i = 0; i != 4; i++)
				state |= ((uint32) data[i]) << (8 * i);
			return state >= SYSTEM_RUNNING;
		}

		// Linux on arm64 runs with FIQ permanently masked, so only the IRQ mask distinguishes a
		// sleepable task (interrupts on) from the idle loop and interrupt-context reschedules.
		private async bool interrupts_masked (GDB.Thread thread, Cancellable? cancellable) throws Error, IOError {
			if (machine is IA32Machine || machine is X64Machine) {
				uint64 eflags = yield thread.read_register ("eflags", cancellable);
				return (eflags & INTERRUPT_ENABLE_BIT) == 0;
			}

			uint64 cpsr = yield thread.read_register ("cpsr", cancellable);
			return (cpsr & IRQ_MASK_BIT) != 0;
		}

		private const uint32 SYSTEM_RUNNING = 3;
		private const uint64 IRQ_MASK_BIT = 1ULL << 7;
		private const uint64 INTERRUPT_ENABLE_BIT = 1ULL << 9;
		private const uint LINUX_REGISTER_ARGUMENTS = 3;
		private const uint64 TASK_VA_MIN = 0xffffffc000000000;
		private const uint64 TASK_VA_MAX = 0xffffffc100000000;
	}

	internal sealed class Win9xKernelFlavor : Object, KernelFlavor {
		public bool stays_attached {
			get { return true; }
		}

		private Machine machine;
		private uint64 yield_point;

		public Win9xKernelFlavor (Machine machine, Gee.Map<string, SymbolInfo> symbols) throws Error {
			this.machine = machine;

			SymbolInfo? get_system_time = symbols["Get_System_Time"];
			if (get_system_time == null)
				throw new Error.NOT_SUPPORTED ("Missing symbol for Get_System_Time");
			yield_point = get_system_time.offset;
		}

		public async void prepare (Cancellable? cancellable) throws Error, IOError {
			yield run_until_yield_point (cancellable);
		}

		public async void settle (Cancellable? cancellable) throws Error, IOError {
			yield machine.gdb.continue (cancellable);
		}

		private async void run_until_yield_point (Cancellable? cancellable) throws Error, IOError {
			GDB.Client gdb = machine.gdb;
			var bp = yield gdb.add_breakpoint (SOFT, yield_point, 1, cancellable);

			while (true) {
				var exception = yield gdb.continue_until_exception (cancellable);
				if (exception.breakpoint == bp && yield stopped_in_ring_zero (gdb, cancellable))
					break;
			}

			yield bp.remove (cancellable);
		}

		private async bool stopped_in_ring_zero (GDB.Client gdb, Cancellable? cancellable)
				throws Error, IOError {
			uint64 cs = yield gdb.exception.thread.read_register ("cs", cancellable);

			return (cs & RING_MASK) == 0;
		}

		private const uint64 RING_MASK = 3;

	}

	internal sealed class WinNtKernelFlavor : Object, KernelFlavor {
		public bool stays_attached {
			get { return true; }
		}

		private Machine machine;
		private uint64 yield_point;
		private SymbolInfo? bug_check;

		public WinNtKernelFlavor (Machine machine, Gee.Map<string, SymbolInfo> symbols) throws Error {
			this.machine = machine;

			// A thread in this system service is at PASSIVE_LEVEL on its own kernel stack, which the
			// agent needs to start.
			SymbolInfo? wait_for_single_object = symbols["NtWaitForSingleObject"];
			if (wait_for_single_object == null)
				throw new Error.NOT_SUPPORTED ("Missing symbol for NtWaitForSingleObject");
			yield_point = wait_for_single_object.offset;

			bug_check = symbols["KeBugCheckEx"];
		}

		public async void prepare (Cancellable? cancellable) throws Error, IOError {
			var arm64 = machine as Arm64Machine;
			if (bug_check != null && arm64 != null)
				arm64.call_landing_zone = bug_check.offset;
			var arm = machine as ArmMachine;
			if (bug_check != null && arm != null)
				arm.call_landing_zone = bug_check.offset;

			yield run_until_yield_point (cancellable);
		}

		public async void settle (Cancellable? cancellable) throws Error, IOError {
			yield machine.gdb.continue (cancellable);
		}

		private async void run_until_yield_point (Cancellable? cancellable) throws Error, IOError {
			GDB.Client gdb = machine.gdb;
			var bp = yield gdb.add_breakpoint (SOFT, yield_point, breakpoint_size_for (gdb), cancellable);

			while (true) {
				var exception = yield gdb.continue_until_exception (cancellable);
				if (exception.breakpoint == bp && yield stopped_in_kernel_mode (gdb, cancellable))
					break;
			}

			yield bp.remove (cancellable);
		}

		private static size_t breakpoint_size_for (GDB.Client gdb) {
			return (gdb.arch == GDB.TargetArch.ARM64) ? 4 : 1;
		}
	}
}
