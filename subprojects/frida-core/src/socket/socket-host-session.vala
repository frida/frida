namespace Frida {
	public sealed class SocketHostSessionBackend : Object, HostSessionBackend {
		private SocketHostSessionProvider provider;

		public async void start (Cancellable? cancellable) throws IOError {
			provider = new SocketHostSessionProvider ();
			provider_available (provider);
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			provider_unavailable (provider);
			yield provider.close (cancellable);
			provider = null;
		}
	}

	public sealed class SocketHostSessionProvider : Object, HostSessionProvider {
		public string id {
			get { return "socket"; }
		}

		public string name {
			get { return _name; }
		}
		private string _name = "Local Socket";

		public Variant? icon {
			get { return _icon; }
		}
		private Variant _icon;

		public HostSessionProviderKind kind {
			get { return HostSessionProviderKind.REMOTE; }
		}

		private Gee.Set<HostEntry> hosts = new Gee.HashSet<HostEntry> ();

		private Cancellable io_cancellable = new Cancellable ();

		construct {
			_icon = make_provider_icon (Frida.Data.Icons.get_socket_png_blob ().data);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			while (!hosts.is_empty) {
				var iterator = hosts.iterator ();
				iterator.next ();
				HostEntry entry = iterator.get ();

				hosts.remove (entry);

				yield destroy_host_entry (entry, APPLICATION_REQUESTED, cancellable);
			}

			io_cancellable.cancel ();
		}

		public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			string? raw_address = null;
			TlsCertificate? certificate = null;
			string? origin = null;
			string? token = null;
			int keepalive_interval = -1;
			if (options != null) {
				var opts = options.map;

				Value? address_val = opts["address"];
				if (address_val != null)
					raw_address = address_val.get_string ();

				Value? cert_val = opts["certificate"];
				if (cert_val != null)
					certificate = (TlsCertificate) cert_val.get_object ();

				Value? origin_val = opts["origin"];
				if (origin_val != null)
					origin = origin_val.get_string ();

				Value? token_val = opts["token"];
				if (token_val != null)
					token = token_val.get_string ();

				Value? keepalive_interval_val = opts["keepalive_interval"];
				if (keepalive_interval_val != null)
					keepalive_interval = keepalive_interval_val.get_int ();
			}
			SocketConnectable connectable = parse_control_address (raw_address);

			connecting ("Connecting to frida-server", 0.0);
			SocketConnection socket_connection;
			try {
				var client = new SocketClient ();
				socket_connection = yield client.connect_async (connectable, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CONNECTION_REFUSED)
					throw new Error.SERVER_NOT_RUNNING ("Unable to connect to remote frida-server");
				else
					throw new Error.SERVER_NOT_RUNNING ("Unable to connect to remote frida-server: %s", e.message);
			}

			Socket socket = socket_connection.socket;
			SocketFamily family = socket.get_family ();

			if (family != UNIX)
				Tcp.enable_nodelay (socket);

			if (keepalive_interval == -1)
				keepalive_interval = (family == UNIX) ? 0 : 30;

			IOStream stream = socket_connection;

			if (certificate != null) {
				connecting ("Performing the TLS handshake", 0.25);
				try {
					var tc = TlsClientConnection.new (stream, connectable);
					tc.set_database (null);
					var accept_handler = tc.accept_certificate.connect ((peer_cert, errors) => {
						return peer_cert.verify (null, certificate) == 0;
					});
					try {
						yield tc.handshake_async (Priority.DEFAULT, cancellable);
					} finally {
						tc.disconnect (accept_handler);
					}
					stream = tc;
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("%s", e.message);
				}
			}

			var transport = (certificate != null) ? WebServiceTransport.TLS : WebServiceTransport.PLAIN;
			string host = (raw_address != null) ? raw_address : "lolcathost";

			connecting ("Negotiating the connection", 0.5);
			stream = yield negotiate_connection (stream, transport, host, origin, cancellable);

			DBusConnection connection;
			try {
				connection = yield new DBusConnection (stream, null, DBusConnectionFlags.NONE, null, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}

			if (token != null) {
				connecting ("Authenticating", 0.75);
				AuthenticationService auth_service;
				try {
					auth_service = yield connection.get_proxy (null, ObjectPath.AUTHENTICATION_SERVICE,
						DO_NOT_LOAD_PROPERTIES, cancellable);
				} catch (IOError e) {
					throw new Error.PROTOCOL ("Incompatible frida-server version");
				}

				try {
					yield auth_service.authenticate (token, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			HostSession host_session;
			try {
				host_session = yield connection.get_proxy (null, ObjectPath.HOST_SESSION, DO_NOT_LOAD_PROPERTIES,
					cancellable);
			} catch (IOError e) {
				throw new Error.PROTOCOL ("Incompatible frida-server version");
			}

			var entry = new HostEntry (connection, host_session, keepalive_interval);
			entry.agent_session_detached.connect (on_agent_session_detached);
			hosts.add (entry);

			connection.on_closed.connect (on_host_connection_closed);

			return host_session;
		}

		public async void destroy (HostSession host_session, Cancellable? cancellable) throws Error, IOError {
			foreach (var entry in hosts) {
				if (entry.host_session == host_session) {
					hosts.remove (entry);
					yield destroy_host_entry (entry, APPLICATION_REQUESTED, cancellable);
					return;
				}
			}
			throw new Error.INVALID_ARGUMENT ("Invalid host session");
		}

		private async void destroy_host_entry (HostEntry entry, SessionDetachReason reason,
				Cancellable? cancellable) throws IOError {
			entry.connection.on_closed.disconnect (on_host_connection_closed);

			yield entry.destroy (reason, cancellable);

			entry.agent_session_detached.disconnect (on_agent_session_detached);

			host_session_detached (entry.host_session);
		}

		private HostEntry get_host_entry_by_session (HostSession session) throws Error {
			var entry = find_host_entry_by_session (session);
			if (entry == null)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");
			return entry;
		}

		private HostEntry? find_host_entry_by_session (HostSession session) {
			foreach (var entry in hosts) {
				if (entry.host_session == session)
					return entry;
			}
			return null;
		}

		private void on_host_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			bool closed_by_us = (!remote_peer_vanished && error == null);
			if (closed_by_us)
				return;

			HostEntry entry_to_remove = null;
			foreach (var entry in hosts) {
				if (entry.connection == connection) {
					entry_to_remove = entry;
					break;
				}
			}
			assert (entry_to_remove != null);

			hosts.remove (entry_to_remove);
			destroy_host_entry.begin (entry_to_remove, CONNECTION_TERMINATED, io_cancellable);
		}

		public async AgentSession link_agent_session (HostSession host_session, AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			var entry = get_host_entry_by_session (host_session);
			return yield entry.link_agent_session (id, sink, cancellable);
		}

		public void unlink_agent_session (HostSession host_session, AgentSessionId id) {
			var entry = find_host_entry_by_session (host_session);
			if (entry != null)
				entry.unlink_agent_session (id);
		}

		public async IOStream link_channel (HostSession host_session, ChannelId id, Cancellable? cancellable)
				throws Error, IOError {
			var entry = get_host_entry_by_session (host_session);
			return yield entry.link_channel (id, cancellable);
		}

		public void unlink_channel (HostSession host_session, ChannelId id) {
			var entry = find_host_entry_by_session (host_session);
			if (entry != null)
				entry.unlink_channel (id);
		}

		public async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id, Cancellable? cancellable)
				throws Error, IOError {
			var entry = get_host_entry_by_session (host_session);
			return yield entry.link_service_session (id, cancellable);
		}

		public void unlink_service_session (HostSession host_session, ServiceSessionId id) {
			var entry = find_host_entry_by_session (host_session);
			if (entry != null)
				entry.unlink_service_session (id);
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			agent_session_detached (id, reason, crash);
		}

		private class HostEntry : Object {
			public signal void agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash);

			public DBusConnection connection {
				get;
				construct;
			}

			public HostSession host_session {
				get;
				construct;
			}

			public uint keepalive_interval {
				get;
				construct;
			}

			private TimeoutSource? keepalive_timer;

			private Gee.HashMap<AgentSessionId?, AgentSessionEntry> agent_sessions =
				new Gee.HashMap<AgentSessionId?, AgentSessionEntry> (AgentSessionId.hash, AgentSessionId.equal);
			private ChannelRegistry channel_registry = new ChannelRegistry ();
			private ServiceSessionRegistry service_session_registry = new ServiceSessionRegistry ();

			private Cancellable io_cancellable = new Cancellable ();

			public HostEntry (DBusConnection connection, HostSession host_session, uint keepalive_interval) {
				Object (
					connection: connection,
					host_session: host_session,
					keepalive_interval: keepalive_interval
				);

				host_session.agent_session_detached.connect (on_agent_session_detached);
			}

			construct {
				if (keepalive_interval != 0) {
					var source = new TimeoutSource.seconds (keepalive_interval);
					source.set_callback (on_keepalive_tick);
					source.attach (MainContext.get_thread_default ());
					keepalive_timer = source;

					on_keepalive_tick ();
				}
			}

			public async void destroy (SessionDetachReason reason, Cancellable? cancellable) throws IOError {
				io_cancellable.cancel ();

				if (keepalive_timer != null) {
					keepalive_timer.destroy ();
					keepalive_timer = null;
				}

				host_session.agent_session_detached.disconnect (on_agent_session_detached);

				var no_crash = CrashInfo.empty ();
				foreach (AgentSessionId id in agent_sessions.keys)
					agent_session_detached (id, reason, no_crash);
				agent_sessions.clear ();

				channel_registry.clear ();

				service_session_registry.clear ();

				if (reason != CONNECTION_TERMINATED) {
					try {
						yield connection.close (cancellable);
					} catch (GLib.Error e) {
					}
				}
			}

			public async AgentSession link_agent_session (AgentSessionId id, AgentMessageSink sink,
					Cancellable? cancellable) throws Error, IOError {
				if (agent_sessions.has_key (id))
					throw new Error.INVALID_OPERATION ("Already linked");

				var entry = new AgentSessionEntry (connection);
				agent_sessions[id] = entry;

				AgentSession session = yield connection.get_proxy (null, ObjectPath.for_agent_session (id),
					DO_NOT_LOAD_PROPERTIES, cancellable);

				entry.sink_registration_id = connection.register_object (ObjectPath.for_agent_message_sink (id), sink);

				return session;
			}

			public void unlink_agent_session (AgentSessionId id) {
				AgentSessionEntry? entry = agent_sessions[id];
				if (entry == null || entry.sink_registration_id == 0)
					return;

				entry.connection.unregister_object (entry.sink_registration_id);
				entry.sink_registration_id = 0;
			}

			public async IOStream link_channel (ChannelId id, Cancellable? cancellable) throws Error, IOError {
				Channel channel = yield connection.get_proxy (null, ObjectPath.for_channel (id),
					DO_NOT_LOAD_PROPERTIES, cancellable);

				var stream = new ChannelStream (channel);
				channel_registry.register (id, stream);

				return stream;
			}

			public void unlink_channel (ChannelId id) {
				channel_registry.unlink (id);
			}

			public async ServiceSession link_service_session (ServiceSessionId id, Cancellable? cancellable)
					throws Error, IOError {
				ServiceSession session = yield connection.get_proxy (null, ObjectPath.for_service_session (id),
					DO_NOT_LOAD_PROPERTIES, cancellable);
				service_session_registry.register (id, session);
				return session;
			}

			public void unlink_service_session (ServiceSessionId id) {
				service_session_registry.unlink (id);
			}

			private bool on_keepalive_tick () {
				host_session.ping.begin (keepalive_interval, io_cancellable);
				return true;
			}

			private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
				agent_sessions.unset (id);
				agent_session_detached (id, reason, crash);
			}
		}

		private class AgentSessionEntry {
			public DBusConnection connection {
				get;
				set;
			}

			public uint sink_registration_id {
				get;
				set;
			}

			public AgentSessionEntry (DBusConnection connection) {
				this.connection = connection;
			}

			~AgentSessionEntry () {
				if (sink_registration_id != 0)
					connection.unregister_object (sink_registration_id);
			}
		}
	}
}
