namespace Frida {
	/**
	 * Hosts a Frida control endpoint that frida-server-style clients can connect
	 * to, exposing the local system over the network.
	 */
	public sealed class ControlService : Object {
		/**
		 * The endpoint the service listens on.
		 */
		public EndpointParameters endpoint_params {
			get;
			construct;
		}

		/**
		 * The options the service was configured with.
		 */
		public ControlServiceOptions options {
			get;
			construct;
		}

		private HostSession host_session;
		private HostSessionProvider provider;

		private State state = STOPPED;

		private WebService service;
		private ConnectionHandler main_handler;
		private Gee.Map<string, ConnectionHandler> dynamic_interface_handlers = new Gee.HashMap<string, ConnectionHandler> ();

		private Gee.Set<ControlChannel> spawn_gaters = new Gee.HashSet<ControlChannel> ();
		private Gee.Map<uint, PendingSpawn> pending_spawn = new Gee.HashMap<uint, PendingSpawn> ();
		private Gee.Map<AgentSessionId?, AgentSessionEntry> agent_sessions =
			new Gee.HashMap<AgentSessionId?, AgentSessionEntry> (AgentSessionId.hash, AgentSessionId.equal);
		private Gee.Map<ChannelId?, ChannelEntry> channels =
			new Gee.HashMap<ChannelId?, ChannelEntry> (ChannelId.hash, ChannelId.equal);
		private Gee.Map<ServiceSessionId?, ServiceSessionEntry> service_sessions =
			new Gee.HashMap<ServiceSessionId?, ServiceSessionEntry> (ServiceSessionId.hash, ServiceSessionId.equal);

		private Cancellable io_cancellable = new Cancellable ();

		private MainContext? main_context;

		private enum State {
			STOPPED,
			STARTING,
			STARTED,
			STOPPING
		}

		/**
		 * Creates a control service listening on the given endpoint.
		 *
		 * @param endpoint_params the endpoint to listen on
		 * @param options service options, or null for the defaults
		 */
		public ControlService (EndpointParameters endpoint_params, ControlServiceOptions? options = null) throws Error {
#if HAVE_LOCAL_BACKEND
			ControlServiceOptions opts = (options != null) ? options : new ControlServiceOptions ();

			HostSession session;
#if WINDOWS
			var tempdir = new TemporaryDirectory ();
			session = new WindowsHostSession (new WindowsHelperProcess (tempdir), tempdir);
#endif
#if DARWIN
			session = new DarwinHostSession (new DarwinHelperBackend (), new TemporaryDirectory (), opts.report_crashes);
#endif
#if LINUX
			var tempdir = new TemporaryDirectory ();
			session = new LinuxHostSession (new LinuxHelperProcess (tempdir), tempdir, opts.report_crashes);
#endif
#if FREEBSD
			session = new FreebsdHostSession ();
#endif
#if QNX
			session = new QnxHostSession ();
#endif

			Object (
				endpoint_params: endpoint_params,
				options: opts
			);

			assign_session (session, new PrecreatedLocalHostSessionProvider ((LocalHostSession) session));
#else
			throw new Error.NOT_SUPPORTED ("Local backend not available");
#endif
		}

		/**
		 * Creates a control service exposing a specific device rather than the
		 * local system.
		 *
		 * @param device the device to expose
		 * @param endpoint_params the endpoint to listen on
		 * @param options service options, or null for the defaults
		 */
		public async ControlService.with_device (Device device, EndpointParameters endpoint_params,
				ControlServiceOptions? options = null, Cancellable? cancellable = null) throws Error, IOError {
			ControlServiceOptions opts = (options != null) ? options : new ControlServiceOptions ();

			var session = yield device.get_host_session (cancellable);

			Object (
				endpoint_params: endpoint_params,
				options: opts
			);

			assign_session (session, device.provider);
		}

		construct {
			var iface_observer = new TunnelInterfaceObserver ();
			iface_observer.interface_detached.connect (on_interface_detached);

			service = new WebService (endpoint_params, WebServiceFlavor.CONTROL, PortConflictBehavior.FAIL, iface_observer);

			main_handler = new ConnectionHandler (this, null);
		}

