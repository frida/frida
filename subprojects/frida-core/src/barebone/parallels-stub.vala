[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class ParallelsStubClient : GDB.Client {
		private const uint HALT_TIMEOUT_MSEC = 1000;

		private ParallelsStubClient (IOStream stream) {
			Object (stream: stream);
		}

		public static new async ParallelsStubClient open (IOStream stream, Cancellable? cancellable = null)
				throws Error, IOError {
			var client = new ParallelsStubClient (stream);

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return client;
		}

		protected override async void detect_vendor_features (Cancellable? cancellable) throws Error, IOError {
			supported_features.add ("parallels");
			supported_features.add ("protected-code");
		}

		protected override async void enable_extensions (Cancellable? cancellable) throws Error, IOError {
			string info = yield query_property ("HostInfo", cancellable);
			var host = GDB.Client.PropertyDictionary.parse (info);

			arch = arch_from_cpu_type (GDB.Protocol.parse_uint (host.get_string ("cputype"), 10));
			pointer_size = GDB.Protocol.parse_uint (host.get_string ("ptrsize"), 10);
			byte_order = (host.get_string ("endian") == "little") ? ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN;

			yield halt (cancellable, HALT_TIMEOUT_MSEC);
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
	}
}
