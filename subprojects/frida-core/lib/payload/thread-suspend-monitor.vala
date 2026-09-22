namespace Frida {
#if DARWIN
	public sealed class ThreadSuspendMonitor : Object {
		public weak ProcessInvader invader {
			get;
			construct;
		}

		private TaskThreadsFunc task_threads;
		private ThreadSuspendFunc thread_resume;
		private ThreadResumeFunc thread_suspend;

		private const string LIBSYSTEM_KERNEL = "/usr/lib/system/libsystem_kernel.dylib";

		[CCode (has_target = false)]
		private delegate int TaskThreadsFunc (uint task_id, uint ** threads, uint * count);
		[CCode (has_target = false)]
		private delegate int ThreadSuspendFunc (uint thread_id);
		[CCode (has_target = false)]
		private delegate int ThreadResumeFunc (uint thread_id);

		public ThreadSuspendMonitor (ProcessInvader invader) {
			Object (invader: invader);
		}

		construct {
			var interceptor = Gum.Interceptor.obtain ();

			var kernel = Gum.Process.find_module_by_name (LIBSYSTEM_KERNEL);
			task_threads = (TaskThreadsFunc) kernel.find_export_by_name ("task_threads");
			thread_suspend = (ThreadSuspendFunc) kernel.find_export_by_name ("thread_suspend");
			thread_resume = (ThreadResumeFunc) kernel.find_export_by_name ("thread_resume");

			var replace_options = Gum.ReplaceOptions ();
			replace_options.replacement_data = this;
			interceptor.replace ((void *) task_threads, (void *) replacement_task_threads, null, replace_options);
			interceptor.replace ((void *) thread_suspend, (void *) replacement_thread_suspend, null, replace_options);
			interceptor.replace ((void *) thread_resume, (void *) replacement_thread_resume, null, replace_options);
		}

		public override void dispose () {
			var interceptor = Gum.Interceptor.obtain ();

			interceptor.revert ((void *) task_threads);
			interceptor.revert ((void *) thread_suspend);
			interceptor.revert ((void *) thread_resume);

			base.dispose ();
		}

		private static int replacement_task_threads (uint task_id, uint ** threads, uint * count) {
			unowned Gum.InvocationContext context = Gum.Interceptor.get_current_invocation ();
			unowned ThreadSuspendMonitor monitor = (ThreadSuspendMonitor) context.get_replacement_data ();

			if (monitor.is_called_by_frida (context))
				return monitor.task_threads (task_id, threads, count);

			return monitor.handle_task_threads (task_id, threads, count);
		}

		private int handle_task_threads (uint task_id, uint ** threads, uint * count) {
			int result = task_threads (task_id, threads, count);

			_remove_cloaked_threads (task_id, threads, count);

			return result;
		}

		public extern static void _remove_cloaked_threads (uint task_id, uint ** threads, uint * count);

		private static int replacement_thread_suspend (uint thread_id) {
			unowned Gum.InvocationContext context = Gum.Interceptor.get_current_invocation ();
			unowned ThreadSuspendMonitor monitor = (ThreadSuspendMonitor) context.get_replacement_data ();

			if (monitor.is_called_by_frida (context))
				return monitor.thread_suspend (thread_id);

			return monitor.handle_thread_suspend (thread_id);
		}

		private int handle_thread_suspend (uint thread_id) {
			if (Gum.Cloak.has_thread (thread_id))
				return 0;

			var script_backend = invader.get_active_script_backend ();
			uint caller_thread_id = (uint) Gum.Process.get_current_thread_id ();
			if (script_backend == null || thread_id == caller_thread_id)
				return thread_suspend (thread_id);

			var interceptor = Gum.Interceptor.obtain ();

			int result = 0;
			while (true) {
				script_backend.with_lock_held (() => {
					interceptor.with_lock_held (() => {
						Gum.Cloak.with_lock_held (() => {
							result = thread_suspend (thread_id);
						});
					});
				});

				if (result != 0 || (!script_backend.is_locked () && !Gum.Cloak.is_locked () && !interceptor.is_locked ()))
					break;

				if (thread_resume (thread_id) != 0)
					break;
			}

			return result;
		}

		private static int replacement_thread_resume (uint thread_id) {
			unowned Gum.InvocationContext context = Gum.Interceptor.get_current_invocation ();
			unowned ThreadSuspendMonitor monitor = (ThreadSuspendMonitor) context.get_replacement_data ();

			if (monitor.is_called_by_frida (context))
				return monitor.thread_resume (thread_id);

			return monitor.handle_thread_resume (thread_id);
		}

		private int handle_thread_resume (uint thread_id) {
			if (Gum.Cloak.has_thread (thread_id))
				return 0;

			return thread_resume (thread_id);
		}

		private bool is_called_by_frida (Gum.InvocationContext context) {
			Gum.MemoryRange range = invader.get_memory_range ();
			var caller = Gum.Address.from_pointer (context.get_return_address ());
			return caller >= range.base_address && caller < range.base_address + range.size;
		}
	}
#else
	public sealed class ThreadSuspendMonitor : Object {
		public weak ProcessInvader invader {
			get;
			construct;
		}

		public ThreadSuspendMonitor (ProcessInvader invader) {
			Object (invader: invader);
		}
	}
#endif
}
