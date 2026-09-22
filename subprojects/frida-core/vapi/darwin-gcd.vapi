[CCode (cheader_filename = "dispatch/dispatch.h", gir_namespace = "Darwin", gir_version = "1.0")]
namespace Darwin.GCD {
	[Compact]
	[CCode (cname = "void", ref_function = "_frida_dispatch_retain", unref_function = "dispatch_release",
		cheader_filename = "frida-darwin.h")]
	public class DispatchQueue {
		[CCode (cname = "dispatch_queue_create")]
		public DispatchQueue (string label, DispatchQueueAttr attr);

		[CCode (cname = "dispatch_async_f")]
		public void dispatch_async ([CCode (delegate_target_pos = 0.9)] DispatchFunction work);

		[CCode (cname = "dispatch_sync_f")]
		public void dispatch_sync ([CCode (delegate_target_pos = 0.9)] DispatchFunction work);
	}

	[CCode (cname = "dispatch_function_t")]
	public delegate void DispatchFunction ();

	[Compact]
	[CCode (cname = "dispatch_queue_attr_t", cprefix = "DISPATCH_QUEUE_")]
	public class DispatchQueueAttr {
		public static DispatchQueueAttr SERIAL;
		public static DispatchQueueAttr SERIAL_INACTIVE;
		public static DispatchQueueAttr CONCURRENT;
		public static DispatchQueueAttr CONCURRENT_INACTIVE;
		public static DispatchQueueAttr SERIAL_WITH_AUTORELEASE_POOL;
		public static DispatchQueueAttr CONCURRENT_WITH_AUTORELEASE_POOL;
	}
}
