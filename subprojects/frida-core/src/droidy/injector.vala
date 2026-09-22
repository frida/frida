[CCode (gir_namespace = "FridaDroidyInjector", gir_version = "1.0")]
namespace Frida.Droidy.Injector {
	public static async GadgetDetails inject (InputStream gadget, string package, string device_serial, Cancellable? cancellable = null)
			throws Error, IOError {
		var session = new Session (gadget, package, device_serial);
		return yield session.run (cancellable);
	}

	public sealed class GadgetDetails : Object {
		public uint pid {
			get;
			construct;
		}

		public string unix_socket_path {
			get;
			construct;
		}

		public JDWP.Client jdwp {
			get;
			construct;
		}

		public GadgetDetails (uint pid, string unix_socket_path, JDWP.Client jdwp) {
			Object (
				pid: pid,
				unix_socket_path: unix_socket_path,
				jdwp: jdwp
			);
		}
	}

	private sealed class Session : Object {
		public InputStream gadget {
			get;
			construct;
		}

		public string package {
			get;
			construct;
		}

		public string device_serial {
			get;
			construct;
		}

		public Session (InputStream gadget, string package, string device_serial) {
			Object (
				gadget: gadget,
				package: package,
				device_serial: device_serial
			);
		}

		public async GadgetDetails run (Cancellable? cancellable) throws Error, IOError {
			var existing_gadget = yield setup (cancellable);
			if (existing_gadget != null) {
				return existing_gadget;
			}

			var result = yield inject_gadget (cancellable);

			yield teardown (cancellable);

			return result;
		}

		private async GadgetDetails? setup (Cancellable? cancellable) throws Error, IOError {
			return null;
		}

		private async void teardown (Cancellable? cancellable) throws Error, IOError {
		}

