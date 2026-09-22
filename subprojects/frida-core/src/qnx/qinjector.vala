namespace Frida {
	public class Qinjector : Object, Injector {
		public string temp_directory {
			owned get {
				return resource_store.tempdir.path;
			}
		}

		public ResourceStore resource_store {
			get {
				if (_resource_store == null) {
					try {
						_resource_store = new ResourceStore ();
					} catch (Error e) {
						assert_not_reached ();
					}
				}
				return _resource_store;
			}
		}
		private ResourceStore _resource_store;

		/* these should be private, but must be accessible to glue code */
		private MainContext main_context;

		public Gee.HashMap<uint, void *> instances = new Gee.HashMap<uint, void *> ();
		private Gee.HashMap<uint, RemoteThreadSession> sessions = new Gee.HashMap<uint, RemoteThreadSession> ();
		public uint next_instance_id = 1;

		private Gee.HashMap<uint, TemporaryFile> blob_files = new Gee.HashMap<uint, TemporaryFile> ();
		private uint next_blob_id = 1;

		private Cancellable io_cancellable = new Cancellable ();

		construct {
			main_context = MainContext.ref_thread_default ();
		}

		~Qinjector () {
			foreach (var instance in instances.values)
				_free_instance (instance, RESIDENT);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			_resource_store = null;
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			var id = _do_inject (pid, path, entrypoint, data, resource_store.tempdir.path);

			yield establish_session (id, pid);

			return id;
		}

		public async uint inject_library_blob (uint pid, Bytes blob, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			var name = "blob%u.so".printf (next_blob_id++);
			var file = new TemporaryFile.from_stream (name, new MemoryInputStream.from_bytes (blob), resource_store.tempdir);
			var path = file.path;
			FileUtils.chmod (path, 0755);

			var id = yield inject_library_file (pid, path, entrypoint, data, cancellable);

			blob_files[id] = file;

			return id;
		}

		public async uint inject_library_resource (uint pid, AgentDescriptor descriptor, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var path = resource_store.ensure_copy_of (descriptor);
			return yield inject_library_file (pid, path, entrypoint, data, cancellable);
		}

		private async void establish_session (uint id, uint pid) throws Error {
			var session = new RemoteThreadSession (id, pid, instances[id]);
			try {
				yield session.establish ();
			} catch (Error e) {
				_destroy_instance (id, IMMEDIATE);
				throw e;
			}

			sessions[id] = session;
			session.ended.connect (on_remote_thread_session_ended);
		}

		private void on_remote_thread_session_ended (RemoteThreadSession session, UnloadPolicy unload_policy) {
			var id = session.id;

			session.ended.disconnect (on_remote_thread_session_ended);
			sessions.unset (id);

			_destroy_instance (id, unload_policy);
		}

		protected void _destroy_instance (uint id, UnloadPolicy unload_policy) {
			void * instance;
			bool found = instances.unset (id, out instance);
			assert (found);

			_free_instance (instance, unload_policy);

			blob_files.unset (id);

			uninjected (id);
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public async uint demonitor_and_clone_state (uint id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public async void recreate_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public bool any_still_injected () {
			return !instances.is_empty;
		}

		public bool is_still_injected (uint id) {
			return instances.has_key (id);
		}

		public extern void _free_instance (void * instance, UnloadPolicy unload_policy);
		public extern uint _do_inject (uint pid, string path, string entrypoint, string data, string temp_path) throws Error;

		public sealed class ResourceStore {
			public TemporaryDirectory tempdir {
				get;
				private set;
			}

			private Gee.HashMap<string, TemporaryFile> agents = new Gee.HashMap<string, TemporaryFile> ();

			public ResourceStore () throws Error {
				tempdir = new TemporaryDirectory ();
				FileUtils.chmod (tempdir.path, 0755);
			}

			~ResourceStore () {
				foreach (var tempfile in agents.values)
					tempfile.destroy ();
				tempdir.destroy ();
			}

			public string ensure_copy_of (AgentDescriptor desc) throws Error {
				var temp_agent = agents[desc.name];
				if (temp_agent == null) {
					temp_agent = new TemporaryFile.from_stream (desc.name, desc.sofile, tempdir);
					FileUtils.chmod (temp_agent.path, 0755);
					agents[desc.name] = temp_agent;
				}
				return temp_agent.path;
			}
		}
	}

	public sealed class AgentDescriptor : Object {
		public string name {
			get;
			construct;
		}

		public InputStream sofile {
			get {
				reset_stream (_sofile);
				return _sofile;
			}

			construct {
				_sofile = value;
			}
		}
		private InputStream _sofile;

		public AgentDescriptor (string name, InputStream sofile) {
			Object (name: name, sofile: sofile);

			assert (sofile is Seekable);
		}

		private void reset_stream (InputStream stream) {
			try {
				((Seekable) stream).seek (0, SeekType.SET);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
		}
	}

	private sealed class RemoteThreadSession : Object {
		public signal void ended (UnloadPolicy unload_policy);

		public uint id {
			get;
			construct;
		}

		public uint pid {
			get;
			construct;
		}

		public void * instance {
			get;
			construct;
		}

		private Thread<void>? worker;
		private PendingHello? pending_hello;
		private uint tid;
		private UnloadPolicy unload_policy = IMMEDIATE;

		private MainContext? main_context;

		public RemoteThreadSession (uint id, uint pid, void * instance) {
			Object (id: id, pid: pid, instance: instance);
		}

		construct {
			main_context = MainContext.get_thread_default ();
		}

		public async void establish () throws Error {
			assert (pending_hello == null);
			pending_hello = new PendingHello (establish.callback);

			bool timed_out = false;
			var timeout_source = new TimeoutSource.seconds (2);
			timeout_source.set_callback (() => {
				timed_out = true;
				establish.callback ();
				return false;
			});
			timeout_source.attach (main_context);

			assert (worker == null);
			worker = new Thread<void> ("pulse-reader", process_io);

			yield;

			timeout_source.destroy ();
			pending_hello = null;

			if (timed_out)
				throw new Error.PROCESS_NOT_RESPONDING ("Unexpectedly timed out while waiting for pulse to arrive");
		}

		private void process_io () {
			while (true) {
				try {
					QnxPulseCode code;
					int val;
					_receive_pulse (instance, out code, out val);

					var source = new IdleSource ();
					source.set_callback (() => {
						switch (code) {
							case HELLO:
								on_hello_received (val);
								break;
							case BYE:
								on_bye_received ((UnloadPolicy) val);
								break;
							case DISCONNECT:
								on_disconnect_received ();
								break;
						}
						return false;
					});
					source.attach (main_context);
				} catch (Error e) {
					break;
				}
			}
		}

		private void on_hello_received (uint tid) {
			this.tid = tid;

			if (pending_hello != null) {
				var hello = pending_hello;
				hello.complete ();
			}
		}

		private void on_bye_received (UnloadPolicy unload_policy) {
			this.unload_policy = unload_policy;
		}

		private void on_disconnect_received () {
			if (pending_hello != null) {
				// The DISCONNECT pulse is higher priority than HELLO, so defer handling a bit.
				var source = new TimeoutSource (50);
				source.set_callback (() => {
					join_and_end.begin ();
					return false;
				});
				source.attach (main_context);
			} else {
				join_and_end.begin ();
			}
		}

		private async void join_and_end () {
			if (tid != 0) {
				while (_thread_is_alive (pid, tid)) {
					var source = new TimeoutSource (50);
					source.set_callback (join_and_end.callback);
					source.attach (main_context);
					yield;
				}
			}

			ended (unload_policy);
		}

		private class PendingHello {
			private SourceFunc? handler;

			public PendingHello (owned SourceFunc handler) {
				this.handler = (owned) handler;
			}

			public void complete () {
				handler ();
				handler = null;
			}
		}

		public extern static void _receive_pulse (void * instance, out QnxPulseCode code, out int val) throws Error;
		public extern static bool _thread_is_alive (uint pid, uint tid);
	}

	public enum QnxPulseCode {
		DISCONNECT = -33,
		HELLO = 0,
		BYE = 1,
	}
}
