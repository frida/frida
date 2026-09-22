[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public enum CallingConvention {
		SYSTEM_V,
		MICROSOFT
	}

	public sealed class X64Machine : Object, Machine {
		public override GDB.Client gdb {
			get;
			set;
		}

		public override string llvm_target {
			get { return "x86_64-unknown-none"; }
		}

		public override string llvm_code_model {
			get { return "small"; }
		}

		public PhysicalMemory? physical_memory {
			get {
				return page_tables.physical_memory;
			}
			set {
				page_tables.physical_memory = value;
			}
		}

		private X86PageTables page_tables;

		// The two conventions use different registers for the first arguments. They also disagree
		// about the space below the return address: one needs none, the other needs four slots.
		public CallingConvention calling_convention {
			get;
			set;
			default = SYSTEM_V;
		}

		internal unowned string[] arg_reg_names {
			get {
				return (calling_convention == MICROSOFT) ? ARG_REG_NAMES_MS : ARG_REG_NAMES_SYSV;
			}
		}

		private size_t reserved_below_sp {
			get {
				return (calling_convention == MICROSOFT) ? SHADOW_SPACE_SIZE : RED_ZONE_SIZE;
			}
		}

		private const string[] ARG_REG_NAMES_SYSV = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
		private const string[] ARG_REG_NAMES_MS = { "rcx", "rdx", "r8", "r9" };
		private const size_t RED_ZONE_SIZE = 128;
		private const size_t SHADOW_SPACE_SIZE = 32;

		public X64Machine (GDB.Client gdb) {
			Object (gdb: gdb);

			page_tables = new X86PageTables (gdb);
		}

		public async size_t query_page_size (Cancellable? cancellable) throws Error, IOError {
			return 4096;
		}

		private const uint64 CODE_SELECTOR_PRIVILEGE = 3;
		private const uint64 USER_PRIVILEGE = 3;

		public async uint query_exception_level (Cancellable? cancellable) throws Error, IOError {
			GDB.Exception? exception = gdb.exception;
			if (exception == null)
				throw new Error.INVALID_OPERATION ("Unable to query in current state");
			GDB.Thread thread = exception.thread;

			var cs = yield thread.read_register ("cs", cancellable);

			return ((cs & CODE_SELECTOR_PRIVILEGE) == USER_PRIVILEGE) ? 0 : 1;
		}

		public async void enumerate_ranges (Gum.PageProtection prot, FoundRangeFunc func, Cancellable? cancellable)
				throws Error, IOError {
			foreach (RangeDetails r in coalesce_ranges (yield page_tables.collect_ranges (cancellable))) {
				if ((r.protection & prot) != prot)
					continue;
				if (!func (r))
					return;
			}
		}

		public async Allocation allocate_pages (Gee.List<uint64?> physical_addresses, Cancellable? cancellable)
				throws Error, IOError {
			return yield page_tables.map (physical_addresses, cancellable);
		}

		public async void protect_pages (uint64 virtual_address, size_t size, Gum.PageProtection prot,
				Cancellable? cancellable) throws Error, IOError {
			yield page_tables.protect (virtual_address, size, prot, cancellable);
		}

		public async Gee.List<uint64?> scan_ranges (Gee.List<Gum.MemoryRange?> ranges, MatchPattern pattern, uint max_matches,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public void apply_relocation (Gum.ElfRelocationDetails r, uint64 base_va, Buffer relocated) throws Error {
			Gum.ElfX64Relocation type = (Gum.ElfX64Relocation) r.type;
			switch (type) {
				case NONE:
					break;
				case @64:
					// The in-place value is already the link-time target, the image being linked
					// at zero and loaded as a unit.
					relocated.write_uint64 ((size_t) r.address,
						base_va + relocated.read_uint64 ((size_t) r.address));
					break;
				case RELATIVE:
					// A loader applies this kind. It gives the value in the addend and leaves the slot empty.
					relocated.write_uint64 ((size_t) r.address, base_va + r.addend);
					break;
				case PC32:
				case PLT32:
				case GOTPCREL:
				case GOTPCRELX:
				case REX_GOTPCRELX:
					// Both ends of the displacement move by the same amount.
					break;
				default:
					throw new Error.NOT_SUPPORTED ("Unsupported relocation type: %s",
						Marshal.enum_to_nick<Gum.ElfX64Relocation> (type));
			}
		}

		// The large code model gives each target address in full.
		public bool relocates_text () {
			return true;
		}

		public async uint64 invoke (uint64 impl, uint64[] args, Cancellable? cancellable) throws Error, IOError {
			if (args.length > arg_reg_names.length)
				throw new Error.NOT_SUPPORTED ("Unsupported number of arguments; please open a PR");

			bool was_running = gdb.state != STOPPED;
			if (was_running)
				yield gdb.stop (cancellable);

			GDB.Thread thread = gdb.exception.thread;
			Gee.Map<string, Variant> saved_regs = yield thread.read_registers (cancellable);

			var regs = new Gee.HashMap<string, Variant> ();
			regs.set_all (saved_regs);

			uint64 landing_zone = saved_regs["rip"].get_uint64 ();

			uint64 sp = saved_regs["rsp"].get_uint64 () - reserved_below_sp - 8;
			sp = (sp & ~15ULL) - 8;

			var builder = gdb.make_buffer_builder ();
			builder.append_uint64 (landing_zone);
			yield gdb.write_byte_array (sp, builder.build (), cancellable);

			for (uint i = 0; i != args.length; i++)
				regs[arg_reg_names[i]] = args[i];

			regs["rip"] = impl;
			regs["rsp"] = sp;
			yield thread.write_registers (regs, cancellable);

			// The rest of the kernel also runs through this return address. Thus arrival is not
			// sufficient: only our call returns on the stack that we supplied.
			uint64 landing_sp = sp + 8;
			GDB.Breakpoint bp = yield gdb.add_breakpoint (SOFT, landing_zone, 1, cancellable);
			GDB.Exception ex = null;
			while (true) {
				ex = yield gdb.continue_until_exception (cancellable);
				if (ex.breakpoint != bp)
					continue;
				if ((yield ex.thread.read_register ("rsp", cancellable)) == landing_sp)
					break;
			}
			yield bp.remove (cancellable);

			GDB.Thread landed = ex.thread;
			uint64 retval = yield landed.read_register ("rax", cancellable);

			yield landed.write_registers (saved_regs, cancellable);

			if (was_running)
				yield gdb.continue (cancellable);

			return retval;
		}

		public async CallFrame load_call_frame (GDB.Thread thread, uint arity, Cancellable? cancellable) throws Error, IOError {
			var regs = yield thread.read_registers (cancellable);

			uint64 original_rsp = regs["rsp"].get_uint64 ();
			var num_stack_args = int.max ((int) arity - arg_reg_names.length, 0);
			size_t stack_args_offset = stack_args_offset_of_frame;
			var stack = yield gdb.read_buffer (original_rsp, stack_args_offset + num_stack_args * 8, cancellable);

			return new X64CallFrame (thread, regs, stack, original_rsp, arg_reg_names, stack_args_offset);
		}

		private size_t stack_args_offset_of_frame {
			get {
				return (calling_convention == MICROSOFT) ? 8 + SHADOW_SPACE_SIZE : 8;
			}
		}

		private class X64CallFrame : Object, CallFrame {
			public uint64 return_address {
				get { return stack.read_uint64 (0); }
			}

			public Gee.Map<string, Variant> registers {
				get { return regs; }
			}

			private GDB.Thread thread;

			private Gee.Map<string, Variant> regs;

			private Buffer stack;
			private uint64 original_rsp;
			private unowned string[] arg_reg_names;
			private size_t stack_args_offset;
			private State stack_state = PRISTINE;


			private enum State {
				PRISTINE,
				MODIFIED
			}

			public X64CallFrame (GDB.Thread thread, Gee.Map<string, Variant> regs, Buffer stack, uint64 original_rsp,
					string[] arg_reg_names, size_t stack_args_offset) {
				this.thread = thread;

				this.regs = regs;

				this.stack = stack;
				this.original_rsp = original_rsp;
				this.arg_reg_names = arg_reg_names;
				this.stack_args_offset = stack_args_offset;
			}

			public uint64 get_nth_argument (uint n) {
				unowned string name;
				if (try_get_register_name_of_nth_argument (n, out name))
					return regs[name].get_uint64 ();

				size_t offset;
				if (try_get_stack_offset_of_nth_argument (n, out offset))
					return stack.read_uint64 (offset);

				return uint64.MAX;
			}

			public void replace_nth_argument (uint n, uint64 val) {
				unowned string name;
				if (try_get_register_name_of_nth_argument (n, out name)) {
					regs[name] = val;
					invalidate_regs ();
					return;
				}

				size_t offset;
				if (try_get_stack_offset_of_nth_argument (n, out offset)) {
					stack.write_uint64 (offset, val);
					invalidate_stack ();
				}
			}

			private bool try_get_register_name_of_nth_argument (uint n, out unowned string name) {
				if (n >= arg_reg_names.length) {
					name = "";
					return false;
				}

				name = arg_reg_names[n];
				return true;
			}

			private bool try_get_stack_offset_of_nth_argument (uint n, out size_t offset) {
				offset = 0;

				if (n < arg_reg_names.length)
					return false;
				size_t start = stack_args_offset + (n - arg_reg_names.length) * 8;
				size_t end = start + 8;
				if (end > stack.bytes.get_size ())
					return false;

				offset = start;
				return true;
			}

			public uint64 get_return_value () {
				return regs["rax"].get_uint64 ();
			}

			public void replace_return_value (uint64 retval) {
				regs["rax"] = retval;
				invalidate_regs ();
			}

			public void force_return () {
				regs["rip"] = return_address;
				invalidate_regs ();
			}

			private void invalidate_regs () {
				regs.set_data ("dirty", true);
			}

			private void invalidate_stack () {
				stack_state = MODIFIED;
			}

			public async void commit (Cancellable? cancellable) throws Error, IOError {
				if (regs.get_data<bool> ("dirty"))
					yield thread.write_registers (regs, cancellable);

				if (stack_state == MODIFIED)
					yield thread.client.write_byte_array (original_rsp, stack.bytes, cancellable);
			}
		}

		public uint64 address_from_funcptr (uint64 ptr) {
			return ptr;
		}

		public size_t breakpoint_size_from_funcptr (uint64 ptr) {
			return 1;
		}

		public async InlineHook create_inline_hook (uint64 target, uint64 handler, Allocator allocator, Cancellable? cancellable)
				throws Error, IOError {
			return yield X86InlineHook.create (target, handler, AMD64, allocator, gdb, cancellable);
		}

		public override async uint64 translate_address (uint64 va, Cancellable? cancellable) throws Error, IOError {
			return yield page_tables.translate (va, cancellable);
		}
	}
}
