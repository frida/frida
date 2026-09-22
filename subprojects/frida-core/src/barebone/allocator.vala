[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public interface Allocator : Object {
		public abstract size_t page_size {
			get;
		}

		public abstract Gum.PageProtection granted_protection ();

		public abstract async Allocation allocate (size_t size, size_t alignment, Cancellable? cancellable)
			throws Error, IOError;
	}

	public interface Allocation : Object {
		public abstract uint64 virtual_address {
			get;
		}

		public abstract size_t size {
			get;
		}

		public abstract async void deallocate (Cancellable? cancellable) throws Error, IOError;
	}

	internal async void protect_unless_already (Machine machine, Allocator allocator, uint64 address, size_t size,
			Gum.PageProtection prot, Cancellable? cancellable) throws Error, IOError {
		if ((allocator.granted_protection () & prot) == prot)
			return;

		yield machine.protect_pages (address, size, prot, cancellable);
	}

	// This gives memory that the caller reserved before. Injection stops the guest to write, thus
	// all work that needs the guest must occur first.
	public sealed class FixedAllocator : Object, Allocator {
		public Gum.PageProtection granted_protection () {
			return READ | WRITE;
		}

		public size_t page_size {
			get {
				return _page_size;
			}
		}

		private uint64 address;
		private size_t _size;
		private size_t _page_size;

		public FixedAllocator (uint64 address, size_t size, size_t page_size) {
			this.address = address;
			this._size = size;
			this._page_size = page_size;
		}

		public async Allocation allocate (size_t size, size_t alignment, Cancellable? cancellable)
				throws Error, IOError {
			if (size > _size)
				throw new Error.NOT_SUPPORTED ("Reserved region is too small");

			return new FixedAllocation (address, size);
		}
	}

	private sealed class FixedAllocation : Object, Allocation {
		public uint64 virtual_address {
			get {
				return _virtual_address;
			}
		}

		public size_t size {
			get {
				return _size;
			}
		}

		private uint64 _virtual_address;
		private size_t _size;

		public FixedAllocation (uint64 virtual_address, size_t size) {
			this._virtual_address = virtual_address;
			this._size = size;
		}

		public async void deallocate (Cancellable? cancellable) throws Error, IOError {
		}
	}

	public sealed class NullAllocator : Object, Allocator {
		public Gum.PageProtection granted_protection () {
			return READ | WRITE;
		}

		public size_t page_size {
			get {
				return _page_size;
			}
		}

		private size_t _page_size;

		public NullAllocator (size_t page_size) {
			this._page_size = page_size;
		}

		public async Allocation allocate (size_t size, size_t alignment, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("To enable this feature, specify an allocator in your FRIDA_BAREBONE_CONFIG");
		}
	}

	public sealed class PhysicalAllocator : Object, Allocator {
		public Gum.PageProtection granted_protection () {
			return READ | WRITE;
		}

		public size_t page_size {
			get {
				return _page_size;
			}
		}

		private Machine machine;
		private size_t _page_size;

		private uint64 cursor;

		public PhysicalAllocator (Machine machine, size_t page_size, BarebonePhysicalAllocatorConfig config) {
			this.machine = machine;
			this._page_size = page_size;
			this.cursor = config.physical_base.address;
		}

		public async Allocation allocate (size_t size, size_t alignment, Cancellable? cancellable) throws Error, IOError {
			uint64 address_pa = cursor;

			size_t vm_size = round_size_up (size, _page_size);
			cursor += vm_size;

			uint num_pages = (uint) (vm_size / _page_size);

			var physical_addresses = new Gee.ArrayList<uint64?> ();
			for (uint i = 0; i != num_pages; i++)
				physical_addresses.add (address_pa + (i * _page_size));

			Allocation page_allocation = yield machine.allocate_pages (physical_addresses, cancellable);

			return new PhysicalAllocation (page_allocation);
		}

		private class PhysicalAllocation : Object, Allocation {
			public uint64 virtual_address {
				get {
					return page_allocation.virtual_address;
				}
			}

			public size_t size {
				get {
					return page_allocation.size;
				}
			}

			private Allocation page_allocation;

			public PhysicalAllocation (Allocation allocation) {
				page_allocation = allocation;
			}

			public async void deallocate (Cancellable? cancellable) throws Error, IOError {
				// TODO: Add to freelist.
				yield page_allocation.deallocate (cancellable);
			}
		}
	}

	public sealed class TargetFunctionsAllocator : Object, Allocator {
		public Gum.PageProtection granted_protection () {
			return _protection;
		}

		public size_t page_size {
			get {
				return _page_size;
			}
		}

		private Machine machine;
		private size_t _page_size;
		private Gum.PageProtection _protection;
		private uint64 alloc_function;
		private uint64 free_function;
		private Gee.List<BareboneCallArgument> alloc_arguments;
		private Gee.List<BareboneCallArgument> free_arguments;

		public TargetFunctionsAllocator (Machine machine, size_t page_size, Gum.PageProtection protection,
				BareboneTargetFunctionsAllocatorConfig config, uint64 alloc_function, uint64 free_function) {
			this.machine = machine;
			this._page_size = page_size;
			this._protection = protection;
			this.alloc_function = alloc_function;
			this.free_function = free_function;
			this.alloc_arguments = config._effective_alloc_arguments ();
			this.free_arguments = config._effective_free_arguments ();
		}

		// A kernel allocator gives blocks with an alignment of less than a page, and the caller can
		// change the protection and write whole pages. Thus allocate enough for every page the
		// result spans, give out full pages, and keep the start of the block for the release.
		public async Allocation allocate (size_t size, size_t alignment, Cancellable? cancellable) throws Error, IOError {
			size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
			size_t padded_size = aligned_size + alignment - 1;

			uint64 block = yield machine.invoke (alloc_function,
				resolve_arguments (alloc_arguments, padded_size, 0), cancellable);
			if (block == 0)
				throw new Error.NOT_SUPPORTED ("Unable to allocate %zu bytes in the target", padded_size);

			uint64 address = (block + alignment - 1) & ~((uint64) alignment - 1);

			return new TargetAllocation (address, size, block, padded_size, machine, free_function,
				free_arguments);
		}

		private static uint64[] resolve_arguments (Gee.List<BareboneCallArgument> template, size_t size, uint64 address) {
			var arguments = new uint64[template.size];
			for (int i = 0; i != arguments.length; i++) {
				BareboneCallArgument a = template[i];
				switch (a.role) {
					case SIZE:
						arguments[i] = size;
						break;
					case ADDRESS:
						arguments[i] = address;
						break;
					default:
						arguments[i] = a.value;
						break;
				}
			}
			return arguments;
		}

		private class TargetAllocation : Object, Allocation {
			public uint64 virtual_address {
				get {
					return _virtual_address;
				}
			}

			public size_t size {
				get {
					return _size;
				}
			}

			private uint64 _virtual_address;
			public size_t _size;
			private uint64 block;
			private size_t block_size;
			private Machine machine;
			private uint64 free_function;
			private Gee.List<BareboneCallArgument> free_arguments;

			public TargetAllocation (uint64 address, size_t size, uint64 block, size_t block_size, Machine m,
					uint64 free_function, Gee.List<BareboneCallArgument> free_arguments) {
				_virtual_address = address;
				_size = size;
				this.block = block;
				this.block_size = block_size;
				machine = m;
				this.free_function = free_function;
				this.free_arguments = free_arguments;
			}

			public async void deallocate (Cancellable? cancellable) throws Error, IOError {
				yield machine.invoke (free_function,
					resolve_arguments (free_arguments, block_size, block), cancellable);
			}
		}
	}
}
