namespace Frida {
#if WINDOWS
	public sealed class FileDescriptorGuard : Object {
		public Gum.MemoryRange agent_range {
			get;
			construct;
		}

		public FileDescriptorGuard (Gum.MemoryRange agent_range) {
			Object (agent_range: agent_range);
		}
	}
#else
	public sealed class FileDescriptorGuard : Object {
		public Gum.MemoryRange agent_range {
			get;
			construct;
		}

		private CloseListener close_listener;

		public FileDescriptorGuard (Gum.MemoryRange agent_range) {
			Object (agent_range: agent_range);
		}

		construct {
			var interceptor = Gum.Interceptor.obtain ();

			var close = Gum.Process.get_libc_module ().find_export_by_name ("close");
			close_listener = new CloseListener (this);
			interceptor.attach ((void *) close, close_listener);
		}

		~FileDescriptorGuard () {
			var interceptor = Gum.Interceptor.obtain ();

			interceptor.detach (close_listener);
		}

		private class CloseListener : Object, Gum.InvocationListener {
			public weak FileDescriptorGuard parent {
				get;
				construct;
			}

			public CloseListener (FileDescriptorGuard parent) {
				Object (parent: parent);
			}

			private void on_enter (Gum.InvocationContext context) {
				Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));

				var caller = (Gum.Address) context.get_return_address ();
				var range = parent.agent_range;
				bool caller_is_frida = (caller >= range.base_address && caller < range.base_address + range.size);
				if (caller_is_frida) {
					invocation.is_cloaked = false;
					return;
				}

				var fd = (int) context.get_nth_argument (0);
				invocation.is_cloaked = Gum.Cloak.has_file_descriptor (fd);
				if (invocation.is_cloaked) {
					fd = -1;
					context.replace_nth_argument (0, (void *) fd);
				}
			}

			private void on_leave (Gum.InvocationContext context) {
				Invocation * invocation = context.get_listener_invocation_data (sizeof (Invocation));
				if (invocation.is_cloaked) {
					context.replace_return_value ((void *) 0);
					context.system_error = 0;
				}
			}

			private struct Invocation {
				public bool is_cloaked;
			}
		}
	}
#endif
}
