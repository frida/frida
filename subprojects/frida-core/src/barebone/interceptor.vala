[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class Interceptor : Object {
		public Machine machine {
			get;
			construct;
		}

		public Allocator allocator {
			get;
			construct;
		}

		public GDB.Breakpoint.Kind breakpoint_kind {
			get;
			set;
			default = SOFT;
		}

		private GDB.Client gdb;

		private Gee.Map<uint64?, BreakpointEntry> breakpoint_entries =
			new Gee.HashMap<uint64?, BreakpointEntry> (Numeric.uint64_hash, Numeric.uint64_equal);
		private Gee.Map<string, CallStack> call_stacks = new Gee.HashMap<string, CallStack> ();
		private Gee.MultiMap<uint64?, CallStack> pending_returns =
			new Gee.HashMultiMap<uint64?, CallStack> (Numeric.uint64_hash, Numeric.uint64_equal);

		private Gee.Map<uint64?, InlineHook> inline_hooks =
			new Gee.HashMap<uint64?, InlineHook> (Numeric.uint64_hash, Numeric.uint64_equal);

		private Cancellable io_cancellable = new Cancellable ();

		private const uint MAX_ARITY = 8;

		public Interceptor (Machine machine, Allocator allocator) {
			Object (machine: machine, allocator: allocator);
		}

		construct {
			gdb = machine.gdb;
			gdb.notify["state"].connect (on_gdb_state_changed);
		}

		~Interceptor () {
			gdb.notify["state"].disconnect (on_gdb_state_changed);
		}

		public async void attach (uint64 target, BreakpointInvocationListener listener, Cancellable? cancellable)
				throws Error, IOError {
			uint64 address = machine.address_from_funcptr (target);

			BreakpointEntry? entry = breakpoint_entries[address];
			if (entry == null) {
				entry = new BreakpointEntry ();
				breakpoint_entries[address] = entry;
			}

			entry.listeners.add (listener);
			if (listener.kind == CALL)
				entry.has_call_listener = true;

			if (entry.listeners.size == 1) {
				try {
					entry.breakpoint = yield gdb.add_breakpoint (breakpoint_kind, address,
						machine.breakpoint_size_from_funcptr (target), cancellable);
				} catch (GLib.Error e) {
					breakpoint_entries.unset (address);
					throw_api_error (e);
				}
			}
		}

		public async void attach_inline (uint64 target, InlineInvocationListener listener, Cancellable? cancellable)
				throws Error, IOError {
			if (listener.kind != PROBE)
				throw new Error.NOT_SUPPORTED ("Only probe-style hooks are currently supported for inline hooks");

			if (inline_hooks.has_key (target))
				throw new Error.INVALID_ARGUMENT ("Only one probe per target is currently supported for inline hooks");
			var hook = yield machine.create_inline_hook (target, listener.on_enter, allocator, cancellable);
			inline_hooks[target] = hook;

			try {
				yield hook.enable (cancellable);
			} catch (GLib.Error e) {
				inline_hooks.unset (target);
				throw_api_error (e);
			}

			hook.set_data ("listener", listener);
		}

		public async void detach (InvocationListener listener, Cancellable? cancellable) throws Error, IOError {
			BreakpointInvocationListener? bpl = listener as BreakpointInvocationListener;
			if (bpl != null) {
				foreach (var e in breakpoint_entries.entries.to_array ()) {
					uint64 address = e.key;
					BreakpointEntry entry = e.value;
					if (entry.listeners.remove (bpl)) {
						if (entry.listeners.is_empty) {
							yield entry.breakpoint.remove (cancellable);
							breakpoint_entries.unset (address);
						} else {
							entry.has_call_listener = entry.listeners.any_match (l => l.kind == CALL);
						}
					}
				}
			}

			InlineInvocationListener? iil = listener as InlineInvocationListener;
			if (iil != null) {
				foreach (var e in inline_hooks.entries.to_array ()) {
					uint64 address = e.key;
					InlineHook hook = e.value;
					if (hook.get_data<InlineInvocationListener> ("listener") == iil) {
						hook.set_data ("listener", null);
						inline_hooks.unset (address);
						yield hook.destroy (cancellable);
						return;
					}
				}
			}
		}

		private void on_gdb_state_changed (Object object, ParamSpec pspec) {
			if (gdb.state != STOPPED)
				return;

			GDB.Exception? exception = gdb.exception;
			if (exception == null)
				return;

			GDB.Breakpoint? bp = exception.breakpoint;
			if (bp == null)
				return;

			handle_breakpoint_hit.begin (bp, exception.thread);
		}

		private async void handle_breakpoint_hit (GDB.Breakpoint bp, GDB.Thread thread) throws Error, IOError {
			uint64 address = bp.address;

			BreakpointEntry? entry = breakpoint_entries[address];
			if (entry != null)
				yield handle_invocation (entry, bp, thread);

			unowned string tid = thread.id;
			foreach (CallStack candidate in pending_returns[address]) {
				if (candidate.thread_id == tid) {
					yield handle_return (candidate, bp, thread);
					return;
				}
			}
		}

		private async void handle_invocation (BreakpointEntry entry, GDB.Breakpoint bp, GDB.Thread thread) throws Error, IOError {
			unowned string tid = thread.id;
			CallStack? call_stack = call_stacks[tid];
			if (call_stack == null) {
				call_stack = new CallStack (tid);
				call_stacks[tid] = call_stack;
			}
			uint depth = call_stack.items.size;

			var frame = yield machine.load_call_frame (thread, MAX_ARITY, io_cancellable);

			var ic = new BreakpointInvocationContext (frame, thread, depth);

			foreach (BreakpointInvocationListener listener in entry.listeners.to_array ())
				listener.on_enter (ic);

			yield frame.commit (io_cancellable);

			bool will_trap_on_leave = entry.has_call_listener;
			if (will_trap_on_leave) {
				bool can_trap_on_leave = true;
				uint64 return_target = frame.return_address;
				uint64 return_address = machine.address_from_funcptr (return_target);
				if (!pending_returns.contains (return_address)) {
					try {
						yield gdb.add_breakpoint (breakpoint_kind, return_address,
							machine.breakpoint_size_from_funcptr (return_target), io_cancellable);
					} catch (GLib.Error e) {
						can_trap_on_leave = false;
					}
				}
				if (can_trap_on_leave) {
					call_stack.items.offer (new CallStack.Item (entry, ic));
					pending_returns[return_address] = call_stack;
				}
			}

			yield continue_from_breakpoint (bp, thread);
		}

		private async void handle_return (CallStack call_stack, GDB.Breakpoint bp, GDB.Thread thread) throws Error, IOError {
			var frame = yield machine.load_call_frame (thread, 0, io_cancellable);

			uint64 return_address = machine.address_from_funcptr (frame.return_address);

			CallStack.Item? item = call_stack.items.poll ();
			if (item != null) {
				BreakpointInvocationContext ic = item.ic;
				ic.switch_frame (frame);

				foreach (BreakpointInvocationListener listener in item.entry.listeners.to_array ()) {
					if (listener.kind == CALL)
						listener.on_leave (item.ic);
				}

				yield frame.commit (io_cancellable);
			}

			pending_returns.remove (return_address, call_stack);
			if (pending_returns.contains (return_address)) {
				yield continue_from_breakpoint (bp, thread);
			} else {
				yield bp.remove (io_cancellable);
				yield gdb.continue (io_cancellable);
			}
		}

		private async void continue_from_breakpoint (GDB.Breakpoint bp, GDB.Thread thread) throws Error, IOError {
			yield bp.disable (io_cancellable);
			yield thread.step (io_cancellable);
			yield bp.enable (io_cancellable);
			yield gdb.continue (io_cancellable);
		}

		private class BreakpointEntry {
			public Gee.List<BreakpointInvocationListener> listeners = new Gee.ArrayList<BreakpointInvocationListener> ();
			public bool has_call_listener = false;
			public GDB.Breakpoint? breakpoint;
		}

		private class CallStack {
			public string thread_id;
			public Gee.Queue<Item> items = new Gee.ArrayQueue<Item> ();

			public CallStack (string thread_id) {
				this.thread_id = thread_id;
			}

			public class Item {
				public BreakpointEntry entry;
				public BreakpointInvocationContext ic;

				public Item (BreakpointEntry entry, BreakpointInvocationContext ic) {
					this.entry = entry;
					this.ic = ic;
				}
			}
		}

		private class BreakpointInvocationContext : Object, InvocationContext {
			public uint64 return_address {
				get { return frame.return_address; }
			}

			public unowned string thread_id {
				get { return thread.id; }
			}

			public uint depth {
				get { return _depth; }
			}

			public Gee.Map<string, Variant> registers {
				get { return frame.registers; }
			}

			public Gee.Map<void *, Object> user_data {
				get;
				default = new Gee.HashMap<void *, Object> ();
			}

			private CallFrame frame;
			private GDB.Thread thread;
			private uint _depth;

			public BreakpointInvocationContext (CallFrame frame, GDB.Thread thread, uint depth) {
				this.frame = frame;
				this.thread = thread;
				this._depth = depth;
			}

			internal void switch_frame (CallFrame frame) {
				this.frame = frame;
			}

			public uint64 get_nth_argument (uint n) {
				return frame.get_nth_argument (n);
			}

			public void replace_nth_argument (uint n, uint64 val) {
				frame.replace_nth_argument (n, val);
			}

			public uint64 get_return_value () {
				return frame.get_return_value ();
			}

			public void replace_return_value (uint64 retval) {
				frame.replace_return_value (retval);
			}
		}
	}

	public interface InvocationListener : Object {
		public abstract Kind kind {
			get;
		}

		public enum Kind {
			CALL,
			PROBE
		}
	}

	public interface BreakpointInvocationListener : InvocationListener {
		public abstract void on_enter (InvocationContext context);
		public abstract void on_leave (InvocationContext context);
	}

	public interface InlineInvocationListener : InvocationListener {
		public abstract uint64 on_enter {
			get;
		}

		public abstract uint64 on_leave {
			get;
		}
	}

	public interface InvocationContext : Object {
		public abstract uint64 return_address {
			get;
		}

		public abstract unowned string thread_id {
			get;
		}

		public abstract uint depth {
			get;
		}

		public abstract Gee.Map<string, Variant> registers {
			get;
		}

		public abstract Gee.Map<void *, Object> user_data {
			get;
		}

		public abstract uint64 get_nth_argument (uint n);
		public abstract void replace_nth_argument (uint n, uint64 val);

		public abstract uint64 get_return_value ();
		public abstract void replace_return_value (uint64 retval);
	}
}
