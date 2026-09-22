[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	internal class X86InlineHook : Object, InlineHook {
		private uint64 target;
		private Bytes old_target_code;
		private Bytes new_target_code;
		private Allocation allocation;
		private GDB.Client gdb;
		private State state = DISABLED;

		private enum State {
			DISABLED,
			ENABLED,
			DESTROYED
		}

		private const size_t SCRATCH_SIZE = 512;
		private const size_t DIRECT_REDIRECT_SIZE = 5;
		private const size_t INDIRECT_REDIRECT_SIZE = 16;
		private const size_t MAX_INSTRUCTION_SIZE = 15;

		public static async InlineHook create (uint64 target, uint64 handler, Gum.CpuType cpu, Allocator allocator,
				GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
			Allocation allocation = yield allocator.allocate (allocator.page_size, allocator.page_size, cancellable);
			uint64 code_va = allocation.virtual_address;

			var scratch = new uint8[SCRATCH_SIZE];
			size_t pointer_size = (cpu == IA32) ? 4 : 8;

			var cw = new Gum.X86Writer (scratch);
			cw.set_target_cpu (cpu);
			cw.pc = code_va;

			emit_prolog (cw, target);
			cw.put_mov_reg_reg (Gum.X86Reg.XAX, Gum.X86Reg.XSP);
			cw.put_call_address_with_arguments (CAPI, handler, 1, Gum.ArgType.REGISTER, Gum.X86Reg.XAX);
			emit_epilog (cw, pointer_size);

			size_t redirect_size = Gum.X86Writer.can_branch_directly_between (target, code_va)
				? DIRECT_REDIRECT_SIZE
				: INDIRECT_REDIRECT_SIZE;

			Bytes displaced = yield gdb.read_byte_array (target, redirect_size + MAX_INSTRUCTION_SIZE, cancellable);

			var rl = new Gum.X86Relocator (displaced.get_data (), cw);
			rl.input_pc = target;
			uint reloc_bytes = 0;
			do
				reloc_bytes = rl.read_one ();
			while (reloc_bytes < redirect_size);
			rl.write_all ();
			if (!rl.eoi)
				cw.put_jmp_address (target + reloc_bytes);
			cw.flush ();

			var trampoline_code = new Bytes (scratch[:cw.offset ()]);
			yield gdb.write_byte_array (code_va, trampoline_code, cancellable);

			cw.reset (scratch);
			cw.set_target_cpu (cpu);
			cw.pc = target;
			cw.put_jmp_address (code_va);
			cw.flush ();
			var new_target_code = new Bytes (scratch[:cw.offset ()]);

			var old_target_code = new Bytes (displaced.get_data ()[:new_target_code.get_size ()]);

			return new X86InlineHook (target, old_target_code, new_target_code, allocation, gdb);
		}

		private static void emit_prolog (Gum.X86Writer cw, uint64 target) {
			cw.put_push_u32 (0);
			cw.put_pushax ();
			cw.put_mov_reg_address (Gum.X86Reg.XAX, target);
			cw.put_push_reg (Gum.X86Reg.XAX);
		}

		private static void emit_epilog (Gum.X86Writer cw, size_t pointer_size) {
			cw.put_add_reg_imm (Gum.X86Reg.XSP, (ssize_t) pointer_size);
			cw.put_popax ();
			cw.put_add_reg_imm (Gum.X86Reg.XSP, (ssize_t) pointer_size);
		}

		public X86InlineHook (uint64 target, Bytes old_target_code, Bytes new_target_code, Allocation allocation,
				GDB.Client gdb) {
			this.target = target;
			this.old_target_code = old_target_code;
			this.new_target_code = new_target_code;
			this.allocation = allocation;
			this.gdb = gdb;
		}

		public async void destroy (Cancellable? cancellable) throws Error, IOError {
			if (state == DESTROYED)
				return;

			bool was_running = gdb.state != STOPPED;
			if (was_running)
				yield gdb.stop (cancellable);

			yield disable (cancellable);
			yield allocation.deallocate (cancellable);
			state = DESTROYED;

			if (was_running)
				yield gdb.continue (cancellable);
		}

		public async void enable (Cancellable? cancellable) throws Error, IOError {
			if (state == ENABLED)
				return;
			if (state != DISABLED)
				throw new Error.INVALID_OPERATION ("Invalid operation");
			yield gdb.write_byte_array (target, new_target_code, cancellable);
			state = ENABLED;
		}

		public async void disable (Cancellable? cancellable) throws Error, IOError {
			if (state != ENABLED)
				return;
			yield gdb.write_byte_array (target, old_target_code, cancellable);
			state = DISABLED;
		}
	}
}
