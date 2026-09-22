namespace Frida {
	public sealed class FruityHostSessionBackend : Object, HostSessionBackend {
		private Fruity.DeviceMonitor device_monitor = new Fruity.DeviceMonitor ();
		private Gee.Map<Fruity.Device, FruityHostSessionProvider> providers =
			new Gee.HashMap<Fruity.Device, FruityHostSessionProvider> ();

		private Cancellable io_cancellable = new Cancellable ();

		construct {
			device_monitor.device_attached.connect (on_device_attached);
			device_monitor.device_detached.connect (on_device_detached);
		}

		public async void start (Cancellable? cancellable) throws IOError {
			yield device_monitor.start (cancellable);
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			yield device_monitor.stop (cancellable);

			foreach (var provider in providers.values) {
				provider_unavailable (provider);
				yield provider.close (cancellable);
			}
			providers.clear ();
		}

		private void on_device_attached (Fruity.Device device) {
			var provider = new FruityHostSessionProvider (device);
			providers[device] = provider;
			provider_available (provider);
		}

		private void on_device_detached (Fruity.Device device) {
			FruityHostSessionProvider provider;
			if (providers.unset (device, out provider)) {
				provider_unavailable (provider);
				provider.close.begin (io_cancellable);
			}
		}
	}

	public sealed class FruityHostSessionProvider : Object, HostSessionProvider, HostChannelProvider, Pairable {
		public Fruity.Device device {
			get;
			construct;
		}

		public string id {
			get { return device.udid; }
		}

		public string name {
			get { return _name; }
		}

		public Variant? icon {
			get { return device.icon ?? _network_icon; }
		}

		public HostSessionProviderKind kind {
			get {
				return (device.connection_type == USB)
					? HostSessionProviderKind.USB
					: HostSessionProviderKind.REMOTE;
			}
		}

		private FruityHostSession? host_session;
		private string _name;
		private static Variant _network_icon;

		static construct {
			_network_icon = make_provider_icon (Frida.Data.Icons.get_fruity_network_png_blob ().data);
		}

		public FruityHostSessionProvider (Fruity.Device device) {
			Object (device: device);
			_name = device.name;
		}

		public async void close (Cancellable? cancellable) throws IOError {
			yield Fruity.DTXConnection.close_all (device, cancellable);
		}

		public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != null)
				throw new Error.INVALID_OPERATION ("Already created");

			uint16 control_port = DEFAULT_CONTROL_PORT;

			if (options != null) {
				Value? control_endpoint_val = options.map["control-endpoint"];
				if (control_endpoint_val != null) {
					if (!control_endpoint_val.holds (typeof (string)))
						throw new Error.INVALID_ARGUMENT ("The control-endpoint option must be a string");
					unowned string control_endpoint = control_endpoint_val.get_string ();
					if (!control_endpoint.has_prefix ("tcp:"))
						throw new Error.INVALID_ARGUMENT ("The control-endpoint must be TCP-flavored");
					control_port = (uint16) uint.parse (control_endpoint[4:]);
				}
			}

			host_session = new FruityHostSession (device, control_port);
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

		public async IOStream open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			return yield device.open_channel (address, cancellable);
		}

		private async void unpair (Cancellable? cancellable) throws Error, IOError {
			try {
				var client = yield device.get_lockdown_client (cancellable);
				yield client.unpair (cancellable);
			} catch (Fruity.LockdownError e) {
				if (e is Fruity.LockdownError.NOT_PAIRED)
					return;
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
		}
	}

	public sealed class FruityHostSession : Object, HostSession {
		public Fruity.Device device {
			get;
			construct;
		}

		public uint16 control_port {
			get;
			construct;
		}

		private Gee.Map<uint, Fruity.ProcessControlService> pids_launched_without_lldb =
			new Gee.HashMap<uint, Fruity.ProcessControlService> ();
		private Gee.HashMap<uint, LLDBSession> lldb_sessions = new Gee.HashMap<uint, LLDBSession> ();
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
		private const string DEBUGSERVER_ENDPOINT_17PLUS = "com.apple.internal.dt.remote.debugproxy";
		private const string DEBUGSERVER_ENDPOINT_14PLUS = "com.apple.debugserver.DVTSecureSocketProxy";
		private const string DEBUGSERVER_ENDPOINT_LEGACY = "com.apple.debugserver?tls=handshake-only";
		private const string[] DEBUGSERVER_ENDPOINT_CANDIDATES = {
			DEBUGSERVER_ENDPOINT_17PLUS,
			DEBUGSERVER_ENDPOINT_14PLUS,
			DEBUGSERVER_ENDPOINT_LEGACY,
		};

		public FruityHostSession (Fruity.Device device, uint16 control_port) {
			Object (device: device, control_port: control_port);
		}

		construct {
			channel_registry.channel_closed.connect (on_channel_closed);
			service_session_registry.session_closed.connect (on_service_session_closed);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (remote_server_request != null) {
				try {
					var server = yield try_get_remote_server (cancellable);
					if (server != null) {
						try {
							yield server.connection.close (cancellable);
						} catch (GLib.Error e) {
						}
					}
				} catch (Error e) {
				}
			}

			while (!gadget_entries.is_empty) {
				var iterator = gadget_entries.values.iterator ();
				iterator.next ();
				var entry = iterator.get ();
				yield entry.close (cancellable);
			}

			while (!lldb_sessions.is_empty) {
				var iterator = lldb_sessions.values.iterator ();
				iterator.next ();
				var session = iterator.get ();
				yield session.close (cancellable);
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

			try {
				var lockdown = yield device.get_lockdown_client (cancellable);
				var response = yield lockdown.get_value (null, null, cancellable);
				Fruity.PlistDict properties = response.get_dict ("Value");

				var os = new HashTable<string, Variant> (str_hash, str_equal);
				os["id"] = "ios";
				os["name"] = properties.get_string ("ProductName");
				os["version"] = properties.get_string ("ProductVersion");
				os["build"] = properties.get_string ("BuildVersion");
				parameters["os"] = os;

				parameters["platform"] = "darwin";

				parameters["arch"] = properties.get_string ("CPUArchitecture").has_prefix ("arm64") ? "arm64" : "arm";

				var hardware = new HashTable<string, Variant> (str_hash, str_equal);
				hardware["product"] = properties.get_string ("ProductType");
				hardware["platform"] = properties.get_string ("HardwarePlatform");
				hardware["model"] = properties.get_string ("HardwareModel");
				parameters["hardware"] = hardware;

				parameters["access"] = "jailed";

				parameters["name"] = properties.get_string ("DeviceName");
				parameters["udid"] = properties.get_string ("UniqueDeviceID");

				add_interfaces (parameters, properties);
			} catch (Fruity.LockdownError e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			} catch (Fruity.PlistError e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			return parameters;
		}

		private static void add_interfaces (HashTable<string, Variant> parameters,
				Fruity.PlistDict properties) throws Fruity.PlistError {
			var ifaces = new VariantBuilder (new VariantType.array (VariantType.VARDICT));

			add_network_interface (ifaces, "ethernet", properties.get_string ("EthernetAddress"));
			add_network_interface (ifaces, "wifi", properties.get_string ("WiFiAddress"));
			add_network_interface (ifaces, "bluetooth", properties.get_string ("BluetoothAddress"));

			if (properties.has ("PhoneNumber")) {
				ifaces.open (VariantType.VARDICT);
				ifaces.add ("{sv}", "type", new Variant.string ("cellular"));
				ifaces.add ("{sv}", "phone-number", new Variant.string (properties.get_string ("PhoneNumber")));
				ifaces.close ();
			}

			parameters["interfaces"] = ifaces.end ();
		}

		private static void add_network_interface (VariantBuilder ifaces, string type, string address) {
			ifaces.open (VariantType.VARDICT);
			ifaces.add ("{sv}", "type", new Variant.string (type));
			ifaces.add ("{sv}", "address", new Variant.string (address));
			ifaces.close ();
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

			var opts = FrontmostQueryOptions._deserialize (options);
			var scope = opts.scope;

			var processes_request = new Promise<Gee.List<Fruity.DeviceInfoService.ProcessInfo>> ();
			var apps_request = new Promise<Gee.List<Fruity.ApplicationDetails>> ();
			fetch_processes.begin (processes_request, cancellable);
			fetch_apps.begin (apps_request, cancellable);

			Gee.List<Fruity.DeviceInfoService.ProcessInfo> processes =
				yield processes_request.future.wait_async (cancellable);
			Fruity.DeviceInfoService.ProcessInfo? process = null;
			string? app_path = null;
			foreach (Fruity.DeviceInfoService.ProcessInfo candidate in processes) {
				if (!candidate.foreground_running)
					continue;

				if (!candidate.is_application)
					continue;

				bool is_main_process;
				string path = compute_app_path_from_executable_path (candidate.real_app_name, out is_main_process);
				if (!is_main_process)
					continue;

				process = candidate;
				app_path = path;
				break;
			}
			if (process == null)
				return HostApplicationInfo.empty ();

			Gee.List<Fruity.ApplicationDetails> apps = yield apps_request.future.wait_async (cancellable);
			Fruity.ApplicationDetails? app = apps.first_match (app => app.path == app_path);
			if (app == null)
				return HostApplicationInfo.empty ();

			unowned string identifier = app.identifier;

			var info = HostApplicationInfo (identifier, app.name, process.pid, make_parameters_dict ());

			if (scope != MINIMAL) {
				add_app_metadata (info.parameters, app);

				add_process_metadata (info.parameters, process);

				if (scope == FULL) {
					var springboard = yield Fruity.SpringboardServicesClient.open (device, cancellable);

					Bytes png = yield springboard.get_icon_png_data (identifier);
					add_app_icons (info.parameters, png);
				}
			}

			return info;
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

			var opts = ApplicationQueryOptions._deserialize (options);
			var scope = opts.scope;

			var apps_request = new Promise<Gee.List<Fruity.ApplicationDetails>> ();
			var processes_request = new Promise<Gee.List<Fruity.DeviceInfoService.ProcessInfo>> ();
			fetch_apps.begin (apps_request, cancellable);
			fetch_processes.begin (processes_request, cancellable);

			Gee.List<Fruity.ApplicationDetails> apps = yield apps_request.future.wait_async (cancellable);
			apps = maybe_filter_apps (apps, opts);

			Gee.Map<string, Bytes>? icons = null;
			if (scope == FULL) {
				var springboard = yield Fruity.SpringboardServicesClient.open (device, cancellable);

				var app_ids = new Gee.ArrayList<string> ();
				foreach (var app in apps)
					app_ids.add (app.identifier);

				icons = yield springboard.get_icon_png_data_batch (app_ids.to_array (), cancellable);
			}

			Gee.List<Fruity.DeviceInfoService.ProcessInfo> processes =
				yield processes_request.future.wait_async (cancellable);
			var process_by_app_path = new Gee.HashMap<string, Fruity.DeviceInfoService.ProcessInfo> ();
			foreach (Fruity.DeviceInfoService.ProcessInfo process in processes) {
				bool is_main_process;
				string app_path = compute_app_path_from_executable_path (process.real_app_name, out is_main_process);
				if (is_main_process)
					process_by_app_path[app_path] = process;
			}

			var result = new HostApplicationInfo[0];

			foreach (Fruity.ApplicationDetails app in apps) {
				unowned string identifier = app.identifier;
				Fruity.DeviceInfoService.ProcessInfo? process = process_by_app_path[app.path];

				var info = HostApplicationInfo (identifier, app.name, (process != null) ? process.pid : 0,
					make_parameters_dict ());

				if (scope != MINIMAL) {
					add_app_metadata (info.parameters, app);

					if (process != null) {
						add_app_state (info.parameters, process);

						add_process_metadata (info.parameters, process);
					}
				}

				if (scope == FULL)
					add_app_icons (info.parameters, icons[identifier]);

				result += info;
			}

			if (server != null && server.flavor == GADGET) {
				try {
					foreach (var app in yield server.session.enumerate_applications (options, cancellable))
						result += app;
				} catch (GLib.Error e) {
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

			var opts = ProcessQueryOptions._deserialize (options);
			var scope = opts.scope;

			var processes_request = new Promise<Gee.List<Fruity.DeviceInfoService.ProcessInfo>> ();
			var apps_request = new Promise<Gee.List<Fruity.ApplicationDetails>> ();
			fetch_processes.begin (processes_request, cancellable);
			fetch_apps.begin (apps_request, cancellable);

			Gee.List<Fruity.DeviceInfoService.ProcessInfo> processes =
				yield processes_request.future.wait_async (cancellable);
			processes = maybe_filter_processes (processes, opts);

			Gee.List<Fruity.ApplicationDetails> apps = yield apps_request.future.wait_async (cancellable);
			var app_by_path = new Gee.HashMap<string, Fruity.ApplicationDetails> ();
			foreach (var app in apps)
				app_by_path[app.path] = app;

			var app_ids = new Gee.ArrayList<string> ();
			var app_pids = new Gee.ArrayList<uint> ();
			var app_by_main_pid = new Gee.HashMap<uint, Fruity.ApplicationDetails> ();
			var app_by_related_pid = new Gee.HashMap<uint, Fruity.ApplicationDetails> ();
			foreach (Fruity.DeviceInfoService.ProcessInfo process in processes) {
				unowned string executable_path = process.real_app_name;

				bool is_main_process;
				string app_path = compute_app_path_from_executable_path (executable_path, out is_main_process);

				Fruity.ApplicationDetails? app = app_by_path[app_path];
				if (app != null) {
					uint pid = process.pid;

					if (is_main_process) {
						app_ids.add (app.identifier);
						app_pids.add (pid);
						app_by_main_pid[pid] = app;
					} else {
						app_by_related_pid[pid] = app;
					}
				}
			}

			Gee.Map<uint, Bytes>? icon_by_pid = null;
			if (scope == FULL) {
				icon_by_pid = new Gee.HashMap<uint, Bytes> ();

				var springboard = yield Fruity.SpringboardServicesClient.open (device, cancellable);

				var pngs = yield springboard.get_icon_png_data_batch (app_ids.to_array (), cancellable);

				int i = 0;
				foreach (string app_id in app_ids) {
					icon_by_pid[app_pids[i]] = pngs[app_id];
					i++;
				}
			}

			var result = new HostProcessInfo[0];

			foreach (Fruity.DeviceInfoService.ProcessInfo process in processes) {
				uint pid = process.pid;
				if (pid == 0)
					continue;

				Fruity.ApplicationDetails? app = app_by_main_pid[pid];
				string name = (app != null) ? app.name : process.name;

				var info = HostProcessInfo (pid, name, make_parameters_dict ());

				if (scope != MINIMAL) {
					var parameters = info.parameters;

					add_process_metadata (parameters, process);

					parameters["path"] = process.real_app_name;

					Fruity.ApplicationDetails? related_app = (app != null) ? app : app_by_related_pid[pid];
					if (related_app != null) {
						string[] applications = { related_app.identifier };
						parameters["applications"] = applications;
					}

					if (app != null && process.foreground_running)
						parameters["frontmost"] = true;
				}

				if (scope == FULL) {
					Bytes? png = icon_by_pid[pid];
					if (png != null)
						add_app_icons (info.parameters, png);
				}

				result += info;
			}

			if (server != null && server.flavor == GADGET) {
				try {
					foreach (var process in yield server.session.enumerate_processes (options, cancellable))
						result += process;
				} catch (GLib.Error e) {
				}
			}

			return result;
		}

		private async void fetch_apps (Promise<Gee.List<Fruity.ApplicationDetails>> promise, Cancellable? cancellable) {
			try {
				var installation_proxy = yield Fruity.InstallationProxyClient.open (device, cancellable);

				var apps = yield installation_proxy.browse (cancellable);

				promise.resolve (apps);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private async void fetch_processes (Promise<Gee.List<Fruity.DeviceInfoService.ProcessInfo>> promise,
				Cancellable? cancellable) {
			try {
				var device_info = yield Fruity.DeviceInfoService.open (device, cancellable);

				var processes = yield device_info.enumerate_processes (cancellable);

				promise.resolve (processes);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private Gee.List<Fruity.ApplicationDetails> maybe_filter_apps (Gee.List<Fruity.ApplicationDetails> apps,
				ApplicationQueryOptions options) {
			if (!options.has_selected_identifiers ())
				return apps;

			var app_by_identifier = new Gee.HashMap<string, Fruity.ApplicationDetails> ();
			foreach (Fruity.ApplicationDetails app in apps)
				app_by_identifier[app.identifier] = app;

			var filtered_apps = new Gee.ArrayList<Fruity.ApplicationDetails> ();
			options.enumerate_selected_identifiers (identifier => {
				Fruity.ApplicationDetails? app = app_by_identifier[identifier];
				if (app != null)
					filtered_apps.add (app);
			});

			return filtered_apps;
		}

		private Gee.List<Fruity.DeviceInfoService.ProcessInfo> maybe_filter_processes (
				Gee.List<Fruity.DeviceInfoService.ProcessInfo> processes, ProcessQueryOptions options) {
			if (!options.has_selected_pids ())
				return processes;

			var process_by_pid = new Gee.HashMap<uint, Fruity.DeviceInfoService.ProcessInfo> ();
			foreach (Fruity.DeviceInfoService.ProcessInfo process in processes)
				process_by_pid[process.pid] = process;

			var filtered_processes = new Gee.ArrayList<Fruity.DeviceInfoService.ProcessInfo> ();
			options.enumerate_selected_pids (pid => {
				Fruity.DeviceInfoService.ProcessInfo? process = process_by_pid[pid];
				if (process != null)
					filtered_processes.add (process);
			});

			return filtered_processes;
		}

		private static string compute_app_path_from_executable_path (string executable_path, out bool is_main_process) {
			string app_path = executable_path;

			int dot_app_start = app_path.last_index_of (".app/");
			if (dot_app_start != -1) {
				app_path = app_path[0:dot_app_start + 4];

				string subpath = executable_path[app_path.length + 1:];
				is_main_process = !("/" in subpath);
			} else {
				is_main_process = false;
			}

			return app_path;
		}

		private void add_app_metadata (HashTable<string, Variant> parameters, Fruity.ApplicationDetails app) {
			string? version = app.version;
			if (version != null)
				parameters["version"] = version;

			string? build = app.build;
			if (build != null)
				parameters["build"] = build;

			parameters["path"] = app.path;

			Gee.Map<string, string> containers = app.containers;
			if (!containers.is_empty) {
				var containers_dict = new VariantBuilder (VariantType.VARDICT);
				foreach (var entry in containers.entries)
					containers_dict.add ("{sv}", entry.key, new Variant.string (entry.value));
				parameters["containers"] = containers_dict.end ();
			}

			if (app.debuggable)
				parameters["debuggable"] = true;
		}

		private void add_app_state (HashTable<string, Variant> parameters, Fruity.DeviceInfoService.ProcessInfo process) {
			if (process.foreground_running)
				parameters["frontmost"] = true;
		}

		private void add_app_icons (HashTable<string, Variant> parameters, Bytes png) {
			var icons = new VariantBuilder (new VariantType.array (VariantType.VARDICT));

			icons.open (VariantType.VARDICT);
			icons.add ("{sv}", "format", new Variant.string ("png"));
			icons.add ("{sv}", "image", Variant.new_from_data (new VariantType ("ay"), png.get_data (), true, png));
			icons.close ();

			parameters["icons"] = icons.end ();
		}

		private void add_process_metadata (HashTable<string, Variant> parameters, Fruity.DeviceInfoService.ProcessInfo? process) {
			DateTime? started = process.start_date;
			if (started != null)
				parameters["started"] = started.format_iso8601 ();
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

			if (options.has_envp)
				throw new Error.NOT_SUPPORTED ("The 'envp' option is not supported when spawning iOS apps");

			if (options.cwd.length > 0)
				throw new Error.NOT_SUPPORTED ("The 'cwd' option is not supported when spawning iOS apps");

			var installation_proxy = yield Fruity.InstallationProxyClient.open (device, cancellable);

			var query = new Fruity.PlistDict ();
			var ids = new Fruity.PlistArray ();
			ids.add_string (program);
			query.set_array ("BundleIDs", ids);

			var matches = yield installation_proxy.lookup (query, cancellable);
			var app = matches[program];
			if (app == null)
				throw new Error.INVALID_ARGUMENT ("Unable to find app with bundle identifier “%s”", program);

			var process_control = yield Fruity.ProcessControlService.open (device, cancellable);

			yield ensure_app_not_running (program, process_control, cancellable);

			if (!app.debuggable) {
				var launch_options = new Fruity.LaunchOptions ();

				string[] args = {};
				if (options.has_argv) {
					var provided_argv = options.argv;
					var length = provided_argv.length;
					for (int i = 1; i < length; i++)
						args += provided_argv[i];
				}
				launch_options.arguments = args;

				uint pid = yield process_control.launch (program, launch_options, cancellable);
				yield process_control.send_signal (pid, LLDB.Signal.SIGSTOP);

				pids_launched_without_lldb[pid] = process_control;
				process_control.process_terminated.connect (on_non_debuggable_process_terminated);
				yield process_control.start_observing_pid (pid, cancellable);

				return pid;
			}

			var launch_options = new LLDB.LaunchOptions ();

			if (options.has_env)
				launch_options.env = options.env;

			HashTable<string, Variant> aux = options.aux;

			Variant? aslr = aux["aslr"];
			if (aslr != null) {
				if (!aslr.is_of_type (VariantType.STRING))
					throw new Error.INVALID_ARGUMENT ("The 'aslr' option must be a string");
				launch_options.aslr = LLDB.ASLR.from_nick (aslr.get_string ());
			}

			string? gadget_path = null;
			Variant? gadget_value = aux["gadget"];
			if (gadget_value != null) {
				if (!gadget_value.is_of_type (VariantType.STRING)) {
					throw new Error.INVALID_ARGUMENT ("The 'gadget' option must be a string pointing at the " +
						"frida-gadget.dylib to use");
				}
				gadget_path = gadget_value.get_string ();
			}

			string[] argv = { app.path };
			if (options.has_argv) {
				var provided_argv = options.argv;
				var length = provided_argv.length;
				for (int i = 1; i < length; i++)
					argv += provided_argv[i];
			}

			var lldb = yield start_lldb_service (cancellable);
			var process = yield lldb.launch (argv, launch_options, cancellable);
			if (process.observed_state == ALREADY_RUNNING) {
				try {
					yield lldb.kill (cancellable);
				} catch (Error e) {
				}
				yield lldb.close (cancellable);

				lldb = yield start_lldb_service (cancellable);
				process = yield lldb.launch (argv, launch_options, cancellable);
			}

			var session = new LLDBSession (lldb, process, gadget_path, ExceptionHandlingStatus.LIMITED, device);
			add_lldb_session (session);

			return process.pid;
		}

		private async void ensure_app_not_running (string bundle_id, Fruity.ProcessControlService process_control,
				Cancellable? cancellable) throws Error, IOError {
			uint existing_pid = yield process_control.process_identifier_for_bundle_identifier (bundle_id, cancellable);
			if (existing_pid == 0)
				return;

			var terminated = new Promise<bool> ();
			var handler = process_control.process_terminated.connect ((pid, exit_code, crashing_signal) => {
				if (pid == existing_pid)
					terminated.resolve (true);
			});

			try {
				yield process_control.start_observing_pid (existing_pid, cancellable);

				uint pid_now = yield process_control.process_identifier_for_bundle_identifier (bundle_id, cancellable);
				if (pid_now != existing_pid) {
					yield process_control.stop_observing_pid (existing_pid, cancellable);
					return;
				}

				yield process_control.kill (existing_pid, cancellable);

				yield terminated.future.wait_async (cancellable);
			} finally {
				process_control.disconnect (handler);
			}
		}

		private void on_non_debuggable_process_terminated (uint pid, int exit_code, int crashing_signal) {
			pids_launched_without_lldb.unset (pid);
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
			Fruity.ProcessControlService process_control;
			if (pids_launched_without_lldb.unset (pid, out process_control)) {
				yield process_control.stop_observing_pid (pid, cancellable);
				yield process_control.send_signal (pid, LLDB.Signal.SIGCONT);
				return;
			}

			var session = lldb_sessions[pid];
			if (session != null) {
				foreach (var entry in gadget_entries.values) {
					if (entry.pid == pid) {
						yield entry.resume (cancellable);
						return;
					}
				}
				assert_not_reached ();
			}

			var server = yield get_remote_server (cancellable);
			try {
				yield server.session.resume (pid, cancellable);
			} catch (GLib.Error e) {
				throw_dbus_error (e);
			}
		}

		public async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			var lldb_session = lldb_sessions[pid];
			if (lldb_session != null) {
				yield lldb_session.kill (cancellable);
				return;
			}

			var server = yield try_get_remote_server (cancellable);
			if (server != null) {
				try {
					yield server.session.kill (pid, cancellable);
					return;
				} catch (GLib.Error e) {
					if (server.flavor == REGULAR)
						throw_dbus_error (e);
				}
			}

			try {
				var lldb = yield start_lldb_service (cancellable);
				var process = yield lldb.attach_by_pid (pid, cancellable);

				lldb_session = new LLDBSession (lldb, process, null, ExceptionHandlingStatus.FULL, device);
				yield lldb_session.kill (cancellable);
				yield lldb_session.close (cancellable);
			} catch (Error e) {
				var process_control = yield Fruity.ProcessControlService.open (device, cancellable);
				yield process_control.kill (pid, cancellable);
			}
		}

		public async AgentSessionId attach (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			var lldb_session = lldb_sessions[pid];
			if (lldb_session != null) {
				var gadget_details = yield lldb_session.query_gadget_details (cancellable);

				lldb_session.attach_count++;
				return yield attach_via_gadget (pid, options, gadget_details, cancellable);
			}

			var server = yield try_get_remote_server (cancellable);
			if (server != null) {
				try {
					return yield attach_via_remote (pid, options, server, cancellable);
				} catch (Error e) {
					if (server.flavor == REGULAR)
						throw_api_error (e);
				}
			}

			if (pid == 0)
				throw new Error.NOT_SUPPORTED ("The Frida system session is not available on jailed iOS");

			var lldb = yield start_lldb_service (cancellable);
			var process = yield lldb.attach_by_pid (pid, cancellable);

			string? gadget_path = null;

			lldb_session = new LLDBSession (lldb, process, gadget_path, ExceptionHandlingStatus.LIMITED, device);
			add_lldb_session (lldb_session);

			var gadget_details = yield lldb_session.query_gadget_details (cancellable);

			lldb_session.attach_count++;
			return yield attach_via_gadget (pid, options, gadget_details, cancellable);
		}

		public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		private async LLDB.Client start_lldb_service (Cancellable? cancellable) throws Error, IOError {
			foreach (unowned string endpoint in DEBUGSERVER_ENDPOINT_CANDIDATES) {
				try {
					var lldb_stream = yield device.open_lockdown_service (endpoint, cancellable);
					return yield LLDB.Client.open (lldb_stream, cancellable);
				} catch (Error e) {
					if (!(e is Error.NOT_SUPPORTED))
						throw new Error.NOT_SUPPORTED ("%s", e.message);
				}
			}

			throw new Error.NOT_SUPPORTED ("This feature requires an iOS Developer Disk Image to be mounted; " +
				"run Xcode briefly or use ideviceimagemounter to mount one manually");
		}

		private async AgentSessionId attach_via_gadget (uint pid, HashTable<string, Variant> options,
				Fruity.Injector.GadgetDetails gadget_details, Cancellable? cancellable) throws Error, IOError {
			try {
				var stream = yield device.open_channel (
					("tcp:%" + uint16.FORMAT_MODIFIER + "u").printf (gadget_details.port),
					cancellable);

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
				var gadget_entry = new GadgetEntry (local_session_id, host_session, connection, pid);
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
					entry.connection = yield establish_direct_connection (transport_broker, remote_session_id, server,
						cancellable);
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
			var stream = yield device.open_channel (address, cancellable);

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

		private void on_service_session_closed (ServiceSessionId id) {
			service_session_closed (id);
		}

		public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
			var session = yield do_open_service (address, cancellable);

			var id = ServiceSessionId.generate ();
			service_session_registry.register (id, session);

			return id;
		}

		private async ServiceSession do_open_service (string address, Cancellable? cancellable) throws Error, IOError {
			string[] tokens = address.split (":", 2);
			unowned string protocol = tokens[0];
			unowned string service_name = tokens[1];

			if (protocol == "plist") {
				var stream = yield device.open_lockdown_service (service_name, cancellable);

				return new PlistServiceSession (stream);
			}

			if (protocol == "dtx") {
				var connection = yield Fruity.DTXConnection.obtain (device, cancellable);

				return new DTXServiceSession (service_name, connection);
			}

			if (protocol == "xpc") {
				var tunnel = yield device.find_tunnel (cancellable);
				if (tunnel == null)
					throw new Error.NOT_SUPPORTED ("RemoteXPC not supported by device");

				var service_info = tunnel.discovery.get_service (service_name);
				var stream = yield tunnel.open_tcp_connection (service_info.port, cancellable);

				return new XpcServiceSession (new Fruity.XpcConnection (stream));
			}

			if (protocol == "syscall-trace") {
				var core_profile = yield Fruity.CoreProfileService.open (device, cancellable);
				var device_info = yield Fruity.DeviceInfoService.open (device, cancellable);

				return new FruitySyscallTraceServiceSession (core_profile, device_info);
			}

			throw new Error.NOT_SUPPORTED ("Unsupported service address");
		}

		public ServiceSession link_service_session (ServiceSessionId id) throws Error {
			return service_session_registry.link (id);
		}

		public void unlink_service_session (ServiceSessionId id) {
			service_session_registry.unlink (id);
		}

		private void add_lldb_session (LLDBSession session) {
			lldb_sessions[session.process.pid] = session;

			session.closed.connect (on_lldb_session_closed);
			session.output.connect (on_lldb_session_output);
		}

		private void remove_lldb_session (LLDBSession session) {
			lldb_sessions.unset (session.process.pid);

			session.closed.disconnect (on_lldb_session_closed);
			session.output.disconnect (on_lldb_session_output);
		}

		private void on_lldb_session_closed (LLDBSession session) {
			remove_lldb_session (session);
		}

		private void on_lldb_session_output (LLDBSession session, Bytes bytes) {
			output (session.process.pid, 1, bytes.get_data ());
		}

		private void on_gadget_entry_detached (GadgetEntry entry, SessionDetachReason reason) {
			detach_entry.begin (entry, reason);
		}

		private async void detach_entry (GadgetEntry entry, SessionDetachReason reason) throws Error, IOError {
			AgentSessionId id = entry.local_session_id;
			var no_crash = CrashInfo.empty ();

			var lldb_session = lldb_sessions[entry.pid];
			if (lldb_session != null) {
				lldb_session.attach_count--;
				if (lldb_session.attach_count == 0) {
					remove_lldb_session (lldb_session);
					if (reason == SessionDetachReason.APPLICATION_REQUESTED)
						yield entry.detach_lldb (lldb_session, io_cancellable);
					else
						yield lldb_session.close (io_cancellable);
				}
			}

			gadget_entries.unset (id);
			agent_sessions.unset (id);

			entry.detached.disconnect (on_gadget_entry_detached);

			agent_session_detached (id, reason, no_crash);

			entry.close.begin (io_cancellable);
		}

		private async RemoteServer? try_get_remote_server (Cancellable? cancellable) throws Error, IOError {
			try {
				return yield get_remote_server (cancellable);
			} catch (Error e) {
				if (e is Error.SERVER_NOT_RUNNING)
					return null;
				throw e;
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
				var channel = yield connect_to_remote_server (cancellable);

				IOStream stream = channel.stream;
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

				var server = new RemoteServer (flavor, session, connection, channel, device, transport_broker);
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
					if (e is Error) {
						api_error = e;
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

		private async Fruity.TcpChannel connect_to_remote_server (Cancellable? cancellable) throws Error, IOError {
			var tunnel = yield device.find_tunnel (cancellable);
			bool tunnel_recently_opened = tunnel != null && get_monotonic_time () - tunnel.opened_at < 1000000;

			uint delays[] = { 0, 50, 250 };
			uint max_attempts = tunnel_recently_opened ? delays.length : 1;
			var main_context = MainContext.ref_thread_default ();

			Error? pending_error = null;
			for (uint attempts = 0; attempts != max_attempts; attempts++) {
				uint delay = delays[attempts];
				if (delay != 0) {
					var timeout_source = new TimeoutSource (delay);
					timeout_source.set_callback (connect_to_remote_server.callback);
					timeout_source.attach (main_context);

					var cancel_source = new CancellableSource (cancellable);
					cancel_source.set_callback (connect_to_remote_server.callback);
					cancel_source.attach (main_context);

					yield;

					cancel_source.destroy ();
					timeout_source.destroy ();

					if (cancellable.is_cancelled ())
						break;
				}

				bool is_last_attempt = attempts == max_attempts - 1;
				var open_flags = is_last_attempt
					? Fruity.OpenTcpChannelFlags.ALLOW_ANY_TRANSPORT
					: Fruity.OpenTcpChannelFlags.ALLOW_TUNNEL;

				try {
					return yield device.open_tcp_channel (control_port.to_string (), open_flags, cancellable);
				} catch (Error e) {
					pending_error = e;
					if (!(e is Error.SERVER_NOT_RUNNING))
						break;
				}
			}
			throw pending_error;
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

		private class LLDBSession : Object {
			public signal void closed ();
			public signal void detached ();
			public signal void output (Bytes bytes);

			public LLDB.Client lldb {
				get;
				construct;
			}

			public LLDB.Process process {
				get;
				construct;
			}

			public string? gadget_path {
				get;
				construct;
			}

			public weak HostChannelProvider channel_provider {
				get;
				construct;
			}

			public ExceptionHandlingStatus exception_handling_status {
				get;
				construct;
			}

			public uint attach_count = 0;

			private Promise<Fruity.Injector.GadgetDetails>? gadget_request;
			private ulong exception_handler = 0;
			private string? main_thread_id;
			private bool resumed = false;

			private Cancellable io_cancellable = new Cancellable ();

			private const size_t PAGE_SIZE = 0x4000;

			public LLDBSession (LLDB.Client lldb, LLDB.Process process, string? gadget_path, ExceptionHandlingStatus ehs,
					HostChannelProvider channel_provider) {
				Object (
					lldb: lldb,
					process: process,
					gadget_path: gadget_path,
					exception_handling_status: ehs,
					channel_provider: channel_provider
				);
			}

			construct {
				lldb.closed.connect (on_lldb_closed);
				lldb.console_output.connect (on_lldb_console_output);
			}

			~LLDBSession () {
				detach_exception_handler ();

				lldb.closed.disconnect (on_lldb_closed);
				lldb.console_output.disconnect (on_lldb_console_output);
			}

			public async void close (Cancellable? cancellable) throws IOError {
				io_cancellable.cancel ();

				detach_exception_handler ();

				yield lldb.close (cancellable);
			}

			private async void handle_exception (GDB.Exception exception) throws Error, IOError {
				var e = (Frida.LLDB.Exception) exception;

				var sig = (LLDB.Signal) e.signum;
				var medata = e.medata;
				if (sig == SIGTRAP && e.metype == EXC_BREAKPOINT && medata.size == 2) {
					uint64 x1 = e.context["x1"];
					uint64 x2 = e.context["x2"];
					var action = (GadgetBreakpointAction) e.context["x3"];
					if (x1 == x2 == 1337) {
						uint64 pc = e.context["pc"];
						var thread = e.thread;
						yield thread.write_register ("pc", pc + 4, io_cancellable);

						switch (action) {
							case RESUME: {
								resumed = true;
								yield lldb.continue (io_cancellable);

								return;
							}
							case DETACH: {
								yield lldb.detach (io_cancellable);
								detached ();

								break;
							}
							case PAGE_PLAN: {
								var plan_size = (size_t) e.context["x4"];
								var plan_address = e.context["x5"];
								yield DebuggerMappings.handle_page_plan (lldb, plan_address, plan_size,
									PAGE_SIZE, io_cancellable);

								yield thread.write_register ("x0", 0x1337, io_cancellable);

								if (resumed)
									yield lldb.continue (io_cancellable);
								else
									yield continue_gadget_threads (io_cancellable);

								return;
							}
							default:
								break;
						}
					}
				}

				detach_exception_handler ();

				yield lldb.close (io_cancellable);
			}

			private async void continue_gadget_threads (Cancellable? cancellable) throws Error, IOError {
				var gadget_threads = new Gee.ArrayList<LLDB.Thread> ();
				yield lldb.enumerate_threads (thread => {
					if (thread.id != main_thread_id)
						gadget_threads.add (thread);
					return true;
				}, cancellable);
				yield lldb.continue_specific_threads (gadget_threads, cancellable);
			}

			public async void kill (Cancellable? cancellable) throws Error, IOError {
				io_cancellable.cancel ();

				detach_exception_handler ();

				if (exception_handling_status == FULL)
					yield lldb.kill (cancellable);
				else
					yield lldb.close (cancellable);
			}

			public async Fruity.Injector.GadgetDetails query_gadget_details (Cancellable? cancellable) throws Error, IOError {
				while (gadget_request != null) {
					try {
						return yield gadget_request.future.wait_async (cancellable);
					} catch (Error e) {
						throw e;
					} catch (IOError e) {
						cancellable.set_error_if_cancelled ();
					}
				}
				gadget_request = new Promise<Fruity.Injector.GadgetDetails> ();

				try {
					string? path = gadget_path;
					if (path == null) {
						path = Path.build_filename (Environment.get_user_cache_dir (), "frida", "gadget-ios.dylib");
						if (!FileUtils.test (path, FileTest.EXISTS)) {
							throw new Error.NOT_SUPPORTED ("Need Gadget to attach on jailed iOS; its default location is: %s",
								path);
						}
					}

					if (process.cpu_type != ARM64)
						throw new Error.NOT_SUPPORTED ("Unsupported CPU; only arm64 is supported on jailed iOS");

					var ptrauth_support = (process.cpu_subtype == ARM64E)
						? Gum.PtrauthSupport.SUPPORTED
						: Gum.PtrauthSupport.UNSUPPORTED;
					var module = new Gum.DarwinModule.from_file (path, Gum.CpuType.ARM64, ptrauth_support);

					Fruity.Injector.Transaction transaction =
						yield Fruity.Injector.inject ((owned) module, lldb, channel_provider, cancellable);
					Fruity.Injector.GadgetDetails gadget = transaction.gadget;
					main_thread_id = transaction.main_thread_id;

					exception_handler = lldb.notify["exception"].connect ((obj, pspec) => {
						var exception = lldb.exception;
						if (exception != null)
							handle_exception.begin (exception);
					});

					yield transaction.commit (cancellable);

					gadget_request.resolve (gadget);

					return gadget;
				} catch (GLib.Error e) {
					var api_error = (e is Error)
						? (Error) e
						: new Error.NOT_SUPPORTED ("%s", e.message);

					gadget_request.reject (api_error);
					gadget_request = null;

					throw api_error;
				}
			}

			private void detach_exception_handler () {
				if (exception_handler != 0) {
					if (lldb.state != CLOSED)
						lldb.disconnect (exception_handler);
					exception_handler = 0;
				}
			}

			private void on_lldb_closed () {
				closed ();
			}

			private void on_lldb_console_output (Bytes bytes) {
				output (bytes);
			}
		}

		private enum ExceptionHandlingStatus {
			FULL,
			LIMITED,
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

			public uint pid {
				get;
				construct;
			}

			private Promise<bool>? close_request;
			private Promise<bool>? detach_request;

			public GadgetEntry (AgentSessionId local_session_id, HostSession host_session, DBusConnection connection, uint pid) {
				Object (
					local_session_id: local_session_id,
					host_session: host_session,
					connection: connection,
					pid: pid
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

			public async void resume (Cancellable? cancellable) throws Error, IOError {
				try {
					GadgetSession session = yield connection.get_proxy (null, ObjectPath.GADGET_SESSION,
						DO_NOT_LOAD_PROPERTIES, cancellable);

					yield session.break_and_resume (cancellable);
				} catch (GLib.Error e) {
					throw_dbus_error (e);
				}
			}

			public async void detach_lldb (LLDBSession lldb_session, Cancellable? cancellable) throws Error, IOError {
				while (detach_request != null) {
					try {
						yield detach_request.future.wait_async (cancellable);
						return;
					} catch (Error e) {
						assert_not_reached ();
					} catch (IOError e) {
						cancellable.set_error_if_cancelled ();
					}
				}
				detach_request = new Promise<bool> ();

				try {
					GadgetSession session = yield connection.get_proxy (null, ObjectPath.GADGET_SESSION,
						DO_NOT_LOAD_PROPERTIES, cancellable);

					lldb_session.detached.connect (on_lldb_session_detached);

					yield session.break_and_detach (cancellable);

					yield detach_request.future.wait_async (cancellable);
				} catch (GLib.Error e) {
					detach_request.reject (e);
					throw_dbus_error (e);
				} finally {
					lldb_session.detached.disconnect (on_lldb_session_detached);
					detach_request = null;
				}
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

			private void on_lldb_session_detached () {
				if (detach_request != null)
					detach_request.resolve (true);
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

		private class RemoteServer : Object, HostChannelProvider {
			public Flavor flavor {
				get;
				construct;
			}

			public HostSession session {
				get;
				construct;
			}

			public DBusConnection connection {
				get;
				construct;
			}

			public Fruity.TcpChannel channel {
				get;
				construct;
			}

			public Fruity.Device device {
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

			public RemoteServer (Flavor flavor, HostSession session, DBusConnection connection, Fruity.TcpChannel channel,
					Fruity.Device device, TransportBroker? transport_broker) {
				Object (
					flavor: flavor,
					session: session,
					connection: connection,
					channel: channel,
					device: device,
					transport_broker: transport_broker
				);
			}

			public async IOStream open_channel (string address, Cancellable? cancellable) throws Error, IOError {
				if (!address.has_prefix ("tcp:"))
					throw new Error.NOT_SUPPORTED ("Unsupported channel address");
				var flags = (channel.kind == TUNNEL)
					? Fruity.OpenTcpChannelFlags.ALLOW_TUNNEL
					: Fruity.OpenTcpChannelFlags.ALLOW_USBMUX;
				var channel = yield device.open_tcp_channel (address[4:], flags, cancellable);
				return channel.stream;
			}
		}
	}

	private sealed class PlistServiceSession : Object, ServiceSession {
		public IOStream stream {
			get;
			construct;
		}

		private Fruity.PlistServiceClient client;
		private bool client_closed = false;

		public PlistServiceSession (IOStream stream) {
			Object (stream: stream);
		}

		construct {
			client = new Fruity.PlistServiceClient (stream);
			client.closed.connect (on_client_closed);
		}

		~PlistServiceSession () {
			client.closed.disconnect (on_client_closed);
		}

		public async void activate (Cancellable? cancellable) throws Error, IOError {
			ensure_active ();
		}

		private void ensure_active () throws Error {
			if (client_closed)
				throw new Error.INVALID_OPERATION ("Service is closed");
		}

		public async void cancel (Cancellable? cancellable) throws IOError {
			if (client_closed)
				return;
			client_closed = true;

			yield client.close (cancellable);

			close ();
		}

		public async Variant request (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			ensure_active ();

			var reader = new VariantReader (parameters);

			string type = reader.read_member ("type").get_string_value ();
			reader.end_member ();

			try {
				if (type == "query") {
					reader.read_member ("payload");
					var payload = plist_from_variant (reader.current_object);
					var raw_response = yield client.query (payload, cancellable);
					return plist_to_variant (raw_response);
				} else if (type == "read") {
					var plist = yield client.read_message (cancellable);
					return plist_to_variant (plist);
				} else {
					throw new Error.INVALID_ARGUMENT ("Unsupported request type: %s", type);
				}
			} catch (Fruity.PlistServiceError e) {
				if (e is Fruity.PlistServiceError.CONNECTION_CLOSED)
					throw new Error.TRANSPORT ("Connection closed during request");
				throw new Error.PROTOCOL ("%s", e.message);
			}
		}

		private void on_client_closed () {
			client_closed = true;
			close ();
		}

		private Fruity.Plist plist_from_variant (Variant val) throws Error {
			if (!val.is_of_type (VariantType.VARDICT))
				throw new Error.INVALID_ARGUMENT ("Expected a dictionary");

			var plist = new Fruity.Plist ();

			foreach (var item in val) {
				string k;
				Variant v;
				item.get ("{sv}", out k, out v);

				plist.set_value (k, plist_value_from_variant (v));
			}

			return plist;
		}

		private Value plist_value_from_variant (Variant val) throws Error {
			switch (val.classify ()) {
				case BOOLEAN:
					return val.get_boolean ();
				case INT64:
					return val.get_int64 ();
				case DOUBLE:
					return val.get_double ();
				case STRING:
					return val.get_string ();
				case ARRAY:
					if (val.is_of_type (new VariantType ("ay")))
						return val.get_data_as_bytes ();

					if (val.is_of_type (VariantType.VARDICT)) {
						var dict = new Fruity.PlistDict ();

						foreach (var item in val) {
							string k;
							Variant v;
							item.get ("{sv}", out k, out v);

							dict.set_value (k, plist_value_from_variant (v));
						}

						return dict;
					}

					if (val.is_of_type (new VariantType ("av"))) {
						var arr = new Fruity.PlistArray ();

						foreach (var item in val) {
							Variant v;
							item.get ("v", out v);

							arr.add_value (plist_value_from_variant (v));
						}

						return arr;
					}

					break;
				default:
					break;
			}

			throw new Error.INVALID_ARGUMENT ("Unsupported type: %s", (string) val.get_type ().peek_string ());
		}

		private Variant plist_to_variant (Fruity.Plist plist) {
			return plist_dict_to_variant (plist);
		}

		private Variant plist_dict_to_variant (Fruity.PlistDict dict) {
			var builder = new VariantBuilder (VariantType.VARDICT);
			foreach (var e in dict.entries)
				builder.add ("{sv}", e.key, plist_value_to_variant (e.value));
			return builder.end ();
		}

		private Variant plist_array_to_variant (Fruity.PlistArray arr) {
			var builder = new VariantBuilder (new VariantType.array (VariantType.VARIANT));
			foreach (var e in arr.elements)
				builder.add ("v", plist_value_to_variant (e));
			return builder.end ();
		}

		private Variant plist_value_to_variant (Value * v) {
			Type t = v.type ();

			if (t == typeof (bool))
				return v.get_boolean ();

			if (t == typeof (int64))
				return v.get_int64 ();

			if (t == typeof (float))
				return (double) v.get_float ();

			if (t == typeof (double))
				return v.get_double ();

			if (t == typeof (string))
				return v.get_string ();

			if (t == typeof (Bytes)) {
				var bytes = (Bytes) v.get_boxed ();
				return Variant.new_from_data (new VariantType.array (VariantType.BYTE), bytes.get_data (), true, bytes);
			}

			if (t == typeof (Fruity.PlistDict))
				return plist_dict_to_variant ((Fruity.PlistDict) v.get_object ());

			if (t == typeof (Fruity.PlistArray))
				return plist_array_to_variant ((Fruity.PlistArray) v.get_object ());

			if (t == typeof (Fruity.PlistUid))
				return ((Fruity.PlistUid) v.get_object ()).uid;

			assert_not_reached ();
		}
	}

	private sealed class DTXServiceSession : Object, ServiceSession {
		public string identifier {
			get;
			construct;
		}

		public Fruity.DTXConnection connection {
			get;
			construct;
		}

		private State state = INACTIVE;
		private bool connection_closed = false;
		private Fruity.DTXChannel? channel;

		private enum State {
			INACTIVE,
			ACTIVE,
		}

		public DTXServiceSession (string identifier, Fruity.DTXConnection connection) {
			Object (identifier: identifier, connection: connection);
		}

		construct {
			connection.notify["state"].connect (on_connection_state_changed);
		}

		~DTXServiceSession () {
			connection.notify["state"].disconnect (on_connection_state_changed);
		}

		public async void activate (Cancellable? cancellable) throws Error, IOError {
			ensure_active ();
		}

		private void ensure_active () throws Error {
			if (connection_closed)
				throw new Error.INVALID_OPERATION ("Service is closed");

			if (state == INACTIVE) {
				state = ACTIVE;

				channel = connection.make_channel (identifier);
				channel.invocation.connect (on_channel_invocation);
				channel.notification.connect (on_channel_notification);
			}
		}

		private void ensure_closed () {
			if (connection_closed)
				return;
			connection_closed = true;

			if (channel != null) {
				channel.invocation.disconnect (on_channel_invocation);
				channel.notification.disconnect (on_channel_notification);
				channel = null;
			}

			close ();
		}

		public async void cancel (Cancellable? cancellable) throws IOError {
			ensure_closed ();
		}

		public async Variant request (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			ensure_active ();

			var reader = new VariantReader (parameters);

			string method_name = reader.read_member ("method").get_string_value ();
			reader.end_member ();

			Fruity.DTXArgumentListBuilder? args = null;
			if (reader.has_member ("args")) {
				reader.read_member ("args");
				args = new Fruity.DTXArgumentListBuilder ();
				uint n = reader.count_elements ();
				for (uint i = 0; i != n; i++) {
					reader.read_element (i);
					args.append_object (nsobject_from_variant (reader.current_object));
					reader.end_element ();
				}
			}

			var result = yield channel.invoke (method_name, args, cancellable);

			return nsobject_to_variant (result);
		}

		private void on_connection_state_changed (Object obj, ParamSpec pspec) {
			if (connection.state == CLOSED)
				ensure_closed ();
		}

		private void on_channel_invocation (string method_name, Fruity.DTXArgumentList args,
				Fruity.DTXMessageTransportFlags transport_flags) {
			var envelope = new HashTable<string, Variant> (str_hash, str_equal);
			envelope["type"] = "invocation";
			envelope["payload"] = invocation_to_variant (method_name, args);
			envelope["expects-reply"] = (transport_flags & Fruity.DTXMessageTransportFlags.EXPECTS_REPLY) != 0;
			message (envelope);
		}

		private void on_channel_notification (Fruity.NSObject obj) {
			var envelope = new HashTable<string, Variant> (str_hash, str_equal);
			envelope["type"] = "notification";
			envelope["payload"] = nsobject_to_variant (obj);
			message (envelope);
		}

		private Fruity.NSObject? nsobject_from_variant (Variant val) throws Error {
			switch (val.classify ()) {
				case BOOLEAN:
					return new Fruity.NSNumber.from_boolean (val.get_boolean ());
				case INT64:
					return new Fruity.NSNumber.from_integer (val.get_int64 ());
				case DOUBLE:
					return new Fruity.NSNumber.from_double (val.get_double ());
				case STRING:
					return new Fruity.NSString (val.get_string ());
				case ARRAY:
					if (val.is_of_type (new VariantType ("ay")))
						return new Fruity.NSData (val.get_data_as_bytes ());

					if (val.is_of_type (VariantType.VARDICT)) {
						var dict = new Fruity.NSDictionary ();

						foreach (var item in val) {
							string k;
							Variant v;
							item.get ("{sv}", out k, out v);

							dict.set_value (k, nsobject_from_variant (v));
						}

						return dict;
					}

					if (val.is_of_type (new VariantType ("av"))) {
						var arr = new Fruity.NSArray ();

						foreach (var item in val) {
							Variant v;
							item.get ("v", out v);

							arr.add_object (nsobject_from_variant (v));
						}

						return arr;
					}

					break;
				default:
					break;
			}

			throw new Error.INVALID_ARGUMENT ("Unsupported type: %s", (string) val.get_type ().peek_string ());
		}

		private Variant nsobject_to_variant (Fruity.NSObject? obj) {
			if (obj == null)
				return new Variant ("()");

			var num = obj as Fruity.NSNumber;
			if (num != null)
				return num.integer;

			var str = obj as Fruity.NSString;
			if (str != null)
				return str.str;

			var data = obj as Fruity.NSData;
			if (data != null) {
				Bytes bytes = data.bytes;
				return Variant.new_from_data (new VariantType.array (VariantType.BYTE), bytes.get_data (), true, bytes);
			}

			var dict = obj as Fruity.NSDictionary;
			if (dict != null)
				return nsdictionary_to_variant (dict);

			var dict_raw = obj as Fruity.NSDictionaryRaw;
			if (dict_raw != null)
				return nsdictionary_raw_to_variant (dict_raw);

			var arr = obj as Fruity.NSArray;
			if (arr != null)
				return nsarray_to_variant (arr);

			var date = obj as Fruity.NSDate;
			if (date != null)
				return date.to_date_time ().format_iso8601 ();

			var err = obj as Fruity.NSError;
			if (err != null)
				return nserror_to_variant (err);

			var msg = obj as Fruity.DTTapMessage;
			if (msg != null)
				return nsdictionary_to_variant (msg.plist);

			assert_not_reached ();
		}

		private Variant nsdictionary_to_variant (Fruity.NSDictionary dict) {
			var builder = new VariantBuilder (VariantType.VARDICT);
			foreach (var e in dict.entries)
				builder.add ("{sv}", e.key, nsobject_to_variant (e.value));
			return builder.end ();
		}

		private Variant nsdictionary_raw_to_variant (Fruity.NSDictionaryRaw dict) {
			var builder = new VariantBuilder (
				new VariantType.array (new VariantType.dict_entry (VariantType.VARIANT, VariantType.VARIANT)));
			foreach (var e in dict.entries)
				builder.add ("{vv}", nsobject_to_variant (e.key), nsobject_to_variant (e.value));
			return builder.end ();
		}

		private Variant nsarray_to_variant (Fruity.NSArray arr) {
			var builder = new VariantBuilder (new VariantType.array (VariantType.VARIANT));
			foreach (var e in arr.elements)
				builder.add ("v", nsobject_to_variant (e));
			return builder.end ();
		}

		private Variant nserror_to_variant (Fruity.NSError e) {
			var result = new HashTable<string, Variant> (str_hash, str_equal);
			result["domain"] = e.domain.str;
			result["code"] = e.code;
			result["user-info"] = nsdictionary_to_variant (e.user_info);
			return result;
		}

		private Variant invocation_to_variant (string method_name, Fruity.DTXArgumentList args) {
			var invocation = new HashTable<string, Variant> (str_hash, str_equal);
			invocation["method"] = method_name;
			invocation["args"] = invocation_args_to_variant (args);
			return invocation;
		}

		private Variant invocation_args_to_variant (Fruity.DTXArgumentList args) {
			var builder = new VariantBuilder (new VariantType.array (VariantType.VARIANT));
			foreach (var e in args.elements)
				builder.add ("v", value_to_variant (e));
			return builder.end ();
		}

		private Variant value_to_variant (Value v) {
			Type t = v.type ();

			if (t == typeof (int))
				return v.get_int ();

			if (t == typeof (int64))
				return v.get_int64 ();

			if (t == typeof (double))
				return v.get_double ();

			if (t == typeof (string))
				return v.get_string ();

			if (t.is_a (typeof (Fruity.NSObject)))
				return nsobject_to_variant ((Fruity.NSObject) v.get_boxed ());

			assert_not_reached ();
		}
	}

	private sealed class XpcServiceSession : Object, ServiceSession {
		public Fruity.XpcConnection connection {
			get;
			construct;
		}

		private State state = INACTIVE;
		private bool connection_closed = false;

		private enum State {
			INACTIVE,
			ACTIVE,
		}

		public XpcServiceSession (Fruity.XpcConnection connection) {
			Object (connection: connection);
		}

		construct {
			connection.close.connect (on_close);
			connection.message.connect (on_message);
		}

		~XpcServiceSession () {
			connection.close.disconnect (on_close);
			connection.message.disconnect (on_message);

			connection.cancel ();
		}

		public async void activate (Cancellable? cancellable) throws Error, IOError {
			ensure_active ();
		}

		private void ensure_active () throws Error {
			if (connection_closed)
				throw new Error.INVALID_OPERATION ("Service is closed");

			if (state == INACTIVE) {
				state = ACTIVE;

				connection.activate ();
			}
		}

		public async void cancel (Cancellable? cancellable) throws IOError {
			if (state == ACTIVE)
				connection.cancel ();
			else if (!connection_closed)
				on_close (null);
		}

		public async Variant request (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			ensure_active ();

			yield connection.wait_until_ready (cancellable);

			if (!parameters.is_of_type (VariantType.VARDICT))
				throw new Error.INVALID_ARGUMENT ("Expected a dictionary");

			var builder = new Fruity.XpcBodyBuilder ();
			builder.begin_dictionary ();
			add_vardict_values (parameters, builder);
			Fruity.TrustedService.add_standard_request_values (builder);
			builder.end_dictionary ();

			Fruity.XpcMessage response = yield connection.request (builder.build (), cancellable);

			return response.body;
		}

		private void on_close (Error? error) {
			connection_closed = true;

			close ();
		}

		private void on_message (Fruity.XpcMessage msg) {
			message (msg.body);
		}

		private static void add_vardict_values (Variant dict, Fruity.XpcBodyBuilder builder) throws Error {
			foreach (var item in dict) {
				string key;
				Variant val;
				item.get ("{sv}", out key, out val);

				builder.set_member_name (key);
				add_variant_value (val, builder);
			}
		}

		private static void add_vararray_values (Variant arr, Fruity.XpcBodyBuilder builder) throws Error {
			foreach (var item in arr) {
				Variant val;
				item.get ("v", out val);

				add_variant_value (val, builder);
			}
		}

		private static void add_variant_value (Variant val, Fruity.XpcBodyBuilder builder) throws Error {
			switch (val.classify ()) {
				case BOOLEAN:
					builder.add_bool_value (val.get_boolean ());
					return;
				case INT64:
					builder.add_int64_value (val.get_int64 ());
					return;
				case STRING:
					builder.add_string_value (val.get_string ());
					return;
				case ARRAY:
					if (val.is_of_type (new VariantType ("ay"))) {
						builder.add_data_value (val.get_data_as_bytes ());
						return;
					}

					if (val.is_of_type (VariantType.VARDICT)) {
						builder.begin_dictionary ();
						add_vardict_values (val, builder);
						builder.end_dictionary ();
						return;
					}

					if (val.is_of_type (new VariantType ("av"))) {
						builder.begin_array ();
						add_vararray_values (val, builder);
						builder.end_array ();
						return;
					}

					break;
				case TUPLE:
					if (val.n_children () != 2) {
						throw new Error.INVALID_ARGUMENT ("Invalid type annotation: %s",
							(string) val.get_type ().peek_string ());
					}

					var type = val.get_child_value (0);
					if (!type.is_of_type (VariantType.STRING)) {
						throw new Error.INVALID_ARGUMENT ("Invalid type annotation: %s",
							(string) val.get_type ().peek_string ());
					}
					unowned string type_str = type.get_string ();

					add_variant_value_of_type (val.get_child_value (1), type_str, builder);
					return;
				default:
					break;
			}

			throw new Error.INVALID_ARGUMENT ("Unsupported type: %s", (string) val.get_type ().peek_string ());
		}

		private static void add_variant_value_of_type (Variant val, string type, Fruity.XpcBodyBuilder builder) throws Error {
			switch (type) {
				case "data":
					check_type (val, new VariantType ("ay"));
					builder.add_data_value (val.get_data_as_bytes ());
					break;
				case "uuid":
					check_type (val, new VariantType ("ay"));
					if (val.get_size () != 16)
						throw new Error.INVALID_ARGUMENT ("Invalid UUID");
					unowned uint8[] data = (uint8[]) val.get_data ();
					builder.add_uuid_value (data[:16]);
					break;
				default:
					throw new Error.INVALID_ARGUMENT ("Unsupported type: %s", type);
			}
		}

		private static void check_type (Variant v, VariantType t) throws Error {
			if (!v.is_of_type (t)) {
				throw new Error.INVALID_ARGUMENT ("Invalid %s: %s",
					(string) t.peek_string (),
					(string) v.get_type ().peek_string ());
			}
		}
	}

	private sealed class FruitySyscallTraceServiceSession : Object, ServiceSession {
		public Fruity.CoreProfileService core_profile {
			get;
			construct;
		}

		public Fruity.DeviceInfoService info_service {
			get;
			construct;
		}

		private MainContext main_context;
		private ThreadPool<Bytes> pool;
		private Cancellable io_cancellable = new Cancellable ();

		private Mutex mutex = Mutex ();

		private Gee.Queue<Variant> pending_events = new Gee.ArrayQueue<Variant> ();

		private Gee.Map<uint, Fruity.CsSignature> target_pids = new Gee.HashMap<uint, Fruity.CsSignature> ();
		private Gee.Set<int32> excluded_syscalls = new Gee.HashSet<int32> ();
		private Gee.Map<uint64?, uint32>? tid_to_pid = null;

		private Gee.Map<uint, uint32> map_gen_by_pid = new Gee.HashMap<uint, uint32> ();

		private Gee.Set<uint> pids_needing_sig_refresh = new Gee.HashSet<uint> ();
		private Gee.Set<uint> pids_with_sig_refresh_in_flight = new Gee.HashSet<uint> ();
		private Gee.Set<uint> pids_dirtied_during_sig_refresh = new Gee.HashSet<uint> ();
		private bool sig_refresh_scheduled = false;

		private Fruity.KperfdataStreamParser kperf = new Fruity.KperfdataStreamParser ();

		private Gee.Map<uint64?, PendingSyscall> pending_syscall_by_tid =
			new Gee.HashMap<uint64?, PendingSyscall> (Numeric.uint64_hash, Numeric.uint64_equal);
		private static Gee.Map<int32, Bytes>? path_arg_indices_by_syscall = null;

		private Gee.Map<uint32, StackInfo> stacks_by_id = new Gee.HashMap<uint32, StackInfo> ();
		private uint32 next_stack_id = 1;

		private Gee.Map<Bytes, Gee.ArrayList<uint32>> stack_ids_by_hash =
			new Gee.HashMap<Bytes, Gee.ArrayList<uint32>> (Numeric.bytes_hash, Numeric.bytes_equal);

		private class PendingSyscall {
			public int32 nr;

			public Fruity.KdBuf buf;

			public uint pid;
			public uint32 map_gen;

			public bool stack_ready = false;
			public int32 stack_id = -1;
			public uint nframes;
			public uint32 async_index;
			public uint32 async_nframes;
			public uint remaining_evts;
			public Array<uint64> frames = new Array<uint64> (false, false);

			public unowned uint8[]? path_arg_indices = null;
			public uint path_cursor = 0;

			public Gee.List<Variant> enter_attachments = new Gee.ArrayList<Variant> ();

			public Array<uint8>? vfs_bytes = null;

			public bool ready_to_emit {
				get {
					if (!stack_ready)
						return false;

					if (path_arg_indices == null)
						return true;
					return path_cursor == path_arg_indices.length;
				}
			}
		}

		private class StackInfo {
			public Array<uint64> frames;
			public uint32 async_index;
			public uint32 async_nframes;

			public StackInfo (Array<uint64> frames, uint32 async_index, uint32 async_nframes) {
				this.frames = frames;
				this.async_index = async_index;
				this.async_nframes = async_nframes;
			}
		}

		private const size_t MAX_BATCH_BYTES = 4U * 1024U * 1024U;

		private const uint64 FNV1A64_OFFSET = 1469598103934665603ULL;
		private const uint64 FNV1A64_PRIME  = 1099511628211ULL;

		public FruitySyscallTraceServiceSession (Fruity.CoreProfileService core_profile, Fruity.DeviceInfoService info_service) {
			Object (core_profile: core_profile, info_service: info_service);
		}

		construct {
			main_context = MainContext.ref_thread_default ();

			try {
				pool = new ThreadPool<Bytes>.with_owned_data (handle_kperfdata, 1, false);
			} catch (ThreadError e) {
				assert_not_reached ();
			}

			build_path_arg_index_table ();

			core_profile.stackshot.connect (on_stackshot);
			core_profile.kperfdata.connect (on_kperfdata);
		}

		public async void activate (Cancellable? cancellable) throws Error, IOError {
		}

		public async void cancel (Cancellable? cancellable) throws IOError {
			try {
				yield core_profile.stop (cancellable);
			} catch (Error e) {
			}
		}

		public async Variant request (Variant parameters, Cancellable? cancellable = null) throws Error, IOError {
			var reader = new VariantReader (parameters);

			string type = reader.read_member ("type").get_string_value ();
			reader.end_member ();

			var reply = new VariantBuilder (VariantType.VARDICT);

			if (type == "read-events") {
				var events = new VariantBuilder (new VariantType ("av"));
				var processes = new VariantBuilder (new VariantType ("a(us)"));

				size_t total = 0;
				var seen_pids = new Gee.HashSet<uint> ();
				bool more = true;
				do {
					Variant? ev;
					{
						var l = new MutexLocker (mutex);
						ev = pending_events.poll ();
						l.free ();
					}
					if (ev == null) {
						more = false;
						break;
					}
					events.add_value (new Variant.variant (ev));
					total += ev.get_size ();
					seen_pids.add (ev.get_child_value (3).get_uint32 ());
				} while (total < MAX_BATCH_BYTES);

				foreach (var pid in seen_pids)
					processes.add ("(us)", pid, "native");

				vardict_add (reply, "events", events.end ());
				vardict_add (reply, "processes", processes.end ());
				vardict_add (reply, "status", more ? "more" : "drained");

				return reply.end ();
			}

			if (type == "resolve-stacks") {
				reader.read_member ("ids");
				var ids = read_uint32_array (reader);
				reader.end_member ();

				var stacks = new VariantBuilder (new VariantType ("aat"));

				foreach (var id in ids) {
					var one = new VariantBuilder (new VariantType ("at"));

					StackInfo? si = stacks_by_id[id];
					if (si != null) {
						foreach (uint64 pc in si.frames)
							one.add_value (pc);
					}

					stacks.add_value (one.end ());
				}

				vardict_add (reply, "stacks", stacks.end ());
				return reply.end ();
			}

			if (type == "resolve-symbols") {
				var pid = (uint) reader.read_member ("pid").get_int64_value ();
				reader.end_member ();

				var gen = (uint32) reader.read_member ("gen").get_int64_value ();
				reader.end_member ();

				reader.read_member ("addresses");
				var addrs = read_uint64_array (reader);
				reader.end_member ();

				var symbols = new VariantBuilder (new VariantType ("a(uu)"));
				var modules = new VariantBuilder (new VariantType ("av"));

				var l = new MutexLocker (mutex);
				Fruity.CsSignature? sig = target_pids[pid];
				if (sig != null) {
					sig.resolve_addresses (gen, addrs,
						(addr, mod_idx, rel32) => {
							symbols.add ("(uu)", mod_idx, rel32);
						},
						owners => {
							var byte_array_type = new VariantType ("ay");
							foreach (var owner in owners) {
								Variant fields[3];
								fields[0] = owner.path;
								fields[1] = new Variant.from_bytes (byte_array_type, owner.uuid, true);
								int num_fields = 2;
								if (owner.version != null)
									fields[num_fields++] = owner.version;
								var tuple = new Variant.tuple (fields[:num_fields]);
								modules.add_value (new Variant.variant (tuple));
							}
						});
					l.free ();
				} else {
					l.free ();
					foreach (var addr in addrs)
						symbols.add ("(uu)", uint32.MAX, 0);
				}

				vardict_add (reply, "modules", modules.end ());
				vardict_add (reply, "symbols", symbols.end ());

				return reply.end ();
			}

			if (type == "read-stats") {
				/* TODO */

				vardict_add (reply, "emitted-events", (uint64) 0);
				vardict_add (reply, "emitted-bytes", (uint64) 0);

				vardict_add (reply, "dropped-events", (uint64) 0);
				vardict_add (reply, "dropped-bytes", (uint64) 0);

				return reply.end ();
			}

			if (type == "add-targets") {
				check_for_unsupported_targets (reader);

				if (reader.has_member ("pids")) {
					reader.read_member ("pids");
					uint32[] pids = read_uint32_array (reader);
					reader.end_member ();

					var pids_to_trace = new Gee.ArrayList<uint32> ();
					pids_to_trace.add_all_array (pids);

					var l = new MutexLocker (mutex);
					if (!target_pids.is_empty) {
						pids_to_trace.add_all (target_pids.keys);
						yield core_profile.stop (cancellable);
					}
					l.free ();

					var config = new Fruity.KtraceConfig ();

					var syscall_codes = new Fruity.KdebugCodeSet ();
					syscall_codes
						.add (Fruity.KdebugCode.from_parts (MACH, Fruity.KdebugMachSubclass.EXCP_SC))
						.add (Fruity.KdebugCode.from_parts (BSD, Fruity.KdebugBsdSubclass.EXCP_SC));

					var all_codes = syscall_codes.copy ();
					all_codes.add (Fruity.KdebugCode.from_parts (TRACE, Fruity.KdebugTraceSubclass.DATA));
					all_codes.add (Fruity.KdebugCode.from_parts (DYLD, Fruity.KdebugDyldSubclass.UUID));
					all_codes.add (Fruity.KdebugCode.from_parts (PERF, Fruity.KdebugPerfSubclass.CALLSTACK));
					all_codes.add (Fruity.KdebugCode.from_parts (FSYSTEM, Fruity.KdebugFsystemSubclass.FSRW));

					var tc = new Fruity.KtraceTapTriggerConfig (KDEBUG);
					tc.filter = all_codes;
					foreach (var pid in pids_to_trace)
						tc.include_pid (pid);
					tc.callstack_frame_depth = 128;
					tc.actions
						.add_baseline ()
						.add_kdebug_codeset (syscall_codes)
						.add_stack_collection (USER);

					config.add_trigger_config (tc);

					yield core_profile.set_config (config, cancellable);

					yield core_profile.start (cancellable);

					l = new MutexLocker (mutex);
					foreach (var pid in pids_to_trace) {
						if (target_pids.has_key (pid))
							continue;

						Fruity.CsSignature sig;
						try {
							sig = yield info_service.query_symbolicator_signature (pid, cancellable);
						} catch (Error e) {
							sig = new Fruity.CsSignature ();
							sig.pid = pid;
						}

						target_pids[pid] = sig;
					}
					l.free ();
				}

				return reply.end ();
			}

			if (type == "remove-targets") {
				check_for_unsupported_targets (reader);

				if (reader.has_member ("pids")) {
					reader.read_member ("pids");
					uint32[] pids = read_uint32_array (reader);
					reader.end_member ();

					var l = new MutexLocker (mutex);
					foreach (var pid in pids) {
						target_pids.unset (pid);
						map_gen_by_pid.unset (pid);
						pids_needing_sig_refresh.remove (pid);
						pids_with_sig_refresh_in_flight.remove (pid);
						pids_dirtied_during_sig_refresh.remove (pid);
					}
					bool no_more_targets = target_pids.is_empty;
					l.free ();

					if (no_more_targets)
						yield core_profile.stop (cancellable);
				}

				return reply.end ();
			}

			if (type == "get-signatures") {
				vardict_add (reply, "native", build_signatures_variant ());

				return reply.end ();
			}

			if (type == "exclude-syscalls") {
				reader.read_member ("native");
				var nrs = read_int32_array (reader);
				reader.end_member ();

				var l = new MutexLocker (mutex);
				excluded_syscalls.add_all_array (nrs);
				l.free ();

				return reply.end ();
			}

			if (type == "include-syscalls") {
				reader.read_member ("native");
				var nrs = read_int32_array (reader);
				reader.end_member ();

				var l = new MutexLocker (mutex);
				excluded_syscalls.remove_all_array (nrs);
				l.free ();

				return reply.end ();
			}

			throw new Error.INVALID_ARGUMENT ("Unsupported request type: %s", type);
		}

		private static void check_for_unsupported_targets (VariantReader reader) throws Error {
			if (reader.has_member ("uids") || reader.has_member ("users"))
				throw new Error.NOT_SUPPORTED ("Target type not supported on this platform");
		}

		private void on_stackshot (Bytes blob) {
			tid_to_pid = new Gee.HashMap<uint64?, uint32> (Numeric.uint64_hash, Numeric.uint64_equal);

			var r = new Fruity.Kcdata.Reader (blob);
			try {
				r.for_each_container (STACKSHOT_CONTAINER_TASK, task => {
					var ts = task.get_first (STACKSHOT_TASK_SNAPSHOT);
					ts.seek (Fruity.Kcdata.TASK_SNAPSHOT_PID_OFFSET);
					var pid = (uint32) ts.read_int32 ();

					task.for_each_container (STACKSHOT_CONTAINER_THREAD, (thread) => {
						var ths = thread.get_first (STACKSHOT_THREAD_SNAPSHOT);
						var tid = ths.read_uint64 ();
						tid_to_pid[tid] = pid;
						return CONTINUE;
					});

					return CONTINUE;
				});
			} catch (Error e) {
				assert_not_reached ();
			}
		}

		private void on_kperfdata (Bytes blob) {
			try {
				pool.add (blob);
			} catch (ThreadError e) {
				assert_not_reached ();
			}
		}

		private void handle_kperfdata (owned Bytes blob) {
			var new_events = new Gee.ArrayList<Variant> ();

			try {
				kperf.push (blob, buf => {
					if (maybe_handle_syscall (buf, new_events))
						return;

					var kc = buf.kcode;
					var klass = kc.klass;
					var subclass = kc.subclass;

					if (klass == PERF && subclass == Fruity.KdebugPerfSubclass.CALLSTACK) {
						var e = (Fruity.KdebugPerfCallstackEvent) kc.code;
						handle_callstack_event (e, buf, new_events);
					} else if (klass == FSYSTEM && subclass == Fruity.KdebugFsystemSubclass.FSRW) {
						var e = (Fruity.KdebugFsystemFsrwEvent) kc.code;
						handle_fsystem_fsrw_event (e, buf, new_events);
					} else if (klass == TRACE && subclass == Fruity.KdebugTraceSubclass.DATA) {
						var e = (Fruity.KdebugTraceDataEvent) kc.code;
						handle_trace_data_event (e, buf, new_events);
					} else if (klass == DYLD && subclass == Fruity.KdebugDyldSubclass.UUID) {
						var e = (Fruity.KdebugDyldUuidEvent) kc.code;
						handle_dyld_uuid_event (e, buf);
					}
				});
			} catch (Error e) {
				assert_not_reached ();
			}

			if (new_events.is_empty)
				return;

			bool should_notify;
			{
				var l = new MutexLocker (mutex);
				should_notify = pending_events.is_empty;
				pending_events.add_all (new_events);
				l.free ();
			}

			if (should_notify) {
				var idle = new IdleSource ();
				idle.set_callback (() => {
					var b = new VariantBuilder (VariantType.VARDICT);
					vardict_add (b, "type", "events-available");
					message (b.end ());
					return Source.REMOVE;
				});
				idle.attach (main_context);
			}
		}

		private bool maybe_handle_syscall (Fruity.KdBuf buf, Gee.List<Variant> new_events) {
			if (!is_syscall (buf))
				return false;

			uint64 tid = buf.arg5;
			uint pid = tid_to_pid[tid];

			var syscall_nr = (int32) buf.kcode.code;
			if (buf.kcode.klass == MACH)
				syscall_nr = -syscall_nr;

			uint32 map_gen;
			{
				var l = new MutexLocker (mutex);
				if (!target_pids.has_key (pid) || excluded_syscalls.contains (syscall_nr))
					return true;
				map_gen = get_map_gen_unlocked (pid);
				l.free ();
			}

			maybe_complete_pending_syscall (tid, new_events);

			pending_syscall_by_tid[tid] = new PendingSyscall () {
				nr = syscall_nr,
				buf = buf,
				pid = pid,
				map_gen = map_gen,
				path_arg_indices = get_path_arg_indices_for_syscall_nr (syscall_nr),
			};

			return true;
		}

		private void maybe_emit_ready_syscall (uint64 tid, PendingSyscall sc, Gee.List<Variant> new_events) {
			if (!sc.ready_to_emit)
				return;
			pending_syscall_by_tid.unset (tid);
			new_events.add (make_syscall_event (sc));
		}

		private void maybe_complete_pending_syscall (uint64 tid, Gee.List<Variant> new_events) {
			PendingSyscall? sc;
			if (pending_syscall_by_tid.unset (tid, out sc))
				new_events.add (make_syscall_event (sc));
		}

		private static bool is_syscall (Fruity.KdBuf buf) {
			var kc = buf.kcode;
			var klass = kc.klass;
			var subclass = kc.subclass;
			return (klass == MACH && subclass == Fruity.KdebugMachSubclass.EXCP_SC) ||
				(klass == BSD && subclass == Fruity.KdebugBsdSubclass.EXCP_SC);
		}

		private static Variant make_syscall_event (PendingSyscall sc) {
			Fruity.KdBuf buf = sc.buf;
			uint64 tid = buf.arg5;

			var attachments = new VariantBuilder (new VariantType ("a(uv)"));

			if (buf.kcode.func_qual == START) {
				if (sc.enter_attachments != null) {
					foreach (var a in sc.enter_attachments)
						attachments.add_value (a);
				}

				var ab = new VariantBuilder (new VariantType ("at"));
				ab.add_value (buf.arg1);
				ab.add_value (buf.arg2);
				ab.add_value (buf.arg3);
				ab.add_value (buf.arg4);

				return new Variant.tuple ({
					"enter",
					buf.timestamp,
					tid,
					sc.pid,
					sc.nr,
					sc.stack_id,
					sc.map_gen,
					ab.end (),
					attachments.end ()
				});
			} else {
				int64 retval;
				int error = (int) buf.arg1;
				if (error != 0)
					retval = error;
				else
					retval = (int64) (((uint64) buf.arg3 << 32) | (uint64) buf.arg2);

				return new Variant.tuple ({
					"exit",
					buf.timestamp,
					tid,
					sc.pid,
					sc.nr,
					sc.stack_id,
					sc.map_gen,
					retval,
					attachments.end ()
				});
			}
		}

		private void handle_callstack_event (Fruity.KdebugPerfCallstackEvent e, Fruity.KdBuf buf, Gee.List<Variant> new_events) {
			switch (e) {
				case UHDR: {
					uint64 tid = buf.arg5;

					PendingSyscall? sc = pending_syscall_by_tid[tid];
					if (sc == null)
						break;

					uint32 sync_nframes = (uint32) buf.arg2;
					uint32 async_index = (uint32) buf.arg3;
					uint32 async_nframes = (uint32) buf.arg4;

					uint32 nframes = sync_nframes + async_nframes;
					uint nevts = (nframes + 3u) / 4u;

					sc.nframes = nframes;
					sc.async_index = async_index;
					sc.async_nframes = async_nframes;
					sc.remaining_evts = nevts;

					break;
				}
				case UDATA: {
					uint64 tid = buf.arg5;

					PendingSyscall? sc = pending_syscall_by_tid[tid];
					if (sc == null)
						break;

					sc.frames.append_val (buf.arg1);
					uint remaining = sc.nframes - sc.frames.length;
					if (remaining != 0) {
						sc.frames.append_val (buf.arg2);
						remaining--;
					}
					if (remaining != 0) {
						sc.frames.append_val (buf.arg3);
						remaining--;
					}
					if (remaining != 0) {
						sc.frames.append_val (buf.arg4);
						remaining--;
					}

					sc.remaining_evts--;

					if (sc.remaining_evts == 0) {
						sc.stack_id = (int32) intern_stack (sc.frames, sc.async_index, sc.async_nframes);
						sc.stack_ready = true;

						maybe_emit_ready_syscall (tid, sc, new_events);
					}

					break;
				}
				default:
					break;
			}
		}

		private void handle_fsystem_fsrw_event (Fruity.KdebugFsystemFsrwEvent e, Fruity.KdBuf buf, Gee.List<Variant> new_events) {
			if (e != LOOKUP)
				return;

			uint64 tid = buf.arg5;

			var l = new MutexLocker (mutex);

			PendingSyscall? sc = pending_syscall_by_tid[tid];
			if (sc == null || sc.path_arg_indices == null)
				return;

			var kc = buf.kcode;
			bool is_start = kc.func_qual == START;
			bool is_end = kc.func_qual == END;

			if (sc.vfs_bytes == null)
				sc.vfs_bytes = new Array<uint8> (false, false);

			if (is_start) {
				append_u64_le (sc.vfs_bytes, buf.arg2);
				append_u64_le (sc.vfs_bytes, buf.arg3);
				append_u64_le (sc.vfs_bytes, buf.arg4);
			} else {
				append_u64_le (sc.vfs_bytes, buf.arg1);
				append_u64_le (sc.vfs_bytes, buf.arg2);
				append_u64_le (sc.vfs_bytes, buf.arg3);
				append_u64_le (sc.vfs_bytes, buf.arg4);
			}

			if (!is_end)
				return;

			string path = decode_c_string (sc.vfs_bytes);
			sc.vfs_bytes = null;

			if (sc.path_cursor != sc.path_arg_indices.length) {
				uint32 arg_index = sc.path_arg_indices[sc.path_cursor++];
				sc.enter_attachments.add (new Variant.tuple ({ arg_index, new Variant.variant (path) }));

				maybe_emit_ready_syscall (tid, sc, new_events);
			}

			l.free ();
		}

		private void handle_trace_data_event (Fruity.KdebugTraceDataEvent e, Fruity.KdBuf buf, Gee.List<Variant> new_events) {
			switch (e) {
				case NEWTHREAD: {
					uint64 tid = buf.arg1;
					uint32 pid = (uint32) buf.arg2;
					bool is_exec = buf.arg3 == 1;

					if (is_exec)
						remove_all_tids_for_pid (pid, new_events);

					tid_to_pid[tid] = pid;
					break;
				}
				case EXEC: {
					uint32 pid = (uint32) buf.arg1;
					uint64 tid = buf.arg5;

					remove_all_tids_for_pid (pid, new_events);
					tid_to_pid[tid] = pid;

					break;
				}
				case THREAD_TERMINATE: {
					uint64 tid = buf.arg1;

					remove_tid (tid, new_events);
					break;
				}
				case THREAD_TERMINATE_PID: {
					uint64 tid = buf.arg5;

					remove_tid (tid, new_events);
					break;
				}
				default:
					assert_not_reached ();
			}
		}

		private void remove_all_tids_for_pid (uint32 pid, Gee.List<Variant> new_events) {
			var dead = new Gee.ArrayList<uint64?> ();
			foreach (var e in tid_to_pid.entries) {
				if (e.value == pid)
					dead.add (e.key);
			}

			foreach (var tid in dead)
				remove_tid (tid, new_events);
		}

		private void remove_tid (uint64 tid, Gee.List<Variant> new_events) {
			tid_to_pid.unset (tid);

			maybe_complete_pending_syscall (tid, new_events);
		}

		private void handle_dyld_uuid_event (Fruity.KdebugDyldUuidEvent e, Fruity.KdBuf buf) {
			var l = new MutexLocker (mutex);

			uint64 tid = buf.arg5;
			uint pid = tid_to_pid[tid];

			Fruity.CsSignature? sig = target_pids[pid];
			if (sig == null)
				return;

			bool in_flight = pids_with_sig_refresh_in_flight.contains (pid);

			switch (e) {
				case MAP_A:
				case SHARED_CACHE_A:
				case AOT_MAP_A: {
					if (!in_flight)
						bump_map_gen_unlocked (pid);
					else
						pids_dirtied_during_sig_refresh.add (pid);

					schedule_signature_refresh_unlocked (pid);

					break;
				}
				case UNMAP_A: {
					Bytes uuid = uuid_from_u64s (buf.arg1, buf.arg2);

					uint32 gen;
					if (!in_flight) {
						gen = bump_map_gen_unlocked (pid);
					} else {
						gen = get_map_gen_unlocked (pid);
						pids_dirtied_during_sig_refresh.add (pid);
					}

					sig.note_unmapped_uuid (uuid, gen);

					schedule_signature_refresh_unlocked (pid);

					break;
				}
				default:
					break;
			}

			l.free ();
		}

		private uint32 get_map_gen_unlocked (uint pid) {
			uint32 gen = map_gen_by_pid[pid];
			if (gen != 0)
				return gen;

			map_gen_by_pid[pid] = 1;
			return 1;
		}

		private uint32 bump_map_gen_unlocked (uint pid) {
			uint32 gen = get_map_gen_unlocked (pid) + 1;
			map_gen_by_pid[pid] = gen;
			return gen;
		}

		private void schedule_signature_refresh_unlocked (uint pid) {
			pids_needing_sig_refresh.add (pid);

			if (sig_refresh_scheduled)
				return;

			sig_refresh_scheduled = true;

			var idle = new IdleSource ();
			idle.set_callback (() => {
				var l = new MutexLocker (mutex);

				sig_refresh_scheduled = false;

				var pids = new Gee.ArrayList<uint> ();
				pids.add_all (pids_needing_sig_refresh);
				pids_needing_sig_refresh.clear ();

				l.free ();

				refresh_signatures.begin (pids);

				return Source.REMOVE;
			});
			idle.attach (main_context);
		}

		private async void refresh_signatures (Gee.List<uint> pids) {
			foreach (var pid in pids) {
				{
					var l = new MutexLocker (mutex);

					if (!target_pids.has_key (pid))
						continue;

					pids_with_sig_refresh_in_flight.add (pid);

					l.free ();
				}

				Fruity.CsSignature? fresh = null;

				try {
					fresh = yield info_service.query_symbolicator_signature (pid, io_cancellable);
				} catch (GLib.Error e) {
				}

				bool should_rerun = false;

				{
					var l = new MutexLocker (mutex);

					pids_with_sig_refresh_in_flight.remove (pid);

					Fruity.CsSignature? current = target_pids[pid];

					if (current != null && fresh != null) {
						uint32 gen = get_map_gen_unlocked (pid);
						current.apply_refresh (fresh, gen);
					}

					if (pids_dirtied_during_sig_refresh.remove (pid)) {
						bump_map_gen_unlocked (pid);
						should_rerun = current != null;
					}

					l.free ();
				}

				if (should_rerun) {
					var l = new MutexLocker (mutex);
					schedule_signature_refresh_unlocked (pid);
					l.free ();
				}
			}
		}

		private static Variant build_signatures_variant () {
			var result = new VariantBuilder (new VariantType ("a(isa(ss))"));

			foreach (unowned XnuSyscallSignature sig in get_xnu_mach_traps ())
				result.add_value (build_signature_variant (sig));
			foreach (unowned XnuSyscallSignature sig in get_xnu_bsd_syscalls ())
				result.add_value (build_signature_variant (sig));

			return result.end ();
		}

		private static Variant build_signature_variant (XnuSyscallSignature sig) {
			var args = new VariantBuilder (new VariantType ("a(ss)"));
			for (uint8 i = 0; i != sig.nargs; i++) {
				unowned XnuSyscallArg arg = sig.args[i];
				args.add_value (new Variant.tuple ({ arg.type, arg.name }));
			}
			return new Variant.tuple ({ sig.nr, sig.name, args.end () });
		}

		private static int32[] read_int32_array (VariantReader reader) throws Error {
			uint n = reader.count_elements ();
			var arr = new int32[n];

			for (uint i = 0; i != n; i++) {
				int64 v = reader.read_element (i).get_int64_value ();
				if (v < int32.MIN || v > int32.MAX)
					throw new Error.INVALID_ARGUMENT ("Value is out of range");

				arr[i] = (int32) v;
				reader.end_element ();
			}

			return arr;
		}

		private static uint32[] read_uint32_array (VariantReader reader) throws Error {
			uint n = reader.count_elements ();
			var arr = new uint32[n];

			for (uint i = 0; i != n; i++) {
				int64 v = reader.read_element (i).get_int64_value ();
				if (v < 0 || v > uint32.MAX)
					throw new Error.INVALID_ARGUMENT ("Value is out of range");

				arr[i] = (uint32) v;
				reader.end_element ();
			}

			return arr;
		}

		private static uint64[] read_uint64_array (VariantReader reader) throws Error {
			uint n = reader.count_elements ();
			var arr = new uint64[n];

			for (uint i = 0; i != n; i++) {
				arr[i] = reader.read_element (i).get_uint64_value ();
				reader.end_element ();
			}

			return arr;
		}

		private static void vardict_add (VariantBuilder b, string key, Variant val) {
			b.add_value (new Variant.dict_entry (new Variant.string (key), new Variant.variant (val)));
		}

		private uint32 intern_stack (Array<uint64> frames, uint32 async_index, uint32 async_nframes) {
			uint64 h64 = hash_stack_fnv1a (frames, async_index, async_nframes);
			Bytes key = u64_to_bytes_key (h64);

			Gee.ArrayList<uint32>? candidates = stack_ids_by_hash[key];
			if (candidates != null) {
				foreach (uint32 id in candidates) {
					StackInfo? existing = stacks_by_id[id];
					if (existing != null && stack_equal (existing, frames, async_index, async_nframes))
						return id;
				}
			}

			uint32 id = next_stack_id++;
			stacks_by_id[id] = new StackInfo (frames, async_index, async_nframes);

			if (candidates == null) {
				candidates = new Gee.ArrayList<uint32> ();
				stack_ids_by_hash[key] = candidates;
			}
			candidates.add (id);

			return id;
		}

		private static bool stack_equal (StackInfo existing, Array<uint64> frames, uint32 async_index, uint32 async_nframes) {
			if (existing.async_index != async_index || existing.async_nframes != async_nframes)
				return false;

			if (existing.frames.length != frames.length)
				return false;

			for (uint i = 0; i != frames.length; i++) {
				if (existing.frames.index (i) != frames.index (i))
					return false;
			}

			return true;
		}

		private static uint64 hash_stack_fnv1a (Array<uint64> frames, uint32 async_index, uint32 async_nframes) {
			uint64 h = FNV1A64_OFFSET;

			for (uint i = 0; i != frames.length; i++) {
				uint64 v = frames.index (i);
				for (uint shift = 0; shift != 64; shift += 8)
					h = fnv1a64_update_byte (h, (uint8) ((v >> shift) & 0xff));
			}

			for (uint shift = 0; shift != 32; shift += 8)
				h = fnv1a64_update_byte (h, (uint8) ((async_index >> shift) & 0xff));

			for (uint shift = 0; shift != 32; shift += 8)
				h = fnv1a64_update_byte (h, (uint8) ((async_nframes >> shift) & 0xff));

			uint32 len = frames.length;
			for (uint shift = 0; shift != 32; shift += 8)
				h = fnv1a64_update_byte (h, (uint8) ((len >> shift) & 0xff));

			return h;
		}

		private static inline uint64 fnv1a64_update_byte (uint64 h, uint8 b) {
			h ^= b;
			h *= FNV1A64_PRIME;
			return h;
		}

		private static Bytes u64_to_bytes_key (uint64 v) {
			var b = new uint8[8];
			for (uint i = 0; i != 8; i++)
				b[i] = (uint8) ((v >> (i * 8)) & 0xff);
			return new Bytes.take ((owned) b);
		}

		private static Bytes uuid_from_u64s (uint64 a, uint64 b) {
			var bytes = new uint8[16];

			for (uint i = 0; i != 8; i++)
				bytes[i] = (uint8) ((a >> (i * 8)) & 0xff);

			for (uint i = 0; i != 8; i++)
				bytes[8 + i] = (uint8) ((b >> (i * 8)) & 0xff);

			return new Bytes.take ((owned) bytes);
		}

		private static void append_u64_le (Array<uint8> arr, uint64 v) {
			for (uint i = 0; i != 8; i++) {
				var b = (uint8) ((v >> (i * 8)) & 0xff);
				arr.append_val (b);
			}
		}

		private static string decode_c_string (Array<uint8> data) {
			uint n = data.length;
			uint end = n;
			for (uint i = 0; i != n; i++) {
				if (data.index (i) == 0) {
					end = i;
					break;
				}
			}

			var b = new uint8[end + 1];
			Memory.copy (b, data.data, end);

			return (string) b;
		}

		private static void build_path_arg_index_table () {
			var map = new Gee.HashMap<int32, Bytes> ();

			foreach (unowned XnuSyscallSignature sig in get_xnu_mach_traps ())
				add_path_indices_for_sig (map, sig);

			foreach (unowned XnuSyscallSignature sig in get_xnu_bsd_syscalls ())
				add_path_indices_for_sig (map, sig);

			path_arg_indices_by_syscall = map;
		}

		private static void add_path_indices_for_sig (Gee.Map<int32, Bytes> indices, XnuSyscallSignature sig) {
			var arr = new uint8[0];

			for (uint8 i = 0; i != sig.nargs; i++) {
				unowned XnuSyscallArg a = sig.args[i];
				if (arg_is_filesystem_path (a.type, a.name))
					arr += i;
			}

			if (arr.length == 0)
				return;

			indices[sig.nr] = new Bytes.take ((owned) arr);
		}

		private static bool arg_is_filesystem_path (string type, string name) {
			if (!(type == "char *" || type == "const char *"))
				return false;

			return (
				name == "attrname" ||
				name == "file_path" ||
				name == "fname" ||
				name == "from" ||
				name == "mountdir" ||
				name == "new_rootfs_path_before" ||
				name == "old_rootfs_path_after" ||
				name == "path" ||
				name == "path1" ||
				name == "path2" ||
				name == "path_p" ||
				name == "to"
			);
		}

		private static unowned uint8[]? get_path_arg_indices_for_syscall_nr (int32 nr) {
			Bytes? indices = path_arg_indices_by_syscall[nr];
			if (indices == null)
				return null;
			return indices.get_data ();
		}
	}
}
