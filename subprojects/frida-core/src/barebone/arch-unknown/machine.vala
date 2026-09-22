[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class UnknownMachine : Object, Machine {
		public GDB.Client gdb {
			get;
			set;
		}

		public override string llvm_target {
			get { return "none"; }
		}

		public override string llvm_code_model {
			get { return "none"; }
		}

		public UnknownMachine (GDB.Client gdb) {
			Object (gdb: gdb);
		}

		public async size_t query_page_size (Cancellable? cancellable) throws Error, IOError {
			return 4096;
		}

		public async uint query_exception_level (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void enumerate_ranges (Gum.PageProtection prot, FoundRangeFunc func, Cancellable? cancellable)
				throws Error, IOError {
		}

		public async Allocation allocate_pages (Gee.List<uint64?> physical_addresses, Cancellable? cancellable)
				throws Error, IOError {
			throw_not_supported ();
		}

		public async void protect_pages (uint64 virtual_address, size_t size, Gum.PageProtection prot,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async Gee.List<uint64?> scan_ranges (Gee.List<Gum.MemoryRange?> ranges, MatchPattern pattern,
				uint max_matches, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public void apply_relocation (Gum.ElfRelocationDetails r, uint64 base_va, Buffer relocated) throws Error {
			throw_not_supported ();
		}

		public async uint64 invoke (uint64 impl, uint64[] args, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async CallFrame load_call_frame (GDB.Thread thread, uint arity, Cancellable? cancellable)
				throws Error, IOError {
			var regs = yield thread.read_registers (cancellable);

			return new UnknownCallFrame (thread, regs);
		}

		private class UnknownCallFrame : Object, CallFrame {
			public uint64 return_address {
				get { return uint64.MAX; }
			}

			public Gee.Map<string, Variant> registers {
				get { return regs; }
			}

			private GDB.Thread thread;

			private Gee.Map<string, Variant> regs;

			private enum State {
				PRISTINE,
				MODIFIED
			}

			public UnknownCallFrame (GDB.Thread thread, Gee.Map<string, Variant> regs) {
				this.thread = thread;

				this.regs = regs;
			}

			public uint64 get_nth_argument (uint n) {
				return uint64.MAX;
			}

			public void replace_nth_argument (uint n, uint64 val) {
			}

			public uint64 get_return_value () {
				return uint64.MAX;
			}

			public void replace_return_value (uint64 retval) {
			}

			public void force_return () {
			}

			public async void commit (Cancellable? cancellable) throws Error, IOError {
				if (regs.get_data<bool> ("dirty"))
					yield thread.write_registers (regs, cancellable);
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
			throw_not_supported ();
		}
	}
}
