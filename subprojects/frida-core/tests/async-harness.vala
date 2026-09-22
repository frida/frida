namespace Frida.Test {
	public class AsyncHarness : Object {
		public delegate void TestSequenceFunc (void * h);
		private TestSequenceFunc test_sequence;

		private MainLoop main_loop;
		private uint timeout_id;

		public AsyncHarness (owned TestSequenceFunc func) {
			test_sequence = (owned) func;
		}

		public void run () {
			main_loop = new MainLoop ();

			var timed_out = false;

			uint timeout = provide_timeout ();
			if (timeout != 0) {
				timeout_id = Timeout.add_seconds (timeout, () => {
					timed_out = true;
					main_loop.quit ();
					return false;
				});
			}

			Idle.add (() => {
				test_sequence (this);
				return false;
			});

			main_loop.run ();

			assert_false (timed_out);
			if (timeout_id != 0) {
				Source.remove (timeout_id);
				timeout_id = 0;
			}
		}

		protected virtual uint provide_timeout () {
			return 60;
		}

		public async void process_events () {
			Timeout.add (10, process_events.callback);
			yield;
		}

		public void disable_timeout () {
			if (timeout_id != 0) {
				Source.remove (timeout_id);
				timeout_id = 0;
			}
		}

		public virtual void done () {
			/* Queue an idle handler, allowing MainContext to perform any outstanding completions, in turn cleaning up resources */
			Idle.add (() => {
				main_loop.quit ();
				return false;
			});
		}
	}
}
