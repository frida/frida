namespace Frida {
	public sealed class ActivitySampler : Object {
		public uint pid {
			get;
			construct;
		}

		public Gee.List<SampledStack> stacks {
			get {
				return collected_stacks;
			}
		}

		private BpfRingbufReader? events_reader;
		private Source? events_source;

		private Gee.Collection<FileDescriptor> perf_event_fds = new Gee.ArrayList<FileDescriptor> ();
		private Gee.Collection<BpfLink> links = new Gee.ArrayList<BpfLink> ();

		private Gee.List<SampledStack> collected_stacks = new Gee.ArrayList<SampledStack> ();

		private const uint64 SAMPLE_PERIOD_NS = 1000000;

		public ActivitySampler (uint pid) {
			Object (pid: pid);
		}

		protected override void dispose () {
			stop ();

			base.dispose ();
		}

		public void start () throws Error {
			var obj = BpfObject.open ("activity-sampler.elf", Frida.Data.HelperBackend.get_activity_sampler_elf_blob ().data);

			var target_tgid = obj.maps.get_by_name ("target_tgid");
			var events = obj.maps.get_by_name ("events");

			obj.prepare ();

			target_tgid.update_u32_u32 (0, pid);
			events_reader = new BpfRingbufReader (events);

			obj.load ();

			BpfProgram program = obj.programs.get_by_name ("on_perf_event");

			uint ncpus = get_num_processors ();
			for (uint cpu = 0; cpu != ncpus; cpu++) {
				var pea = PerfEventAttr ();
				pea.event_type = SOFTWARE;
				pea.size = (uint32) sizeof (PerfEventAttr);
				pea.config = PERF_EVENT_COUNT_SW_CPU_CLOCK;
				pea.sample_period = SAMPLE_PERIOD_NS;

				var pefd = PerfEvent.open (&pea, -1, (int) cpu, -1, 0);
				perf_event_fds.add (pefd);

				var link = program.attach_perf_event (pefd);
				links.add (link);
			}

			var ch = new IOChannel.unix_new (events.fd);
			var src = new IOSource (ch, IN);
			var state = new WatchState (this, events_reader);
			src.set_callback (state.on_ready);
			src.attach (MainContext.get_thread_default ());
			events_source = src;
		}

		public void stop () {
			events_source?.destroy ();
			events_source = null;

			links.clear ();

			events_reader = null;
		}

		private void handle_sample (SampleEvent * e) {
			if (e->depth == 0)
				return;
			var frames = new uint64[e->depth];
			Memory.copy (frames, e->ips, e->depth * sizeof (uint64));
			// XXX: Should we trim these?
			e->tgid; e->tid; e->time_ns; e->stack_err;
			collected_stacks.add (new SampledStack ((owned) frames));
		}

		private struct SampleEvent {
			public uint64 time_ns;
			public uint32 tgid;
			public uint32 tid;
			public int32 stack_err;
			public uint32 depth;
			public uint64 ips[16];
		}

		private class WatchState : Object {
			public unowned ActivitySampler sampler;
			public BpfRingbufReader reader;

			public WatchState (ActivitySampler sampler, BpfRingbufReader reader) {
				this.sampler = sampler;
				this.reader = reader;
			}

			public bool on_ready (IOChannel ch, IOCondition cond) {
				reader.drain (payload => {
					assert (payload.length == sizeof (SampleEvent));
					sampler.handle_sample ((SampleEvent *) payload);
					return CONTINUE;
				});
				return Source.CONTINUE;
			}
		}
	}

	public sealed class SampledStack {
		public uint64[] frames;

		public SampledStack (owned uint64[] frames) {
			this.frames = (owned) frames;
		}
	}
}
