namespace Frida {
	public sealed class DroidyHostSessionBackend : Object, HostSessionBackend {
		private Droidy.DeviceTracker tracker;

		private Gee.HashMap<string, DroidyHostSessionProvider> providers = new Gee.HashMap<string, DroidyHostSessionProvider> ();

		private Promise<bool> start_request;
		private Cancellable start_cancellable;
		private SourceFunc on_start_completed;

		private Cancellable io_cancellable = new Cancellable ();

		public async void start (Cancellable? cancellable) throws IOError {
			start_request = new Promise<bool> ();
			start_cancellable = new Cancellable ();
			on_start_completed = start.callback;

			var main_context = MainContext.get_thread_default ();

			var timeout_source = new TimeoutSource (500);
			timeout_source.set_callback (start.callback);
			timeout_source.attach (main_context);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (start.callback);
			cancel_source.attach (main_context);

			do_start.begin ();

			yield;

			cancel_source.destroy ();
			timeout_source.destroy ();
			on_start_completed = null;
		}

		private async void do_start () {
			bool success = true;

			tracker = new Droidy.DeviceTracker ();
			tracker.device_attached.connect (details => {
				var provider = new DroidyHostSessionProvider (details);
				providers[details.serial] = provider;
				provider_available (provider);
			});
			tracker.device_detached.connect (serial => {
				DroidyHostSessionProvider provider;
				providers.unset (serial, out provider);
				provider_unavailable (provider);
				provider.close.begin (io_cancellable);
			});

			try {
				yield tracker.open (start_cancellable);
			} catch (GLib.Error e) {
				success = false;
			}

			start_request.resolve (success);

			if (on_start_completed != null)
				on_start_completed ();
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			start_cancellable.cancel ();

			try {
				yield start_request.future.wait_async (cancellable);
			} catch (Error e) {
				assert_not_reached ();
			}

			if (tracker != null) {
				yield tracker.close (cancellable);
				tracker = null;
			}

			io_cancellable.cancel ();

			foreach (var provider in providers.values) {
				provider_unavailable (provider);
				yield provider.close (cancellable);
			}
			providers.clear ();
		}
	}

	public sealed class DroidyHostSessionProvider : Object, HostSessionProvider, HostChannelProvider {
		public string id {
			get { return device_details.serial; }
		}

		public string name {
			get { return device_details.name; }
		}

		public Variant? icon {
			get { return _icon; }
		}
		private Variant _icon;

		public HostSessionProviderKind kind {
			get { return HostSessionProviderKind.USB; }
		}

		public Droidy.DeviceDetails device_details {
			get;
			construct;
		}

		private DroidyHostSession? host_session;

		private const double MAX_CLIENT_AGE = 30.0;

		public DroidyHostSessionProvider (Droidy.DeviceDetails details) {
			Object (device_details: details);
		}

		construct {
			_icon = make_provider_icon (Frida.Data.Icons.get_droidy_png_blob ().data);
		}

		public async void close (Cancellable? cancellable) throws IOError {
		}

		public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != null)
				throw new Error.INVALID_ARGUMENT ("Already created");

			string control_endpoint = ("tcp:%" + uint16.FORMAT_MODIFIER + "u").printf (DEFAULT_CONTROL_PORT);

			if (options != null) {
				Value? control_endpoint_val = options.map["control-endpoint"];
				if (control_endpoint_val != null) {
					if (!control_endpoint_val.holds (typeof (string)))
						throw new Error.INVALID_ARGUMENT ("The control-endpoint option must be a string");
					control_endpoint = control_endpoint_val.get_string ();
				}
			}

			host_session = new DroidyHostSession (device_details, this, control_endpoint);
			host_session.agent_session_detached.connect (on_agent_session_detached);

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
			if (host_session != this.host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			return this.host_session.link_channel (id);
		}

		public void unlink_channel (HostSession host_session, ChannelId id) {
			if (host_session != this.host_session)
				return;

			this.host_session.unlink_channel (id);
		}

