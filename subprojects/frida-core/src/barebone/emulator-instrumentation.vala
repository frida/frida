namespace Frida {
	private sealed class EmulatorInstrumentation : Object {
		private DeviceManager manager;
		private Script script;

		private EmulatorInstrumentation (DeviceManager manager, Script script) {
			this.manager = manager;
			this.script = script;
		}

		public static async EmulatorInstrumentation apply (BareboneConfig config, Cancellable? cancellable)
				throws Error, IOError {
			var manager = new DeviceManager ();
			bool adopted = false;
			try {
				var device = yield manager.get_device_by_type (DeviceType.LOCAL, 0, cancellable);
				var session = yield device.attach (config.connection.pid, null, cancellable);

				unowned string source = (string) Frida.Data.Barebone.get_emulator_gdbstub_shim_js_blob ().data;
				var script = yield session.create_script (source, null, cancellable);

				var armed = new Promise<bool> ();
				script.message.connect ((json, data) => {
					if (!armed.future.ready)
						handle_arming_message (json, armed);
				});

				yield script.load (cancellable);
				yield armed.future.wait_async (cancellable);

				string? pipe_path = pipe_socket_path (config);
				if (pipe_path != null) {
					var builder = new Json.Builder ();
					builder.begin_object ();
					builder.set_member_name ("type");
					builder.add_string_value ("allow-pipe-path");
					builder.set_member_name ("path");
					builder.add_string_value (pipe_path);
					builder.end_object ();
					script.post (Json.to_string (builder.get_root (), false));
				}

				var instrumentation = new EmulatorInstrumentation (manager, script);
				adopted = true;
				return instrumentation;
			} finally {
				if (!adopted) {
					try {
						yield manager.close (cancellable);
					} catch (IOError e) {
					}
				}
			}
		}

		public async void tear_down (Cancellable? cancellable) throws IOError {
			try {
				yield script.unload (cancellable);
			} catch (GLib.Error e) {
			}
			yield manager.close (cancellable);
		}

		private static void handle_arming_message (string json, Promise<bool> armed) {
			try {
				var parser = new Json.Parser ();
				parser.load_from_data (json, -1);
				var root = parser.get_root ().get_object ();

				string kind = root.get_string_member_with_default ("type", "");
				if (kind == "error") {
					armed.reject (new Error.NOT_SUPPORTED ("Emulator instrumentation failed: %s",
						root.get_string_member_with_default ("description", "script error")));
					return;
				}
				if (kind != "send")
					return;

				Json.Object? payload = root.get_object_member ("payload");
				if (payload == null)
					return;

				string payload_type = payload.get_string_member_with_default ("type", "");
				if (payload_type == "armed")
					armed.resolve (true);
				else if (payload_type == "shim-error")
					armed.reject (new Error.NOT_SUPPORTED ("Emulator instrumentation refused to patch: %s",
						payload.get_string_member_with_default ("message", "offset mismatch")));
			} catch (GLib.Error e) {
			}
		}

		private static string? pipe_socket_path (BareboneConfig config) {
			var injected = config.agent as BareboneInjectedAgentConfig;
			if (injected == null)
				return null;

			var pipe_vsock = injected.transport as BareboneVsockPipeTransportConfig;
			if (pipe_vsock != null)
				return pipe_vsock.socket_path;

			var vsock = injected.transport as BareboneVsockTransportConfig;
			if (vsock != null)
				return vsock.socket_path;

			return null;
		}
	}
}
