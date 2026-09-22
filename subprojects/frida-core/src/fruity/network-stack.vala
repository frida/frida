[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public interface NetworkStack : Object {
		public abstract InetAddress listener_ip {
			get;
		}

		public abstract uint scope_id {
			get;
		}

		public abstract async IOStream open_tcp_connection (InetSocketAddress address, Cancellable? cancellable)
			throws Error, IOError;

		public async IOStream open_tcp_connection_with_timeout (InetSocketAddress address, uint timeout, Cancellable? cancellable)
				throws Error, IOError {
			bool timed_out = false;
			var open_cancellable = new Cancellable ();

			var main_context = MainContext.get_thread_default ();

			var timeout_source = new TimeoutSource (timeout);
			timeout_source.set_callback (() => {
				timed_out = true;
				open_cancellable.cancel ();
				return Source.REMOVE;
			});
			timeout_source.attach (main_context);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				open_cancellable.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (main_context);

			try {
				return yield open_tcp_connection (address, open_cancellable);
			} catch (IOError e) {
				assert (e is IOError.CANCELLED);
				if (timed_out)
					throw new Error.TIMED_OUT ("Networked Apple device is not responding");
				throw e;
			} finally {
				timeout_source.destroy ();
				cancel_source.destroy ();
			}
		}

		public abstract UdpSocket create_udp_socket () throws Error;
	}

	public interface UdpSocket : Object {
		public abstract DatagramBased datagram_based {
			get;
		}

		public abstract void bind (InetSocketAddress address) throws Error;
		public abstract InetSocketAddress get_local_address () throws Error;
		public abstract void socket_connect (InetSocketAddress address, Cancellable? cancellable) throws Error;
	}

	public sealed class SystemNetworkStack : Object, NetworkStack {
		public InetAddress listener_ip {
			get {
				return _listener_ip;
			}
		}

		public uint scope_id {
			get {
				return _scope_id;
			}
		}

		private InetAddress _listener_ip;
		private uint _scope_id;

		public SystemNetworkStack (InetAddress listener_ip, uint scope_id) {
			_listener_ip = listener_ip;
			_scope_id = scope_id;
		}

		public async IOStream open_tcp_connection (InetSocketAddress address, Cancellable? cancellable) throws Error, IOError {
			SocketConnection connection;
			try {
				var client = new SocketClient ();
				connection = yield client.connect_async (address, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CONNECTION_REFUSED)
					throw new Error.SERVER_NOT_RUNNING ("%s", e.message);
				throw new Error.TRANSPORT ("%s", e.message);
			}

			Tcp.enable_nodelay (connection.socket);

			return connection;
		}

		public UdpSocket create_udp_socket () throws Error {
			try {
				var handle = new Socket (IPV6, DATAGRAM, UDP);
				return new SystemUdpSocket (handle);
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
		}

		private class SystemUdpSocket : Object, UdpSocket {
			public Socket handle {
				get;
				construct;
			}

			public DatagramBased datagram_based {
				get {
					return handle;
				}
			}

			public SystemUdpSocket (Socket handle) {
				Object (handle: handle);
			}

			public void bind (InetSocketAddress address) throws Error {
				try {
					handle.bind (address, true);
				} catch (GLib.Error e) {
					throw new Error.NOT_SUPPORTED ("%s", e.message);
				}
			}

			public InetSocketAddress get_local_address () throws Error {
				try {
					return (InetSocketAddress) handle.get_local_address ();
				} catch (GLib.Error e) {
					throw new Error.NOT_SUPPORTED ("%s", e.message);
				}
			}

			public void socket_connect (InetSocketAddress address, Cancellable? cancellable) throws Error {
				try {
					handle.connect (address, cancellable);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("%s", e.message);
				}
			}
		}
	}

	public sealed class VirtualNetworkStack : Object, NetworkStack {
		public signal void outgoing_datagrams (Gee.Collection<Bytes> datagrams);

		public Bytes? ethernet_address {
			get;
			construct;
		}

		public InetAddress? ipv6_address {
			get;
			construct;
		}

		public InetAddress listener_ip {
			get {
				if (_cached_listener_ip == null)
					_cached_listener_ip = ip6_address_to_inet_address (raw_ipv6_address);
				return _cached_listener_ip;
			}
		}

		public uint scope_id {
			get {
				return raw_ipv6_address.zone;
			}
		}

		public uint16 mtu {
			get;
			construct;
		}

		private State state = STARTED;

		private Gee.Queue<Bytes>? pending_outgoing;

		private Gee.Queue<Request> requests = new Gee.ArrayQueue<Request> ();
		private unowned Thread<bool>? lwip_thread;

		private LWIP.NetworkInterface handle;
		private LWIP.IP6Address raw_ipv6_address;
		private InetAddress? _cached_listener_ip;

		private MainContext main_context;

		private enum State {
			STARTED,
			STOPPED
		}

		public VirtualNetworkStack (Bytes? ethernet_address, InetAddress? ipv6_address, uint16 mtu) {
			Object (
				ethernet_address: ethernet_address,
				ipv6_address: ipv6_address,
				mtu: mtu
			);
		}

		static construct {
			LWIP.Runtime.init (() => {});
		}

		construct {
			main_context = MainContext.ref_thread_default ();

			perform_on_lwip_thread (() => {
				LWIP.NetworkInterface.add_noaddr (ref handle, this, on_netif_init);
				handle.set_link_up ();
				handle.set_up ();
				return OK;
			});
			state = STARTED;
		}

		private static LWIP.ErrorCode on_netif_init (LWIP.NetworkInterface handle) {
			VirtualNetworkStack * self = handle.state;
			self->configure_netif (ref handle);
			return OK;
		}

		private void configure_netif (ref LWIP.NetworkInterface handle) {
			if (ethernet_address != null) {
				handle.output_ip6 = LWIP.Ethernet.IPv6.output;
				handle.linkoutput = on_netif_link_output;
			} else {
				handle.output_ip6 = on_netif_output_ip6;
			}

			handle.mtu = mtu;
			handle.flags = BROADCAST;

			if (ethernet_address != null) {
				assert (ethernet_address.length == LWIP.Ethernet.HWADDR_LEN);
				Memory.copy (&handle.hwaddr, ethernet_address.get_data (), LWIP.Ethernet.HWADDR_LEN);
				handle.hwaddr_len = LWIP.Ethernet.HWADDR_LEN;

				handle.flags |= ETHARP;
			}

			int8 chosen_index = 0;
			if (ipv6_address != null)
				handle.add_ip6_address (ip6_address_from_inet_address (ipv6_address), &chosen_index);
			else
				handle.create_ip6_linklocal_address (true);
			handle.ip6_addr_set_state (chosen_index, PREFERRED); // No need for conflict detection.
			raw_ipv6_address = handle.ip6_addr[chosen_index];
		}

		public override void dispose () {
			stop ();

			base.dispose ();
		}

		public void stop () {
			if (state == STOPPED)
				return;
			perform_on_lwip_thread (() => {
				handle.remove ();
				return OK;
			});
			state = STOPPED;
		}

		public async IOStream open_tcp_connection (InetSocketAddress address, Cancellable? cancellable = null)
				throws Error, IOError {
			check_started ();
			return yield TcpConnection.open (this, address, cancellable);
		}

		public UdpSocket create_udp_socket () throws Error {
			check_started ();
			return new Ipv6UdpSocket (this);
		}

		public async void handle_incoming_datagrams (Gee.Collection<Bytes> datagrams) throws Error {
			LWIP.ErrorCode err = OK;

			check_started ();

			schedule_on_lwip_thread (() => {
				begin_output ();

				foreach (var d in datagrams) {
					var pbuf = LWIP.PacketBuffer.alloc (RAW, (uint16) d.get_size (), POOL);
					pbuf.take (d.get_data ());

					err = handle.input (pbuf, ref handle);
					if (err == OK)
						*((void **) &pbuf) = null;
					else
						break;
				}

				end_output ();

				schedule_on_frida_thread (handle_incoming_datagrams.callback);

				return err;
			});

			yield;

			check (err);
		}

		private static LWIP.ErrorCode on_netif_link_output (LWIP.NetworkInterface handle, LWIP.PacketBuffer pbuf) {
			VirtualNetworkStack * self = handle.state;
			self->emit_datagram (pbuf);
			return OK;
		}

		private static LWIP.ErrorCode on_netif_output_ip6 (LWIP.NetworkInterface handle, LWIP.PacketBuffer pbuf,
				LWIP.IP6Address address) {
			VirtualNetworkStack * self = handle.state;
			self->emit_datagram (pbuf);
			return OK;
		}

		internal void begin_output () {
			pending_outgoing = new Gee.ArrayQueue<Bytes> ();
		}

		internal void end_output () {
			var datagrams = pending_outgoing;
			pending_outgoing = null;
			if (datagrams.is_empty)
				return;

			schedule_on_frida_thread (() => {
				if (state == STARTED)
					outgoing_datagrams (datagrams);
				return Source.REMOVE;
			});
		}

		private void emit_datagram (LWIP.PacketBuffer pbuf) {
			var buffer = new uint8[pbuf.tot_len];
			unowned uint8[] packet = pbuf.get_contiguous (buffer, pbuf.tot_len);
			var datagram = new Bytes (packet[:pbuf.tot_len]);

			if (pending_outgoing != null) {
				pending_outgoing.offer (datagram);
				return;
			}

			schedule_on_frida_thread (() => {
				if (state == STARTED) {
					var datagrams = new Gee.ArrayQueue<Bytes> ();
					datagrams.offer (datagram);
					outgoing_datagrams (datagrams);
				}
				return Source.REMOVE;
			});
		}

		internal LWIP.ErrorCode perform_on_lwip_thread (owned WorkFunc work) {
			var req = submit_work ((owned) work);
			return req.join ();
		}

		internal void schedule_on_lwip_thread (owned WorkFunc work) {
			submit_work ((owned) work);
		}

		private Request submit_work (owned WorkFunc work) {
			var req = new Request ((owned) work);

			if (Thread.self<bool> () == lwip_thread) {
				perform_request (req);
			} else {
				lock (requests)
					requests.offer (req);
				LWIP.Runtime.schedule (perform_next_request);
			}

			return req;
		}

		private void perform_next_request () {
			if (lwip_thread == null)
				lwip_thread = Thread.self ();

			Request req;
			lock (requests)
				req = requests.poll ();

			perform_request (req);
		}

		private static void perform_request (Request req) {
			LWIP.ErrorCode err = req.work ();
			req.complete (err);
		}

		private void check_started () throws Error {
			if (state != STARTED)
				throw new Error.INVALID_OPERATION ("Networking stack has been stopped");
		}

		internal delegate LWIP.ErrorCode WorkFunc ();

		private class Request {
			public WorkFunc work;

			private bool completed = false;
			private LWIP.ErrorCode error;
			private Mutex mutex = Mutex ();
			private Cond cond = Cond ();

			public Request (owned WorkFunc work) {
				this.work = (owned) work;
			}

			public LWIP.ErrorCode join () {
				mutex.lock ();
				while (!completed)
					cond.wait (mutex);
				var err = error;
				mutex.unlock ();
				return err;
			}

			public void complete (LWIP.ErrorCode err) {
				mutex.lock ();
				completed = true;
				error = err;
				cond.signal ();
				mutex.unlock ();
			}
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}

		private class TcpConnection : VirtualStream, AsyncInitable {
			public VirtualNetworkStack netstack {
				get;
				construct;
			}

			public InetSocketAddress address {
				get;
				construct;
			}

			private Promise<bool> established = new Promise<bool> ();

			private unowned LWIP.TcpPcb? pcb;
			private ByteArray rx_buf = new ByteArray.sized (64 * 1024);
			private bool tx_possible = false;
			private bool tx_check_pending = false;

			public static async TcpConnection open (VirtualNetworkStack netstack, InetSocketAddress address,
					Cancellable? cancellable) throws Error, IOError {
				var connection = new TcpConnection (netstack, address);

				try {
					yield connection.init_async (Priority.DEFAULT, cancellable);
				} catch (GLib.Error e) {
					throw_api_error (e);
				}

				return connection;
			}

			private TcpConnection (VirtualNetworkStack netstack, InetSocketAddress address) {
				Object (netstack: netstack, address: address);
			}

			public override void dispose () {
				stop ();

				base.dispose ();
			}

			protected override VirtualStream.State query_initial_state () {
				return OPENING;
			}

			protected override IOCondition query_events () {
				IOCondition new_events = 0;

				if (rx_buf.len != 0 || state == CLOSED)
					new_events |= IN;

				if (tx_possible)
					new_events |= OUT;

				return new_events;
			}

			private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
				LWIP.Runtime.schedule (do_start);

				try {
					yield established.future.wait_async (cancellable);
				} catch (GLib.Error e) {
					stop ();
					throw_api_error (e);
				}

				return true;
			}

			private void do_start () {
				pcb = LWIP.TcpPcb.make (V6);
				pcb.set_user_data (this);
				pcb.set_recv_callback ((user_data, pcb, pbuf, err) => {
					TcpConnection * self = user_data;
					if (self != null)
						self->on_recv ((owned) pbuf, err);
					return OK;
				});
				pcb.set_sent_callback ((user_data, pcb, len) => {
					TcpConnection * self = user_data;
					if (self != null)
						self->on_sent (len);
					return OK;
				});
				pcb.set_poll_callback ((user_data, pcb) => {
					TcpConnection * self = user_data;
					if (self != null)
						self->on_poll ();
					return OK;
				}, 2);
				pcb.set_error_callback ((user_data, err) => {
					TcpConnection * self = user_data;
					if (self != null)
						self->on_error (err);
				});
				pcb.set_flags (TIMESTAMP | SACK);
				pcb.nagle_disable ();
				pcb.bind_netif (&netstack.handle);

				var err = pcb.connect (ip6_address_from_inet_socket_address (address), address.get_port (), (user_data, pcb, err) => {
					TcpConnection * self = user_data;
					if (self != null)
						self->on_connect ();
					return OK;
				});
				if (err != OK) {
					schedule_on_frida_thread (() => {
						established.reject (parse_error (err));
						return Source.REMOVE;
					});
				}
			}

			private void stop () {
				netstack.perform_on_lwip_thread (() => {
					if (pcb == null)
						return OK;
					pcb.set_user_data (null);
					if (pcb.close () != OK)
						pcb.abort ();
					pcb = null;
					return OK;
				});

				with_state_lock (() => {
					state = CLOSED;
					update_pending_io ();
				});
			}

			private void detach_from_pcb () {
				pcb.set_user_data (null);
				pcb = null;
			}

			private void on_connect () {
				with_state_lock (() => {
					tx_possible = true;
					update_pending_io ();
				});

				schedule_on_frida_thread (() => {
					state = OPEN;

					if (!established.future.ready)
						established.resolve (true);

					return Source.REMOVE;
				});
			}

			private void on_recv (owned LWIP.PacketBuffer? pbuf, LWIP.ErrorCode err) {
				if (pbuf == null) {
					detach_from_pcb ();
					with_state_lock (() => {
						state = CLOSED;
						update_pending_io ();
					});
					return;
				}

				var buffer = new uint8[pbuf.tot_len];
				unowned uint8[] chunk = pbuf.get_contiguous (buffer, pbuf.tot_len);
				with_state_lock (() => {
					rx_buf.append (chunk[:pbuf.tot_len]);
					update_pending_io ();
				});
			}

			private void on_sent (uint16 len) {
				maybe_reenable_tx ();
			}

			private void on_poll () {
				if (tx_check_pending)
					maybe_reenable_tx ();
			}

			private void maybe_reenable_tx () {
				if (pcb_is_writable ()) {
					with_state_lock (() => {
						tx_possible = true;
						tx_check_pending = false;
						update_pending_io ();
					});
				}
			}

			private bool pcb_is_writable () {
				return pcb.query_send_buffer_space () > 0 &&
					pcb.query_send_queue_length () < LWIP.Tcp.SEND_QUEUE_LOW_WATERMARK;
			}

			private void on_error (LWIP.ErrorCode err) {
				bool pcb_already_freed = err == ABRT;
				if (pcb_already_freed)
					pcb = null;
				else
					detach_from_pcb ();

				with_state_lock (() => {
					state = CLOSED;
					update_pending_io ();
				});

				schedule_on_frida_thread (() => {
					if (!established.future.ready)
						established.reject (parse_error (err));

					return Source.REMOVE;
				});
			}

			protected override void handle_close () {
				stop ();
			}

			public override void shutdown_read () throws IOError {
				check_io (netstack.perform_on_lwip_thread (() => {
					if (pcb == null)
						return OK;
					return pcb.shutdown (true, false);
				}));
			}

			public override void shutdown_write () throws IOError {
				check_io (netstack.perform_on_lwip_thread (() => {
					if (pcb == null)
						return OK;
					return pcb.shutdown (false, true);
				}));
			}

			public override ssize_t read (uint8[] buffer) throws IOError {
				ssize_t n = 0;

				netstack.perform_on_lwip_thread (() => {
					with_state_lock (() => {
						n = ssize_t.min (buffer.length, rx_buf.len);
						if (n == 0)
							return;
						Memory.copy (buffer, rx_buf.data, n);
						rx_buf.remove_range (0, (uint) n);
						update_pending_io ();
					});
					if (n == 0)
						return OK;

					if (pcb == null)
						return OK;

					size_t remainder = n;
					while (remainder != 0) {
						uint16 chunk = (uint16) size_t.min (remainder, uint16.MAX);
						pcb.notify_received (chunk);
						remainder -= chunk;
					}

					return OK;
				});

				if (n == 0) {
					if (state == CLOSED)
						return 0;
					throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
				}

				return n;
			}

			public override ssize_t write (uint8[] buffer) throws IOError {
				ssize_t n = 0;

				check_io (netstack.perform_on_lwip_thread (() => {
					if (pcb == null)
						return OK;

					LWIP.TcpPcb.WriteFlags flags = 0;
					LWIP.ErrorCode write_err = OK;
					do {
						flags = COPY;

						size_t len = buffer.length - n;
						if (len > uint16.MAX) {
							len = uint16.MAX;
							flags |= MORE;
						}

						size_t available_space = pcb.query_send_buffer_space ();
						if (available_space < len) {
							len = available_space;
							if (len == 0)
								break;
						}

						write_err = pcb.write (buffer[n:n + len], flags);
						if (write_err == OK)
							n += (ssize_t) len;
					} while ((flags & LWIP.TcpPcb.WriteFlags.MORE) != 0 && write_err == OK);

					if (write_err != OK && write_err != MEM)
						return write_err;

					if (n < buffer.length) {
						with_state_lock (() => {
							tx_possible = false;
							tx_check_pending = true;
							update_pending_io ();
						});
					} else {
						if (!pcb_is_writable ()) {
							with_state_lock (() => {
								tx_possible = false;
								update_pending_io ();
							});
						}
					}

					netstack.begin_output ();
					var output_err = pcb.output ();
					netstack.end_output ();

					if (output_err == RTE)
						return output_err;

					return OK;
				}));

				if (n == 0) {
					if (state == CLOSED)
						throw new IOError.CLOSED ("Connection is closed");
					throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
				}

				return n;
			}

			private void schedule_on_frida_thread (owned SourceFunc function) {
				var source = new IdleSource ();
				source.set_callback ((owned) function);
				source.attach (main_context);
			}
		}

		private class Ipv6UdpSocket : Object, UdpSocket, DatagramBased {
			public VirtualNetworkStack netstack {
				get;
				construct;
			}

			public DatagramBased datagram_based {
				get {
					return this;
				}
			}

			public IOCondition pending_io {
				get {
					lock (_pending_io)
						return _pending_io;
				}
			}

			private unowned LWIP.UdpPcb? pcb;
			private IOCondition _pending_io = OUT;
			private Gee.Queue<Packet> rx_queue = new Gee.ArrayQueue<Packet> ();

			private Gee.Map<unowned Source, IOCondition> sources = new Gee.HashMap<unowned Source, IOCondition> ();

			public Ipv6UdpSocket (VirtualNetworkStack netstack) {
				Object (netstack: netstack);
			}

			construct {
				netstack.perform_on_lwip_thread (() => {
					pcb = LWIP.UdpPcb.make (V6);
					pcb.set_recv_callback (on_recv);
					pcb.bind_netif (&netstack.handle);
					return OK;
				});
			}

			public override void dispose () {
				_netstack.perform_on_lwip_thread (() => {
					pcb.remove ();
					pcb = null;
					return OK;
				});

				base.dispose ();
			}

			private void on_recv (LWIP.UdpPcb pcb, owned LWIP.PacketBuffer? pbuf, LWIP.IP6Address addr, uint16 port) {
				var buffer = new uint8[pbuf.tot_len];
				unowned uint8[] chunk = pbuf.get_contiguous (buffer, pbuf.tot_len);

				var bytes = new Bytes (chunk[:pbuf.tot_len]);
				var sender = ip6_address_to_inet_socket_address (addr, port);
				var packet = new Packet (bytes, sender);

				lock (pending_io)
					rx_queue.offer (packet);
				update_pending_io ();
			}

			public void bind (InetSocketAddress address) throws Error {
				check (netstack.perform_on_lwip_thread (() => {
					return pcb.bind (ip6_address_from_inet_socket_address (address), address.get_port ());
				}));
			}

			public InetSocketAddress get_local_address () throws Error {
				InetSocketAddress? result = null;
				netstack.perform_on_lwip_thread (() => {
					result = ip6_address_to_inet_socket_address (pcb.local_ip, pcb.local_port);
					return OK;
				});
				return result;
			}

			public void socket_connect (InetSocketAddress address, Cancellable? cancellable) throws Error {
				check (netstack.perform_on_lwip_thread (() => {
					return pcb.connect (ip6_address_from_inet_socket_address (address), address.get_port ());
				}));
			}

			public int datagram_receive_messages (InputMessage[] messages, int flags, int64 timeout,
					Cancellable? cancellable) throws GLib.Error {
				if (flags != 0)
					throw new IOError.NOT_SUPPORTED ("Flags not supported");
				if (timeout != 0)
					throw new IOError.NOT_SUPPORTED ("Blocking I/O not supported");

				int received;
				for (received = 0; received != messages.length; received++) {
					Packet? packet;
					lock (pending_io)
						packet = rx_queue.poll ();
					if (packet == null) {
						if (received == 0)
							throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");
						break;
					}
					update_pending_io ();

					if (messages[received].address != null)
						*messages[received].address = packet.address.ref ();

					messages[received].bytes_received = 0;
					messages[received].flags = 0;

					var bytes = packet.bytes;
					uint8 * data = bytes.get_data ();
					size_t remaining = bytes.get_size ();
					foreach (unowned InputVector vector in messages[received].vectors) {
						size_t n = size_t.min (remaining, vector.size);
						if (n == 0)
							break;
						Memory.copy (vector.buffer, data, n);
						data += n;
						remaining -= n;
						messages[received].bytes_received += n;
					}
				}

				return received;
			}

			public virtual int datagram_send_messages (OutputMessage[] messages, int flags, int64 timeout,
					Cancellable? cancellable) throws GLib.Error {
				if (flags != 0)
					throw new IOError.NOT_SUPPORTED ("Flags not supported");

				var packets = new Gee.ArrayList<Packet> ();
				foreach (unowned OutputMessage message in messages) {
					var bytes = new ByteArray ();
					foreach (unowned OutputVector vector in message.vectors) {
						unowned uint8[] data = (uint8[]) vector.buffer;
						bytes.append (data[:vector.size]);
					}
					packets.add (
						new Packet (ByteArray.free_to_bytes ((owned) bytes), (InetSocketAddress) message.address));
				}

				int sent = 0;
				var err = netstack.perform_on_lwip_thread (() => {
					LWIP.ErrorCode err = OK;
					foreach (var packet in packets) {
						var pbuf = LWIP.PacketBuffer.alloc (RAW, (uint16) packet.bytes.get_size (), POOL);
						pbuf.take (packet.bytes.get_data ());

						InetSocketAddress? dst_addr = packet.address;
						if (dst_addr != null)
							err = pcb.sendto (pbuf, ip6_address_from_inet_socket_address (dst_addr), dst_addr.get_port ());
						else
							err = pcb.send (pbuf);
						if (err == OK)
							sent++;
						else
							break;
					}
					return err;
				});
				if (sent == 0)
					check_io (err);
				return sent;
			}

			public virtual DatagramBasedSource datagram_create_source (IOCondition condition, Cancellable? cancellable) {
				return new Ipv6UdpSocketSource (this, condition);
			}

			public virtual IOCondition datagram_condition_check (IOCondition condition) {
				assert_not_reached ();
			}

			public virtual bool datagram_condition_wait (IOCondition condition, int64 timeout, Cancellable? cancellable)
					throws GLib.Error {
				assert_not_reached ();
			}

			public void register_source (Source source, IOCondition condition) {
				lock (pending_io)
					sources[source] = condition | IOCondition.ERR | IOCondition.HUP;
			}

			public void unregister_source (Source source) {
				lock (pending_io)
					sources.unset (source);
			}

			private void update_pending_io () {
				lock (pending_io) {
					_pending_io = OUT;

					if (!rx_queue.is_empty)
						_pending_io |= IN;

					foreach (var entry in sources.entries) {
						unowned Source source = entry.key;
						IOCondition c = entry.value;
						if ((_pending_io & c) != 0)
							source.set_ready_time (0);
					}
				}

				notify_property ("pending-io");
			}

			private class Packet {
				public Bytes bytes;
				public InetSocketAddress? address;

				public Packet (Bytes bytes, InetSocketAddress? address) {
					this.bytes = bytes;
					this.address = address;
				}
			}
		}

		private class Ipv6UdpSocketSource : DatagramBasedSource {
			public Ipv6UdpSocket socket;
			public IOCondition condition;

			public Ipv6UdpSocketSource (Ipv6UdpSocket socket, IOCondition condition) {
				this.socket = socket;
				this.condition = condition;

				socket.register_source (this, condition);
			}

			~Ipv6UdpSocketSource () {
				socket.unregister_source (this);
			}

			protected override bool prepare (out int timeout) {
				timeout = -1;
				return (socket.pending_io & condition) != 0;
			}

			protected override bool check () {
				return (socket.pending_io & condition) != 0;
			}

			protected override bool dispatch (SourceFunc? callback) {
				set_ready_time (-1);

				if (callback == null)
					return Source.REMOVE;

				DatagramBasedSourceFunc f = (DatagramBasedSourceFunc) callback;
				return f (socket, socket.pending_io);
			}
		}

		private static void check (LWIP.ErrorCode err) throws Error {
			if (err != OK)
				throw parse_error (err);
		}

		private static void check_io (LWIP.ErrorCode err) throws IOError {
			if (err != OK)
				throw IOError.from_errno (err.to_errno ());
		}

		private static Error parse_error (LWIP.ErrorCode err) {
			unowned string message = strerror (err.to_errno ());
			if (err == RST)
				return new Error.SERVER_NOT_RUNNING ("%s", message);
			return new Error.TRANSPORT ("%s", message);
		}

		private static LWIP.IP6Address ip6_address_from_inet_socket_address (InetSocketAddress address) {
			var addr = ip6_address_from_inet_address (address.get_address ());
			addr.zone = (uint8) address.scope_id;
			return addr;
		}

		private static LWIP.IP6Address ip6_address_from_inet_address (InetAddress address) {
			return LWIP.IP6Address.parse (address.to_string ());
		}

		private static InetSocketAddress ip6_address_to_inet_socket_address (LWIP.IP6Address address, uint16 port) {
			return (InetSocketAddress) Object.new (typeof (InetSocketAddress),
				address: ip6_address_to_inet_address (address),
				port: port,
				scope_id: address.zone
			);
		}

		private static InetAddress ip6_address_to_inet_address (LWIP.IP6Address address) {
			char buf[40];
			return new InetAddress.from_string (address.to_string (buf));
		}
	}
}
