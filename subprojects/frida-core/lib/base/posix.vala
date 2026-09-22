namespace Frida {
	public sealed class StdioPipes : Object {
		public OutputStream? input {
			get;
			construct;
		}

		public InputStream output {
			get;
			construct;
		}

		public InputStream error {
			get;
			construct;
		}

		public StdioPipes (FileDescriptor? input, FileDescriptor output, FileDescriptor error) {
			Object (
				input: (input != null) ? new UnixOutputStream (ensure_nonblocking (input.steal ()), true) : null,
				output: new UnixInputStream (ensure_nonblocking (output.steal ()), true),
				error: new UnixInputStream (ensure_nonblocking (error.steal ()), true)
			);
		}

		private static int ensure_nonblocking (int fd) {
			try {
				Unix.set_fd_nonblocking (fd, true);
				return fd;
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
		}
	}

	public class FileDescriptor : Object, FileDescriptorBased {
		public int handle;

		public FileDescriptor (int handle) {
			this.handle = handle;
		}

		~FileDescriptor () {
			if (handle != -1)
				Posix.close (handle);
		}

		public int steal () {
			int result = handle;
			handle = -1;
			return result;
		}

		public int get_fd () {
			return handle;
		}

		public size_t pread (uint8[] buf, uint64 offset) throws Error {
			while (true) {
				ssize_t res = Posix.pread (handle, buf, buf.length, (Posix.off_t) offset);
				if (res == -1) {
					if (errno == Posix.EINTR)
						continue;
					throw new Error.TRANSPORT ("%s", strerror (errno));
				}

				return res;
			}
		}

		public void pread_all (uint8[] buf, uint64 offset) throws Error {
			size_t total = 0;

			while (total != buf.length) {
				void * dst = (uint8 *) buf + total;
				size_t remaining = buf.length - total;

				while (true) {
					ssize_t res = Posix.pread (handle, dst, remaining, (Posix.off_t) (offset + total));
					if (res == -1) {
						if (errno == Posix.EINTR)
							continue;
						throw new Error.TRANSPORT ("%s", strerror (errno));
					}

					if (res == 0)
						throw new Error.TRANSPORT ("Unexpected EOF: read %zu of %zu bytes", total, buf.length);

					total += (size_t) res;
					break;
				}
			}
		}

		public size_t pwrite (uint8[] buf, uint64 offset) throws Error {
			while (true) {
				ssize_t res = Posix.pwrite (handle, buf, buf.length, (Posix.off_t) offset);
				if (res == -1) {
					if (errno == Posix.EINTR)
						continue;
					throw new Error.TRANSPORT ("%s", strerror (errno));
				}

				return res;
			}
		}

		public void pwrite_all (uint8[] buf, uint64 offset) throws Error {
			size_t total = 0;

			while (total != buf.length) {
				uint8[] chunk = buf[total : buf.length];

				while (true) {
					ssize_t res = Posix.pwrite (handle, chunk, chunk.length, (Posix.off_t) (offset + total));
					if (res == -1) {
						if (errno == Posix.EINTR)
							continue;
						throw new Error.TRANSPORT ("%s", strerror (errno));
					}

					total += (size_t) res;
					break;
				}
			}
		}
	}

	public StdioPipes? make_stdio_pipes (Stdio stdio, bool in_supported, out FileDescriptor? in_fd, out string? in_name,
			out FileDescriptor? out_fd, out string? out_name, out FileDescriptor? err_fd, out string? err_name) throws Error {
		if (stdio == PIPE) {
			FileDescriptor? in_write = null;
			FileDescriptor? out_read, err_read;

			if (in_supported) {
				make_stdio_pipe (out in_fd, out in_write, out in_name);
			} else {
				in_fd = null;
				in_name = null;
			}
			make_stdio_pipe (out out_read, out out_fd, out out_name);
			make_stdio_pipe (out err_read, out err_fd, out err_name);

			return new StdioPipes (in_write, out_read, err_read);
		} else {
			in_fd = null;
			in_name = null;

			out_fd = null;
			out_name = null;

			err_fd = null;
			err_name = null;

			return null;
		}
	}

	public void make_stdio_pipe (out FileDescriptor read, out FileDescriptor write, out string? name = null) throws Error {
#if HAVE_OPENPTY
		int rfd = -1, wfd = -1;
		char buf[Posix.Limits.PATH_MAX];
		var res =
#if DARWIN
			Darwin.XNU.openpty (out rfd, out wfd, buf, null, null)
#elif LINUX
			Linux.openpty (out rfd, out wfd, buf, null, null)
#elif FREEBSD
			FreeBSD.openpty (out rfd, out wfd, buf, null, null)
#endif
			;
		if (res == -1)
			throw new Error.NOT_SUPPORTED ("Unable to open PTY: %s", strerror (errno));
		name = (string) buf;

		enable_close_on_exec (rfd);
		enable_close_on_exec (wfd);

		disable_sigpipe (rfd);
		disable_sigpipe (wfd);

		configure_terminal_attributes (rfd);

		read = new FileDescriptor (rfd);
		write = new FileDescriptor (wfd);
#else
		name = null;

		try {
			int fds[2];
			Unix.open_pipe (fds, Posix.FD_CLOEXEC);

			read = new FileDescriptor (fds[0]);
			write = new FileDescriptor (fds[1]);
		} catch (GLib.Error e) {
			throw new Error.NOT_SUPPORTED ("Unable to open pipe: %s", e.message);
		}
#endif
	}

#if HAVE_OPENPTY
	private void enable_close_on_exec (int fd) {
		Posix.fcntl (fd, Posix.F_SETFD, Posix.fcntl (fd, Posix.F_GETFD) | Posix.FD_CLOEXEC);
	}

	private void disable_sigpipe (int fd) {
#if DARWIN
		Posix.fcntl (fd, Darwin.XNU.F_SETNOSIGPIPE, true);
#endif
	}

	private void configure_terminal_attributes (int fd) {
		var tios = Posix.termios ();
		Posix.tcgetattr (fd, out tios);

		tios.c_oflag &= ~Posix.ONLCR;
		tios.c_cflag = (tios.c_cflag & Posix.CLOCAL) | Posix.CS8 | Posix.CREAD | Posix.HUPCL;
		tios.c_lflag &= ~Posix.ECHO;

		Posix.tcsetattr (fd, 0, tios);
	}
#endif
}
