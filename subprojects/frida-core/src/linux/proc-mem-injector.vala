namespace Frida {
	private sealed class ProcMemInjectSession : Object {
		public uint pid {
			get;
			construct;
		}

		private ProcMemSession mem;

		private const string TRIGGER_FUNCTION = "malloc";
		private const uint TRIGGER_TIMEOUT_SECONDS = 10;
		private const double NUDGE_AFTER_SECONDS = 0.25;
		private const uint REGION_POLL_INTERVAL_MS = 5;
		private const uint DRAIN_MS = 50;
		private const uint SAMPLE_WINDOW_MS = 250;
		private const uint STACK_SCAN_DEPTH = 8;
		private const uint MIN_TRIGGER_VOTES = 4;
#if X86 || X86_64
		private const int SCAN_STEP = 1;
#else
		private const int SCAN_STEP = 4;
#endif
		private const size_t REGION_CODE_BUDGET = 256;
#if X86 || X86_64
		private const size_t BOOTSTRAP_OFFSET = 2;
#elif ARM64
		private const size_t BOOTSTRAP_OFFSET = 4;
		private const uint32 INSN_B_SELF = 0x14000000;
#else
		private const size_t BOOTSTRAP_OFFSET = 0;
#endif

		private ProcMemInjectSession (uint pid, ProcMemSession mem) {
			Object (pid: pid);
			this.mem = mem;
		}

		public static bool is_available () {
#if X86 || X86_64 || ARM64
			return true;
#else
			return false;
#endif
		}

		public static async ProcMemInjectSession open (uint pid, Cancellable? cancellable) throws Error, IOError {
			return new ProcMemInjectSession (pid, ProcMemSession.open (pid));
		}

		public async RemoteAgent inject (InjectSpec spec, Cancellable? cancellable) throws Error, IOError {
			var maps = ProcMapsSnapshot.from_pid (pid);
			var libc = RemoteLibcApi.resolve (maps);

			uint64 target = RemoteLibcApi.resolve_export (maps, TRIGGER_FUNCTION);
			uint64 mmap_impl = (uint64) (uintptr) libc.table.mmap;
			uint64 mprotect_impl = RemoteLibcApi.resolve_export (maps, "mprotect");

			string fallback_address = make_fallback_address ();
			var rendezvous = StackRendezvous.compute (find_stack_scratch (maps));
			var region = RegionLayout.compute (spec, fallback_address);

			Future<RemoteAgent> future_agent = establish_connection (spec, fallback_address, cancellable);

			write_rendezvous (rendezvous);

			var acquired = yield acquire_region (maps, region, rendezvous, mmap_impl, target, cancellable);

			block_callers (acquired.target);
			write_region (acquired.region_base, region, spec, fallback_address, libc, acquired.target, mprotect_impl);
			mem.write_u32 (rendezvous.go, 1);
			yield restore_trigger (acquired.target, acquired.original);

			return yield await_agent (future_agent, cancellable);
		}

		// Install the bootstrap and wait for a thread to run through it. If the trigger
		// stays quiet we either nudge a managed runtime into allocating, or — when there
		// is nothing to nudge — sample what the target is actually running and re-hook a
		// libc function it favours instead.
		private async AcquiredRegion acquire_region (ProcMapsSnapshot maps, RegionLayout region,
				StackRendezvous rendezvous, uint64 mmap_impl, uint64 target, Cancellable? cancellable)
				throws Error, IOError {
			bool managed = targets_managed_runtime (maps);

			uint8[] original = yield install_trigger (maps, region, rendezvous, mmap_impl, target);

			var timer = new Timer ();
			bool prodded = false;
			while (true) {
				cancellable.set_error_if_cancelled ();

				uint64 region_base = mem.read_pointer (rendezvous.mmap_result);
				if (region_base == uint64.MAX)
					throw new Error.NOT_SUPPORTED ("Target failed to mmap a region for the loader");
				if (region_base != 0)
					return new AcquiredRegion (region_base, target, original);

				if (!prodded && timer.elapsed () >= NUDGE_AFTER_SECONDS) {
					prodded = true;
					if (managed) {
						Posix.kill ((Posix.pid_t) pid, Posix.Signal.USR1);
					} else {
						uint64 alt = yield discover_trigger (maps, region, rendezvous, mmap_impl, target,
							cancellable);
						if (alt != 0 && mem.read_pointer (rendezvous.mmap_result) == 0) {
							yield restore_trigger (target, original);
							target = alt;
							original = yield install_trigger (maps, region, rendezvous, mmap_impl, target);
							timer.start ();
						}
					}
				}

				if (timer.elapsed () >= TRIGGER_TIMEOUT_SECONDS)
					throw new Error.PROCESS_NOT_RESPONDING (
						"Timed out waiting for the target to trigger the injected stub");

				yield sleep_ms (REGION_POLL_INTERVAL_MS);
			}
		}

		private async uint8[] install_trigger (ProcMapsSnapshot maps, RegionLayout region, StackRendezvous rendezvous,
				uint64 mmap_impl, uint64 target) throws Error, IOError {
			uint64 reuse = reusable_region (maps, region.total);
			uint8[] stub = build_trigger_stub (target, rendezvous.cas, mmap_impl, reuse, region.total,
				region.entry_offset);
			uint8[] original = mem.read_memory (target, BOOTSTRAP_OFFSET + stub.length);
			yield install_bootstrap (target, stub);
			return original;
		}

		private async void install_bootstrap (uint64 target, uint8[] stub) throws Error, IOError {
			block_callers (target);
			yield sleep_ms (DRAIN_MS);
			mem.write_memory (target + BOOTSTRAP_OFFSET, stub);
			release_callers (target);
		}

		// Tear down symmetrically with install_bootstrap: re-block the entry so callers spin
		// there while we put the body back, and only then restore the first instruction. A
		// multi-byte prologue (e.g. endbr64) would otherwise be re-entered mid-restore, with
		// the fall-through still routing into a half-written instruction.
		private async void restore_trigger (uint64 target, uint8[] original) throws Error, IOError {
			block_callers (target);
			yield sleep_ms (DRAIN_MS);
			mem.write_memory (target + BOOTSTRAP_OFFSET, original[BOOTSTRAP_OFFSET:original.length]);
			restore_prologue (target, original);
		}

		private void block_callers (uint64 target) throws Error {
#if X86 || X86_64
			uint8 jmp_self[2] = { 0xeb, 0xfe };
			mem.write_memory (target, jmp_self);
#elif ARM64
			mem.write_instruction (target, INSN_B_SELF);
#endif
		}

		private void release_callers (uint64 target) throws Error {
#if X86 || X86_64
			uint8 fall_through[1] = { 0x00 };
			mem.write_memory (target + 1, fall_through);
#elif ARM64
			mem.write_instruction (target, branch (target, target + 4));
#endif
		}

		private void restore_prologue (uint64 target, uint8[] original) throws Error {
#if X86 || X86_64
			mem.write_memory (target, original[:BOOTSTRAP_OFFSET]);
#elif ARM64
			mem.write_instruction (target, peek_u32 (original));
#endif
		}

		// The three rendezvous words that must exist before the region does, so they
		// live at the bottom of the main thread's stack (the deepest it has ever grown,
		// so genuinely unused yet CPU-writable). This is all we borrow from the stack:
		// the election word at offset 0, the mmap-result slot at offset 8 and the go
		// flag at offset 16, matching the in-trigger stub. The rest of the working set
		// rides in the mmap()ed region.
		private struct StackRendezvous {
			public uint64 cas;
			public uint64 mmap_result;
			public uint64 go;

			public static StackRendezvous compute (uint64 start) {
				var r = StackRendezvous ();
				r.cas = start;
				r.mmap_result = start + 8;
				r.go = start + 16;
				return r;
			}
		}

		// The mmap()ed region carries the loader, the code the winner runs and the read-only
		// libc table and argument strings it reads — all R-X. The one target-writable datum
		// is the loader context (frida_load stores its worker handle there), so it sits on a
		// trailing page of its own that the winner flips to RW before invoking the loader.
		// W and X thus stay disjoint without ever mapping the region RWX.
		private struct RegionLayout {
			public size_t entry_offset;
			public size_t libc_offset;
			public size_t entrypoint_offset;
			public size_t data_offset;
			public size_t fallback_offset;
			public size_t context_offset;
			public size_t page_size;
			public size_t total;

			public static RegionLayout compute (InjectSpec spec, string fallback_address) {
				size_t loader_size = Frida.Data.HelperBackend.get_loader_bin_blob ().data.length;
				size_t entrypoint_size = make_cstring (spec.entrypoint).length;
				size_t data_size = make_cstring (spec.data).length;
				size_t fallback_size = make_cstring (fallback_address).length;
				size_t page_size = Gum.query_page_size ();

				var l = RegionLayout ();
				l.entry_offset = (size_t) align_up (loader_size, 16);
				size_t o = (size_t) align_up (l.entry_offset + REGION_CODE_BUDGET, 16);
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

		private uint64 find_stack_scratch (ProcMapsSnapshot maps) throws Error {
			var it = maps.iterator ();
			while (it.next ()) {
				var m = it.get ();
				if (m.path == "[stack]" && m.readable && m.writable)
					return m.start;
			}
			throw new Error.NOT_SUPPORTED ("Unable to locate the main thread stack");
		}

		// Rediscover a region we mmap()ed on an earlier injection so a long-lived Frida
		// recycles it rather than leaking one each time. Matching our loader bytes (not a
		// cache, which the short-lived helper would lose) also rules out a recycled pid.
		private uint64 reusable_region (ProcMapsSnapshot maps, size_t needed) throws Error {
			var it = maps.iterator ();
			while (it.next ()) {
				var m = it.get ();
				if (m.executable && m.path == "" && region_fits (maps, m.start, needed)
						&& region_holds_loader (m.start))
					return m.start;
			}
			return 0;
		}

		private bool region_fits (ProcMapsSnapshot maps, uint64 start, size_t needed) {
			uint64 cursor = start;
			for (var m = maps.find_mapping (cursor); m != null && m.start == cursor; m = maps.find_mapping (cursor)) {
				cursor = m.end;
				if (cursor - start >= needed)
					return true;
			}
			return false;
		}

		private bool region_holds_loader (uint64 address) throws Error {
			unowned uint8[] loader = Frida.Data.HelperBackend.get_loader_bin_blob ().data;
			uint8[] current = mem.read_memory (address, loader.length);
			return Memory.cmp (current, loader, loader.length) == 0;
		}

		private bool targets_managed_runtime (ProcMapsSnapshot maps) {
			var it = maps.iterator ();
			while (it.next ()) {
				unowned string path = it.get ().path;
				if (path.has_suffix ("/libart.so") || path.has_suffix ("/libdvm.so"))
					return true;
			}
			return false;
		}

		private void write_rendezvous (StackRendezvous r) throws Error {
			mem.write_memory (r.cas, new uint8[24]);
		}

		private void write_region (uint64 region_base, RegionLayout l, InjectSpec spec, string fallback_address,
				RemoteLibcApi libc, uint64 target, uint64 mprotect_impl) throws Error {
			mem.write_memory (region_base, Frida.Data.HelperBackend.get_loader_bin_blob ().data);
			mem.write_memory (region_base + l.entry_offset, build_region_code (region_base, l, target, mprotect_impl));

			var ctx = HelperLoaderContext ();
			ctx.ctrlfds[0] = -1;
			ctx.ctrlfds[1] = -1;
			ctx.agent_entrypoint = (string *) (region_base + l.entrypoint_offset);
			ctx.agent_data = (string *) (region_base + l.data_offset);
			ctx.fallback_address = (string *) (region_base + l.fallback_offset);
			ctx.libc = (HelperLibcApi *) (region_base + l.libc_offset);

			mem.write_memory (region_base + l.context_offset, (uint8[]) &ctx);
			mem.write_memory (region_base + l.libc_offset, (uint8[]) &libc.table);
			mem.write_memory (region_base + l.entrypoint_offset, make_cstring (spec.entrypoint));
			mem.write_memory (region_base + l.data_offset, make_cstring (spec.data));
			mem.write_memory (region_base + l.fallback_offset, make_cstring (fallback_address));
		}

		private uint8[] build_trigger_stub (uint64 target, uint64 scratch, uint64 mmap_impl, uint64 reuse_region,
				size_t region_size, size_t entry_offset) throws Error {
#if X86
			var buffer = new uint8[REGION_CODE_BUDGET];
			var writer = new Gum.X86Writer ((void *) buffer);
			writer.pc = target + BOOTSTRAP_OFFSET;

			void * loser = (void *) 1;
			void * wait_go = (void *) 2;

			// Elect one winner with a single locked compare-and-swap. The trigger's
			// cdecl args stay on the stack, so we never touch them.
			writer.put_mov_reg_address (EDX, scratch);
			writer.put_xor_reg_reg (EAX, EAX);               // expected = 0
			writer.put_mov_reg_u32 (ECX, 1);                 // desired = 1
			writer.put_lock_cmpxchg_reg_ptr_reg (EDX, ECX);
			writer.put_jcc_short_label (JNE, loser, NO_HINT);

			if (reuse_region != 0) {
				writer.put_mov_reg_address (EAX, reuse_region);
			} else {
				// region = mmap (NULL, region_size, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
				writer.put_add_reg_imm (ESP, -4);                // pad to 16-align esp at the call
				writer.put_push_u32 (0);                         // offset
				writer.put_push_u32 (uint32.MAX);                // fd = -1
				writer.put_push_u32 (0x22);                      // MAP_PRIVATE | MAP_ANONYMOUS
				writer.put_push_u32 (5);                         // PROT_READ | PROT_EXEC
				writer.put_push_u32 ((uint32) region_size);      // length
				writer.put_push_u32 (0);                         // addr
				writer.put_call_address ((Gum.Address) mmap_impl);
				writer.put_add_reg_imm (ESP, 28);                // cdecl cleanup + the pad
			}

			writer.put_mov_reg_address (EDX, scratch);       // reload the scratch base (mmap clobbers edx)
			writer.put_mov_reg_offset_ptr_reg (EDX, 8, EAX); // publish region to the mmap-result slot

			// The region holds code only after the host stages it and raises `go`.
			writer.put_label (wait_go);
			writer.put_mov_reg_reg_offset_ptr (ECX, EDX, 16);
			writer.put_test_reg_reg (ECX, ECX);
			writer.put_jcc_short_label (JE, wait_go, NO_HINT);
			writer.put_add_reg_imm (EAX, (ssize_t) entry_offset);
			writer.put_jmp_reg (EAX);

			// Losers spin at the trigger's first instruction until it is restored.
			writer.put_label (loser);
			writer.put_jmp_address ((Gum.Address) target);

			writer.flush ();
			return buffer[:writer.offset ()];
#elif X86_64
			var buffer = new uint8[REGION_CODE_BUDGET];
			var writer = new Gum.X86Writer ((void *) buffer);
			writer.pc = target + BOOTSTRAP_OFFSET;

			void * loser = (void *) 1;
			void * wait_go = (void *) 2;

			// Elect one winner with a single locked compare-and-swap.
			save_call_args (writer);
			writer.put_mov_reg_address (R11, scratch);
			writer.put_xor_reg_reg (EAX, EAX);               // expected = 0
			writer.put_mov_reg_u32 (ECX, 1);                 // desired = 1
			writer.put_lock_cmpxchg_reg_ptr_reg (R11, ECX);
			writer.put_jcc_short_label (JNE, loser, NO_HINT);

			if (reuse_region != 0) {
				writer.put_mov_reg_u64 (RAX, reuse_region);
			} else {
				// region = mmap (NULL, region_size, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
				writer.put_xor_reg_reg (EDI, EDI);
				writer.put_mov_reg_u32 (ESI, (uint32) region_size);
				writer.put_mov_reg_u32 (EDX, 5);
				writer.put_mov_reg_u32 (ECX, 0x22);
				writer.put_mov_reg_u64 (R8, uint64.MAX);         // fd = -1
				writer.put_xor_reg_reg (R9, R9);
				writer.put_call_address ((Gum.Address) mmap_impl);
			}

			writer.put_mov_reg_address (R11, scratch);       // reload the scratch base (mmap clobbers r11)
			writer.put_mov_reg_offset_ptr_reg (R11, 8, RAX); // publish region to the mmap-result slot

			// The region holds code only after the host stages it and raises `go`.
			writer.put_label (wait_go);
			writer.put_mov_reg_reg_offset_ptr (ECX, R11, 16);
			writer.put_test_reg_reg (ECX, ECX);
			writer.put_jcc_short_label (JE, wait_go, NO_HINT);
			writer.put_add_reg_imm (RAX, (ssize_t) entry_offset);
			writer.put_jmp_reg (RAX);

			// Losers restore the frame and spin at the trigger's first instruction until it is restored.
			writer.put_label (loser);
			restore_call_args (writer);
			writer.put_jmp_address ((Gum.Address) target);

			writer.flush ();
			return buffer[:writer.offset ()];
#elif ARM64
			var buffer = new uint8[REGION_CODE_BUDGET];
			var writer = new Gum.Arm64Writer ((void *) buffer);
			writer.pc = target + BOOTSTRAP_OFFSET;

			void * loser = (void *) 1;
			void * wait_go = (void *) 2;

			// Elect one winner with a single LSE compare-and-swap. We deliberately
			// avoid LDAXR/STLXR: under the foreign tracer that put us on this path the
			// exclusive monitor is cleared out from under us, which livelocks the
			// store-exclusive (and corrupts the address register across retries).
			save_call_args (writer);
			writer.put_ldr_reg_address (X16, scratch);
			writer.put_instruction ((uint32) 0xd2800000); // movz x0, #0 (expected)
			writer.put_instruction ((uint32) 0x52800031); // movz w17, #1 (desired)
			writer.put_instruction ((uint32) 0x88e0fe11); // casal w0, w17, [x16]
			writer.put_cbnz_reg_label (W0, loser);

			if (reuse_region != 0) {
				writer.put_ldr_reg_address (X0, reuse_region);
			} else {
				if (region_size > 0xffff)
					throw new Error.NOT_SUPPORTED ("Region too large for the in-trigger bootstrap");
				// region = mmap (NULL, region_size, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
				writer.put_instruction ((uint32) 0xd2800000);                  // movz x0, #0
				writer.put_instruction (movz_imm (1, (uint16) region_size));   // movz x1, #region_size
				writer.put_instruction ((uint32) 0xd28000a2);                  // movz x2, #5 (R|X)
				writer.put_instruction ((uint32) 0xd2800443);                  // movz x3, #0x22
				writer.put_instruction ((uint32) 0x92800004);                  // movn x4, #0 (-1)
				writer.put_instruction ((uint32) 0xd2800005);                  // movz x5, #0
				if (!writer.put_bl_imm ((Gum.Address) mmap_impl))
					throw new Error.NOT_SUPPORTED ("mmap is out of branch range of the trigger");
			}

			writer.put_ldr_reg_address (X16, scratch);  // reload the scratch base (mmap clobbers x16/IP0)
			writer.put_str_reg_reg_offset (X0, X16, 8); // publish region to the mmap-result slot

			// The region holds code only after the host stages it and raises `go`.
			writer.put_label (wait_go);
			writer.put_ldr_reg_reg_offset (W2, X16, 16); // w2 = go
			writer.put_cbz_reg_label (W2, wait_go);
			writer.put_add_reg_reg_imm (X17, X0, entry_offset);
			writer.put_br_reg (X17);

			// Losers restore the frame and spin at the trigger's first instruction until it is restored.
			writer.put_label (loser);
			restore_call_args (writer);
			if (!writer.put_b_imm ((Gum.Address) target))
				throw new Error.NOT_SUPPORTED ("trigger prologue is out of branch range");

			writer.flush ();
			return buffer[:writer.offset ()];
#else
			throw new Error.NOT_SUPPORTED ("In-trigger bootstrap is only implemented for x86, x86_64 and arm64");
#endif
		}

		// Only the winner reaches here. Flip the context page to RW so frida_load can
		// store its worker handle, run the loader (which spawns the worker thread), then
		// restore the frame and fall back into the real trigger to satisfy the call that
		// brought us in. By now the trigger's first instruction is blocked, so the branch
		// lands on the spin until the host restores the prologue.
		private uint8[] build_region_code (uint64 region_base, RegionLayout l, uint64 target, uint64 mprotect_impl) {
			uint64 context = region_base + l.context_offset;
#if X86
			var buffer = new uint8[REGION_CODE_BUDGET];
			var writer = new Gum.X86Writer ((void *) buffer);
			writer.pc = region_base + l.entry_offset;

			// mprotect (context, page_size, PROT_READ | PROT_WRITE)
			writer.put_push_u32 (3);
			writer.put_push_u32 ((uint32) l.page_size);
			writer.put_push_u32 ((uint32) context);
			writer.put_call_address ((Gum.Address) mprotect_impl);
			writer.put_add_reg_imm (ESP, 12);

			writer.put_add_reg_imm (ESP, -8);                // pad to 16-align esp at the call
			writer.put_push_u32 ((uint32) context);
			writer.put_call_address ((Gum.Address) region_base);
			writer.put_add_reg_imm (ESP, 12);                // pop arg + the pad

			writer.put_jmp_address ((Gum.Address) target);

			writer.flush ();
			return buffer[:writer.offset ()];
#elif X86_64
			var buffer = new uint8[REGION_CODE_BUDGET];
			var writer = new Gum.X86Writer ((void *) buffer);
			writer.pc = region_base + l.entry_offset;

			// mprotect (context, page_size, PROT_READ | PROT_WRITE)
			writer.put_mov_reg_address (RDI, context);
			writer.put_mov_reg_u32 (ESI, (uint32) l.page_size);
			writer.put_mov_reg_u32 (EDX, 3);
			writer.put_call_address ((Gum.Address) mprotect_impl);

			writer.put_mov_reg_address (RDI, context);
			writer.put_call_address ((Gum.Address) region_base);

			restore_call_args (writer);
			writer.put_jmp_address ((Gum.Address) target);

			writer.flush ();
			return buffer[:writer.offset ()];
#elif ARM64
			var buffer = new uint8[REGION_CODE_BUDGET];
			var writer = new Gum.Arm64Writer ((void *) buffer);
			writer.pc = region_base + l.entry_offset;

			// mprotect (context, page_size, PROT_READ | PROT_WRITE)
			writer.put_ldr_reg_address (X0, context);
			writer.put_instruction (movz_imm (1, (uint16) l.page_size));
			writer.put_instruction (movz_imm (2, 3));
			writer.put_ldr_reg_address (X16, mprotect_impl);
			writer.put_blr_reg (X16);

			writer.put_ldr_reg_address (X0, context);
			writer.put_ldr_reg_address (X16, region_base);
			writer.put_blr_reg (X16);

			restore_call_args (writer);
			writer.put_ldr_reg_address (X16, target);
			writer.put_br_reg (X16);

			writer.flush ();
			return buffer[:writer.offset ()];
#else
			assert_not_reached ();
#endif
		}

		// Preserve every integer argument register so the trigger can be re-invoked
		// intact afterwards, whatever its arity. x86 needs no equivalent: its arguments
		// ride the stack, which the stub never disturbs.
#if X86_64
		private void save_call_args (Gum.X86Writer writer) {
			writer.put_push_reg (RDI);
			writer.put_push_reg (RSI);
			writer.put_push_reg (RDX);
			writer.put_push_reg (RCX);
			writer.put_push_reg (R8);
			writer.put_push_reg (R9);
			writer.put_add_reg_imm (RSP, -8);                // keep RSP 16-byte aligned at the calls
		}

		private void restore_call_args (Gum.X86Writer writer) {
			writer.put_add_reg_imm (RSP, 8);
			writer.put_pop_reg (R9);
			writer.put_pop_reg (R8);
			writer.put_pop_reg (RCX);
			writer.put_pop_reg (RDX);
			writer.put_pop_reg (RSI);
			writer.put_pop_reg (RDI);
		}
#elif ARM64
		private void save_call_args (Gum.Arm64Writer writer) {
			writer.put_stp_reg_reg_reg_offset (X0, X1, SP, -16, PRE_ADJUST);
			writer.put_stp_reg_reg_reg_offset (X2, X3, SP, -16, PRE_ADJUST);
			writer.put_stp_reg_reg_reg_offset (X4, X5, SP, -16, PRE_ADJUST);
			writer.put_stp_reg_reg_reg_offset (X6, X7, SP, -16, PRE_ADJUST);
			writer.put_stp_reg_reg_reg_offset (X8, LR, SP, -16, PRE_ADJUST);
		}

		private void restore_call_args (Gum.Arm64Writer writer) {
			writer.put_ldp_reg_reg_reg_offset (X8, LR, SP, 16, POST_ADJUST);
			writer.put_ldp_reg_reg_reg_offset (X6, X7, SP, 16, POST_ADJUST);
			writer.put_ldp_reg_reg_reg_offset (X4, X5, SP, 16, POST_ADJUST);
			writer.put_ldp_reg_reg_reg_offset (X2, X3, SP, 16, POST_ADJUST);
			writer.put_ldp_reg_reg_reg_offset (X0, X1, SP, 16, POST_ADJUST);
		}
#endif

#if ARM64
		private static uint32 movz_imm (uint8 reg, uint16 imm) {
			return (uint32) (0xd2800000 | ((uint32) imm << 5) | reg);
		}

		private static uint32 branch (uint64 from, uint64 to) {
			return (uint32) (INSN_B_SELF | (((to - from) / 4) & 0x3ffffff));
		}

		private static uint32 peek_u32 (uint8[] bytes) {
			return *((uint32 *) bytes);
		}
#endif

		private Future<RemoteAgent> establish_connection (InjectSpec spec, string fallback_address, Cancellable? cancellable)
				throws Error {
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

		// Sample what the target runs and pick a libc function to re-hook. Prefer eBPF
		// stack sampling, falling back to /proc when it is unavailable — typically
		// because perf/eBPF demands privileges we lack. Returns 0 when nothing usable
		// turns up.
		private async uint64 discover_trigger (ProcMapsSnapshot maps, RegionLayout region, StackRendezvous rendezvous,
				uint64 mmap_impl, uint64 exclude, Cancellable? cancellable) throws Error, IOError {
			Gee.List<SampledStack> stacks;

			var sampler = new ActivitySampler (pid);
			try {
				sampler.start ();
				yield sleep_ms (SAMPLE_WINDOW_MS);
				sampler.stop ();
				stacks = sampler.stacks;
			} catch (Error e) {
				stacks = yield sample_threads_via_proc (maps, cancellable);
			}

			return hottest_libc_function (maps, region, rendezvous, mmap_impl, stacks, exclude);
		}

		private async Gee.List<SampledStack> sample_threads_via_proc (ProcMapsSnapshot maps, Cancellable? cancellable)
				throws Error, IOError {
			var stacks = new Gee.ArrayList<SampledStack> ();

			var timer = new Timer ();
			while (timer.elapsed () * 1000.0 < SAMPLE_WINDOW_MS) {
				cancellable.set_error_if_cancelled ();

				foreach (uint tid in enumerate_thread_ids ()) {
					uint64[] frames = read_thread_frames (maps, tid);
					if (frames.length != 0)
						stacks.add (new SampledStack (frames));
				}

				yield sleep_ms (REGION_POLL_INTERVAL_MS);
			}

			return stacks;
		}

		private Gee.List<uint> enumerate_thread_ids () {
			var tids = new Gee.ArrayList<uint> ();
			try {
				var dir = Dir.open ("/proc/%u/task".printf (pid));
				string? name;
				while ((name = dir.read_name ()) != null) {
					uint tid;
					if (uint.try_parse (name, out tid))
						tids.add (tid);
				}
			} catch (FileError e) {
			}
			return tids;
		}

		// The last two fields of /proc/<pid>/task/<tid>/syscall are the thread's user-space
		// stack pointer and PC. The PC lands in whatever libc routine issued the syscall;
		// once the target is multi-threaded that is a tiny arch wrapper too small to host
		// the stub, so also hand back a few stack words, where the caller's return address
		// points at a larger routine we can re-hook instead.
		private uint64[] read_thread_frames (ProcMapsSnapshot maps, uint tid) throws Error {
			string contents;
			try {
				FileUtils.get_contents ("/proc/%u/task/%u/syscall".printf (pid, tid), out contents);
			} catch (FileError e) {
				return {};
			}

			string[] fields = contents.strip ().split (" ");
			if (fields.length < 2)
				return {};

			uint64 pc;
			if (!uint64.try_parse (fields[fields.length - 1], out pc, null, 0) || pc == 0)
				return {};

			uint64 sp;
			var stack = (uint64.try_parse (fields[fields.length - 2], out sp, null, 0) && sp != 0)
				? maps.find_mapping (sp)
				: null;
			if (stack == null)
				return new uint64[] { pc };

			uint depth = uint.min (STACK_SCAN_DEPTH, (uint) ((stack.end - sp) / 8));
			var frames = new uint64[1 + depth];
			frames[0] = pc;
			for (uint i = 0; i != depth; i++)
				frames[1 + i] = mem.read_pointer (sp + (uint64) i * 8);
			return frames;
		}

		// Walk each sample from its leaf outward and vote for the first frame that lands in
		// a libc function big enough to hold the stub, then re-hook the hottest. Preferring
		// the leaf keeps us on the function actually running; falling through to the caller
		// rescues the cases where the leaf is a too-small wrapper (e.g. the syscall
		// cancellation arch shim a multi-threaded target parks in).
		private uint64 hottest_libc_function (ProcMapsSnapshot maps, RegionLayout region, StackRendezvous rendezvous,
				uint64 mmap_impl, Gee.List<SampledStack> stacks, uint64 exclude) throws Error {
			unowned Gum.Module libc = Gum.Process.get_libc_module ();
			var remote = maps.find_module_by_path (libc.path);
			var local = ProcMapsSnapshot.from_pid (Posix.getpid ()).find_module_by_path (libc.path);
			if (remote == null || local == null)
				return 0;

			var symbols = function_symbols (libc, local.start, remote.start);
			var exports = function_exports (libc, local.start, remote.start);

			uint64 min_bytes = trigger_stub_footprint (remote.start, region, rendezvous, mmap_impl);

			var votes = new Gee.HashMap<uint64?, uint> (Numeric.uint64_hash, Numeric.uint64_equal);
			foreach (var sample in stacks) {
				for (int depth = 0; depth != sample.frames.length; depth++) {
					uint64 frame = sample.frames[depth];
					var m = maps.find_mapping (frame);
					if (m == null || m.path != libc.path)
						continue;
					// The leaf is the live PC; deeper frames are raw stack words, so only
					// trust those that a call actually returns to rather than stray data.
					if (depth != 0 && !is_return_site (frame))
						continue;
					uint64 entry = resolve_function (frame, symbols, exports, min_bytes);
					if (entry == 0 || entry == exclude)
						continue;
					votes[entry] = votes[entry] + 1;
					break;
				}
			}

			uint64 best = 0;
			uint best_votes = 0;
			foreach (var vote in votes.entries) {
				if (vote.value > best_votes) {
					best_votes = vote.value;
					best = vote.key;
				}
			}
			return (best_votes >= MIN_TRIGGER_VOTES) ? best : 0;
		}

		private Gee.List<FunctionExtent> function_symbols (Gum.Module libc, uint64 local_base, uint64 remote_base) {
			var functions = new Gee.ArrayList<FunctionExtent> ();
			libc.enumerate_symbols (s => {
				if (s.size > 0 && s.address >= local_base)
					functions.add (new FunctionExtent (remote_base + ((uint64) s.address - local_base), (uint64) s.size));
				return true;
			});
			return sorted_by_entry (functions);
		}

		private Gee.List<FunctionExtent> function_exports (Gum.Module libc, uint64 local_base, uint64 remote_base) {
			var functions = new Gee.ArrayList<FunctionExtent> ();
			libc.enumerate_exports (e => {
				if (e.type == Gum.ExportType.FUNCTION && e.size > 0 && e.address >= local_base)
					functions.add (new FunctionExtent (remote_base + ((uint64) e.address - local_base), (uint64) e.size));
				return true;
			});
			return sorted_by_entry (functions);
		}

		private Gee.List<FunctionExtent> sorted_by_entry (Gee.List<FunctionExtent> functions) {
			functions.sort ((a, b) => (a.entry < b.entry) ? -1 : ((a.entry > b.entry) ? 1 : 0));
			return functions;
		}

		// Resolve the leaf to a function start in priority order: a sized symbol that
		// contains it, then a sized export, then — as a last resort — a scan out to the
		// nearest function boundary either side, which both finds the entry and sizes it.
		private uint64 resolve_function (uint64 leaf, Gee.List<FunctionExtent> symbols,
				Gee.List<FunctionExtent> exports, uint64 min_bytes) throws Error {
			var fn = containing_function (symbols, leaf);
			if (fn == null)
				fn = containing_function (exports, leaf);
			if (fn == null)
				fn = scan_function_extent (leaf);
			if (fn == null)
				return 0;
			return (fn.size >= min_bytes) ? fn.entry : 0;
		}

		// A real return address has a call right before it; data that merely happens to
		// point into the text does not, so this keeps stray stack words from being re-hooked.
		private bool is_return_site (uint64 address) throws Error {
#if X86 || X86_64
			return mem.read_memory (address - 5, 1)[0] == 0xe8;
#elif ARM64
			return (peek_u32 (mem.read_memory (address - 4, 4)) & 0xfc000000) == 0x94000000;
#else
			return false;
#endif
		}

		// The trigger is overwritten in place, so a candidate is only usable if it is at
		// least as large as the bootstrap stub we would stamp into it. Build that stub for
		// the first-injection case (no region to reuse, the larger of the two paths) and
		// measure it, rather than guessing a fixed floor.
		private uint64 trigger_stub_footprint (uint64 sample_target, RegionLayout region, StackRendezvous rendezvous,
				uint64 mmap_impl) throws Error {
			uint8[] stub = build_trigger_stub (sample_target, rendezvous.cas, mmap_impl, 0, region.total,
				region.entry_offset);
			return BOOTSTRAP_OFFSET + stub.length;
		}

		private FunctionExtent? containing_function (Gee.List<FunctionExtent> functions, uint64 ip) {
			int lo = 0;
			int hi = functions.size - 1;
			FunctionExtent? found = null;
			while (lo <= hi) {
				int mid = (lo + hi) / 2;
				var fn = functions[mid];
				if (fn.entry <= ip) {
					found = fn;
					lo = mid + 1;
				} else {
					hi = mid - 1;
				}
			}
			return (found != null && ip < found.entry + found.size) ? found : null;
		}

		// Bound the leaf's function without symbols, by scanning out to the nearest
		// function boundary either side. A boundary is a prologue (paciasp on arm64, an
		// endbr/frame setup on x86) or, on arm64, a ret followed by alignment padding —
		// padding is what tells a function's terminating ret apart from an early return.
		private FunctionExtent? scan_function_extent (uint64 leaf) throws Error {
			size_t window = 4096;
			uint64 lo = leaf - window;
			uint8[] before = mem.read_memory (lo, window);
			uint8[] after = mem.read_memory (leaf, window);

			uint64 entry = 0;
			for (int off = (int) window - 8; off >= 0; off -= SCAN_STEP) {
				if (function_boundary (before, off, window)) {
					entry = lo + boundary_address (before, off, window);
					break;
				}
			}
			if (entry == 0)
				return null;

			uint64 end = 0;
			for (int off = SCAN_STEP; off <= (int) window - 8; off += SCAN_STEP) {
				if (function_boundary (after, off, window)) {
					end = leaf + boundary_address (after, off, window);
					break;
				}
			}
			if (end == 0)
				return null;

			return new FunctionExtent (entry, end - entry);
		}

		private bool function_boundary (uint8[] code, int off, size_t window) {
#if ARM64
			uint32 insn = arm64_insn (code, off);
			if (insn == 0xd503233f || insn == 0xd503237f) // paciasp / pacibsp
				return true;
			return (insn & 0xfffffc1f) == 0xd65f0000 && is_padding (arm64_insn (code, off + 4)); // ret then padding
#elif X86_64
			return code[off] == 0xf3 && code[off + 1] == 0x0f && code[off + 2] == 0x1e && code[off + 3] == 0xfa;
#elif X86
			return code[off] == 0xf3 && code[off + 1] == 0x0f && code[off + 2] == 0x1e && code[off + 3] == 0xfb;
#else
			return false;
#endif
		}

		// The entry the boundary marks: a prologue is itself the start; a terminating ret
		// hands the start to the next function, past the padding.
		private int boundary_address (uint8[] code, int off, size_t window) {
#if ARM64
			if (arm64_insn (code, off) == 0xd503233f || arm64_insn (code, off) == 0xd503237f)
				return off;
			int e = off + 4;
			while (e <= (int) window - 4 && is_padding (arm64_insn (code, e)))
				e += 4;
			return e;
#else
			return off;
#endif
		}

#if ARM64
		private uint32 arm64_insn (uint8[] code, int off) {
			return code[off] | ((uint32) code[off + 1] << 8) | ((uint32) code[off + 2] << 16)
				| ((uint32) code[off + 3] << 24);
		}

		private bool is_padding (uint32 insn) {
			return insn == 0xd503201f || insn == 0x00000000; // nop / zero fill
		}
#endif

		private async void sleep_ms (uint ms) {
			var source = new TimeoutSource (ms);
			source.set_callback (() => {
				sleep_ms.callback ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());
			yield;
			source.destroy ();
		}

		private async RemoteAgent await_agent (Future<RemoteAgent> future_agent, Cancellable? cancellable)
				throws Error, IOError {
			var trigger_cancellable = new Cancellable ();
			var main_context = MainContext.get_thread_default ();

			var timeout_source = new TimeoutSource.seconds (TRIGGER_TIMEOUT_SECONDS);
			timeout_source.set_callback (() => {
				trigger_cancellable.cancel ();
				return Source.REMOVE;
			});
			timeout_source.attach (main_context);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				trigger_cancellable.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (main_context);

			RemoteAgent? agent = null;
			try {
				agent = yield future_agent.wait_async (trigger_cancellable);
			} catch (IOError e) {
				cancellable.set_error_if_cancelled ();
				throw new Error.PROCESS_NOT_RESPONDING (
					"Timed out waiting for the injected agent to connect back");
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

	private sealed class AcquiredRegion {
		public uint64 region_base;
		public uint64 target;
		public uint8[] original;

		public AcquiredRegion (uint64 region_base, uint64 target, owned uint8[] original) {
			this.region_base = region_base;
			this.target = target;
			this.original = (owned) original;
		}
	}

	private sealed class FunctionExtent {
		public uint64 entry;
		public uint64 size;

		public FunctionExtent (uint64 entry, uint64 size) {
			this.entry = entry;
			this.size = size;
		}
	}

	private sealed class ProcMemSession : Object {
		public uint pid {
			get;
			construct;
		}

		private FileDescriptor mem;

		private ProcMemSession (uint pid, FileDescriptor mem) {
			Object (pid: pid);
			this.mem = mem;
		}

		public static ProcMemSession open (uint pid) throws Error {
			int fd = Posix.open ("/proc/%u/mem".printf (pid), Posix.O_RDWR);
			if (fd == -1)
				throw new Error.PERMISSION_DENIED ("Unable to access /proc/%u/mem: %s", pid, strerror (errno));
			return new ProcMemSession (pid, new FileDescriptor (fd));
		}

		public uint8[] read_memory (uint64 address, size_t size) throws Error {
			var result = new uint8[size];

			size_t offset = 0;
			while (offset != size) {
				ssize_t n = Posix.pread (mem.handle, (uint8 *) result + offset, size - offset,
					(Posix.off_t) (address + offset));
				if (n <= 0)
					throw new Error.NOT_SUPPORTED ("Unable to read remote memory at 0x%" + uint64.FORMAT_MODIFIER + "x",
						address + offset);
				offset += n;
			}

			return result;
		}

		public uint64 read_pointer (uint64 address) throws Error {
			uint8[] raw = read_memory (address, sizeof (uint64));
			return *((uint64 *) raw);
		}

		public void write_memory (uint64 address, uint8[] data) throws Error {
			size_t size = data.length;
			size_t offset = 0;
			while (offset != size) {
				ssize_t n = Posix.pwrite (mem.handle, (uint8 *) data + offset, size - offset,
					(Posix.off_t) (address + offset));
				if (n <= 0)
					throw new Error.NOT_SUPPORTED ("Unable to write remote memory at 0x%" + uint64.FORMAT_MODIFIER + "x",
						address + offset);
				offset += n;
			}
		}

#if ARM64
		public void write_instruction (uint64 address, uint32 insn) throws Error {
			ssize_t n = Posix.pwrite (mem.handle, &insn, sizeof (uint32), (Posix.off_t) address);
			if (n != sizeof (uint32))
				throw new Error.NOT_SUPPORTED ("Unable to perform aligned instruction write at 0x%"
					+ uint64.FORMAT_MODIFIER + "x", address);
		}
#endif

		public void write_u32 (uint64 address, uint32 value) throws Error {
			ssize_t n = Posix.pwrite (mem.handle, &value, sizeof (uint32), (Posix.off_t) address);
			if (n != sizeof (uint32))
				throw new Error.NOT_SUPPORTED ("Unable to write word at 0x%" + uint64.FORMAT_MODIFIER + "x", address);
		}
	}

	private sealed class RemoteLibcApi {
		public HelperLibcApi table;

		private static Gee.Map<string, Symbol> symbols;

		private class Symbol {
			public string module_path;
			public uint64 offset;

			public Symbol (string module_path, uint64 offset) {
				this.module_path = module_path;
				this.offset = offset;
			}
		}

		private const string[] LIBC_SYMBOLS = {
			"malloc",
			"sprintf",
			"mmap",
			"mprotect",
			"munmap",
			"socket",
			"socketpair",
			"connect",
			"recvmsg",
			"send",
			"fcntl",
			"close",
			"pthread_create",
			"pthread_detach",
		};

		static construct {
			symbols = new Gee.HashMap<string, Symbol> ();

			var local_maps = ProcMapsSnapshot.from_pid (Posix.getpid ());

			var libc = Gum.Process.get_libc_module ();
			var libc_mapping = local_maps.find_module_by_path (libc.path);
			foreach (unowned string name in LIBC_SYMBOLS) {
				Gum.Address addr = libc.find_export_by_name (name);
				if (addr != 0 && libc_mapping != null)
					symbols[name] = new Symbol (libc.path, (uint64) addr - libc_mapping.start);
			}

			register_by_address (local_maps, "dlopen", (uint64) (uintptr) dlopen);
			register_by_address (local_maps, "dlclose", (uint64) (uintptr) dlclose);
			register_by_address (local_maps, "dlsym", (uint64) (uintptr) dlsym);
			register_by_address (local_maps, "dlerror", (uint64) (uintptr) dlerror);
		}

		private static void register_by_address (ProcMapsSnapshot local_maps, string name, uint64 local_addr) {
			var module = Gum.Process.find_module_by_address ((Gum.Address) local_addr);
			if (module == null)
				return;
			var mapping = local_maps.find_module_by_path (module.path);
			if (mapping == null)
				return;
			symbols[name] = new Symbol (module.path, local_addr - mapping.start);
		}

		public static RemoteLibcApi resolve (ProcMapsSnapshot remote_maps) throws Error {
			var api = new RemoteLibcApi ();
			api.table = HelperLibcApi ();

			api.table.sprintf = resolve_one (remote_maps, "sprintf");
			api.table.mmap = resolve_one (remote_maps, "mmap");
			api.table.munmap = resolve_one (remote_maps, "munmap");
			api.table.socket = resolve_one (remote_maps, "socket");
			api.table.socketpair = resolve_one (remote_maps, "socketpair");
			api.table.connect = resolve_one (remote_maps, "connect");
			api.table.recvmsg = resolve_one (remote_maps, "recvmsg");
			api.table.send = resolve_one (remote_maps, "send");
			api.table.fcntl = resolve_one (remote_maps, "fcntl");
			api.table.close = resolve_one (remote_maps, "close");
			api.table.pthread_create = resolve_one (remote_maps, "pthread_create");
			api.table.pthread_detach = resolve_one (remote_maps, "pthread_detach");

			api.table.dlopen = resolve_one (remote_maps, "dlopen");
			api.table.dlopen_flags = RTLD_LAZY;
			api.table.dlclose = resolve_one (remote_maps, "dlclose");
			api.table.dlsym = resolve_one (remote_maps, "dlsym");
			api.table.dlerror = resolve_one (remote_maps, "dlerror");

			return api;
		}

		public static uint64 resolve_export (ProcMapsSnapshot remote_maps, string name) throws Error {
			return (uint64) (uintptr) resolve_one (remote_maps, name);
		}

		private static void * resolve_one (ProcMapsSnapshot remote_maps, string name) throws Error {
			Symbol? symbol = symbols[name];
			if (symbol == null)
				throw new Error.NOT_SUPPORTED ("Unable to resolve %s in this process", name);

			var mapping = remote_maps.find_module_by_path (symbol.module_path);
			if (mapping == null)
				throw new Error.NOT_SUPPORTED ("Unable to locate %s in target process", symbol.module_path);

			return (void *) (mapping.start + symbol.offset);
		}

		private const int RTLD_LAZY = 1;
	}
}
