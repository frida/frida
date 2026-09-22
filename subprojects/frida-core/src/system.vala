namespace Frida {
	namespace System {
		public extern static Frida.HostApplicationInfo get_frontmost_application (FrontmostQueryOptions options) throws Error;
		public extern static Frida.HostApplicationInfo[] enumerate_applications (ApplicationQueryOptions options);
		public extern static Frida.HostProcessInfo[] enumerate_processes (ProcessQueryOptions options);
		public extern static void kill (uint pid);
	}

	public sealed class ApplicationEnumerator : Object {
		private ThreadPool<EnumerateRequest> pool;
		private MainContext main_context;

		construct {
			try {
				pool = new ThreadPool<EnumerateRequest>.with_owned_data (handle_request, 1, false);
			} catch (ThreadError e) {
				assert_not_reached ();
			}

			main_context = MainContext.ref_thread_default ();
		}

		public async HostApplicationInfo[] enumerate_applications (ApplicationQueryOptions options) {
			var request = new EnumerateRequest (options, enumerate_applications.callback);
			try {
				pool.add (request);
			} catch (ThreadError e) {
				assert_not_reached ();
			}
			yield;
			return request.result;
		}

		private void handle_request (owned EnumerateRequest request) {
			var applications = System.enumerate_applications (request.options);

			var source = new IdleSource ();
			source.set_callback (() => {
				request.complete (applications);
				return false;
			});
			source.attach (main_context);
		}

		private class EnumerateRequest {
			public ApplicationQueryOptions options {
				get;
				private set;
			}

			public HostApplicationInfo[] result {
				get;
				private set;
			}

			private SourceFunc? handler;

			public EnumerateRequest (ApplicationQueryOptions options, owned SourceFunc handler) {
				this.options = options;
				this.handler = (owned) handler;
			}

			public void complete (HostApplicationInfo[] applications) {
				this.result = applications;
				handler ();
				handler = null;
			}
		}
	}

	public sealed class ProcessEnumerator : Object {
		private ThreadPool<EnumerateRequest> pool;
		private MainContext main_context;

		construct {
			try {
				pool = new ThreadPool<EnumerateRequest>.with_owned_data (handle_request, 1, false);
			} catch (ThreadError e) {
				assert_not_reached ();
			}

			main_context = MainContext.ref_thread_default ();
		}

		public async HostProcessInfo[] enumerate_processes (ProcessQueryOptions options) {
			var request = new EnumerateRequest (options, enumerate_processes.callback);
			try {
				pool.add (request);
			} catch (ThreadError e) {
				assert_not_reached ();
			}
			yield;
			return request.result;
		}

		private void handle_request (owned EnumerateRequest request) {
			var processes = System.enumerate_processes (request.options);

			var source = new IdleSource ();
			source.set_callback (() => {
				request.complete (processes);
				return false;
			});
			source.attach (main_context);
		}

		private class EnumerateRequest {
			public ProcessQueryOptions options {
				get;
				private set;
			}

			public HostProcessInfo[] result {
				get;
				private set;
			}

			private SourceFunc? handler;

			public EnumerateRequest (ProcessQueryOptions options, owned SourceFunc handler) {
				this.options = options;
				this.handler = (owned) handler;
			}

			public void complete (HostProcessInfo[] processes) {
				this.result = processes;
				handler ();
				handler = null;
			}
		}
	}

	public sealed class TemporaryDirectory {
		private string? name;

		public string path {
			owned get {
				if (file == null) {
					if (name != null)
						file = File.new_for_path (Path.build_filename (system_tmp_directory, name));
					else
						file = File.new_for_path (system_tmp_directory);

					try {
						file.make_directory_with_parents ();
					} catch (GLib.Error e) {
						// Following operations will fail
					}
				}

				return file.get_path ();
			}
		}
		private File? file;

		public bool is_ours {
			get;
			private set;
		}

		public static TemporaryDirectory system_default {
			owned get {
				return new TemporaryDirectory.with_file (File.new_for_path (system_tmp_directory), false);
			}
		}

		private static string system_tmp_directory {
			owned get {
				return (sysroot != null)
					? Path.build_filename (sysroot, get_system_tmp ())
					: get_system_tmp ();
			}
		}

		private static string? fixed_name = null;
		private static string? sysroot = null;

		public TemporaryDirectory () {
#if !QNX
			this.name = (fixed_name != null) ? fixed_name : make_name ();
			this.is_ours = true;

			if (fixed_name != null) {
				try {
					var future_file = File.new_for_path (Path.build_filename (get_system_tmp (), name));
					var path = future_file.get_path ();
					var dir = Dir.open (path);
					string? child;
					while ((child = dir.read_name ()) != null) {
						FileUtils.unlink (Path.build_filename (path, child));
					}
				} catch (FileError e) {
				}
			}
#endif
		}

		public TemporaryDirectory.with_file (File file, bool is_ours) {
			this.file = file;
			this.is_ours = is_ours;
		}

		~TemporaryDirectory () {
			destroy ();
		}

		public static void always_use (string? name) {
			fixed_name = name;
		}

		public static void use_sysroot (string? root) {
			sysroot = root;
		}

		public void destroy () {
			if (is_ours && file != null) {
				try {
					var enumerator = file.enumerate_children ("standard::*", 0);

					FileInfo file_info;
					while ((file_info = enumerator.next_file ()) != null) {
						if (file_info.get_file_type () == DIRECTORY) {
							File subdir = file.get_child (file_info.get_name ());
							try {
								subdir.delete ();
							} catch (GLib.Error e) {
							}
						}
					}
				} catch (GLib.Error e) {
				}

				try {
					file.delete ();
				} catch (GLib.Error e) {
				}
			}
		}

		public static string make_name () {
			var builder = new StringBuilder ("frida-");
			for (var i = 0; i != 16; i++)
				builder.append_printf ("%02x", Random.int_range (0, 256));
			return builder.str;
		}

		private extern static string get_system_tmp ();
	}

	public sealed class TemporaryFile {
		public string path {
			owned get {
				return file.get_path ();
			}
		}
		private File file;

		public TemporaryDirectory parent {
			get {
				return directory;
			}
		}
		private TemporaryDirectory directory;

		public TemporaryFile.from_stream (string name, InputStream istream, TemporaryDirectory? directory = null) throws Error {
			if (directory != null)
				this.directory = directory;
			else
				this.directory = TemporaryDirectory.system_default;

			string file_path = Path.build_filename (this.directory.path, name);
			string directory_path = Path.get_dirname (file_path);

			if (!FileUtils.test (directory_path, GLib.FileTest.IS_DIR)) {
				try {
					File tmp_dir = File.new_for_path (directory_path);
					tmp_dir.make_directory_with_parents ();
				} catch (GLib.Error e) {
					throw new Error.PERMISSION_DENIED ("%s", e.message);
				}
			}

			this.file = File.new_for_path (file_path);

			try {
				// FIXME: REPLACE_DESTINATION doesn't work?!
				file.delete ();
			} catch (GLib.Error delete_error) {
			}

			try {
				var ostream = file.create (FileCreateFlags.REPLACE_DESTINATION, null);

				var buf_size = 128 * 1024;
				var buf = new uint8[buf_size];

				while (true) {
					var bytes_read = istream.read (buf);
					if (bytes_read == 0)
						break;
					buf.resize ((int) bytes_read);

					size_t bytes_written;
					ostream.write_all (buf, out bytes_written);
				}

				ostream.close (null);
			} catch (GLib.Error e) {
				throw new Error.PERMISSION_DENIED ("%s", e.message);
			}
		}

		public TemporaryFile (File file, TemporaryDirectory directory) {
			this.file = file;
			this.directory = directory;
		}

		~TemporaryFile () {
			destroy ();
		}

		public void destroy () {
			if (file != null) {
				try {
					file.delete (null);
				} catch (GLib.Error e) {
				}
				file = null;
			}
			directory = null;
		}
	}
}
