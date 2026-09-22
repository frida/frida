namespace Frida {
	public abstract class VirtualStream : IOStream {
		public override InputStream input_stream {
			get {
				return _input_stream;
			}
		}

		public override OutputStream output_stream {
			get {
				return _output_stream;
			}
		}

		public IOCondition pending_io {
			get {
				lock (state)
					return _pending_io;
			}
		}

		protected State state = OPEN;

		private VirtualInputStream _input_stream;
		private VirtualOutputStream _output_stream;

		private IOCondition _pending_io;

		private Gee.Map<unowned Source, IOCondition> sources = new Gee.HashMap<unowned Source, IOCondition> ();

		protected Cancellable io_cancellable = new Cancellable ();

		protected MainContext main_context;

		protected enum State {
			CREATED,
			OPENING,
			OPEN,
			CLOSED
		}

		protected delegate void WorkFunc ();

		construct {
			_input_stream = new VirtualInputStream (this);
			_output_stream = new VirtualOutputStream (this);

			main_context = MainContext.ref_thread_default ();

			state = query_initial_state ();
			_pending_io = query_events ();
		}

		public override void dispose () {
			_output_stream.detach ();
			_input_stream.detach ();

			io_cancellable.cancel ();

			base.dispose ();
		}

		protected virtual State query_initial_state () {
			return OPEN;
		}

		protected abstract IOCondition query_events ();

		public override bool close (GLib.Cancellable? cancellable) throws IOError {
			_close ();
			return true;
		}

		public override async bool close_async (int io_priority, GLib.Cancellable? cancellable) throws IOError {
			_close ();
			return true;
		}

		private void _close () {
			if (main_context.is_owner ()) {
				do_close ();
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_close ();
					return Source.REMOVE;
				});
				source.attach (main_context);
			}
		}

		private void do_close () {
			if (state == CLOSED)
				return;

			handle_close ();
		}

		protected abstract void handle_close ();

		public virtual void shutdown_read () throws IOError {
		}

		public virtual void shutdown_write () throws IOError {
		}

		public abstract ssize_t read (uint8[] buffer) throws IOError;

		public abstract ssize_t write (uint8[] buffer) throws IOError;

		protected void with_state_lock (WorkFunc func) {
			lock (state)
				func ();
		}

		public void register_source (Source source, IOCondition condition) {
			lock (state)
				sources[source] = condition | IOCondition.ERR | IOCondition.HUP;
		}

		public void unregister_source (Source source) {
			lock (state)
				sources.unset (source);
		}

		protected virtual void update_pending_io () {
			lock (state) {
				_pending_io = query_events ();

				foreach (var entry in sources.entries) {
					unowned Source source = entry.key;
					IOCondition c = entry.value;
					if ((_pending_io & c) != 0)
						source.set_ready_time (0);
				}
			}

			notify_property ("pending-io");
		}
	}

	private sealed class VirtualInputStream : InputStream, PollableInputStream {
		private weak VirtualStream stream;

		public VirtualInputStream (VirtualStream stream) {
			Object ();
			this.stream = stream;
		}

		internal void detach () {
			stream = null;
		}

		public override bool close (Cancellable? cancellable) throws IOError {
			if (stream != null)
				stream.shutdown_read ();
			return true;
		}

		public override async bool close_async (int io_priority, Cancellable? cancellable) throws GLib.IOError {
			return close (cancellable);
		}

		public override ssize_t read (uint8[] buffer, Cancellable? cancellable) throws IOError {
			if (stream == null)
				return 0;

			if (!is_readable ()) {
				bool done = false;
				var mutex = Mutex ();
				var cond = Cond ();

				ulong io_handler = stream.notify["pending-io"].connect ((obj, pspec) => {
					if (is_readable ()) {
						mutex.lock ();
						done = true;
						cond.signal ();
						mutex.unlock ();
					}
				});
				ulong cancellation_handler = 0;
				if (cancellable != null) {
					cancellation_handler = cancellable.connect (() => {
						mutex.lock ();
						done = true;
						cond.signal ();
						mutex.unlock ();
					});
				}

				if (!is_readable ()) {
					mutex.lock ();
					while (!done)
						cond.wait (mutex);
					mutex.unlock ();
				}

				if (cancellation_handler != 0)
					cancellable.disconnect (cancellation_handler);
				stream.disconnect (io_handler);

				cancellable.set_error_if_cancelled ();
			}

			return stream.read (buffer);
		}

		public bool can_poll () {
			return true;
		}

		public bool is_readable () {
			if (stream == null)
				return true;
			return (stream.pending_io & IOCondition.IN) != 0;
		}

		public PollableSource create_source (Cancellable? cancellable) {
			return new PollableSource.full (this, new VirtualIOSource (stream, IOCondition.IN), cancellable);
		}

		public ssize_t read_nonblocking_fn (uint8[] buffer) throws GLib.Error {
			if (stream == null)
				return 0;
			return stream.read (buffer);
		}
	}

	private sealed class VirtualOutputStream : OutputStream, PollableOutputStream {
		private weak VirtualStream? stream;

		public VirtualOutputStream (VirtualStream stream) {
			Object ();
			this.stream = stream;
		}

		internal void detach () {
			stream = null;
		}

		public override bool close (Cancellable? cancellable) throws IOError {
			if (stream != null)
				stream.shutdown_write ();
			return true;
		}

		public override async bool close_async (int io_priority, Cancellable? cancellable) throws GLib.IOError {
			return close (cancellable);
		}

		public override bool flush (GLib.Cancellable? cancellable) throws GLib.Error {
			return true;
		}

		public override async bool flush_async (int io_priority, GLib.Cancellable? cancellable) throws GLib.Error {
			return true;
		}

		public override ssize_t write (uint8[] buffer, Cancellable? cancellable) throws IOError {
			return stream.write (buffer);
		}

		public bool can_poll () {
			return true;
		}

		public bool is_writable () {
			if (stream == null)
				return false;
			return (stream.pending_io & IOCondition.OUT) != 0;
		}

		public PollableSource create_source (Cancellable? cancellable) {
			return new PollableSource.full (this, new VirtualIOSource (stream, IOCondition.OUT), cancellable);
		}

		public ssize_t write_nonblocking_fn (uint8[]? buffer) throws GLib.Error {
			if (stream == null)
				throw new IOError.CLOSED ("Virtual is closed");
			return stream.write (buffer);
		}

		public PollableReturn writev_nonblocking_fn (OutputVector[] vectors, out size_t bytes_written) throws GLib.Error {
			assert_not_reached ();
		}
	}

	private class VirtualIOSource : Source {
		public VirtualStream? stream;
		public IOCondition condition;

		public VirtualIOSource (VirtualStream stream, IOCondition condition) {
			this.stream = stream;
			this.condition = condition;

			if (stream != null)
				stream.register_source (this, condition);
		}

		~VirtualIOSource () {
			if (stream != null)
				stream.unregister_source (this);
		}

		protected override bool prepare (out int timeout) {
			timeout = -1;
			return is_ready ();
		}

		protected override bool check () {
			return is_ready ();
		}

		private bool is_ready () {
			IOCondition pending_io = (stream != null) ? stream.pending_io : IOCondition.IN;
			return (pending_io & condition) != 0;
		}

		protected override bool dispatch (SourceFunc? callback) {
			set_ready_time (-1);

			if (callback == null)
				return Source.REMOVE;

			return callback ();
		}

		protected static bool closure_callback (Closure closure) {
			var return_value = Value (typeof (bool));

			closure.invoke (ref return_value, {});

			return return_value.get_boolean ();
		}
	}
}
