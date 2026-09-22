namespace Frida.FS {
	public void mkdirp (File dir, Cancellable? cancellable = null) throws Error {
		try {
			dir.make_directory_with_parents (cancellable);
		} catch (GLib.Error e) {
			if (!(e is IOError.EXISTS))
				throw new Error.PERMISSION_DENIED ("%s", e.message);
		}
	}

	public void rmtree (File dir, Cancellable? cancellable = null) throws Error {
		rmtree_worker (dir, 0, cancellable);
	}

	private void rmtree_worker (File dir, uint depth, Cancellable? cancellable = null) throws Error {
		FileEnumerator enumerator;
		try {
			enumerator = dir.enumerate_children (FileAttribute.STANDARD_NAME, NOFOLLOW_SYMLINKS, cancellable);
		} catch (GLib.Error e) {
			if (depth == 0 && e is IOError.NOT_FOUND)
				return;
			throw new Error.PERMISSION_DENIED ("%s", e.message);
		}

		try {
			FileInfo? info;
			File? child;
			while (enumerator.iterate (out info, out child, cancellable) && info != null) {
				if (info == null)
					continue;
				if (info.get_file_type () == DIRECTORY)
					rmtree (child, cancellable);
				else
					child.delete (cancellable);
			}

			dir.delete (cancellable);
		} catch (GLib.Error e) {
			if (!(e is IOError.EXISTS))
				throw new Error.PERMISSION_DENIED ("%s", e.message);
		}
	}

	public async void rmtree_async (File dir, Cancellable? cancellable = null) throws Error, IOError {
		yield rmtree_async_worker (dir, 0, cancellable);
	}

	private async void rmtree_async_worker (File dir, uint depth, Cancellable? cancellable = null) throws Error, IOError {
		int io_priority = Priority.DEFAULT;

		FileEnumerator enumerator;
		try {
			enumerator = yield dir.enumerate_children_async (FileAttribute.STANDARD_NAME, NOFOLLOW_SYMLINKS, io_priority,
				cancellable);
		} catch (GLib.Error e) {
			if (depth == 0 && e is IOError.NOT_FOUND)
				return;
			throw new Error.PERMISSION_DENIED ("%s", e.message);
		}

		try {
			while (true) {
				var batch = yield enumerator.next_files_async (1000, io_priority, cancellable);
				if (batch.is_empty ())
					break;
				foreach (FileInfo info in batch) {
					File child = dir.get_child (info.get_name ());
					if (info.get_file_type () == DIRECTORY)
						yield rmtree_async_worker (child, depth + 1, cancellable);
					else
						yield child.delete_async (io_priority, cancellable);
				}
			}

			yield dir.delete_async (io_priority, cancellable);
		} catch (GLib.Error e) {
			throw new Error.PERMISSION_DENIED ("%s", e.message);
		}
	}

	public async string read_all_text (File f, Cancellable? cancellable) throws Error, IOError {
		Bytes b = yield read_all_bytes (f, cancellable);
		string text = (string) b.get_data ();
		if (!text.validate ())
			throw new Error.PROTOCOL ("Unable to read '%s': invalid UTF-8", f.get_parse_name ());
		return text;
	}

	public async void write_all_text (File f, string t, Cancellable? cancellable) throws Error, IOError {
		yield write_all_bytes (f, new Bytes (t.data), cancellable);
	}

	public async Bytes read_all_bytes (File f, Cancellable? cancellable) throws Error, IOError {
		try {
			return yield f.load_bytes_async (cancellable, null);
		} catch (GLib.Error e) {
			if (e is IOError.CANCELLED)
				throw (IOError) e;
			if (e is IOError.NOT_FOUND)
				throw new Error.INVALID_ARGUMENT ("%s", e.message);
			throw new Error.PERMISSION_DENIED ("%s", e.message);
		}
	}

	public async void write_all_bytes (File f, Bytes b, Cancellable? cancellable) throws Error, IOError {
		try {
			yield f.replace_contents_bytes_async (b, null, false, FileCreateFlags.NONE, cancellable, null);
		} catch (GLib.Error e) {
			if (e is IOError.CANCELLED)
				throw (IOError) e;
			throw new Error.PERMISSION_DENIED ("%s", e.message);
		}
	}
}
