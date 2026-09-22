namespace Frida.Server {
	private static Application application;

	private const string DEFAULT_DIRECTORY = "re.frida.server";
	private static bool output_version = false;
	private static string? device_id = null;
	private static string? listen_address = null;
	private static string? certpath = null;
	private static string? origin = null;
	private static string? token = null;
	private static string? asset_root = null;
	private static string? directory = null;
#if !WINDOWS && !TVOS
	private static bool daemonize = false;
#endif
	private static string? softener_flavor_str = null;
	private static bool enable_preload = true;
	private static bool report_crashes = true;
	private static bool verbose = false;

	private enum PolicySoftenerFlavor {
		SYSTEM,
		INTERNAL;

		public static PolicySoftenerFlavor from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<PolicySoftenerFlavor> (nick);
		}
	}

	private delegate void ReadyHandler (bool success);

	const OptionEntry[] option_entries = {
		{ "version", 0, 0, OptionArg.NONE, ref output_version, "Output version information and exit", null },
		{ "device", 0, 0, OptionArg.STRING, ref device_id, "Serve device with the given ID", "ID" },
		{ "listen", 'l', 0, OptionArg.STRING, ref listen_address, "Listen on ADDRESS", "ADDRESS" },
		{ "certificate", 0, 0, OptionArg.FILENAME, ref certpath, "Enable TLS using CERTIFICATE", "CERTIFICATE" },
		{ "origin", 0, 0, OptionArg.STRING, ref origin, "Only accept requests with “Origin” header matching ORIGIN " +
			"(by default any origin will be accepted)", "ORIGIN" },
		{ "token", 0, 0, OptionArg.STRING, ref token, "Require authentication using TOKEN", "TOKEN" },
		{ "asset-root", 0, 0, OptionArg.FILENAME, ref asset_root, "Serve static files inside ROOT (by default no files are served)",
			"ROOT" },
		{ "directory", 'd', 0, OptionArg.STRING, ref directory, "Store binaries in DIRECTORY", "DIRECTORY" },
#if !WINDOWS && !TVOS
		{ "daemonize", 'D', 0, OptionArg.NONE, ref daemonize, "Detach and become a daemon", null },
#endif
		{ "policy-softener", 0, 0, OptionArg.STRING, ref softener_flavor_str, "Select policy softener", "system|internal" },
		{ "disable-preload", 'P', OptionFlags.REVERSE, OptionArg.NONE, ref enable_preload, "Disable preload optimization", null },
		{ "ignore-crashes", 'C', OptionFlags.REVERSE, OptionArg.NONE, ref report_crashes,
			"Disable native crash reporter integration", null },
		{ "verbose", 'v', 0, OptionArg.NONE, ref verbose, "Be verbose", null },
		{ null }
	};

	private static int main (string[] args) {
		Environment.init ();

#if DARWIN
		if (Path.get_basename (args[0]) == "frida-policyd") {
			return Policyd._main ();
		}
#endif

		try {
			var ctx = new OptionContext ();
			ctx.set_help_enabled (true);
			ctx.add_main_entries (option_entries, null);
			ctx.parse (ref args);
		} catch (OptionError e) {
			printerr ("%s\n", e.message);
			printerr ("Run '%s --help' to see a full list of available command line options.\n", args[0]);
			return 1;
		}

		if (output_version) {
			stdout.printf ("%s\n", version_string ());
			return 0;
		}

		Environment.set_verbose_logging_enabled (verbose);

		EndpointParameters endpoint_params;
		try {
			endpoint_params = new EndpointParameters (listen_address, 0, parse_certificate (certpath), origin,
				(token != null) ? new StaticAuthenticationService (token) : null,
				(asset_root != null) ? File.new_for_path (asset_root) : null);
		} catch (GLib.Error e) {
			printerr ("%s\n", e.message);
			return 2;
		}

		var options = new ControlServiceOptions ();
		options.enable_preload = enable_preload;
		options.report_crashes = report_crashes;

#if (IOS || TVOS) && !HAVE_EMBEDDED_ASSETS
		string? program_path = null;
		Gum.Process.enumerate_modules (m => {
			uint32 * file_type = (uint32 *) (m.range.base_address + 12);
			const uint32 MH_EXECUTE = 2;
			if (*file_type == MH_EXECUTE) {
				program_path = m.path;
				return false;
			}
			return true;
		});
		int prefix_pos = program_path.last_index_of (Config.FRIDA_PREFIX + "/");
		if (prefix_pos != -1 && prefix_pos != 0)
			TemporaryDirectory.use_sysroot (program_path[:prefix_pos]);
#endif

		PolicySoftenerFlavor softener_flavor = SYSTEM;
		if (softener_flavor_str != null) {
			try {
				softener_flavor = PolicySoftenerFlavor.from_nick (softener_flavor_str);
			} catch (Error e) {
				printerr ("%s\n", e.message);
				return 3;
			}
		}

#if IOS || TVOS
		if (softener_flavor == INTERNAL)
			InternalIOSTVOSPolicySoftener.enable ();
#endif

		ReadyHandler? on_ready = null;
#if !WINDOWS && !TVOS
		if (daemonize) {
			var sync_fds = new int[2];

			try {
				Unix.open_pipe (sync_fds, 0);
				Unix.set_fd_nonblocking (sync_fds[0], true);
				Unix.set_fd_nonblocking (sync_fds[1], true);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			var sync_in = new UnixInputStream (sync_fds[0], true);
			var sync_out = new UnixOutputStream (sync_fds[1], true);

			var pid = Posix.fork ();
			if (pid != 0) {
				try {
					var status = new uint8[1];
					sync_in.read (status);
					return status[0];
				} catch (GLib.Error e) {
					return 4;
				}
			}

			sync_in = null;
			on_ready = (success) => {
				if (success) {
					Posix.setsid ();

					var null_in = Posix.open ("/dev/null", Posix.O_RDONLY);
					var null_out = Posix.open ("/dev/null", Posix.O_WRONLY);
					Posix.dup2 (null_in, Posix.STDIN_FILENO);
					Posix.dup2 (null_out, Posix.STDOUT_FILENO);
					Posix.dup2 (null_out, Posix.STDERR_FILENO);
					Posix.close (null_in);
					Posix.close (null_out);
				}

				var status = new uint8[1];
				status[0] = success ? 0 : 1;
				try {
					sync_out.write (status);
				} catch (GLib.Error e) {
				}
				sync_out = null;
			};
		}
#endif

		Environment.configure ();

#if DARWIN
		var worker = new Thread<int> ("frida-server-main-loop", () => {
			var exit_code = run_application (device_id, endpoint_params, options, on_ready);

			_stop_run_loop ();

			return exit_code;
		});
		_start_run_loop ();

		var exit_code = worker.join ();

		return exit_code;
#else
		return run_application (device_id, endpoint_params, options, on_ready);
#endif
	}

	private static int run_application (string? device_id, EndpointParameters endpoint_params, ControlServiceOptions options,
			ReadyHandler on_ready) {
		TemporaryDirectory.always_use ((directory != null) ? directory : DEFAULT_DIRECTORY);

		application = new Application (device_id, endpoint_params, options);

		Posix.signal (Posix.Signal.INT, (sig) => {
			application.stop ();
		});
		Posix.signal (Posix.Signal.TERM, (sig) => {
			application.stop ();
		});

		if (on_ready != null) {
			application.ready.connect (success => {
				on_ready (success);
				on_ready = null;
			});
		}

		return application.run ();
	}

	namespace Environment {
		public extern void init ();
		public extern void set_verbose_logging_enabled (bool enabled);
		public extern void configure ();
	}

#if DARWIN
	public extern void _start_run_loop ();
	public extern void _stop_run_loop ();

	namespace Policyd {
		public extern int _main ();
	}
#endif

	private sealed class Application : Object {
		public signal void ready (bool success);

		public string? device_id {
			get;
			construct;
		}

		public EndpointParameters endpoint_params {
			get;
			construct;
		}

		public ControlServiceOptions options {
			get;
			construct;
		}

		private DeviceManager? manager;
		private ControlService? service;

		private Cancellable io_cancellable = new Cancellable ();

		private MainLoop loop = new MainLoop ();
		private int exit_code;
		private bool stopping;

		public Application (string? device_id, EndpointParameters endpoint_params, ControlServiceOptions options) {
			Object (
				device_id: device_id,
				endpoint_params: endpoint_params,
				options: options
			);
		}

		public int run () {
			Idle.add (() => {
				start.begin ();
				return false;
			});

			exit_code = 0;

			loop.run ();

			return exit_code;
		}

		private async void start () {
			try {
				if (device_id != null && device_id != "local") {
					manager = new DeviceManager.with_nonlocal_backends_only ();

					var device = yield manager.get_device_by_id (device_id, 0, io_cancellable);
					device.lost.connect (on_device_lost);

					service = yield new ControlService.with_device (device, endpoint_params, options);
				} else {
					service = new ControlService (endpoint_params, options);
				}

				yield service.start (io_cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					return;
				printerr ("Unable to start: %s\n", e.message);
				exit_code = 5;
				loop.quit ();
				ready (false);
				return;
			}

			Idle.add (() => {
				ready (true);
				return false;
			});
		}

		public void stop () {
			Idle.add (() => {
				perform_stop.begin ();
				return false;
			});
		}

		private async void perform_stop () {
			if (stopping)
				return;
			stopping = true;

			io_cancellable.cancel ();

			try {
				if (service != null) {
					yield service.stop ();
					service = null;
				}

				if (manager != null) {
					yield manager.close ();
					manager = null;
				}
			} catch (GLib.Error e) {
			}

			Idle.add (() => {
				loop.quit ();
				return false;
			});
		}

		private void on_device_lost () {
			stop ();
		}
	}

	private TlsCertificate? parse_certificate (string? path) throws GLib.Error {
		if (path == null)
			return null;

		return new TlsCertificate.from_file (path);
	}
}
