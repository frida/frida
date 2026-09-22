[CCode (cheader_filename = "gum/gum-heap.h", gir_namespace = "FridaGum", gir_version = "1.0")]
namespace Gum {
	public class InstanceTracker : GLib.Object {
		public InstanceTracker ();

		public void begin (Gum.InstanceVTable? vtable = null);
		public void end ();

		public uint peek_total_count (string type_name);
		public GLib.List peek_instances ();
		public void walk_instances (Gum.WalkInstanceFunc func);
	}

	public delegate void WalkInstanceFunc (Gum.InstanceDetails id);

	public struct InstanceVTable {
		void * create_instance;
		void * free_instance;

		void * type_id_to_name;
	}

	public struct InstanceDetails {
		public void * address;
		public uint ref_count;
		public string type_name;
	}

	public class BoundsChecker : GLib.Object {
		public BoundsChecker (Gum.Backtracer? backtracer = null, Gum.BoundsOutputFunc? output = null);

		public uint pool_size { get; set; }
		public uint front_alignment { get; set; }

		public void attach ();
		public void attach_to_apis (Gum.HeapApiList apis);
		public void detach ();
	}

	public delegate void BoundsOutputFunc (string text);
}
