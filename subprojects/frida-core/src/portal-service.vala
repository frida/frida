namespace Frida {
	/**
	 * Hosts a Frida portal: a rendezvous point where instrumented processes
	 * (nodes) and controllers meet, letting controllers reach nodes without a
	 * direct connection to them.
	 */
	public sealed class PortalService : Object {
		/**
		 * Emitted when a node connects to the cluster endpoint.
		 *
		 * @param connection_id the connection's ID
		 * @param remote_address the node's network address
		 */
		public signal void node_connected (uint connection_id, SocketAddress remote_address);
		/**
		 * Emitted when a node joins the portal, identifying its application.
		 *
		 * @param connection_id the connection's ID
		 * @param application the node's application
		 */
		public signal void node_joined (uint connection_id, Application application);
		/**
		 * Emitted when a node leaves the portal.
		 *
		 * @param connection_id the connection's ID
		 * @param application the node's application
		 */
		public signal void node_left (uint connection_id, Application application);
		/**
		 * Emitted when a node disconnects from the cluster endpoint.
		 *
		 * @param connection_id the connection's ID
		 * @param remote_address the node's network address
		 */
		public signal void node_disconnected (uint connection_id, SocketAddress remote_address);

		/**
		 * Emitted when a controller connects to the control endpoint.
		 *
		 * @param connection_id the connection's ID
		 * @param remote_address the controller's network address
		 */
		public signal void controller_connected (uint connection_id, SocketAddress remote_address);
		/**
		 * Emitted when a controller disconnects from the control endpoint.
		 *
		 * @param connection_id the connection's ID
		 * @param remote_address the controller's network address
		 */
		public signal void controller_disconnected (uint connection_id, SocketAddress remote_address);

		/**
		 * Emitted when a connection successfully authenticates.
		 *
		 * @param connection_id the connection's ID
		 * @param session_info JSON describing the authenticated session
		 */
		public signal void authenticated (uint connection_id, string session_info);
		/**
		 * Emitted when a controller subscribes to portal messages.
		 *
		 * @param connection_id the connection's ID
		 */
		public signal void subscribe (uint connection_id);
		/**
		 * Emitted when a message is received from a connection.
		 *
		 * @param connection_id the connection's ID
		 * @param json the message as a JSON string
		 * @param data binary data accompanying the message, if any
		 */
		public signal void message (uint connection_id, string json, Bytes? data);

		/**
		 * A device representing the portal itself, for interacting with the
		 * connected nodes.
		 */
		public Device device {
			get {
				return _device;
			}
		}
		private Device _device;

		/**
		 * The endpoint that nodes connect to.
		 */
		public EndpointParameters cluster_params {
			get;
			construct;
		}

		/**
		 * The endpoint that controllers connect to, if any.
		 */
		public EndpointParameters? control_params {
			get;
			construct;
		}

		private State state = STOPPED;

		private Gee.List<WebService> cluster_services = new Gee.ArrayList<WebService> ();
		private Gee.List<WebService> control_services = new Gee.ArrayList<WebService> ();

		private Gee.Map<uint, ConnectionEntry> connections = new Gee.HashMap<uint, ConnectionEntry> ();
		private Gee.MultiMap<string, ConnectionEntry> tags = new Gee.HashMultiMap<string, ConnectionEntry> ();
		private uint next_connection_id = 1;

		private Gee.Map<DBusConnection, Peer> peers = new Gee.HashMap<DBusConnection, Peer> ();

		private Gee.Map<uint, ClusterNode> node_by_pid = new Gee.HashMap<uint, ClusterNode> ();
		private Gee.Map<string, ClusterNode> node_by_identifier = new Gee.HashMap<string, ClusterNode> ();

		private Gee.Set<ControlChannel> spawn_gaters = new Gee.HashSet<ControlChannel> ();
		private Gee.Map<uint, PendingSpawn> pending_spawn = new Gee.HashMap<uint, PendingSpawn> ();
		private Gee.Map<AgentSessionId?, AgentSessionEntry> sessions =
			new Gee.HashMap<AgentSessionId?, AgentSessionEntry> (AgentSessionId.hash, AgentSessionId.equal);

		private Cancellable? io_cancellable;

		private enum State {
			STOPPED,
			STARTING,
			STARTED
		}

		/**
		 * Creates a portal service.
		 *
		 * @param cluster_params the endpoint that nodes connect to
		 * @param control_params the endpoint that controllers connect to, or
		 *   null to disable the control endpoint
		 */
		public PortalService (EndpointParameters cluster_params, EndpointParameters? control_params = null) {
			Object (cluster_params: cluster_params, control_params: control_params);
		}

		construct {
			_device = new Device (null, new PortalHostSessionProvider (this));

			cluster_services.add (make_cluster_service (cluster_params));
			if (control_params != null)
				control_services.add (make_control_service (control_params));
		}

		public override void dispose () {
			if (_device != null) {
				Device d = _device;
				_device = null;
				teardown_device.begin (d);
			}

			base.dispose ();
		}

		private async void teardown_device (Device d) {
			try {
				yield d._do_close (SessionDetachReason.DEVICE_LOST, true, null);
			} catch (IOError e) {
				assert_not_reached ();
			}
		}

		/**
		 * Adds another endpoint for nodes to connect to.
		 *
		 * @param endpoint_params the endpoint to add
		 */
		public void add_cluster_endpoint (EndpointParameters endpoint_params) throws Error {
			if (state != STOPPED)
				throw new Error.INVALID_OPERATION ("Cannot add endpoint after service has started");
			cluster_services.add (make_cluster_service (endpoint_params));
		}

		/**
		 * Adds another endpoint for controllers to connect to.
		 *
		 * @param endpoint_params the endpoint to add
		 */
		public void add_control_endpoint (EndpointParameters endpoint_params) throws Error {
			if (state != STOPPED)
				throw new Error.INVALID_OPERATION ("Cannot add endpoint after service has started");
			control_services.add (make_control_service (endpoint_params));
		}

		private WebService make_cluster_service (EndpointParameters endpoint_params) {
			var service = new WebService (endpoint_params, CLUSTER);
			service.incoming.connect (on_incoming_cluster_connection);
			return service;
		}

		private WebService make_control_service (EndpointParameters endpoint_params) {
			var service = new WebService (endpoint_params, CONTROL);
			service.incoming.connect (on_incoming_control_connection);
			return service;
		}

		/**
		 * Starts the portal, binding its endpoints and accepting connections.
		 */
		public async void start (Cancellable? cancellable = null) throws Error, IOError {
			if (state != STOPPED)
				throw new Error.INVALID_OPERATION ("Invalid operation");
			state = STARTING;

			io_cancellable = new Cancellable ();

			try {
				foreach (var service in cluster_services)
					yield service.start (cancellable);

				foreach (var service in control_services)
					yield service.start (cancellable);

				state = STARTED;
			} catch (GLib.Error e) {
				throw new Error.ADDRESS_IN_USE ("%s", e.message);
			} finally {
				if (state != STARTED)
					state = STOPPED;
			}
		}

		public void start_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<StartTask> ().execute (cancellable);
		}

		private class StartTask : PortalServiceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.start (cancellable);
			}
		}

		/**
		 * Stops the portal, disconnecting everyone and releasing its endpoints.
		 */
		public async void stop (Cancellable? cancellable = null) throws Error, IOError {
			if (state != STARTED)
				throw new Error.INVALID_OPERATION ("Invalid operation");

			foreach (var service in control_services)
				service.stop ();

			foreach (var service in cluster_services)
				service.stop ();

			if (io_cancellable != null)
				io_cancellable.cancel ();

			foreach (var peer in peers.values.to_array ())
				peer.close ();
			peers.clear ();

			state = STOPPED;
		}

		public void stop_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<StopTask> ().execute (cancellable);
		}

		private class StopTask : PortalServiceTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.stop (cancellable);
			}
		}

		/**
		 * Disconnects a connection from the portal.
		 *
		 * @param connection_id the connection to disconnect
		 */
		public void kick (uint connection_id) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_kick (connection_id);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_kick (connection_id);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_kick (uint connection_id) {
			ConnectionEntry? entry = connections[connection_id];
			if (entry == null)
				return;
			entry.connection.close.begin (io_cancellable);
		}

		/**
		 * Posts a message to a single connection.
		 *
		 * @param connection_id the connection to send to
		 * @param json the message as a JSON string
		 * @param data binary data to accompany the message, if any
		 */
		public void post (uint connection_id, string json, Bytes? data = null) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_post (connection_id, json, data);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_post (connection_id, json, data);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_post (uint connection_id, string json, Bytes? data) {
			ConnectionEntry? entry = connections[connection_id];
			if (entry != null)
				entry.post (json, data);
		}

		/**
		 * Posts a message to every connection bearing the given tag.
		 *
		 * @param tag the tag identifying the recipients
		 * @param json the message as a JSON string
		 * @param data binary data to accompany the message, if any
		 */
		public void narrowcast (string tag, string json, Bytes? data = null) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_narrowcast (tag, json, data);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_narrowcast (tag, json, data);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_narrowcast (string tag, string json, Bytes? data) {
			foreach (ConnectionEntry entry in tags[tag])
				entry.post (json, data);
		}

		/**
		 * Posts a message to every connection.
		 *
		 * @param json the message as a JSON string
		 * @param data binary data to accompany the message, if any
		 */
		public void broadcast (string json, Bytes? data = null) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_broadcast (json, data);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_broadcast (json, data);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_broadcast (string json, Bytes? data) {
			var has_data = data != null;
			var data_param = has_data ? data.get_data () : new uint8[0];

			foreach (Peer peer in peers.values) {
				ControlChannel? controller = peer as ControlChannel;
				if (controller == null)
					continue;

				BusService bus = controller.bus;
				if (bus.status != ATTACHED)
					continue;

				bus.message (json, has_data, data_param);
			}
		}

		/**
		 * Gets the tags currently applied to a connection.
		 *
		 * @param connection_id the connection to inspect
		 * @return the connection's tags, or null if it has none
		 */
		public string[]? enumerate_tags (uint connection_id) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				return do_enumerate_tags (connection_id);
			} else {
				string[]? result = null;
				bool completed = false;
				var mutex = Mutex ();
				var cond = Cond ();

				var source = new IdleSource ();
				source.set_callback (() => {
					result = do_enumerate_tags (connection_id);
					mutex.lock ();
					completed = true;
					cond.signal ();
					mutex.unlock ();
					return false;
				});
				source.attach (context);

				mutex.lock ();
				while (!completed)
					cond.wait (mutex);
				mutex.unlock ();

				return result;
			}
		}

		private string[]? do_enumerate_tags (uint connection_id) {
			ConnectionEntry? entry = connections[connection_id];
			if (entry == null)
				return null;

			Gee.Set<string>? tags = entry.tags;
			if (tags == null)
				return null;

			string[] elements = tags.to_array ();
			string[] strv = new string[elements.length + 1];
			for (int i = 0; i != elements.length; i++)
				strv[i] = elements[i];
			strv.length = elements.length;
			return strv;
		}

		/**
		 * Applies a tag to a connection, so it can be reached with
		 * {@link PortalService.narrowcast}.
		 *
		 * @param connection_id the connection to tag
		 * @param tag the tag to apply
		 */
		public void tag (uint connection_id, string tag) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_tag (connection_id, tag);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_tag (connection_id, tag);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_tag (uint connection_id, string tag) {
			ConnectionEntry? entry = connections[connection_id];
			if (entry == null)
				return;

			tags[tag] = entry;

			if (entry.tags == null)
				entry.tags = new Gee.HashSet<string> ();
			entry.tags.add (tag);
		}

		/**
		 * Removes a tag from a connection.
		 *
		 * @param connection_id the connection to untag
		 * @param tag the tag to remove
		 */
		public void untag (uint connection_id, string tag) {
			MainContext context = get_main_context ();
			if (context.is_owner ()) {
				do_untag (connection_id, tag);
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					do_untag (connection_id, tag);
					return false;
				});
				source.attach (context);
			}
		}

		private void do_untag (uint connection_id, string tag) {
			ConnectionEntry? entry = connections[connection_id];
			if (entry == null)
				return;

			if (entry.tags == null)
				return;
			entry.tags.remove (tag);

			tags.remove (tag, entry);
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class PortalServiceTask<T> : AsyncTask<T> {
			public weak PortalService parent {
				get;
				construct;
			}
		}

		private void on_incoming_cluster_connection (IOStream connection, SocketAddress remote_address) {
			handle_incoming_connection.begin (connection, remote_address, cluster_params);
		}

		private void on_incoming_control_connection (IOStream connection, SocketAddress remote_address) {
			handle_incoming_connection.begin (connection, remote_address, control_params);
		}

		private async void handle_incoming_connection (IOStream web_connection, SocketAddress remote_address,
				EndpointParameters parameters) throws GLib.Error {
			var connection = yield new DBusConnection (web_connection, null, DELAY_MESSAGE_PROCESSING, null, io_cancellable);
			connection.on_closed.connect (on_connection_closed);

			uint connection_id = register_connection (connection, remote_address, parameters);

			Peer peer;
			if (parameters.auth_service != null)
				peer = setup_unauthorized_peer (connection_id, connection, parameters);
			else
				peer = yield setup_authorized_peer (connection_id, connection, parameters);
			peers[connection] = peer;
		}

		private uint register_connection (DBusConnection connection, SocketAddress address,
				EndpointParameters parameters) throws GLib.Error {
			uint id = next_connection_id++;

			var entry = new ConnectionEntry (connection, address, parameters);
			connections[id] = entry;

			if (parameters == cluster_params)
				node_connected (id, address);
			else
				controller_connected (id, address);

			return id;
		}

		private void on_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			Peer peer;
			if (peers.unset (connection, out peer)) {
				peer.close ();

				uint id = peer.connection_id;

				ConnectionEntry entry;
				connections.unset (id, out entry);

				Gee.Set<string> tags_to_remove = entry.tags;
				if (tags_to_remove != null) {
					foreach (string tag in tags_to_remove)
						tags.remove (tag, entry);
				}

				if (entry.parameters == cluster_params)
					node_disconnected (id, entry.address);
				else
					controller_disconnected (id, entry.address);
			}
		}

		private Peer setup_unauthorized_peer (uint connection_id, DBusConnection connection, EndpointParameters parameters) {
			var channel = new AuthenticationChannel (this, connection_id, connection, parameters);

			try {
				if (parameters == cluster_params) {
					PortalSession portal_session = new UnauthorizedPortalSession ();
					channel.take_registration (connection.register_object (ObjectPath.PORTAL_SESSION, portal_session));
				} else {
					HostSession host_session = new UnauthorizedHostSession ();
					channel.take_registration (connection.register_object (ObjectPath.HOST_SESSION, host_session));

					BusSession bus_session = new UnauthorizedBusSession ();
					channel.take_registration (connection.register_object (ObjectPath.BUS_SESSION, bus_session));
				}
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			connection.start_message_processing ();

			return channel;
		}

		private async void promote_authentication_channel (AuthenticationChannel channel, string session_info) throws GLib.Error {
			uint connection_id = channel.connection_id;
			DBusConnection connection = channel.connection;

			peers.unset (connection);
			channel.close ();

			peers[connection] = yield setup_authorized_peer (connection_id, connection, channel.parameters);

			authenticated (connection_id, session_info);
		}

		private void kick_authentication_channel (AuthenticationChannel channel) {
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

		private async Peer setup_authorized_peer (uint connection_id, DBusConnection connection,
				EndpointParameters parameters) throws GLib.Error {
			Peer peer;
			if (parameters == cluster_params)
				peer = yield setup_cluster_node (connection_id, connection);
			else
				peer = setup_control_channel (connection_id, connection);

			ConnectionEntry? entry = connections[peer.connection_id];
			if (entry == null)
				throw new Error.TRANSPORT ("Peer disconnected");
			entry.peer = peer;

			return peer;
		}

		private ControlChannel setup_control_channel (uint connection_id, DBusConnection connection) {
			var channel = new ControlChannel (this, connection_id, connection);

			connection.start_message_processing ();

			return channel;
		}

		private void teardown_control_channel (ControlChannel channel) {
			foreach (AgentSessionId id in channel.sessions) {
				AgentSessionEntry entry = sessions[id];

				AgentSession? session = entry.session;
				if (entry.persist_timeout == 0 || session == null) {
					sessions.unset (id);

					ClusterNode? node = entry.node;
					if (node != null)
						node.sessions.remove (id);

					if (session != null)
						session.close.begin (io_cancellable);
				} else {
					entry.detach_controller ();
					session.interrupt.begin (io_cancellable);
				}
			}

			disable_spawn_gating (channel);
		}

		private async ClusterNode setup_cluster_node (uint connection_id, DBusConnection connection) throws GLib.Error {
			var node = new ClusterNode (this, connection_id, connection);
			node.session_closed.connect (on_agent_session_closed);

			node.session_provider = yield connection.get_proxy (null, ObjectPath.AGENT_SESSION_PROVIDER, DO_NOT_LOAD_PROPERTIES,
				io_cancellable);

			connection.start_message_processing ();

			return node;
		}

		private void teardown_cluster_node (ClusterNode node) {
			var no_crash = CrashInfo.empty ();
			foreach (var id in node.sessions) {
				AgentSessionEntry entry = sessions[id];

				ControlChannel? c = entry.controller;
				if (c != null)
					c.sessions.remove (id);

				AgentSession? session = entry.session;
				if (entry.persist_timeout == 0 || session == null) {
					sessions.unset (id);
					if (c != null)
						c.agent_session_detached (id, SessionDetachReason.PROCESS_TERMINATED, no_crash);
				} else {
					entry.detach_node_and_controller ();
					if (c != null)
						c.agent_session_detached (id, SessionDetachReason.CONNECTION_TERMINATED, no_crash);
				}
			}

			Application? app = node.application;
			if (app != null) {
				uint pid = app.pid;

				node_left (node.connection_id, app);

				node_by_pid.unset (pid);
				node_by_identifier.unset (app.identifier);

				PendingSpawn spawn;
				if (pending_spawn.unset (pid, out spawn))
					notify_spawn_removed (spawn);
			}
		}

		private HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				ControlChannel requester) throws Error {
			var opts = ApplicationQueryOptions._deserialize (options);
			var scope = opts.scope;

			Gee.List<Application> apps = new Gee.ArrayList<Application> ();
			all_nodes_accessible_by (requester).foreach (node => {
				apps.add (node.application);
				return true;
			});
			apps = maybe_filter_apps_using_ids (apps, opts);

			var result = new HostApplicationInfo[apps.size];
			int i = 0;
			foreach (var app in apps) {
				result[i++] = HostApplicationInfo (app.identifier, app.name, app.pid,
					(scope != MINIMAL) ? app.parameters : make_parameters_dict ());
			}
			return result;
		}

		private HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options, ControlChannel requester) throws Error {
			var opts = ProcessQueryOptions._deserialize (options);
			var scope = opts.scope;

			Gee.List<Application> apps = new Gee.ArrayList<Application> ();
			all_nodes_accessible_by (requester).foreach (node => {
				apps.add (node.application);
				return true;
			});
			apps = maybe_filter_apps_using_pids (apps, opts);

			var result = new HostProcessInfo[apps.size];
			int i = 0;
			foreach (var app in apps) {
				result[i++] = HostProcessInfo (app.pid, app.name,
					(scope != MINIMAL) ? app.parameters : make_parameters_dict ());
			}
			return result;
		}

		private Gee.List<Application> maybe_filter_apps_using_ids (Gee.List<Application> apps, ApplicationQueryOptions options) {
			if (!options.has_selected_identifiers ())
				return apps;

			var app_by_identifier = new Gee.HashMap<string, Application> ();
			foreach (var app in apps)
				app_by_identifier[app.identifier] = app;

			var filtered_apps = new Gee.ArrayList<Application> ();
			options.enumerate_selected_identifiers (identifier => {
				Application? app = app_by_identifier[identifier];
				if (app != null)
					filtered_apps.add (app);
			});

			return filtered_apps;
		}

		private Gee.List<Application> maybe_filter_apps_using_pids (Gee.List<Application> apps, ProcessQueryOptions options) {
			if (!options.has_selected_pids ())
				return apps;

			var app_by_pid = new Gee.HashMap<uint, Application> ();
			foreach (Application app in apps)
				app_by_pid[app.pid] = app;

			var filtered_apps = new Gee.ArrayList<Application> ();
			options.enumerate_selected_pids (pid => {
				Application? app = app_by_pid[pid];
				if (app != null)
					filtered_apps.add (app);
			});

			return filtered_apps;
		}

		private void enable_spawn_gating (HashTable<string, Variant> options, ControlChannel requester) {
			spawn_gaters.add (requester);
			foreach (PendingSpawn spawn in pending_spawn.values) {
				bool requester_has_access = find_node_accessible_by (requester, spawn.info.pid) != null;
				if (requester_has_access)
					spawn.pending_approvers.add (requester);
			}
		}

		private void disable_spawn_gating (ControlChannel requester) {
			if (spawn_gaters.remove (requester)) {
				foreach (uint pid in pending_spawn.keys.to_array ())
					resume (pid, requester);
			}
		}

		private HostSpawnInfo[] enumerate_pending_spawn (ControlChannel requester) {
			var result = new HostSpawnInfo[pending_spawn.size];
			int i = 0;
			foreach (PendingSpawn spawn in pending_spawn.values) {
				if (spawn.pending_approvers.contains (requester))
					result[i++] = spawn.info;
			}
			result.length = i;
			return result;
		}

		private void resume (uint pid, ControlChannel requester) {
			PendingSpawn? spawn = pending_spawn[pid];
			if (spawn == null)
				return;

			var approvers = spawn.pending_approvers;
			approvers.remove (requester);
			if (approvers.is_empty) {
				pending_spawn.unset (pid);

				ClusterNode? node = node_by_pid[pid];
				assert (node != null);
				node.resume ();

				notify_spawn_removed (spawn);
			}
		}

		private void kill (uint pid, ControlChannel requester) {
			ClusterNode? node = find_node_accessible_by (requester, pid);
			if (node == null)
				return;

			node.kill ();
		}

		private void handle_bus_attach (uint connection_id) {
			subscribe (connection_id);
		}

		private void handle_bus_message (uint connection_id, string json, Bytes? data) {
			message (connection_id, json, data);
		}

		private async AgentSessionId attach (uint pid, HashTable<string, Variant> options, ControlChannel requester,
				Cancellable? cancellable) throws Error, IOError {
			ClusterNode? node = find_node_accessible_by (requester, pid);
			if (node == null)
				throw new Error.PROCESS_NOT_FOUND ("Unable to find process with pid %u", pid);

			var id = AgentSessionId.generate ();

			yield node.open_session (id, options, cancellable);

			requester.sessions.add (id);

			var opts = SessionOptions._deserialize (options);

			var entry = new AgentSessionEntry (node, requester, id, opts.persist_timeout, io_cancellable);
			sessions[id] = entry;
			entry.expired.connect (on_agent_session_expired);

			yield link_session (id, entry, requester, cancellable);

			return id;
		}

		private async void reattach (AgentSessionId id, ControlChannel requester, Cancellable? cancellable) throws Error, IOError {
			AgentSessionEntry? entry = sessions[id];
			if (entry == null || entry.controller != null)
				throw new Error.INVALID_OPERATION ("Invalid session ID");
			if (entry.node == null)
				throw new Error.INVALID_OPERATION ("Cluster node is temporarily unavailable");

			entry.attach_controller (requester);
			requester.sessions.add (id);

			yield link_session (id, entry, requester, cancellable);
		}

		private async void link_session (AgentSessionId id, AgentSessionEntry entry, ControlChannel requester,
				Cancellable? cancellable) throws Error, IOError {
			DBusConnection node_connection = entry.node.connection;

			AgentSession? session = entry.session;
			if (session == null) {
				try {
					session = yield node_connection.get_proxy (null, ObjectPath.for_agent_session (id),
						DO_NOT_LOAD_PROPERTIES, cancellable);
				} catch (IOError e) {
					throw_dbus_error (e);
				}
				entry.session = session;
			}

			DBusConnection? controller_connection = requester.connection;
			if (controller_connection != null) {
				AgentMessageSink sink;
				try {
					sink = yield controller_connection.get_proxy (null, ObjectPath.for_agent_message_sink (id),
						DO_NOT_LOAD_PROPERTIES, cancellable);
				} catch (IOError e) {
					throw_dbus_error (e);
				}

				try {
					entry.take_controller_registration (
						controller_connection.register_object (ObjectPath.for_agent_session (id), session));
					entry.take_node_registration (
						node_connection.register_object (ObjectPath.for_agent_message_sink (id), sink));
				} catch (IOError e) {
					assert_not_reached ();
				}
			}
		}

		private async void handle_join_request (ClusterNode node, HostApplicationInfo app, SpawnStartState current_state,
				AgentSessionId[] interrupted_sessions, HashTable<string, Variant> options, Cancellable? cancellable,
				out SpawnStartState next_state) throws Error, IOError {
			if (node.application != null)
				throw new Error.PROTOCOL ("Already joined");
			if (node.session_provider == null)
				throw new Error.PROTOCOL ("Missing session provider");

			Variant? acl = options["acl"];
			if (acl != null) {
				if (!acl.is_of_type (VariantType.STRING_ARRAY))
					throw new Error.INVALID_ARGUMENT ("The 'acl' option must be a string array");

				ConnectionEntry entry = connections[node.connection_id];

				Gee.Set<string>? tags = entry.tags;
				if (tags == null) {
					tags = new Gee.HashSet<string> ();
					entry.tags = tags;
				}
				tags.add_all_array (acl.get_strv ());
			}

			foreach (AgentSessionId id in interrupted_sessions) {
				AgentSessionEntry? entry = sessions[id];
				if (entry == null)
					continue;
				if (entry.node != null)
					throw new Error.PROTOCOL ("Session already claimed");
				entry.attach_node (node);
				node.sessions.add (id);
			}

			uint pid = app.pid;
			while (node_by_pid.has_key (pid))
				pid++;

			string real_identifier = app.identifier;
			string candidate = real_identifier;
			uint serial = 2;
			while (node_by_identifier.has_key (candidate))
				candidate = "%s[%u]".printf (real_identifier, serial++);
			string identifier = candidate;

			node.application = new Application (identifier, app.name, pid, app.parameters);

			node_by_pid[pid] = node;
			node_by_identifier[identifier] = node;

			node_joined (node.connection_id, node.application);

			if (current_state == SUSPENDED && !spawn_gaters.is_empty) {
				var eligible_gaters = all_spawn_gaters_with_access_to (node);
				if (eligible_gaters.has_next ()) {
					next_state = SUSPENDED;

					var spawn = new PendingSpawn (node, pid, identifier, eligible_gaters);
					pending_spawn[pid] = spawn;

					foreach (ControlChannel controller in spawn.pending_approvers) {
						controller.spawn_added (spawn.info);
					}
				} else {
					next_state = RUNNING;
				}
			} else {
				next_state = RUNNING;
			}
		}

		private void notify_spawn_removed (PendingSpawn spawn) {
			all_spawn_gaters_with_access_to (spawn.node).foreach (controller => {
				controller.spawn_removed (spawn.info);
				return true;
			});
		}

		private void on_agent_session_expired (AgentSessionEntry entry) {
			sessions.unset (entry.id);

			ClusterNode? node = entry.node;
			if (node != null)
				node.sessions.remove (entry.id);
		}

		private void on_agent_session_closed (AgentSessionId id) {
			AgentSessionEntry entry;
			if (sessions.unset (id, out entry)) {
				ControlChannel? controller = entry.controller;
				if (controller != null) {
					controller.sessions.remove (id);
					controller.agent_session_detached (id, SessionDetachReason.APPLICATION_REQUESTED,
						CrashInfo.empty ());
				}
			}
		}

		private Gee.Iterator<ClusterNode> all_nodes_accessible_by (ControlChannel requester) {
			if (requester.is_local)
				return node_by_pid.values.iterator ();

			Gee.Set<string>? requester_tags = connections[requester.connection_id].tags;
			return node_by_pid.values.filter (node => {
				ConnectionEntry entry = connections[node.connection_id];

				Gee.Set<string>? acl = entry.tags;
				if (acl == null)
					return true;

				if (requester_tags == null)
					return false;

				return acl.any_match (tag => requester_tags.contains (tag));
			});
		}

		private ClusterNode? find_node_accessible_by (ControlChannel requester, uint pid) {
			ClusterNode? node = node_by_pid[pid];
			if (node == null)
				return null;

			return can_access (node, requester) ? node : null;
		}

		private Gee.Iterator<ControlChannel> all_spawn_gaters_with_access_to (ClusterNode node) {
			Gee.Set<string>? acl = connections[node.connection_id].tags;
			if (acl == null)
				return spawn_gaters.iterator ();

			return spawn_gaters.filter (controller => {
				if (controller.is_local)
					return true;

				Gee.Set<string> tags = connections[controller.connection_id].tags;
				if (tags == null)
					return false;

				return acl.any_match (tag => tags.contains (tag));
			});
		}

		private bool can_access (ClusterNode node, ControlChannel requester) {
			if (requester.is_local)
				return true;

			Gee.Set<string>? acl = connections[node.connection_id].tags;
			if (acl == null)
				return true;

			Gee.Set<string> requester_tags = connections[requester.connection_id].tags;
			if (requester_tags == null)
				return false;

			return acl.any_match (tag => requester_tags.contains (tag));
		}

		private class PortalHostSessionProvider : Object, HostSessionProvider {
			public weak PortalService parent {
				get;
				construct;
			}

			public string id {
				get { return "portal"; }
			}

			public string name {
				get { return "Portal"; }
			}

			public Variant? icon {
				get { return _icon; }
			}
			private Variant _icon;

			public HostSessionProviderKind kind {
				get { return HostSessionProviderKind.LOCAL; }
			}

			private ControlChannel? channel;

			public PortalHostSessionProvider (PortalService parent) {
				Object (parent: parent);
			}

			construct {
				_icon = make_provider_icon (Frida.Data.Icons.get_portal_png_blob ().data);
			}

			public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
					Cancellable? cancellable) throws Error, IOError {
				if (channel != null)
					throw new Error.INVALID_OPERATION ("Already created");

				channel = new ControlChannel (parent);
				channel.agent_session_detached.connect (on_agent_session_detached);

				return channel;
			}

			public async void destroy (HostSession host_session, Cancellable? cancellable) throws Error, IOError {
				if (host_session != channel)
					throw new Error.INVALID_ARGUMENT ("Invalid host session");

				channel.agent_session_detached.disconnect (on_agent_session_detached);

				HostSession session = channel;

				channel.close ();
				channel = null;

				host_session_detached (session);
			}

			public async AgentSession link_agent_session (HostSession host_session, AgentSessionId id, AgentMessageSink sink,
					Cancellable? cancellable) throws Error, IOError {
				if (host_session != channel)
					throw new Error.INVALID_ARGUMENT ("Invalid host session");

				AgentSessionEntry? entry = parent.sessions[id];
				if (entry == null)
					throw new Error.INVALID_ARGUMENT ("Invalid session ID");

				try {
					entry.take_node_registration (
						entry.node.connection.register_object (ObjectPath.for_agent_message_sink (id), sink));
				} catch (IOError e) {
					assert_not_reached ();
				}

				return entry.session;
			}

			public void unlink_agent_session (HostSession host_session, AgentSessionId id) {
				if (host_session != channel)
					return;

				AgentSessionEntry? entry = parent.sessions[id];
				if (entry == null)
					return;

				entry.clear_node_registrations ();
			}

			public async IOStream link_channel (HostSession host_session, ChannelId id, Cancellable? cancellable)
					throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
			}

			public void unlink_channel (HostSession host_session, ChannelId id) {
			}

			public async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Services are not supported by this backend");
			}

			public void unlink_service_session (HostSession host_session, ServiceSessionId id) {
			}

			private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
				agent_session_detached (id, reason, crash);
			}
		}

		private class ConnectionEntry {
			public DBusConnection connection;
			public SocketAddress address;
			public EndpointParameters parameters;
			public Peer? peer;
			public Gee.Set<string>? tags;

			public ConnectionEntry (DBusConnection connection, SocketAddress address, EndpointParameters parameters) {
				this.connection = connection;
				this.address = address;
				this.parameters = parameters;
			}

			public void post (string json, Bytes? data) {
				if (peer == null)
					return;

				ControlChannel? controller = peer as ControlChannel;
				if (controller == null)
					return;

				BusService bus = controller.bus;
				if (bus.status != ATTACHED)
					return;

				bool has_data = data != null;
				var data_param = has_data ? data.get_data () : new uint8[0];
				bus.message (json, has_data, data_param);
			}
		}

		private interface Peer : Object {
			public abstract uint connection_id {
				get;
				construct;
			}

			public abstract void close ();
		}

		private class AuthenticationChannel : Object, Peer, AuthenticationService {
			public weak PortalService parent {
				get;
				construct;
			}

			public uint connection_id {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public EndpointParameters parameters {
				get;
				construct;
			}

			private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();

			public AuthenticationChannel (PortalService parent, uint connection_id, DBusConnection connection,
					EndpointParameters parameters) {
				Object (
					parent: parent,
					connection_id: connection_id,
					connection: connection,
					parameters: parameters
				);
			}

			construct {
				try {
					registrations.add (connection.register_object (ObjectPath.AUTHENTICATION_SERVICE,
						(AuthenticationService) this));
				} catch (IOError e) {
					assert_not_reached ();
				}
			}

			public void close () {
				foreach (var id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}

			public void take_registration (uint id) {
				registrations.add (id);
			}

			public async string authenticate (string token, Cancellable? cancellable) throws GLib.Error {
				try {
					string session_info = yield parameters.auth_service.authenticate (token, cancellable);
					yield parent.promote_authentication_channel (this, session_info);
					return session_info;
				} catch (GLib.Error e) {
					if (e is Error.INVALID_ARGUMENT)
						parent.kick_authentication_channel (this);
					throw e;
				}
			}
		}

		private class ControlChannel : Object, Peer, HostSession {
			public weak PortalService parent {
				get;
				construct;
			}

			public uint connection_id {
				get;
				construct;
			}

			public DBusConnection? connection {
				get;
				construct;
			}

			public Gee.Set<AgentSessionId?> sessions {
				get;
				default = new Gee.HashSet<AgentSessionId?> (AgentSessionId.hash, AgentSessionId.equal);
			}

			public BusService? bus {
				get {
					return _bus;
				}
			}

			public bool is_local {
				get {
					return connection == null;
				}
			}

			private BusService? _bus;
			private Gee.Set<uint> registrations = new Gee.HashSet<uint> ();
			private TimeoutSource? ping_timer;

			public ControlChannel (PortalService parent, uint connection_id = 0, DBusConnection? connection = null) {
				Object (
					parent: parent,
					connection_id: connection_id,
					connection: connection
				);
			}

			construct {
				if (connection != null) {
					_bus = new BusService (parent, connection_id);

					try {
						registrations.add (
							connection.register_object (ObjectPath.HOST_SESSION, (HostSession) this));

						registrations.add (
							connection.register_object (ObjectPath.BUS_SESSION, (BusSession) _bus));

						AuthenticationService null_auth = new NullAuthenticationService ();
						registrations.add (
							connection.register_object (Frida.ObjectPath.AUTHENTICATION_SERVICE, null_auth));
					} catch (IOError e) {
						assert_not_reached ();
					}
				}
			}

			public void close () {
				discard_ping_timer ();

				parent.teardown_control_channel (this);

				if (connection != null) {
					foreach (var id in registrations)
						connection.unregister_object (id);
					registrations.clear ();
				}
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
				throw new Error.NOT_SUPPORTED ("Not supported");
			}

			public async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not supported");
			}

			public async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				return parent.enumerate_applications (options, this);
			}

			public async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				return parent.enumerate_processes (options, this);
			}

			public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
				parent.enable_spawn_gating (make_parameters_dict (), this);
			}

			public async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				parent.enable_spawn_gating (options, this);
			}

			public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
				parent.disable_spawn_gating (this);
			}

			public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
				return parent.enumerate_pending_spawn (this);
			}

			public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws Error, IOError {
				return {};
			}

			public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not supported");
			}

			public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not supported");
			}

			public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
				parent.resume (pid, this);
			}

			public async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
				parent.kill (pid, this);
			}

			public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				return yield parent.attach (pid, options, this, cancellable);
			}

			public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
				yield parent.reattach (id, this, cancellable);
			}

			public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not supported");
			}

			public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
					Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Not supported");
			}

			public async ChannelId open_channel (string address, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
			}

			public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
				throw new Error.NOT_SUPPORTED ("Services are not supported by this backend");
			}
		}

		private class BusService : Object, BusSession {
			public weak PortalService parent {
				get;
				construct;
			}

			public uint connection_id {
				get;
				construct;
			}

			public BusStatus status {
				get {
					return _status;
				}
			}
			private BusStatus _status = DETACHED;

			public BusService (PortalService parent, uint connection_id) {
				Object (parent: parent, connection_id: connection_id);
			}

			public async void attach (Cancellable? cancellable) throws Error, IOError {
				if (_status == ATTACHED)
					return;
				_status = ATTACHED;
				parent.handle_bus_attach (connection_id);
			}

			public async void post (string json, bool has_data, uint8[] data, Cancellable? cancellable) throws Error, IOError {
				parent.handle_bus_message (connection_id, json, has_data ? new Bytes (data) : null);
			}
		}

		private enum BusStatus {
			DETACHED,
			ATTACHED
		}

		private class ClusterNode : Object, Peer, PortalSession {
			public signal void session_closed (AgentSessionId id);

			public weak PortalService parent {
				get;
				construct;
			}

			public uint connection_id {
				get;
				construct;
			}

			public Application? application {
				get;
				set;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public AgentSessionProvider? session_provider {
				get {
					return _session_provider;
				}
				set {
					if (_session_provider != null)
						_session_provider.closed.disconnect (on_session_closed);
					_session_provider = value;
					_session_provider.closed.connect (on_session_closed);
				}
			}
			private AgentSessionProvider? _session_provider;

			public Gee.Set<AgentSessionId?> sessions {
				get;
				default = new Gee.HashSet<AgentSessionId?> (AgentSessionId.hash, AgentSessionId.equal);
			}

			private Gee.Set<uint> registrations = new Gee.HashSet<uint> ();

			public ClusterNode (PortalService parent, uint connection_id, DBusConnection connection) {
				Object (
					parent: parent,
					connection_id: connection_id,
					connection: connection
				);
			}

			construct {
				try {
					PortalSession session = this;
					registrations.add (connection.register_object (ObjectPath.PORTAL_SESSION, session));

					AuthenticationService null_auth = new NullAuthenticationService ();
					registrations.add (connection.register_object (Frida.ObjectPath.AUTHENTICATION_SERVICE, null_auth));
				} catch (IOError e) {
					assert_not_reached ();
				}
			}

			public void close () {
				parent.teardown_cluster_node (this);

				foreach (var id in registrations)
					connection.unregister_object (id);
				registrations.clear ();
			}

			public async void join (HostApplicationInfo app, SpawnStartState current_state,
					AgentSessionId[] interrupted_sessions, HashTable<string, Variant> options,
					Cancellable? cancellable, out SpawnStartState next_state) throws Error, IOError {
				yield parent.handle_join_request (this, app, current_state, interrupted_sessions, options,
					cancellable, out next_state);
			}

			public async void open_session (AgentSessionId id, HashTable<string, Variant> options,
					Cancellable? cancellable) throws Error, IOError {
				try {
					yield session_provider.open (id, options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				sessions.add (id);
			}

			private void on_session_closed (AgentSessionId id) {
				if (sessions.remove (id))
					session_closed (id);
			}
		}

		private class PendingSpawn {
			public ClusterNode node;
			public HostSpawnInfo info;
			public Gee.Set<ControlChannel> pending_approvers = new Gee.HashSet<ControlChannel> ();

			public PendingSpawn (ClusterNode n, uint pid, string identifier, Gee.Iterator<ControlChannel> gaters) {
				node = n;
				info = HostSpawnInfo (pid, identifier);
				pending_approvers.add_all_iterator (gaters);
			}
		}

		private class AgentSessionEntry {
			public signal void expired ();

			public ClusterNode? node {
				get;
				private set;
			}

			public ControlChannel? controller {
				get;
				private set;
			}

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

			public Cancellable io_cancellable {
				get;
				private set;
			}

			private Gee.Collection<uint> node_registrations = new Gee.ArrayList<uint> ();
			private Gee.Collection<uint> controller_registrations = new Gee.ArrayList<uint> ();

			private TimeoutSource? expiry_timer;

			public AgentSessionEntry (ClusterNode node, ControlChannel controller, AgentSessionId id, uint persist_timeout,
					Cancellable io_cancellable) {
				this.node = node;
				this.controller = controller;
				this.id = id;
				this.persist_timeout = persist_timeout;
				this.io_cancellable = io_cancellable;
			}

			~AgentSessionEntry () {
				stop_expiry_timer ();
				unregister_all ();
			}

			public void detach_node_and_controller () {
				unregister_all ();
				session = null;
				controller = null;
				node = null;

				start_expiry_timer ();
			}

			public void attach_node (ClusterNode n) {
				assert (node == null);
				node = n;
			}

			public void detach_controller () {
				unregister_all ();
				controller = null;

				start_expiry_timer ();
			}

			public void attach_controller (ControlChannel c) {
				stop_expiry_timer ();

				assert (node != null);
				assert (controller == null);
				controller = c;
			}

			public void take_node_registration (uint id) {
				node_registrations.add (id);
			}

			public void take_controller_registration (uint id) {
				controller_registrations.add (id);
			}

			private void unregister_all () {
				clear_controller_registrations ();
				clear_node_registrations ();
			}

			private void clear_controller_registrations () {
				if (controller != null)
					unregister_all_in (controller_registrations, controller.connection);
			}

			public void clear_node_registrations () {
				if (node != null)
					unregister_all_in (node_registrations, node.connection);
			}

			private void unregister_all_in (Gee.Collection<uint> ids, DBusConnection connection) {
				foreach (uint id in ids)
					connection.unregister_object (id);
				ids.clear ();
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
	}
}
