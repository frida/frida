namespace Frida.Portal {
	private static Application application;

	private static bool output_version = false;
	private static string? cluster_address = null;
	private static string? cluster_certpath = null;
	private static string? cluster_token = null;
	private static string? control_address = null;
	private static string? control_certpath = null;
	private static string? control_origin = null;
	private static string? control_token = null;
	private static string? control_asset_root = null;
#if !WINDOWS && !TVOS
	private static bool daemonize = false;
#endif

	private delegate void ReadyHandler (bool success);

	const OptionEntry[] options = {
		{ "version", 0, 0, OptionArg.NONE, ref output_version, "Output version information and exit", null },
		{ "cluster-endpoint", 0, 0, OptionArg.STRING, ref cluster_address, "Expose cluster endpoint on ADDRESS", "ADDRESS" },
		{ "cluster-certificate", 0, 0, OptionArg.FILENAME, ref cluster_certpath, "Enable TLS on cluster endpoint using CERTIFICATE",
			"CERTIFICATE" },
		{ "cluster-token", 0, 0, OptionArg.STRING, ref cluster_token, "Require authentication on cluster endpoint using TOKEN",
			"TOKEN" },
		{ "control-endpoint", 0, 0, OptionArg.STRING, ref control_address, "Expose control endpoint on ADDRESS", "ADDRESS" },
		{ "control-certificate", 0, 0, OptionArg.FILENAME, ref control_certpath, "Enable TLS on control endpoint using CERTIFICATE",
			"CERTIFICATE" },
		{ "control-origin", 0, 0, OptionArg.STRING, ref control_origin, "Only accept control endpoint requests with “Origin” " +
			"header matching ORIGIN (by default any origin will be accepted)", "ORIGIN" },
		{ "control-token", 0, 0, OptionArg.STRING, ref control_token, "Require authentication on control endpoint using TOKEN",
			"TOKEN" },
		{ "control-asset-root", 0, 0, OptionArg.FILENAME, ref control_asset_root, "Serve static files inside ROOT on control " +
			"endpoint (by default no files are served)", "ROOT" },
#if !WINDOWS && !TVOS
		{ "daemonize", 'D', 0, OptionArg.NONE, ref daemonize, "Detach and become a daemon", null },
#endif
		{ null }
	};

	private static int main (string[] args) {
#if HAVE_GIOAPPLE
		GIOApple.register ();
#elif HAVE_GIOOPENSSL
		GIOOpenSSL.register ();
#endif

		try {
			var ctx = new OptionContext ();
			ctx.set_help_enabled (true);
			ctx.add_main_entries (options, null);
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

		EndpointParameters cluster_params, control_params;
		try {
			cluster_params = new EndpointParameters (cluster_address, 0, parse_certificate (cluster_certpath), null,
				(cluster_token != null) ? new StaticAuthenticationService (cluster_token) : null);
			control_params = new EndpointParameters (control_address, 0, parse_certificate (control_certpath), control_origin,
				(control_token != null) ? new StaticAuthenticationService (control_token) : null,
				(control_asset_root != null) ? File.new_for_path (control_asset_root) : null);
		} catch (GLib.Error e) {
			printerr ("%s\n", e.message);
			return 2;
		}

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
					return 3;
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

		application = new Application (new PortalService (cluster_params, control_params));

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

	private sealed class Application : Object {
		public signal void ready (bool success);

		public PortalService service {
			get;
			construct;
		}

		private Cancellable io_cancellable = new Cancellable ();

		private MainLoop loop = new MainLoop ();
		private int exit_code;
		private bool stopping;

		public Application (PortalService service) {
			Object (service: service);
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
				yield service.start (io_cancellable);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					return;
				printerr ("Unable to start: %s\n", e.message);
				exit_code = 4;
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
				yield service.stop ();
			} catch (GLib.Error e) {
			}

			Idle.add (() => {
				loop.quit ();
				return false;
			});
		}
	}

	private TlsCertificate? parse_certificate (string? path) throws GLib.Error {
		if (path == null)
			return null;

		return new TlsCertificate.from_file (path);
	}
}
