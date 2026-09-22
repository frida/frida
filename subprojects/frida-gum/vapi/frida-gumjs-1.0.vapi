[CCode (cheader_filename = "gumjs/gumjs.h", lower_case_cprefix = "gumjs_")]
namespace GumJS {
	public void prepare_to_fork ();
	public void recover_from_fork_in_parent ();
	public void recover_from_fork_in_child ();
}

namespace Gum {
	[CCode (cheader_filename = "gumjs/gumscriptbackend.h", type_cname = "GumScriptBackendInterface")]
	public interface ScriptBackend : GLib.Object {
		public delegate void LockedFunc ();

		public static unowned ScriptBackend obtain ();
		public static unowned ScriptBackend? obtain_qjs ();
		public static unowned ScriptBackend? obtain_v8 ();

		public async Script create (string name, string source, GLib.Bytes? snapshot = null, GLib.Cancellable? cancellable = null)
			throws Gum.Error;
		public Script create_sync (string name, string source, GLib.Bytes? snapshot = null, GLib.Cancellable? cancellable = null)
			throws Gum.Error;
		public async Script create_from_bytes (GLib.Bytes bytes, GLib.Bytes? snapshot = null, GLib.Cancellable? cancellable = null)
			throws Gum.Error;
		public Script create_from_bytes_sync (GLib.Bytes bytes, GLib.Bytes? snapshot = null, GLib.Cancellable? cancellable = null)
			throws Gum.Error;

		public async GLib.Bytes compile (string name, string source, GLib.Cancellable? cancellable = null) throws Gum.Error;
		public GLib.Bytes compile_sync (string name, string source, GLib.Cancellable? cancellable = null) throws Gum.Error;
		public async GLib.Bytes snapshot (string embed_script, string? warmup_script, GLib.Cancellable? cancellable = null)
			throws Gum.Error;
		public GLib.Bytes snapshot_sync (string embed_script, string? warmup_script, GLib.Cancellable? cancellable = null)
			throws Gum.Error;

		public static unowned ScriptScheduler get_scheduler ();

		public void with_lock_held (Gum.ScriptBackend.LockedFunc func);
		public bool is_locked ();
	}

	[CCode (cheader_filename = "gumjs/gumscript.h", type_cname = "GumScriptInterface")]
	public interface Script : GLib.Object {
		public delegate void MessageHandler (string message, GLib.Bytes? data);
		public delegate void DebugMessageHandler (string message);

		public async void load (GLib.Cancellable? cancellable = null);
		public void load_sync (GLib.Cancellable? cancellable = null);
		public async void unload (GLib.Cancellable? cancellable = null);
		public void unload_sync (GLib.Cancellable? cancellable = null);
		public void interrupt ();
		public void terminate ();

		public void set_message_handler (owned Gum.Script.MessageHandler? handler);
		public void post (string message, GLib.Bytes? data = null);

		public void set_debug_message_handler (owned Gum.Script.DebugMessageHandler? handler);
		public void post_debug_message (string message);

		public unowned Stalker get_stalker ();
	}

	[CCode (cheader_filename = "gumjs/gumscriptscheduler.h")]
	public class ScriptScheduler : GLib.Object {
		public void enable_background_thread ();
		public void disable_background_thread ();
		public void start ();
		public void stop ();

		public unowned GLib.MainContext get_js_context ();

		public void push_job_on_js_thread (int priority, owned ScriptJob.Func func);
		public void push_job_on_thread_pool (owned ScriptJob.Func func);
	}

	[Compact]
	[CCode (cheader_filename = "gumjs/gumscriptscheduler.h", free_function = "gum_script_job_free")]
	public class ScriptJob {
		public delegate void Func ();

		public ScriptJob (ScriptScheduler scheduler, owned Func func);
	}
}
