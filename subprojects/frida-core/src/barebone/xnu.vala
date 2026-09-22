[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	/// Where a process keeps its number and its name. XNU ships no type information and its
	/// small accessors are inlined away, so the two are found by asking the kernel for its
	/// processes and looking at what they hold: one of them is launchd, and launchd says so.
	public sealed class XnuLayout : Object {
		public uint64 number_offset;
		public uint64 name_offset;
	}

	public static async XnuLayout? collect_xnu_layout (Machine machine, uint64 iterate_processes,
			uint64 find_process, uint64 release_process, Allocator allocator,
			Cancellable? cancellable) throws Error, IOError {
		var processes = (iterate_processes != 0)
			? yield each_process (machine, iterate_processes, allocator, cancellable)
			: yield each_numbered_process (machine, find_process, release_process, cancellable);
		if (processes.size < 2)
			return null;

		var pages = new Gee.HashMap<uint64?, Bytes> (Numeric.uint64_hash, Numeric.uint64_equal);
		foreach (uint64 process in processes)
			pages[process] = yield machine.gdb.read_byte_array (process, (size_t) PROCESS_HEAD_SIZE, cancellable);

		uint64? number_offset = where_the_number_is (processes, pages);
		if (number_offset == null)
			return null;

		uint64? name_offset = where_the_name_is (processes, pages, number_offset);
		if (name_offset == null)
			return null;

		return new XnuLayout () {
			number_offset = number_offset,
			name_offset = name_offset,
		};
	}

	private static uint64? where_the_number_is (Gee.List<uint64?> processes,
			Gee.Map<uint64?, Bytes> pages) {
		for (uint64 offset = 0; offset != PROCESS_HEAD_SIZE - 4; offset += 4) {
			var seen = new Gee.HashSet<uint> ();
			bool launchd_is_here = false;
			bool plausible = true;

			foreach (uint64 process in processes) {
				uint number = read_uint32 (pages[process], offset);
				if (number > HIGHEST_PROCESS_NUMBER || !seen.add (number)) {
					plausible = false;
					break;
				}
				if (number == LAUNCHD)
					launchd_is_here = true;
			}

			if (plausible && launchd_is_here)
				return offset;
		}

		return null;
	}

	private static uint64? where_the_name_is (Gee.List<uint64?> processes,
			Gee.Map<uint64?, Bytes> pages, uint64 number_offset) {
		uint64? launchd = null;
		foreach (uint64 process in processes) {
			if (read_uint32 (pages[process], number_offset) == LAUNCHD) {
				launchd = process;
				break;
			}
		}
		if (launchd == null)
			return null;

		Bytes said = pages[launchd];
		for (uint64 offset = 0; offset != PROCESS_HEAD_SIZE - LAUNCHD_NAME.length; offset++) {
			if (reads_as (said, offset, LAUNCHD_NAME))
				return offset;
		}

		return null;
	}

	private static async Gee.List<uint64?> each_process (Machine machine, uint64 iterate_processes,
			Allocator allocator, Cancellable? cancellable) throws Error, IOError {
		var found = new Gee.ArrayList<uint64?> ();

		var landing = yield allocator.allocate (LANDING_SIZE, LANDING_SIZE, cancellable);
		var handler = new ProcessNoted (found);
		var callback = yield new Callback (landing.virtual_address, handler, machine, cancellable);

		try {
			yield machine.invoke (iterate_processes, {
				ALL_PROCESSES,
				landing.virtual_address,
				0,
				0,
				0
			}, cancellable);
		} finally {
			callback.destroy.begin (cancellable);
			landing.deallocate.begin (cancellable);
		}

		return found;
	}

	private static async Gee.List<uint64?> each_numbered_process (Machine machine, uint64 find_process,
			uint64 release_process, Cancellable? cancellable) throws Error, IOError {
		var found = new Gee.ArrayList<uint64?> ();
		if (find_process == 0 || release_process == 0)
			return found;

		for (uint64 number = 1; number != FIRST_PROCESS_NUMBERS && found.size != ENOUGH_PROCESSES;
				number++) {
			uint64 process = yield machine.invoke (find_process, { number }, cancellable);
			if (process == 0)
				continue;

			found.add (process);

			yield machine.invoke (release_process, { process }, cancellable);
		}

		return found;
	}

	private sealed class ProcessNoted : Object, CallbackHandler {
		public uint arity {
			get { return 2; }
		}

		private Gee.List<uint64?> found;

		public ProcessNoted (Gee.List<uint64?> found) {
			this.found = found;
		}

		public async uint64 handle_invocation (uint64[] args, CallFrame frame, Cancellable? cancellable)
				throws Error, IOError {
			if (found.size < ENOUGH_PROCESSES)
				found.add (args[0]);

			return KEEP_GOING;
		}
	}

	private static uint read_uint32 (Bytes bytes, uint64 offset) {
		unowned uint8[] data = bytes.get_data ();
		return data[offset]
			| ((uint) data[offset + 1] << 8)
			| ((uint) data[offset + 2] << 16)
			| ((uint) data[offset + 3] << 24);
	}

	private static bool reads_as (Bytes bytes, uint64 offset, string text) {
		unowned uint8[] data = bytes.get_data ();
		for (int i = 0; i != text.length; i++) {
			if (data[offset + i] != text[i])
				return false;
		}

		return data[offset + text.length] == 0;
	}

	private const size_t LANDING_SIZE = 8;
	private const uint64 PROCESS_HEAD_SIZE = 1024;
	private const uint64 ALL_PROCESSES = 1;
	private const uint64 KEEP_GOING = 0;
	private const uint LAUNCHD = 1;
	private const string LAUNCHD_NAME = "launchd";
	private const uint HIGHEST_PROCESS_NUMBER = 200000;
	private const int ENOUGH_PROCESSES = 24;
	private const uint64 FIRST_PROCESS_NUMBERS = 512;
}
