namespace Frida {
	public sealed class HostSessionService : Object {
		private Gee.ArrayList<HostSessionBackend> backends = new Gee.ArrayList<HostSessionBackend> ();

		public signal void provider_available (HostSessionProvider provider);
		public signal void provider_unavailable (HostSessionProvider provider);

		private delegate void NotifyCompleteFunc ();

		public HostSessionService.with_default_backends () {
			add_local_backends ();
			add_nonlocal_backends ();
		}

		public HostSessionService.with_local_backend_only () {
			add_local_backends ();
		}

		public HostSessionService.with_nonlocal_backends_only () {
			add_nonlocal_backends ();
		}

		public HostSessionService.with_socket_backend_only () {
#if HAVE_SOCKET_BACKEND
			add_backend (new SocketHostSessionBackend ());
#endif
		}

		private void add_local_backends () {
#if HAVE_LOCAL_BACKEND
#if WINDOWS
			add_backend (new WindowsHostSessionBackend ());
#endif
#if DARWIN
			add_backend (new DarwinHostSessionBackend ());
#endif
#if LINUX
			add_backend (new LinuxHostSessionBackend ());
#endif
#if FREEBSD
			add_backend (new FreebsdHostSessionBackend ());
#endif
#if QNX
			add_backend (new QnxHostSessionBackend ());
#endif
#endif
		}

		private void add_nonlocal_backends () {
#if HAVE_SIMMY_BACKEND
			add_backend (new SimmyHostSessionBackend ());
#endif
#if !IOS && !ANDROID && !TVOS
#if HAVE_FRUITY_BACKEND
			add_backend (new FruityHostSessionBackend ());
#endif
#if HAVE_DROIDY_BACKEND
			add_backend (new DroidyHostSessionBackend ());
#endif
#endif
#if HAVE_SOCKET_BACKEND
			add_backend (new SocketHostSessionBackend ());
#endif
#if HAVE_BAREBONE_BACKEND
			add_backend (new BareboneHostSessionBackend ());
#endif
		}

		public async void start (Cancellable? cancellable = null) throws IOError {
			var remaining = backends.size + 1;

			NotifyCompleteFunc on_complete = () => {
				remaining--;
				if (remaining == 0)
					start.callback ();
			};

			foreach (var backend in backends)
				perform_start.begin (backend, cancellable, on_complete);

			var source = new IdleSource ();
			source.set_callback (() => {
				on_complete ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());

			yield;

			on_complete = null;
		}

		public async void stop (Cancellable? cancellable = null) throws IOError {
			var remaining = backends.size + 1;

			NotifyCompleteFunc on_complete = () => {
				remaining--;
				if (remaining == 0)
					stop.callback ();
			};

			foreach (var backend in backends)
				perform_stop.begin (backend, cancellable, on_complete);

			var source = new IdleSource ();
			source.set_callback (() => {
				on_complete ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());

			yield;

			on_complete = null;
		}

		private async void perform_start (HostSessionBackend backend, Cancellable? cancellable, NotifyCompleteFunc on_complete) {
			try {
				yield backend.start (cancellable);
			} catch (IOError e) {
			}

			on_complete ();
		}

		private async void perform_stop (HostSessionBackend backend, Cancellable? cancellable, NotifyCompleteFunc on_complete) {
			try {
				yield backend.stop (cancellable);
			} catch (IOError e) {
			}

			on_complete ();
		}

		public void add_backend (HostSessionBackend backend) {
			backends.add (backend);
			backend.provider_available.connect ((provider) => {
				provider_available (provider);
			});
			backend.provider_unavailable.connect ((provider) => {
				provider_unavailable (provider);
			});
		}

		public void remove_backend (HostSessionBackend backend) {
			backends.remove (backend);
		}
	}

	public interface HostSessionProvider : Object {
		public abstract string id {
			get;
		}

		public abstract string name {
			get;
		}

		public abstract Variant? icon {
			get;
		}

		public abstract HostSessionProviderKind kind {
			get;
		}

		public abstract async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
			Cancellable? cancellable) throws Error, IOError;
		public abstract async void destroy (HostSession session, Cancellable? cancellable = null) throws Error, IOError;
		public signal void host_session_detached (HostSession session);

		public abstract async AgentSession link_agent_session (HostSession host_session, AgentSessionId id, AgentMessageSink sink,
			Cancellable? cancellable = null) throws Error, IOError;
		public abstract void unlink_agent_session (HostSession host_session, AgentSessionId id);
		public signal void agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash);

		public abstract async IOStream link_channel (HostSession host_session, ChannelId id, Cancellable? cancellable = null)
			throws Error, IOError;
		public abstract void unlink_channel (HostSession host_session, ChannelId id);

