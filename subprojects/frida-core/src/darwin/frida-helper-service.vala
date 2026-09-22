namespace Frida {
	public int main (string[] args) {
		Posix.setsid ();

		Gum.init ();

		var parent_address = args[1];
		var worker = new Thread<int> ("frida-helper-main-loop", () => {
			var service = new DarwinHelperService (parent_address);

			var exit_code = service.run ();
			_stop_run_loop ();

			return exit_code;
		});
		_start_run_loop ();
		var exit_code = worker.join ();

		return exit_code;
	}

	public extern void _start_run_loop ();
	public extern void _stop_run_loop ();

	public sealed class DarwinHelperService : Object, DarwinRemoteHelper {
		public string parent_address {
			get;
			construct;
		}

		private MainLoop loop = new MainLoop ();
		private int run_result = 0;
		private Promise<bool> shutdown_request;

		private DBusConnection connection;
		private uint helper_registration_id = 0;

		private DarwinHelperBackend backend = new DarwinHelperBackend ();

		public DarwinHelperService (string parent_address) {
			Object (parent_address: parent_address);
		}

		construct {
			backend.idle.connect (on_backend_idle);
			backend.output.connect (on_backend_output);
			backend.gating_cancelled.connect (on_backend_gating_cancelled);
			backend.spawn_added.connect (on_backend_spawn_added);
			backend.spawn_removed.connect (on_backend_spawn_removed);
			backend.injected.connect (on_backend_injected);
			backend.uninjected.connect (on_backend_uninjected);
			backend.process_resumed.connect (on_backend_process_resumed);
			backend.process_killed.connect (on_backend_process_killed);
		}

		public int run () {
			Idle.add (() => {
				start.begin ();
				return false;
			});

			loop.run ();

			return run_result;
		}

		private async void shutdown () {
			if (shutdown_request != null) {
				try {
					yield shutdown_request.future.wait_async (null);
				} catch (GLib.Error e) {
					assert_not_reached ();
				}
				return;
			}
			shutdown_request = new Promise<bool> ();

			if (connection != null) {
				if (helper_registration_id != 0)
					connection.unregister_object (helper_registration_id);

				connection.on_closed.disconnect (on_connection_closed);
				try {
					yield connection.close ();
				} catch (GLib.Error connection_error) {
				}
				connection = null;
			}

			try {
				yield backend.close (null);
			} catch (IOError e) {
				assert_not_reached ();
			}
			backend.idle.disconnect (on_backend_idle);
			backend.output.disconnect (on_backend_output);
			backend.gating_cancelled.disconnect (on_backend_gating_cancelled);
			backend.spawn_added.disconnect (on_backend_spawn_added);
			backend.spawn_removed.disconnect (on_backend_spawn_removed);
			backend.injected.disconnect (on_backend_injected);
			backend.uninjected.disconnect (on_backend_uninjected);
			backend.process_resumed.disconnect (on_backend_process_resumed);
			backend.process_killed.disconnect (on_backend_process_killed);
			backend = null;

			shutdown_request.resolve (true);

			Idle.add (() => {
				loop.quit ();
				return false;
			});
		}

		private async void start () {
			try {
				int fd = int.parse (parent_address.substring ("socket-fd:".length));
				var sock = new Socket.from_fd (fd);
				IOStream stream = SocketConnection.factory_create_connection (sock);

				connection = yield new DBusConnection (stream, null,
					AUTHENTICATION_CLIENT | DELAY_MESSAGE_PROCESSING);
				connection.on_closed.connect (on_connection_closed);

				DarwinRemoteHelper helper = this;
				helper_registration_id = connection.register_object (Frida.ObjectPath.HELPER, helper);

				connection.start_message_processing ();
			} catch (GLib.Error e) {
				printerr ("Unable to start: %s\n", e.message);
				run_result = 1;
				shutdown.begin ();
			}
		}

		public async void stop (Cancellable? cancellable) throws Error, IOError {
			Timeout.add (20, () => {
				shutdown.begin ();
				return false;
			});
		}

		private void on_backend_idle () {
			if (connection.is_closed ())
				shutdown.begin ();
		}

		private void on_connection_closed (bool remote_peer_vanished, GLib.Error? error) {
			if (backend.is_idle)
				shutdown.begin ();
		}

		public async void enable_spawn_gating (SpawnGatingScope scope, Cancellable? cancellable) throws Error, IOError {
			yield backend.enable_spawn_gating (scope, cancellable);
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield backend.disable_spawn_gating (cancellable);
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			return yield backend.enumerate_pending_spawn (cancellable);
		}

		public async uint spawn (string path, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			return yield backend.spawn (path, options, null, null, null, cancellable);
		}

		public async uint spawn_with_stdio (string path, HostSpawnOptions options, UnixInputStream stdin_stream,
				UnixOutputStream stdout_stream, UnixOutputStream stderr_stream, Cancellable? cancellable) throws Error, IOError {
			return yield backend.spawn (path, options, stdin_stream, stdout_stream, stderr_stream, cancellable);
		}

		public async void launch (string identifier, HostSpawnOptions options, Cancellable? cancellable) throws Error, IOError {
			yield backend.launch (identifier, options, cancellable);
		}

		public async void notify_launch_completed (string identifier, uint pid, Cancellable? cancellable) throws Error, IOError {
			yield backend.notify_launch_completed (identifier, pid, cancellable);
		}

		public async void notify_exec_completed (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield backend.notify_exec_completed (pid, cancellable);
		}

		public async void wait_until_suspended (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield backend.wait_until_suspended (pid, cancellable);
		}

		public async void cancel_pending_waits (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield backend.cancel_pending_waits (pid, cancellable);
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			yield backend.input (pid, data, cancellable);
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield backend.resume (pid, cancellable);
		}

		public async void kill_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield backend.kill_process (pid, cancellable);
		}

		public async void kill_application (string identifier, Cancellable? cancellable) throws Error, IOError {
			yield backend.kill_application (identifier, cancellable);
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			return yield backend.inject_library_file (pid, path, entrypoint, data, cancellable);
		}

		public async uint inject_library_blob (uint pid, string name, MappedLibraryBlob blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			return yield backend.inject_library_blob (pid, name, blob, entrypoint, data, cancellable);
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			yield backend.demonitor (id, cancellable);
		}

		public async uint demonitor_and_clone_injectee_state (uint id, Cancellable? cancellable) throws Error, IOError {
			return yield backend.demonitor_and_clone_injectee_state (id, cancellable);
		}

		public async void recreate_injectee_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			yield backend.recreate_injectee_thread (pid, id, cancellable);
		}

		public async void transfer_socket (uint pid, GLib.Socket sock, Cancellable? cancellable, out string remote_address)
				throws Error, IOError {
			yield backend.prepare_target (pid, cancellable);

			var task = DarwinHelperBackend.task_for_pid (pid);
			try {
				DarwinHelperBackend.make_pipe_endpoint_from_socket (pid, task, sock, out remote_address);
			} finally {
				DarwinHelperBackend.deallocate_port (task);
			}
		}

		private void on_backend_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, data);
		}

		private void on_backend_gating_cancelled () {
			gating_cancelled ();
		}

		private void on_backend_spawn_added (HostSpawnInfo info) {
			spawn_added (info);
		}

		private void on_backend_spawn_removed (HostSpawnInfo info) {
			spawn_removed (info);
		}

		private void on_backend_injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module) {
			injected (id, pid, has_mapped_module, mapped_module);
		}

		private void on_backend_uninjected (uint id) {
			uninjected (id);
		}

		private void on_backend_process_resumed (uint pid) {
			process_resumed (pid);
		}

		private void on_backend_process_killed (uint pid) {
			process_killed (pid);
		}
	}
}
