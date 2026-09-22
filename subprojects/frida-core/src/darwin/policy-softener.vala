namespace Frida {
	public interface PolicySoftener : Object {
		public abstract void soften (uint pid) throws Error;
		public abstract void retain (uint pid) throws Error;
		public abstract void release (uint pid);
		public abstract void forget (uint pid);
	}

	public sealed class NullPolicySoftener : Object, PolicySoftener {
		public void soften (uint pid) throws Error {
		}

		public void retain (uint pid) throws Error {
		}

		public void release (uint pid) {
		}

		public void forget (uint pid) {
		}
	}

#if IOS || TVOS
	public class IOSTVOSPolicySoftener : Object, PolicySoftener {
		private Gee.HashMap<uint, ProcessEntry> process_entries = new Gee.HashMap<uint, ProcessEntry> ();

		public void soften (uint pid) throws Error {
			if (process_entries.has_key (pid))
				return;

			var entry = perform_softening (pid);

			var expiry_source = new TimeoutSource.seconds (20);
			expiry_source.set_callback (() => {
				entry.expiry_source = null;

				forget (pid);

				return false;
			});
			expiry_source.attach (MainContext.get_thread_default ());
			entry.expiry_source = expiry_source;
		}

		public void retain (uint pid) throws Error {
			var entry = process_entries[pid];
			if (entry == null)
				entry = perform_softening (pid);
			entry.cancel_expiry ();
			entry.usage_count++;
		}

		public void release (uint pid) {
			var entry = process_entries[pid];
			if (entry == null)
				return;
			assert (entry.usage_count != 0);
			entry.usage_count--;
			if (entry.usage_count == 0) {
				revert_softening (entry);
				process_entries.unset (pid);
			}
		}

		public void forget (uint pid) {
			ProcessEntry entry;
			if (process_entries.unset (pid, out entry))
				entry.cancel_expiry ();
		}

		protected virtual ProcessEntry perform_softening (uint pid) throws Error {
			MemlimitProperties? saved_memory_limits = null;
			if (!DarwinHelperBackend.is_application_process (pid)) {
				saved_memory_limits = try_commit_memlimit_properties (pid, MemlimitProperties.without_limits ());
			}

			var entry = new ProcessEntry (pid, saved_memory_limits);
			process_entries[pid] = entry;

			return entry;
		}

		protected virtual void revert_softening (ProcessEntry entry) {
			if (entry.saved_memory_limits != null)
				try_set_memlimit_properties (entry.pid, entry.saved_memory_limits);
		}

		private static MemlimitProperties? try_commit_memlimit_properties (uint pid, MemlimitProperties props) {
			var previous_props = MemlimitProperties ();
			if (!try_get_memlimit_properties (pid, out previous_props))
				return null;

			if (!try_set_memlimit_properties (pid, props))
				return null;

			return previous_props;
		}

		private static bool try_get_memlimit_properties (uint pid, out MemlimitProperties props) {
			props = MemlimitProperties.with_system_defaults ();
			return memorystatus_control (GET_MEMLIMIT_PROPERTIES, (int32) pid, 0, &props, sizeof (MemlimitProperties)) == 0;
		}

		private static bool try_set_memlimit_properties (uint pid, MemlimitProperties props) {
			return memorystatus_control (SET_MEMLIMIT_PROPERTIES, (int32) pid, 0, &props, sizeof (MemlimitProperties)) == 0;
		}

		protected class ProcessEntry {
			public uint pid;
			public uint usage_count;
			public MemlimitProperties? saved_memory_limits;
			public Source? expiry_source;

			public ProcessEntry (uint pid, MemlimitProperties? saved_memory_limits) {
				this.pid = pid;
				this.usage_count = 0;
				this.saved_memory_limits = saved_memory_limits;
			}

			~ProcessEntry () {
				cancel_expiry ();
			}

			public void cancel_expiry () {
				if (expiry_source != null) {
					expiry_source.destroy ();
					expiry_source = null;
				}
			}
		}

		[CCode (cname = "memorystatus_control")]
		private extern static int memorystatus_control (MemoryStatusCommand command, int32 pid, uint32 flags, void * buffer, size_t buffer_size);

		private enum MemoryStatusCommand {
			SET_MEMLIMIT_PROPERTIES = 7,
			GET_MEMLIMIT_PROPERTIES = 8,
		}

		protected struct MemlimitProperties {
			public int32 active;
			public MemlimitAttributes active_attr;
			public int32 inactive;
			public MemlimitAttributes inactive_attr;

			public MemlimitProperties.with_system_defaults () {
				active = 0;
				active_attr = 0;
				inactive = 0;
				inactive_attr = 0;
			}

			public MemlimitProperties.without_limits () {
				active = int32.MAX;
				active_attr = 0;
				inactive = int32.MAX;
				inactive_attr = 0;
			}
		}

		[Flags]
		protected enum MemlimitAttributes {
			FATAL = 1,
		}
	}

	public sealed class InternalIOSTVOSPolicySoftener : IOSTVOSPolicySoftener {
		private static bool enabled = false;

		public static void enable () {
			enabled = true;
		}

		public static bool is_available () {
			return enabled;
		}

		protected override IOSTVOSPolicySoftener.ProcessEntry perform_softening (uint pid) throws Error {
			_soften (pid);

			return base.perform_softening (pid);
		}

		private extern static void _soften (uint pid) throws Error;
	}

	public sealed class ElectraPolicySoftener : IOSTVOSPolicySoftener {
		private const string LIBJAILBREAK_PATH = "/usr/lib/libjailbreak.dylib";

		private Module libjailbreak;
		private void * jbd_call;

		private uint connection;

		construct {
			try {
				libjailbreak = new Module (LIBJAILBREAK_PATH, LAZY);
			} catch (ModuleError e) {
				assert_not_reached ();
			}

			jbd_call = resolve_symbol ("jbd_call");
			assert (jbd_call != null);

			connection = _internal_jb_connect ();

			entitle_and_platformize (Posix.getpid ());
		}

		~ElectraPolicySoftener () {
			_internal_jb_disconnect (connection);
		}

		public static bool is_available () {
			return FileUtils.test (LIBJAILBREAK_PATH, FileTest.EXISTS);
		}

		protected override IOSTVOSPolicySoftener.ProcessEntry perform_softening (uint pid) throws Error {
			entitle_and_platformize (pid);

			return base.perform_softening (pid);
		}

		private void entitle_and_platformize (uint pid) {
			_internal_jb_entitle_now (jbd_call, connection, (Posix.pid_t) pid);
		}

		private void * resolve_symbol (string name) {
			void * symbol;
			bool found = libjailbreak.symbol (name, out symbol);
			assert (found);
			return symbol;
		}

		private extern static uint _internal_jb_connect ();
		private extern static void _internal_jb_disconnect (uint connection);
		private extern static int _internal_jb_entitle_now (void * jbd_call, uint connection, uint pid);
	}

	public sealed class Unc0verPolicySoftener : IOSTVOSPolicySoftener {
		private const string SUBSTITUTED_PATH = "/usr/libexec/substituted";

		private uint connection;

		construct {
			connection = _internal_connect ();
		}

		~Unc0verPolicySoftener () {
			_internal_disconnect (connection);
		}

		public static bool is_available () {
			return FileUtils.test (SUBSTITUTED_PATH, FileTest.EXISTS);
		}

		protected override IOSTVOSPolicySoftener.ProcessEntry perform_softening (uint pid) throws Error {
			substitute_setup_process (pid);

			return base.perform_softening (pid);
		}

		private void substitute_setup_process (uint pid) {
			_internal_substitute_setup_process (connection, (Posix.pid_t) pid);
		}

		private extern static uint _internal_connect ();
		private extern static void _internal_disconnect (uint connection);
		private extern static void _internal_substitute_setup_process (uint connection, uint pid);
	}
#endif
}