		public abstract async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id,
			Cancellable? cancellable = null) throws Error, IOError;
		public abstract void unlink_service_session (HostSession host_session, ServiceSessionId id);
	}

	public delegate void ConnectingFunc (string status, double progress);

	private const uint16 PROVIDER_ICON_SIZE = 96;

	internal Variant make_provider_icon (uint8[] png) {
		return icon_from_png (png, PROVIDER_ICON_SIZE, PROVIDER_ICON_SIZE);
	}

	public interface HostSessionConnection : Object {
		public abstract HostSession host_session {
			get;
		}

		public abstract async AgentSession link_agent_session (AgentSessionId id, AgentMessageSink sink,
			Cancellable? cancellable = null) throws Error, IOError;
		public abstract void unlink_agent_session (AgentSessionId id);
	}

	public interface HostSessionHub : Object {
		public abstract async HostSessionEntry resolve_host_session (string id, Cancellable? cancellable) throws Error, IOError;
	}

	public class NullHostSessionHub : Object, HostSessionHub {
		public async HostSessionEntry resolve_host_session (string id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Host session lookup not supported");
		}
	}

	public class HostSessionEntry : Object {
		public HostSessionProvider provider {
			get;
			construct;
		}

		public HostSession session {
			get;
			construct;
		}

		public HostSessionEntry (HostSessionProvider provider, HostSession session) {
			Object (provider: provider, session: session);
		}
	}

	public enum HostSessionProviderKind {
		LOCAL,
		REMOTE,
		USB
	}

	public sealed class HostSessionOptions : Object {
		public Gee.Map<string, Value?> map {
			get;
			set;
			default = new Gee.HashMap<string, Value?> ();
		}

		public HostSessionOptions copy () {
			var opts = new HostSessionOptions ();
			opts.map.set_all (map);
			return opts;
		}
	}

	public interface HostChannelProvider : Object {
		public abstract async IOStream open_channel (string address, Cancellable? cancellable = null) throws Error, IOError;
	}

	public interface Pairable : Object {
		public abstract async void unpair (Cancellable? cancellable = null) throws Error, IOError;
	}

	public interface HostSessionBackend : Object {
		public signal void provider_available (HostSessionProvider provider);
		public signal void provider_unavailable (HostSessionProvider provider);

		public abstract async void start (Cancellable? cancellable = null) throws IOError;
		public abstract async void stop (Cancellable? cancellable = null) throws IOError;
	}

	public abstract class LocalHostSessionBackend : Object, HostSessionBackend {
		private LocalHostSessionProvider local_provider;

		public async void start (Cancellable? cancellable) throws IOError {
			assert (local_provider == null);
			local_provider = make_provider ();

			provider_available (local_provider);
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			assert (local_provider != null);

			provider_unavailable (local_provider);

			yield local_provider.close (cancellable);
			local_provider = null;
		}

		protected abstract LocalHostSessionProvider make_provider ();
	}

	public abstract class LocalHostSessionProvider : Object, HostSessionProvider {
		public string id {
			get { return "local"; }
		}

		public string name {
			get { return "Local System"; }
		}

		public Variant? icon {
			get { return _icon; }
		}
		private Variant? _icon;

		public HostSessionProviderKind kind {
			get { return HostSessionProviderKind.LOCAL; }
		}

		private LocalHostSession host_session;

		construct {
			_icon = load_icon ();
		}

		protected abstract LocalHostSession make_host_session (HostSessionOptions? options) throws Error;

		protected void take_host_session (LocalHostSession session) {
			host_session = session;
			host_session.agent_session_detached.connect (on_agent_session_detached);
		}

		protected virtual Variant? load_icon () {
			return make_provider_icon (Frida.Data.Icons.get_local_png_blob ().data);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (host_session == null)
				return;

			host_session.agent_session_detached.disconnect (on_agent_session_detached);

			yield host_session.close (cancellable);
			host_session = null;
		}

		public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != null)
				throw new Error.INVALID_OPERATION ("Already created");

			take_host_session (make_host_session (options));

			return host_session;
		}

		public async void destroy (HostSession session, Cancellable? cancellable) throws Error, IOError {
			if (session != host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			host_session.agent_session_detached.disconnect (on_agent_session_detached);

			yield host_session.close (cancellable);
			host_session = null;
		}

		public async AgentSession link_agent_session (HostSession host_session, AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != this.host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			return yield this.host_session.link_agent_session (id, sink, cancellable);
		}

		public void unlink_agent_session (HostSession host_session, AgentSessionId id) {
			if (host_session != this.host_session)
				return;

			this.host_session.unlink_agent_session (id);
		}

		public async IOStream link_channel (HostSession host_session, ChannelId id, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
		}

		public void unlink_channel (HostSession host_session, ChannelId id) {
		}

		public async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id, Cancellable? cancellable)
				throws Error, IOError {
			if (host_session != this.host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			return this.host_session.link_service_session (id);
		}

		public void unlink_service_session (HostSession host_session, ServiceSessionId id) {
			if (host_session != this.host_session)
				return;

			this.host_session.unlink_service_session (id);
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			agent_session_detached (id, reason, crash);
		}
	}

	public abstract class LocalHostSession : Object, HostSession, HostSessionConnection, AgentController {
		public HostSession host_session {
			get {
				return this;
			}
		}

		private Gee.HashMap<uint, Cancellable> pending_establish_ops = new Gee.HashMap<uint, Cancellable> ();

		private Gee.HashMap<uint, Future<AgentEntry>> agent_entries = new Gee.HashMap<uint, Future<AgentEntry>> ();

		private Gee.HashMap<AgentSessionId?, AgentSessionEntry> agent_sessions =
			new Gee.HashMap<AgentSessionId?, AgentSessionEntry> (AgentSessionId.hash, AgentSessionId.equal);

		private Gee.HashMap<HostChildId?, ChildEntry> child_entries =
			new Gee.HashMap<HostChildId?, ChildEntry> (HostChildId.hash, HostChildId.equal);
		private uint next_host_child_id = 1;
		private Gee.HashMap<uint, HostChildInfo?> pending_children = new Gee.HashMap<uint, HostChildInfo?> ();
		private Gee.HashMap<uint, SpawnAckRequest> pending_acks = new Gee.HashMap<uint, SpawnAckRequest> ();
		private Promise<bool> pending_children_gc_request;
		private Source pending_children_gc_timer;

		protected Injector injector;
		protected Gee.HashMap<uint, uint> injectee_by_pid = new Gee.HashMap<uint, uint> ();

		protected ServiceSessionRegistry service_session_registry = new ServiceSessionRegistry ();

		protected Cancellable io_cancellable = new Cancellable ();

		public virtual async void preload (Cancellable? cancellable) throws Error, IOError {
		}

		public virtual async void close (Cancellable? cancellable) throws IOError {
			if (pending_children_gc_timer != null) {
				pending_children_gc_timer.destroy ();
				pending_children_gc_timer = null;
			}

			if (pending_children_gc_request != null)
				yield garbage_collect_pending_children (cancellable);

			foreach (var ack_request in pending_acks.values)
				ack_request.complete ();
			pending_acks.clear ();

			while (!agent_entries.is_empty) {
				var iterator = agent_entries.values.iterator ();
				iterator.next ();
				var entry_future = iterator.get ();
				try {
					var entry = yield entry_future.wait_async (cancellable);

					var resume_request = entry.resume_request;
					if (resume_request != null) {
						resume_request.resolve (true);
						entry.resume_request = null;
					}

					yield destroy (entry, APPLICATION_REQUESTED, cancellable);
				} catch (Error e) {
				}
			}

			io_cancellable.cancel ();
		}

		protected abstract async AgentSessionProvider create_system_session_provider (Cancellable? cancellable,
			out DBusConnection connection) throws Error, IOError;

		public async void ping (uint interval_seconds, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable) throws Error, IOError {
			return compute_system_parameters ();
		}

		public abstract async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
			Cancellable? cancellable) throws Error, IOError;

		public abstract async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
			Cancellable? cancellable) throws Error, IOError;

		public abstract async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
			Cancellable? cancellable) throws Error, IOError;

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield enable_spawn_gating_with_options (make_parameters_dict (), cancellable);
		}

		public abstract async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
			Cancellable? cancellable) throws Error, IOError;

		public abstract async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError;

		public abstract async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError;

		public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws Error, IOError {
			var result = new HostChildInfo[pending_children.size];
			var index = 0;
			foreach (var child in pending_children.values)
				result[index++] = child;
			return result;
		}

		public abstract async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError;

		protected virtual bool try_handle_child (HostChildInfo info) {
			return false;
		}

		protected virtual void notify_child_resumed (uint pid) {
		}

		protected virtual void notify_child_gating_changed (uint pid, uint subscriber_count) {
		}

		protected virtual async void prepare_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
		}

		protected virtual async void await_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not supported on this OS");
		}

		protected virtual async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not supported on this OS");
		}

		protected abstract bool process_is_alive (uint pid);

		public abstract async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError;

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			if (yield try_resume_child (pid, cancellable))
				return;

			yield perform_resume (pid, cancellable);
		}

		private async bool try_resume_child (uint pid, Cancellable? cancellable) throws Error, IOError {
			HostChildInfo? info;
			if (pending_children.unset (pid, out info))
				child_removed (info);

			SpawnAckRequest ack_request;
			if (pending_acks.unset (pid, out ack_request)) {
				try {
					if (ack_request.start_state == RUNNING)
						yield perform_resume (pid, cancellable);
				} finally {
					ack_request.complete ();
				}

				notify_child_resumed (pid);

				return true;
			}

			var entry_future = agent_entries[pid];
			if (entry_future == null || !entry_future.ready)
				return false;

			var entry = entry_future.value;

			var resume_request = entry.resume_request;
			if (resume_request == null)
				return false;

			resume_request.resolve (true);
			entry.resume_request = null;

			if (entry.sessions.is_empty) {
				unload_and_destroy.begin (entry, APPLICATION_REQUESTED);
			}

			notify_child_resumed (pid);

			return true;
		}

		protected abstract async void perform_resume (uint pid, Cancellable? cancellable) throws Error, IOError;

		protected bool still_attached_to (uint pid) {
			return agent_entries.has_key (pid);
		}

		public abstract async void kill (uint pid, Cancellable? cancellable) throws Error, IOError;

		public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var raw_opts = options;
			var opts = SessionOptions._deserialize (raw_opts);

			if (opts.realm == EMULATED) {
				if (opts.emulated_agent_path == null) {
					opts.emulated_agent_path = get_emulated_agent_path (pid);
					raw_opts = opts._serialize ();
				}
			}

			var entry = yield establish (pid, raw_opts, cancellable);

			var id = AgentSessionId.generate ();
			entry.sessions.add (id);

			try {
				yield entry.provider.open (id, raw_opts, cancellable);
			} catch (GLib.Error e) {
				entry.sessions.remove (id);

				throw new Error.PROTOCOL ("%s", e.message);
			}

			agent_sessions[id] = new AgentSessionEntry (entry.connection);

			return id;
		}

		public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		private async AgentEntry establish (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			while (agent_entries.has_key (pid)) {
				var future = agent_entries[pid];
				try {
					return yield future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			var promise = new Promise<AgentEntry> ();
			agent_entries[pid] = promise.future;

			AgentEntry entry = null;
			CancellableSource? cancel_source = null;
			try {
				DBusConnection connection;
				AgentSessionProvider provider;

				if (pid == 0) {
					provider = yield create_system_session_provider (cancellable, out connection);
					entry = new AgentEntry (pid, null, connection, provider);
				} else {
					yield wait_for_uninject_of_pid (pid, cancellable);

					var io_cancellable = new Cancellable ();
					pending_establish_ops[pid] = io_cancellable;

					cancel_source = new CancellableSource (cancellable);
					cancel_source.set_callback (() => {
						io_cancellable.cancel ();
						return false;
					});
					cancel_source.attach (MainContext.get_thread_default ());

					Object transport;
					var stream_request = yield perform_attach_to (pid, options, io_cancellable, out transport);

					IOStream stream = yield stream_request.wait_async (io_cancellable);

					uint controller_registration_id;
					try {
						connection = yield new DBusConnection (stream, ServerGuid.HOST_SESSION_SERVICE,
							AUTHENTICATION_SERVER | AUTHENTICATION_ALLOW_ANONYMOUS | DELAY_MESSAGE_PROCESSING,
							null, io_cancellable);

						controller_registration_id = connection.register_object (ObjectPath.AGENT_CONTROLLER,
							(AgentController) this);

						connection.start_message_processing ();

						provider = yield connection.get_proxy (null, ObjectPath.AGENT_SESSION_PROVIDER,
							DO_NOT_LOAD_PROPERTIES, io_cancellable);
					} catch (GLib.Error e) {
						if (e is IOError.CANCELLED)
							throw e;
						else
							throw new Error.PROCESS_NOT_RESPONDING ("%s", e.message);
					}

					entry = new AgentEntry (pid, transport, connection, provider, controller_registration_id);
				}

				connection.on_closed.connect (on_agent_connection_closed);
				provider.closed.connect (on_agent_session_provider_closed);
				provider.eternalized.connect (on_agent_session_provider_eternalized);
				entry.child_gating_changed.connect (on_child_gating_changed);

				promise.resolve (entry);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED && (cancellable == null || !cancellable.is_cancelled ())) {
					e = new Error.PROCESS_NOT_RESPONDING ("Process with pid %u either refused to load frida-agent, " +
						"or terminated during injection", pid);
				}

				agent_entries.unset (pid);

				promise.reject (e);
				throw_api_error (e);
			} finally {
				pending_establish_ops.unset (pid);
				if (cancel_source != null)
					cancel_source.destroy ();
			}

			return entry;
		}

		internal async void wait_for_uninject_of_pid (uint pid, Cancellable? cancellable) throws IOError {
			yield wait_for_uninject (injector, cancellable, () => {
				return injectee_by_pid.has_key (pid);
			});
		}

		protected virtual void on_uninjected (uint id) {
			foreach (var entry in injectee_by_pid.entries) {
				if (entry.value == id) {
					uint pid = entry.key;

					injectee_by_pid.unset (pid);

					var io_cancellable = pending_establish_ops[pid];
					if (io_cancellable != null)
						io_cancellable.cancel ();

					return;
				}
			}

			uninjected (InjectorPayloadId (id));
		}

		protected abstract async Future<IOStream> perform_attach_to (uint pid, HashTable<string, Variant> options,
			Cancellable? cancellable, out Object? transport) throws Error, IOError;

		protected virtual string? get_emulated_agent_path (uint pid) throws Error {
			return null;
		}

		protected string make_agent_parameters (uint pid, string remote_address, HashTable<string, Variant> options) throws Error {
			var parameters = new StringBuilder (remote_address);

			if (pid == 0) {
				parameters.append ("|exit-monitor:off|thread-suspend-monitor:off");
				return parameters.str;
			}

			Variant? exceptor = options["exceptor"];
			if (exceptor != null) {
				if (!exceptor.is_of_type (VariantType.STRING))
					throw new Error.INVALID_ARGUMENT ("The 'exceptor' option is invalid");
				unowned string mode = exceptor.get_string ();
				if (mode != "off" && mode != "handler-only")
					throw new Error.INVALID_ARGUMENT ("The 'exceptor' option is invalid");
				parameters
					.append ("|exceptor:")
					.append (mode);
			}

			string[] toggles = { "unwind-broker", "exit-monitor", "thread-suspend-monitor" };
			foreach (string feature in toggles) {
				Variant? val = options[feature];
				if (val != null) {
					if (!val.is_of_type (VariantType.STRING) || val.get_string () != "off")
						throw new Error.INVALID_ARGUMENT ("The '%s' option is invalid", feature);
					parameters
						.append_c ('|')
						.append (feature)
						.append (":off");
				}
			}

			Variant? offsets = options["linker-notifier-offsets"];
			if (offsets != null) {
				if (!offsets.is_of_type (new VariantType ("au")))
					throw new Error.INVALID_ARGUMENT ("The 'linker-notifier-offsets' option is invalid");
				uint n = (uint) offsets.n_children ();
				if (n != 0) {
					parameters.append ("|linker-notifier-offsets:");
					for (uint i = 0; i != n; i++) {
						if (i != 0)
							parameters.append_c (',');
						parameters.append (offsets.get_child_value (i).get_uint32 ().to_string ());
					}
				}
			}

			return parameters.str;
		}

		public async AgentSession link_agent_session (AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			AgentSessionEntry? entry = agent_sessions[id];
			if (entry == null)
				throw new Error.INVALID_ARGUMENT ("Invalid session ID");

			DBusConnection connection = entry.connection;

			AgentSession session;
			try {
				session = yield connection.get_proxy (null, ObjectPath.for_agent_session (id), DO_NOT_LOAD_PROPERTIES,
					cancellable);
			} catch (IOError e) {
				throw_dbus_error (e);
			}

			assert (entry.sink_registration_id == 0);
			try {
				entry.sink_registration_id = connection.register_object (ObjectPath.for_agent_message_sink (id), sink);
			} catch (IOError e) {
				assert_not_reached ();
			}

			return session;
		}

		public void unlink_agent_session (AgentSessionId id) {
			AgentSessionEntry? entry = agent_sessions[id];
			if (entry == null || entry.sink_registration_id == 0)
				return;

			entry.connection.unregister_object (entry.sink_registration_id);
			entry.sink_registration_id = 0;
		}

		public bool can_pass_file_descriptors_to_agent_session (AgentSessionId id) throws Error {
			AgentSessionEntry? entry = agent_sessions[id];
			if (entry == null)
				throw new Error.INVALID_ARGUMENT ("Invalid session ID");

			return (entry.connection.get_capabilities () & DBusCapabilityFlags.UNIX_FD_PASSING) != 0;
		}

		public AgentSessionProvider obtain_session_provider (AgentSessionId id) throws Error {
			foreach (var future in agent_entries.values) {
				if (!future.ready)
					continue;

				var entry = future.value;
				if (entry.sessions.contains (id))
					return entry.provider;
			}

			throw new Error.INVALID_ARGUMENT ("Invalid session ID");
		}

		private void on_agent_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			bool closed_by_us = !remote_peer_vanished && error == null;
			if (closed_by_us)
				return;

			AgentEntry entry_to_remove = null;
			foreach (var future in agent_entries.values) {
				if (!future.ready)
					continue;

				var entry = future.value;
				if (entry.connection == connection) {
					entry_to_remove = entry;
					break;
				}
			}
			assert (entry_to_remove != null);

			destroy.begin (entry_to_remove, entry_to_remove.disconnect_reason, io_cancellable);
		}

		private void on_agent_session_provider_closed (AgentSessionId id) {
			var closed_after_opening = agent_sessions.unset (id);
			if (!closed_after_opening)
				return;
			var reason = SessionDetachReason.APPLICATION_REQUESTED;
			agent_session_detached (id, reason, CrashInfo.empty ());

			foreach (var future in agent_entries.values) {
				if (!future.ready)
					continue;

				var entry = future.value;

				var sessions = entry.sessions;
				if (sessions.remove (id)) {
					if (sessions.is_empty) {
						bool is_system_session = entry.pid == 0;
						if (!is_system_session && !entry.eternalized)
							unload_and_destroy.begin (entry, reason);
					}

					break;
				}
			}
		}

		private void on_agent_session_provider_eternalized (AgentSessionProvider provider) {
			foreach (var future in agent_entries.values) {
				if (!future.ready)
					continue;

				var entry = future.value;
				if (entry.provider == provider) {
					entry.eternalized = true;
					break;
				}
			}
		}

		private void on_child_gating_changed (AgentEntry entry, uint subscriber_count) {
			var pid = entry.pid;

			if (subscriber_count == 0) {
				foreach (var child in pending_children.values.to_array ()) {
					if (child.parent_pid == pid)
						resume.begin (child.pid, null);
				}
			}

			notify_child_gating_changed (pid, subscriber_count);
		}

		private async void unload_and_destroy (AgentEntry entry, SessionDetachReason reason) throws IOError {
			if (!prepare_teardown (entry))
				return;

			try {
				yield entry.provider.unload (io_cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED) {
					entry.detach ();
					throw (IOError) e;
				}
			}

			try {
				yield teardown (entry, reason, io_cancellable);
			} catch (IOError e) {
				entry.detach ();
				throw e;
			}
		}

		private async void destroy (AgentEntry entry, SessionDetachReason reason, Cancellable? cancellable) throws IOError {
			if (!prepare_teardown (entry))
				return;

			try {
				yield teardown (entry, reason, cancellable);
			} catch (IOError e) {
				entry.detach ();
				throw e;
			}
		}

		private bool prepare_teardown (AgentEntry entry) {
			if (!agent_entries.unset (entry.pid))
				return false;

			entry.child_gating_changed.disconnect (on_child_gating_changed);
			entry.provider.closed.disconnect (on_agent_session_provider_closed);
			entry.provider.eternalized.disconnect (on_agent_session_provider_eternalized);
			entry.connection.on_closed.disconnect (on_agent_connection_closed);

			return true;
		}

		private async void teardown (AgentEntry entry, SessionDetachReason reason, Cancellable? cancellable) throws IOError {
			CrashInfo? crash = null;
			if (reason == PROCESS_TERMINATED)
				crash = yield try_collect_crash (entry.pid, cancellable);

			var crash_info = (crash != null) ? crash : CrashInfo.empty ();
			foreach (var id in entry.sessions) {
				if (agent_sessions.unset (id))
					agent_session_detached (id, reason, crash_info);
			}

			yield entry.close (cancellable);
		}

		protected virtual async CrashInfo? try_collect_crash (uint pid, Cancellable? cancellable) throws IOError {
			return null;
		}

		public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var raw_id = yield injector.inject_library_file (pid, path, entrypoint, data, cancellable);
			return InjectorPayloadId (raw_id);
		}

		public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var blob_bytes = new Bytes (blob);
			var raw_id = yield injector.inject_library_blob (pid, blob_bytes, entrypoint, data, cancellable);
			return InjectorPayloadId (raw_id);
		}

		public async ChannelId open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
		}

		public virtual async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Services are not supported by this backend");
		}

		public ServiceSession link_service_session (ServiceSessionId id) throws Error {
			return service_session_registry.link (id);
		}

		public void unlink_service_session (ServiceSessionId id) {
			service_session_registry.unlink (id);
		}

