[CCode (cheader_filename = "frida-atomics.h")]
namespace Frida.Atomics {
	public static uint64 load_u64_acquire (uint64 * p);
	public static void store_u64_release (uint64 * p, uint64 v);
	public static uint32 load_u32_acquire (uint32 * p);
}
