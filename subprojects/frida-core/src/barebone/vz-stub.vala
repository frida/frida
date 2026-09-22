[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	/**
	 * GDB-remote client for the kernel debug stub exposed by Apple's
	 * Virtualization.framework (the stub behind com.apple.Virtualization.VirtualMachine).
	 *
	 * It speaks an lldb debugserver-flavoured RSP dialect with a few quirks that the
	 * generic client cannot cope with: it stays silent until a CPU is halted, implements
	 * neither `qSupported` nor `qXfer:features:read` (registers are discovered through
	 * `qRegisterInfo`), reads memory with the binary `x` packet, and toggles
	 * physical-vs-virtual addressing through the `vf.` vendor namespace instead of `qemu.`.
	 */
	public sealed class VzStubClient : GDB.Client {
		private const size_t MAX_BYTES_PER_READ = 0x800;

		private const uint HALT_TIMEOUT_MSEC = 1000;

		private VzStubClient (IOStream stream) {
			Object (stream: stream);
		}

		public static new async VzStubClient open (IOStream stream, Cancellable? cancellable = null)
				throws Error, IOError {
			var client = new VzStubClient (stream);

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return client;
		}

		protected override async void prepare_connection (Cancellable? cancellable) throws Error, IOError {
			yield halt (cancellable, HALT_TIMEOUT_MSEC);
			yield start_no_ack_mode (cancellable);
		}

		protected override async void detect_vendor_features (Cancellable? cancellable) throws Error, IOError {
			var vcont = yield query_simple ("vCont?", cancellable);
			unowned string payload = vcont.payload;
			if (payload.length > 0 && payload[0] != 'E')
				supported_features.add ("vcont");

			string phy_mem_mode = yield query_property ("vf.PhyMemMode", cancellable);
			if (phy_mem_mode.length == 1)
				supported_features.add ("vf-phy-mem-mode");
		}

		protected override async void enable_extensions (Cancellable? cancellable) throws Error, IOError {
			yield execute_simple ("QThreadSuffixSupported", cancellable);
			supported_features.add ("thread-suffix");
			yield execute_simple ("QListThreadsInStopReply", cancellable);

			yield discover_target (cancellable);
		}

		private async void discover_target (Cancellable? cancellable) throws Error, IOError {
			string info = yield query_property ("HostInfo", cancellable);
			var host = GDB.Client.PropertyDictionary.parse (info);
			arch = arch_from_cpu_type (GDB.Protocol.parse_uint (host.get_string ("cputype"), 10));
			pointer_size = GDB.Protocol.parse_uint (host.get_string ("ptrsize"), 10);
			byte_order = (host.get_string ("endian") == "little") ? ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN;

			set_max_packet_size (0x4000);

			// Force per-register access: the bulk G packet writes every register by position,
			// which would clobber the system registers (PAC keys, ...) that must be preserved.
			bulk_registers = false;

			var regs = new Gee.ArrayList<GDB.Client.Register> ();
			for (uint n = 0; ; n++) {
				var response = yield query_simple ("qRegisterInfo%x".printf (n), cancellable);
				unowned string payload = response.payload;
				if (payload.length == 0 || payload[0] == 'E')
					break;

				var descriptor = GDB.Client.PropertyDictionary.parse (payload);
				string name = descriptor.get_string ("name");
				string? altname = descriptor.has ("alt-name") ? descriptor.get_string ("alt-name") : null;
				uint bitsize = GDB.Protocol.parse_uint (descriptor.get_string ("bitsize"), 10);
				// Writing system registers back would clobber live state such as the PAC keys
				// (apiakey*, apdakey*, ...), breaking kernel-wide pointer authentication, so mark
				// only the general-purpose set as writable.
				bool writable = descriptor.has ("set") && descriptor.get_string ("set") == "General Purpose Registers";
				regs.add (new GDB.Client.Register (name, altname, n, bitsize, writable));
			}
			install_registers (regs);
		}

		private static GDB.TargetArch arch_from_cpu_type (uint cpu_type) {
			switch (cpu_type) {
				case 0x00000007:	return IA32;
				case 0x01000007:	return X64;
				case 0x0000000c:	return ARM;
				case 0x0100000c:	return ARM64;
				default:		return UNKNOWN;
			}
		}

		public override async Bytes read_byte_array (uint64 address, size_t size, Cancellable? cancellable = null)
				throws Error, IOError {
			var result = new uint8[size];

			size_t offset = 0;
			while (offset != size) {
				size_t chunk_size = size_t.min (size - offset, MAX_BYTES_PER_READ);

				var request = make_packet_builder_sized (32)
					.append_c ('x')
					.append_address (address + offset)
					.append_c (',')
					.append_size (chunk_size)
					.build ();
				var response = yield query (request, cancellable);

				Bytes chunk = response.payload_bytes;
				if (chunk.get_size () != chunk_size) {
					throw new Error.INVALID_ARGUMENT (
						"Unable to read from 0x%" + uint64.FORMAT_MODIFIER + "x: invalid address", address);
				}

				Memory.copy ((uint8 *) result + offset, chunk.get_data (), chunk_size);

				offset += chunk_size;
			}

			return new Bytes.take ((owned) result);
		}
	}
}
