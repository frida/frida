namespace Frida.Test {
	public sealed class Process : Object {
		public void * handle {
			get;
			set;
		}

		public uint id {
			get;
			construct;
		}

		public bool auto_kill {
			get;
			construct;
		}

		public unowned string filename {
			get {
				if (_filename == null) {
					_filename = ProcessBackend.filename_of (handle).replace ("/./", "/");
				}

				return _filename;
			}
		}
		private string _filename = null;

		public static Process current {
			owned get {
				return new Process (ProcessBackend.self_handle (), ProcessBackend.self_id (), false);
			}
		}

		private Process (void * handle, uint id, bool auto_kill) {
			Object (handle: handle, id: id, auto_kill: auto_kill);
		}

		~Process () {
			if (handle != null && auto_kill) {
				try {
					kill ();
				} catch (Error e) {
				}
			}
		}

		public static Process create (string path, string[]? args = null, string[]? env = null, Arch arch = Arch.CURRENT) throws Error {
			return _create (path, args, env, arch, true);
		}

		public static Process start (string path, string[]? args = null, string[]? env = null, Arch arch = Arch.CURRENT) throws Error {
			return _create (path, args, env, arch, false);
		}

		private static Process _create (string path, string[]? args, string[]? env, Arch arch, bool suspended) throws Error {
			var argv = new string[1 + ((args != null) ? args.length : 0)];
			argv[0] = path;
			if (args != null) {
				for (var i = 0; i != args.length; i++)
					argv[1 + i] = args[i];
			}

			string[] envp = (env != null) ? env : Environ.get ();

			void * handle;
			uint id;
			ProcessBackend.create (path, argv, envp, arch, suspended, out handle, out id);

			return new Process (handle, id, true);
		}

		public void resume () throws Error {
			ProcessBackend.resume (handle);
		}

		public int join (uint timeout_msec = 0) throws Error {
			if (handle == null)
				throw new Error.INVALID_OPERATION ("Process already joined or killed");

			var result = ProcessBackend.join (handle, timeout_msec);
			handle = null;

			return result;
		}

		public void kill () throws Error {
			if (handle == null)
				throw new Error.INVALID_OPERATION ("Process already joined or killed");

			ProcessBackend.kill (handle);
			handle = null;
		}

		public ResourceUsageSnapshot snapshot_resource_usage () {
			return ResourceUsageSnapshot.create_for_pid (id);
		}
	}

	public sealed class ResourceUsageSnapshot : Object {
		private HashTable<string, uint> metrics = new HashTable<string, uint> (str_hash, str_equal);

		public static ResourceUsageSnapshot create_for_self () {
			return create_for_pid (0);
		}

		public extern static ResourceUsageSnapshot create_for_pid (uint pid);

		public void _add (string name, uint val) {
			metrics[name] = val;
		}

		public void print () {
			printerr ("TYPE\tCOUNT\n");
			metrics.for_each ((key, current_value) => {
				printerr ("%s\t%u\n", key, current_value);
			});
		}

		public void print_comparison (ResourceUsageSnapshot previous_snapshot) {
			printerr ("TYPE\tBEFORE\tAFTER\n");
			var previous_metrics = previous_snapshot.metrics;
			metrics.for_each ((key, current_value) => {
				var previous_value = previous_metrics[key];
				printerr ("%s\t%u\t%u\n", key, previous_value, current_value);
			});
		}

		public void assert_equals (ResourceUsageSnapshot previous_snapshot) {
			uint num_differences = 0;

			var previous_metrics = previous_snapshot.metrics;
			metrics.for_each ((key, current_value) => {
				var previous_value = previous_metrics[key];
				if (current_value != previous_value) {
					if (num_differences == 0) {
						printerr (
							"\n\n" +
							"***************************\n" +
							"UH-OH, RESOURCE LEAK FOUND!\n" +
							"***************************\n" +
							"\n" +
							"TYPE\tBEFORE\tAFTER\n"
						);
					}

					printerr ("%s\t%u\t%u\n", key, previous_value, current_value);

					num_differences++;
				}
			});

			if (num_differences > 0)
				printerr ("\n");

			// assert_true (num_differences == 0);
		}
	}

	namespace ProcessBackend {
		private extern void * self_handle ();
		private extern uint self_id ();
		private extern string filename_of (void * handle);
		private extern void create (string path, string[] argv, string[] envp, Arch arch, bool suspended, out void * handle, out uint id) throws Error;
		private extern int join (void * handle, uint timeout_msec) throws Error;
		private extern void resume (void * handle) throws Error;
		private extern void kill (void * handle);
	}
}
