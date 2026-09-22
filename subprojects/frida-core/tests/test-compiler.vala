namespace Frida.CompilerTest {
	public static void add_tests () {
		GLib.Test.add_func ("/Compiler/Performance/build-simple-agent", () => {
			var h = new Harness ((h) => Performance.build_simple_agent.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Compiler/Performance/watch-simple-agent", () => {
			var h = new Harness ((h) => Performance.watch_simple_agent.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Compiler/LanguageServer/complete-simple-agent", () => {
			var h = new Harness ((h) => LanguageServerTests.complete_simple_agent.begin (h as Harness));
			h.run ();
		});
	}

	namespace LanguageServerTests {
		private static async void complete_simple_agent (Harness h) {
			if (skip_slow_test ()) {
				stdout.printf ("<skipping, run in slow mode> ");
				h.done ();
				return;
			}

			try {
				string project_dir = DirUtils.make_tmp ("compiler-test.XXXXXX");
				string agent_ts_path = Path.build_filename (project_dir, "agent.ts");
				string agent_ts_source = "const m = Process.mainModule;\nm.\n";
				FileUtils.set_contents (agent_ts_path, agent_ts_source);

				var server = new LanguageServer (project_dir);

				var client = new Client (server);
				yield server.start ();

				yield client.request ("initialize", """{
					"processId": null,
					"rootUri": "%s",
					"capabilities": {}
				}""".printf (file_uri (project_dir)));
				client.send_notification ("initialized", "{}");

				client.send_notification ("textDocument/didOpen", """{
					"textDocument": {
						"uri": "%s",
						"languageId": "typescript",
						"version": 1,
						"text": "%s"
					}
				}""".printf (file_uri (agent_ts_path), agent_ts_source.escape ()));

				var completion = yield client.request ("textDocument/completion", """{
					"textDocument": { "uri": "%s" },
					"position": { "line": 1, "character": 2 }
				}""".printf (file_uri (agent_ts_path)));
				assert ("\"enumerateExports\"" in completion);

				yield client.request ("shutdown", null);
				client.send_notification ("exit", null);
				server.stop ();

				FileUtils.unlink (agent_ts_path);
				DirUtils.remove (project_dir);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n\n", e.message);
				assert_not_reached ();
			}

			h.done ();
		}

		private static string file_uri (string path) throws ConvertError {
			return Filename.to_uri (path);
		}

		private class Client : Object {
			private LanguageServer server;
			private int next_id = 1;
			private Gee.Map<int, PendingRequest> pending = new Gee.HashMap<int, PendingRequest> ();

			public Client (LanguageServer server) {
				this.server = server;
				server.message.connect (on_message);
			}

			public async string request (string method, string? params) throws Error {
				int id = next_id++;
				var pending_request = new PendingRequest (request.callback);
				pending[id] = pending_request;

				server.post ("""{"jsonrpc": "2.0", "id": %d, "method": "%s"%s}""".printf (id, method, params_member (params)));
				yield;

				return pending_request.result;
			}

			public void send_notification (string method, string? params) throws Error {
				server.post ("""{"jsonrpc": "2.0", "method": "%s"%s}""".printf (method, params_member (params)));
			}

			private static string params_member (string? params) {
				return (params != null) ? ", \"params\": " + params : "";
			}

			private void on_message (string json) {
				if (GLib.Test.verbose ())
					print ("<<< %s\n", json);

				Json.Reader reader;
				try {
					reader = make_json_reader (json);
				} catch (GLib.Error e) {
					assert_not_reached ();
				}

				bool is_response = !reader.read_member ("method");
				reader.end_member ();

				if (is_response)
					handle_response (reader, json);
				else
					handle_call (reader);
			}

			private void handle_response (Json.Reader reader, string json) {
				reader.read_member ("id");
				int id = (int) reader.get_int_value ();
				reader.end_member ();

				PendingRequest pending_request;
				pending.unset (id, out pending_request);

				pending_request.result = json;
				pending_request.callback ();
			}

			private void handle_call (Json.Reader reader) {
				bool is_request = reader.read_member ("id");
				if (is_request)
					reply_to_server_request (reader.get_string_value ());
				reader.end_member ();
			}

			private void reply_to_server_request (string id) {
				try {
					server.post ("""{"jsonrpc": "2.0", "id": "%s", "result": null}""".printf (id));
				} catch (Error e) {
					assert_not_reached ();
				}
			}

			private class PendingRequest {
				public SourceFunc callback;
				public string? result;

				public PendingRequest (owned SourceFunc callback) {
					this.callback = (owned) callback;
				}
			}
		}
	}

	namespace Performance {
		private static async void build_simple_agent (Harness h) {
			if (skip_slow_test ()) {
				stdout.printf ("<skipping, run in slow mode> ");
				h.done ();
				return;
			}

			try {
				var device_manager = new DeviceManager ();
				var compiler = new Compiler (device_manager);

				string project_dir = DirUtils.make_tmp ("compiler-test.XXXXXX");
				string agent_ts_path = Path.build_filename (project_dir, "agent.ts");
				FileUtils.set_contents (agent_ts_path, """
import { log } from "./logger.js";

const woot = Buffer.from("w00t").toString("base64");

log("Hello World: " + woot);
log(hexdump(Process.mainModule.base, { ansi: true }));
""");

				string logger_ts_path = Path.build_filename (project_dir, "logger.ts");
				FileUtils.set_contents (logger_ts_path, """
export function log(...items: any[]) {
    const message = items.join("\n");
    console.log(`[LOG] ${message}`);
}
""");

				compiler.diagnostics.connect (d => printerr ("DIAGNOSTICS: %s\n", d.print (false)));
				var timer = new Timer ();
				var code = yield compiler.build (agent_ts_path);
				uint elapsed_msec = (uint) (timer.elapsed () * 1000.0);

				if (GLib.Test.verbose ()) {
					print ("Output:\nvvv\n%s^^^\n", code);
					print ("Built in %u ms\n", elapsed_msec);
				}

				unowned string? test_log_path = Environment.get_variable ("FRIDA_TEST_LOG");
				if (test_log_path != null) {
					var test_log = FileStream.open (test_log_path, "w");
					assert (test_log != null);

					test_log.printf ("build-time,%u\n", elapsed_msec);

					Gum.Process.enumerate_modules (m => {
						if ("frida-agent" in m.path) {
							var r = m.range;
							test_log.printf (("agent-range,0x%" + uint64.FORMAT_MODIFIER + "x,0x%" +
									uint64.FORMAT_MODIFIER + "x\n"),
								r.base_address, r.base_address + r.size);
							return false;
						}

						return true;
					});

					test_log = null;
				}

				FileUtils.unlink (agent_ts_path);
				DirUtils.remove (project_dir);

				compiler = null;
				yield device_manager.close ();
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n\n", e.message);
				assert_not_reached ();
			}

			h.done ();
		}

		private static async void watch_simple_agent (Harness h) {
			if (skip_slow_test ()) {
				stdout.printf ("<skipping, run in slow mode> ");
				h.done ();
				return;
			}

			try {
				var device_manager = new DeviceManager ();
				var compiler = new Compiler (device_manager);

				string project_dir = DirUtils.make_tmp ("compiler-test.XXXXXX");
				string agent_ts_path = Path.build_filename (project_dir, "agent.ts");
				FileUtils.set_contents (agent_ts_path, "console.log(\"Hello World\");");

				string? bundle = null;
				bool waiting = false;
				compiler.output.connect (b => {
					bundle = b;
					if (waiting)
						watch_simple_agent.callback ();
				});

				var timer = new Timer ();
				yield compiler.watch (agent_ts_path);
				while (bundle == null) {
					waiting = true;
					yield;
					waiting = false;
				}
				uint elapsed_msec = (uint) (timer.elapsed () * 1000.0);

				if (GLib.Test.verbose ())
					print ("Watch built first bundle in %u ms\n", elapsed_msec);

				unowned string? test_log_path = Environment.get_variable ("FRIDA_TEST_LOG");
				if (test_log_path != null) {
					var test_log = FileStream.open (test_log_path, "w");
					assert (test_log != null);

					test_log.printf ("build-time,%u\n", elapsed_msec);

					Gum.Process.enumerate_modules (m => {
						if ("frida-agent" in m.path) {
							var r = m.range;
							test_log.printf (("agent-range,0x%" + uint64.FORMAT_MODIFIER + "x,0x%" +
									uint64.FORMAT_MODIFIER + "x\n"),
								r.base_address, r.base_address + r.size);
							return false;
						}

						return true;
					});

					test_log = null;
				}

				FileUtils.unlink (agent_ts_path);
				DirUtils.remove (project_dir);

				compiler = null;
				yield device_manager.close ();
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			h.done ();
		}

	}

	private static bool skip_slow_test () {
		if (GLib.Test.slow ())
			return false;

		if (Frida.Test.os () == Frida.Test.OS.IOS)
			return true;

		switch (Frida.Test.cpu ()) {
			case ARM_32:
			case ARM_64: {
				bool likely_running_in_an_emulator = ByteOrder.HOST == ByteOrder.BIG_ENDIAN;
				if (likely_running_in_an_emulator)
					return true;
				break;
			}
			default:
				break;
		}

		return false;
	}

	private sealed class Harness : Frida.Test.AsyncHarness {
		public Harness (owned Frida.Test.AsyncHarness.TestSequenceFunc func) {
			base ((owned) func);
		}
	}
}
