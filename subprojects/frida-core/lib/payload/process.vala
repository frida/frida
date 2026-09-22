namespace Frida {
	public extern void libc_shim_prepare_to_deinit ();
#if DARWIN
	public extern void libc_shim_deinit ();
#endif
	public extern void run_atexit_handlers ();

	public extern uint get_process_id ();

	public extern void * get_current_pthread ();
	public extern void join_pthread (void * thread);

	public extern void kill_process (uint pid);

	public string get_executable_path () {
		var path = try_get_executable_path ();
		if (path != null)
			return path;

		Gum.Process.enumerate_modules ((details) => {
			path = details.name;
			return false;
		});
		assert (path != null);

		return path;
	}

	private extern string? try_get_executable_path ();

	public Gum.MemoryRange detect_own_range_and_path (Gum.MemoryRange? mapped_range, out string? path) {
		Gum.MemoryRange? own_range = mapped_range;
		string? own_path = null;

		if (own_range == null) {
			Gum.Address our_address = Gum.Address.from_pointer (Gum.strip_code_pointer ((void *) detect_own_range_and_path));

			Gum.Process.enumerate_modules ((details) => {
				var range = details.range;

				if (our_address >= range.base_address && our_address < range.base_address + range.size) {
					own_range = range;
					own_path = details.path;
					return false;
				}

				return true;
			});

			if (own_range == null) {
				Gum.Process.enumerate_ranges (READ | EXECUTE, details => {
					var range = details.range;

					if (our_address >= range.base_address && our_address < range.base_address + range.size) {
						own_range = range;
						if (details.file != null)
							own_path = details.file.path;
						return false;
					}

					return true;
				});
			}

			assert (own_range != null);
			assert (own_path != null);
		}

		path = own_path;

		return own_range;
	}

	public interface ProcessInvader : Object {
		public abstract SpawnStartState query_current_spawn_state ();

		public abstract Gum.MemoryRange get_memory_range ();

		public abstract Gum.ScriptBackend get_script_backend (ScriptRuntime runtime) throws Error;
		public abstract Gum.ScriptBackend? get_active_script_backend ();

		public abstract void acquire_child_gating () throws Error;
		public abstract void release_child_gating ();

		public abstract async PortalMembershipId join_portal (string address, PortalOptions options,
			Cancellable? cancellable) throws Error, IOError;
		public abstract async void leave_portal (PortalMembershipId membership_id, Cancellable? cancellable) throws Error, IOError;
	}

	public enum TerminationReason {
		UNLOAD,
		EXIT,
		EXEC;

		public string to_nick () {
			return Marshal.enum_to_nick<TerminationReason> (this);
		}
	}
}
