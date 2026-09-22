namespace Frida {
#if DARWIN
	public sealed class UnwindSitter : Object, Gum.UnwindSectionsProvider {
		public weak ProcessInvader invader {
			get;
			construct;
		}

		public unowned Gum.MemoryRange? range {
			get { return _range; }
		}

		private Gum.MemoryRange? _range;
		private Gum.UnwindBroker broker;

		public UnwindSitter (ProcessInvader invader) {
			Object (invader: invader);
		}

		construct {
			_range = invader.get_memory_range ();

			broker = Gum.UnwindBroker.obtain ();
			broker.add_sections_provider (this);
		}

		public override void dispose () {
			broker.remove_sections_provider (this);

			base.dispose ();
		}

		public bool fill (Gum.Address address, void * info) {
			_fill_unwind_sections (_range.base_address, _range.base_address + _range.size, info);
			return true;
		}

		public extern static void _fill_unwind_sections (Gum.Address invader_start, Gum.Address invader_end, void * info);
	}
#else
	public sealed class UnwindSitter : Object {
		public weak ProcessInvader invader {
			get;
			construct;
		}

		public UnwindSitter (ProcessInvader invader) {
			Object (invader: invader);
		}
	}
#endif
}
