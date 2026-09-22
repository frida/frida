public sealed class Frida.TunnelInterfaceObserver : Object, DynamicInterfaceObserver {
#if IOS || TVOS
	private Gee.Map<string, DynamicInterface> interfaces = new Gee.HashMap<string, DynamicInterface> ();

	private Darwin.SystemConfiguration.DynamicStore? store;
	private Darwin.GCD.DispatchQueue event_queue =
		new Darwin.GCD.DispatchQueue ("re.frida.endpoint-enumerator", Darwin.GCD.DispatchQueueAttr.SERIAL);

	private MainContext? main_context;

	public override void dispose () {
		if (store != null) {
			store.set_dispatch_queue (null);
			store = null;
			ref ();
			event_queue.dispatch_async (do_dispose);
		}

		base.dispose ();
	}

	private void do_dispose () {
		unref ();
	}

	public void start () {
		main_context = MainContext.ref_thread_default ();

		Darwin.SystemConfiguration.DynamicStoreContext context = { 0, };
		context.info = this;
		store = new Darwin.SystemConfiguration.DynamicStore (null, CoreFoundation.String.make ("Frida"),
			on_interfaces_changed_wrapper, context);

		var pattern = CoreFoundation.String.make ("State:/Network/Interface/utun.*/IPv6");
		var patterns = new CoreFoundation.Array (null, ((CoreFoundation.Type[]) &pattern)[:1]);
		store.set_notification_keys (null, patterns);

		store.set_dispatch_queue (event_queue);

		var initial_keys = store.copy_key_list (pattern);
		if (initial_keys != null)
			handle_interface_changes (initial_keys);
	}

	private static void on_interfaces_changed_wrapper (Darwin.SystemConfiguration.DynamicStore store,
			CoreFoundation.Array changed_keys, void * info) {
		unowned TunnelInterfaceObserver enumerator = (TunnelInterfaceObserver) info;
		enumerator.on_interfaces_changed (changed_keys);
	}

	private void on_interfaces_changed (CoreFoundation.Array changed_keys) {
		schedule_on_frida_thread (() => {
			if (store != null)
				handle_interface_changes (changed_keys);
			return Source.REMOVE;
		});
	}

	private void handle_interface_changes (CoreFoundation.Array changed_keys) {
		var addresses_str = CoreFoundation.String.make ("Addresses");

		foreach (var key in CFArray.wrap<CoreFoundation.String> (changed_keys)) {
			string name = key.to_string ().split ("/")[3];

			var val = (CoreFoundation.Dictionary) store.copy_value (key);
			if (val != null) {
				InetAddress? address = null;
				foreach (var raw_address in CFArray.wrap<CoreFoundation.String> (val[addresses_str])) {
					var str = raw_address.to_string ();
					bool is_reserved_ipv6_range = str.has_prefix ("fc") || str.has_prefix ("fd");
					bool is_tunnel = is_reserved_ipv6_range && str.has_suffix ("::1");
					if (is_tunnel) {
						address = new InetAddress.from_string (str);
						break;
					}
				}
				if (address != null && !interfaces.has_key (name)) {
					var iface = new DynamicInterface (name, address);
					interfaces[name] = iface;
					interface_attached (iface);
				}
			} else {
				DynamicInterface iface;
				if (interfaces.unset (name, out iface))
					interface_detached (iface);
			}
		}
	}

	private void schedule_on_frida_thread (owned SourceFunc function) {
		var source = new IdleSource ();
		source.set_callback ((owned) function);
		source.attach (main_context);
	}
#else
	public void start () {
	}
#endif
}
