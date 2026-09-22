namespace Frida {
	public sealed class AgentContainer : Object, AgentSessionProvider {
		public DBusConnection connection {
			get;
			private set;
		}

		private Module module;
		[CCode (has_target = false)]
		private delegate void AgentMainFunc (string data, ref UnloadPolicy unload_policy, void * injector_state);
		private AgentMainFunc main_impl;
#if LINUX
		private FileDescriptor agent_ctrlfd_for_peer;
#else
		private PipeTransport transport;
#endif
		private string transport_address;
		private Thread<bool> thread;
		private AgentSessionProvider provider;

		public static async AgentContainer create (string agent_filename, Cancellable? cancellable) throws Error, IOError {
			var container = new AgentContainer ();

			try {
				container.module = new Module (agent_filename, 0);
			} catch (ModuleError e) {
				throw new Error.PERMISSION_DENIED ("%s", e.message);
			}

			void * main_func_symbol;
			var main_func_found = container.module.symbol ("frida_agent_main", out main_func_symbol);
			assert (main_func_found);
			container.main_impl = (AgentMainFunc) main_func_symbol;

			Future<IOStream> stream_request;
#if LINUX
			int agent_ctrlfds[2];
			if (Posix.socketpair (Posix.AF_UNIX, Posix.SOCK_STREAM, 0, agent_ctrlfds) != 0)
				throw new Error.NOT_SUPPORTED ("Unable to allocate socketpair");
			var agent_ctrlfd = new FileDescriptor (agent_ctrlfds[0]);
			container.agent_ctrlfd_for_peer = new FileDescriptor (agent_ctrlfds[1]);
			container.transport_address = "";

			try {
				Socket socket = new Socket.from_fd (agent_ctrlfd.handle);
				agent_ctrlfd.steal ();
				var promise = new Promise<IOStream> ();
				promise.resolve (SocketConnection.factory_create_connection (socket));
				stream_request = promise.future;
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
#else
			var transport = new PipeTransport ();
			container.transport = transport;
			container.transport_address = transport.remote_address;

			stream_request = Pipe.open (transport.local_address, cancellable);
#endif

			container.start_worker_thread ();

			DBusConnection connection;
			AgentSessionProvider provider;
			try {
				var stream = yield stream_request.wait_async (cancellable);

				connection = yield new DBusConnection (stream, ServerGuid.HOST_SESSION_SERVICE,
					AUTHENTICATION_SERVER | AUTHENTICATION_ALLOW_ANONYMOUS, null, cancellable);

				provider = yield connection.get_proxy (null, ObjectPath.AGENT_SESSION_PROVIDER, DO_NOT_LOAD_PROPERTIES,
					cancellable);
				provider.opened.connect (container.on_session_opened);
				provider.closed.connect (container.on_session_closed);
			} catch (GLib.Error e) {
				throw new Error.PERMISSION_DENIED ("%s", e.message);
			}

			container.connection = connection;
			container.provider = provider;

			return container;
		}

		public async void destroy (Cancellable? cancellable) throws IOError {
			provider.opened.disconnect (on_session_opened);
			provider.closed.disconnect (on_session_closed);
			provider = null;

			try {
				yield connection.close (cancellable);
			} catch (GLib.Error connection_error) {
			}
			connection = null;

			stop_worker_thread ();

#if !LINUX
			transport = null;
#endif

			module = null;
		}

		private void start_worker_thread () {
			thread = new Thread<bool> ("frida-agent-container", run);
		}

		private void stop_worker_thread () {
			Thread<bool> t = thread;
			t.join ();
			thread = null;
		}

		private bool run () {
			UnloadPolicy unload_policy = IMMEDIATE;
			void * injector_state = null;

#if LINUX
			var s = LinuxInjectorState ();
			s.frida_ctrlfd = -1;
			s.agent_ctrlfd = agent_ctrlfd_for_peer.steal ();
			injector_state = &s;
#endif

			string agent_parameters = transport_address + "|exit-monitor:off|thread-suspend-monitor:off";

			main_impl (agent_parameters, ref unload_policy, injector_state);

			return true;
		}

		public async void open (AgentSessionId id, HashTable<string, Variant> options, Cancellable? cancellable) throws GLib.Error {
			yield provider.open (id, options, cancellable);
		}

#if !WINDOWS
		private async void migrate (AgentSessionId id, Socket to_socket, Cancellable? cancellable) throws GLib.Error {
			yield provider.migrate (id, to_socket, cancellable);
		}
#endif

		public async void unload (Cancellable? cancellable) throws GLib.Error {
			yield provider.unload (cancellable);
		}

		private void on_session_opened (AgentSessionId id) {
			opened (id);
		}

		private void on_session_closed (AgentSessionId id) {
			closed (id);
		}
	}
}
