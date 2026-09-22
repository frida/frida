namespace Gum {
	[CCode (cheader_filename = "gumjs/guminspectorserver.h")]
	public class InspectorServer : GLib.Object {
		public signal void message (string message);

		public uint port {
			get;
			construct;
		}

		public InspectorServer ();
		public InspectorServer.with_port (uint port);

		public bool start () throws Gum.Error;
		public void stop ();

		public void post_message (string message);
	}
}
