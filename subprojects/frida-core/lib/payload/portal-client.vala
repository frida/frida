namespace Frida {
	public sealed class PortalClient : Object, AgentSessionProvider {
		public signal void resume ();
		public signal void kill ();

		public weak ProcessInvader invader {
			get;
			construct;
		}

		public SocketConnectable connectable {
			get;
			construct;
		}

		public string host {
			get;
			construct;
		}

		public TlsCertificate? certificate {
			get;
			construct;
		}

		public string? token {
			get;
			construct;
		}

		public string[]? acl {
			get;
			construct;
		}

		public HostApplicationInfo app_info {
			get;
			construct;
		}

		private DBusConnection? connection;
		private SourceFunc? on_connection_event;
		private TimeoutSource? reconnect_timer;
		private Promise<bool> stopped = new Promise<bool> ();
		private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();
		private PortalSession? portal_session;
		private Gee.Map<AgentSessionId?, LiveAgentSession> agent_sessions =
			new Gee.HashMap<AgentSessionId?, LiveAgentSession> (AgentSessionId.hash, AgentSessionId.equal);

		private Gee.Collection<Gum.Script> eternalized_scripts = new Gee.ArrayList<Gum.Script> ();

		private Cancellable io_cancellable = new Cancellable ();

		public PortalClient (ProcessInvader invader, SocketConnectable connectable, string host, TlsCertificate? certificate, string? token,
				string[]? acl, HostApplicationInfo app_info) {
			Object (
				invader: invader,
				connectable: connectable,
				host: host,
				certificate: certificate,
				token: token,
				acl: acl,
				app_info: app_info
			);
		}

		public async void start (Cancellable? cancellable = null) throws Error, IOError {
			var promise = new Promise<bool> ();

			maintain_connection.begin (promise);

			yield promise.future.wait_async (cancellable);
		}

		public async void stop (Cancellable? cancellable = null) throws IOError {
			if (reconnect_timer != null) {
				reconnect_timer.destroy ();
				reconnect_timer = null;
			}

			io_cancellable.cancel ();

			if (on_connection_event != null)
				on_connection_event ();

			try {
				yield stopped.future.wait_async (cancellable);
				yield teardown_connection (cancellable);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
		}

		private async void maintain_connection (Promise<bool> start_request) {
			bool waiting = false;
			on_connection_event = () => {
				if (waiting)
					maintain_connection.callback ();
				return false;
			};

			uint reconnect_delay = 0;

			do {
				try {
					yield establish_connection ();

					if (start_request != null) {
						start_request.resolve (true);
						start_request = null;
					}

					reconnect_delay = 0;

					waiting = true;
					yield;
					waiting = false;
				} catch (GLib.Error e) {
					if (start_request != null) {
						DBusError.strip_remote_error (e);
						GLib.Error start_error = (e is Error || e is IOError.CANCELLED)
							? e
							: new Error.TRANSPORT ("%s", e.message);
						start_request.reject (start_error);
						start_request = null;
						break;
					}
				}

				if (io_cancellable.is_cancelled ())
					break;

				var source = new TimeoutSource (reconnect_delay + Random.int_range (0, 3000));
				source.set_callback (() => {
					maintain_connection.callback ();
					return false;
				});
				source.attach (MainContext.get_thread_default ());
				reconnect_timer = source;
				waiting = true;
				yield;
				waiting = false;

				reconnect_delay = (reconnect_delay != 0)
					? uint.min (reconnect_delay * 2, 17000)
					: 2000;
			} while (!io_cancellable.is_cancelled ());

			on_connection_event = null;

			stopped.resolve (true);
		}

		private async void establish_connection () throws GLib.Error {
			var client = new SocketClient ();
			SocketConnection socket_connection = yield client.connect_async (connectable, io_cancellable);

			var socket = socket_connection.socket;
			if (socket.get_family () != UNIX)
				Tcp.enable_nodelay (socket);

			IOStream stream = socket_connection;

			if (certificate != null) {
				var tc = TlsClientConnection.new (stream, connectable);
				tc.set_database (null);
				var accept_handler = tc.accept_certificate.connect ((peer_cert, errors) => {
					return peer_cert.verify (null, certificate) == 0;
				});
				try {
					yield tc.handshake_async (Priority.DEFAULT, io_cancellable);
				} finally {
					tc.disconnect (accept_handler);
				}
				stream = tc;
			}

			var transport = (certificate != null) ? WebServiceTransport.TLS : WebServiceTransport.PLAIN;
			string? origin = null;

			stream = yield negotiate_connection (stream, transport, host, origin, io_cancellable);

			connection = yield new DBusConnection (stream, null, DELAY_MESSAGE_PROCESSING, null, io_cancellable);
			connection.on_closed.connect (on_connection_closed);

			AgentSessionProvider provider = this;
			registrations.add (connection.register_object (ObjectPath.AGENT_SESSION_PROVIDER, provider));

			connection.start_message_processing ();

			if (token != null) {
				AuthenticationService auth_service = yield connection.get_proxy (null, ObjectPath.AUTHENTICATION_SERVICE,
					DO_NOT_LOAD_PROPERTIES, io_cancellable);
				yield auth_service.authenticate (token, io_cancellable);
			}

			portal_session = yield connection.get_proxy (null, ObjectPath.PORTAL_SESSION, DO_NOT_LOAD_PROPERTIES,
				io_cancellable);
			portal_session.resume.connect (on_resume);
			portal_session.kill.connect (on_kill);

			SpawnStartState current_state = invader.query_current_spawn_state ();
			SpawnStartState next_state;

			var interrupted_sessions = new AgentSessionId[0];
			foreach (LiveAgentSession session in agent_sessions.values.to_array ()) {
				AgentSessionId id = session.id;

				assert (session.persist_timeout != 0);
				interrupted_sessions += id;

				try {
					session.message_sink = yield connection.get_proxy (null, ObjectPath.for_agent_message_sink (id),
						DO_NOT_LOAD_PROPERTIES, io_cancellable);
				} catch (IOError e) {
					throw_dbus_error (e);
				}

				assert (session.registration_id == 0);
				try {
					session.registration_id = connection.register_object (ObjectPath.for_agent_session (id),
						(AgentSession) session);
				} catch (IOError io_error) {
					assert_not_reached ();
				}
			}

			HashTable<string, Variant> options = make_parameters_dict ();
			if (acl != null)
				options["acl"] = new Variant.strv (acl);

			yield portal_session.join (app_info, current_state, interrupted_sessions, options, io_cancellable, out next_state);

			if (next_state == RUNNING)
				resume ();
		}

		private void on_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			teardown_connection.begin (null);
		}

		private async void teardown_connection (Cancellable? cancellable) throws IOError {
			if (connection == null)
				return;

			bool stopping = io_cancellable.is_cancelled ();

			foreach (var session in agent_sessions.values.to_array ()) {
				if (!stopping && session.persist_timeout != 0) {
					unregister_session (session);
					session.interrupt.begin (io_cancellable);
					continue;
				}

				try {
					yield session.close (cancellable);
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					throw (IOError) e;
				}
			}

			foreach (var id in registrations)
				connection.unregister_object (id);
			registrations.clear ();

			connection = null;

			if (on_connection_event != null)
				on_connection_event ();
		}

		private async void open (AgentSessionId id, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var opts = SessionOptions._deserialize (options);

			if (opts.realm == EMULATED)
				throw new Error.NOT_SUPPORTED ("Emulated realm is not supported by frida-gadget");

			AgentMessageSink sink;
			try {
				sink = yield connection.get_proxy (null, ObjectPath.for_agent_message_sink (id), DO_NOT_LOAD_PROPERTIES,
					cancellable);
			} catch (IOError e) {
				throw_dbus_error (e);
			}

			MainContext dbus_context = yield get_dbus_context ();

			LiveAgentSession? session = agent_sessions[id];
			if (session != null)
				throw new Error.INVALID_ARGUMENT ("Session already exists");
			session = new LiveAgentSession (invader, id, opts.persist_timeout, sink, dbus_context);
			agent_sessions[id] = session;
			session.closed.connect (on_session_closed);
			session.script_eternalized.connect (on_script_eternalized);

			try {
				session.registration_id = connection.register_object (ObjectPath.for_agent_session (id),
					(AgentSession) session);
			} catch (IOError io_error) {
				assert_not_reached ();
			}

			opened (id);
		}

#if !WINDOWS
		private async void migrate (AgentSessionId id, Socket to_socket, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Session migration is not supported with frida-portal");
		}
#endif

		private async void unload (Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Unload is not allowed with frida-portal");
		}

		private void on_resume () {
			resume ();
		}

		private void on_kill () {
			kill ();
		}

		private void on_session_closed (BaseAgentSession base_session) {
			LiveAgentSession session = (LiveAgentSession) base_session;

			closed (session.id);

			unregister_session (session);

			session.script_eternalized.disconnect (on_script_eternalized);
			session.closed.disconnect (on_session_closed);
			agent_sessions.unset (session.id);
		}

		private void unregister_session (LiveAgentSession session) {
			var id = session.registration_id;
			if (id != 0) {
				connection.unregister_object (id);
				session.registration_id = 0;
			}
		}

		private void on_script_eternalized (Gum.Script script) {
			eternalized_scripts.add (script);
			eternalized ();
		}

		private class LiveAgentSession : BaseAgentSession {
			public uint registration_id {
				get;
				set;
			}

			public LiveAgentSession (ProcessInvader invader, AgentSessionId id, uint persist_timeout, AgentMessageSink sink,
					MainContext dbus_context) {
				Object (
					invader: invader,
					id: id,
					persist_timeout: persist_timeout,
					message_sink: sink,
					frida_context: MainContext.ref_thread_default (),
					dbus_context: dbus_context
				);
			}
		}
	}
}