#if !WINDOWS
		public async HostChildId prepare_to_fork (uint parent_pid, Cancellable? cancellable, out uint parent_injectee_id,
				out uint child_injectee_id, out GLib.Socket child_socket) throws Error, IOError {
			if (!injectee_by_pid.has_key (parent_pid))
				throw new Error.INVALID_ARGUMENT ("No injectee found for PID %u", parent_pid);
			parent_injectee_id = injectee_by_pid[parent_pid];
			child_injectee_id = yield injector.demonitor_and_clone_state (parent_injectee_id, cancellable);

			var fds = new int[2];
			Posix.socketpair (Posix.AF_UNIX, Posix.SOCK_STREAM, 0, fds);

			UnixSocket.tune_buffer_sizes (fds[0]);
			UnixSocket.tune_buffer_sizes (fds[1]);

			Socket local_socket, remote_socket;
			IOStream local_stream;
			try {
				local_socket = new Socket.from_fd (fds[0]);
				remote_socket = new Socket.from_fd (fds[1]);

				local_stream = SocketConnection.factory_create_connection (local_socket);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			var id = HostChildId (next_host_child_id++);

			start_child_connection.begin (id, local_stream);

			child_socket = remote_socket;

			return id;
		}
#endif

		public async HostChildId prepare_to_specialize (uint pid, string identifier, Cancellable? cancellable,
				out uint specialized_injectee_id, out string specialized_pipe_address) throws Error, IOError {
			Future<AgentEntry>? request = agent_entries[pid];
			if (request == null || !request.ready || !injectee_by_pid.has_key (pid))
				throw new Error.INVALID_ARGUMENT ("No injectee found for PID %u", pid);

			specialized_injectee_id = injectee_by_pid[pid];

			yield injector.demonitor (specialized_injectee_id, cancellable);

			var transport = new PipeTransport ();
			var stream_request = Pipe.open (transport.local_address, cancellable);

			var id = HostChildId (next_host_child_id++);

			start_specialized_connection.begin (id, transport, stream_request);

			specialized_pipe_address = transport.remote_address;

			return id;
		}

		private async void start_specialized_connection (HostChildId id, PipeTransport transport,
				Future<IOStream> stream_request) throws GLib.Error {
			IOStream stream = yield stream_request.wait_async (io_cancellable);

			yield start_child_connection (id, stream);
		}

		private async void start_child_connection (HostChildId id, IOStream stream) throws GLib.Error {
			var connection = yield new DBusConnection (stream, ServerGuid.HOST_SESSION_SERVICE,
				AUTHENTICATION_SERVER | AUTHENTICATION_ALLOW_ANONYMOUS | DELAY_MESSAGE_PROCESSING,
				null, io_cancellable);

			uint controller_registration_id = connection.register_object (ObjectPath.AGENT_CONTROLLER, (AgentController) this);

			connection.start_message_processing ();

			var entry = new ChildEntry (connection, controller_registration_id);
			child_entries[id] = entry;
			connection.on_closed.connect (on_child_connection_closed);
		}

		private void on_child_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			ChildEntry entry_to_remove = null;
			HostChildId? child_id = null;
			foreach (var e in child_entries.entries) {
				var entry = e.value;
				if (entry.connection == connection) {
					entry_to_remove = entry;
					child_id = e.key;
					break;
				}
			}
			assert (entry_to_remove != null);

			connection.on_closed.disconnect (on_child_connection_closed);
			child_entries.unset (child_id);

			entry_to_remove.close.begin (io_cancellable);
		}

		public async void recreate_agent_thread (uint pid, uint injectee_id, Cancellable? cancellable) throws Error, IOError {
			injectee_by_pid[pid] = injectee_id;

			yield injector.recreate_thread (pid, injectee_id, cancellable);
		}

		public async void wait_for_permission_to_resume (HostChildId id, HostChildInfo info, Cancellable? cancellable)
				throws Error, IOError {
			var child_entry = child_entries[id];
			if (child_entry == null)
				throw new Error.INVALID_ARGUMENT ("Invalid ID");

			uint pid = info.pid;
			var connection = child_entry.connection;

			var promise = new Promise<AgentEntry> ();
			agent_entries[pid] = promise.future;

			AgentSessionProvider provider;
			try {
				provider = yield connection.get_proxy (null, ObjectPath.AGENT_SESSION_PROVIDER, DO_NOT_LOAD_PROPERTIES,
					cancellable);
			} catch (GLib.Error e) {
				agent_entries.unset (pid);
				promise.reject (new Error.TRANSPORT (e.message));

				child_entry.close_soon ();

				return;
			}

			connection.on_closed.disconnect (on_child_connection_closed);
			child_entries.unset (id);

			var resume_request = new Promise<bool> ();

			var agent_entry = new AgentEntry (pid, null, connection, provider, child_entry.controller_registration_id);
			agent_entry.resume_request = resume_request;
			promise.resolve (agent_entry);

			connection.on_closed.connect (on_agent_connection_closed);
			provider.closed.connect (on_agent_session_provider_closed);
			provider.eternalized.connect (on_agent_session_provider_eternalized);
			agent_entry.child_gating_changed.connect (on_child_gating_changed);

			if (!try_handle_child (info))
				add_pending_child (info);

			yield resume_request.future.wait_async (cancellable);
		}

		public async void prepare_to_exec (HostChildInfo info, Cancellable? cancellable) throws Error, IOError {
			var pid = info.pid;

			AgentEntry? entry_to_wait_for = null;
			var entry_future = agent_entries[pid];
			if (entry_future != null) {
				try {
					var entry = yield entry_future.wait_async (cancellable);
					entry.disconnect_reason = PROCESS_REPLACED;
					entry_to_wait_for = entry;
				} catch (GLib.Error e) {
				}
			}

			yield prepare_exec_transition (pid, cancellable);

			wait_for_exec_and_deliver.begin (info, entry_to_wait_for, cancellable);
		}

		private async void wait_for_exec_and_deliver (HostChildInfo info, AgentEntry? entry_to_wait_for, Cancellable? cancellable)
				throws IOError {
			var pid = info.pid;

			try {
				yield await_exec_transition (pid, cancellable);
			} catch (GLib.Error e) {
				return;
			}

			if (entry_to_wait_for != null)
				yield entry_to_wait_for.wait_until_closed (cancellable);

			add_pending_child (info);
		}

		public async void cancel_exec (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield cancel_exec_transition (pid, cancellable);

			var entry_future = agent_entries[pid];
			if (entry_future != null) {
				try {
					var entry = yield entry_future.wait_async (cancellable);
					entry.disconnect_reason = PROCESS_TERMINATED;
				} catch (GLib.Error e) {
				}
			}
		}

		public async void acknowledge_spawn (HostChildInfo info, SpawnStartState start_state, Cancellable? cancellable)
				throws Error, IOError {
			var pid = info.pid;

			var request = new SpawnAckRequest (start_state);

			pending_acks[pid] = request;

			add_pending_child (info);

			yield request.await (cancellable);
		}

		private void add_pending_child (HostChildInfo info) {
			pending_children[info.pid] = info;
			child_added (info);

			garbage_collect_pending_children_soon ();
		}

		private void garbage_collect_pending_children_soon () {
			if (pending_children_gc_timer != null || pending_children_gc_request != null)
				return;

			var timer = new TimeoutSource.seconds (1);
			timer.set_callback (() => {
				pending_children_gc_timer = null;
				garbage_collect_pending_children.begin (io_cancellable);
				return false;
			});
			timer.attach (MainContext.get_thread_default ());
			pending_children_gc_timer = timer;
		}

		private async void garbage_collect_pending_children (Cancellable? cancellable) throws IOError {
			while (pending_children_gc_request != null) {
				try {
					yield pending_children_gc_request.future.wait_async (cancellable);
					return;
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					cancellable.set_error_if_cancelled ();
				}
			}
			pending_children_gc_request = new Promise<bool> ();

			foreach (var pid in pending_children.keys.to_array ()) {
				if (!process_is_alive (pid)) {
					try {
						yield resume (pid, cancellable);
					} catch (GLib.Error e) {
					}
				}
			}

			pending_children_gc_request.resolve (true);
			pending_children_gc_request = null;

			if (!pending_children.is_empty)
				garbage_collect_pending_children_soon ();
		}

		private class AgentEntry : Object {
			public signal void child_gating_changed (uint subscriber_count);

			public uint pid {
				get;
				construct;
			}

			public Object? transport {
				get;
				construct;
			}

			public DBusConnection? connection {
				get;
				construct;
			}

			public AgentSessionProvider provider {
				get;
				construct;
			}

			public uint controller_registration_id {
				get;
				construct;
			}

			public Gee.Set<AgentSessionId?> sessions {
				get;
				default = new Gee.HashSet<AgentSessionId?> (AgentSessionId.hash, AgentSessionId.equal);
			}

			public SessionDetachReason disconnect_reason {
				get;
				set;
				default = PROCESS_TERMINATED;
			}

			public Promise<bool>? resume_request {
				get;
				set;
			}

			public bool eternalized {
				get;
				set;
				default = false;
			}

			private bool closing = false;
			private bool registered = true;
			private Promise<bool> close_request = new Promise<bool> ();

			public AgentEntry (uint pid, Object? transport, DBusConnection? connection, AgentSessionProvider provider,
					uint controller_registration_id = 0) {
				Object (
					pid: pid,
					transport: transport,
					connection: connection,
					provider: provider,
					controller_registration_id: controller_registration_id
				);
			}

			construct {
				provider.child_gating_changed.connect (on_child_gating_changed);
			}

			public void detach () {
				if (!registered)
					return;

				var id = controller_registration_id;
				if (id != 0)
					connection.unregister_object (id);

				registered = false;
			}

			public async void close (Cancellable? cancellable) throws IOError {
				if (closing) {
					yield wait_until_closed (cancellable);
					return;
				}
				closing = true;

				provider.child_gating_changed.disconnect (on_child_gating_changed);

				if (connection != null) {
					try {
						yield connection.close (cancellable);
					} catch (GLib.Error e) {
					}
				}

				detach ();

				close_request.resolve (true);
			}

			public async void wait_until_closed (Cancellable? cancellable) throws IOError {
				try {
					yield close_request.future.wait_async (cancellable);
				} catch (Error e) {
					assert_not_reached ();
				}
			}

			private void on_child_gating_changed (uint subscriber_count) {
				child_gating_changed (subscriber_count);
			}
		}

		private class AgentSessionEntry {
			public DBusConnection connection {
				get;
				private set;
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

		private class ChildEntry : Object {
			public DBusConnection connection {
				get;
				construct;
			}

			public uint controller_registration_id {
				get;
				construct;
			}

			private Promise<bool> close_request;

			public ChildEntry (DBusConnection connection, uint controller_registration_id = 0) {
				Object (
					connection: connection,
					controller_registration_id: controller_registration_id
				);
			}

			public async void close (Cancellable? cancellable) throws IOError {
				while (close_request != null) {
					try {
						yield close_request.future.wait_async (cancellable);
						return;
					} catch (GLib.Error e) {
						assert (e is IOError.CANCELLED);
						cancellable.set_error_if_cancelled ();
					}
				}
				close_request = new Promise<bool> ();

				try {
					yield connection.close (cancellable);
				} catch (GLib.Error e) {
				}

				var id = controller_registration_id;
				if (id != 0) {
					connection.unregister_object (id);
				}

				close_request.resolve (true);
			}

			public void close_soon () {
				var source = new IdleSource ();
				source.set_priority (Priority.LOW);
				source.set_callback (() => {
					close.begin (null);
					return false;
				});
				source.attach (MainContext.get_thread_default ());
			}
		}

		private class SpawnAckRequest : Object {
			public SpawnStartState start_state {
				get;
				construct;
			}

			private Promise<bool> promise = new Promise<bool> ();

			public SpawnAckRequest (SpawnStartState start_state) {
				Object (start_state: start_state);
			}

			public async void await (Cancellable? cancellable) throws IOError {
				try {
					yield promise.future.wait_async (cancellable);
				} catch (Error e) {
					assert_not_reached ();
				}
			}

			public void complete () {
				promise.resolve (true);
			}
		}
	}

	internal interface HelperFile : Object {
		public abstract string path {
			owned get;
		}
	}

	internal sealed class InstalledHelperFile : Object, HelperFile {
		public string path {
			owned get {
				return installed_path;
			}
		}

		public string installed_path {
			get;
			construct;
		}

		public InstalledHelperFile.for_path (string path) {
			Object (installed_path: path);
		}
	}

	internal sealed class TemporaryHelperFile : Object, HelperFile {
		public string path {
			owned get {
				return file.path;
			}
		}

		public TemporaryFile file {
			get;
			construct;
		}

		public TemporaryHelperFile (TemporaryFile file) {
			Object (file: file);
		}
	}

	public abstract class InternalAgent : Object, AgentMessageSink, RpcPeer {
		public signal void unloaded ();

		public weak HostSessionConnection connection {
			get;
			construct;
		}

		public ScriptRuntime script_runtime {
			get;
			construct;
			default = DEFAULT;
		}

		private Promise<bool> ensure_request;
		private Promise<bool> _unloaded = new Promise<bool> ();

		protected HashTable<string, Variant> attach_options = make_parameters_dict ();
		protected uint target_pid;
		protected AgentSessionId session_id;
		protected AgentSession session;
		protected AgentScriptId script;
		private RpcClient rpc_client;

		construct {
			rpc_client = new RpcClient (this);

			connection.host_session.agent_session_detached.connect (on_agent_session_detached);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (ensure_request != null) {
				try {
					yield ensure_loaded (cancellable);
				} catch (Error e) {
				}
			}

			yield ensure_unloaded (cancellable);

			connection.host_session.agent_session_detached.disconnect (on_agent_session_detached);
		}

		protected abstract async uint get_target_pid (Cancellable? cancellable) throws Error, IOError;

		protected abstract async string? load_source (Cancellable? cancellable) throws Error, IOError;

		protected virtual async Bytes? load_snapshot (Cancellable? cancellable, out SnapshotTransport transport)
				throws Error, IOError {
			transport = INLINE;
			return null;
		}

		protected virtual void on_event (string type, Json.Array event) {
		}

		protected async Json.Node call (string method, Json.Node[] args, Bytes? data, Cancellable? cancellable)
				throws Error, IOError {
			yield ensure_loaded (cancellable);

			return yield rpc_client.call (method, args, data, cancellable);
		}

		protected async void post (Json.Node message, Cancellable? cancellable) throws Error, IOError {
			yield ensure_loaded (cancellable);

			string json = Json.to_string (message, false);

			try {
				yield session.post_messages ({ AgentMessage (SCRIPT, script, json, false, {}) }, 0, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		protected async void ensure_loaded (Cancellable? cancellable) throws Error, IOError {
			while (ensure_request != null) {
				try {
					yield ensure_request.future.wait_async (cancellable);
					return;
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			ensure_request = new Promise<bool> ();

			try {
				yield ensure_unloaded (cancellable);

				target_pid = yield get_target_pid (cancellable);

				try {
					session_id = yield connection.host_session.attach (target_pid, attach_options, cancellable);

					session = yield connection.link_agent_session (session_id, (AgentMessageSink) this, cancellable);

					string? source = yield load_source (cancellable);
					if (source != null) {
						var options = new ScriptOptions ();
						options.name = "internal-agent";
						SnapshotTransport transport;
						Bytes? snapshot = yield load_snapshot (cancellable, out transport);
						if (snapshot != null) {
							options.snapshot = snapshot;
							options.snapshot_transport = transport;
						}
						options.runtime = script_runtime;

						script = yield session.create_script (source, options._serialize (), cancellable);

						yield load_script (cancellable);
					}
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				ensure_request.resolve (true);
			} catch (GLib.Error e) {
				ensure_request.reject (e);
			}

			var pending_error = ensure_request.future.error;
			if (pending_error != null) {
				try {
					yield ensure_unloaded (cancellable);
				} finally {
					ensure_request = null;
				}

				throw_api_error (pending_error);
			}
		}

		private async void ensure_unloaded (Cancellable? cancellable) throws IOError {
			if (session == null && script.handle == 0)
				return;

			yield perform_unload (cancellable);
		}

		protected virtual async void perform_unload (Cancellable? cancellable) throws IOError {
			if (script.handle != 0)
				yield destroy_script (cancellable);

			if (session != null) {
				try {
					yield session.close (cancellable);
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED)
						return;
				}
				session = null;
			}
		}

		protected virtual async void load_script (Cancellable? cancellable) throws Error, IOError {
			try {
				yield session.load_script (script, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		protected virtual async void destroy_script (Cancellable? cancellable) throws IOError {
			try {
				yield session.destroy_script (script, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					return;
			}
			script = AgentScriptId (0);
		}

		protected async void wait_for_unload (Cancellable? cancellable) throws IOError {
			try {
				yield _unloaded.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			if (id.handle != session_id.handle)
				return;

			_unloaded.resolve (true);
			unloaded ();
		}

		protected async void post_messages (AgentMessage[] messages, uint batch_id,
				Cancellable? cancellable) throws Error, IOError {
			foreach (var m in messages) {
				if (m.kind == SCRIPT && m.script_id == script)
					on_message_from_script (m.text);
			}
		}

		private void on_message_from_script (string json) {
			bool handled = rpc_client.try_handle_message (json);
			if (handled)
				return;

			var parser = new Json.Parser ();
			try {
				parser.load_from_data (json);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
			var message = parser.get_root ().get_object ();

			var type = message.get_string_member ("type");
			if (type == "send") {
				var event = message.get_array_member ("payload");
				var event_type = event.get_string_element (0);
				on_event (event_type, event);

				handled = true;
			} else if (type == "log") {
				var text = message.get_string_member ("payload");
				printerr ("%s\n", text);

				handled = true;
			}

			if (!handled)
				printerr ("%s\n", json);
		}

		private async void post_rpc_message (string json, Bytes? data, Cancellable? cancellable) throws Error, IOError {
			var has_data = data != null;
			var data_param = has_data ? data.get_data () : new uint8[0];
			try {
				yield session.post_messages ({ AgentMessage (SCRIPT, script, json, has_data, data_param) }, 0, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}
	}

	internal async void wait_for_uninject (Injector injector, Cancellable? cancellable, UninjectPredicate is_injected) throws IOError {
		if (!is_injected ())
			return;

		var uninjected_handler = injector.uninjected.connect ((id) => {
			wait_for_uninject.callback ();
		});

		var cancel_source = new CancellableSource (cancellable);
		cancel_source.set_callback (wait_for_uninject.callback);
		cancel_source.attach (MainContext.get_thread_default ());

		while (is_injected () && !cancellable.is_cancelled ())
			yield;

		cancel_source.destroy ();

		injector.disconnect (uninjected_handler);
	}

	internal delegate bool UninjectPredicate ();

#if HAVE_FRUITY_BACKEND || HAVE_DROIDY_BACKEND
	internal async DBusConnection establish_direct_connection (TransportBroker broker, AgentSessionId id,
			HostChannelProvider channel_provider, Cancellable? cancellable) throws Error, IOError {
		uint16 port;
		string token;
		try {
			yield broker.open_tcp_transport (id, cancellable, out port, out token);
		} catch (GLib.Error e) {
			if (e is Error)
				throw (Error) e;
			throw new Error.TRANSPORT ("%s", e.message);
		}

		var stream = yield channel_provider.open_channel (("tcp:%" + uint16.FORMAT_MODIFIER + "u").printf (port), cancellable);

		try {
			size_t bytes_written;
			yield stream.output_stream.write_all_async (token.data, Priority.DEFAULT, cancellable, out bytes_written);

			return yield new DBusConnection (stream, null, DBusConnectionFlags.NONE, null, cancellable);
		} catch (GLib.Error e) {
			throw new Error.TRANSPORT ("%s", e.message);
		}
	}
#endif
}
