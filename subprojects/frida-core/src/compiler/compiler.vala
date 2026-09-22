namespace Frida {
	/**
	 * Compiles a TypeScript or JavaScript project into a single script bundle
	 * that can be loaded with {@link Session.create_script}.
	 */
	public sealed class Compiler : Object {
		/**
		 * Emitted when a build is starting.
		 */
		public signal void starting ();
		/**
		 * Emitted when a build has finished.
		 */
		public signal void finished ();
		/**
		 * Emitted with the freshly built bundle, primarily when watching.
		 *
		 * @param bundle the compiled script bundle
		 */
		public signal void output (string bundle);
		/**
		 * Emitted with compiler diagnostics, such as type errors and warnings.
		 *
		 * @param diagnostics the diagnostics, as a variant
		 */
		public signal void diagnostics (Variant diagnostics);

		private size_t watch_session_handle = 0;
		private Gee.Queue<Diagnostic> pending_diagnostics = new Gee.ArrayQueue<Diagnostic> ();

		private MainContext main_context;

		// TODO: Remove the DeviceManager parameter.
		/**
		 * Creates a compiler.
		 *
		 * @param manager unused; kept for compatibility
		 */
		public Compiler (DeviceManager? manager = null) {
			Object ();
		}

		static construct {
			CompilerBackend.init ();
		}

		construct {
			main_context = Frida.get_main_context ();
		}

		~Compiler () {
			cancel_watch ();
		}

		/**
		 * Builds the project rooted at @entrypoint once.
		 *
		 * @param entrypoint path to the project's entrypoint module
		 * @param options build options, or null for the defaults
		 * @return the compiled script bundle
		 */
		public async string build (string entrypoint, BuildOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			CompilerBackend.check_available ();

			BuildOptions opts = (options != null) ? options : new BuildOptions ();
			string project_root = compute_project_root (entrypoint, opts);

			starting ();
			try {
				string? bundle = null;
				string? error_message = null;
				CompilerBackend.BuildCompleteFunc on_complete = (b, e) => {
					bundle = b;
					error_message = e;
					schedule_on_frida_thread (build.callback);
				};

				CompilerBackend.build (project_root, entrypoint, opts.output_format, opts.bundle_format,
					(size_t) (opts.type_check == NONE), (size_t) (opts.source_maps == INCLUDED),
					(size_t) (opts.compression == TERSER), opts.platform.to_nick (),
					opts.externals.to_array (), on_diagnostic, (owned) on_complete);
				yield;

				if (error_message != null)
					throw new Error.INVALID_ARGUMENT ("%s", error_message);

				output (bundle);

				return bundle;
			} finally {
				finished ();
			}
		}

		public string build_sync (string entrypoint, BuildOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<BuildTask> ();
			task.entrypoint = entrypoint;
			task.options = options;
			return task.execute (cancellable);
		}

		private class BuildTask : CompilerTask<string> {
			public string entrypoint;
			public BuildOptions? options;

			protected override async string perform_operation () throws Error, IOError {
				return yield parent.build (entrypoint, options, cancellable);
			}
		}

		/**
		 * Builds the project and keeps rebuilding it as its sources change,
		 * emitting {@link Compiler.output} with each fresh bundle.
		 *
		 * @param entrypoint path to the project's entrypoint module
		 * @param options watch options, or null for the defaults
		 */
		public async void watch (string entrypoint, WatchOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			CompilerBackend.check_available ();

			WatchOptions opts = (options != null) ? options : new WatchOptions ();
			string project_root = compute_project_root (entrypoint, opts);

			cancel_watch ();

			size_t session_handle = 0;
			string? error_message = null;
			CompilerBackend.WatchReadyFunc on_ready = (h, e) => {
				session_handle = h;
				error_message = e;
				schedule_on_frida_thread (watch.callback);
			};

			CompilerBackend.watch (project_root, entrypoint, opts.output_format, opts.bundle_format,
				(size_t) (opts.type_check == NONE), (size_t) (opts.source_maps == INCLUDED),
				(size_t) (opts.compression == TERSER), opts.platform.to_nick (),
				opts.externals.to_array (), on_starting, on_finished, on_output, on_diagnostic,
				(owned) on_ready);
			yield;

			if (error_message != null)
				throw new Error.INVALID_ARGUMENT ("%s", error_message);

			watch_session_handle = session_handle;
		}

		private void cancel_watch () {
			if (watch_session_handle != 0) {
				CompilerBackend.WatchSession.dispose (watch_session_handle);
				watch_session_handle = 0;
			}
		}

		public void watch_sync (string entrypoint, WatchOptions? options = null, Cancellable? cancellable = null)
				throws Error, IOError {
			var task = create<WatchTask> ();
			task.entrypoint = entrypoint;
			task.options = options;
			task.execute (cancellable);
		}

		private class WatchTask : CompilerTask<void> {
			public string entrypoint;
			public WatchOptions? options;

			protected override async void perform_operation () throws Error, IOError {
				yield parent.watch (entrypoint, options, cancellable);
			}
		}

		private void on_starting () {
			schedule_on_frida_thread (() => {
				starting ();
				return Source.REMOVE;
			});
		}

		private void on_finished () {
			schedule_on_frida_thread (() => {
				finished ();
				return Source.REMOVE;
			});
		}

		private void on_output (string bundle) {
			string bundle_copy = bundle;
			schedule_on_frida_thread (() => {
				output (bundle_copy);
				return Source.REMOVE;
			});
		}

		private void on_diagnostic (string category, int code, string? path, int line, int character,
				string text) {
			var diag = new Diagnostic () {
				category = category,
				code = code,
				path = path,
				line = line,
				character = character,
				text = text,
			};

			bool schedule_emit = false;
			lock (pending_diagnostics) {
				schedule_emit = pending_diagnostics.is_empty;
				pending_diagnostics.add (diag);
			}

			if (schedule_emit) {
				schedule_on_frida_thread (() => {
					emit_pending_diagnostics ();
					return Source.REMOVE;
				});
			}
		}

		private void emit_pending_diagnostics () {
			var batch = new Gee.ArrayList<Diagnostic> ();
			lock (pending_diagnostics) {
				batch.add_all (pending_diagnostics);
				pending_diagnostics.clear ();
			}

			var builder = new VariantBuilder (new VariantType.array (VariantType.VARDICT));

			foreach (var d in batch) {
				builder.open (VariantType.VARDICT);
				builder.add ("{sv}", "category", new Variant.string (d.category));
				builder.add ("{sv}", "code", new Variant.int64 (d.code));
				if (d.path != null) {
					var b = new VariantBuilder (VariantType.VARDICT);
					b.add ("{sv}", "path", new Variant.string (d.path));
					b.add ("{sv}", "line", new Variant.int64 (d.line));
					b.add ("{sv}", "character", new Variant.int64 (d.character));
					builder.add ("{sv}", "file", b.end ());
				}
				builder.add ("{sv}", "text", new Variant.string (d.text));
				builder.close ();
			}

			diagnostics (builder.end ());
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		protected void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}

		private class Diagnostic {
			public string category;
			public int code;
			public string? path;
			public int line;
			public int character;
			public string text;
		}

		private abstract class CompilerTask<T> : AsyncTask<T> {
			public weak Compiler parent {
				get;
				construct;
			}
		}
	}

	/**
	 * Speaks the Language Server Protocol for a TypeScript or JavaScript
	 * project, powered by the same compiler as {@link Compiler}.
	 */
	public sealed class LanguageServer : Object {
		/**
		 * Emitted with each message from the server, as JSON-RPC.
		 *
		 * @param json the message
		 */
		public signal void message (string json);

		/**
		 * The project root directory.
		 */
		public string project_root {
			get;
			construct;
		}

		private size_t handle = 0;

		private MainContext main_context;

		/**
		 * Creates a language server for the project rooted at @project_root.
		 *
		 * @param project_root path to the project root directory
		 */
		public LanguageServer (string project_root) {
			Object (project_root: project_root);
		}

		static construct {
			CompilerBackend.init ();
		}

		construct {
			main_context = Frida.get_main_context ();
		}

		~LanguageServer () {
			stop ();
		}

		/**
		 * Starts the server, after which messages may be posted to it.
		 */
		public async void start (Cancellable? cancellable = null) throws Error, IOError {
			CompilerBackend.check_available ();

			size_t h = 0;
			string? error_message = null;
			CompilerBackend.LanguageServerReadyFunc on_ready = (handle, e) => {
				h = handle;
				error_message = e;
				schedule_on_frida_thread (start.callback);
			};

			CompilerBackend.LanguageServer.open (project_root, on_message, (owned) on_ready);
			yield;

			if (error_message != null)
				throw new Error.INVALID_ARGUMENT ("%s", error_message);

			handle = h;
		}

		public void start_sync (Cancellable? cancellable = null) throws Error, IOError {
			var task = create<StartTask> ();
			task.execute (cancellable);
		}

		private class StartTask : LanguageServerTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.start (cancellable);
			}
		}

		/**
		 * Stops the server. Send it the LSP shutdown and exit messages first
		 * to let it wind down its projects.
		 */
		public void stop () {
			if (handle == 0)
				return;
			CompilerBackend.LanguageServer.close (handle);
			handle = 0;
		}

		/**
		 * Posts a message to the server.
		 *
		 * @param json the JSON-RPC message
		 */
		public void post (string json) throws Error {
			if (handle == 0)
				throw new Error.INVALID_OPERATION ("Language server not started");

			string? error_message = CompilerBackend.LanguageServer.post (handle, json);
			if (error_message != null)
				throw new Error.PROTOCOL ("%s", error_message);
		}

		private void on_message (string json) {
			string json_copy = json;
			schedule_on_frida_thread (() => {
				message (json_copy);
				return Source.REMOVE;
			});
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}

		private abstract class LanguageServerTask<T> : AsyncTask<T> {
			public weak LanguageServer parent {
				get;
				construct;
			}
		}
	}

	namespace CompilerBackend {
		private void init () {
			if (initialized)
				return;
			initialized = true;

#if HAVE_COMPILER_BACKEND
#if COMPILER_BACKEND_LINKED
			_init_go_runtime ();
			build = (BuildFunc) _build;
			watch = (WatchFunc) _watch;
			WatchSession.dispose = (WatchSession.DisposeFunc) WatchSession._dispose;
			LanguageServer.open = (LanguageServer.OpenFunc) LanguageServer._open;
			LanguageServer.close = (LanguageServer.CloseFunc) LanguageServer._close;
			LanguageServer.post = (LanguageServer.PostFunc) LanguageServer._post;
#elif COMPILER_BACKEND_INSTALLED_LIBRARY
			Module? backend = null;
			try {
				backend = new Module (Frida.compiler_backend_path, LOCAL);
			} catch (ModuleError e) {
				return;
			}
			backend.make_resident ();

			build = resolve_symbol (backend, "_frida_compiler_backend_build");
			watch = resolve_symbol (backend, "_frida_compiler_backend_watch");
			WatchSession.dispose = resolve_symbol (backend, "_frida_compiler_backend_watch_session_dispose");
			LanguageServer.open = resolve_symbol (backend, "_frida_compiler_backend_language_server_open");
			LanguageServer.close = resolve_symbol (backend, "_frida_compiler_backend_language_server_close");
			LanguageServer.post = resolve_symbol (backend, "_frida_compiler_backend_language_server_post");
#elif COMPILER_BACKEND_EMBEDDED_LIBRARY
			unowned uint8[] backend_so = Frida.Data.Compiler.get_frida_compiler_backend_so_blob ().data;

			Module? backend = null;

			if (MemoryFileDescriptor.is_supported ()) {
				var fd = MemoryFileDescriptor.from_bytes ("frida-compiler-backend.so", new Bytes.static (backend_so));
				try {
					backend = new Module ("/proc/self/fd/%d".printf (fd.handle), LOCAL);
				} catch (ModuleError e) {
				}
			}

			if (backend == null) {
				try {
					string name_used;
					{
						var fd = new FileDescriptor (FileUtils.open_tmp ("frida-compiler-backend-XXXXXX.so", out name_used));
						fd.pwrite_all (backend_so, 0);
					}

					try {
						backend = new Module (name_used, LOCAL);
					} catch (ModuleError e) {
						assert_not_reached ();
					}

					FileUtils.unlink (name_used);
				} catch (GLib.Error e) {
					assert_not_reached ();
				}
			}
			backend.make_resident ();

			build = resolve_symbol (backend, "_frida_compiler_backend_build");
			watch = resolve_symbol (backend, "_frida_compiler_backend_watch");
			WatchSession.dispose = resolve_symbol (backend, "_frida_compiler_backend_watch_session_dispose");
			LanguageServer.open = resolve_symbol (backend, "_frida_compiler_backend_language_server_open");
			LanguageServer.close = resolve_symbol (backend, "_frida_compiler_backend_language_server_close");
			LanguageServer.post = resolve_symbol (backend, "_frida_compiler_backend_language_server_post");
#elif COMPILER_BACKEND_EMBEDDED_EXECUTABLE || COMPILER_BACKEND_INSTALLED_EXECUTABLE
			backend_process = new BackendProcess ();

			build = executable_build;
			watch = executable_watch;
			WatchSession.dispose = executable_watch_session_dispose;
			LanguageServer.open = executable_language_server_open;
			LanguageServer.close = executable_language_server_close;
			LanguageServer.post = executable_language_server_post;
#endif
#endif
		}

		private void check_available () throws Error {
			if (build == null) {
#if COMPILER_BACKEND_INSTALLED_LIBRARY || COMPILER_BACKEND_INSTALLED_EXECUTABLE
				throw new Error.NOT_SUPPORTED (
					"Compiler backend plugin not installed; expected at: %s",
					Frida.compiler_backend_path);
#else
				throw new Error.NOT_SUPPORTED ("Compiler backend disabled at build-time");
#endif
			}
		}

		private bool initialized = false;
		private BuildFunc? build;
		private WatchFunc? watch;

		[CCode (has_target = false)]
		private delegate void BuildFunc (string project_root, string entrypoint, OutputFormat output_format,
			BundleFormat bundle_format, size_t disable_type_check, size_t source_map, size_t compress,
			string platform, string[] externals, DiagnosticFunc on_diagnostic, owned BuildCompleteFunc on_complete);

		[CCode (has_target = false)]
		private delegate void WatchFunc (string project_root, string entrypoint, OutputFormat output_format,
			BundleFormat bundle_format, size_t disable_type_check, size_t source_map, size_t compress,
			string platform, string[] externals, StartingFunc on_starting, FinishedFunc on_finished,
			OutputFunc on_output, DiagnosticFunc on_diagnostic, owned WatchReadyFunc on_ready);

#if COMPILER_BACKEND_LINKED
		private extern void _init_go_runtime ();
		private extern void _build ();
		private extern void _watch ();
#endif

		namespace WatchSession {
			[CCode (has_target = false)]
			private delegate void DisposeFunc (size_t handle);

			private DisposeFunc? dispose;

#if COMPILER_BACKEND_LINKED
			private extern void _dispose ();
#endif
		}

		namespace LanguageServer {
			[CCode (has_target = false)]
			private delegate void OpenFunc (string project_root, LanguageServerMessageFunc on_message,
				owned LanguageServerReadyFunc on_ready);

			[CCode (has_target = false)]
			private delegate void CloseFunc (size_t handle);

			[CCode (has_target = false)]
			private delegate string? PostFunc (size_t handle, string json);

			private OpenFunc? open;
			private CloseFunc? close;
			private PostFunc? post;

#if COMPILER_BACKEND_LINKED
			private extern void _open ();
			private extern void _close ();
			private extern void _post ();
#endif
		}

		private delegate void BuildCompleteFunc (string? bundle, string? error_message);
		private delegate void WatchReadyFunc (size_t session_handle, string? error_message);
		private delegate void StartingFunc ();
		private delegate void FinishedFunc ();
		private delegate void OutputFunc (string bundle);
		private delegate void DiagnosticFunc (string category, int code, string? path, int line, int character,
			string text);
		private delegate void LanguageServerReadyFunc (size_t handle, string? error_message);
		private delegate void LanguageServerMessageFunc (string json);

#if HAVE_COMPILER_BACKEND && (COMPILER_BACKEND_EMBEDDED_LIBRARY || COMPILER_BACKEND_INSTALLED_LIBRARY)
		private T resolve_symbol<T> (Module m, string name) {
			void * address;
			if (!m.symbol (name, out address))
				assert_not_reached ();
			return (T) address;
		}
#endif

#if COMPILER_BACKEND_EMBEDDED_EXECUTABLE || COMPILER_BACKEND_INSTALLED_EXECUTABLE
		private BackendProcess? backend_process;

		private static void executable_build (string project_root, string entrypoint, OutputFormat output_format,
				BundleFormat bundle_format, size_t disable_type_check, size_t source_map, size_t compress,
				string platform, string[] externals, DiagnosticFunc on_diagnostic, owned BuildCompleteFunc on_complete) {
			backend_process.build (project_root, entrypoint, output_format, bundle_format, disable_type_check, source_map,
				compress, platform, externals, on_diagnostic, (owned) on_complete);
		}

		private static void executable_watch (string project_root, string entrypoint, OutputFormat output_format,
				BundleFormat bundle_format, size_t disable_type_check, size_t source_map, size_t compress,
				string platform, string[] externals, StartingFunc on_starting, FinishedFunc on_finished,
				OutputFunc on_output, DiagnosticFunc on_diagnostic, owned WatchReadyFunc on_ready) {
			backend_process.watch (project_root, entrypoint, output_format, bundle_format, disable_type_check, source_map,
				compress, platform, externals, on_starting, on_finished, on_output, on_diagnostic, (owned) on_ready);
		}

		private static void executable_watch_session_dispose (size_t handle) {
			backend_process.dispose_watch_session (handle);
		}

		private static void executable_language_server_open (string project_root, LanguageServerMessageFunc on_message,
				owned LanguageServerReadyFunc on_ready) {
			backend_process.open_language_server (project_root, on_message, (owned) on_ready);
		}

		private static void executable_language_server_close (size_t handle) {
			backend_process.close_language_server (handle);
		}

		private static string? executable_language_server_post (size_t handle, string json) {
			return backend_process.post_to_language_server (handle, json);
		}

		private class BackendProcess : Object {
			private Subprocess? process;
			private BufferedInputStream? input;
			private OutputStream? output;
			private DataInputStream? errput;

			private ByteArray pending_output = new ByteArray ();
			private bool writing = false;

			private uint next_request_id = 1;
			private uint next_session_id = 1;

			private Gee.Map<uint, PendingBuild> pending_builds = new Gee.HashMap<uint, PendingBuild> ();
			private Gee.Map<uint, WatchEntry> watches = new Gee.HashMap<uint, WatchEntry> ();
			private Gee.Map<uint, LanguageServerEntry> language_servers = new Gee.HashMap<uint, LanguageServerEntry> ();

			private Cancellable io_cancellable = new Cancellable ();

			construct {
				try_start ();
			}

			private void try_start () {
				try {
					ensure_started ();
				} catch (GLib.Error e) {
				}
			}

			private void ensure_started () throws GLib.Error {
				if (process != null)
					return;

#if COMPILER_BACKEND_INSTALLED_EXECUTABLE
				unowned string path = Frida.compiler_backend_path;
				bool unlink_after = false;
#else
				string path = extract_backend_executable ();
				bool unlink_after = true;
#endif
				try {
					var p = new Subprocess (STDIN_PIPE | STDOUT_PIPE | STDERR_PIPE, path);
					process = p;

					input = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
						"base-stream", p.get_stdout_pipe (),
						"close-base-stream", false,
						"buffer-size", 128 * 1024);
					output = p.get_stdin_pipe ();
					errput = new DataInputStream (p.get_stderr_pipe ());

					process_incoming_messages.begin ();
					process_stderr_stream.begin (errput);
				} finally {
					if (unlink_after)
						FileUtils.unlink (path);
				}
			}

#if COMPILER_BACKEND_EMBEDDED_EXECUTABLE
			private static string extract_backend_executable () throws GLib.Error {
				unowned uint8[] blob = Frida.Data.Compiler.get_frida_compiler_backend_blob ().data;

				string path;
				{
					var fd = new FileDescriptor (FileUtils.open_tmp ("frida-compiler-backend-XXXXXX", out path));
					fd.pwrite_all (blob, 0);
				}

				FileUtils.chmod (path, 0700);

				return path;
			}
#endif

			private void handle_process_failure (string message) {
				io_cancellable.cancel ();
				io_cancellable = new Cancellable ();

				if (process != null) {
					if (!process.get_if_exited ())
						process.force_exit ();
				}

				process = null;
				input = null;
				output = null;
				errput = null;

				foreach (var e in pending_builds.entries)
					e.value.on_complete (null, message);
				pending_builds.clear ();

				foreach (var e in watches.entries) {
					var entry = e.value;
					if (entry.on_ready != null)
						entry.on_ready (0, message);
				}
				watches.clear ();

				foreach (var e in language_servers.entries) {
					var entry = e.value;
					if (entry.on_ready != null)
						entry.on_ready (0, message);
				}
				language_servers.clear ();

				pending_output = new ByteArray ();
				writing = false;
			}

			public void build (string project_root, string entrypoint, OutputFormat output_format, BundleFormat bundle_format,
					size_t disable_type_check, size_t source_map, size_t compress, string platform, string[] externals,
					DiagnosticFunc on_diagnostic, owned BuildCompleteFunc on_complete) {
				try {
					ensure_started ();
				} catch (GLib.Error e) {
					on_complete (null, e.message);
					return;
				}

				uint request_id = allocate_request_id ();

				pending_builds[request_id] = new PendingBuild ((owned) on_complete, on_diagnostic);

				post_message (make_build_request (
					request_id,
					project_root,
					entrypoint,
					output_format,
					bundle_format,
					disable_type_check != 0,
					source_map != 0,
					compress != 0,
					platform,
					externals
				));
			}

			private class PendingBuild {
				public BuildCompleteFunc on_complete;
				public unowned DiagnosticFunc on_diagnostic;

				public PendingBuild (owned BuildCompleteFunc on_complete, DiagnosticFunc on_diagnostic) {
					this.on_complete = (owned) on_complete;
					this.on_diagnostic = on_diagnostic;
				}
			}

			public void watch (string project_root, string entrypoint, OutputFormat output_format, BundleFormat bundle_format,
					size_t disable_type_check, size_t source_map, size_t compress, string platform, string[] externals,
					StartingFunc on_starting, FinishedFunc on_finished, OutputFunc on_output,
					DiagnosticFunc on_diagnostic, owned WatchReadyFunc on_ready) {
				try {
					ensure_started ();
				} catch (GLib.Error e) {
					on_ready (0, e.message);
					return;
				}

				uint session_id = allocate_session_id ();

				watches[session_id] = new WatchEntry ((owned) on_ready, on_starting, on_finished, on_output, on_diagnostic);

				post_message (make_watch_request (
					session_id,
					project_root,
					entrypoint,
					output_format,
					bundle_format,
					disable_type_check != 0,
					source_map != 0,
					compress != 0,
					platform,
					externals
				));
			}

			private class WatchEntry {
				public WatchReadyFunc? on_ready;
				public unowned StartingFunc on_starting;
				public unowned FinishedFunc on_finished;
				public unowned OutputFunc on_output;
				public unowned DiagnosticFunc on_diagnostic;

				public WatchEntry (owned WatchReadyFunc on_ready, StartingFunc on_starting, FinishedFunc on_finished,
						OutputFunc on_output, DiagnosticFunc on_diagnostic) {
					this.on_ready = (owned) on_ready;
					this.on_starting = on_starting;
					this.on_finished = on_finished;
					this.on_output = on_output;
					this.on_diagnostic = on_diagnostic;
				}
			}

			public void dispose_watch_session (size_t handle) {
				uint session_id = (uint) handle;

				WatchEntry entry;
				if (!watches.unset (session_id, out entry))
					return;

				if (process != null)
					post_message (make_dispose_request (session_id));
			}

			public void open_language_server (string project_root, LanguageServerMessageFunc on_message,
					owned LanguageServerReadyFunc on_ready) {
				try {
					ensure_started ();
				} catch (GLib.Error e) {
					on_ready (0, e.message);
					return;
				}

				uint session_id = allocate_session_id ();

				language_servers[session_id] = new LanguageServerEntry ((owned) on_ready, on_message);

				post_message (make_language_server_open_request (session_id, project_root));
			}

			private class LanguageServerEntry {
				public LanguageServerReadyFunc? on_ready;
				public unowned LanguageServerMessageFunc on_message;

				public LanguageServerEntry (owned LanguageServerReadyFunc on_ready, LanguageServerMessageFunc on_message) {
					this.on_ready = (owned) on_ready;
					this.on_message = on_message;
				}
			}

			public void close_language_server (size_t handle) {
				uint session_id = (uint) handle;

				LanguageServerEntry entry;
				if (!language_servers.unset (session_id, out entry))
					return;

				if (process != null)
					post_message (make_language_server_close_request (session_id));
			}

			public string? post_to_language_server (size_t handle, string json) {
				uint session_id = (uint) handle;

				if (!language_servers.has_key (session_id) || process == null)
					return "Language server not running";

				post_message (make_language_server_post_request (session_id, json));

				return null;
			}

			private static string make_build_request (uint id, string project_root, string entrypoint,
					OutputFormat output_format, BundleFormat bundle_format, bool disable_type_check,
					bool source_map, bool compress, string platform, string[] externals) {
				return make_request ("build", "id", id, project_root, entrypoint, output_format, bundle_format,
					disable_type_check, source_map, compress, platform, externals);
			}

			private static string make_watch_request (uint session_id, string project_root, string entrypoint,
					OutputFormat output_format, BundleFormat bundle_format, bool disable_type_check,
					bool source_map, bool compress, string platform, string[] externals) {
				return make_request ("watch", "session_id", session_id, project_root, entrypoint, output_format,
					bundle_format, disable_type_check, source_map, compress, platform, externals);
			}

			private static string make_request (string type, string id_name, uint id, string project_root, string entrypoint,
					OutputFormat output_format, BundleFormat bundle_format, bool disable_type_check,
					bool source_map, bool compress, string platform, string[] externals) {
				var builder = new Json.Builder ();

				builder
					.begin_object ()
						.set_member_name ("type")
						.add_string_value (type)
						.set_member_name (id_name)
						.add_int_value (id)
						.set_member_name ("project_root")
						.add_string_value (project_root)
						.set_member_name ("entrypoint")
						.add_string_value (entrypoint)
						.set_member_name ("output_format")
						.add_string_value (output_format.to_nick ())
						.set_member_name ("bundle_format")
						.add_string_value (bundle_format.to_nick ())
						.set_member_name ("disable_type_check")
						.add_boolean_value (disable_type_check)
						.set_member_name ("source_map")
						.add_boolean_value (source_map)
						.set_member_name ("compress")
						.add_boolean_value (compress)
						.set_member_name ("platform")
						.add_string_value (platform)
						.set_member_name ("externals")
						.begin_array ();

				foreach (unowned string e in externals)
					builder.add_string_value (e);

				builder
						.end_array ()
					.end_object ();

				return Json.to_string (builder.get_root (), false);
			}

			private static string make_language_server_open_request (uint session_id, string project_root) {
				var builder = new Json.Builder ();

				builder
					.begin_object ()
						.set_member_name ("type")
						.add_string_value ("language-server:open")
						.set_member_name ("session_id")
						.add_int_value (session_id)
						.set_member_name ("project_root")
						.add_string_value (project_root)
					.end_object ();

				return Json.to_string (builder.get_root (), false);
			}

			private static string make_language_server_close_request (uint session_id) {
				var builder = new Json.Builder ();

				builder
					.begin_object ()
						.set_member_name ("type")
						.add_string_value ("language-server:close")
						.set_member_name ("session_id")
						.add_int_value (session_id)
					.end_object ();

				return Json.to_string (builder.get_root (), false);
			}

			private static string make_language_server_post_request (uint session_id, string json) {
				var builder = new Json.Builder ();

				builder
					.begin_object ()
						.set_member_name ("type")
						.add_string_value ("language-server:post")
						.set_member_name ("session_id")
						.add_int_value (session_id)
						.set_member_name ("text")
						.add_string_value (json)
					.end_object ();

				return Json.to_string (builder.get_root (), false);
			}

			private static string make_dispose_request (uint session_id) {
				var builder = new Json.Builder ();

				builder
					.begin_object ()
						.set_member_name ("type")
						.add_string_value ("dispose")
						.set_member_name ("session_id")
						.add_int_value (session_id)
					.end_object ();

				return Json.to_string (builder.get_root (), false);
			}

			private uint allocate_request_id () {
				uint start = next_request_id;

				do {
					uint id = next_request_id++;
					if (next_request_id == 0)
						next_request_id = 1;

					if (!pending_builds.has_key (id))
						return id;
				} while (next_request_id != start);

				assert_not_reached ();
			}

			private uint allocate_session_id () {
				uint start = next_session_id;

				do {
					uint id = next_session_id++;
					if (next_session_id == 0)
						next_session_id = 1;

					if (!watches.has_key (id) && !language_servers.has_key (id))
						return id;
				} while (next_session_id != start);

				assert_not_reached ();
			}

			private void post_message (string json) {
				unowned uint8[] raw_json = json.data;

				uint32 size = ((uint32) raw_json.length).to_big_endian ();
				pending_output.append ((uint8[]) &size);
				pending_output.append (raw_json);

				if (!writing) {
					writing = true;

					var source = new IdleSource ();
					source.set_callback (() => {
						process_pending_output.begin ();
						return false;
					});
					source.attach (MainContext.get_thread_default ());
				}
			}

			private async void process_pending_output () {
				while (pending_output.len > 0) {
					uint8[] batch = pending_output.steal ();

					size_t bytes_written;
					try {
						yield output.write_all_async (batch, Priority.DEFAULT, io_cancellable, out bytes_written);
					} catch (GLib.Error e) {
						handle_process_failure ("Compiler backend process terminated unexpectedly");
						return;
					}
				}

				writing = false;
			}

			private async void process_incoming_messages () {
				try {
					while (true) {
						size_t header_size = 4;
						if (input.get_available () < header_size)
							yield fill_until_n_bytes_available (header_size);

						uint32 body_size = 0;
						unowned uint8[] size_buf = ((uint8[]) &body_size)[:4];
						input.peek (size_buf);
						body_size = uint32.from_big_endian (body_size);

						size_t full_size = header_size + body_size;
						if (input.get_available () < full_size)
							yield fill_until_n_bytes_available (full_size);

						var raw_json = new uint8[body_size + 1];
						input.peek (raw_json[:body_size], header_size);

						unowned string json = (string) raw_json;

						handle_message (json);

						input.skip (full_size, io_cancellable);
					}
				} catch (GLib.Error e) {
					handle_process_failure (e.message);
				}
			}

			private void handle_message (string json) throws Error {
				Json.Reader reader;
				try {
					reader = make_json_reader (json);
				} catch (GLib.Error e) {
					throw new Error.PROTOCOL ("%s", e.message);
				}

				reader.read_member ("type");
				unowned string? type = reader.get_string_value ();
				if (type == null)
					throw new Error.PROTOCOL ("Missing or invalid 'type' value");
				reader.end_member ();

				var tokens = type.split (":", 2);
				if (tokens.length != 2)
					throw new Error.PROTOCOL ("Invalid 'type' value");

				unowned string scope = tokens[0];
				unowned string subtype = tokens[1];

				if (scope == "build")
					handle_build_message (subtype, reader);
				else if (scope == "watch")
					handle_watch_message (subtype, reader);
				else if (scope == "language-server")
					handle_language_server_message (subtype, reader);
				else
					throw new Error.PROTOCOL ("Unknown 'type' scope");
			}

			private void handle_build_message (string type, Json.Reader reader) throws Error {
				if (type == "complete")
					handle_build_complete_message (reader);
				else if (type == "diagnostic")
					handle_build_diagnostic_message (reader);
				else
					throw new Error.PROTOCOL ("Unknown build message type: %s", type);
			}

			private void handle_watch_message (string type, Json.Reader reader) throws Error {
				if (type == "ready")
					handle_watch_ready_message (reader);
				else if (type == "starting" || type == "finished" || type == "output" || type == "diagnostic")
					handle_watch_event_message (reader, type);
				else
					throw new Error.PROTOCOL ("Unknown watch message type: %s", type);
			}

			private void handle_language_server_message (string type, Json.Reader reader) throws Error {
				reader.read_member ("session_id");
				uint session_id = (uint) reader.get_int_value ();
				reader.end_member ();

				var server = language_servers[session_id];
				if (server == null)
					throw new Error.PROTOCOL ("Invalid language server session ID: %u", session_id);

				if (type == "ready") {
					if (reader.read_member ("error")) {
						unowned string? error = reader.get_string_value ();
						if (error == null)
							throw new Error.PROTOCOL ("Missing or invalid 'error' value");
						reader.end_member ();

						language_servers.unset (session_id);
						server.on_ready (0, error);
						return;
					}
					reader.end_member ();

					var on_ready = (owned) server.on_ready;
					if (on_ready == null)
						throw new Error.PROTOCOL ("Duplicate language-server:ready for session ID: %u", session_id);

					server.on_ready = null;
					on_ready (session_id, null);
					return;
				}

				if (type == "message") {
					reader.read_member ("text");
					unowned string? text = reader.get_string_value ();
					if (text == null)
						throw new Error.PROTOCOL ("Missing or invalid 'text' value");
					reader.end_member ();

					server.on_message (text);
					return;
				}

				if (type == "error") {
					reader.read_member ("error");
					unowned string? error = reader.get_string_value ();
					if (error == null)
						throw new Error.PROTOCOL ("Missing or invalid 'error' value");
					reader.end_member ();

					printerr ("[frida-compiler-backend] Language server: %s\n", error);
					return;
				}

				throw new Error.PROTOCOL ("Unknown language-server message type: %s", type);
			}

			private void handle_build_complete_message (Json.Reader reader) throws Error {
				reader.read_member ("id");
				uint id = (uint) reader.get_int_value ();
				reader.end_member ();

				PendingBuild pending;
				if (!pending_builds.unset (id, out pending))
					throw new Error.PROTOCOL ("Invalid pending build ID: %u", id);

				if (reader.read_member ("error")) {
					unowned string? error = reader.get_string_value ();
					if (error == null)
						throw new Error.PROTOCOL ("Missing or invalid 'error' value");
					reader.end_member ();

					pending.on_complete (null, error);
					return;
				}
				reader.end_member ();

				reader.read_member ("bundle");
				unowned string? bundle = reader.get_string_value ();
				if (bundle == null)
					throw new Error.PROTOCOL ("Missing or invalid 'bundle' value");
				reader.end_member ();

				pending.on_complete (bundle, null);
			}

			private void handle_build_diagnostic_message (Json.Reader reader) throws Error {
				reader.read_member ("id");
				uint id = (uint) reader.get_int_value ();
				reader.end_member ();

				var pending = pending_builds[id];
				if (pending == null)
					throw new Error.PROTOCOL ("Invalid pending build ID: %u", id);

				emit_diagnostic_from_json (reader, pending.on_diagnostic);
			}

			private void handle_watch_ready_message (Json.Reader reader) throws Error {
				reader.read_member ("session_id");
				uint session_id = (uint) reader.get_int_value ();
				reader.end_member ();

				var watch = watches[session_id];
				if (watch == null)
					throw new Error.PROTOCOL ("Invalid watch session ID: %u", session_id);

				if (reader.read_member ("error")) {
					unowned string? error = reader.get_string_value ();
					if (error == null)
						throw new Error.PROTOCOL ("Missing or invalid 'error' value");
					reader.end_member ();

					watches.unset (session_id);
					watch.on_ready (0, error);
					return;
				}
				reader.end_member ();

				var on_ready = (owned) watch.on_ready;
				if (on_ready == null)
					throw new Error.PROTOCOL ("Duplicate watch:ready for session ID: %u", session_id);

				watch.on_ready = null;
				on_ready (session_id, null);
			}

			private void handle_watch_event_message (Json.Reader reader, string type) throws Error {
				reader.read_member ("session_id");
				var session_id = (uint) reader.get_int_value ();
				reader.end_member ();

				var watch = watches[session_id];
				if (watch == null)
					throw new Error.PROTOCOL ("Invalid watch session ID: %u", session_id);

				if (type == "starting") {
					watch.on_starting ();
					return;
				}

				if (type == "finished") {
					watch.on_finished ();
					return;
				}

				if (type == "output") {
					reader.read_member ("bundle");
					unowned string? bundle = reader.get_string_value ();
					if (bundle == null)
						throw new Error.PROTOCOL ("Missing or invalid 'bundle' value");
					reader.end_member ();

					watch.on_output (bundle);
					return;
				}

				emit_diagnostic_from_json (reader, watch.on_diagnostic);
			}

			private static void emit_diagnostic_from_json (Json.Reader reader, DiagnosticFunc on_diagnostic) throws Error {
				reader.read_member ("category");
				unowned string? category = reader.get_string_value ();
				if (category == null)
					throw new Error.PROTOCOL ("Missing or invalid 'category' value");
				reader.end_member ();

				reader.read_member ("code");
				int code = (int) reader.get_int_value ();
				reader.end_member ();

				string? path = null;
				int line = 0;
				int character = 0;

				if (reader.read_member ("path")) {
					path = reader.get_string_value ();
					reader.end_member ();

					reader.read_member ("line");
					line = (int) reader.get_int_value ();
					reader.end_member ();

					reader.read_member ("character");
					character = (int) reader.get_int_value ();
					reader.end_member ();
				} else {
					reader.end_member ();
				}

				reader.read_member ("text");
				unowned string? text = reader.get_string_value ();
				if (text == null)
					throw new Error.PROTOCOL ("Missing or invalid 'text' value");
				reader.end_member ();

				on_diagnostic (category, code, path, line, character, text);
			}

			private async void process_stderr_stream (DataInputStream stream) {
				try {
					while (true) {
						string? line = yield stream.read_line_utf8_async (Priority.DEFAULT, io_cancellable);
						if (line == null)
							break;
						printerr ("[frida-compiler-backend stderr] %s\n", line);
					}
				} catch (GLib.Error e) {
				}
			}

			private async void fill_until_n_bytes_available (size_t minimum) throws Error, IOError {
				size_t available = input.get_available ();
				while (available < minimum) {
					if (input.get_buffer_size () < minimum)
						input.set_buffer_size (minimum);

					ssize_t n;
					try {
						n = yield input.fill_async ((ssize_t) (input.get_buffer_size () - available),
							Priority.DEFAULT, io_cancellable);
					} catch (GLib.Error e) {
						throw new Error.TRANSPORT ("Compiler backend process terminated unexpectedly");
					}

					if (n == 0)
						throw new Error.TRANSPORT ("Compiler backend process terminated unexpectedly");

					available += n;
				}
			}
		}
