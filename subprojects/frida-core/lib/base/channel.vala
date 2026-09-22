namespace Frida {
	public sealed class ChannelRegistry : Object {
		public signal void channel_closed (ChannelId id);

		private Gee.Map<ChannelId?, weak IOStream> channels =
			new Gee.HashMap<ChannelId?, weak IOStream> (ChannelId.hash, ChannelId.equal);

		~ChannelRegistry () {
			clear ();
		}

		public void clear () {
			foreach (IOStream stream in channels.values.to_array ()) {
				var channel_stream = stream as ChannelStream;
				if (channel_stream != null)
					channel_stream.abandon ();
			}
			channels.clear ();
		}

		public void register (ChannelId id, IOStream stream) {
			stream.set_data ("channel-registry-entry", new Entry (this, id));
			lock (channels)
				channels[id] = stream;
		}

		public IOStream link (ChannelId id) throws Error {
			IOStream? stream;
			lock (channels)
				stream = channels[id];
			if (stream == null)
				throw new Error.INVALID_ARGUMENT ("Invalid channel ID");
			return stream;
		}

		public void unlink (ChannelId id) {
			lock (channels) {
				if (!channels.unset (id))
					return;
			}

			channel_closed (id);
		}

		private class Entry {
			public ChannelRegistry parent;
			public ChannelId id;

			public Entry (ChannelRegistry parent, ChannelId id) {
				this.parent = parent;
				this.id = id;
			}

			~Entry () {
				parent.unlink (id);
			}
		}
	}

	public sealed class ChannelEndpoint : Object, Channel {
		private IOStream? stream;

		private ByteArray write_queue = new ByteArray ();
		private bool writing = false;

		private Cancellable io_cancellable = new Cancellable ();

		public ChannelEndpoint (IOStream stream) {
			this.stream = stream;

			read_loop.begin ();
		}

		public async void close (Cancellable? cancellable) throws Error, IOError {
			io_cancellable.cancel ();
		}

		public async void input (uint8[] data, Cancellable? cancellable) throws Error, IOError {
			if (io_cancellable.is_cancelled ())
				throw new Error.INVALID_OPERATION ("Channel is closed");

			write_queue.append (data);

			if (!writing) {
				writing = true;

				var source = new IdleSource ();
				source.set_callback (() => {
					process_write_queue.begin ();
					return false;
				});
				source.attach (MainContext.get_thread_default ());
			}
		}

		private async void read_loop () {
			var input = stream.get_input_stream ();
			var buffer = new uint8[64 * 1024];

			while (true) {
				ssize_t n;
				try {
					n = yield input.read_async (buffer, Priority.DEFAULT, io_cancellable);
				} catch (GLib.Error e) {
					break;
				}

				output (buffer[:n]);

				if (n == 0)
					break;
			}

			stream.close_async.begin ();
			stream = null;
		}

		private async void process_write_queue () {
			if (stream != null) {
				var output = stream.get_output_stream ();

				while (write_queue.len > 0) {
					uint8[] batch = write_queue.steal ();

					size_t bytes_written;
					try {
						yield output.write_all_async (batch, Priority.DEFAULT, io_cancellable, out bytes_written);
					} catch (GLib.Error e) {
						break;
					}
				}
			}

			writing = false;
		}
	}

	public sealed class ChannelStream : VirtualStream {
		public Channel channel {
			get;
			construct;
		}

		private ByteArray recv_queue = new ByteArray ();
		private ByteArray send_queue = new ByteArray ();

		public ChannelStream (Channel channel) {
			Object (channel: channel);
		}

		construct {
			channel.output.connect (on_output);
		}

		public void abandon () {
			with_state_lock (() => {
				state = CLOSED;
				update_pending_io ();
			});
		}

		protected override IOCondition query_events () {
			IOCondition new_events = 0;

			if (recv_queue.len != 0 || state == CLOSED)
				new_events |= IN;

			if (state == OPEN)
				new_events |= OUT;

			return new_events;
		}

		protected override void handle_close () {
			channel.close.begin (null);
		}

		public override ssize_t read (uint8[] buffer) throws IOError {
			ssize_t n = 0;
			with_state_lock (() => {
				n = ssize_t.min (recv_queue.len, buffer.length);
				if (n > 0) {
					Memory.copy (buffer, recv_queue.data, n);
					recv_queue.remove_range (0, (uint) n);

					update_pending_io ();
				} else {
					if (state == OPEN)
						n = -1;
				}
			});

			if (n == -1)
				throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");

			return n;
		}

		public override ssize_t write (uint8[] buffer) throws IOError {
			with_state_lock (() => {
				send_queue.append (buffer);
			});

			if (main_context.is_owner ()) {
				process_send_queue ();
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					process_send_queue ();
					return Source.REMOVE;
				});
				source.attach (main_context);
			}

			return buffer.length;
		}

		private void process_send_queue () {
			uint8[]? chunk = null;
			with_state_lock (() => {
				size_t n = send_queue.len;
				if (n == 0)
					return;
				chunk = send_queue.data[0:n];
				send_queue.remove_range (0, (uint) n);
			});
			if (chunk == null)
				return;

			channel.input.begin (chunk, io_cancellable);
		}

		private void on_output (uint8[] data) {
			with_state_lock (() => {
				if (data.length != 0)
					recv_queue.append (data);
				else
					state = CLOSED;
				update_pending_io ();
			});
		}
	}
}
