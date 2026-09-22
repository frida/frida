public sealed class Frida.SyscallTracer : Object {
	public signal void events_available ();

	public State state {
		get {
			return _state;
		}
	}

	public SymbolResolver resolver {
		get;
		default = new SymbolResolver ();
	}

	public enum State {
		STOPPED,
		STARTED,
	}

	private State _state = STOPPED;

	private BpfMap? target_tgids;
	private BpfMap? target_uids;
	private BpfMap? excluded_syscalls;

	private BpfRingbufReader? syscall_events_reader;
	private IOChannel? syscall_events_channel;
	private Source? syscall_events_source;

	private BpfRingbufReader? map_events_reader;
	private IOChannel? map_events_channel;
	private Source? map_events_source;

	private BpfMap? stacks;
	private BpfMap? process_states;
	private BpfMap? stats;

	private Gee.Collection<BpfLink> links = new Gee.ArrayList<BpfLink> ();

	public const size_t SYSCALL_NARGS = 6;

	protected override void dispose () {
		stop ();

		base.dispose ();
	}

	public void start () throws Error {
		assert (state == STOPPED);

		var obj = BpfObject.open ("syscall-tracer.elf", Frida.Data.HelperBackend.get_syscall_tracer_elf_blob ().data);

		target_tgids = obj.maps.get_by_name ("target_tgids");
		target_uids = obj.maps.get_by_name ("target_uids");
		excluded_syscalls = obj.maps.get_by_name ("excluded_syscalls");
		var syscall_events = obj.maps.get_by_name ("syscall_events");
		var map_events = obj.maps.get_by_name ("map_events");
		stacks = obj.maps.get_by_name ("stacks");
		process_states = obj.maps.get_by_name ("process_states");
		stats = obj.maps.get_by_name ("stats");

		obj.prepare ();

		syscall_events_reader = new BpfRingbufReader (syscall_events);
		map_events_reader = new BpfRingbufReader (map_events);

		obj.load ();

		foreach (var program in obj.programs) {
			var link = program.attach ();
			links.add (link);
		}

		syscall_events_channel = new IOChannel.unix_new (syscall_events.fd);
		arm_syscall_events_watch ();

		map_events_channel = new IOChannel.unix_new (map_events.fd);
		arm_map_events_watch ();

		_state = STARTED;
	}

	public void stop () {
		syscall_events_source?.destroy ();
		syscall_events_source = null;
		syscall_events_channel = null;
		syscall_events_reader = null;

		map_events_source?.destroy ();
		map_events_source = null;
		map_events_channel = null;
		map_events_reader = null;

		links.clear ();

		stats = null;
		process_states = null;
		stacks = null;
		excluded_syscalls = null;
		target_uids = null;
		target_tgids = null;

		_state = STOPPED;
	}

	public void add_target_tgid (uint tgid) throws Error {
		target_tgids.update_u32_u8 (tgid, 1);
	}

	public void remove_target_tgid (uint tgid) throws Error {
		target_tgids.remove_u32 (tgid);
	}

	public void add_target_uid (uint uid) throws Error {
		target_uids.update_u32_u8 (uid, 1);
	}

	public void remove_target_uid (uint uid) throws Error {
		target_uids.remove_u32 (uid);
	}

	public void exclude_syscall (Abi abi, uint nr) throws Error {
		uint64 key = ((uint64) abi << 32) | (uint64) nr;
		excluded_syscalls.update_u64_u8 (key, 1);
	}

	public void include_syscall (Abi abi, uint nr) throws Error {
		uint64 key = ((uint64) abi << 32) | (uint64) nr;
		excluded_syscalls.remove_u64 (key);
	}

	public DrainStatus drain_events (SyscallEventHandler on_event) {
		var status = syscall_events_reader.drain (payload => {
			assert (payload.length >= sizeof (SyscallEvent));
			var e = (Event *) payload;
			var se = (SyscallEvent *) payload;
			SyscallEventView ev = { e, se, payload };

			return (on_event (ev) == CONTINUE)
					? BpfRingbufReader.RecordAction.CONTINUE
					: BpfRingbufReader.RecordAction.STOP;
		});

		if (status == DRAINED)
			arm_syscall_events_watch ();

		return (status == DRAINED)
			? DrainStatus.DRAINED
			: DrainStatus.STOPPED;
	}

	public enum DrainStatus {
		DRAINED,
		STOPPED,
	}

	public delegate EventFlow SyscallEventHandler (SyscallEventView ev);

	public enum EventFlow {
		CONTINUE,
		STOP,
	}

	public struct SyscallEventView {
		public Event * event;
		public SyscallEvent * se;
		public unowned uint8[] bytes;
	}

	private void arm_syscall_events_watch () {
		if (syscall_events_source != null)
			return;

		var src = new IOSource (syscall_events_channel, IOCondition.IN);
		var state = new SyscallEventsWatchState (this);
		src.set_callback (state.on_ready);
		src.attach (MainContext.get_thread_default ());
		syscall_events_source = src;
	}

	private class SyscallEventsWatchState : Object {
		public unowned SyscallTracer tracer;

		public SyscallEventsWatchState (SyscallTracer tracer) {
			this.tracer = tracer;
		}

		public bool on_ready (IOChannel ch, IOCondition cond) {
			tracer.on_syscall_events_readable ();
			return Source.REMOVE;
		}
	}

	private void on_syscall_events_readable () {
		syscall_events_source?.destroy ();
		syscall_events_source = null;

		events_available ();
	}

	private void arm_map_events_watch () {
		assert (map_events_source == null);

		var src = new IOSource (map_events_channel, IOCondition.IN);
		var state = new MapEventsWatchState (this);
		src.set_callback (state.on_ready);
		src.attach (MainContext.get_thread_default ());
		map_events_source = src;
	}

	private class MapEventsWatchState : Object {
		public unowned SyscallTracer tracer;

		public MapEventsWatchState (SyscallTracer tracer) {
			this.tracer = tracer;
		}

		public bool on_ready (IOChannel ch, IOCondition cond) {
			tracer.on_map_events_readable ();
			return Source.CONTINUE;
		}
	}

	private void on_map_events_readable () {
		map_events_reader.drain (payload => {
			assert (payload.length >= sizeof (Event));
			on_map_event ((Event *) payload);

			return CONTINUE;
		});
	}

	private void on_map_event (Event * ev) {
		var type = (EventType) ev->type;

		switch (type) {
			case NEED_SNAPSHOT: {
				try {
					resolver.refresh_snapshot (ev->tgid);

					var s = ProcessState ();
					process_states.lookup_raw ((uint8[]) &ev->tgid, (uint8[]) &s);
					s.map_gen = 1;
					process_states.update_raw ((uint8[]) &ev->tgid, (uint8[]) &s);
				} catch (GLib.Error e) {
					try {
						process_states.remove_u32 (ev->tgid);
					} catch (Error _) {
					}
				}

				Posix.kill ((Posix.pid_t) ev->tgid, Posix.Signal.CONT);

				break;
			}
			case MAP_CREATE: {
				var me = (MapCreateEvent *) ev;

				unowned string? filename = null;
				if (ev->attachment_count != 0) {
					var h = (AttachmentHeader *) ((uint8 *) &me->gen + sizeof (uint32));
					filename = (string) (h + 1);
				}

				uint64 file_offset = me->pgoff * Gum.query_page_size ();

				resolver.apply_map_create (
					me->parent.tgid,
					me->gen,
					me->start,
					me->end,
					file_offset,
					me->vm_flags,
					DevId.unpack (me->device),
					me->inode,
					filename
				);

				break;
			}
			case MAP_DESTROY_RANGE: {
				var de = (MapDestroyRangeEvent *) ev;

				resolver.apply_map_destroy_range (
					de->parent.tgid,
					de->gen,
					de->start,
					de->end
				);

				break;
			}
			default:
				break;
		}
	}

	public struct Event {
		public uint64 time_ns;
		public uint32 tgid;
		public uint32 tid;

		public uint16 type;

		public uint16 attachment_count;
	}

	public enum EventType {
		SYSCALL_ENTER,
		SYSCALL_EXIT,

		NEED_SNAPSHOT,
		MAP_CREATE,
		MAP_DESTROY_RANGE,
	}

	public struct AttachmentHeader {
		public uint16 type;
		public uint16 arg_index;
		public uint16 capacity;
		public uint16 size;
	}

	public enum AttachmentType {
		STRING,
		BYTES,
	}

	public struct SyscallEvent {
		public Event parent;

		public int32 syscall_nr;
		public int32 stack_id;
		public uint32 map_gen;
	}

	public struct SyscallEnterEvent {
		public SyscallEvent parent;

		public uint64 args[6];
	}

	public struct SyscallExitEvent {
		public SyscallEvent parent;

		public int64 retval;
	}

	private struct MapCreateEvent {
		public Event parent;

		public uint64 start;
		public uint64 end;

		public uint64 pgoff;
		public uint64 vm_flags;

		public uint64 device;
		public uint64 inode;

		public uint32 gen;
	}

	private struct MapDestroyRangeEvent {
		public Event parent;

		public uint64 start;
		public uint64 end;

		public uint32 gen;
	}

	private struct ProcessState {
		public uint32 abi;
		public uint32 map_gen;
	}

	public enum Abi {
		INVALID,
		NATIVE,
		COMPAT32,
	}

	public Abi get_process_abi (uint32 tgid) throws Error {
		var s = ProcessState ();
		process_states.lookup_raw ((uint8[]) &tgid, (uint8[]) &s);
		return s.abi;
	}

	public void resolve_stacks (uint32[] stack_ids, StackFramesHandler on_frames) {
		var tmp = new uint64[stacks.value_size / sizeof (uint64)];

		foreach (var sid in stack_ids) {
			uint n = 0;

			try {
				stacks.lookup_raw ((uint8[]) &sid, (uint8[]) tmp);

				for (uint i = 0; i != tmp.length; i++) {
					if (tmp[i] == 0)
						break;
					n++;
				}
			} catch (Error e) {
			}

			on_frames (sid, tmp[:n]);
		}
	}

	public delegate void StackFramesHandler (uint32 stack_id, uint64[] frames);

	public Stats read_stats () throws Error {
		Stats total = Stats ();

		uint32 key = 0;
		stats.foreach_percpu_value<Stats?> ((uint8[]) &key, (cpu, s) => {
			total.emitted_events += s.emitted_events;
			total.emitted_bytes += s.emitted_bytes;

			total.dropped_events += s.dropped_events;
			total.dropped_bytes += s.dropped_bytes;
		});

		return total;
	}

	public struct Stats {
		public uint64 emitted_events;
		public uint64 emitted_bytes;

		public uint64 dropped_events;
		public uint64 dropped_bytes;
	}
}
