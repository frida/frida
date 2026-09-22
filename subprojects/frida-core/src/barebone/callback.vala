[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class Callback : Object {
		private uint64 code;
		private CallbackHandler handler;
		private Machine machine;
		private GDB.Client gdb;
		private GDB.Breakpoint breakpoint;

		private Cancellable io_cancellable = new Cancellable ();

		public async Callback (uint64 code, CallbackHandler handler, Machine machine, Cancellable? cancellable)
				throws Error, IOError {
			this.code = code;
			this.handler = handler;
			this.machine = machine;
			this.gdb = machine.gdb;

			gdb.notify["state"].connect (on_gdb_state_changed);

			breakpoint = yield gdb.add_breakpoint (SOFT, machine.address_from_funcptr (code),
				machine.breakpoint_size_from_funcptr (code), cancellable);
		}

		~Callback () {
			gdb.notify["state"].disconnect (on_gdb_state_changed);
		}

		public async void destroy (Cancellable? cancellable) throws Error, IOError {
			yield breakpoint.remove (cancellable);
		}

		private void on_gdb_state_changed (Object object, ParamSpec pspec) {
			if (gdb.state != STOPPED)
				return;

			GDB.Exception? exception = gdb.exception;
			if (exception == null)
				return;

			if (exception.breakpoint != breakpoint)
				return;

			handle_invocation.begin (exception.thread);
		}

		private async void handle_invocation (GDB.Thread thread) throws Error, IOError {
			uint arity = handler.arity;

			var frame = yield machine.load_call_frame (thread, arity, io_cancellable);

			var args = new uint64[arity];
			for (uint i = 0; i != arity; i++)
				args[i] = frame.get_nth_argument (i);

			uint64 retval = yield handler.handle_invocation (args, frame, io_cancellable);

			frame.replace_return_value (retval);
			frame.force_return ();
			yield frame.commit (io_cancellable);

			yield gdb.continue (io_cancellable);
		}
	}

	public interface CallbackHandler : Object {
		public abstract uint arity {
			get;
		}

		public abstract async uint64 handle_invocation (uint64[] args, CallFrame frame, Cancellable? cancellable)
			throws Error, IOError;
	}
}
