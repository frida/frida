[CCode (cheader_filename = "darwin-systemconfiguration.h", cprefix = "SC", gir_namespace = "Darwin", gir_version = "1.0")]
namespace Darwin.SystemConfiguration {
	[Compact]
	[CCode (cname = "struct __SCDynamicStore")]
	public class DynamicStore : CoreFoundation.Type {
		[CCode (cname = "SCDynamicStoreCreate")]
		public DynamicStore (CoreFoundation.Allocator? allocator, CoreFoundation.String name, DynamicStoreCallBack callout,
			DynamicStoreContext context);

		[CCode (cname = "SCDynamicStoreSetNotificationKeys")]
		public bool set_notification_keys (CoreFoundation.Array? keys, CoreFoundation.Array? patterns);

		[CCode (cname = "SCDynamicStoreSetDispatchQueue")]
		public bool set_dispatch_queue (Darwin.GCD.DispatchQueue? queue);

		[CCode (cname = "SCDynamicStoreCopyKeyList")]
		public CoreFoundation.Array copy_key_list (CoreFoundation.String pattern);

		[CCode (cname = "SCDynamicStoreCopyValue")]
		public CoreFoundation.Type? copy_value (CoreFoundation.String key);

		[CCode (cname = "SCDynamicStoreCopyMultiple")]
		public CoreFoundation.Dictionary copy_multiple (CoreFoundation.Array? keys, CoreFoundation.Array? patterns = null);
	}

	[CCode (has_target = false)]
	public delegate void DynamicStoreCallBack (DynamicStore store, CoreFoundation.Array changed_keys, void * info);

	public struct DynamicStoreContext {
		public CoreFoundation.Index version;
		public void * info;
		public RetainFunc? retain;
		public ReleaseFunc? release;
		public CopyDescriptionFunc? copyDescription;
	}

	[CCode (has_target = false)]
	public delegate void * RetainFunc (void * info);
	[CCode (has_target = false)]
	public delegate void ReleaseFunc (void * info);
	[CCode (has_target = false)]
	public delegate CoreFoundation.String CopyDescriptionFunc (void * info);
}
