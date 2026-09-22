namespace Frida {
	public sealed class Fruitjector : Object, Injector {
		public signal void injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module);

		public DarwinHelper helper {
			get;
			construct;
		}

		public bool close_helper {
			get;
			construct;
		}

		public TemporaryDirectory tempdir {
			get;
			construct;
		}

		private Gee.HashMap<uint, uint> pid_by_id = new Gee.HashMap<uint, uint> ();
		private Gee.HashMap<uint, TemporaryFile> blob_file_by_id = new Gee.HashMap<uint, TemporaryFile> ();
		private uint next_blob_id = 1;

		public Fruitjector (DarwinHelper helper, bool close_helper, TemporaryDirectory tempdir) {
			Object (helper: helper, close_helper: close_helper, tempdir: tempdir);
		}

		construct {
			helper.injected.connect (on_injected);
			helper.uninjected.connect (on_uninjected);
		}

		~Fruitjector () {
			helper.injected.disconnect (on_injected);
			helper.uninjected.disconnect (on_uninjected);

			if (close_helper) {
				helper.close.begin (null);

				tempdir.destroy ();
			}
		}

		public async void close (Cancellable? cancellable) throws IOError {
			helper.injected.disconnect (on_injected);
			helper.uninjected.disconnect (on_uninjected);

			if (close_helper) {
				yield helper.close (cancellable);

				tempdir.destroy ();
			}
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var id = yield helper.inject_library_file (pid, path, entrypoint, data, cancellable);
			pid_by_id[id] = pid;
			return id;
		}

		public async uint inject_library_blob (uint pid, Bytes blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			// We can optimize this later when our mapper is always used instead of dyld
			FileUtils.chmod (tempdir.path, 0755);

			var name = "blob%u.dylib".printf (next_blob_id++);
			var file = new TemporaryFile.from_stream (name, new MemoryInputStream.from_bytes (blob), tempdir);

			var id = yield inject_library_file (pid, file.path, entrypoint, data, cancellable);

			blob_file_by_id[id] = file;

			return id;
		}

		public async uint inject_library_resource (uint pid, AgentResource resource, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			var blob = yield helper.try_mmap (resource.blob, cancellable);
			if (blob == null)
				return yield inject_library_file (pid, resource.get_file ().path, entrypoint, data, cancellable);

			var id = yield helper.inject_library_blob (pid, resource.name, blob, entrypoint, data, cancellable);
			pid_by_id[id] = pid;
			return id;
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			yield helper.demonitor (id, cancellable);
		}

		public async uint demonitor_and_clone_state (uint id, Cancellable? cancellable) throws Error, IOError {
			return yield helper.demonitor_and_clone_injectee_state (id, cancellable);
		}

		public async void recreate_thread (uint pid, uint id, Cancellable? cancellable) throws Error, IOError {
			yield helper.recreate_injectee_thread (pid, id, cancellable);
		}

		public bool any_still_injected () {
			return !pid_by_id.is_empty;
		}

		public bool is_still_injected (uint id) {
			return pid_by_id.has_key (id);
		}

		private void on_injected (uint id, uint pid, bool has_mapped_module, DarwinModuleDetails mapped_module) {
			injected (id, pid, has_mapped_module, mapped_module);
		}

		private void on_uninjected (uint id) {
			pid_by_id.unset (id);
			blob_file_by_id.unset (id);

			uninjected (id);
		}
	}

	public sealed class AgentResource : Object {
		public string name {
			get;
			construct;
		}

		public Bytes blob {
			get;
			construct;
		}

		public TemporaryDirectory? tempdir {
			get;
			construct;
		}

		private TemporaryFile _file;

		public AgentResource (string name, Bytes blob, TemporaryDirectory? tempdir = null) {
			Object (name: name, blob: blob, tempdir: tempdir);
		}

		public TemporaryFile get_file () throws Error {
			if (_file == null) {
				var stream = new MemoryInputStream.from_bytes (blob);
				_file = new TemporaryFile.from_stream (name, stream, tempdir);
				FileUtils.chmod (_file.path, 0755);
			}
			return _file;
		}
	}
}
