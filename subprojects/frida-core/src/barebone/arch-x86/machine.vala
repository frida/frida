[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class IA32Machine : Object, Machine {
		public override GDB.Client gdb {
			get;
			set;
		}

		public override string llvm_target {
			get { return "x86-unknown-none"; }
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

		public IA32Machine (GDB.Client gdb) {
			Object (gdb: gdb);

			page_tables = new X86PageTables (gdb);
		}

		public async size_t query_page_size (Cancellable? cancellable) throws Error, IOError {
			return 4096;
		}

		public uint arguments_in_registers = 0;

		private const string[] ARGUMENT_REGISTERS = { "eax", "edx", "ecx" };

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

		// Read the memory and do the match here. A script asks about small ranges, thus a scanner in
		// the guest would cost more round trips than it saves.
		public async Gee.List<uint64?> scan_ranges (Gee.List<Gum.MemoryRange?> ranges, MatchPattern pattern, uint max_matches,
				Cancellable? cancellable) throws Error, IOError {
			var matches = new Gee.ArrayList<uint64?> ();

			foreach (Gum.MemoryRange r in ranges) {
				uint64 end = r.base_address + r.size;
				uint64 cursor = r.base_address;
				while (cursor + pattern.size <= end) {
					size_t chunk_size = (size_t) uint64.min (SCAN_CHUNK_SIZE, end - cursor);

					// A range from a script can include memory that the guest paged out.
					Bytes chunk;
					try {
						chunk = yield gdb.read_byte_array (cursor, chunk_size, cancellable);
					} catch (Error e) {
						cursor += chunk_size;
						continue;
					}

					uint64 chunk_start = cursor;
					find_pattern_matches (pattern, chunk.get_data (), max_matches - matches.size, offset => {
						matches.add (chunk_start + offset);
					});
					if (matches.size == max_matches)
						return matches;

					cursor += chunk_size - (pattern.size - 1);
				}
			}

			return matches;
		}

		// This size is large enough to make the read longer than the round trip, and small enough to
		// keep the overlap at the boundaries small.
		private const size_t SCAN_CHUNK_SIZE = 256 * 1024;

		public void apply_relocation (Gum.ElfRelocationDetails r, uint64 base_va, Buffer relocated) throws Error {
			Gum.ElfIA32Relocation type = (Gum.ElfIA32Relocation) r.type;
			switch (type) {
				case NONE:
					break;
				case @32:
				case RELATIVE:
					// The in-place value is already the link-time target, the image being linked
					// at zero and loaded as a unit.
					relocated.write_uint32 ((size_t) r.address,
						(uint32) (base_va + relocated.read_uint32 ((size_t) r.address)));
					break;
				case PC32:
				case PLT32:
				case GOTPC:
				case GOTOFF:
					// Both ends of the displacement move by the same amount.
					break;
				default:
					throw new Error.NOT_SUPPORTED ("Unsupported relocation type: %s",
						Marshal.enum_to_nick<Gum.ElfIA32Relocation> (type));
			}
		}

		public async uint64 invoke (uint64 impl, uint64[] args, Cancellable? cancellable) throws Error, IOError {
			bool was_running = gdb.state != STOPPED;
			if (was_running)
				yield gdb.stop (cancellable);

			GDB.Thread thread = gdb.exception.thread;
			Gee.Map<string, Variant> saved_regs = yield thread.read_registers (cancellable);

			var regs = new Gee.HashMap<string, Variant> ();
			regs.set_all (saved_regs);

			uint64 landing_zone = saved_regs["eip"].get_uint64 ();

			uint in_registers = uint.min (arguments_in_registers, args.length);
			uint on_stack = args.length - in_registers;

			uint64 sp = saved_regs["esp"].get_uint64 () - ((1 + on_stack) * 4);
			sp = (sp & ~15ULL) - 4;

			var builder = gdb.make_buffer_builder ();
			builder.append_uint32 ((uint32) landing_zone);
			for (uint i = in_registers; i != args.length; i++)
				builder.append_uint32 ((uint32) args[i]);
			yield gdb.write_byte_array (sp, builder.build (), cancellable);

			for (uint i = 0; i != in_registers; i++)
				regs[ARGUMENT_REGISTERS[i]] = args[i];
			regs["eip"] = impl;
			regs["esp"] = sp;
			yield thread.write_registers (regs, cancellable);

			// The rest of the kernel also runs through this return address. Thus arrival is not
			// sufficient: only our call returns on the stack that we supplied.
			// The position in the stack depends on the callee, which can remove the arguments.
			uint64 first_landing_sp = sp + 4;
			uint64 last_landing_sp = sp + ((1 + args.length) * 4);
			GDB.Breakpoint bp = yield gdb.add_breakpoint (SOFT, landing_zone, 1, cancellable);
			GDB.Exception ex = null;
			while (true) {
				ex = yield gdb.continue_until_exception (cancellable);
				if (ex.breakpoint != bp)
					continue;
				uint64 landed_sp = yield ex.thread.read_register ("esp", cancellable);
				if (landed_sp >= first_landing_sp && landed_sp <= last_landing_sp)
					break;
			}
			yield bp.remove (cancellable);

			GDB.Thread landed = ex.thread;
			uint64 retval = yield landed.read_register ("eax", cancellable);

			yield landed.write_registers (saved_regs, cancellable);

			if (was_running)
				yield gdb.continue (cancellable);

			return retval;
		}

		public async CallFrame load_call_frame (GDB.Thread thread, uint arity, Cancellable? cancellable) throws Error, IOError {
			var regs = yield thread.read_registers (cancellable);

			uint64 original_esp = regs["esp"].get_uint64 ();
			var stack = yield gdb.read_buffer (original_esp, (1 + arity) * 4, cancellable);

			return new IA32CallFrame (thread, regs, stack, original_esp);
		}

		private class IA32CallFrame : Object, CallFrame {
			public uint64 return_address {
				get { return stack.read_uint32 (0); }
			}

			public Gee.Map<string, Variant> registers {
				get { return regs; }
			}

			private GDB.Thread thread;

			private Gee.Map<string, Variant> regs;

			private Buffer stack;
			private uint64 original_esp;
			private State stack_state = PRISTINE;

			private enum State {
				PRISTINE,
				MODIFIED
			}

			public IA32CallFrame (GDB.Thread thread, Gee.Map<string, Variant> regs, Buffer stack, uint64 original_esp) {
				this.thread = thread;

				this.regs = regs;

				this.stack = stack;
				this.original_esp = original_esp;
			}

			public uint64 get_nth_argument (uint n) {
				size_t offset;
				if (try_get_stack_offset_of_nth_argument (n, out offset))
					return stack.read_uint32 (offset);
				return uint64.MAX;
			}

			public void replace_nth_argument (uint n, uint64 val) {
				size_t offset;
				if (try_get_stack_offset_of_nth_argument (n, out offset)) {
					stack.write_uint32 (offset, (uint32) val);
					invalidate_stack ();
				}
			}

			private bool try_get_stack_offset_of_nth_argument (uint n, out size_t offset) {
				size_t start = (1 + n) * 4;
				size_t end = start + 4;
				if (end > stack.bytes.get_size ()) {
					offset = 0;
					return false;
				}

				offset = start;
				return true;
			}

			public uint64 get_return_value () {
				return regs["eax"].get_uint64 ();
			}

			public void replace_return_value (uint64 retval) {
				regs["eax"] = retval;
				invalidate_regs ();
			}

			public void force_return () {
				regs["eip"] = return_address;
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
					yield thread.client.write_byte_array (original_esp, stack.bytes, cancellable);
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
			return yield X86InlineHook.create (target, handler, IA32, allocator, gdb, cancellable);
		}

		public override async uint64 translate_address (uint64 va, Cancellable? cancellable) throws Error, IOError {
			return yield page_tables.translate (va, cancellable);
		}
	}
}