		private void assign_session (HostSession session, HostSessionProvider provider) {
			host_session = session;
			host_session.spawn_gating_disabled.connect (on_spawn_gating_disabled);
			host_session.spawn_added.connect (notify_spawn_added);
			host_session.child_added.connect (notify_child_added);
			host_session.child_removed.connect (notify_child_removed);
			host_session.process_crashed.connect (notify_process_crashed);
			host_session.output.connect (notify_output);
			host_session.agent_session_detached.connect (on_agent_session_detached);
			host_session.channel_closed.connect (on_channel_closed);
			host_session.service_session_closed.connect (on_service_session_closed);
			host_session.uninjected.connect (notify_uninjected);

			this.provider = provider;
		}

		/**
		 * Starts the service, binding the endpoint and accepting clients.
		 */
		public async void start (Cancellable? cancellable = null) throws Error, IOError {
			if (state != STOPPED)
				throw new Error.INVALID_OPERATION ("Invalid operation");
			state = STARTING;

			main_context = MainContext.ref_thread_default ();

			service.incoming.connect (on_server_connection);

			try {
				yield service.start (cancellable);

				if (options.enable_preload) {
					var base_host_session = host_session as LocalHostSession;
					if (base_host_session != null)
						base_host_session.preload.begin (io_cancellable);
				}

				state = STARTED;
			} finally {
				if (state != STARTED)
					state = STOPPED;
			}
		}

		public void start_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<StartTask> ().execute (cancellable);
		}

