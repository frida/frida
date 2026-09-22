namespace Frida {
	/**
	 * Watches a filesystem path and reports changes to it.
	 */
	public sealed class FileMonitor : Object {
		/**
		 * Emitted when a change is observed at the watched path.
		 *
		 * @param file_path the path that changed
		 * @param other_file_path the other path involved, for example the
		 *   destination of a rename, or null
		 * @param event the kind of change
		 */
		public signal void change (string file_path, string? other_file_path, FileMonitorEvent event);

		/**
		 * The path being watched.
		 */
		public string path {
			get;
			construct;
		}

		private GLib.FileMonitor monitor;

		/**
		 * Creates a file monitor for the given path.
		 *
		 * @param path the filesystem path to watch
		 */
		public FileMonitor (string path) {
			Object (path: path);
		}

		~FileMonitor () {
			clear ();
		}

		/**
		 * Starts watching, after which {@link FileMonitor.change} is emitted on
		 * each change.
		 */
		public async void enable (Cancellable? cancellable = null) throws Error, IOError {
			if (monitor != null)
				throw new Error.INVALID_OPERATION ("Already enabled");

			var file = File.parse_name (path);

			try {
				monitor = file.monitor (FileMonitorFlags.NONE, cancellable);
			} catch (GLib.Error e) {
				throw new Error.INVALID_OPERATION ("%s", e.message);
			}

			monitor.changed.connect (on_changed);
		}

		public void enable_sync (Cancellable? cancellable = null) throws Error, IOError {
			var task = create<EnableTask> () as EnableTask;
			task.execute (cancellable);
		}

		private class EnableTask : FileMonitorTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.enable (cancellable);
			}
		}

		/**
		 * Stops watching.
		 */
		public async void disable (Cancellable? cancellable = null) throws Error, IOError {
			if (monitor == null)
				throw new Error.INVALID_OPERATION ("Already disabled");

			clear ();
		}

		private void clear () {
			if (monitor == null)
				return;

			monitor.changed.disconnect (on_changed);
			monitor.cancel ();
			monitor = null;
		}

		public void disable_sync (Cancellable? cancellable = null) throws Error, IOError {
			create<DisableTask> ().execute (cancellable);
		}

		private class DisableTask : FileMonitorTask<void> {
			protected override async void perform_operation () throws Error, IOError {
				yield parent.disable (cancellable);
			}
		}

		private void on_changed (File file, File? other_file, FileMonitorEvent event) {
			change (file.get_parse_name (), (other_file != null) ? other_file.get_parse_name () : null, event);
		}

		private T create<T> () {
			return Object.new (typeof (T), parent: this);
		}

		private abstract class FileMonitorTask<T> : AsyncTask<T> {
			public weak FileMonitor parent {
				get;
				construct;
			}
		}
	}
}
