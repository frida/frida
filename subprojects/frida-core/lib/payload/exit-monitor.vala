namespace Frida {
	public sealed class ExitMonitor : Object, Gum.InvocationListener {
		public weak ExitHandler handler {
			get;
			construct;
		}

		public MainContext main_context {
			get;
			construct;
		}

		public ExitMonitor (ExitHandler handler, MainContext main_context) {
			Object (handler: handler, main_context: main_context);
		}

		private PreparationState preparation_state = UNPREPARED;
		private Mutex mutex;
		private Cond cond;
		private MainContext? blocked_main_context;
		private MainLoop loop;

		private enum PreparationState {
			UNPREPARED,
			PREPARING,
			PREPARED
		}

		construct {
			var interceptor = Gum.Interceptor.obtain ();

			unowned Gum.InvocationListener listener = this;

#if WINDOWS
			interceptor.attach ((void *) Gum.Process.find_module_by_name ("kernel32.dll").find_export_by_name ("ExitProcess"),
				listener);
#else
			var libc = Gum.Process.get_libc_module ();
			const string[] apis = {
				"exit",
				"_exit",
				"abort",
			};
			foreach (var symbol in apis) {
				interceptor.attach ((void *) libc.find_export_by_name (symbol), listener);
			}
#endif
		}

		public override void dispose () {
			var interceptor = Gum.Interceptor.obtain ();

			interceptor.detach (this);

			base.dispose ();
		}

		private void on_enter (Gum.InvocationContext context) {
			if (context.get_depth () > 0)
				return;

			mutex.lock ();
			wait_until_prepared ();
			mutex.unlock ();
		}

		private void on_leave (Gum.InvocationContext context) {
		}

		private void wait_until_prepared () {
			if (preparation_state == PREPARED)
				return;

			if (preparation_state == UNPREPARED) {
				preparation_state = PREPARING;

				if (handler.supports_async_exit ()) {
					schedule_prepare ();
				} else {
					handler.prepare_to_exit_sync ();
					preparation_state = PREPARED;
					return;
				}
			}

			blocked_main_context = MainContext.get_thread_default ();
			if (blocked_main_context != null) {
				loop = new MainLoop (blocked_main_context);

				mutex.unlock ();
				loop.run ();
				mutex.lock ();

				loop = null;
				blocked_main_context = null;
			} else {
				while (preparation_state != PREPARED)
					cond.wait (mutex);
			}
		}

		private void schedule_prepare () {
			var source = new IdleSource ();
			source.set_callback (() => {
				do_prepare.begin ();
				return false;
			});
			source.attach (main_context);
		}

		private async void do_prepare () {
			yield handler.prepare_to_exit ();

			mutex.lock ();

			preparation_state = PREPARED;
			cond.broadcast ();

			if (blocked_main_context != null) {
				var source = new IdleSource ();
				source.set_callback (() => {
					loop.quit ();
					return false;
				});
				source.attach (blocked_main_context);
			}

			mutex.unlock ();
		}
	}

	public interface ExitHandler : Object {
		public abstract bool supports_async_exit ();
		public abstract async void prepare_to_exit ();
		public abstract void prepare_to_exit_sync ();
	}
}
