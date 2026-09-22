namespace Frida {
	public sealed class DarwinHelperProcess : Object, DarwinHelper {
		public uint pid {
			get {
				return (uint) process_pid;
			}
		}

		public TemporaryDirectory tempdir {
			get;
			construct;
		}

		private ResourceStore _resource_store;

		private Pid process_pid;
		private DBusConnection connection;
		private DarwinRemoteHelper proxy;
		private Promise<DarwinRemoteHelper> obtain_request;

		public DarwinHelperProcess (TemporaryDirectory tempdir) {
			Object (tempdir: tempdir);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (proxy != null) {
				try {
					yield proxy.stop (cancellable);
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED)
						throw (IOError) e;
				}
			}

			if (connection != null) {
				try {
					yield connection.close (cancellable);
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED)
						throw (IOError) e;
				}
			}

			process_pid = 0;

			_resource_store = null;
		}

		private ResourceStore get_resource_store () throws Error {
			if (_resource_store == null)
				_resource_store = new ResourceStore (tempdir);
			return _resource_store;
		}

		public async void preload (Cancellable? cancellable) throws Error, IOError {
			yield obtain (cancellable);
		}

		public async void enable_spawn_gating (SpawnGatingScope scope, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.enable_spawn_gating (scope, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.disable_spawn_gating (cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				return yield helper.enumerate_pending_spawn (cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async uint spawn (string path, HostSpawnOptions options, UnixInputStream? stdin_stream,
				UnixOutputStream? stdout_stream, UnixOutputStream? stderr_stream, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				if (stdin_stream != null)
					return yield helper.spawn_with_stdio (path, options, stdin_stream, stdout_stream, stderr_stream, cancellable);
				return yield helper.spawn (path, options, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void launch (string identifier, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.launch (identifier, options, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void notify_launch_completed (string identifier, uint pid, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.notify_launch_completed (identifier, pid, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void notify_exec_completed (uint pid, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.notify_exec_completed (pid, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void wait_until_suspended (uint pid, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.wait_until_suspended (pid, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void cancel_pending_waits (uint pid, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.cancel_pending_waits (pid, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.input (pid, data, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.resume (pid, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void kill_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.kill_process (pid, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void kill_application (string identifier, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.kill_application (identifier, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				return yield helper.inject_library_file (pid, path, entrypoint, data, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async uint inject_library_blob (uint pid, string name, MappedLibraryBlob blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				return yield helper.inject_library_blob (pid, name, blob, entrypoint, data, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.demonitor (id, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async uint demonitor_and_clone_injectee_state (uint id, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				return yield helper.demonitor_and_clone_injectee_state (id, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			var helper = yield obtain (cancellable);
			try {
				yield helper.recreate_injectee_thread (pid, id, cancellable);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}
		}

		public async Future<IOStream> open_pipe_stream (uint remote_pid, Cancellable? cancellable, out string remote_address)
				throws Error, IOError {
			var result = new Promise<IOStream> ();

			var helper = yield obtain (cancellable);

			var fds = new int[2];
			Posix.socketpair (Posix.AF_UNIX, Posix.SOCK_STREAM, 0, fds);

			UnixSocket.tune_buffer_sizes (fds[0]);
			UnixSocket.tune_buffer_sizes (fds[1]);

			Socket local_socket, remote_socket;
			try {
				local_socket = new Socket.from_fd (fds[0]);
				remote_socket = new Socket.from_fd (fds[1]);

				var local_stream = SocketConnection.factory_create_connection (local_socket);
				result.resolve (local_stream);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			try {
				yield helper.transfer_socket (remote_pid, remote_socket, cancellable, out remote_address);
			} catch (GLib.Error e) {
				throw_helper_error (e);
			}

			return result.future;
		}

		[NoReturn]
		private static void throw_helper_error (GLib.Error e) throws Error, IOError {
#if MACOS
			if (e is IOError.CLOSED) {
				throw new Error.PERMISSION_DENIED ("Oops, frida-helper appears to have crashed. It may have been killed " +
					"by the system while trying to access a hardened process. If this is the case, try setting these " +
					"boot arguments: `sudo nvram boot-args=\"-arm64e_preview_abi thid_should_crash=0 " +
					"tss_should_crash=0\"`. For more information, see: https://github.com/frida/frida-core/issues/524");
			}
#endif

			throw_dbus_error (e);
		}

		public async MappedLibraryBlob? try_mmap (Bytes blob, Cancellable? cancellable) throws Error, IOError {
			return null;
		}

		private async DarwinRemoteHelper obtain (Cancellable? cancellable) throws Error, IOError {
			while (obtain_request != null) {
				try {
					return yield obtain_request.future.wait_async (cancellable);
				} catch (Error e) {
					throw e;
				} catch (IOError e) {
					cancellable.set_error_if_cancelled ();
				}
			}
			obtain_request = new Promise<DarwinRemoteHelper> ();

			try {
				var proxy = yield launch_helper (cancellable);
				obtain_request.resolve (proxy);
				return proxy;
			} catch (GLib.Error e) {
				if (e is Error.PROCESS_NOT_FOUND && get_resource_store ().maybe_thin_helper_to_basic_abi ()) {
					try {
						var proxy = yield launch_helper (cancellable);
						obtain_request.resolve (proxy);
						return proxy;
					} catch (GLib.Error e) {
						obtain_request.reject (e);
						obtain_request = null;
						throw_api_error (e);
					}
				}

				obtain_request.reject (e);
				obtain_request = null;
				throw_api_error (e);
			}
		}

		private async DarwinRemoteHelper launch_helper (Cancellable? cancellable) throws Error, IOError {
			Pid pending_pid = 0;
			IOStream? pending_stream = null;
			DBusConnection pending_connection = null;
			DarwinRemoteHelper pending_proxy = null;

			bool child_exited = false;
			int child_status = 0;
			ChildExitMonitor? exit_monitor = null;
			ulong exit_handler = 0;

			DBusConnection? established = null;
			GLib.Error? handshake_error = null;
			bool handshake_done = false;
			bool waiting = false;

			try {
				HelperFile helper_file = get_resource_store ().helper;
				string helper_path = helper_file.path;

				int sv[2];
				if (Posix.socketpair (Posix.AF_UNIX, Posix.SOCK_STREAM, 0, sv) != 0)
					throw new Error.NOT_SUPPORTED ("Unable to allocate socketpair: %s", strerror (errno));
				var parent_fd = new FileDescriptor (sv[0]);
				var peer_fd = new FileDescriptor (sv[1]);

				Socket parent_socket = new Socket.from_fd (parent_fd.steal ());
				pending_stream = SocketConnection.factory_create_connection (parent_socket);

				string socket_address = "socket-fd:%d".printf (peer_fd.handle);
				string[] argv = { helper_path, socket_address };

				GLib.SpawnFlags flags =
					GLib.SpawnFlags.LEAVE_DESCRIPTORS_OPEN |
					GLib.SpawnFlags.DO_NOT_REAP_CHILD |
					GLib.SpawnFlags.STDOUT_TO_DEV_NULL |
					GLib.SpawnFlags.STDERR_TO_DEV_NULL |
					/* GLib.SpawnFlags.STDIN_FROM_DEV_NULL */ 2048 |
					/* GLib.SpawnFlags.CLOEXEC_PIPES */ 256;

				GLib.Process.spawn_async (null, argv, null, flags, null, out pending_pid);

				peer_fd = null;

				GLib.SourceFunc resume = launch_helper.callback;

				var handshake_cancellable = new Cancellable ();
				ulong cancel_chain = 0;
				if (cancellable != null) {
					cancel_chain = cancellable.cancelled.connect (() => {
						handshake_cancellable.cancel ();
					});
				}

				var main_context = MainContext.get_thread_default ();
				exit_monitor = new ChildExitMonitor (pending_pid, main_context);
				exit_handler = exit_monitor.exited.connect ((status) => {
					child_exited = true;
					child_status = status;
					handshake_cancellable.cancel ();
				});

				establish_dbus_connection.begin (pending_stream, handshake_cancellable, (obj, res) => {
					try {
						established = establish_dbus_connection.end (res);
					} catch (GLib.Error e) {
						handshake_error = e;
					}
					handshake_done = true;
					if (waiting)
						resume ();
				});

				waiting = true;
				while (!handshake_done)
					yield;
				waiting = false;

				if (cancel_chain != 0)
					cancellable.disconnect (cancel_chain);

				if (handshake_error != null && !child_exited)
					exit_monitor.poll ();

				if (child_exited)
					throw new Error.PROCESS_NOT_FOUND ("Helper exited during launch (status=%d)", child_status);
				if (handshake_error != null)
					throw handshake_error;

				pending_connection = established;
				pending_proxy = yield pending_connection.get_proxy (null, ObjectPath.HELPER, DO_NOT_LOAD_PROPERTIES,
					cancellable);

				if (pending_connection.is_closed ())
					throw new Error.NOT_SUPPORTED ("Helper terminated prematurely");
			} catch (GLib.Error e) {
				if (pending_pid != 0 && !child_exited) {
					Posix.kill ((Posix.pid_t) pending_pid, Posix.Signal.KILL);
					reap_later.begin (pending_pid);
				}

				if (e is Error)
					throw (Error) e;
				throw new Error.PERMISSION_DENIED ("%s", e.message);
			} finally {
				if (exit_monitor != null) {
					exit_monitor.disconnect (exit_handler);
					exit_monitor.stop ();
				}
			}

			process_pid = pending_pid;

			connection = pending_connection;
			connection.on_closed.connect (on_connection_closed);

			proxy = pending_proxy;
			proxy.output.connect (on_output);
			proxy.gating_cancelled.connect (on_gating_cancelled);
			proxy.spawn_added.connect (on_spawn_added);
			proxy.spawn_removed.connect (on_spawn_removed);
			proxy.injected.connect (on_injected);
			proxy.uninjected.connect (on_uninjected);
			proxy.process_resumed.connect (on_process_resumed);
			proxy.process_killed.connect (on_process_killed);

			return proxy;
		}

		private async DBusConnection establish_dbus_connection (IOStream stream, Cancellable? cancellable) throws GLib.Error {
			return yield new DBusConnection (stream, ServerGuid.HOST_SESSION_SERVICE,
				AUTHENTICATION_SERVER | AUTHENTICATION_ALLOW_ANONYMOUS, null, cancellable);
		}

		private void on_connection_closed (bool remote_peer_vanished, GLib.Error? error) {
			obtain_request = null;

			proxy.output.disconnect (on_output);
			proxy.gating_cancelled.disconnect (on_gating_cancelled);
			proxy.spawn_added.disconnect (on_spawn_added);
			proxy.spawn_removed.disconnect (on_spawn_removed);
			proxy.injected.disconnect (on_injected);
			proxy.uninjected.disconnect (on_uninjected);
			proxy.process_resumed.disconnect (on_process_resumed);
			proxy.process_killed.disconnect (on_process_killed);
			proxy = null;

			connection.on_closed.disconnect (on_connection_closed);
			connection = null;

			Pid pid = process_pid;
			process_pid = 0;
			if (pid != 0)
				reap_later.begin (pid);
		}

		private void on_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, data);
		}

		private void on_gating_cancelled () {
			gating_cancelled ();
		}

		private void on_spawn_added (HostSpawnInfo info) {
			spawn_added (info);
		}

		private void on_spawn_removed (HostSpawnInfo info) {
			spawn_removed (info);
		}

		private void on_injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module) {
			injected (id, pid, has_mapped_module, mapped_module);
		}

		private void on_uninjected (uint id) {
			uninjected (id);
		}

		private void on_process_resumed (uint pid) {
			process_resumed (pid);
		}

		private void on_process_killed (uint pid) {
			process_killed (pid);
		}
	}

	private sealed class ResourceStore {
		public HelperFile helper {
			get;
			private set;
		}

		private TemporaryDirectory tempdir;

#if MACOS && ARM64
		private bool thinned = false;
#endif

		public ResourceStore (TemporaryDirectory tempdir) throws Error {
			this.tempdir = tempdir;
#if HAVE_EMBEDDED_ASSETS
			FileUtils.chmod (tempdir.path, 0755);

			var blob = Frida.Data.Helper.get_frida_helper_blob ();
			helper = new TemporaryHelperFile (
				new TemporaryFile.from_stream ("frida-helper",
					new MemoryInputStream.from_data (blob.data, null),
					tempdir));
			FileUtils.chmod (helper.path, 0700);
#else
			helper = new InstalledHelperFile.for_path (Frida.helper_path);
#endif
		}

		~ResourceStore () {
			if (helper is TemporaryHelperFile)
				((TemporaryHelperFile) helper).file.destroy ();
		}

		public bool maybe_thin_helper_to_basic_abi () {
#if MACOS && ARM64
			if (thinned)
				return false;

			try {
				uint8[] universal_data;
				FileUtils.get_data (helper.path, out universal_data);

				var input = new DataInputStream (new MemoryInputStream.from_data (universal_data, null));
				input.byte_order = BIG_ENDIAN;

				const uint32 fat_magic = 0xcafebabeU;
				var magic = input.read_uint32 ();
				if (magic != fat_magic)
					return false;

				uint32 arm64e_offset = 0;

				uint32 arm64_offset = 0;
				uint32 arm64_size = 0;

				var nfat_arch = input.read_uint32 ();
				for (uint32 i = 0; i != nfat_arch; i++) {
					var cputype = input.read_uint32 ();
					var cpusubtype = input.read_uint32 ();
					var offset = input.read_uint32 ();
					var size = input.read_uint32 ();
					input.skip (4);

					bool is_arm64 = cputype == 0x0100000cU;
					bool is_arm64e = is_arm64 && (cpusubtype & 0x00ffffffU) == 2;
					if (is_arm64e) {
						arm64e_offset = offset;
					} else if (is_arm64) {
						arm64_offset = offset;
						arm64_size = size;
					}
				}

				if (arm64e_offset == 0 || arm64_offset == 0)
					return false;

				var thin_file = new TemporaryFile.from_stream ("frida-helper-arm64",
					new MemoryInputStream.from_data (universal_data[arm64_offset:arm64_offset + arm64_size], null),
					tempdir);
				FileUtils.chmod (thin_file.path, 0700);

				if (helper is TemporaryHelperFile)
					((TemporaryHelperFile) helper).file.destroy ();

				helper = new TemporaryHelperFile (thin_file);
				thinned = true;

				return true;
			} catch (GLib.Error e) {
			}
#endif

			return false;
		}
	}

	private class ChildExitMonitor : Object {
		public signal void exited (int status);

		private Pid pid;
		private MainContext? context;

		private TimeoutSource? source;

		private uint interval_ms = 10;
		private const uint MAX_INTERVAL_MS = 1000;

		public ChildExitMonitor (Pid pid, MainContext? ctx = null) {
			this.pid = pid;

			context = ctx ?? MainContext.get_thread_default ();

			arm ();
		}

		public void stop () {
			if (source != null) {
				source.destroy ();
				source = null;
			}
		}

		public bool poll () {
			if (source == null)
				return false;

			int status;
			if (!try_reap (pid, out status))
				return false;

			stop ();
			exited (status);
			return true;
		}

		private void arm () {
			source = new TimeoutSource (interval_ms);
			source.set_callback (() => {
				int status;
				if (try_reap (pid, out status)) {
					stop ();
					exited (status);
					return Source.REMOVE;
				}

				if (interval_ms < MAX_INTERVAL_MS) {
					interval_ms = uint.min (MAX_INTERVAL_MS, 2 * interval_ms);
					stop ();
					arm ();
					return Source.REMOVE;
				}

				return Source.CONTINUE;
			});
			source.attach (context);
		}

		private static bool try_reap (Pid pid, out int status) {
			status = 0;
			var r = Posix.waitpid ((Posix.pid_t) pid, out status, Posix.WNOHANG);
			if (r > 0)
				return true;
			if (r == 0)
				return false;

			if (Posix.errno == Posix.EINTR)
				return false;

			if (Posix.errno == Posix.ECHILD)
				return true;

			return true;
		}
	}

	private async void reap_later (Pid pid) {
		var monitor = new ChildExitMonitor (pid);

		bool done = false;
		ulong h = monitor.exited.connect ((status) => {
			done = true;
			reap_later.callback ();
		});

		try {
			while (!done)
				yield;
		} finally {
			monitor.disconnect (h);
			monitor.stop ();
		}
	}
}
