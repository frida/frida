[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public interface PhysicalMemory : Object {
		public abstract uint8[] read (uint64 pa, size_t size) throws Error;
		public abstract void write (uint64 pa, uint8[] data) throws Error;
		public abstract bool contains (uint64 pa);
	}

	public interface Machine : Object {
		public abstract GDB.Client gdb {
			get;
			set;
		}

		public abstract string llvm_target {
			get;
		}

		public abstract string llvm_code_model {
			get;
		}

		public abstract async size_t query_page_size (Cancellable? cancellable) throws Error, IOError;

		public virtual async Bytes read_virtual (uint64 va, size_t size, Cancellable? cancellable)
				throws Error, IOError {
			return yield gdb.read_byte_array (va, size, cancellable);
		}

		public virtual async void write_virtual (uint64 va, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			yield gdb.write_byte_array (va, new Bytes (data), cancellable);
		}

		public abstract async uint query_exception_level (Cancellable? cancellable) throws Error, IOError;

		public async void enter_exception_level (uint level, uint timeout, Cancellable? cancellable) throws Error, IOError {
			var timer = new Timer ();

			do {
				var el = yield query_exception_level (cancellable);
				if (el == level)
					return;

				yield gdb.continue (cancellable);

				var source = new TimeoutSource (10);
				source.set_callback (enter_exception_level.callback);
				source.attach (MainContext.get_thread_default ());
				yield;

				yield gdb.stop (cancellable);
			} while ((uint) (timer.elapsed () * 1000.0) < timeout);

			throw new Error.TIMED_OUT ("Timed out while trying to get target to exception level %u", level);
		}

		public abstract async void enumerate_ranges (Gum.PageProtection prot, FoundRangeFunc func, Cancellable? cancellable)
			throws Error, IOError;

		public virtual async uint64 translate_address (uint64 va, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Address translation is not implemented for this architecture");
		}

		public abstract async Allocation allocate_pages (Gee.List<uint64?> physical_addresses, Cancellable? cancellable)
			throws Error, IOError;

		public virtual async void protect_pages_leaving_the_flush (uint64 virtual_address, size_t size,
				Gum.PageProtection prot, Cancellable? cancellable) throws Error, IOError {
			yield protect_pages (virtual_address, size, prot, cancellable);
		}

		public abstract async void protect_pages (uint64 virtual_address, size_t size, Gum.PageProtection prot,
			Cancellable? cancellable) throws Error, IOError;

		public abstract async Gee.List<uint64?> scan_ranges (Gee.List<Gum.MemoryRange?> ranges, MatchPattern pattern,
			uint max_matches, Cancellable? cancellable) throws Error, IOError;

		public Bytes relocate (Gum.ElfModule elf, Bytes raw_elf, uint64 base_va) throws Error {
			uint64 file_start = uint64.MAX;
			uint64 file_end = 0;
			elf.enumerate_segments (s => {
				if (s.file_size != 0) {
					file_start = uint64.min (s.file_offset, file_start);
					file_end = uint64.max (s.file_offset + s.file_size, file_end);
				}
				return true;
			});

			// A position-independent image contains its relocations two times: the relocations from the
			// build, which --emit-relocs keeps, and the dynamic relocations for a loader. Only the
			// second set covers the tables that the linker made, thus apply only that set.
			bool position_independent = elf.etype == Gum.ElfType.DYN;

			var relocated_buf = gdb.make_buffer (new Bytes (raw_elf[(size_t) file_start:(size_t) file_end].get_data ()));
			Error? pending_error = null;
			elf.enumerate_relocations (r => {
				unowned string parent_section = (r.parent != null) ? r.parent.name : "";
				if (parent_section.has_prefix (".rela.debug_"))
					return true;
				if (position_independent && applied_by_a_loader (parent_section))
					return true;
				if (parent_section == ".rela.text" && !relocates_text ())
					return true;

				try {
					apply_relocation (r, base_va, relocated_buf);
				} catch (Error e) {
					pending_error = e;
					return false;
				}

				return true;
			});
			if (pending_error != null)
				throw pending_error;

			Bytes relocated_bytes = relocated_buf.bytes;
			Bytes relocated_image = gdb.make_buffer_builder ()
				.append_bytes (relocated_bytes)
				.skip ((size_t) (elf.mapped_size - relocated_bytes.get_size ()))
				.build ();
			return relocated_image;
		}

		private static bool applied_by_a_loader (string section) {
			return section != ".rela.dyn" && section != ".rel.dyn";
		}

		public abstract void apply_relocation (Gum.ElfRelocationDetails r, uint64 base_va, Buffer relocated) throws Error;

		// Code that uses displacements in the instructions needs no relocation, and its relocation
		// section holds types that this code does not know. Code that uses absolute addresses needs
		// all of them.
		public virtual bool relocates_text () {
			return false;
		}

		public abstract async uint64 invoke (uint64 impl, uint64[] args, Cancellable? cancellable) throws Error, IOError;

		public abstract async CallFrame load_call_frame (GDB.Thread thread, uint arity, Cancellable? cancellable)
			throws Error, IOError;

		public abstract uint64 address_from_funcptr (uint64 ptr);
		public abstract size_t breakpoint_size_from_funcptr (uint64 ptr);

		public abstract async InlineHook create_inline_hook (uint64 target, uint64 handler, Allocator allocator,
			Cancellable? cancellable) throws Error, IOError;
	}

	public delegate bool FoundRangeFunc (RangeDetails details);

	public class RangeDetails {
		public uint64 base_va;
		public uint64 base_pa;
		public uint64 size;
		public Gum.PageProtection protection;
		public MappingType type;

		public uint64 end {
			get { return base_va + size; }
		}

		public RangeDetails (uint64 base_va, uint64 base_pa, uint64 size, Gum.PageProtection protection, MappingType type) {
			this.base_va = base_va;
			this.base_pa = base_pa;
			this.size = size;
			this.protection = protection;
			this.type = type;
		}

		public RangeDetails clone () {
			return new RangeDetails (base_va, base_pa, size, protection, type);
		}

		public bool contains_virtual_address (uint64 va) {
			return va >= base_va && va < base_va + size;
		}

		public bool contains_physical_address (uint64 pa) {
			return pa >= base_pa && pa < base_pa + size;
		}

		public uint64 virtual_to_physical (uint64 va) {
			assert (contains_virtual_address (va));
			return base_pa + (va - base_va);
		}

		public uint64 physical_to_virtual (uint64 pa) {
			assert (contains_physical_address (pa));
			return base_va + (pa - base_pa);
		}
	}

	public enum MappingType {
		UNKNOWN,
		MEMORY,
		DEVICE;

		public string to_nick () {
			return Marshal.enum_to_nick<MappingType> (this);
		}
	}

	public interface CallFrame : Object {
		public abstract uint64 return_address {
			get;
		}

		public abstract Gee.Map<string, Variant> registers {
			get;
		}

		public abstract uint64 get_nth_argument (uint n);
		public abstract void replace_nth_argument (uint n, uint64 val);
		public abstract uint64 get_return_value ();
		public abstract void replace_return_value (uint64 retval);

		public abstract void force_return ();

		public abstract async void commit (Cancellable? cancellable) throws Error, IOError;
	}

	public interface InlineHook : Object {
		public abstract async void destroy (Cancellable? cancellable) throws Error, IOError;
		public abstract async void enable (Cancellable? cancellable) throws Error, IOError;
		public abstract async void disable (Cancellable? cancellable) throws Error, IOError;
	}

	internal enum AddressingMode {
		VIRTUAL,
		PHYSICAL
	}

	// Restoring the mode must be awaited rather than fired and forgotten: a reply left in flight
	// arrives while the next command is pending, and that command consumes it instead — leaving
	// its caller with an empty response and the stub still in the old mode. Vala rejects a yield
	// inside a finally block, so callers hold on to any failure across the restore.
	internal static async bool halt_guest (GDB.Client gdb, Cancellable? cancellable) throws Error, IOError {
		bool was_running = gdb.state != STOPPED;
		if (was_running)
			yield gdb.stop (cancellable);

		return was_running;
	}

	internal static async void resume_guest (GDB.Client gdb, bool was_running, Cancellable? cancellable)
			throws Error, IOError {
		if (was_running)
			yield gdb.continue (cancellable);
	}

	internal static async bool enter_physical_addressing (GDB.Client gdb, Cancellable? cancellable)
			throws Error, IOError {
		bool was_running = yield halt_guest (gdb, cancellable);

		yield set_addressing_mode (gdb, PHYSICAL, cancellable);

		return was_running;
	}

	internal static async void leave_physical_addressing (GDB.Client gdb, bool was_running, Cancellable? cancellable)
			throws Error, IOError {
		yield set_addressing_mode (gdb, VIRTUAL, cancellable);

		yield resume_guest (gdb, was_running, cancellable);
	}

	internal static async void set_addressing_mode (GDB.Client gdb, AddressingMode mode, Cancellable? cancellable)
			throws Error, IOError {
		Gee.Set<string> features = gdb.features;
		string enabled = (mode == PHYSICAL) ? "1" : "0";
		if ("qemu-phy-mem-mode" in features)
			yield gdb.execute_simple ("Qqemu.PhyMemMode:" + enabled, cancellable);
		else if ("vf-phy-mem-mode" in features)
			yield gdb.execute_simple ("Qvf.PhyMemMode:" + enabled, cancellable);
		else
			throw new Error.NOT_SUPPORTED ("Unsupported GDB remote stub; please file a bug");
	}

	internal static void throw_if_failed (GLib.Error? failure) throws Error, IOError {
		if (failure == null)
			return;
		if (failure is IOError)
			throw (IOError) failure;
		throw (Error) failure;
	}

	internal static Gee.List<RangeDetails> coalesce_ranges (Gee.List<RangeDetails> ranges) {
		var result = new Gee.ArrayList<RangeDetails> ();

		RangeDetails? pending = null;
		foreach (RangeDetails r in ranges) {
			if (pending == null) {
				pending = r.clone ();
				continue;
			}

			if (r.base_va == pending.base_va + pending.size &&
					r.base_pa == pending.base_pa + pending.size &&
					r.protection == pending.protection &&
					r.type == pending.type) {
				pending.size += r.size;
				continue;
			}

			result.add (pending);
			pending = r.clone ();
		}
		if (pending != null)
			result.add (pending);

		return result;
	}

	internal static uint64 round_address_up (uint64 address, size_t n) {
		return (address + n - 1) & ~((uint64) n - 1);
	}

	internal static size_t round_size_up (size_t size, size_t n) {
		return (size + n - 1) & ~(n - 1);
	}

	internal static uint64 page_start (uint64 address, size_t page_size) {
		return address & ~((uint64) page_size - 1);
	}
}
