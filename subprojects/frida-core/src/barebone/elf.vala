[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public async Allocation inject_elf (Gum.ElfModule elf, Bytes raw_elf, size_t page_size, Machine machine, Allocator allocator,
			owned UploadProgressFunc on_upload_progress, Cancellable? cancellable) throws Error, IOError {
		size_t vm_size = (size_t) elf.mapped_size;

		uint num_pages = (uint) (vm_size / page_size);
		if (vm_size % page_size != 0)
			num_pages++;

		uint64 text_base = 0;
		size_t text_size = 0;
		uint64 data_base = vm_size;
		elf.enumerate_segments (s => {
			if ((s.protection & Gum.PageProtection.EXECUTE) != 0 && text_size == 0) {
				text_base = s.vm_address;
				text_size = (size_t) s.vm_size;
			} else if ((s.protection & Gum.PageProtection.WRITE) != 0) {
				data_base = uint64.min (data_base, s.vm_address);
			}
			return true;
		});
		if (text_size == 0)
			throw new Error.NOT_SUPPORTED ("Unable to detect text segment");

		var allocation = yield allocator.allocate (num_pages * page_size, page_size, cancellable);
		try {
			uint64 base_va = allocation.virtual_address;

			Bytes relocated_image = machine.relocate (elf, raw_elf, base_va);
			yield upload (machine, base_va, relocated_image, (owned) on_upload_progress, cancellable);

			yield clear_tail_beyond_file (machine, base_va, relocated_image.get_size (),
				num_pages * page_size, cancellable);

			yield protect_unless_already (machine, allocator, base_va + text_base, text_size,
				READ | EXECUTE, cancellable);
			yield protect_unless_already (machine, allocator, base_va + data_base,
				(num_pages * page_size) - (size_t) data_base, READ | WRITE, cancellable);
		} catch (GLib.Error e) {
			yield allocation.deallocate (cancellable);
			throw_api_error (e);
		}

		return allocation;
	}

	private async void upload (Machine machine, uint64 va, Bytes image, owned UploadProgressFunc on_progress,
			Cancellable? cancellable) throws Error, IOError {
		unowned uint8[] data = image.get_data ();
		size_t slice_size = size_t.max (data.length / UPLOAD_PROGRESS_STEPS, MIN_UPLOAD_SLICE);

		size_t offset = 0;
		while (offset != data.length) {
			size_t slice = size_t.min (slice_size, data.length - offset);
			yield machine.write_virtual (va + offset, data[offset : offset + slice], cancellable);
			offset += slice;
			on_progress ((double) offset / data.length);
		}
	}

	public delegate void UploadProgressFunc (double uploaded);

	private const uint UPLOAD_PROGRESS_STEPS = 64;
	private const size_t MIN_UPLOAD_SLICE = 256 * 1024;

	private async void clear_tail_beyond_file (Machine machine, uint64 base_va, size_t written,
			size_t mapped, Cancellable? cancellable) throws Error, IOError {
		if (written >= mapped)
			return;

		var blank = new uint8[mapped - written];
		yield machine.write_virtual (base_va + written, blank, cancellable);
	}
}