		private class StartTask : ControlServiceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.start (cancellable);
			}
		}

		/**
		 * Stops the service, disconnecting clients and releasing the endpoint.
		 */
		public async void stop (Cancellable? cancellable = null) throws Error, IOError {
			if (state != STARTED)
				throw new Error.INVALID_OPERATION ("Invalid operation");
			state = STOPPING;

			service.incoming.disconnect (on_server_connection);

			io_cancellable.cancel ();

			service.stop ();

			foreach (var handler in dynamic_interface_handlers.values.to_array ())
				yield handler.close (cancellable);
			dynamic_interface_handlers.clear ();

			yield main_handler.close (cancellable);

			if (provider is PrecreatedLocalHostSessionProvider)
				yield provider.destroy (host_session, cancellable);

			state = STOPPED;
		}

		public void stop_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<StopTask> ().execute (cancellable);
		}

		private class StopTask : ControlServiceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.stop (cancellable);
			}
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class ControlServiceTask<T> : AsyncTask<T> {
			public weak ControlService parent {
				get;
				construct;
			}
		}

		private void on_server_connection (IOStream connection, SocketAddress remote_address, DynamicInterface? dynamic_iface) {
#if HAVE_LOCAL_BACKEND && (IOS || TVOS)
			/*
			 * We defer the launchd injection until the first connection is established in order
			 * to avoid bootloops on unsupported jailbreaks.
			 */
			var darwin_host_session = host_session as DarwinHostSession;
			if (darwin_host_session != null)
				darwin_host_session.activate_crash_reporter_integration ();
#endif

			ConnectionHandler handler;
			unowned string iface_name = dynamic_iface?.name;
			if (iface_name != null) {
				handler = dynamic_interface_handlers[iface_name];
				if (handler == null) {
					handler = new ConnectionHandler (this, dynamic_iface);
					dynamic_interface_handlers[iface_name] = handler;
				}
			} else {
				handler = main_handler;
			}

			handler.handle_server_connection.begin (connection);
		}

		private void on_interface_detached (DynamicInterface iface) {
			schedule_on_frida_thread (() => {
				ConnectionHandler handler;
				if (dynamic_interface_handlers.unset (iface.name, out handler))
					handler.close.begin (io_cancellable);
				return Source.REMOVE;
			});
		}

		private async void teardown_control_channel (ControlChannel channel) {
			foreach (AgentSessionId id in channel.agent_sessions) {
				AgentSessionEntry entry = agent_sessions[id];

				provider.unlink_agent_session (host_session, id);

				AgentSession? session = entry.session;

				if (entry.persist_timeout == 0 || session == null) {
					agent_sessions.unset (id);
					if (session != null)
						session.close.begin (io_cancellable);
				} else {
					entry.detach_controller ();
					session.interrupt.begin (io_cancellable);
				}
			}

			foreach (ChannelId id in channel.channels.to_array ()) {
				ChannelEntry entry = channels[id];

				provider.unlink_channel (host_session, id);

				channels.unset (id);

				Channel? ch = entry.channel;
				if (ch != null)
					ch.close.begin (null);
			}

			foreach (ServiceSessionId id in channel.service_sessions.to_array ()) {
				provider.unlink_service_session (host_session, id);

				service_sessions.unset (id);
			}

			try {
				yield disable_spawn_gating (channel);
			} catch (GLib.Error e) {
			}
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			assert (main_context != null);

			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}

		private class ConnectionHandler : Object {
			public weak ControlService parent {
				get;
				construct;
			}

			public DynamicInterface? dynamic_iface {
				get;
				construct;
			}

			public HostSession host_session {
				get {
					return parent.host_session;
				}
			}

			private Gee.Map<DBusConnection, Peer> peers = new Gee.HashMap<DBusConnection, Peer> ();

			private SocketService broker_service = new SocketService ();
#if !WINDOWS
			private uint16 broker_port = 0;
#endif
			private Gee.Map<string, Transport> transports = new Gee.HashMap<string, Transport> ();

			private Cancellable io_cancellable = new Cancellable ();

			public ConnectionHandler (ControlService parent, DynamicInterface? dynamic_iface) {
				Object (parent: parent, dynamic_iface: dynamic_iface);
			}

			construct {
				broker_service.incoming.connect (on_broker_service_connection);
			}

			public async void close (Cancellable? cancellable) throws IOError {
				broker_service.incoming.disconnect (on_broker_service_connection);

				io_cancellable.cancel ();

				broker_service.stop ();

				transports.clear ();

				foreach (var peer in peers.values.to_array ())
					yield peer.close (cancellable);
				peers.clear ();
			}

			public Gee.Iterator<ControlChannel> all_control_channels () {
				return (Gee.Iterator<ControlChannel>) peers.values.filter (peer => peer is ControlChannel);
			}

			public async void handle_server_connection (IOStream raw_connection) throws GLib.Error {
				var connection = yield new DBusConnection (raw_connection, null, DELAY_MESSAGE_PROCESSING, null,
					io_cancellable);
				connection.on_closed.connect (on_connection_closed);

				AuthenticationService? auth_service = parent.endpoint_params.auth_service;
				peers[connection] = (auth_service != null)
					? (Peer) new AuthenticationChannel (this, connection, auth_service)
					: (Peer) new ControlChannel (this, connection);

				connection.start_message_processing ();
			}

			private void on_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
				Peer peer;
				if (peers.unset (connection, out peer))
					peer.close.begin (io_cancellable);
			}

			public async void promote_authentication_channel (AuthenticationChannel channel) throws GLib.Error {
				DBusConnection connection = channel.connection;

				peers.unset (connection);
				yield channel.close (io_cancellable);

				peers[connection] = new ControlChannel (this, connection);
			}

			public void kick_authentication_channel (AuthenticationChannel channel) {
				var source = new IdleSource ();
				source.set_callback (() => {
					perform_kick.begin (channel);
					return false;
				});
				source.attach (MainContext.get_thread_default ());
			}

			private async void perform_kick (AuthenticationChannel channel) {
				try {
					yield channel.connection.flush (io_cancellable);
				} catch (GLib.Error e) {
				}
				try {
					yield channel.connection.close (io_cancellable);
				} catch (GLib.Error e) {
				}
			}

			public async void teardown_control_channel (ControlChannel channel) {
				yield parent.teardown_control_channel (channel);
			}

			public async void enable_spawn_gating (HashTable<string, Variant> options, ControlChannel requester)
					throws GLib.Error {
				yield parent.enable_spawn_gating (options, requester);
			}

			public async void disable_spawn_gating (ControlChannel requester) throws GLib.Error {
				yield parent.disable_spawn_gating (requester);
			}

			public HostSpawnInfo[] enumerate_pending_spawn () {
				return parent.enumerate_pending_spawn ();
			}

			public async void resume (uint pid, ControlChannel requester) throws GLib.Error {
				yield parent.resume (pid, requester);
			}

			public async AgentSessionId attach (uint pid, HashTable<string, Variant> options, ControlChannel requester,
					Cancellable? cancellable) throws Error, IOError {
				return yield parent.attach (pid, options, requester, cancellable);
			}

			public async void reattach (AgentSessionId id, ControlChannel requester, Cancellable? cancellable)
					throws Error, IOError {
				yield parent.reattach (id, requester, cancellable);
			}

			public async ChannelId open_channel (string address, ControlChannel requester, Cancellable? cancellable)
					throws Error, IOError {
				return yield parent.open_channel (address, requester, cancellable);
			}

			public async ServiceSessionId open_service (string address, ControlChannel requester, Cancellable? cancellable)
					throws Error, IOError {
				return yield parent.open_service (address, requester, cancellable);
			}

			public void open_tcp_transport (AgentSessionId id, Cancellable? cancellable, out uint16 port, out string token)
					throws Error {
#if WINDOWS
				throw new Error.NOT_SUPPORTED ("Not yet supported on Windows");
#else
				var base_host_session = host_session as LocalHostSession;
				if (base_host_session == null)
					throw new Error.NOT_SUPPORTED ("Not supported for remote host sessions");
				if (!base_host_session.can_pass_file_descriptors_to_agent_session (id))
					throw new Error.INVALID_ARGUMENT ("Not supported by this particular agent session");

				if (broker_port == 0) {
					try {
						if (dynamic_iface != null) {
							SocketAddress effective_address;
							broker_service.add_address (
								new InetSocketAddress (dynamic_iface.ip, 0),
								STREAM,
								TCP,
								null,
								out effective_address);
							broker_port = ((InetSocketAddress) effective_address).get_port ();
						} else {
							broker_port = broker_service.add_any_inet_port (null);
						}
					} catch (GLib.Error e) {
						throw new Error.NOT_SUPPORTED ("Unable to listen: %s", e.message);
					}

					broker_service.start ();
				}

				string transport_id = Uuid.string_random ();

				var expiry_source = new TimeoutSource.seconds (20);
				expiry_source.set_callback (() => {
					transports.unset (transport_id);
					return false;
				});
				expiry_source.attach (MainContext.get_thread_default ());

				transports[transport_id] = new Transport (id, expiry_source);

				port = broker_port;
				token = transport_id;
#endif
			}

			private bool on_broker_service_connection (SocketConnection connection, Object? source_object) {
				handle_broker_connection.begin (connection);
				return true;
			}

			private async void handle_broker_connection (SocketConnection connection) throws GLib.Error {
				var socket = connection.socket;
				if (socket.get_family () != UNIX)
					Tcp.enable_nodelay (socket);

				const size_t uuid_length = 36;

				var raw_token = new uint8[uuid_length + 1];
				size_t bytes_read;
				yield connection.input_stream.read_all_async (raw_token[0:uuid_length], Priority.DEFAULT, io_cancellable,
					out bytes_read);
				unowned string token = (string) raw_token;

				Transport transport;
				if (!transports.unset (token, out transport))
					return;

				transport.expiry_source.destroy ();

#if !WINDOWS
				AgentSessionId session_id = transport.session_id;

				var base_host_session = host_session as LocalHostSession;
				if (base_host_session == null)
					throw new Error.NOT_SUPPORTED ("Not supported for remote host sessions");

				AgentSessionProvider provider = base_host_session.obtain_session_provider (session_id);
				yield provider.migrate (session_id, socket, io_cancellable);
#endif
			}

			private class Transport {
				public AgentSessionId session_id;
				public Source expiry_source;

				public Transport (AgentSessionId session_id, Source expiry_source) {
					this.session_id = session_id;
					this.expiry_source = expiry_source;
				}
			}
		}

		private Gee.Iterator<ControlChannel> all_control_channels () {
			var channels = new Gee.ArrayList<ControlChannel> ();
			channels.add_all_iterator (main_handler.all_control_channels ());
			foreach (var handler in dynamic_interface_handlers.values)
				channels.add_all_iterator (handler.all_control_channels ());
			return channels.iterator ();
		}

		private async void enable_spawn_gating (HashTable<string, Variant> options, ControlChannel requester)
				throws GLib.Error {
			bool is_first = spawn_gaters.is_empty;
			spawn_gaters.add (requester);
			foreach (var spawn in pending_spawn.values)
				spawn.pending_approvers.add (requester);

			if (is_first)
				yield host_session.enable_spawn_gating_with_options (options, io_cancellable);
		}

		private async void disable_spawn_gating (ControlChannel requester) throws GLib.Error {
			if (spawn_gaters.remove (requester)) {
				foreach (uint pid in pending_spawn.keys.to_array ()) {
					PendingSpawn spawn = pending_spawn[pid];
					var approvers = spawn.pending_approvers;
					if (approvers.remove (requester) && approvers.is_empty) {
						forget_pending_spawn (pid, spawn);
						host_session.resume.begin (pid, io_cancellable);
					}
				}
			}

			if (spawn_gaters.is_empty)
				yield host_session.disable_spawn_gating (io_cancellable);
		}

		private HostSpawnInfo[] enumerate_pending_spawn () {
			var result = new HostSpawnInfo[pending_spawn.size];
			var i = 0;
			foreach (var spawn in pending_spawn.values)
				result[i++] = spawn.info;
			return result;
		}

		private async void resume (uint pid, ControlChannel requester) throws GLib.Error {
			PendingSpawn? spawn = pending_spawn[pid];
			if (spawn == null) {
				yield host_session.resume (pid, io_cancellable);
				return;
			}

			var approvers = spawn.pending_approvers;
			approvers.remove (requester);
			if (approvers.is_empty) {
				forget_pending_spawn (pid, spawn);
				yield host_session.resume (pid, io_cancellable);
			}
		}

		private async AgentSessionId attach (uint pid, HashTable<string, Variant> options, ControlChannel requester,
				Cancellable? cancellable) throws Error, IOError {
			AgentSessionId id;
			try {
				id = yield host_session.attach (pid, options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			requester.agent_sessions.add (id);

			var opts = SessionOptions._deserialize (options);

			var entry = new AgentSessionEntry (requester, id, opts.persist_timeout);
			agent_sessions[id] = entry;
			entry.expired.connect (on_agent_session_expired);

			yield link_session (id, entry, requester, cancellable);

			return id;
		}

		private async void reattach (AgentSessionId id, ControlChannel requester, Cancellable? cancellable) throws Error, IOError {
			AgentSessionEntry? entry = agent_sessions[id];
			if (entry == null || entry.controller != null)
				throw new Error.INVALID_OPERATION ("Invalid session ID");

			requester.agent_sessions.add (id);

			entry.attach_controller (requester);

			yield link_session (id, entry, requester, cancellable);
		}

		private async void link_session (AgentSessionId id, AgentSessionEntry entry, ControlChannel requester,
				Cancellable? cancellable) throws Error, IOError {
			DBusConnection controller_connection = requester.connection;

			AgentMessageSink sink;
			try {
				sink = yield controller_connection.get_proxy (null, ObjectPath.for_agent_message_sink (id),
					DO_NOT_LOAD_PROPERTIES, cancellable);
			} catch (IOError e) {
				throw_dbus_error (e);
			}

			var session = yield provider.link_agent_session (host_session, id, sink, cancellable);

			entry.session = session;
			try {
				entry.take_controller_registration (
					controller_connection.register_object (ObjectPath.for_agent_session (id), session));
			} catch (IOError e) {
				assert_not_reached ();
			}
		}

		private async ChannelId open_channel (string address, ControlChannel requester, Cancellable? cancellable)
				throws Error, IOError {
			ChannelId id;
			try {
				id = yield host_session.open_channel (address, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			requester.channels.add (id);

			var entry = new ChannelEntry (requester, id);
			channels[id] = entry;

			var stream = yield provider.link_channel (host_session, id, cancellable);

			Channel channel = new ChannelEndpoint (stream);
			entry.channel = channel;
			entry.take_controller_registration (requester.connection.register_object (ObjectPath.for_channel (id), channel));

			return id;
		}

		private async ServiceSessionId open_service (string address, ControlChannel requester, Cancellable? cancellable)
				throws Error, IOError {
			ServiceSessionId id;
			try {
				id = yield host_session.open_service (address, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}

			requester.service_sessions.add (id);

			var entry = new ServiceSessionEntry (requester, id);
			service_sessions[id] = entry;

			var session = yield provider.link_service_session (host_session, id, cancellable);

			entry.session = session;
			entry.take_controller_registration (
				requester.connection.register_object (ObjectPath.for_service_session (id), session));

			return id;
		}

		// Backend disabled gating and already resumed everything; drop our mirror, tell the gaters.
		private void on_spawn_gating_disabled (SpawnGatingDisabledReason reason) {
			foreach (var spawn in pending_spawn.values.to_array ())
				notify_spawn_removed (spawn.info);
			pending_spawn.clear ();

			foreach (ControlChannel channel in spawn_gaters)
				channel.spawn_gating_disabled (reason);
			spawn_gaters.clear ();
		}

		private void notify_spawn_added (HostSpawnInfo info) {
			pending_spawn[info.pid] = new PendingSpawn (info.pid, info.identifier, spawn_gaters.iterator ());

			foreach (ControlChannel channel in spawn_gaters)
				channel.spawn_added (info);
		}

		private void forget_pending_spawn (uint pid, PendingSpawn spawn) {
			pending_spawn.unset (pid);
			notify_spawn_removed (spawn.info);
		}

		private void notify_spawn_removed (HostSpawnInfo info) {
			foreach (ControlChannel channel in spawn_gaters)
				channel.spawn_removed (info);
		}

		private void notify_child_added (HostChildInfo info) {
			all_control_channels ().foreach (channel => {
				channel.child_added (info);
				return true;
			});
		}

		private void notify_child_removed (HostChildInfo info) {
			all_control_channels ().foreach (channel => {
				channel.child_removed (info);
				return true;
			});
		}

		private void notify_process_crashed (CrashInfo crash) {
			all_control_channels ().foreach (channel => {
				channel.process_crashed (crash);
				return true;
			});
		}

		private void notify_output (uint pid, int fd, uint8[] data) {
			all_control_channels ().foreach (channel => {
				channel.output (pid, fd, data);
				return true;
			});
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			AgentSessionEntry entry;
			if (agent_sessions.unset (id, out entry)) {
				ControlChannel? controller = entry.controller;
				if (controller != null) {
					controller.agent_sessions.remove (id);
					controller.agent_session_detached (id, reason, crash);
				}
			}
		}

		private void on_channel_closed (ChannelId id) {
			ChannelEntry entry;
			if (channels.unset (id, out entry)) {
				ControlChannel controller = entry.controller;
				controller.channels.remove (id);
				controller.channel_closed (id);
			}
		}

		private void on_service_session_closed (ServiceSessionId id) {
			ServiceSessionEntry entry;
			if (service_sessions.unset (id, out entry)) {
				ControlChannel controller = entry.controller;
				controller.service_sessions.remove (id);
				controller.service_session_closed (id);
			}
		}

		private void notify_uninjected (InjectorPayloadId id) {
			all_control_channels ().foreach (channel => {
				channel.uninjected (id);
				return true;
			});
		}

		private void on_agent_session_expired (AgentSessionEntry entry) {
			agent_sessions.unset (entry.id);
		}

		private interface Peer : Object {
			public abstract async void close (Cancellable? cancellable = null) throws IOError;
		}

		private class AuthenticationChannel : Object, Peer, AuthenticationService {
			public weak ConnectionHandler parent {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public AuthenticationService service {
				get;
				construct;
			}

			private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();

			public AuthenticationChannel (ConnectionHandler parent, DBusConnection connection, AuthenticationService service) {
				Object (
					parent: parent,
					connection: connection,
					service: service
				);
			}

			construct {
				try {
					registrations.add (connection.register_object (ObjectPath.AUTHENTICATION_SERVICE,
						(AuthenticationService) this));

					HostSession host_session = new UnauthorizedHostSession ();
					registrations.add (connection.register_object (ObjectPath.HOST_SESSION, host_session));
				} catch (IOError e) {
					assert_not_reached ();
				}
			}

			public async void close (Cancellable? cancellable) throws IOError {
				foreach (var id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}

			public async string authenticate (string token, Cancellable? cancellable) throws GLib.Error {
				try {
					string session_info = yield service.authenticate (token, cancellable);
					yield parent.promote_authentication_channel (this);
					return session_info;
				} catch (GLib.Error e) {
					if (e is Error.INVALID_ARGUMENT)
						parent.kick_authentication_channel (this);
					throw e;
				}
			}
		}

		private class ControlChannel : Object, Peer, HostSession, TransportBroker {
			public weak ConnectionHandler parent {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public Gee.Set<AgentSessionId?> agent_sessions {
				get;
				default = new Gee.HashSet<AgentSessionId?> (AgentSessionId.hash, AgentSessionId.equal);
			}

			public Gee.Set<ChannelId?> channels {
				get;
				default = new Gee.HashSet<ChannelId?> (ChannelId.hash, ChannelId.equal);
			}

			public Gee.Set<ServiceSessionId?> service_sessions {
				get;
				default = new Gee.HashSet<ServiceSessionId?> (ServiceSessionId.hash, ServiceSessionId.equal);
			}

			private Gee.Set<uint> registrations = new Gee.HashSet<uint> ();
			private TimeoutSource? ping_timer;

			public ControlChannel (ConnectionHandler parent, DBusConnection connection) {
				Object (parent: parent, connection: connection);
			}

			construct {
				try {
					HostSession session = this;
					registrations.add (connection.register_object (ObjectPath.HOST_SESSION, session));

					AuthenticationService null_auth = new NullAuthenticationService ();
					registrations.add (connection.register_object (Frida.ObjectPath.AUTHENTICATION_SERVICE, null_auth));

					TransportBroker broker = this;
					registrations.add (connection.register_object (Frida.ObjectPath.TRANSPORT_BROKER, broker));
				} catch (IOError e) {
					assert_not_reached ();
				}
			}

			public async void close (Cancellable? cancellable) throws IOError {
				discard_ping_timer ();

				yield parent.teardown_control_channel (this);

				foreach (var id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}

			public async void ping (uint interval_seconds, Cancellable? cancellable) throws Error, IOError {
				discard_ping_timer ();

				if (interval_seconds != 0) {
					ping_timer = new TimeoutSource (interval_seconds * 1500);
					ping_timer.set_callback (on_ping_timeout);
					ping_timer.attach (MainContext.get_thread_default ());
				}
			}

			private void discard_ping_timer () {
				if (ping_timer == null)
					return;
				ping_timer.destroy ();
				ping_timer = null;
			}

			private bool on_ping_timeout () {
				connection.close.begin ();
				return false;
			}

			public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.query_system_parameters (cancellable);
			}

			public async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
					Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.get_frontmost_application (options, cancellable);
			}

			public async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
					Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.enumerate_applications (options, cancellable);
			}

			public async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
					Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.enumerate_processes (options, cancellable);
			}

			public async void enable_spawn_gating (Cancellable? cancellable) throws GLib.Error {
				yield parent.enable_spawn_gating (make_parameters_dict (), this);
			}

			public async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
					Cancellable? cancellable) throws GLib.Error {
				yield parent.enable_spawn_gating (options, this);
			}

			public async void disable_spawn_gating (Cancellable? cancellable) throws GLib.Error {
				yield parent.disable_spawn_gating (this);
			}

			public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws GLib.Error {
				return parent.enumerate_pending_spawn ();
			}

			public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.enumerate_pending_children (cancellable);
			}

			public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.spawn (program, options, cancellable);
			}

			public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws GLib.Error {
				yield parent.host_session.input (pid, data, cancellable);
			}

			public async void resume (uint pid, Cancellable? cancellable) throws GLib.Error {
				yield parent.resume (pid, this);
			}

			public async void kill (uint pid, Cancellable? cancellable) throws GLib.Error {
				yield parent.host_session.kill (pid, cancellable);
			}

			public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
					Cancellable? cancellable) throws GLib.Error {
				return yield parent.attach (pid, options, this, cancellable);
			}

			public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
				yield parent.reattach (id, this, cancellable);
			}

			public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
					Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.inject_library_file (pid, path, entrypoint, data, cancellable);
			}

			public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
					Cancellable? cancellable) throws GLib.Error {
				return yield parent.host_session.inject_library_blob (pid, blob, entrypoint, data, cancellable);
			}

			public async ChannelId open_channel (string address, Cancellable? cancellable) throws GLib.Error {
				return yield parent.open_channel (address, this, cancellable);
			}

			public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws GLib.Error {
				return yield parent.open_service (address, this, cancellable);
			}

			private async void open_tcp_transport (AgentSessionId id, Cancellable? cancellable, out uint16 port,
					out string token) throws Error {
				parent.open_tcp_transport (id, cancellable, out port, out token);
			}
		}

		private class PendingSpawn {
			public HostSpawnInfo info {
				get;
				private set;
			}

			public Gee.Set<ControlChannel> pending_approvers {
				get;
				default = new Gee.HashSet<ControlChannel> ();
			}

			public PendingSpawn (uint pid, string identifier, Gee.Iterator<ControlChannel> gaters) {
				info = HostSpawnInfo (pid, identifier);
				pending_approvers.add_all_iterator (gaters);
			}
		}

		private abstract class Entry {
			public ControlChannel? controller {
				get {
					return _controller;
				}
				protected set {
					unregister_all ();
					_controller = value;
				}
			}

			private ControlChannel? _controller;
			private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();

			protected Entry (ControlChannel? controller) {
				this.controller = controller;
			}

			~Entry () {
				unregister_all ();
			}

			public void take_controller_registration (uint id) {
				registrations.add (id);
			}

			private void unregister_all () {
				if (_controller == null)
					return;
				var connection = _controller.connection;
				foreach (uint id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}
		}

		private class AgentSessionEntry : Entry {
			public signal void expired ();

			public AgentSessionId id {
				get;
				private set;
			}

			public AgentSession? session {
				get;
				set;
			}

			public uint persist_timeout {
				get;
				private set;
			}

			private TimeoutSource? expiry_timer;

			public AgentSessionEntry (ControlChannel? controller, AgentSessionId id, uint persist_timeout) {
				base (controller);

				this.id = id;
				this.persist_timeout = persist_timeout;
			}

			~AgentSessionEntry () {
				stop_expiry_timer ();
			}

			public void detach_controller () {
				controller = null;
				session = null;

				start_expiry_timer ();
			}

			public void attach_controller (ControlChannel c) {
				stop_expiry_timer ();

				assert (controller == null);
				controller = c;
			}

			private void start_expiry_timer () {
				if (expiry_timer != null)
					return;
				expiry_timer = new TimeoutSource.seconds (persist_timeout + 1);
				expiry_timer.set_callback (() => {
					expired ();
					return false;
				});
				expiry_timer.attach (MainContext.get_thread_default ());
			}

			private void stop_expiry_timer () {
				if (expiry_timer == null)
					return;
				expiry_timer.destroy ();
				expiry_timer = null;
			}
		}

		private class ChannelEntry : Entry {
			public ChannelId id {
				get;
				private set;
			}

			public Channel? channel {
				get;
				set;
			}

			public ChannelEntry (ControlChannel controller, ChannelId id) {
				base (controller);

				this.id = id;
			}
		}

		private class ServiceSessionEntry : Entry {
			public ServiceSessionId id {
				get;
				private set;
			}

			public ServiceSession? session {
				get;
				set;
			}

			public ServiceSessionEntry (ControlChannel controller, ServiceSessionId id) {
				base (controller);

				this.id = id;
			}
		}
	}

	private sealed class PrecreatedLocalHostSessionProvider : LocalHostSessionProvider {
		public PrecreatedLocalHostSessionProvider (LocalHostSession session) {
			take_host_session (session);
		}

		protected override LocalHostSession make_host_session (HostSessionOptions? options) throws Error {
			throw new Error.NOT_SUPPORTED ("Not supported");
		}
	}

	/**
	 * Options for a {@link ControlService}.
	 */
	public sealed class ControlServiceOptions : Object {
		/**
		 * Whether to preload the agent for faster first attach.
		 */
		public bool enable_preload {
			get;
			set;
			default = true;
		}

		/**
		 * Whether to capture and report crashes in instrumented processes.
		 */
		public bool report_crashes {
			get;
			set;
			default = true;
		}
	}
}
