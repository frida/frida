namespace Frida {
	// We need to tease out GDBus' private MainContext as libnice needs to know the MainContext up front :(
	public async MainContext get_dbus_context () {
		if (get_context_request != null) {
			try {
				return yield get_context_request.future.wait_async (null);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
		}
		get_context_request = new Promise<MainContext> ();

		MainContext dbus_context;
		try {
			var input = new DummyInputStream ();
			var output = new MemoryOutputStream (null);
			var connection = yield new DBusConnection (new SimpleIOStream (input, output), null, 0, null, null);

			var caller_context = MainContext.ref_thread_default ();
			int filter_calls = 0;

			uint filter_id = connection.add_filter ((connection, message, incoming) => {
				MainContext ctx = MainContext.ref_thread_default ();

				if (AtomicInt.add (ref filter_calls, 1) == 0) {
					var idle_source = new IdleSource ();
					idle_source.set_callback (() => {
						get_context_request.resolve (ctx);
						return false;
					});
					idle_source.attach (caller_context);
				}

				return message;
			});

			var io_cancellable = new Cancellable ();
			do_get_proxy.begin (connection, io_cancellable);

			dbus_context = yield get_context_request.future.wait_async (null);

			io_cancellable.cancel ();
			connection.remove_filter (filter_id);
			input.unblock ();
			yield connection.close ();
		} catch (GLib.Error e) {
			assert_not_reached ();
		}

		return dbus_context;
	}

	public void invalidate_dbus_context () {
		get_context_request = null;
	}

	private Promise<MainContext>? get_context_request;

	private async HostSession do_get_proxy (DBusConnection connection, Cancellable cancellable) throws IOError {
		return yield connection.get_proxy (null, ObjectPath.HOST_SESSION, DBusProxyFlags.NONE, cancellable);
	}

	private sealed class DummyInputStream : InputStream {
		private bool done = false;
		private Mutex mutex;
		private Cond cond;

		public void unblock () {
			mutex.lock ();
			done = true;
			cond.signal ();
			mutex.unlock ();
		}

		public override bool close (Cancellable? cancellable) throws GLib.IOError {
			return true;
		}

		public override ssize_t read (uint8[] buffer, GLib.Cancellable? cancellable) throws GLib.IOError {
			mutex.lock ();
			while (!done)
				cond.wait (mutex);
			mutex.unlock ();
			return 0;
		}
	}
}