		public async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id, Cancellable? cancellable)
				throws Error, IOError {
			if (host_session != this.host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			return yield this.host_session.link_service_session (id, cancellable);
		}

		public void unlink_service_session (HostSession host_session, ServiceSessionId id) {
			if (host_session != this.host_session)
				return;

			this.host_session.unlink_service_session (id);
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			agent_session_detached (id, reason, crash);
		}

		public async IOStream open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			if (address.contains (":")) {
				Droidy.Client client = null;
				try {
					client = yield Droidy.Client.open (cancellable);
					yield client.request ("host:transport:" + device_details.serial, cancellable);
					yield client.request_protocol_change (address, cancellable);
					return client.stream;
				} catch (GLib.Error e) {
					if (client != null)
						client.close.begin ();

					throw new Error.TRANSPORT ("%s", e.message);
				}
			}

			throw new Error.NOT_SUPPORTED ("Unsupported channel address");
		}
	}

	public sealed class DroidyHostSession : Object, HostSession {
		public Droidy.DeviceDetails device_details {
			get;
			construct;
		}

		public weak HostChannelProvider channel_provider {
			get;
			construct;
		}

		public string control_endpoint {
			get;
			construct;
		}

		private Promise<AndroidHelperClient>? helper_client_request;
		private Droidy.ShellSession? helper_shell;

		private Gee.HashMap<uint, Droidy.Injector.GadgetDetails> gadgets =
			new Gee.HashMap<uint, Droidy.Injector.GadgetDetails> ();
		private Gee.HashMap<AgentSessionId?, GadgetEntry> gadget_entries =
			new Gee.HashMap<AgentSessionId?, GadgetEntry> (AgentSessionId.hash, AgentSessionId.equal);

		private Promise<RemoteServer>? remote_server_request;
		private RemoteServer? current_remote_server;
		private Timer? last_server_check_timer;
		private Error? last_server_check_error;
		private Gee.HashMap<AgentSessionId?, AgentSessionId?> remote_agent_sessions =
			new Gee.HashMap<AgentSessionId?, AgentSessionId?> (AgentSessionId.hash, AgentSessionId.equal);

		private Gee.HashMap<AgentSessionId?, AgentSessionEntry> agent_sessions =
			new Gee.HashMap<AgentSessionId?, AgentSessionEntry> (AgentSessionId.hash, AgentSessionId.equal);

		private ChannelRegistry channel_registry = new ChannelRegistry ();

		private ServiceSessionRegistry service_session_registry = new ServiceSessionRegistry ();

		private Cancellable io_cancellable = new Cancellable ();

		private const double MIN_SERVER_CHECK_INTERVAL = 5.0;
		private const string GADGET_APP_ID = "re.frida.Gadget";

		public DroidyHostSession (Droidy.DeviceDetails device_details, HostChannelProvider channel_provider,
				string control_endpoint) {
			Object (
				device_details: device_details,
				channel_provider: channel_provider,
				control_endpoint: control_endpoint
			);
		}

		construct {
			channel_registry.channel_closed.connect (on_channel_closed);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (remote_server_request != null) {
				var server = yield try_get_remote_server (cancellable);
				if (server != null) {
					try {
						yield server.connection.close (cancellable);
					} catch (GLib.Error e) {
					}
				}
			}

			while (!gadget_entries.is_empty) {
				var iterator = gadget_entries.values.iterator ();
				iterator.next ();
				var entry = iterator.get ();
				yield entry.close (cancellable);
			}

			if (helper_client_request != null) {
				AndroidHelperClient? helper = yield try_get_helper_client (cancellable);
				if (helper != null) {
					var transport = (AndroidHelperStreamTransport) helper.transport;
					on_helper_stream_transport_closed (transport);
					yield transport.close (cancellable);
				}
			}

			if (helper_shell != null) {
				yield helper_shell.close (cancellable);
				helper_shell = null;
			}

			io_cancellable.cancel ();
		}

		public async void ping (uint interval_seconds, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable) throws Error, IOError {
			var server = yield try_get_remote_server (cancellable);
			if (server != null && server.flavor == REGULAR) {
				try {
					return yield server.session.query_system_parameters (cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var parameters = new HashTable<string, Variant> (str_hash, str_equal);

			var os = new HashTable<string, Variant> (str_hash, str_equal);
			os["id"] = "android";
			os["name"] = "Android";

			string properties = yield Droidy.ShellCommand.run ("getprop", device_details.serial, cancellable);
			var property_pattern = /\[(.+?)\]: \[(.*?)\]/s;
			try {
				MatchInfo info;
				for (property_pattern.match (properties, 0, out info); info.matches (); info.next ()) {
					string key = info.fetch (1);
					string val = info.fetch (2);
					switch (key) {
						case "ro.build.version.release":
							os["version"] = val;
							break;
						case "ro.build.version.sdk":
							parameters["api-level"] = int64.parse (val);
							break;
						case "ro.product.cpu.abi":
							parameters["arch"] = infer_arch_from_abi (val);
							break;
						default:
							break;
					}
				}
			} catch (RegexError e) {
			}

			parameters["os"] = os;

			parameters["platform"] = "linux";

			parameters["access"] = "jailed";

			return parameters;
		}

		private static string infer_arch_from_abi (string abi) throws Error {
			switch (abi) {
				case "x86":
					return "ia32";
				case "x86_64":
					return "x64";
				case "armeabi":
				case "armeabi-v7a":
					return "arm";
				case "arm64-v8a":
					return "arm64";
				case "mips":
				case "mips64":
					return "mips";
				default:
					throw new Error.NOT_SUPPORTED ("Unsupported ABI: “%s”; please file a bug", abi);
			}
		}

		public async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var server = yield try_get_remote_server (cancellable);
			if (server != null && server.flavor == REGULAR) {
				try {
					return yield server.session.get_frontmost_application (options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var helper = yield try_get_helper_client (cancellable);
			if (helper == null) {
				if (server == null)
					server = yield get_remote_server (cancellable);
				try {
					return yield server.session.get_frontmost_application (options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var opts = FrontmostQueryOptions._deserialize (options);

			return yield helper.get_frontmost_application (opts, cancellable);
		}

		public async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var server = yield try_get_remote_server (cancellable);
			if (server != null && server.flavor == REGULAR) {
				try {
					return yield server.session.enumerate_applications (options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var helper = yield try_get_helper_client (cancellable);
			if (helper == null) {
				if (server == null)
					server = yield get_remote_server (cancellable);
				try {
					return yield server.session.enumerate_applications (options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var opts = ApplicationQueryOptions._deserialize (options);

			var result = yield helper.enumerate_applications (opts, cancellable);

			if (server != null && server.flavor == GADGET) {
				bool gadget_is_selected = true;
				if (opts.has_selected_identifiers ()) {
					gadget_is_selected = false;
					opts.enumerate_selected_identifiers (identifier => {
						if (identifier == "re.frida.Gadget")
							gadget_is_selected = true;
					});
				}

				if (gadget_is_selected) {
					try {
						foreach (var app in yield server.session.enumerate_applications (options, cancellable))
							result += app;
					} catch (GLib.Error e) {
					}
				}
			}

			return result;
		}

		public async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var server = yield try_get_remote_server (cancellable);
			if (server != null && server.flavor == REGULAR) {
				try {
					return yield server.session.enumerate_processes (options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var helper = yield try_get_helper_client (cancellable);
			if (helper == null) {
				if (server == null)
					server = yield get_remote_server (cancellable);
				try {
					return yield server.session.enumerate_processes (options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			var opts = ProcessQueryOptions._deserialize (options);

			var result = yield helper.enumerate_processes (opts, cancellable);

			if (server != null && server.flavor == GADGET) {
				try {
					foreach (var process in yield server.session.enumerate_processes (options, cancellable)) {
						bool gadget_is_selected = true;
						if (opts.has_selected_pids ()) {
							gadget_is_selected = false;
							uint gadget_pid = process.pid;
							opts.enumerate_selected_pids (pid => {
								if (pid == gadget_pid)
									gadget_is_selected = true;
							});
						}

						if (gadget_is_selected)
							result += process;
					}
				} catch (GLib.Error e) {
				}
			}

			return result;
		}

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				yield server.session.enable_spawn_gating (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		// Forward the error untranslated so the caller can spot UNKNOWN_METHOD from an older remote.
		public async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
				Cancellable? cancellable) throws GLib.Error {
			var server = yield get_remote_server (cancellable);
			yield server.session.enable_spawn_gating_with_options (options, cancellable);
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				yield server.session.disable_spawn_gating (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				return yield server.session.enumerate_pending_spawn (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				return yield server.session.enumerate_pending_children (cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			var server = yield try_get_remote_server (cancellable);
			if (server != null && (server.flavor != GADGET || program == GADGET_APP_ID)) {
				try {
					return yield server.session.spawn (program, options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			if (program[0] == '/')
				throw new Error.NOT_SUPPORTED ("Only able to spawn apps");

			unowned string package = program;

			HashTable<string, Variant> aux = options.aux;

			string? user_gadget_path = null;
			Variant? user_gadget_value = aux["gadget"];
			if (user_gadget_value != null) {
				if (!user_gadget_value.is_of_type (VariantType.STRING)) {
					throw new Error.INVALID_ARGUMENT ("The 'gadget' option must be a string pointing at the " +
						"frida-gadget.so to use");
				}
				user_gadget_path = user_gadget_value.get_string ();
			}

			string gadget_path;
			if (user_gadget_path != null) {
				gadget_path = user_gadget_path;
			} else {
				gadget_path = Path.build_filename (Environment.get_user_cache_dir (), "frida", "gadget-android-arm64.so");
			}

			InputStream gadget;
			try {
				var gadget_file = File.new_for_path (gadget_path);
				gadget = yield gadget_file.read_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.NOT_FOUND && user_gadget_path == null) {
					throw new Error.NOT_SUPPORTED (
						"Need Gadget to attach on jailed Android; its default location is: %s", gadget_path);
				} else {
					throw new Error.NOT_SUPPORTED ("%s", e.message);
				}
			}

			var details = yield Droidy.Injector.inject (gadget, package, device_details.serial, cancellable);
			gadgets[details.pid] = details;

			return details.pid;
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				yield server.session.input (pid, data, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			var gadget = gadgets[pid];
			if (gadget != null) {
				yield gadget.jdwp.resume (cancellable);
				return;
			}

			var server = yield get_remote_server (cancellable);
			try {
				yield server.session.resume (pid, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				yield server.session.kill (pid, cancellable);
				return;
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var gadget = gadgets[pid];
			if (gadget != null)
				return yield attach_via_gadget (pid, options, gadget, cancellable);

			var server = yield get_remote_server (cancellable);
			try {
				return yield attach_via_remote (pid, options, server, cancellable);
			} catch (Error e) {
				throw_dbus_error (e);
			}
		}

		public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		private async AgentSessionId attach_via_gadget (uint pid, HashTable<string, Variant> options,
				Droidy.Injector.GadgetDetails gadget, Cancellable? cancellable) throws Error, IOError {
			try {
				var stream = yield channel_provider.open_channel ("localabstract:" + gadget.unix_socket_path, cancellable);

				WebServiceTransport transport = PLAIN;
				string? origin = null;

				stream = yield negotiate_connection (stream, transport, "lolcathost", origin, cancellable);

				var connection = yield new DBusConnection (stream, null, DBusConnectionFlags.NONE, null, cancellable);

				HostSession host_session = yield connection.get_proxy (null, ObjectPath.HOST_SESSION,
					DO_NOT_LOAD_PROPERTIES, cancellable);

				AgentSessionId remote_session_id;
				try {
					remote_session_id = yield host_session.attach (pid, options, cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}

				var local_session_id = AgentSessionId.generate ();
				var gadget_entry = new GadgetEntry (local_session_id, host_session, connection);
				gadget_entry.detached.connect (on_gadget_entry_detached);
				gadget_entries[local_session_id] = gadget_entry;
				agent_sessions[local_session_id] = new AgentSessionEntry (remote_session_id, connection);

				return local_session_id;
			} catch (GLib.Error e) {
				if (e is Error)
					throw (Error) e;
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
		}

		private async AgentSessionId attach_via_remote (uint pid, HashTable<string, Variant> options, RemoteServer server,
				Cancellable? cancellable) throws Error, IOError {
			AgentSessionId remote_session_id;
			try {
				remote_session_id = yield server.session.attach (pid, options, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
			var local_session_id = AgentSessionId.generate ();

			var entry = new AgentSessionEntry (remote_session_id, server.connection);

			remote_agent_sessions[remote_session_id] = local_session_id;
			agent_sessions[local_session_id] = entry;

			var transport_broker = server.transport_broker;
			if (transport_broker != null) {
				try {
					entry.connection = yield establish_direct_connection (transport_broker, remote_session_id,
						channel_provider, cancellable);
				} catch (Error e) {
					if (e is Error.NOT_SUPPORTED)
						server.transport_broker = null;
				}
			}

			return local_session_id;
		}

		public async AgentSession link_agent_session (AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			AgentSessionEntry entry = agent_sessions[id];
			if (entry == null)
				throw new Error.INVALID_ARGUMENT ("Invalid session ID");

			DBusConnection connection = entry.connection;
			AgentSessionId remote_id = entry.remote_session_id;

			AgentSession session = yield connection.get_proxy (null, ObjectPath.for_agent_session (remote_id),
				DO_NOT_LOAD_PROPERTIES, cancellable);

			entry.sink_registration_id = connection.register_object (ObjectPath.for_agent_message_sink (remote_id), sink);

			return session;
		}

		public void unlink_agent_session (AgentSessionId id) {
			AgentSessionEntry? entry = agent_sessions[id];
			if (entry == null || entry.sink_registration_id == 0)
				return;

			entry.connection.unregister_object (entry.sink_registration_id);
			entry.sink_registration_id = 0;
		}

		public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				return yield server.session.inject_library_file (pid, path, entrypoint, data, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				return yield server.session.inject_library_blob (pid, blob, entrypoint, data, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async ChannelId open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			var stream = yield channel_provider.open_channel (address, cancellable);

			var id = ChannelId.generate ();
			channel_registry.register (id, stream);

			return id;
		}

		public IOStream link_channel (ChannelId id) throws Error {
			return channel_registry.link (id);
		}

		public void unlink_channel (ChannelId id) {
			channel_registry.unlink (id);
		}

		private void on_channel_closed (ChannelId id) {
			channel_closed (id);
		}

		public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			try {
				return yield server.session.open_service (address, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async ServiceSession link_service_session (ServiceSessionId id, Cancellable? cancellable) throws Error, IOError {
			var server = yield get_remote_server (cancellable);
			ServiceSession session = yield server.connection.get_proxy (null, ObjectPath.for_service_session (id),
				DO_NOT_LOAD_PROPERTIES, cancellable);
			service_session_registry.register (id, session);
			return session;
		}

		public void unlink_service_session (ServiceSessionId id) {
			service_session_registry.unlink (id);
		}

		private void on_gadget_entry_detached (GadgetEntry entry, SessionDetachReason reason) {
			AgentSessionId id = entry.local_session_id;
			var no_crash = CrashInfo.empty ();

			gadget_entries.unset (id);
			agent_sessions.unset (id);

			entry.detached.disconnect (on_gadget_entry_detached);

			agent_session_detached (id, reason, no_crash);

			entry.close.begin (io_cancellable);
		}

		private async RemoteServer? try_get_remote_server (Cancellable? cancellable) throws IOError {
			try {
				return yield get_remote_server (cancellable);
			} catch (Error e) {
				return null;
			}
		}

		private async RemoteServer get_remote_server (Cancellable? cancellable) throws Error, IOError {
			if (current_remote_server != null)
				return current_remote_server;

			while (remote_server_request != null) {
				try {
					return yield remote_server_request.future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}

			if (last_server_check_timer != null && last_server_check_timer.elapsed () < MIN_SERVER_CHECK_INTERVAL)
				throw last_server_check_error;
			last_server_check_timer = new Timer ();

			remote_server_request = new Promise<RemoteServer> ();

			DBusConnection? connection = null;
			try {
				var stream = yield channel_provider.open_channel (control_endpoint, cancellable);

				WebServiceTransport transport = PLAIN;
				string? origin = null;

				stream = yield negotiate_connection (stream, transport, "lolcathost", origin, cancellable);

				connection = yield new DBusConnection (stream, null, DBusConnectionFlags.NONE, null, cancellable);

				HostSession session = yield connection.get_proxy (null, ObjectPath.HOST_SESSION, DO_NOT_LOAD_PROPERTIES,
					cancellable);

				RemoteServer.Flavor flavor = REGULAR;
				try {
					var app = yield session.get_frontmost_application (make_parameters_dict (), cancellable);
					if (app.identifier == GADGET_APP_ID)
						flavor = GADGET;
				} catch (GLib.Error e) {
				}

				TransportBroker? transport_broker = null;
				if (flavor == REGULAR) {
					transport_broker = yield connection.get_proxy (null, ObjectPath.TRANSPORT_BROKER,
						DO_NOT_LOAD_PROPERTIES, cancellable);
				}

				if (connection.closed)
					throw new Error.SERVER_NOT_RUNNING ("Unable to connect to remote frida-server");

				var server = new RemoteServer (session, connection, flavor, transport_broker);
				attach_remote_server (server);
				current_remote_server = server;
				last_server_check_timer = null;
				last_server_check_error = null;

				remote_server_request.resolve (server);

				return server;
			} catch (GLib.Error e) {
				GLib.Error api_error;

				if (e is IOError.CANCELLED) {
					api_error = new IOError.CANCELLED ("%s", e.message);

					last_server_check_timer = null;
					last_server_check_error = null;
				} else {
					if (e is Error.SERVER_NOT_RUNNING) {
						api_error = new Error.SERVER_NOT_RUNNING ("Unable to connect to remote frida-server");
					} else if (connection != null) {
						api_error = new Error.PROTOCOL ("Incompatible frida-server version");
					} else {
						api_error = new Error.SERVER_NOT_RUNNING ("Unable to connect to remote frida-server: %s",
							e.message);
					}

					last_server_check_error = (Error) api_error;
				}

				remote_server_request.reject (api_error);
				remote_server_request = null;

				throw_api_error (api_error);
			}
		}

		private void attach_remote_server (RemoteServer server) {
			server.connection.on_closed.connect (on_remote_connection_closed);

			var session = server.session;
			session.spawn_gating_disabled.connect (on_remote_spawn_gating_disabled);
			session.spawn_added.connect (on_remote_spawn_added);
			session.spawn_removed.connect (on_remote_spawn_removed);
			session.child_added.connect (on_remote_child_added);
			session.child_removed.connect (on_remote_child_removed);
			session.process_crashed.connect (on_remote_process_crashed);
			session.output.connect (on_remote_output);
			session.agent_session_detached.connect (on_remote_agent_session_detached);
			session.uninjected.connect (on_remote_uninjected);
		}

		private void detach_remote_server (RemoteServer server) {
			server.connection.on_closed.disconnect (on_remote_connection_closed);

			var session = server.session;
			session.spawn_gating_disabled.disconnect (on_remote_spawn_gating_disabled);
			session.spawn_added.disconnect (on_remote_spawn_added);
			session.spawn_removed.disconnect (on_remote_spawn_removed);
			session.child_added.disconnect (on_remote_child_added);
			session.child_removed.disconnect (on_remote_child_removed);
			session.process_crashed.disconnect (on_remote_process_crashed);
			session.output.disconnect (on_remote_output);
			session.agent_session_detached.disconnect (on_remote_agent_session_detached);
			session.uninjected.disconnect (on_remote_uninjected);
		}

		private void on_remote_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
			detach_remote_server (current_remote_server);
			current_remote_server = null;
			remote_server_request = null;

			var no_crash = CrashInfo.empty ();
			foreach (var remote_id in remote_agent_sessions.keys.to_array ())
				on_remote_agent_session_detached (remote_id, CONNECTION_TERMINATED, no_crash);
		}

		private void on_remote_spawn_gating_disabled (SpawnGatingDisabledReason reason) {
			spawn_gating_disabled (reason);
		}

		private void on_remote_spawn_added (HostSpawnInfo info) {
			spawn_added (info);
		}

		private void on_remote_spawn_removed (HostSpawnInfo info) {
			spawn_removed (info);
		}

		private void on_remote_child_added (HostChildInfo info) {
			child_added (info);
		}

		private void on_remote_child_removed (HostChildInfo info) {
			child_removed (info);
		}

		private void on_remote_process_crashed (CrashInfo crash) {
			process_crashed (crash);
		}

		private void on_remote_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, data);
		}

		private void on_remote_agent_session_detached (AgentSessionId remote_id, SessionDetachReason reason, CrashInfo crash) {
			AgentSessionId? local_id;
			if (!remote_agent_sessions.unset (remote_id, out local_id))
				return;

			bool agent_session_found = agent_sessions.unset (local_id);
			assert (agent_session_found);

			agent_session_detached (local_id, reason, crash);
		}

		private void on_remote_uninjected (InjectorPayloadId id) {
			uninjected (id);
		}

		private async AndroidHelperClient? try_get_helper_client (Cancellable? cancellable) throws IOError {
			try {
				return yield get_helper_client (cancellable);
			} catch (Error e) {
				return null;
			}
		}

		private async AndroidHelperClient get_helper_client (Cancellable? cancellable) throws Error, IOError {
			while (helper_client_request != null) {
				try {
					return yield helper_client_request.future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			helper_client_request = new Promise<AndroidHelperClient> ();

			Droidy.ShellSession? shell = null;
			try {
				string device_serial = device_details.serial;
				string instance_id = Uuid.string_random ().replace ("-", "");
				string helper_path = "/data/local/tmp/frida-helper-" + instance_id + ".dex";

				var helper_dex = new MemoryInputStream.from_bytes (
					new Bytes.static (Frida.Data.Android.get_helper_dex_blob ().data));

				var helper_meta = new Droidy.FileMetadata ();
				helper_meta.mode = 0100644;
				helper_meta.time_modified = new DateTime.now_utc ();

				yield Droidy.FileSync.send (helper_dex, helper_meta, helper_path, device_serial, cancellable);

				shell = new Droidy.ShellSession ();
				var output = new StringBuilder ();
				bool waiting = false;
				var output_handler = shell.output.connect ((pipe, bytes) => {
					if (pipe == STDOUT) {
						unowned string str = (string) bytes.get_data ();
						output.append (str);
						if (waiting)
							get_helper_client.callback ();
					}
				});
				try {
					yield shell.open (device_serial, cancellable);

					shell.send_command (("CLASSPATH=%s app_process " +
							"/data/local/tmp " +
							"--nice-name=re.frida.helper " +
							"re.frida.Helper " +
							"%s; " +
							"rm -f %s; " +
							"echo BYE.").printf (helper_path, instance_id, helper_path));

					while (!output.str.has_prefix ("READY.\n")) {
						waiting = true;
						yield;
						waiting = false;

						if (output.str.has_prefix ("BYE.\n"))
							throw new Error.NOT_SUPPORTED ("Unable to start helper");
					}
				} finally {
					shell.disconnect (output_handler);
				}

				var client = yield Droidy.Client.open (cancellable);
				try {
					yield client.request ("host:transport:" + device_serial, cancellable);
					yield client.request_protocol_change ("localabstract:/frida-helper-" + instance_id, cancellable);
				} catch (GLib.Error e) {
					client.close.begin ();
					throw e;
				}

				var transport = new AndroidHelperStreamTransport (client.stream);
				transport.closed.connect (on_helper_stream_transport_closed);

				var helper = new AndroidHelperClient (transport);

				helper_shell = shell;

				helper_client_request.resolve (helper);

				return helper;
			} catch (GLib.Error e) {
				if (shell != null)
					shell.close.begin (io_cancellable);

				var api_error = new Error.NOT_SUPPORTED ("%s", e.message);

				helper_client_request.reject (api_error);

				throw_api_error (api_error);
			}
		}

		private void on_helper_stream_transport_closed (AndroidHelperStreamTransport transport) {
			transport.closed.disconnect (on_helper_stream_transport_closed);
			helper_client_request = null;

			if (helper_shell != null) {
				helper_shell.close.begin (io_cancellable);
				helper_shell = null;
			}
		}

		private class GadgetEntry : Object {
			public signal void detached (SessionDetachReason reason);

			public AgentSessionId local_session_id {
				get;
				construct;
			}

			public HostSession host_session {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			private Promise<bool>? close_request;

			public GadgetEntry (AgentSessionId local_session_id, HostSession host_session, DBusConnection connection) {
				Object (
					local_session_id: local_session_id,
					host_session: host_session,
					connection: connection
				);
			}

			construct {
				connection.on_closed.connect (on_connection_closed);
				host_session.agent_session_detached.connect (on_session_detached);
			}

			~GadgetEntry () {
				connection.on_closed.disconnect (on_connection_closed);
				host_session.agent_session_detached.disconnect (on_session_detached);
			}

			public async void close (Cancellable? cancellable) throws IOError {
				while (close_request != null) {
					try {
						yield close_request.future.wait_async (cancellable);
						return;
					} catch (Error e) {
						assert_not_reached ();
					} catch (IOError e) {
						cancellable.set_error_if_cancelled ();
					}
				}
				close_request = new Promise<bool> ();

				try {
					yield connection.close (cancellable);
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED) {
						close_request.reject (e);
						close_request = null;

						throw (IOError) e;
					}
				}

				close_request.resolve (true);
			}

			private void on_connection_closed (DBusConnection connection, bool remote_peer_vanished, GLib.Error? error) {
				if (close_request == null) {
					close_request = new Promise<bool> ();
					close_request.resolve (true);
				}

				detached (PROCESS_TERMINATED);
			}

			private void on_session_detached (AgentSessionId id, SessionDetachReason reason) {
				detached (reason);
			}
		}

		private class AgentSessionEntry {
			public AgentSessionId remote_session_id {
				get;
				private set;
			}

			public DBusConnection connection {
				get;
				set;
			}

			public uint sink_registration_id {
				get;
				set;
			}

			public AgentSessionEntry (AgentSessionId remote_session_id, DBusConnection connection) {
				this.remote_session_id = remote_session_id;
				this.connection = connection;
			}

			~AgentSessionEntry () {
				if (sink_registration_id != 0)
					connection.unregister_object (sink_registration_id);
			}
		}

		private class RemoteServer : Object {
			public HostSession session {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public Flavor flavor {
				get;
				construct;
			}

			public enum Flavor {
				REGULAR,
				GADGET
			}

			public TransportBroker? transport_broker {
				get;
				set;
			}

			public RemoteServer (HostSession session, DBusConnection connection, Flavor flavor,
					TransportBroker? transport_broker) {
				Object (
					session: session,
					connection: connection,
					flavor: flavor,
					transport_broker: transport_broker
				);
			}
		}
	}
}
