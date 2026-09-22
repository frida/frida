namespace Frida {
	public sealed class Linjector : Object, Injector {
		public LinuxHelper helper {
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
		private uint next_injectee_id = 1;
		private uint next_blob_id = 1;
		private bool did_prep_tempdir = false;

		public Linjector (LinuxHelper helper, bool close_helper, TemporaryDirectory tempdir) {
			Object (helper: helper, close_helper: close_helper, tempdir: tempdir);
		}

		construct {
			helper.uninjected.connect (on_uninjected);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			helper.uninjected.disconnect (on_uninjected);

			if (close_helper) {
				yield helper.close (cancellable);

				tempdir.destroy ();
			}
		}

		public async uint inject_library_file (uint pid, string path, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			AgentFeatures features = 0;
			return yield inject_library_file_with_template (pid, PathTemplate (path), entrypoint, data, features, cancellable);
		}

		public async uint inject_library_file_with_template (uint pid, PathTemplate path_template, string entrypoint, string data,
				AgentFeatures features, Cancellable? cancellable) throws Error, IOError {
			string path = path_template.expand (arch_name_from_pid (pid));
			int fd = Posix.open (path, Posix.O_RDONLY);
			if (fd == -1)
				throw new Error.INVALID_ARGUMENT ("Unable to open library: %s", strerror (errno));
			var library_so = new UnixInputStream (fd, true);

			return yield inject_library_fd (pid, library_so, entrypoint, data, features, cancellable);
		}

		public async uint inject_library_blob (uint pid, Bytes blob, string entrypoint, string data, Cancellable? cancellable)
				throws Error, IOError {
			string name = "blob%u.so".printf (next_blob_id++);
			AgentFeatures features = 0;

			if (MemoryFileDescriptor.is_supported ()) {
				FileDescriptor fd = MemoryFileDescriptor.from_bytes (name, blob);
				adjust_fd_permissions (fd);
				UnixInputStream library_so = new UnixInputStream (fd.steal (), true);
				return yield inject_library_fd (pid, library_so, entrypoint, data, features, cancellable);
			}

			ensure_tempdir_prepared ();

			var file = new TemporaryFile.from_stream (name, new MemoryInputStream.from_bytes (blob), tempdir);
			var path = file.path;
			adjust_file_permissions (path);

			var id = yield inject_library_file (pid, path, entrypoint, data, cancellable);

			blob_file_by_id[id] = file;

			return id;
		}

		public async uint inject_library_resource (uint pid, AgentDescriptor agent, string entrypoint, string data,
				AgentFeatures features, Cancellable? cancellable) throws Error, IOError {
			if (MemoryFileDescriptor.is_supported ()) {
				unowned string arch_name = arch_name_from_pid (pid);
				string name = agent.name_template.expand (arch_name);
				AgentResource? resource = agent.resources.first_match (r => r.name == name);
				if (resource == null) {
					throw new Error.NOT_SUPPORTED ("Unable to handle %s-bit processes due to build configuration",
						arch_name);
				}
				return yield inject_library_fd (pid, resource.get_memfd (), entrypoint, data, features, cancellable);
			}

			ensure_tempdir_prepared ();
			return yield inject_library_file_with_template (pid, agent.get_path_template (), entrypoint, data, features,
				cancellable);
		}

		public async uint inject_library_fd (uint pid, UnixInputStream library_so, string entrypoint, string data,
				AgentFeatures features, Cancellable? cancellable) throws Error, IOError {
			uint id = next_injectee_id++;
			yield helper.inject_library (pid, library_so, entrypoint, data, features, id, cancellable);

			pid_by_id[id] = pid;

			return id;
		}

		public async IOStream request_control_channel (uint id, Cancellable? cancellable) throws Error, IOError {
			return yield helper.request_control_channel (id, cancellable);
		}

		private void ensure_tempdir_prepared () {
			if (did_prep_tempdir)
				return;

			if (tempdir.is_ours)
				adjust_directory_permissions (tempdir.path);

			did_prep_tempdir = true;
		}

		public async void demonitor (uint id, Cancellable? cancellable) throws Error, IOError {
			yield helper.demonitor (id, cancellable);
		}

		public async uint demonitor_and_clone_state (uint id, Cancellable? cancellable) throws Error, IOError {
			uint clone_id = next_injectee_id++;
			yield helper.demonitor_and_clone_injectee_state (id, clone_id, 0, cancellable);
			return clone_id;
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

		private void on_uninjected (uint id) {
			pid_by_id.unset (id);
			blob_file_by_id.unset (id);

			uninjected (id);
		}
	}

	public enum AgentMode {
		INSTANCED,
		SINGLETON
	}

	public sealed class AgentDescriptor : Object {
		public PathTemplate name_template {
			get;
			construct;
		}

		public Gee.Collection<AgentResource> resources {
			get;
			construct;
		}

		public AgentMode mode {
			get;
			construct;
		}

		public TemporaryDirectory? tempdir {
			get;
			construct;
		}

		private PathTemplate? cached_path_template;

		public AgentDescriptor (PathTemplate name_template, Bytes? so32, Bytes? so64, AgentResource[] resources = {},
				AgentMode mode = AgentMode.INSTANCED, TemporaryDirectory? tempdir = null) {
			var all_resources = new Gee.ArrayList<AgentResource> ();
			if (so32 != null && so32.length != 0) {
				all_resources.add (new AgentResource (name_template.expand ("32"),
					(mode == INSTANCED) ? _clone_so (so32) : so32, tempdir));
			}
			if (so64 != null && so64.length != 0) {
				all_resources.add (new AgentResource (name_template.expand ("64"),
					(mode == INSTANCED) ? _clone_so (so64) : so64, tempdir));
			}
			foreach (var r in resources) {
				if (r.blob.length != 0)
					all_resources.add (r);
			}

			Object (name_template: name_template, resources: all_resources, mode: mode, tempdir: tempdir);
		}

		public PathTemplate get_path_template () throws Error {
			if (cached_path_template == null) {
				TemporaryDirectory? first_tempdir = null;
				foreach (AgentResource r in resources) {
					TemporaryFile f = r.get_file ();
					adjust_file_permissions (f.path);
					if (first_tempdir == null)
						first_tempdir = f.parent;
				}

				cached_path_template = PathTemplate (first_tempdir.path + "/" + name_template.str);
			}

			return cached_path_template;
		}

		internal extern static Bytes _clone_so (Bytes so);
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

		private TemporaryFile? _file;
		private UnixInputStream? _memfd;

		public AgentResource (string name, Bytes blob, TemporaryDirectory? tempdir = null) {
			Object (name: name, blob: blob, tempdir: tempdir);
		}

		public TemporaryFile get_file () throws Error {
			if (_file == null) {
				var stream = new MemoryInputStream.from_bytes (blob);
				_file = new TemporaryFile.from_stream (name, stream, tempdir);
			}
			return _file;
		}

		public UnixInputStream get_memfd () throws Error {
			if (_memfd == null) {
				if (!MemoryFileDescriptor.is_supported ())
					throw new Error.NOT_SUPPORTED ("Kernel too old for memfd support");
				FileDescriptor fd = MemoryFileDescriptor.from_bytes (name, blob);
				adjust_fd_permissions (fd);
				_memfd = new UnixInputStream (fd.steal (), true);
			}
			return _memfd;
		}
	}

	private static void adjust_directory_permissions (string path) {
		FileUtils.chmod (path, 0755);
#if ANDROID
		SELinux.setfilecon (path, "u:object_r:frida_file:s0");
#endif
	}

	private static void adjust_file_permissions (string path) {
		FileUtils.chmod (path, path.has_suffix (".so") ? 0755 : 0644);
#if ANDROID
		SELinux.setfilecon (path, "u:object_r:frida_file:s0");
#endif
	}

	private static void adjust_fd_permissions (FileDescriptor fd) {
#if ANDROID
		SELinux.fsetfilecon (fd.handle, "u:object_r:frida_memfd:s0");
#endif
	}
}