		private async GadgetDetails inject_gadget (Cancellable? cancellable) throws Error, IOError {
			string instance_id = Uuid.string_random ().replace ("-", "");
			string so_path_shared = "/data/local/tmp/frida-gadget-" + instance_id + ".so";
			string so_path_app = "/data/data/" + package + "/gadget.so";
			string config_path_shared = "/data/local/tmp/frida-gadget-" + instance_id + ".config";
			string config_path_app = "/data/data/" + package + "/gadget.config";
			string unix_socket_path = "frida:" + package;

			bool waiting = false;
			uint target_pid = 0;
			JDWP.BreakpointEvent? breakpoint_event = null;

			var shell = new Droidy.ShellSession ();
			yield shell.open (device_serial, cancellable);
			try {
				var so_meta = new Droidy.FileMetadata ();
				so_meta.mode = 0100755;
				so_meta.time_modified = new DateTime.now_utc ();

				yield Droidy.FileSync.send (gadget, so_meta, so_path_shared, device_serial, cancellable);

				var config = new Json.Builder ();
				config
					.begin_object ()
						.set_member_name ("interaction")
						.begin_object ()
							.set_member_name ("type")
							.add_string_value ("listen")
							.set_member_name ("address")
							.add_string_value ("unix:" + unix_socket_path)
							.set_member_name ("on_load")
							.add_string_value ("resume")
						.end_object ()
						.set_member_name ("teardown")
						.add_string_value ("full")
					.end_object ();
				string raw_config = Json.to_string (config.get_root (), false);
				var config_meta = new Droidy.FileMetadata ();
				config_meta.mode = 0100644;
				config_meta.time_modified = so_meta.time_modified;
				yield Droidy.FileSync.send (new MemoryInputStream.from_data (raw_config.data), config_meta,
					config_path_shared, device_serial, cancellable);

				yield shell.check_call ("am set-debug-app -w --persistent '%s'".printf (package), cancellable);

				yield shell.check_call ("am force-stop '%s'".printf (package), cancellable);

				var tracker = new Droidy.JDWPTracker ();
				yield tracker.open (device_serial, cancellable);

				var attached_handler = tracker.debugger_attached.connect (pid => {
					target_pid = pid;
					if (waiting)
						inject_gadget.callback ();
				});
				try {
					yield shell.check_call (
						"am start -D $(cmd package resolve-activity --brief '%s'| tail -n 1)".printf (package),
						cancellable);

					if (target_pid == 0) {
						waiting = true;
						yield;
						waiting = false;
					}
				} finally {
					tracker.disconnect (attached_handler);
				}

				yield tracker.close (cancellable);

				JDWP.Client jdwp;
				{
					var c = yield Droidy.Client.open (cancellable);
					yield c.request ("host:transport:" + device_serial, cancellable);
					yield c.request_protocol_change ("jdwp:%u".printf (target_pid), cancellable);

					jdwp = yield JDWP.Client.open (c.stream, cancellable);
				}

				var activity_class = yield jdwp.get_class_by_signature ("Landroid/app/Activity;", cancellable);
				var activity_methods = yield jdwp.get_methods (activity_class.ref_type.id, cancellable);
				foreach (var method in activity_methods) {
					if (method.name == "onCreate") {
						yield jdwp.set_event_request (BREAKPOINT, JDWP.SuspendPolicy.EVENT_THREAD,
							new JDWP.EventModifier[] {
								new JDWP.LocationOnlyModifier (activity_class.ref_type, method.id),
							});
					}
				}

				var breakpoint_handler = jdwp.events_received.connect (events => {
					breakpoint_event = (JDWP.BreakpointEvent) events.items[0];
					if (waiting)
						inject_gadget.callback ();
				});
				try {
					yield jdwp.resume (cancellable);

					if (breakpoint_event == null) {
						waiting = true;
						yield;
						waiting = false;
					}
				} finally {
					jdwp.disconnect (breakpoint_handler);
				}

				yield jdwp.clear_all_breakpoints (cancellable);

				var runtime_class = yield jdwp.get_class_by_signature ("Ljava/lang/Runtime;", cancellable);
				var runtime_methods = yield jdwp.get_methods (runtime_class.ref_type.id, cancellable);
				var get_runtime_method = JDWP.MethodID (0);
				var exec_method = JDWP.MethodID (0);
				var load_method = JDWP.MethodID (0);
				foreach (var method in runtime_methods) {
					if (method.name == "getRuntime" && method.signature == "()Ljava/lang/Runtime;") {
						get_runtime_method = method.id;
					} else if (method.name == "exec" && method.signature == "(Ljava/lang/String;)Ljava/lang/Process;") {
						exec_method = method.id;
					} else if (method.name == "load" && method.signature == "(Ljava/lang/String;)V") {
						load_method = method.id;
					}
				}
				assert (get_runtime_method.handle != 0 && exec_method.handle != 0 && load_method.handle != 0);

				var process_class = yield jdwp.get_class_by_signature ("Ljava/lang/Process;", cancellable);
				var process_methods = yield jdwp.get_methods (process_class.ref_type.id, cancellable);
				var wait_for_method = JDWP.MethodID (0);
				foreach (var method in process_methods) {
					if (method.name == "waitFor" && method.signature == "()I") {
						wait_for_method = method.id;
						break;
					}
				}
				assert (wait_for_method.handle != 0);

				var runtime = (JDWP.Object) yield jdwp.invoke_static_method (runtime_class.ref_type, breakpoint_event.thread,
					get_runtime_method, {}, 0, cancellable);

				var copy_commands = new string[] {
					"cp %s %s".printf (so_path_shared, so_path_app),
					"cp %s %s".printf (config_path_shared, config_path_app),
				};
				foreach (unowned string cmd in copy_commands) {
					var str = yield jdwp.create_string (cmd, cancellable);

					var process = (JDWP.Object) yield jdwp.invoke_instance_method (runtime.val, breakpoint_event.thread,
						runtime_class.ref_type.id, exec_method, new JDWP.Value[] { str, }, 0, cancellable);

					yield jdwp.invoke_instance_method (process.val, breakpoint_event.thread, process_class.ref_type.id,
						wait_for_method, {}, 0, cancellable);
				}

				yield shell.check_call ("rm -f %s; rm -f %s".printf (so_path_shared, config_path_shared), cancellable);

				var gadget_path = yield jdwp.create_string (so_path_app, cancellable);

				yield jdwp.invoke_instance_method (runtime.val, breakpoint_event.thread,
					runtime_class.ref_type.id, load_method, new JDWP.Value[] {
						gadget_path,
					}, 0, cancellable);

				return new GadgetDetails (target_pid, unix_socket_path, jdwp);
			} finally {
				shell.close.begin ();
			}
		}
	}
}
