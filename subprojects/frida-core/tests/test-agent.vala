namespace Frida.AgentTest {
	public static void add_tests () {
		GLib.Test.add_func ("/Agent/Script/load-and-receive-messages", () => {
			var h = new Harness ((h) => Script.load_and_receive_messages.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Agent/Script/performance", () => {
			var h = new Harness ((h) => Script.performance.begin (h as Harness));
			h.run ();
		});

#if DARWIN
		GLib.Test.add_func ("/Agent/Script/Darwin/launch-scenario", () => {
			var h = new Harness ((h) => Script.launch_scenario.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Agent/Script/Darwin/thread-suspend-awareness", () => {
			var h = new Harness ((h) => Script.thread_suspend_awareness.begin (h as Harness));
			h.run ();
		});
#endif
	}

	namespace Script {
		private static async void load_and_receive_messages (Harness h) {
			var session = yield h.load_agent ();

			unowned TargetFunc func = (TargetFunc) target_function;

			AgentScriptId script_id;
			try {
				Cancellable? cancellable = null;
				script_id = yield session.create_script (
					("Interceptor.attach (ptr(\"0x%" + size_t.FORMAT_MODIFIER + "x\"), {" +
					 "  onEnter(args) {" +
					 "    send({ first_argument: args[0].toInt32(), second_argument: args[1].readUtf8String() });" +
					 "  }" +
					 "});").printf ((size_t) func), make_parameters_dict (), cancellable);
				yield session.load_script (script_id, cancellable);
			} catch (GLib.Error attach_error) {
				assert_not_reached ();
			}

			func (1337, "Frida rocks");

			var message = yield h.wait_for_message ();
			assert_true (message.script_id.handle == script_id.handle);
			assert_true (message.text == "{\"type\":\"send\",\"payload\":{\"first_argument\":1337,\"second_argument\":\"Frida rocks\"}}");

			yield h.unload_agent ();

			h.done ();
		}

		private static async void performance (Harness h) {
			var session = yield h.load_agent ();

			var size = 4096;
			var buf = new uint8[size];

			AgentScriptId script_id;
			try {
				Cancellable? cancellable = null;
				script_id = yield session.create_script (
					("const buf = ptr(\"0x%" + size_t.FORMAT_MODIFIER + "x\").readByteArray(%d);" +
					 "const startTime = new Date();" +
					 "let iterations = 0;" +
					 "function sendNext() {" +
					 "  send({}, buf);" +
					 "  if (new Date().getTime() - startTime.getTime() <= 1000) {" +
					 "    setTimeout(sendNext, ((++iterations %% 10) === 0) ? 1 : 0);" +
					 "  } else {" +
					 "    send(null);" +
					 "  }" +
					 "};" +
					 "sendNext();"
					).printf ((size_t) buf, size), make_parameters_dict (), cancellable);
				yield session.load_script (script_id, cancellable);
			} catch (GLib.Error attach_error) {
				assert_not_reached ();
			}

			var first_message = yield h.wait_for_message ();
			assert_true (first_message.text == "{\"type\":\"send\",\"payload\":{}}");

			var timer = new Timer ();
			int count = 0;
			while (true) {
				var message = yield h.wait_for_message ();
				count++;
				if (message.text != "{\"type\":\"send\",\"payload\":{}}") {
					assert_true (message.text == "{\"type\":\"send\",\"payload\":null}");
					break;
				}
			}

			stdout.printf ("<got %d bytes or %d messages in %f seconds> ", count * size, count, timer.elapsed ());

			yield h.unload_agent ();

			h.done ();
		}

#if DARWIN
		private static async void launch_scenario (Harness h) {
			if (!GLib.Test.slow ()) {
				stdout.printf ("<skipping, run in slow mode> ");
				h.done ();
				return;
			}

			var session = yield h.load_agent ();

			AgentScriptId script_id;
			try {
				Cancellable? cancellable = null;
				script_id = yield session.create_script ("""
const POSIX_SPAWN_START_SUSPENDED = 0x0080;

const { pointerSize } = Process;

const upcoming = new Set();
let gating = false;
let active = 0;

rpc.exports = {
  prepareForLaunch(identifier) {
    upcoming.add(identifier);
    active++;
  },
  cancelLaunch(identifier) {
    if (upcoming.delete(identifier))
      active--;
  },
  enableSpawnGating() {
    if (gating)
      throw new Error('Spawn gating already enabled');
    gating = true;
    active++;
  },
  disableSpawnGating() {
    if (!gating)
      throw new Error('Spawn gating already disabled');
    gating = false;
    active--;
  },
};

Interceptor.attach(Process.getModuleByName('/usr/lib/system/libsystem_kernel.dylib').getExportByName('__posix_spawn'), {
  onEnter(args) {
    if (active === 0)
      return;

    const path = args[1].readUtf8String();
    if (path !== '/bin/ls')
      return;

    const rawIdentifier = args[3].add(pointerSize).readPointer().readUtf8String();

    let identifier, event;
    if (rawIdentifier.startsWith('UIKitApplication:')) {
      identifier = rawIdentifier.substring(17, rawIdentifier.indexOf('['));
      if (upcoming.has(identifier))
        event = 'launch:app';
      else if (gating)
        event = 'spawn';
      else
        return;
    } else if (gating) {
      identifier = rawIdentifier;
      event = 'spawn';
    } else {
      return;
    }

    const attrs = args[2].add(pointerSize).readPointer();

    let flags = attrs.readU16();
    flags |= POSIX_SPAWN_START_SUSPENDED;
    attrs.writeU16(flags);

    this.event = event;
    this.identifier = identifier;
    this.pidPtr = args[0];
  },
  onLeave(retval) {
    if (active === 0)
      return;

    const { event, identifier, pidPtr } = this;
    if (event === undefined)
      return;

    if (event === 'launch:app') {
      upcoming.delete(identifier);
      active--;
    }

    if (retval.toInt32() < 0)
      return;

    send([event, identifier, pidPtr.readU32()]);
  }
});
""", make_parameters_dict (), cancellable);
				yield session.load_script (script_id, cancellable);

				h.disable_timeout ();

				print ("\n");

				for (uint i = 0; i != 1000000; i++) {
					int64 next_id = 1;

					var id = next_id++;
					print ("\nLaunch #%u\n", i);

					var request = new Json.Builder ()
						.begin_array ()
						.add_string_value ("frida:rpc")
						.add_int_value (id)
						.add_string_value ("call")
						.add_string_value ("prepareForLaunch")
						.begin_array ()
						.add_string_value ("foo.bar.Baz")
						.end_array ()
						.end_array ();
					var raw_request = Json.to_string (request.get_root (), false);
					yield session.post_messages ({ AgentMessage (SCRIPT, script_id, raw_request, false, {}) }, 0,
						cancellable);

					while (true) {
						var message = yield h.wait_for_message ();

						var reader = make_json_reader (message.text);

						reader.read_member ("type");
						if (reader.get_string_value () != "send") {
							printerr ("%s\n", message.text);
							continue;
						}
						reader.end_member ();

						reader.read_member ("payload");
						if (!reader.is_array ()) {
							printerr ("%s\n", Json.to_string (reader.get_value (), true));
							continue;
						}

						reader.read_element (0);
						assert_true (reader.get_string_value () == "frida:rpc");
						reader.end_element ();

						reader.read_element (1);
						assert_true (reader.get_int_value () == id);
						reader.end_element ();

						reader.read_element (2);
						assert_true (reader.get_string_value () == "ok");
						reader.end_element ();

						reader.read_element (3);
						assert_true (reader.get_null_value ());
						reader.end_element ();

						reader.end_member ();

						break;
					}

					var child = Frida.Test.Process.start ("/bin/ls", new string[] {
						"UIKitApplication:foo.bar.Baz[0x1234]"
					});

					while (true) {
						var message = yield h.wait_for_message ();
						printerr ("got message: %s\n", message.text);

						var reader = make_json_reader (message.text);

						reader.read_member ("type");
						if (reader.get_string_value () != "send") {
							printerr ("%s\n", message.text);
							continue;
						}
						reader.end_member ();

						reader.read_member ("payload");
						if (!reader.is_array ()) {
							printerr ("%s\n", Json.to_string (reader.get_value (), true));
							continue;
						}

						reader.read_element (0);
						assert_true (reader.get_string_value () == "launch:app");
						reader.end_element ();

						reader.read_element (1);
						assert_true (reader.get_string_value () == "foo.bar.Baz");
						reader.end_element ();

						reader.read_element (2);
						assert_true (reader.get_int_value () == child.id);
						reader.end_element ();

						reader.end_member ();

						break;
					}

					child.resume ();
					child.join (5000);

					Timeout.add_seconds (20, launch_scenario.callback);
					print ("waiting 20s\n");
					yield;
					print ("waited 20s\n");
				}
			} catch (GLib.Error e) {
				printerr ("\n\nERROR: %s\n", e.message);
				assert_not_reached ();
			}

			yield h.unload_agent ();

			h.done ();
		}

		private static async void thread_suspend_awareness (Harness h) {
			if (!GLib.Test.slow ()) {
				stdout.printf ("<skipping, run in slow mode> ");
				h.done ();
				return;
			}

			var session = yield h.load_agent ();

			try {
				Cancellable? cancellable = null;

				var script_id = yield session.create_script ("""
console.log('Script runtime is: ' + Script.runtime);

Interceptor.attach(Process.getModuleByName('libsystem_kernel.dylib').getExportByName('open'), () => {
});
""", make_parameters_dict (), cancellable);
				yield session.load_script (script_id, cancellable);

				var thread_id = get_current_thread_id ();

				var worker_thread = new Thread<bool> ("thread-suspend-worker", () => {
					for (int i = 0; i != 1000; i++) {
						thread_suspend (thread_id);
						call_hooked_function ();
						thread_resume (thread_id);

						sleep_for_a_random_duration ();
					}

					return true;
				});

				for (int i = 0; i != 1000; i++) {
					call_hooked_function ();

					sleep_for_a_random_duration ();
				}

				worker_thread.join ();
			} catch (GLib.Error e) {
				printerr ("\n\nERROR: %s\n", e.message);
				assert_not_reached ();
			}

			yield h.unload_agent ();

			h.done ();
		}

		private static void call_hooked_function () {
			var fd = Posix.open ("/etc/hosts", Posix.O_RDONLY);
			assert_true (fd != -1);
			Posix.close (fd);
		}

		private static void sleep_for_a_random_duration () {
			Thread.usleep (Random.int_range (0, 300));
		}

		public extern static uint get_current_thread_id ();
		public extern static void thread_suspend (uint thread_id);
		public extern static void thread_resume (uint thread_id);
#endif

		[CCode (has_target = false)]
		private delegate void TargetFunc (int level, string message);

		public extern static uint target_function (int level, string message);
	}

	private sealed class Harness : Frida.Test.AsyncHarness, AgentController, AgentMessageSink {
		private GLib.Module module;
		[CCode (has_target = false)]
		private delegate void AgentMainFunc (string data, ref Frida.UnloadPolicy unload_policy, void * opaque_injector_state);
		private AgentMainFunc main_impl;
#if LINUX
		private FileDescriptor agent_ctrlfd_for_peer;
#else
		private PipeTransport transport;
#endif
		private string? transport_address;
		private Thread<bool> main_thread;
		private DBusConnection connection;
		private Gee.Collection<uint> registrations = new Gee.ArrayList<uint> ();
		private AgentSessionProvider provider;
		private AgentSession session;

		private Gee.Queue<AgentMessage?> message_queue = new Gee.LinkedList<AgentMessage?> ();

		public Harness (owned Frida.Test.AsyncHarness.TestSequenceFunc func) {
			base ((owned) func);
		}

		public async AgentSession load_agent () {
			Cancellable? cancellable = null;

			string agent_filename;
			string shlib_extension;
#if WINDOWS
			shlib_extension = "dll";
#elif DARWIN
			shlib_extension = "dylib";
#else
			shlib_extension = "so";
#endif
#if IOS || TVOS || ANDROID || QNX
			var deployment_dir = Path.get_dirname (Frida.Test.Process.current.filename);
			agent_filename = Path.build_filename (deployment_dir, "frida-agent." + shlib_extension);
#else
			var frida_root_dir = Path.get_dirname (Path.get_dirname (Frida.Test.Process.current.filename));
			agent_filename = Path.build_filename (frida_root_dir, "lib", "frida", "frida-agent." + shlib_extension);
			if (!FileUtils.test (agent_filename, FileTest.EXISTS))
				agent_filename = Path.build_filename (frida_root_dir, "lib", "agent", "frida-agent." + shlib_extension);
#endif

			try {
				module = new Module (agent_filename, LOCAL | LAZY);
			} catch (ModuleError e) {
				assert_not_reached ();
			}

			void * main_func_symbol;
			var main_func_found = module.symbol ("frida_agent_main", out main_func_symbol);
			assert_true (main_func_found);
			main_impl = (AgentMainFunc) main_func_symbol;

			Future<IOStream> stream_request;
#if LINUX
			int agent_ctrlfds[2];
			if (Posix.socketpair (Posix.AF_UNIX, Posix.SOCK_STREAM, 0, agent_ctrlfds) != 0) {
				printerr ("Unable to allocate socketpair\n");
				assert_not_reached ();
			}
			var agent_ctrlfd = new FileDescriptor (agent_ctrlfds[0]);
			agent_ctrlfd_for_peer = new FileDescriptor (agent_ctrlfds[1]);
			transport_address = "";

			try {
				Socket socket = new Socket.from_fd (agent_ctrlfd.handle);
				agent_ctrlfd.steal ();
				var promise = new Promise<IOStream> ();
				promise.resolve (SocketConnection.factory_create_connection (socket));
				stream_request = promise.future;
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
#else
			try {
				transport = new PipeTransport ();
				transport_address = transport.remote_address;
				stream_request = Pipe.open (transport.local_address, cancellable);
			} catch (Error e) {
				printerr ("Unable to create transport: %s\n", e.message);
				assert_not_reached ();
			}
#endif

			main_thread = new Thread<bool> ("frida-test-agent-worker", agent_main_worker);

			try {
				var stream = yield stream_request.wait_async (cancellable);
				connection = yield new DBusConnection (stream, ServerGuid.HOST_SESSION_SERVICE,
					AUTHENTICATION_SERVER | AUTHENTICATION_ALLOW_ANONYMOUS | DELAY_MESSAGE_PROCESSING,
					null, cancellable);

				var session_id = AgentSessionId.generate ();

				registrations.add_all_array ({
					connection.register_object (ObjectPath.AGENT_CONTROLLER, (AgentController) this),
					connection.register_object (ObjectPath.for_agent_message_sink (session_id), (AgentMessageSink) this)
				});

				connection.start_message_processing ();

				provider = yield connection.get_proxy (null, ObjectPath.AGENT_SESSION_PROVIDER, DO_NOT_LOAD_PROPERTIES,
					cancellable);

				yield provider.open (session_id, make_parameters_dict (), cancellable);

				session = yield connection.get_proxy (null, ObjectPath.for_agent_session (session_id),
					DO_NOT_LOAD_PROPERTIES, cancellable);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}

			return session;
		}

		public async void unload_agent () {
			try {
				yield session.close (null);
			} catch (GLib.Error session_error) {
				assert_not_reached ();
			}
			session = null;
			provider = null;

			try {
				yield connection.close ();
			} catch (GLib.Error connection_error) {
			}
			foreach (var id in registrations)
				connection.unregister_object (id);
			registrations.clear ();
			connection = null;

			Thread<bool> t = main_thread;
			t.join ();
			main_thread = null;

			module = null;
		}

		public async AgentMessage? wait_for_message () {
			AgentMessage? message = null;

			do {
				message = message_queue.poll ();
				if (message == null)
					yield process_events ();
			} while (message == null);

			return message;
		}

		private bool agent_main_worker () {
			UnloadPolicy unload_policy = IMMEDIATE;
			void * injector_state = null;

#if LINUX
			var s = LinuxInjectorState ();
			s.frida_ctrlfd = -1;
			s.agent_ctrlfd = agent_ctrlfd_for_peer.steal ();
			injector_state = &s;
#endif

			string agent_parameters = transport_address;

			main_impl (agent_parameters, ref unload_policy, injector_state);

			return true;
		}

#if !WINDOWS
		private async HostChildId prepare_to_fork (uint parent_pid, Cancellable? cancellable, out uint parent_injectee_id,
				out uint child_injectee_id, out GLib.Socket child_socket) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}
#endif

		public async HostChildId prepare_to_specialize (uint pid, string identifier, Cancellable? cancellable,
				out uint specialized_injectee_id, out string specialized_pipe_address) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}

		private async void recreate_agent_thread (uint pid, uint injectee_id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}

		private async void wait_for_permission_to_resume (HostChildId id, HostChildInfo info, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}

		private async void prepare_to_exec (HostChildInfo info, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}

		private async void cancel_exec (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}

		private async void acknowledge_spawn (HostChildInfo info, SpawnStartState start_state, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not implemented");
		}

		protected async void post_messages (AgentMessage[] messages, uint batch_id,
				Cancellable? cancellable) throws Error, IOError {
			foreach (var m in messages)
				message_queue.offer (m);
		}
	}
}
