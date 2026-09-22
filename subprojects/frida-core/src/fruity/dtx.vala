[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class DeviceInfoService : Object, AsyncInitable {
		public HostChannelProvider channel_provider {
			get;
			construct;
		}

		private DTXChannel channel;

		private DeviceInfoService (HostChannelProvider channel_provider) {
			Object (channel_provider: channel_provider);
		}

		public static async DeviceInfoService open (HostChannelProvider channel_provider, Cancellable? cancellable = null)
				throws Error, IOError {
			var service = new DeviceInfoService (channel_provider);

			try {
				yield service.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return service;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var connection = yield DTXConnection.obtain (channel_provider, cancellable);

			channel = connection.make_channel ("com.apple.instruments.server.services.deviceinfo");

			return true;
		}

		public async Gee.List<ProcessInfo> enumerate_processes (Cancellable? cancellable = null) throws Error, IOError {
			var result = new Gee.ArrayList<ProcessInfo> ();

			var response = yield channel.invoke ("runningProcesses", null, cancellable);

			NSArray? processes = response as NSArray;
			if (processes == null)
				throw new Error.PROTOCOL ("Malformed response");

			foreach (var element in processes.elements) {
				NSDictionary? process = element as NSDictionary;
				if (process == null)
					throw new Error.PROTOCOL ("Malformed response");

				var info = new ProcessInfo ();

				info.pid = (uint) process.get_value<NSNumber> ("pid").integer;

				info.name = process.get_value<NSString> ("name").str;
				info.real_app_name = resolve_real_app_name (process.get_value<NSString> ("realAppName").str);
				info.is_application = process.get_value<NSNumber> ("isApplication").boolean;

				NSNumber? foreground_running;
				if (process.get_optional_value ("foregroundRunning", out foreground_running))
					info.foreground_running = foreground_running.boolean;

				NSDate? start_date;
				if (process.get_optional_value ("startDate", out start_date))
					info.start_date = start_date.to_date_time ();

				result.add (info);
			}

			return result;
		}

		public async CsSignature query_symbolicator_signature (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSNumber.from_integer (pid))
				.append_object (new NSString (""));
			var response = yield channel.invoke ("symbolicatorSignatureForPid:trackingSelector:", args, cancellable);

			NSData? data = response as NSData;
			if (data == null)
				throw new Error.PROTOCOL ("Malformed response");

			var parser = new CsSignatureParser (data.bytes);
			return parser.parse ();
		}

		private static string resolve_real_app_name (string name) {
			if (name.has_prefix ("/var/"))
				return "/private" + name;
			return name;
		}

		public sealed class ProcessInfo : Object {
			public uint pid {
				get;
				set;
			}

			public string name {
				get;
				set;
			}

			public string real_app_name {
				get;
				set;
			}

			public bool is_application {
				get;
				set;
			}

			public bool foreground_running {
				get;
				set;
			}

			public DateTime? start_date {
				get;
				set;
			}
		}
	}

	public sealed class ApplicationListingService : Object, AsyncInitable {
		public HostChannelProvider channel_provider {
			get;
			construct;
		}

		private DTXChannel channel;

		private ApplicationListingService (HostChannelProvider channel_provider) {
			Object (channel_provider: channel_provider);
		}

		public static async ApplicationListingService open (HostChannelProvider channel_provider, Cancellable? cancellable = null)
				throws Error, IOError {
			var service = new ApplicationListingService (channel_provider);

			try {
				yield service.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return service;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var connection = yield DTXConnection.obtain (channel_provider, cancellable);

			channel = connection.make_channel ("com.apple.instruments.server.services.device.applictionListing");

			return true;
		}

		public async Gee.List<ApplicationInfo> enumerate_applications (NSDictionary? query = null,
				Cancellable? cancellable = null) throws Error, IOError {
			var result = new Gee.ArrayList<ApplicationInfo> ();

			var args = new DTXArgumentListBuilder ()
				.append_object ((query != null) ? query : new NSDictionary ())
				.append_object (new NSString (""));
			var response = yield channel.invoke ("installedApplicationsMatching:registerUpdateToken:", args, cancellable);

			NSArray? apps = response as NSArray;
			if (apps == null)
				throw new Error.PROTOCOL ("Malformed response");

			foreach (var element in apps.elements) {
				NSDictionary? app = element as NSDictionary;
				if (app == null)
					throw new Error.PROTOCOL ("Malformed response");

				var info = new ApplicationInfo ();

				info.app_type = ApplicationType.from_dtx (app.get_value<NSString> ("Type").str);
				info.display_name = app.get_value<NSString> ("DisplayName").str;
				info.bundle_identifier = app.get_value<NSString> ("CFBundleIdentifier").str;
				info.bundle_path = app.get_value<NSString> ("BundlePath").str;
				info.restricted = app.get_value<NSNumber> ("Restricted").boolean;

				NSNumber num_val;
				NSString? str_val;
				NSArray arr_val;

				if (app.get_optional_value<NSString> ("Version", out str_val))
					info.version = str_val.str;

				if (app.get_optional_value<NSNumber> ("Placeholder", out num_val))
					info.placeholder = num_val.boolean;

				if (app.get_optional_value<NSString> ("ExecutableName", out str_val))
					info.executable_name = str_val.str;

				if (app.get_optional_value<NSArray> ("AppExtensionUUIDs", out arr_val)) {
					var uuids = new string[arr_val.length];
					uint i = 0;
					foreach (var uuid in arr_val.elements) {
						str_val = uuid as NSString;
						if (str_val == null)
							throw new Error.PROTOCOL ("Malformed response");
						uuids[i] = str_val.str;
						i++;
					}
					info.app_extension_uuids = uuids;
				}

				if (app.get_optional_value<NSString> ("PluginUUID", out str_val))
					info.plugin_uuid = str_val.str;

				if (app.get_optional_value<NSString> ("PluginIdentifier", out str_val))
					info.plugin_identifier = str_val.str;

				if (app.get_optional_value<NSString> ("ContainerBundleIdentifier", out str_val))
					info.container_bundle_identifier = str_val.str;

				if (app.get_optional_value<NSString> ("ContainerBundlePath", out str_val))
					info.container_bundle_path = str_val.str;

				result.add (info);
			}

			return result;
		}

		public sealed class ApplicationInfo : Object {
			public ApplicationType app_type {
				get;
				set;
			}

			public string display_name {
				get;
				set;
			}

			public string bundle_identifier {
				get;
				set;
			}

			public string bundle_path {
				get;
				set;
			}

			public string? version {
				get;
				set;
			}

			public bool placeholder {
				get;
				set;
			}

			public bool restricted {
				get;
				set;
			}

			public string? executable_name {
				get;
				set;
			}

			public string[]? app_extension_uuids {
				get;
				set;
			}

			public string? plugin_uuid {
				get;
				set;
			}

			public string? plugin_identifier {
				get;
				set;
			}

			public string? container_bundle_identifier {
				get;
				set;
			}

			public string? container_bundle_path {
				get;
				set;
			}
		}

		public enum ApplicationType {
			SYSTEM = 1,
			USER,
			PLUGIN_KIT;

			public static ApplicationType from_nick (string nick) throws Error {
				return Marshal.enum_from_nick<ApplicationType> (nick);
			}

			public string to_nick () {
				return Marshal.enum_to_nick<ApplicationType> (this);
			}

			internal static ApplicationType from_dtx (string type) {
				if (type == "System")
					return SYSTEM;

				if (type == "User")
					return USER;

				if (type == "PluginKit")
					return PLUGIN_KIT;

				assert_not_reached ();
			}
		}
	}

	public sealed class ProcessControlService : Object, AsyncInitable {
		public signal void process_terminated (uint pid, int exit_code, int crashing_signal);

		public HostChannelProvider channel_provider {
			get;
			construct;
		}

		private DTXChannel channel;

		private ProcessControlService (HostChannelProvider channel_provider) {
			Object (channel_provider: channel_provider);
		}

		public static async ProcessControlService open (HostChannelProvider channel_provider, Cancellable? cancellable = null)
				throws Error, IOError {
			var service = new ProcessControlService (channel_provider);

			try {
				yield service.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return service;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var connection = yield DTXConnection.obtain (channel_provider, cancellable);

			channel = connection.make_channel ("com.apple.instruments.server.services.processcontrol");
			channel.invocation.connect (on_invocation);

			return true;
		}

		public async uint process_identifier_for_bundle_identifier (string bundle_id, Cancellable? cancellable = null)
				throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSString (bundle_id));
			var response = yield channel.invoke ("processIdentifierForBundleIdentifier:", args, cancellable);

			NSNumber? pid = response as NSNumber;
			if (pid == null)
				throw new Error.PROTOCOL ("Malformed response");

			return (uint) pid.integer;
		}

		public async bool is_pid_debuggable (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSNumber.from_integer (pid));
			var response = yield channel.invoke ("isPidDebuggable:", args, cancellable);

			NSNumber? is_debuggable = response as NSNumber;
			if (is_debuggable == null)
				throw new Error.PROTOCOL ("Malformed response");

			return is_debuggable.boolean;
		}

		public async void start_observing_pid (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSNumber.from_integer (pid));
			yield channel.invoke ("startObservingPid:", args, cancellable);
		}

		public async void stop_observing_pid (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSNumber.from_integer (pid));
			yield channel.invoke ("stopObservingPid:", args, cancellable);
		}

		public async uint launch (string bundle_id, LaunchOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var opts = options;
			if (opts == null)
				opts = new LaunchOptions ();

			var args_val = new NSArray ();
			foreach (unowned string arg in opts.arguments)
				args_val.add_object (new NSString (arg));

			var env_val = new NSDictionary ();
			foreach (unowned string entry in opts.environment) {
				string[] tokens = entry.split ("=", 2);
				if (tokens.length != 2)
					throw new Error.INVALID_ARGUMENT ("Invalid environment variable entry: %s", entry);
				env_val.set_value (tokens[0], new NSString (tokens[1]));
			}

			var opts_val = new NSDictionary ();
			opts_val.set_value ("StartSuspendedKey", new NSNumber.from_boolean (opts.launch_mode == SUSPENDED));
			opts_val.set_value ("KillExisting", new NSNumber.from_boolean (opts.existing_instance_policy == KILL));
			// TODO: Support CaptureOutput + iODestinationKey

			var dtx_args = new DTXArgumentListBuilder ()
				.append_object (new NSString (""))
				.append_object (new NSString (bundle_id))
				.append_object (env_val)
				.append_object (args_val)
				.append_object (opts_val);
			var response = yield channel.invoke (
				"launchSuspendedProcessWithDevicePath:bundleIdentifier:environment:arguments:options:",
				dtx_args, cancellable);

			NSNumber? pid = response as NSNumber;
			if (pid == null)
				throw new Error.PROTOCOL ("Malformed response");

			return (uint) pid.integer;
		}

		public async void kill (uint pid, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSNumber.from_integer (pid));
			yield channel.invoke ("killPid:", args, cancellable);
		}

		public async void send_signal (uint pid, uint sig, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (new NSNumber.from_integer (sig))
				.append_object (new NSNumber.from_integer (pid));
			yield channel.invoke ("sendSignal:toPid:", args, cancellable);
		}

		private void on_invocation (string method_name, DTXArgumentList args, DTXMessageTransportFlags transport_flags) {
			if (method_name == "processWithPID:terminatedWithExitCode:orCrashingSignal:") {
				if (args.elements.length != 3)
					return;

				uint pid;
				if (!try_parse_int (args.elements[0], out pid))
					return;

				int exit_code = -1;
				int crashing_signal = -1;
				if (!try_parse_int (args.elements[1], out exit_code)) {
					if (!try_parse_int (args.elements[2], out crashing_signal))
						return;
				}

				process_terminated (pid, exit_code, crashing_signal);
			}
		}

		private bool try_parse_int (Value v, out int i) {
			i = -1;
			if (!v.holds (typeof (NSNumber)))
				return false;
			i = (int) ((NSNumber) v.get_object ()).integer;
			return true;
		}
	}

	public sealed class LaunchOptions : Object {
		public string[] arguments {
			get;
			set;
			default = {};
		}

		public string[] environment {
			get;
			set;
			default = {};
		}

		public LaunchMode launch_mode {
			get;
			set;
			default = LaunchMode.NORMAL;
		}

		public ExistingInstancePolicy existing_instance_policy {
			get;
			set;
			default = ExistingInstancePolicy.KEEP;
		}
	}

	public enum LaunchMode {
		NORMAL,
		SUSPENDED
	}

	public enum ExistingInstancePolicy {
		KEEP,
		KILL
	}

	public sealed class CoreProfileService : Object, AsyncInitable {
		public signal void stackshot (Bytes blob);
		public signal void kperfdata (Bytes blob);

		public HostChannelProvider channel_provider {
			get;
			construct;
		}

		private DTXChannel channel;

		private CoreProfileService (HostChannelProvider channel_provider) {
			Object (channel_provider: channel_provider);
		}

		public static async CoreProfileService open (HostChannelProvider channel_provider, Cancellable? cancellable = null)
				throws Error, IOError {
			var service = new CoreProfileService (channel_provider);

			try {
				yield service.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return service;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var connection = yield DTXConnection.obtain (channel_provider, cancellable);

			channel = connection.make_channel ("com.apple.instruments.server.services.coreprofilesessiontap");
			channel.notification.connect (on_notification);
			channel.data.connect (on_data);

			return true;
		}

		public async void set_config (KtraceConfig config, Cancellable? cancellable = null) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_object (config.encode ());
			yield channel.invoke ("setConfig:", args, cancellable);
		}

		public async void start (Cancellable? cancellable = null) throws Error, IOError {
			yield channel.invoke ("start", null, cancellable);
		}

		public async void stop (Cancellable? cancellable = null) throws Error, IOError {
			yield channel.invoke ("stop", null, cancellable);
		}

		private void on_notification (NSObject obj) {
			// printerr ("on_notification(): %s\n", obj.to_string ());
		}

		private void on_data (Bytes blob) {
			if (Kcdata.is_stackshot (blob))
				stackshot (blob);
			else
				kperfdata (blob);
		}
	}

	public abstract class TapConfig : Object {
		public TapRecordingMode buffer_mode {
			get {
				return _buffer_mode;
			}
			set {
				_buffer_mode = value;
				if (value != POLLED)
					_polling_interval = 0;
			}
		}

		public uint64 polling_interval {
			get;
			set;
			default = 500;
		}

		public uint64 window_size {
			get;
			set;
			default = 0;
		}

		public bool spool_to_disk_when_possible {
			get;
			set;
			default = false;
		}

		public bool discard_heartbeats_when_possible {
			get;
			set;
			default = false;
		}

		private TapRecordingMode _buffer_mode = POLLED;

		internal virtual NSDictionary encode () {
			var dict = new NSDictionary ();

			dict.set_value ("bm", new NSNumber.from_integer (_buffer_mode));

			if (_polling_interval != 0)
				dict.set_value ("ur", new NSNumber.from_integer ((int64) _polling_interval));

			if (_window_size != 0)
				dict.set_value ("ws", new NSNumber.from_integer ((int64) _window_size));

			if (_spool_to_disk_when_possible)
				dict.set_value ("s2d", new NSNumber.from_boolean (true));

			if (_discard_heartbeats_when_possible)
				dict.set_value ("nohb", new NSNumber.from_boolean (true));

			return dict;
		}
	}

	public enum TapRecordingMode {
		POLLED,
		IMMEDIATE,
		WINDOWED,
	}

	public sealed class KtraceConfig : TapConfig {
		public bool can_use_raw_ktrace_file {
			get;
			set;
			default = false;
		}

		public KtraceRecordingPriority recording_priority {
			get;
			set;
			default = FOREGROUND;
		}

		public uint collection_interval {
			get;
			set;
			default = 0;
		}

		public uint64 buffer_size_override {
			get;
			set;
			default = 0;
		}

		public uint64 buffer_size_override_clamping {
			get;
			set;
			default = 0;
		}

		public NSDictionary? provider_options {
			get;
			set;
			default = null;
		}

		private Gee.List<KtraceTapTriggerConfig> trigger_configs = new Gee.ArrayList<KtraceTapTriggerConfig> ();

		public void add_trigger_config (KtraceTapTriggerConfig tc) {
			trigger_configs.add (tc);
		}

		internal override NSDictionary encode () {
			var dict = base.encode ();

			if (can_use_raw_ktrace_file)
				dict.set_value ("curkt", new NSNumber.from_boolean (true));

			dict.set_value ("rp", new NSNumber.from_integer (_recording_priority));

			if (_collection_interval != 0)
				dict.set_value ("kco", new NSNumber.from_integer (_collection_interval));

			if (_buffer_size_override != 0)
				dict.set_value ("bso", new NSNumber.from_integer ((int64) _buffer_size_override));

			if (_buffer_size_override_clamping != 0)
				dict.set_value ("bsoc", new NSNumber.from_integer ((int64) _buffer_size_override_clamping));

			if (_provider_options != null)
				dict.set_value ("po", _provider_options);

			var tc = new NSArray ();
			foreach (var cfg in trigger_configs)
				tc.add_object (cfg.encode ());
			dict.set_value ("tc", tc);

			return dict;
		}
	}

	public enum KtraceRecordingPriority {
		BACKGROUND = 10,
		FOREGROUND = 100,
	}

	public sealed class KtraceTapTriggerConfig : Object {
		public KtraceTapTriggerKind kind {
			get;
			construct;
		}

		public KdebugCodeSet? filter {
			get;
			set;
		}

		public bool is_all_processes {
			get {
				return included_pids == null;
			}
			set {
				if (value)
					included_pids = null;
				else
					ensure_included_pids ();
			}
		}

		public uint callstack_frame_depth {
			get;
			set;
			default = 0;
		}

		public KtraceTapActions actions {
			get;
			default = new KtraceTapActions ();
		}

		private Gee.Set<uint>? included_pids = null;

		public KtraceTapTriggerConfig (KtraceTapTriggerKind kind) {
			Object (kind: kind);
		}

		public void include_pid (uint pid) {
			ensure_included_pids ().add (pid);
		}

		private unowned Gee.Set<uint> ensure_included_pids () {
			if (included_pids == null)
				included_pids = new Gee.HashSet<uint> ();
			return included_pids;
		}

		internal NSDictionary encode () {
			var dict = new NSDictionary ();

			dict.set_value ("uuid", new NSString (Uuid.string_random ().up ()));

			dict.set_value ("tk", new NSNumber.from_integer (_kind));

			if (_filter != null) {
				dict.set_value ("kdf", new NSString (_filter.legacy_xml));
				dict.set_value ("kdf2", _filter.kdebug_codes);
			}

			if (included_pids != null) {
				var pf = new NSArray ();
				foreach (var pid in included_pids)
					pf.add_object (new NSNumber.from_integer (pid));
				dict.set_value ("pf", pf);
			}

			if (_callstack_frame_depth != 0)
				dict.set_value ("csd", new NSNumber.from_integer (_callstack_frame_depth));

			var actions = _actions.items;
			if (!actions.is_empty) {
				var arr = new NSArray ();
				foreach (var action in actions)
					arr.add_object (action.encode ());
				dict.set_value ("ta", arr);
			}

			return dict;
		}
	}

	public enum KtraceTapTriggerKind {
		TIME = 1,
		PMI,
		KDEBUG,
	}

	public sealed class KtraceTapActions : Object {
		internal Gee.List<KtraceTapAction> items = new Gee.ArrayList<KtraceTapAction> ();

		public unowned KtraceTapActions add (KtraceTapAction action) {
			items.add (action);
			return this;
		}

		public unowned KtraceTapActions add_stack_collection (KtraceTapStackCollectionMode mode) {
			items.add (new KtraceTapStackCollectionAction (mode));
			return this;
		}

		public unowned KtraceTapActions add_baseline () {
			items.add (new KtraceTapAction0 ());
			items.add (new KtraceTapAction2 ());
			return this;
		}

		public unowned KtraceTapActions add_pmc_event (string event_name, string counter_name, uint32 extra = 0) {
			items.add (new KtraceTapAddPmcEventAction (event_name, counter_name, extra));
			return this;
		}

		public unowned KtraceTapActions add_kdebug_codeset (KdebugCodeSet codeset) {
			items.add (new KtraceTapAddKdebugCodeSetAction (codeset.kdebug_codes));
			return this;
		}

		public unowned KtraceTapActions add_kdebug_legacy_backtrace_filter () {
			items.add (new KtraceTapKdebugBacktraceFilterAction ());
			return this;
		}
	}

	public enum KtraceTapActionKind {
		ACTION0,
		STACK_COLLECTION,
		ACTION2,
		KDEBUG_BACKTRACE_FILTER,
		ADD_PMC_EVENT,
		ADD_KDEBUG_CODESET,
	}

	public abstract class KtraceTapAction : Object {
		public abstract KtraceTapActionKind kind {
			get;
		}

		internal abstract NSArray encode ();
	}

	public sealed class KtraceTapAction0 : KtraceTapAction {
		public override KtraceTapActionKind kind {
			get {
				return ACTION0;
			}
		}

		internal override NSArray encode () {
			var a = new NSArray ();
			a.add_object (new NSNumber.from_integer (kind));
			return a;
		}
	}

	public sealed class KtraceTapAction2 : KtraceTapAction {
		public override KtraceTapActionKind kind {
			get {
				return ACTION2;
			}
		}

		internal override NSArray encode () {
			var a = new NSArray ();
			a.add_object (new NSNumber.from_integer (kind));
			return a;
		}
	}

	public sealed class KtraceTapStackCollectionAction : KtraceTapAction {
		public KtraceTapStackCollectionMode mode {
			get {
				return _mode;
			}
			set {
				_mode = value;
			}
		}

		private KtraceTapStackCollectionMode _mode;

		public KtraceTapStackCollectionAction (KtraceTapStackCollectionMode mode) {
			_mode = mode;
		}

		public override KtraceTapActionKind kind {
			get {
				return STACK_COLLECTION;
			}
		}

		internal override NSArray encode () {
			var a = new NSArray ();
			a.add_object (new NSNumber.from_integer (kind));
			a.add_object (new NSNumber.from_boolean ((_mode & KtraceTapStackCollectionMode.USER) != 0));
			a.add_object (new NSNumber.from_boolean ((_mode & KtraceTapStackCollectionMode.KERNEL) != 0));
			return a;
		}
	}

	[Flags]
	public enum KtraceTapStackCollectionMode {
		NONE   = 0,
		USER   = (1 << 0),
		KERNEL = (1 << 1),
	}

	public sealed class KtraceTapKdebugBacktraceFilterAction : KtraceTapAction {
		public override KtraceTapActionKind kind {
			get {
				return KDEBUG_BACKTRACE_FILTER;
			}
		}

		internal override NSArray encode () {
			var a = new NSArray ();
			a.add_object (new NSNumber.from_integer (kind));
			return a;
		}
	}

	public sealed class KtraceTapAddPmcEventAction : KtraceTapAction {
		public string event_name {
			get {
				return _event_name;
			}
			set {
				_event_name = value;
			}
		}

		public string counter_name {
			get {
				return _counter_name;
			}
			set {
				_counter_name = value;
			}
		}

		public uint32 extra {
			get {
				return _extra;
			}
			set {
				_extra = value;
			}
		}

		private string _event_name;
		private string _counter_name;
		private uint32 _extra;

		public KtraceTapAddPmcEventAction (string event_name, string counter_name, uint32 extra = 0) {
			_event_name = event_name;
			_counter_name = counter_name;
			_extra = extra;
		}

		public override KtraceTapActionKind kind {
			get {
				return ADD_PMC_EVENT;
			}
		}

		internal override NSArray encode () {
			var a = new NSArray ();
			a.add_object (new NSNumber.from_integer (kind));
			a.add_object (new NSString (_event_name));
			a.add_object (new NSString (_counter_name));
			a.add_object (new NSNumber.from_integer (_extra));
			return a;
		}
	}

	public sealed class KtraceTapAddKdebugCodeSetAction : KtraceTapAction {
		public NSSet codes {
			get {
				return _codes;
			}
			set {
				_codes = value;
			}
		}

		private NSSet _codes;

		public KtraceTapAddKdebugCodeSetAction (NSSet codes) {
			_codes = codes;
		}

		public override KtraceTapActionKind kind {
			get {
				return ADD_KDEBUG_CODESET;
			}
		}

		internal override NSArray encode () {
			var a = new NSArray ();
			a.add_object (new NSNumber.from_integer (kind));
			a.add_object (_codes);
			return a;
		}
	}

	public sealed class KdebugCodeSet : Object {
		public string legacy_xml {
			owned get {
				var xml = new StringBuilder.sized (256);
				xml.append ("<events>");

				foreach (var c in codes) {
					var kc = KdebugCode (c);

					xml.append ("<event type=\"KDebug\" class=\"");

					var klass = kc.klass;
					if (klass == ANY)
						xml.append_c ('*');
					else
						xml.append_printf ("%u", klass);

					xml.append ("\" subclass=\"");

					var subclass = kc.subclass;
					if (subclass == KDEBUG_SUBCLASS_ANY)
						xml.append_c ('*');
					else
						xml.append_printf ("%u", subclass);

					xml.append ("\" code=\"");

					uint code = kc.code;
					if (code == KDEBUG_CODE_ANY)
						xml.append_c ('*');
					else
						xml.append_printf ("%u", code);

					xml.append ("\"/>");
				}

				xml.append ("</events>");

				return xml.str;
			}
		}

		public NSSet kdebug_codes {
			get {
				if (_kdebug_codes == null) {
					_kdebug_codes = new NSSet ();
					foreach (var c in codes)
						_kdebug_codes.add_object (new NSNumber.from_integer (c));
				}
				return _kdebug_codes;
			}
		}

		private Gee.Set<uint> codes = new Gee.HashSet<uint> ();
		private NSSet _kdebug_codes = null;

		public KdebugCodeSet copy () {
			var s = new KdebugCodeSet ();
			s.codes.add_all (codes);
			s._kdebug_codes = _kdebug_codes;
			return s;
		}

		public unowned KdebugCodeSet add (KdebugCode code) {
			codes.add (code.raw);
			_kdebug_codes = null;
			return this;
		}
	}

	public sealed class DTXConnection : Object, DTXTransport {
		public IOStream stream {
			get;
			construct;
		}

		public State state {
			get {
				return _state;
			}
		}

		public enum State {
			OPEN,
			CLOSED
		}

		private static Gee.HashMap<HostChannelProvider, Future<DTXConnection>> connections;

		private State _state = OPEN;

		private DataInputStream input;
		private OutputStream output;
		private Cancellable io_cancellable = new Cancellable ();

		private Gee.HashMap<uint32, Gee.ArrayList<Fragment>> fragments = new Gee.HashMap<uint32, Gee.ArrayList<Fragment>> ();
		private uint32 next_fragment_identifier = 1;
		private Gee.ArrayQueue<Bytes> pending_writes = new Gee.ArrayQueue<Bytes> ();

		private DTXControlChannel control_channel;
		private Gee.HashMap<uint32, unowned DTXChannel> channels = new Gee.HashMap<uint32, unowned DTXChannel> ();
		private int32 next_channel_code = 1;

		private const uint32 DTX_FRAGMENT_MAGIC = 0x1f3d5b79U;
		private const size_t MAX_MESSAGE_SIZE = 128 * 1024 * 1024;
		private const size_t MAX_FRAGMENT_SIZE = 128 * 1024;
		private const string REMOTESERVER_ENDPOINT_17PLUS = "lockdown:com.apple.instruments.dtservicehub";
		private const string REMOTESERVER_ENDPOINT_14PLUS = "lockdown:com.apple.instruments.remoteserver.DVTSecureSocketProxy";
		private const string REMOTESERVER_ENDPOINT_LEGACY = "lockdown:com.apple.instruments.remoteserver?tls=handshake-only";
 		private const string[] REMOTESERVER_ENDPOINT_CANDIDATES = {
			REMOTESERVER_ENDPOINT_17PLUS,
			REMOTESERVER_ENDPOINT_14PLUS,
			REMOTESERVER_ENDPOINT_LEGACY,
		};

		public static async DTXConnection obtain (HostChannelProvider channel_provider, Cancellable? cancellable)
				throws Error, IOError {
			if (connections == null)
				connections = new Gee.HashMap<HostChannelProvider, Future<DTXConnection>> ();

			while (connections.has_key (channel_provider)) {
				var future = connections[channel_provider];
				try {
					return yield future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}

			var request = new Promise<DTXConnection> ();
			connections[channel_provider] = request.future;

			Error pending_error = null;

			foreach (unowned string endpoint in REMOTESERVER_ENDPOINT_CANDIDATES) {
				try {
					var stream = yield channel_provider.open_channel (endpoint, cancellable);

					var connection = new DTXConnection (stream);
					connection.notify["state"].connect (on_connection_state_changed);

					request.resolve (connection);

					return connection;
				} catch (Error e) {
					if (!(e is Error.NOT_SUPPORTED)) {
						pending_error = e;
						break;
					}
				}
			}

			Error api_error = (pending_error == null)
				? new Error.NOT_SUPPORTED ("This feature requires an iOS Developer Disk Image to be mounted; " +
					"run Xcode briefly or use ideviceimagemounter to mount one manually")
				: pending_error;

			request.reject (api_error);
			connections.unset (channel_provider);

			throw api_error;
		}

		public static async void close_all (HostChannelProvider channel_provider, Cancellable? cancellable) throws IOError {
			if (connections == null)
				return;

			while (connections.has_key (channel_provider)) {
				var future = connections[channel_provider];
				try {
					var connection = yield future.wait_async (cancellable);
					connections.unset (channel_provider);
					yield connection.close (cancellable);
				} catch (Error e) {
					continue;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}

			if (connections.size == 0)
				connections = null;
		}

		private static void on_connection_state_changed (Object object, ParamSpec pspec) {
			DTXConnection connection = (DTXConnection) object;
			if (connection.state != CLOSED)
				return;

			foreach (var entry in connections.entries) {
				var future = entry.value;

				if (future.ready) {
					DTXConnection c = future.value;
					if (c == connection) {
						connections.unset (entry.key);
						return;
					}
				}
			}
		}

		public DTXConnection (IOStream stream) {
			Object (stream: stream);
		}

		construct {
			input = new DataInputStream (stream.get_input_stream ());
			input.byte_order = LITTLE_ENDIAN;
			output = stream.get_output_stream ();

			control_channel = new DTXControlChannel (this);
			channels[control_channel.code] = control_channel;

			try {
				control_channel.notify_of_published_capabilities ();
			} catch (Error e) {
				assert_not_reached ();
			}

			process_incoming_fragments.begin ();
		}

		public override void dispose () {
			foreach (var channel in channels.values)
				channel.transport = null;
			channels.clear ();

			base.dispose ();
		}

		private async void close (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (close.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield stream.close_async (Priority.DEFAULT, cancellable);
			} catch (IOError e) {
			}
		}

		public DTXChannel make_channel (string identifier) throws Error {
			check_open ();

			int32 channel_code = next_channel_code++;

			var channel = new DTXChannel (channel_code, this);
			channels[channel_code] = channel;

			establish_channel.begin (channel, identifier);

			return channel;
		}

		private void remove_channel (DTXChannel channel) {
			channels.unset (channel.code);

			if (channel != control_channel)
				control_channel.cancel_channel.begin (channel.code, io_cancellable);
		}

		private async void establish_channel (DTXChannel channel, string identifier) {
			try {
				yield control_channel.request_channel (channel.code, identifier, io_cancellable);
			} catch (GLib.Error e) {
				channels.unset (channel.code);
			}
		}

		private async void process_incoming_fragments () {
			while (true) {
				try {
					var fragment = yield read_fragment ();

					if (fragment.count == 1) {
						process_message (fragment.bytes.get_data (), fragment);
						continue;
					}

					Gee.ArrayList<Fragment> entries = fragments[fragment.identifier];
					if (entries == null) {
						if (fragment.index != 0)
							throw new Error.PROTOCOL ("Expected first fragment to have index of zero");
						fragment.data_size = 0;

						entries = new Gee.ArrayList<Fragment> ();
						fragments[fragment.identifier] = entries;
					}
					entries.add (fragment);

					var first_fragment = entries[0];

					if (fragment.bytes != null) {
						var size = fragment.bytes.get_size ();

						first_fragment.data_size += (uint32) size;
						if (first_fragment.data_size > MAX_MESSAGE_SIZE)
							throw new Error.PROTOCOL ("Message size exceeds maximum");
					}

					if (entries.size == fragment.count) {
						var message = new uint8[first_fragment.data_size];

						var sorted_entries = entries.order_by ((a, b) => (int) a.index - (int) b.index);
						uint i = 0;
						size_t offset = 0;
						while (sorted_entries.next ()) {
							Fragment f = sorted_entries.get ();

							if (f.index != i)
								throw new Error.PROTOCOL ("Inconsistent fragments received");

							var bytes = f.bytes;
							if (bytes != null) {
								var size = bytes.get_size ();
								Memory.copy ((uint8 *) message + offset, (uint8 *) bytes.get_data (), size);
								offset += size;
							}

							i++;
						}

						fragments.unset (fragment.identifier);

						process_message (message, first_fragment);
					}
				} catch (GLib.Error e) {
					_state = CLOSED;
					notify_property ("state");

					foreach (var channel in channels.values.to_array ())
						channel.close ();
					channels.clear ();

					return;
				}
			}
		}

		private async Fragment read_fragment () throws Error, IOError {
			var io_priority = Priority.DEFAULT;

			try {
				size_t minimum_header_size = 32;

				yield prepare_to_read (minimum_header_size);

				uint32 magic = input.read_uint32 (io_cancellable);
				if (magic != DTX_FRAGMENT_MAGIC)
					throw new Error.PROTOCOL ("Expected DTX message magic, got 0x%08x", magic);

				var fragment = new Fragment ();

				var header_size = input.read_uint32 (io_cancellable);
				if (header_size < minimum_header_size)
					throw new Error.PROTOCOL ("Expected header size of >= 32, got %u", header_size);

				fragment.index = input.read_uint16 (io_cancellable);
				fragment.count = input.read_uint16 (io_cancellable);
				fragment.data_size = input.read_uint32 (io_cancellable);
				fragment.identifier = input.read_uint32 (io_cancellable);
				fragment.conversation_index = input.read_uint32 (io_cancellable);
				fragment.channel_code = input.read_int32 (io_cancellable);
				fragment.flags = input.read_uint32 (io_cancellable);

				size_t extra_header_size = header_size - minimum_header_size;
				if (extra_header_size > 0)
					yield input.skip_async (extra_header_size, io_priority, io_cancellable);

				if (fragment.count == 1 || fragment.index != 0) {
					if (fragment.data_size == 0)
						throw new Error.PROTOCOL ("Empty fragments are not allowed");
					if (fragment.data_size > MAX_FRAGMENT_SIZE)
						throw new Error.PROTOCOL ("Fragment size exceeds maximum");

					if (fragment.data_size > input.get_buffer_size ())
						input.set_buffer_size (fragment.data_size);

					yield prepare_to_read (fragment.data_size);
					fragment.bytes = input.read_bytes (fragment.data_size, io_cancellable);
				}

				return fragment;
			} catch (GLib.Error e) {
				if (e is Error)
					throw (Error) e;
				throw new Error.TRANSPORT ("%s", e.message);
			}
		}

		private void process_message (uint8[] raw_message, Fragment fragment) throws Error {
			const size_t header_size = 16;

			size_t message_size = raw_message.length;
			if (message_size < header_size)
				throw new Error.PROTOCOL ("Malformed message");

			uint8 * m = (uint8 *) raw_message;

			var message = DTXMessage ();
			message.type = (DTXMessageType) *m;
			message.identifier = fragment.identifier;
			message.conversation_index = fragment.conversation_index;
			message.channel_code = fragment.channel_code;
			message.transport_flags = (DTXMessageTransportFlags) fragment.flags;

			uint32 aux_size = uint32.from_little_endian (*((uint32 *) (m + 4)));
			uint64 data_size = uint64.from_little_endian (*((uint64 *) (m + 8)));
			if (aux_size > message_size || data_size > message_size || data_size != message_size - header_size ||
					aux_size > data_size) {
				throw new Error.PROTOCOL ("Malformed message");
			}

			size_t aux_start_offset = header_size;
			size_t aux_end_offset = aux_start_offset + aux_size;
			message.aux_data = raw_message[aux_start_offset:aux_end_offset];

			size_t payload_start_offset = aux_end_offset;
			size_t payload_end_offset = payload_start_offset + (size_t) (data_size - aux_size);
			message.payload_data = raw_message[payload_start_offset:payload_end_offset];

			int32 channel_code = message.channel_code;
			if ((message.conversation_index % 2) == 0)
				channel_code = -channel_code;

			bool is_notification = message.conversation_index == 0;

			var channel = channels[channel_code];
			if (channel == null)
				return;

			switch (message.type) {
				case DISPATCH:
					channel.handle_dispatch (message);
					break;
				case OK:
				case OBJECT:
				case ERROR:
					if (is_notification)
						channel.handle_notification (message);
					else
						channel.handle_response (message);
					break;
				case DATA:
					channel.handle_data (message);
					break;
				case BARRIER:
					channel.handle_barrier (message);
					break;
				case PRIMITIVE:
				case COMPRESSED:
				case PROXIED_MESSAGE:
					throw new Error.PROTOCOL ("Unsupported message type: %s", message.type.to_string ());
			}
		}

		private void send_message (DTXMessage message, out uint32 identifier) {
			const size_t message_header_size = 16;
			uint32 message_aux_size = message.aux_data.length;
			uint64 message_data_size = message_aux_size + message.payload_data.length;
			size_t message_size = message_header_size + (size_t) message_data_size;
			const uint8 message_flags_a = 0;
			const uint8 message_flags_b = 0;
			const uint8 message_reserved = 0;

			const uint32 fragment_header_size = 32;
			const uint16 fragment_index = 0;
			const uint16 fragment_count = 1;
			uint32 fragment_data_size = (uint32) message_size;
			uint32 fragment_identifier = message.identifier;
			if (fragment_identifier == 0)
				fragment_identifier = next_fragment_identifier++;
			uint32 fragment_flags = message.transport_flags;

			var data = new uint8[fragment_header_size + message_size];

			uint8 * p = (uint8 *) data;
			*((uint32 *) (p + 0)) = DTX_FRAGMENT_MAGIC.to_little_endian ();
			*((uint32 *) (p + 4)) = fragment_header_size.to_little_endian ();
			*((uint16 *) (p + 8)) = fragment_index.to_little_endian ();
			*((uint16 *) (p + 10)) = fragment_count.to_little_endian ();
			*((uint32 *) (p + 12)) = fragment_data_size.to_little_endian ();
			*((uint32 *) (p + 16)) = fragment_identifier.to_little_endian ();
			*((uint32 *) (p + 20)) = message.conversation_index.to_little_endian ();
			*((int32 *) (p + 24)) = message.channel_code.to_little_endian ();
			*((uint32 *) (p + 28)) = fragment_flags.to_little_endian ();
			p += fragment_header_size;

			*(p + 0) = message.type;
			*(p + 1) = message_flags_a;
			*(p + 2) = message_flags_b;
			*(p + 3) = message_reserved;
			*((uint32 *) (p + 4)) = message_aux_size.to_little_endian ();
			*((uint64 *) (p + 8)) = message_data_size.to_little_endian ();
			p += message_header_size;

			Memory.copy (p, message.aux_data, message.aux_data.length);
			p += message.aux_data.length;

			Memory.copy (p, message.payload_data, message.payload_data.length);
			p += message.payload_data.length;

			assert (p == (uint8 *) data + data.length);

			write_bytes (new Bytes.take ((owned) data));

			identifier = fragment_identifier;
		}

		private async void prepare_to_read (size_t required) throws GLib.Error {
			while (true) {
				size_t available = input.get_available ();
				if (available >= required)
					return;
				ssize_t n = yield input.fill_async ((ssize_t) (required - available), Priority.DEFAULT, io_cancellable);
				if (n == 0)
					throw new Error.TRANSPORT ("Connection closed");
			}
		}

		private void write_bytes (Bytes bytes) {
			pending_writes.offer_tail (bytes);
			if (pending_writes.size == 1)
				process_pending_writes.begin ();
		}

		private async void process_pending_writes () {
			while (!pending_writes.is_empty) {
				Bytes current = pending_writes.peek_head ();

				size_t bytes_written;
				try {
					yield output.write_all_async (current.get_data (), Priority.DEFAULT, io_cancellable,
						out bytes_written);
				} catch (GLib.Error e) {
					return;
				}

				pending_writes.poll_head ();
			}
		}

		private void check_open () throws Error {
			if (_state != OPEN)
				throw new Error.INVALID_OPERATION ("Connection is closed");
		}

		private class Fragment {
			public uint16 index;
			public uint16 count;
			public uint32 data_size;
			public uint32 identifier;
			public uint32 conversation_index;
			public int32 channel_code;
			public uint32 flags;
			public Bytes? bytes;
		}
	}

	private sealed class DTXControlChannel : DTXChannel {
		public DTXControlChannel (DTXTransport transport) {
			Object (code: 0, transport: transport);
		}

		public void notify_of_published_capabilities () throws Error {
			var capabilities = new NSDictionary ();
			capabilities.set_value ("com.apple.private.DTXConnection", new NSNumber.from_integer (1));
			capabilities.set_value ("com.apple.instruments.client.processcontrol.capability.terminationCallback",
				new NSNumber.from_integer (1));

			var args = new DTXArgumentListBuilder ()
				.append_object (capabilities);
			invoke_without_reply ("_notifyOfPublishedCapabilities:", args);
		}

		public async void request_channel (int32 code, string identifier, Cancellable? cancellable) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_int32 (code)
				.append_object (new NSString (identifier));
			yield invoke ("_requestChannelWithCode:identifier:", args, cancellable);
		}

		public async void cancel_channel (int32 code, Cancellable? cancellable) throws Error, IOError {
			var args = new DTXArgumentListBuilder ()
				.append_int32 (code);
			yield invoke ("_channelCanceled:", args, cancellable);
		}
	}

	public class DTXChannel : Object {
		public signal void invocation (string method_name, DTXArgumentList args, DTXMessageTransportFlags transport_flags);
		public signal void notification (NSObject obj);
		public signal void data (Bytes blob);
		public signal void barrier ();

		public int32 code {
			get;
			construct;
		}

		public weak DTXTransport? transport {
			get;
			set;
		}

		public State state {
			get {
				return _state;
			}
		}

		public enum State {
			OPEN,
			CLOSED
		}

		private State _state = OPEN;

		private Gee.HashMap<uint32, Promise<NSObject?>> pending_responses = new Gee.HashMap<uint32, Promise<NSObject?>> ();

		public DTXChannel (int32 code, DTXTransport transport) {
			Object (code: code, transport: transport);
		}

		public override void dispose () {
			close ();

			base.dispose ();
		}

		internal void close () {
			_state = CLOSED;
			notify_property ("state");

			var error = new Error.TRANSPORT ("Channel closed");
			foreach (var request in pending_responses.values.to_array ())
				request.reject (error);

			if (transport != null) {
				transport.remove_channel (this);
				transport = null;
			}
		}

		public async NSObject? invoke (string method_name, DTXArgumentListBuilder? args, Cancellable? cancellable)
				throws Error, IOError {
			check_open ();

			var message = DTXMessage ();
			message.type = DISPATCH;
			message.channel_code = code;
			message.transport_flags = EXPECTS_REPLY;

			Bytes aux_data;
			if (args != null) {
				aux_data = args.build ();
				message.aux_data = aux_data.get_data ();
			}

			var payload_data = NSKeyedArchive.encode (new NSString (method_name));
			message.payload_data = payload_data;

			uint32 identifier;
			transport.send_message (message, out identifier);

			var request = new Promise<NSObject?> ();
			pending_responses[identifier] = request;

			try {
				return yield request.future.wait_async (cancellable);
			} finally {
				pending_responses.unset (identifier);
			}
		}

		public void invoke_without_reply (string method_name, DTXArgumentListBuilder? args) throws Error {
			check_open ();

			var message = DTXMessage ();
			message.type = DISPATCH;
			message.channel_code = code;
			message.transport_flags = NONE;

			Bytes aux_data;
			if (args != null) {
				aux_data = args.build ();
				message.aux_data = aux_data.get_data ();
			}

			var payload_data = NSKeyedArchive.encode (new NSString (method_name));
			message.payload_data = payload_data;

			uint32 identifier;
			transport.send_message (message, out identifier);
		}

		internal void handle_dispatch (DTXMessage message) throws Error {
			NSString? method_name = NSKeyedArchive.decode (message.payload_data) as NSString;
			if (method_name == null)
				throw new Error.PROTOCOL ("Malformed dispatch payload");

			var args = DTXArgumentList.parse (message.aux_data);

			invocation (method_name.str, args, message.transport_flags);
		}

		internal void handle_response (DTXMessage message) throws Error {
			var request = pending_responses[message.identifier];
			if (request != null) {
				switch (message.type) {
					case OK:
						request.resolve (null);
						break;
					case OBJECT:
						request.resolve (NSKeyedArchive.decode (message.payload_data));
						break;
					case ERROR: {
						NSError? error = NSKeyedArchive.decode (message.payload_data) as NSError;
						if (error == null)
							throw new Error.PROTOCOL ("Malformed error payload");

						var description = new StringBuilder.sized (128);

						var user_info = error.user_info;
						if (user_info != null) {
							NSString? val;
							if (user_info.get_optional_value ("NSLocalizedDescription", out val))
								description.append (val.str);
						}

						if (description.len == 0) {
							description.append_printf ("Invocation failed; domain=%s code=%" +
									int64.FORMAT_MODIFIER + "d",
								error.domain.str, error.code);
						}

						request.reject (new Error.NOT_SUPPORTED ("%s", description.str));

						break;
					}
					default:
						assert_not_reached ();
				}
			}
		}

		internal void handle_notification (DTXMessage message) throws Error {
			var payload = NSKeyedArchive.decode (message.payload_data);
			notification (payload);
		}

		internal void handle_data (DTXMessage message) throws Error {
			data (new Bytes (message.payload_data));
		}

		internal void handle_barrier (DTXMessage message) throws Error {
			barrier ();
		}

		private void check_open () throws Error {
			if (_state != OPEN)
				throw new Error.INVALID_OPERATION ("Channel is closed");
		}
	}

	public interface DTXTransport : Object {
		public abstract void send_message (DTXMessage message, out uint32 identifier);
		public abstract void remove_channel (DTXChannel channel);
	}

	public enum DTXMessageType {
		OK,
		DATA,
		DISPATCH,
		OBJECT,
		ERROR,
		BARRIER,
		PRIMITIVE,
		COMPRESSED,
		PROXIED_MESSAGE,
	}

	public struct DTXMessage {
		public DTXMessageType type;
		public uint32 identifier;
		public uint32 conversation_index;
		public int32 channel_code;
		public DTXMessageTransportFlags transport_flags;
		public unowned uint8[] aux_data;
		public unowned uint8[] payload_data;
	}

	[Flags]
	public enum DTXMessageTransportFlags {
		NONE          = 0,
		EXPECTS_REPLY = (1 << 0),
	}

	public class DTXArgumentList {
		public Value[] elements;

		private DTXArgumentList (owned Value[] elements) {
			this.elements = (owned) elements;
		}

		public static DTXArgumentList parse (uint8[] data) throws Error {
			var elements = new Value[0];

			var reader = new PrimitiveReader (data);

			reader.skip (PRIMITIVE_DICTIONARY_HEADER_SIZE);

			while (reader.available_bytes != 0) {
				PrimitiveType type;

				type = (PrimitiveType) reader.read_uint32 ();
				if (type != INDEX)
					throw new Error.PROTOCOL ("Unsupported primitive dictionary key type");

				type = (PrimitiveType) reader.read_uint32 ();
				switch (type) {
					case STRING: {
						size_t size = reader.read_uint32 ();
						string val = reader.read_string (size);

						var gval = Value (typeof (string));
						gval.take_string ((owned) val);
						elements += (owned) gval;

						break;
					}
					case BUFFER: {
						size_t size = reader.read_uint32 ();
						if (size == 0) {
							var gval = Value (typeof (NSObject));
							elements += (owned) gval;
							break;
						}

						unowned uint8[] buf = reader.read_byte_array (size);

						NSObject? obj = NSKeyedArchive.decode (buf);
						if (obj != null) {
							var gval = Value (Type.from_instance (obj));
							gval.set_instance (obj);
							elements += (owned) gval;
						} else {
							var gval = Value (typeof (NSObject));
							elements += (owned) gval;
						}

						break;
					}
					case INT32: {
						int32 val = reader.read_int32 ();

						var gval = Value (typeof (int));
						gval.set_int (val);
						elements += (owned) gval;

						break;
					}
					case INT64: {
						int64 val = reader.read_int64 ();

						var gval = Value (typeof (int64));
						gval.set_int64 (val);
						elements += (owned) gval;

						break;
					}
					case DOUBLE: {
						double val = reader.read_double ();

						var gval = Value (typeof (double));
						gval.set_double (val);
						elements += (owned) gval;

						break;
					}
					default:
						throw new Error.PROTOCOL ("Unsupported primitive dictionary value type");
				}
			}

			return new DTXArgumentList ((owned) elements);
		}
	}

	public sealed class DTXArgumentListBuilder {
		private PrimitiveBuilder blob = new PrimitiveBuilder ();

		public DTXArgumentListBuilder () {
			blob.seek (PRIMITIVE_DICTIONARY_HEADER_SIZE);
		}

		public unowned DTXArgumentListBuilder append_string (string str) {
			begin_entry (STRING)
				.append_uint32 (str.length)
				.append_string (str);
			return this;
		}

		public unowned DTXArgumentListBuilder append_object (NSObject? obj) {
			var buf = NSKeyedArchive.encode (obj);
			begin_entry (BUFFER)
				.append_uint32 (buf.length)
				.append_byte_array (buf);
			return this;
		}

		public unowned DTXArgumentListBuilder append_int32 (int32 val) {
			begin_entry (INT32)
				.append_int32 (val);
			return this;
		}

		public unowned DTXArgumentListBuilder append_int64 (int64 val) {
			begin_entry (INT64)
				.append_int64 (val);
			return this;
		}

		public unowned DTXArgumentListBuilder append_double (double val) {
			begin_entry (DOUBLE)
				.append_double (val);
			return this;
		}

		private unowned PrimitiveBuilder begin_entry (PrimitiveType type) {
			return blob
				.append_uint32 (PrimitiveType.INDEX)
				.append_uint32 (type);
		}

		public Bytes build () {
			size_t size = blob.offset - PRIMITIVE_DICTIONARY_HEADER_SIZE;
			return blob.seek (0)
				.append_uint64 (size)
				.append_uint64 (size)
				.build ();
		}
	}

	private enum PrimitiveType {
		STRING = 1,
		BUFFER = 2,
		INT32 = 3,
		INT64 = 6,
		DOUBLE = 9,
		INDEX = 10
	}

	private const size_t PRIMITIVE_DICTIONARY_HEADER_SIZE = 16;

	private sealed class PrimitiveReader {
		public size_t available_bytes {
			get {
				return end - cursor;
			}
		}

		private uint8 * cursor;
		private uint8 * end;

		public PrimitiveReader (uint8[] data) {
			cursor = (uint8 *) data;
			end = cursor + data.length;
		}

		public void skip (size_t n) throws Error {
			check_available (n);
			cursor += n;
		}

		public int32 read_int32 () throws Error {
			const size_t n = sizeof (int32);
			check_available (n);

			int32 val = int32.from_little_endian (*((int32 *) cursor));
			cursor += n;

			return val;
		}

		public uint32 read_uint32 () throws Error {
			const size_t n = sizeof (uint32);
			check_available (n);

			uint32 val = uint32.from_little_endian (*((uint32 *) cursor));
			cursor += n;

			return val;
		}

		public int64 read_int64 () throws Error {
			const size_t n = sizeof (int64);
			check_available (n);

			int64 val = int64.from_little_endian (*((int64 *) cursor));
			cursor += n;

			return val;
		}

		public uint64 read_uint64 () throws Error {
			const size_t n = sizeof (uint64);
			check_available (n);

			uint64 val = uint64.from_little_endian (*((uint64 *) cursor));
			cursor += n;

			return val;
		}

		public double read_double () throws Error {
			uint64 bits = read_uint64 ();
			return *((double *) &bits);
		}

		public unowned uint8[] read_byte_array (size_t n) throws Error {
			check_available (n);

			unowned uint8[] arr = ((uint8[]) cursor)[0:n];
			cursor += n;

			return arr;
		}

		public string read_string (size_t size) throws Error {
			check_available (size);

			unowned string data = (string) cursor;
			string str = data.substring (0, (long) size);
			cursor += size;

			return str;
		}

		private void check_available (size_t n) throws Error {
			if (cursor + n > end)
				throw new Error.PROTOCOL ("Invalid dictionary");
		}
	}

	private sealed class PrimitiveBuilder {
		public size_t offset {
			get {
				return cursor;
			}
		}

		private ByteArray buffer = new ByteArray.sized (64);
		private size_t cursor = 0;

		public unowned PrimitiveBuilder seek (size_t offset) {
			if (buffer.len < offset) {
				size_t n = offset - buffer.len;
				Memory.set (get_pointer (offset - n, n), 0, n);
			}
			cursor = offset;
			return this;
		}

		public unowned PrimitiveBuilder append_int32 (int32 val) {
			*((int32 *) get_pointer (cursor, sizeof (int32))) = val.to_little_endian ();
			cursor += (uint) sizeof (int32);
			return this;
		}

		public unowned PrimitiveBuilder append_uint32 (uint32 val) {
			*((uint32 *) get_pointer (cursor, sizeof (uint32))) = val.to_little_endian ();
			cursor += (uint) sizeof (uint32);
			return this;
		}

		public unowned PrimitiveBuilder append_int64 (int64 val) {
			*((int64 *) get_pointer (cursor, sizeof (int64))) = val.to_little_endian ();
			cursor += (uint) sizeof (int64);
			return this;
		}

		public unowned PrimitiveBuilder append_uint64 (uint64 val) {
			*((uint64 *) get_pointer (cursor, sizeof (uint64))) = val.to_little_endian ();
			cursor += (uint) sizeof (uint64);
			return this;
		}

		public unowned PrimitiveBuilder append_double (double val) {
			uint64 raw_val = *((uint64 *) &val);
			*((uint64 *) get_pointer (cursor, sizeof (uint64))) = raw_val.to_little_endian ();
			cursor += (uint) sizeof (uint64);
			return this;
		}

		public unowned PrimitiveBuilder append_byte_array (uint8[] array) {
			uint size = array.length;
			Memory.copy (get_pointer (cursor, size), array, size);
			cursor += size;
			return this;
		}

		public unowned PrimitiveBuilder append_string (string str) {
			uint size = str.length;
			Memory.copy (get_pointer (cursor, size), str, size);
			cursor += size;
			return this;
		}

		private uint8 * get_pointer (size_t offset, size_t n) {
			size_t minimum_size = offset + n;
			if (buffer.len < minimum_size)
				buffer.set_size ((uint) minimum_size);

			return (uint8 *) buffer.data + offset;
		}

		public Bytes build () {
			return ByteArray.free_to_bytes ((owned) buffer);
		}
	}
}