#endif
	}

	private string compute_project_root (string entrypoint, CompilerOptions options) {
		string? project_root = options.project_root;

		if (project_root != null)
			return project_root;

		if (Path.is_absolute (entrypoint))
			return Path.get_dirname (entrypoint);

		return Environment.get_current_dir ();
	}

	/**
	 * Options shared by {@link Compiler.build} and {@link Compiler.watch}.
	 */
	public class CompilerOptions : Object {
		/**
		 * The project root directory, or null to infer it from the entrypoint.
		 */
		public string? project_root {
			get;
			set;
		}

		/**
		 * How the resulting bundle's bytes are encoded.
		 */
		public OutputFormat output_format {
			get;
			set;
			default = UNESCAPED;
		}

		/**
		 * The module format of the bundle.
		 */
		public BundleFormat bundle_format {
			get;
			set;
			default = ESM;
		}

		/**
		 * Whether to type-check the project.
		 */
		public TypeCheckMode type_check {
			get;
			set;
			default = FULL;
		}

		/**
		 * Whether to include source maps in the bundle.
		 */
		public SourceMaps source_maps {
			get;
			set;
			default = INCLUDED;
		}

		/**
		 * Which compressor to run the output through, if any.
		 */
		public JsCompression compression {
			get;
			set;
			default = NONE;
		}

		/**
		 * Which platform the bundle targets.
		 */
		public JsPlatform platform {
			get;
			set;
			default = GUM;
		}

		internal Gee.List<string> externals = new Gee.ArrayList<string> ();

		/**
		 * Removes all configured externals.
		 */
		public void clear_externals () {
			externals.clear ();
		}

		/**
		 * Marks a module as external, leaving it out of the bundle.
		 *
		 * @param external the module specifier to treat as external
		 */
		public void add_external (string external) {
			externals.add (external);
		}

		/**
		 * Invokes @func for each configured external.
		 *
		 * @param func function called with each external
		 */
		public void enumerate_externals (Func<string> func) {
			foreach (var external in externals)
				func (external);
		}
	}

	/**
	 * Options for {@link Compiler.build}.
	 */
	public sealed class BuildOptions : CompilerOptions {
	}

	/**
	 * Options for {@link Compiler.watch}.
	 */
	public sealed class WatchOptions : CompilerOptions {
	}

	/**
	 * How a compiled bundle's bytes are encoded.
	 */
	public enum OutputFormat {
		/**
		 * Raw, unescaped JavaScript.
		 */
		UNESCAPED,
		/**
		 * A hex-encoded byte string.
		 */
		HEX_BYTES,
		/**
		 * A C string literal.
		 */
		C_STRING;

		public static OutputFormat from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<OutputFormat> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<OutputFormat> (this);
		}
	}

	/**
	 * The module format of a compiled bundle.
	 */
	public enum BundleFormat {
		/**
		 * An ECMAScript module.
		 */
		ESM,
		/**
		 * An immediately-invoked function expression.
		 */
		IIFE;

		public static BundleFormat from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<BundleFormat> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<BundleFormat> (this);
		}
	}

	/**
	 * Whether the compiler type-checks the project.
	 */
	public enum TypeCheckMode {
		/**
		 * Perform full type checking.
		 */
		FULL,
		/**
		 * Skip type checking.
		 */
		NONE;

		public static TypeCheckMode from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<TypeCheckMode> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<TypeCheckMode> (this);
		}
	}

	/**
	 * Whether source maps are included in a compiled bundle.
	 */
	public enum SourceMaps {
		/**
		 * Include source maps.
		 */
		INCLUDED,
		/**
		 * Omit source maps.
		 */
		OMITTED;

		public static SourceMaps from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<SourceMaps> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<SourceMaps> (this);
		}
	}

	/**
	 * Which compressor the compiled JavaScript is run through.
	 */
	public enum JsCompression {
		/**
		 * No compression.
		 */
		NONE,
		/**
		 * Compress with Terser.
		 */
		TERSER;

		public static JsCompression from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<JsCompression> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<JsCompression> (this);
		}
	}

	/**
	 * The platform a compiled bundle targets.
	 */
	public enum JsPlatform {
		/**
		 * Frida's Gum runtime inside a target process.
		 */
		GUM,
		/**
		 * A web browser.
		 */
		BROWSER,
		/**
		 * A platform-neutral environment.
		 */
		NEUTRAL;

		public static JsPlatform from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<JsPlatform> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<JsPlatform> (this);
		}
	}
}
