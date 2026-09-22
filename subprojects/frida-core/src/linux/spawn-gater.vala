namespace Frida {
	public sealed class SpawnGater : Object {
		public signal void gating_cancelled ();
		public signal void spawn_added (HostSpawnInfo info);
		public signal void spawn_removed (HostSpawnInfo info);

		public LinuxHelper helper {
			get;
			construct;
		}

		public State state {
			get {
				return _state;
			}
		}

		public enum State {
			STOPPED,
			STARTED,
		}

		private State _state = STOPPED;

		private Gee.Map<uint, HostSpawnInfo?> pending_spawn = new Gee.HashMap<uint, HostSpawnInfo?> ();
		private SpawnGatingWatchdog watchdog = new SpawnGatingWatchdog ();

		private BpfRingbufReader? events_reader;
		private IOChannel? events_channel;
		private Source? events_source;

		private Gee.Collection<BpfLink> links = new Gee.ArrayList<BpfLink> ();

		private const size_t RINGBUF_SIZE = 1U << 22;
		private const size_t MAX_FILENAME = 256;

		// bpf_send_signal(SIGSTOP) races our SIGCONT: one that lands first is dropped, so re-send
		// until the task is observed running.
		private const uint RESUME_MAX_ATTEMPTS = 20;
		private const uint RESUME_RETRY_MSEC = 25;

		public SpawnGater (LinuxHelper helper) {
			Object (helper: helper);
		}

		construct {
			watchdog.expired.connect (on_watchdog_expired);
		}

		protected override void dispose () {
			stop ();

			base.dispose ();
		}

		private void on_watchdog_expired () {
			stop ();
			gating_cancelled ();
		}

		public void start () throws Error {
			assert (state == STOPPED);

			var obj = BpfObject.open ("spawn-gater.elf", Frida.Data.HelperBackend.get_spawn_gater_elf_blob ().data);

			var events = obj.maps.get_by_name ("events");

			obj.prepare ();

			events_reader = new BpfRingbufReader (events);

			obj.load ();

			foreach (var program in obj.programs) {
				var link = program.attach ();
				links.add (link);
			}

			events_channel = new IOChannel.unix_new (events.fd);
			var src = new IOSource (events_channel, IOCondition.IN);
			var state = new EventsWatchState (this);
			src.set_callback (state.on_ready);
			src.attach (MainContext.get_thread_default ());
			events_source = src;

			_state = STARTED;
		}

		public void stop () {
			if (_state == STOPPED)
				return;

			links.clear ();

			events_source?.destroy ();
			events_source = null;

			process_pending_events ();

			foreach (var spawn in pending_spawn.values) {
				spawn_removed (spawn);
				perform_resume.begin (spawn.pid);
			}
			pending_spawn.clear ();
			watchdog.clear ();

			events_channel = null;
			events_reader = null;

			_state = STOPPED;
		}

		public HostSpawnInfo[] enumerate_pending_spawn () {
			var result = new HostSpawnInfo[pending_spawn.size];
			var index = 0;
			foreach (var spawn in pending_spawn.values)
				result[index++] = spawn;
			return result;
		}

		public bool try_resume (uint pid) {
			HostSpawnInfo? spawn;
			if (!pending_spawn.unset (pid, out spawn))
				return false;
			watchdog.cancel (pid);
			spawn_removed (spawn);
			perform_resume.begin (pid);
			return true;
		}

		private async void perform_resume (uint pid) {
			try {
				yield helper.resume (pid, null);
			} catch (GLib.Error e) {
				if (e is Error.INVALID_ARGUMENT)
					yield resume_external (pid);
			}
		}

		private async void resume_external (uint pid) {
			for (uint attempt = 0; attempt != RESUME_MAX_ATTEMPTS; attempt++) {
				if (Posix.kill ((Posix.pid_t) pid, Posix.Signal.CONT) != 0)
					return;
				if (!process_is_stopped (pid))
					return;

				var source = new TimeoutSource (RESUME_RETRY_MSEC);
				source.set_callback (resume_external.callback);
				source.attach (MainContext.get_thread_default ());
				yield;
			}
		}

		private static bool process_is_stopped (uint pid) {
			string contents;
			try {
				FileUtils.get_contents ("/proc/%u/stat".printf (pid), out contents);
			} catch (FileError e) {
				return false;
			}

			// State is the char two past the final ')' (comm can itself contain ')').
			char state = contents[contents.last_index_of_char (')') + 2];
			return state == 'T' || state == 't';
		}

		private void process_pending_events () {
			events_reader.drain (payload => {
				assert (payload.length == sizeof (ExecveEvent));
				handle_event (payload);
				return CONTINUE;
			});
		}

		private void handle_event (ExecveEvent * e) {
			var info = HostSpawnInfo (e->pid, (string) e->command);
			pending_spawn[e->pid] = info;
			watchdog.arm (e->pid);
			spawn_added (info);
		}

		private struct ExecveEvent {
			public int pid;
			public char command[MAX_FILENAME];
		}

		private class EventsWatchState : Object {
			public unowned SpawnGater gater;

			public EventsWatchState (SpawnGater gater) {
				this.gater = gater;
			}

			public bool on_ready (IOChannel ch, IOCondition cond) {
				gater.process_pending_events ();
				return Source.CONTINUE;
			}
		}
	}
}
