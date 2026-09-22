[CCode (gir_namespace = "FridaGDB", gir_version = "1.0")]
namespace Frida.GDB {
	public class Client : Object, AsyncInitable {
		public signal void closed ();
		public signal void console_output (Bytes bytes);

		public IOStream stream {
			get;
			construct;
		}

		public TargetArch arch {
			get;
			set;
			default = UNKNOWN;
		}

		public uint pointer_size {
			get;
			set;
			default = (uint) sizeof (void *);
		}

		public ByteOrder byte_order {
			get;
			set;
			default = HOST;
		}

		public State state {
			get {
				return _state;
			}
		}

		public Exception? exception {
			get {
				return _exception;
			}
		}

		public Gee.Set<string> features {
			get {
				return supported_features;
			}
		}

		private DataInputStream input;
		private OutputStream output;
		private Cancellable io_cancellable = new Cancellable ();

		private State _state = STOPPED;
		private Exception? _exception;
		private Exception? breakpoint_exception;
		private Gee.List<StopObserverEntry> on_stop = new Gee.ArrayList<StopObserverEntry> ();
		private size_t max_packet_size = 1024;
		private AckMode ack_mode = SEND_ACKS;
		internal bool bulk_registers = true;

		protected void set_max_packet_size (size_t size) {
			max_packet_size = size;
		}
		private Gee.Queue<Bytes> pending_writes = new Gee.ArrayQueue<Bytes> ();
		private Promise<uint>? write_request;
		private Gee.Queue<PendingResponse> pending_responses = new Gee.ArrayQueue<PendingResponse> ();

		protected Gee.Set<string> supported_features = new Gee.HashSet<string> ();
		protected Gee.List<Register>? registers;
		protected Gee.Map<string, Register>? register_by_name;
		protected Gee.Map<uint64?, Breakpoint> breakpoints = new Gee.HashMap<uint64?, Breakpoint> (
			(n) => { return int64_hash ((int64?) n); },
			(a, b) => { return int64_equal ((int64?) a, (int64?) b); }
		);

		public enum State {
			STOPPED,
			RUNNING,
			STOPPING,
			CLOSED;

			public string to_nick () {
				return Marshal.enum_to_nick<State> (this);
			}
		}

		private enum MessageHandling {
			SEND_ACKS,
			SKIP_ACKS
		}

		private enum AckMode {
			SEND_ACKS,
			SKIP_ACKS
		}

		protected const char NOTIFICATION_TYPE_EXIT_STATUS = 'W';
		protected const char NOTIFICATION_TYPE_EXIT_SIGNAL = 'X';
		protected const char NOTIFICATION_TYPE_STOP = 'S';
		protected const char NOTIFICATION_TYPE_STOP_WITH_PROPERTIES = 'T';
		protected const char NOTIFICATION_TYPE_OUTPUT = 'O';

		private bool binary_writes_supported = true;

		private const char STOP_CHARACTER = 0x03;
		private const string ACK_NOTIFICATION = "+";
		private const string NACK_NOTIFICATION = "-";
		private const string PACKET_MARKER = "$";
		private const char PACKET_CHARACTER = '$';
		private const string CHECKSUM_MARKER = "#";
		private const char CHECKSUM_CHARACTER = '#';
		private const char ESCAPE_CHARACTER = '}';
		private const uint8 ESCAPE_KEY = 0x20;
		private const char REPEAT_CHARACTER = '*';
		private const uint8 REPEAT_BASE = 0x20;
		private const uint8 REPEAT_BIAS = 3;

		private enum UnixSignal {
			SIGTRAP = 5,
		}

		private Client (IOStream stream) {
			Object (stream: stream);
		}

		public static async Client open (IOStream stream, Cancellable? cancellable = null)
				throws Error, IOError {
			var client = new Client (stream);

			try {
				yield client.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return client;
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			try {
				input = new DataInputStream (stream.get_input_stream ());
				output = stream.get_output_stream ();

				process_incoming_packets.begin ();
				write_string (ACK_NOTIFICATION);

				yield prepare_connection (cancellable);

				string supported_response = yield query_property ("Supported", cancellable);
				supported_features.add_all_array (supported_response.split (";"));

				foreach (string feature in supported_features) {
					if (feature.has_prefix ("PacketSize=")) {
						max_packet_size = (size_t) uint64.parse (feature[11:], 16);
						break;
					}
				}

				if ("QStartNoAckMode+" in supported_features || "qEcho+" in supported_features) {
					yield execute_simple ("QStartNoAckMode", cancellable);
					ack_mode = SKIP_ACKS;
				}

				if ("qXfer:features:read+" in supported_features)
					yield load_target_properties (cancellable);

				yield detect_vendor_features (cancellable);

				yield enable_extensions (cancellable);

				string attached_response = yield query_property ("Attached", cancellable);
				if (attached_response == "1") {
					if (_exception == null) {
						request_stop_info ();
						yield wait_until_stopped (cancellable);
					}
				}
			} catch (GLib.Error e) {
				io_cancellable.cancel ();

				throw new Error.PROTOCOL ("%s", e.message);
			}

			return true;
		}

		protected virtual async void prepare_connection (Cancellable? cancellable) throws Error, IOError {
		}

		protected async void halt (Cancellable? cancellable, uint timeout_msec = 0) throws Error, IOError {
			change_state (RUNNING);
			write_bytes (new Bytes ({ STOP_CHARACTER }));
			request_stop_info ();
			yield wait_until_stopped (cancellable, timeout_msec);
		}

		protected async void start_no_ack_mode (Cancellable? cancellable) throws Error, IOError {
			yield execute_simple ("QStartNoAckMode", cancellable);
			ack_mode = SKIP_ACKS;
		}

		protected void install_registers (Gee.List<Register> regs) {
			registers = regs;

			register_by_name = new Gee.HashMap<string, Register> ();
			foreach (var reg in regs) {
				register_by_name[reg.name] = reg;
				string? altname = reg.altname;
				if (altname != null)
					register_by_name[altname] = reg;
			}
		}

		protected virtual async void detect_vendor_features (Cancellable? cancellable) throws Error, IOError {
			try {
				string info = yield run_remote_command ("info", cancellable);
				if ("Corellium" in info)
					supported_features.add ("corellium");
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
			}

			try {
				var response = yield query_simple ("vCont?", cancellable);
				unowned string payload = response.payload;
				if (payload.length > 0 && payload[0] != 'E')
					supported_features.add ("vcont");
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
			}

			try {
				string response = yield query_property ("qemu.PhyMemMode", cancellable);
				if (response.length == 1)
					supported_features.add ("qemu-phy-mem-mode");
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
			}
		}

		protected virtual async void enable_extensions (Cancellable? cancellable) throws Error, IOError {
		}

		private void change_state (State new_state, Exception? new_exception = null) {
			bool state_differs = new_state != _state;
			if (state_differs)
				_state = new_state;

			bool exception_differs = new_exception != _exception;
			if (exception_differs)
				_exception = new_exception;

			if (state_differs)
				notify_property ("state");

			if (exception_differs)
				notify_property ("exception");
		}

		private void clear_current_exception () {
			if (_exception == null)
				return;

			_exception = null;
			notify_property ("exception");
		}

		public async void close (Cancellable? cancellable = null) throws IOError {
			if (state == CLOSED)
				return;

			io_cancellable.cancel ();

			var source = new IdleSource ();
			source.set_callback (close.callback);
			source.attach (MainContext.get_thread_default ());
			yield;

			try {
				yield stream.close_async (Priority.DEFAULT, cancellable);
			} catch (IOError e) {
			}
		}

		public async void continue (Cancellable? cancellable = null) throws Error, IOError {
			check_stopped ();

			var exception = breakpoint_exception;
			if (exception != null) {
				breakpoint_exception = null;

				var breakpoint = exception.breakpoint;
				yield breakpoint.disable (cancellable);
				yield exception.thread.step (cancellable);
				yield breakpoint.enable (cancellable);

				check_stopped ();
			}

			change_state (RUNNING);

			var command = make_packet_builder_sized (1)
				.append_c ('c')
				.build ();
			write_bytes (command);
		}

		public async void continue_specific_threads (Gee.Iterable<Thread> threads, Cancellable? cancellable = null)
				throws Error, IOError {
			check_stopped ();

			change_state (RUNNING);

			var command = make_packet_builder_sized (1)
				.append ("vCont");
			foreach (var thread in threads) {
				command
					.append (";c:")
					.append (thread.id);
			}
			write_bytes (command.build ());
		}

		public async Exception continue_until_exception (Cancellable? cancellable = null) throws Error, IOError {
			check_stopped ();

			clear_current_exception ();

			if (breakpoint_exception != null)
				yield continue (cancellable);

			if (_exception != null)
				return _exception;

			bool waiting = false;

			var stop_observer = new StopObserverEntry (() => {
				if (waiting)
					continue_until_exception.callback ();
				return false;
			});
			on_stop.add (stop_observer);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				if (waiting)
					continue_until_exception.callback ();
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			try {
				if (state == STOPPED)
					yield continue (cancellable);

				if (state != STOPPED) {
					waiting = true;
					yield;
					waiting = false;
				}
			} finally {
				cancel_source.destroy ();

				on_stop.remove (stop_observer);
			}

			if (_exception == null)
				throw new Error.TRANSPORT ("Connection closed while waiting for exception");

			return _exception;
		}

		public async void stop (Cancellable? cancellable = null) throws Error, IOError {
			if (state == STOPPED)
				return;

			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Unable to stop; connection is closed");

			if (state == RUNNING) {
				change_state (STOPPING);

				write_bytes (new Bytes ({ STOP_CHARACTER }));
			}

			yield wait_until_stopped (cancellable);
		}

		private async void wait_until_stopped (Cancellable? cancellable, uint timeout_msec = 0) throws Error, IOError {
			var stop_observer = new StopObserverEntry (() => {
				wait_until_stopped.callback ();
				return false;
			});
			on_stop.add (stop_observer);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				wait_until_stopped.callback ();
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			bool timed_out = false;
			TimeoutSource? timeout_source = null;
			if (timeout_msec != 0) {
				timeout_source = new TimeoutSource (timeout_msec);
				timeout_source.set_callback (() => {
					timed_out = true;
					wait_until_stopped.callback ();
					return false;
				});
				timeout_source.attach (MainContext.get_thread_default ());
			}

			yield;

			if (timeout_source != null)
				timeout_source.destroy ();
			cancel_source.destroy ();

			on_stop.remove (stop_observer);

			if (timed_out)
				throw new Error.TIMED_OUT ("Timed out while waiting for target to stop");
			if (state == CLOSED)
				throw new Error.TRANSPORT ("Connection closed while waiting for target to stop");
		}

		public async void detach (Cancellable? cancellable = null) throws Error, IOError {
			yield stop (cancellable);

			yield execute_simple ("D", cancellable);
		}

		public void restart () throws Error {
			check_stopped ();

			var command = make_packet_builder_sized (5)
				.append ("R")
				.build ();
			write_bytes (command);
		}

		public async void kill (Cancellable? cancellable = null) throws Error, IOError {
			yield stop (cancellable);

			var kill_response = yield query_simple ("k", cancellable);
			if (kill_response.payload != "X09")
				throw new Error.INVALID_OPERATION ("Unable to kill existing process");

			change_state (STOPPING);
		}

		public async void _step_thread (Thread thread, Cancellable? cancellable) throws Error, IOError {
			check_stopped ();

			change_state (RUNNING);
			if ("vcont" in features) {
				var command = make_packet_builder_sized (16)
					.append ("vCont;s:")
					.append (thread.id)
					.build ();
				write_bytes (command);
			} else {
				// Expensive roundtrip
				yield execute_simple ("Hg" + thread.id, cancellable);
				var command = make_packet_builder_sized (16)
					.append ("s")
					.build ();
				write_bytes (command);
			}

			yield wait_until_stopped (cancellable);
		}

		public void _step_thread_and_continue (Thread thread) throws Error {
			check_stopped ();

			change_state (RUNNING);

			if ("vcont" in features) {
				var command = make_packet_builder_sized (16)
					.append ("vCont;s:")
					.append (thread.id)
					.append (";c")
					.build ();
				write_bytes (command);
			} else {
				var command = make_packet_builder_sized (16)
					.append ("s")
					.build ();
				write_bytes (command);
			}
		}

		public virtual async Bytes read_byte_array (uint64 address, size_t size, Cancellable? cancellable = null)
				throws Error, IOError {
			var result = new uint8[size];

			size_t offset = 0;
			size_t max_bytes_per_packet = (max_packet_size - Packet.OVERHEAD) / 2;
			do {
				size_t chunk_size = size_t.min (size - offset, max_bytes_per_packet);

				var request = make_packet_builder_sized (16)
					.append_c ('m')
					.append_address (address + offset)
					.append_c (',')
					.append_size (chunk_size)
					.build ();
				var response = yield query (request, cancellable);

				Bytes chunk = Protocol.parse_hex_bytes (response.payload);
				if (chunk.get_size () != chunk_size) {
					throw new Error.INVALID_ARGUMENT (
						"Unable to read from 0x%" + uint64.FORMAT_MODIFIER + "x: invalid address", address);
				}

				Memory.copy ((uint8 *) result + offset, chunk.get_data (), chunk_size);

				offset += chunk_size;
			} while (offset != size);

			return new Bytes.take ((owned) result);
		}

		public async void write_byte_array (uint64 address, Bytes bytes, Cancellable? cancellable = null)
				throws Error, IOError {
			size_t header_size = 1 + 16 + 1 + 8 + 1 + Packet.OVERHEAD;
			size_t budget = max_packet_size - header_size;

			var data = bytes.get_data ();
			size_t offset = 0;
			size_t remaining = bytes.length;

			var builder = make_packet_builder_sized (32 + (remaining * 2));

			while (remaining != 0) {
				size_t slice_size, cost;
				if (binary_writes_supported) {
					slice_size = 0;
					cost = 0;
					while (slice_size != remaining) {
						size_t byte_cost = needs_escaping (data[offset + slice_size]) ? 2 : 1;
						if (cost + byte_cost > budget)
							break;
						cost += byte_cost;
						slice_size++;
					}
				} else {
					slice_size = size_t.min (remaining, budget / 2);
				}

				builder
					.append_c (binary_writes_supported ? 'X' : 'M')
					.append_address (address + offset)
					.append_c (',')
					.append_size (slice_size)
					.append_c (':');

				for (size_t i = 0; i != slice_size; i++) {
					uint8 byte = data[offset + i];
					if (binary_writes_supported)
						builder.append_c ((char) byte);
					else
						builder.append_hexbyte (byte);
				}

				if (binary_writes_supported) {
					try {
						yield execute (builder.build (), cancellable);
					} catch (Error e) {
						binary_writes_supported = false;
						builder.reset ();
						continue;
					}
				} else {
					yield execute (builder.build (), cancellable);
				}

				builder.reset ();

				offset += slice_size;
				remaining -= slice_size;
			}
		}

		private static bool needs_escaping (uint8 byte) {
			switch ((char) byte) {
				case PACKET_CHARACTER:
				case CHECKSUM_CHARACTER:
				case ESCAPE_CHARACTER:
				case REPEAT_CHARACTER:
					return true;
				default:
					return false;
			}
		}

		public async uint64 read_pointer (uint64 address, Cancellable? cancellable = null) throws Error, IOError {
			var buffer = yield read_buffer (address, pointer_size, cancellable);
			return buffer.read_pointer (0);
		}

		public async void write_pointer (uint64 address, uint64 val, Cancellable? cancellable = null) throws Error, IOError {
			var buffer = make_buffer_builder ()
				.append_pointer (val)
				.build ();
			yield write_byte_array (address, buffer, cancellable);
		}

		public async bool read_bool (uint64 address, Cancellable? cancellable = null) throws Error, IOError {
			var data = yield read_byte_array (address, 1);
			return data.get (0) != 0 ? true : false;
		}

		public async void write_bool (uint64 address, bool val, Cancellable? cancellable = null) throws Error, IOError {
			yield write_byte_array (address, new Bytes ({ val ? 1 : 0 }), cancellable);
		}

		public BufferBuilder make_buffer_builder () {
			return new BufferBuilder (byte_order, pointer_size);
		}

		public Buffer make_buffer (Bytes bytes) {
			return new Buffer (bytes, byte_order, pointer_size);
		}

		public async Buffer read_buffer (uint64 address, size_t size, Cancellable? cancellable = null) throws Error, IOError {
			var bytes = yield read_byte_array (address, size, cancellable);
			return make_buffer (bytes);
		}

		public async Breakpoint add_breakpoint (Breakpoint.Kind kind, uint64 address, size_t size, Cancellable? cancellable = null)
				throws Error, IOError {
			check_stopped ();

			if (kind == SOFT && "protected-code" in supported_features)
				kind = HARD;

			var breakpoint = new Breakpoint (kind, address, size, this);
			yield breakpoint.enable (cancellable);
			breakpoints[address] = breakpoint;

			breakpoint.removed.connect (on_breakpoint_removed);

			return breakpoint;
		}

		private void on_breakpoint_removed (Breakpoint breakpoint) {
			breakpoints.unset (breakpoint.address);

			var exception = breakpoint_exception;
			if (exception != null && exception.breakpoint == breakpoint)
				breakpoint_exception = null;
		}

		public async string run_remote_command (string command, Cancellable? cancellable = null) throws Error, IOError {
			int n = command.length;
			var builder = make_packet_builder_sized (10 + (n * 2))
				.append ("qRcmd,");
			for (int i = 0; i != n; i++)
				builder.append_hexbyte (command[i]);

			var output = new Gee.ArrayList<Packet> ();
			Packet response = yield query_with_predicate (builder.build (), packet => {
				unowned string payload = packet.payload;
				if (payload.has_prefix ("OK") || payload[0] == 'E' || payload == "")
					return COMPLETE;
				if (payload[0] == NOTIFICATION_TYPE_OUTPUT) {
					output.add (packet);
					return ABSORB;
				}
				return KEEP_TRYING;
			}, cancellable);
			if (response.payload == "")
				throw new Error.NOT_SUPPORTED ("Command not supported by the remote stub");
			check_execute_response (response, new Bytes (command.data));

			var result = new StringBuilder ();
			foreach (Packet p in output)
				result.append (Protocol.parse_hex_encoded_utf8_string (p.payload[1:]));
			return result.str;
		}

		protected async void load_target_properties (Cancellable? cancellable = null) throws Error, IOError {
			TargetSpec spec = yield query_target_spec (cancellable);

			arch = spec.arch;
			pointer_size = infer_pointer_size_from_arch (spec.arch);
			byte_order = infer_byte_order_from_arch (spec.arch);

			install_registers (spec.registers);
		}

		protected void request_stop_info () {
			var command = make_packet_builder_sized (5)
				.append_c ('?')
				.build ();
			write_bytes (command);
		}

		private async TargetSpec query_target_spec (Cancellable? cancellable) throws Error, IOError {
			uint next_regnum = 0;
			FeatureDocument? target = null;
			try {
				target = yield fetch_feature_document ("target.xml", next_regnum, cancellable);
			} catch (Error e) {
				if (e is Error.NOT_SUPPORTED)
					return new TargetSpec (UNKNOWN, new Gee.ArrayList<Register> ());
				throw e;
			}
			next_regnum = target.next_regnum;

			var pending = new Gee.ArrayQueue<string> ();
			var processed = new Gee.HashSet<string> ();

			pending.add_all (target.includes);

			string? href;
			while ((href = pending.poll ()) != null) {
				if (href in processed)
					continue;

				FeatureDocument child = yield fetch_feature_document (href, next_regnum, cancellable);
				next_regnum = child.next_regnum;
				target.registers.add_all (child.registers);

				pending.add_all (child.includes);
				processed.add (href);
			}

			target.registers.sort ((reg_a, reg_b) => {
				uint a = reg_a.id;
				uint b = reg_b.id;
				if (a < b)
					return -1;
				if (a > b)
					return 1;
				return 0;
			});

			return new TargetSpec (target.arch, target.registers);
		}

		private async FeatureDocument fetch_feature_document (string name, uint next_regnum, Cancellable? cancellable)
				throws Error, IOError {
			var xml = new StringBuilder.sized (4096);

			uint offset = 0;
			char status = 'l';
			do {
				var response = yield query_simple ("qXfer:features:read:%s:%x,1ffff".printf (name, offset), cancellable);

				string payload = response.payload;
				if (payload.length == 0 || payload[0] == 'E') {
					// Some stubs advertise a target.xml that includes standard register documents
					// but decline to serve those documents (the Android emulator's does). Fall back
					// to a built-in copy so the register layout is still known.
					unowned string? builtin = builtin_feature_document (name);
					if (builtin != null)
						return FeatureDocument.from_xml (builtin, next_regnum);
					if (payload.length == 0)
						throw new Error.NOT_SUPPORTED ("Feature query not supported by the remote stub");
					throw new Error.INVALID_ARGUMENT ("Feature document '%s' not found", name);
				}

				status = payload[0];

				string * chunk = (string *) payload + 1;
				xml.append (chunk);
				offset += chunk->length;
			} while (status == 'm');

			return FeatureDocument.from_xml (xml.str, next_regnum);
		}

		private static unowned string? builtin_feature_document (string name) {
			switch (name) {
				case "aarch64-core.xml":	return AARCH64_CORE_FEATURE;
				case "aarch64-fpu.xml":		return AARCH64_FPU_FEATURE;
				default:			return null;
			}
		}

		private const string AARCH64_CORE_FEATURE = """<?xml version="1.0"?>
<!DOCTYPE feature SYSTEM "gdb-target.dtd">
<feature name="org.gnu.gdb.aarch64.core">
  <reg name="x0" bitsize="64"/>
  <reg name="x1" bitsize="64"/>
  <reg name="x2" bitsize="64"/>
  <reg name="x3" bitsize="64"/>
  <reg name="x4" bitsize="64"/>
  <reg name="x5" bitsize="64"/>
  <reg name="x6" bitsize="64"/>
  <reg name="x7" bitsize="64"/>
  <reg name="x8" bitsize="64"/>
  <reg name="x9" bitsize="64"/>
  <reg name="x10" bitsize="64"/>
  <reg name="x11" bitsize="64"/>
  <reg name="x12" bitsize="64"/>
  <reg name="x13" bitsize="64"/>
  <reg name="x14" bitsize="64"/>
  <reg name="x15" bitsize="64"/>
  <reg name="x16" bitsize="64"/>
  <reg name="x17" bitsize="64"/>
  <reg name="x18" bitsize="64"/>
  <reg name="x19" bitsize="64"/>
  <reg name="x20" bitsize="64"/>
  <reg name="x21" bitsize="64"/>
  <reg name="x22" bitsize="64"/>
  <reg name="x23" bitsize="64"/>
  <reg name="x24" bitsize="64"/>
  <reg name="x25" bitsize="64"/>
  <reg name="x26" bitsize="64"/>
  <reg name="x27" bitsize="64"/>
  <reg name="x28" bitsize="64"/>
  <reg name="x29" bitsize="64"/>
  <reg name="x30" bitsize="64"/>
  <reg name="sp" bitsize="64" type="data_ptr"/>
  <reg name="pc" bitsize="64" type="code_ptr"/>
  <reg name="cpsr" bitsize="32"/>
</feature>""";

		private const string AARCH64_FPU_FEATURE = """<?xml version="1.0"?>
<!DOCTYPE feature SYSTEM "gdb-target.dtd">
<feature name="org.gnu.gdb.aarch64.fpu">
  <reg name="v0" bitsize="128" type="uint128"/>
  <reg name="v1" bitsize="128" type="uint128"/>
  <reg name="v2" bitsize="128" type="uint128"/>
  <reg name="v3" bitsize="128" type="uint128"/>
  <reg name="v4" bitsize="128" type="uint128"/>
  <reg name="v5" bitsize="128" type="uint128"/>
  <reg name="v6" bitsize="128" type="uint128"/>
  <reg name="v7" bitsize="128" type="uint128"/>
  <reg name="v8" bitsize="128" type="uint128"/>
  <reg name="v9" bitsize="128" type="uint128"/>
  <reg name="v10" bitsize="128" type="uint128"/>
  <reg name="v11" bitsize="128" type="uint128"/>
  <reg name="v12" bitsize="128" type="uint128"/>
  <reg name="v13" bitsize="128" type="uint128"/>
  <reg name="v14" bitsize="128" type="uint128"/>
  <reg name="v15" bitsize="128" type="uint128"/>
  <reg name="v16" bitsize="128" type="uint128"/>
  <reg name="v17" bitsize="128" type="uint128"/>
  <reg name="v18" bitsize="128" type="uint128"/>
  <reg name="v19" bitsize="128" type="uint128"/>
  <reg name="v20" bitsize="128" type="uint128"/>
  <reg name="v21" bitsize="128" type="uint128"/>
  <reg name="v22" bitsize="128" type="uint128"/>
  <reg name="v23" bitsize="128" type="uint128"/>
  <reg name="v24" bitsize="128" type="uint128"/>
  <reg name="v25" bitsize="128" type="uint128"/>
  <reg name="v26" bitsize="128" type="uint128"/>
  <reg name="v27" bitsize="128" type="uint128"/>
  <reg name="v28" bitsize="128" type="uint128"/>
  <reg name="v29" bitsize="128" type="uint128"/>
  <reg name="v30" bitsize="128" type="uint128"/>
  <reg name="v31" bitsize="128" type="uint128"/>
  <reg name="fpsr" bitsize="32"/>
  <reg name="fpcr" bitsize="32"/>
</feature>""";

		private static uint infer_pointer_size_from_arch (TargetArch arch) {
			switch (arch) {
				case UNKNOWN:
					return (uint) sizeof (void *);
				case IA32:
				case ARM:
				case MIPS:
					return 4;
				case X64:
				case ARM64:
					return 8;
			}

			assert_not_reached ();
		}

		private static ByteOrder infer_byte_order_from_arch (TargetArch arch) {
			switch (arch) {
				case UNKNOWN:
					return HOST;
				case IA32:
				case X64:
				case ARM:
				case ARM64:
					return LITTLE_ENDIAN;
				case MIPS:
					return BIG_ENDIAN;
			}

			assert_not_reached ();
		}

		internal Gee.List<Register>? get_registers () {
			return registers;
		}

		internal Register get_register_by_name (string name) throws Error {
			Register? reg = register_by_name[name];
			if (reg == null)
				throw new Error.INVALID_ARGUMENT ("Invalid register name: %s", name);
			return reg;
		}

		internal bool has_register (string name) {
			return register_by_name.has_key (name);
		}

		internal Register get_register_by_index (uint index) throws Error {
			if (index >= registers.size)
				throw new Error.INVALID_ARGUMENT ("Invalid register index: %u", index);
			return registers[(int) index];
		}

		private void check_stopped () throws Error {
			if (state != STOPPED) {
				throw new Error.INVALID_OPERATION ("Invalid operation when not STOPPED, current state is %s",
					state.to_nick ().up ());
			}
		}

		public async void execute_simple (string command, Cancellable? cancellable) throws Error, IOError {
			var raw_command = make_packet_builder_sized (command.length + 15 & (size_t) ~15)
				.append (command)
				.build ();
			yield execute (raw_command, cancellable);
		}

		public async void execute (Bytes command, Cancellable? cancellable) throws Error, IOError {
			Packet response = yield query (command, cancellable);
			check_execute_response (response, command);
		}

		public async void perform_execute (Bytes command, Cancellable? cancellable, Promise<bool> request) throws Error, IOError {
			try {
				yield execute (command, cancellable);
				request.resolve (true);
			} catch (GLib.Error e) {
				request.reject (e);
			}
		}

		private static void check_execute_response (Packet packet, Bytes command) throws Error {
			unowned string response = packet.payload;
			if (response[0] == 'E') {
				string reason = response[1:response.length];
				if (reason == "Locked")
					throw new Error.INVALID_OPERATION ("Device is locked");
				else
					throw new Error.NOT_SUPPORTED ("Stub rejected %s: %s", name_of (command), reason);
			}

			if (response != "OK")
				throw new Error.PROTOCOL ("Unexpected response: %s", response);
		}

		private static string name_of (Bytes command) {
			var text = new StringBuilder ();
			foreach (uint8 byte in command.get_data ()) {
				if (byte < 0x20 || byte > 0x7e || text.len == 24)
					break;
				text.append_c ((char) byte);
			}

			return text.str;
		}

		public async Packet query_simple (string request, Cancellable? cancellable) throws Error, IOError {
			var raw_request = make_packet_builder_sized (request.length + 15 & (size_t) ~15)
				.append (request)
				.build ();
			return yield query (raw_request, cancellable);
		}

		public async string query_property (string name, Cancellable? cancellable) throws Error, IOError {
			Packet response = yield query_simple ("q" + name, cancellable);

			unowned string val = response.payload;
			string ack = "q%s:".printf (name);
			if (val.has_prefix (ack))
				return val[ack.length:];
			return val;
		}

		public async Packet query (Bytes request, Cancellable? cancellable) throws Error, IOError {
			return yield query_with_predicate (request, null, cancellable);
		}

		public async Packet query_with_predicate (Bytes request, owned ResponsePredicate? predicate, Cancellable? cancellable)
				throws Error, IOError {
			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Unable to perform query; connection is closed");

			var pending = new PendingResponse ((owned) predicate, query_with_predicate.callback);
			pending_responses.offer (pending);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				pending.complete_with_error (new IOError.CANCELLED ("Operation was cancelled"));
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			write_bytes (request);

			yield;

			cancel_source.destroy ();

			cancellable.set_error_if_cancelled ();

			var response = pending.response;
			if (response == null)
				throw_api_error (pending.error);

			return response;
		}

		private async void process_incoming_packets () {
			while (true) {
				try {
					var packet = yield read_packet ();

					dispatch_packet (packet);
				} catch (GLib.Error error) {
					change_state (CLOSED);

					foreach (var pending_response in pending_responses)
						pending_response.complete_with_error (error);
					pending_responses.clear ();

					foreach (var observer in on_stop.to_array ())
						observer.func ();

					closed ();

					return;
				}
			}
		}

		private async void process_pending_writes () {
			write_request = new Promise<uint> ();
			size_t total_bytes_written = 0;
			try {
				while (!pending_writes.is_empty) {
					Bytes current = pending_writes.peek ();

					size_t bytes_written;
					try {
						yield output.write_all_async (current.get_data (), Priority.DEFAULT, io_cancellable,
							out bytes_written);
						total_bytes_written += bytes_written;
					} catch (GLib.Error e) {
						return;
					}

					pending_writes.poll ();
				}
			} finally {
				write_request.resolve ((uint) total_bytes_written);
				write_request = null;
			}
		}

		private void dispatch_packet (Packet packet) throws Error {
			if (try_handle_specific_response (packet))
				return;

			if (try_handle_notification (packet))
				return;

			handle_wildcard_response (packet);
		}

		private bool try_handle_specific_response (Packet packet) throws Error {
			ResponseAction action = KEEP_TRYING;
			PendingResponse? pr = pending_responses.first_match (pr => {
				if (pr.predicate == null)
					return false;
				action = pr.predicate (packet);
				return action != KEEP_TRYING;
			});
			if (pr == null)
				return false;

			if (action == ABSORB)
				return true;

			pending_responses.remove (pr);

			pr.complete_with_response (packet);
			return true;
		}

		private void handle_wildcard_response (Packet response) throws Error {
			PendingResponse? pr = pending_responses.first_match (pr => pr.predicate == null);
			if (pr == null)
				throw new Error.PROTOCOL ("Unexpected response");
			pending_responses.remove (pr);

			pr.complete_with_response (response);
		}

		protected bool try_handle_notification (Packet packet) throws Error {
			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Unable to handle notification; connection is closed");

			unowned string payload = packet.payload;
			if (payload == "OK")
				return false;

			unowned string data = (string) ((char *) payload + 1);
			switch (payload[0]) {
				case NOTIFICATION_TYPE_EXIT_STATUS:
				case NOTIFICATION_TYPE_EXIT_SIGNAL:
					handle_exit (data);
					return true;
				case NOTIFICATION_TYPE_STOP:
				case NOTIFICATION_TYPE_STOP_WITH_PROPERTIES:
					handle_stop.begin (data);
					return true;
				case NOTIFICATION_TYPE_OUTPUT:
					handle_output (data);
					return true;
				default:
					return false;
			}
		}

		private void handle_exit (string data) throws Error {
			change_state (STOPPED);
			foreach (var observer in on_stop.to_array ())
				observer.func ();
		}

		private async void handle_stop (string data) throws Error, IOError {
			if (data.length < 2)
				throw new Error.PROTOCOL ("Invalid stop packet");

			uint64 raw_signum;
			try {
				uint64.from_string (data[0:2], out raw_signum, 16);
			} catch (NumberParserError e) {
				throw new Error.PROTOCOL ("Invalid stop packet: %s", e.message);
			}
			var signum = (uint) raw_signum;

			unowned string rest = (string) ((char *) data + 2);
			var properties = PropertyDictionary.parse (rest);

			Exception exception;
			Breakpoint? breakpoint;
			yield parse_stop (signum, properties, out exception, out breakpoint);

			breakpoint_exception = (breakpoint != null) ? exception : null;
			change_state (STOPPED, exception);
			foreach (var observer in on_stop.to_array ())
				observer.func ();
		}

		protected virtual async void parse_stop (uint signum, PropertyDictionary properties, out Exception exception,
				out Breakpoint? breakpoint) throws Error, IOError {
			string thread_id;
			if (properties.has ("thread")) {
				thread_id = properties.get_string ("thread");
			} else {
				Packet thread_id_response = yield query_simple ("qC", io_cancellable);
				unowned string payload = thread_id_response.payload;
				if (payload.length < 3)
					throw new Error.PROTOCOL ("Invalid thread ID response");
				thread_id = payload[2:];
			}

			string? name = null;
			Packet info_response = yield query_simple ("qThreadExtraInfo," + thread_id, io_cancellable);
			unowned string payload = info_response.payload;
			if (payload.length > 0)
				name = Protocol.parse_hex_encoded_utf8_string (payload);

			Thread thread = new Thread (thread_id, name, this);

			if (signum == UnixSignal.SIGTRAP) {
				string pc_reg_name;
				switch (arch) {
					case IA32:
						pc_reg_name = "eip";
						break;
					case X64:
						pc_reg_name = "rip";
						break;
					default:
						pc_reg_name = "pc";
						break;
				}
				uint64 pc = yield thread.read_register (pc_reg_name, io_cancellable);

				breakpoint = breakpoints[pc];
			} else {
				breakpoint = null;
			}

			exception = new Exception (signum, breakpoint, thread);
		}

		private void handle_output (string hex_bytes) throws Error {
			var bytes = Protocol.parse_hex_bytes (hex_bytes);
			console_output (bytes);
		}

		public PacketBuilder make_packet_builder_sized (size_t capacity) {
			return new PacketBuilder (capacity);
		}

		private async Packet read_packet () throws Error, IOError {
			string? header = null;
			do {
				header = yield read_string (1);
			} while (header == ACK_NOTIFICATION || header == NACK_NOTIFICATION);

			string? body;
			size_t body_size;
			try {
				body = yield input.read_upto_async (CHECKSUM_MARKER, 1, Priority.DEFAULT, io_cancellable, out body_size);
			} catch (IOError e) {
				if (e is IOError.CANCELLED)
					throw e;
				throw new Error.TRANSPORT ("%s", e.message);
			}
			if (body == null)
				body = "";

			string trailer = yield read_string (3);

			var packet = depacketize (header, body, body_size, trailer);

			if (ack_mode == SEND_ACKS) {
				write_string (ACK_NOTIFICATION);
				var req = write_request.future;
				yield req.wait_async (io_cancellable);
			}

			return packet;
		}

		private async string read_string (uint length) throws Error, IOError {
			var buf = new uint8[length + 1];
			buf[length] = 0;

			size_t bytes_read;
			try {
				yield input.read_all_async (buf[0:length], Priority.DEFAULT, io_cancellable, out bytes_read);
			} catch (GLib.Error e) {
				if (e is IOError.CANCELLED)
					throw (IOError) e;
				throw new Error.TRANSPORT ("%s", e.message);
			}

			if (bytes_read == 0)
				throw new Error.TRANSPORT ("Connection closed");

			return (string) buf;
		}

		private static bool tracing = Environment.get_variable ("FRIDA_GDB_TRACE") != null;

		private static string printable (Bytes bytes) {
			var text = new StringBuilder ();
			foreach (uint8 byte in bytes.get_data ()) {
				if (byte >= 0x20 && byte <= 0x7e)
					text.append_c ((char) byte);
				else
					text.append_printf ("\\x%02x", byte);
				if (text.len > 120) {
					text.append ("...");
					break;
				}
			}

			return text.str;
		}

		private void write_string (string str) {
			unowned uint8[] buf = (uint8[]) str;
			write_bytes (new Bytes (buf[0:str.length]));
		}

		private void write_bytes (Bytes bytes) {
			if (tracing)
				printerr ("[gdb] > %s\n", printable (bytes));

			pending_writes.offer (bytes);
			if (pending_writes.size == 1)
				process_pending_writes.begin ();
		}

		private static Packet depacketize (string header, string data, size_t data_size, string trailer) throws Error {
			var result = new StringBuilder.sized (data_size);

			for (size_t offset = 0; offset != data_size; offset++) {
				char ch = data[(long) offset];
				if (ch == ESCAPE_CHARACTER) {
					if (offset == data_size - 1)
						throw new Error.PROTOCOL ("Invalid packet");
					uint8 escaped_byte = data[(long) (++offset)];
					result.append_c ((char) (escaped_byte ^ ESCAPE_KEY));
				} else if (ch == REPEAT_CHARACTER) {
					if (offset == 0 || offset == data_size - 1)
						throw new Error.PROTOCOL ("Invalid packet");
					char char_to_repeat = data[(long) (offset - 1)];
					uint8 repeat_count = (uint8) data[(long) (++offset)] - REPEAT_BASE + REPEAT_BIAS;
					for (uint8 repeat_index = 0; repeat_index != repeat_count; repeat_index++)
						result.append_c (char_to_repeat);
				} else {
					result.append_c (ch);
				}
			}

			return new Packet.from_bytes (StringBuilder.free_to_bytes ((owned) result));
		}

		private static uint8 compute_checksum (string data, long offset, long length) {
			uint8 sum = 0;

			long end_index = offset + length;
			for (long i = offset; i != end_index; i++)
				sum += (uint8) data[i];

			return sum;
		}

		public sealed class Packet {
			public const size_t OVERHEAD = 1 + 1 + 2;

			public string payload {
				get;
				private set;
			}

			public Bytes payload_bytes {
				get;
				private set;
			}

			public Packet.from_bytes (Bytes payload_bytes) {
				this.payload = (string) payload_bytes.get_data ();
				this.payload_bytes = payload_bytes;
			}
		}

		public sealed class PacketBuilder {
			private StringBuilder? buffer;
			private size_t initial_capacity;

			public PacketBuilder (size_t capacity) {
				this.initial_capacity = capacity + Packet.OVERHEAD;

				reset ();
			}

			public void reset () {
				if (buffer == null)
					buffer = new StringBuilder.sized (initial_capacity);
				else
					buffer.truncate ();

				buffer.append_c (PACKET_CHARACTER);
			}

			public unowned PacketBuilder append (string val) {
				long length = val.length;
				for (long i = 0; i != length; i++)
					append_c (val[i]);
				return this;
			}

			public unowned PacketBuilder append_c (char c) {
				switch (c) {
					case PACKET_CHARACTER:
					case CHECKSUM_CHARACTER:
					case ESCAPE_CHARACTER:
					case REPEAT_CHARACTER:
						buffer.append_c (ESCAPE_CHARACTER);
						buffer.append_c ((char) ((uint8) c ^ ESCAPE_KEY));
						break;
					default:
						buffer.append_c (c);
						break;
				}
				return this;
			}

			public unowned PacketBuilder append_escaped (string val) {
				buffer.append (val);
				return this;
			}

			public unowned PacketBuilder append_c_escaped (char c) {
				buffer.append_c (c);
				return this;
			}

			public unowned PacketBuilder append_address (uint64 address) {
				buffer.append_printf ("%" + uint64.FORMAT_MODIFIER + "x", address);
				return this;
			}

			public unowned PacketBuilder append_size (size_t size) {
				buffer.append_printf ("%" + size_t.FORMAT_MODIFIER + "x", size);
				return this;
			}

			public unowned PacketBuilder append_uint (uint val) {
				buffer.append_printf ("%u", val);
				return this;
			}

			public unowned PacketBuilder append_process_id (uint process_id) {
				buffer.append_printf ("%x", process_id);
				return this;
			}

			public unowned PacketBuilder append_register_id (uint register_id) {
				buffer.append_printf ("%x", register_id);
				return this;
			}

			public unowned PacketBuilder append_register_value (uint64 val) {
				return append_address (val);
			}

			public unowned PacketBuilder append_hexbyte (uint8 byte) {
				buffer.append_c (Protocol.NIBBLE_TO_HEX_CHAR[byte >> 4]);
				buffer.append_c (Protocol.NIBBLE_TO_HEX_CHAR[byte & 0xf]);
				return this;
			}

			public Bytes build () {
				buffer.append_c (CHECKSUM_CHARACTER);
				buffer.append_printf ("%02x", compute_checksum (buffer.str, 1, buffer.len - 2));
				return StringBuilder.free_to_bytes ((owned) buffer);
			}
		}

		private class StopObserverEntry {
			public SourceFunc? func;

			public StopObserverEntry (owned SourceFunc func) {
				this.func = (owned) func;
			}
		}

		public delegate ResponseAction ResponsePredicate (Packet packet);

		public enum ResponseAction {
			COMPLETE,
			ABSORB,
			KEEP_TRYING
		}

		private class PendingResponse {
			public ResponsePredicate? predicate;

			public Packet? response {
				get;
				private set;
			}

			public GLib.Error? error {
				get;
				private set;
			}

			private SourceFunc? handler;

			public PendingResponse (owned ResponsePredicate predicate, owned SourceFunc handler) {
				this.predicate = (owned) predicate;
				this.handler = (owned) handler;
			}

			public void complete_with_response (Packet response) {
				this.response = response;
				invoke_handler_in_idle ();
			}

			public void complete_with_error (GLib.Error error) {
				this.error = error;
				invoke_handler_in_idle ();
			}

			private void invoke_handler_in_idle () {
				var source = new IdleSource ();
				source.set_callback (() => {
					if (handler != null) {
						handler ();
						handler = null;
					}
					return Source.REMOVE;
				});
				source.attach (MainContext.get_thread_default ());
			}
		}

		protected class PropertyDictionary {
			private Gee.HashMap<string, string> properties = new Gee.HashMap<string, string> ();

			public static PropertyDictionary parse (string raw_properties) throws Error {
				var dictionary = new PropertyDictionary ();

				var properties = dictionary.properties;

				var pairs = raw_properties.split (";");
				foreach (var pair in pairs) {
					if (pair.length == 0)
						continue;

					var tokens = pair.split (":", 2);
					if (tokens.length != 2)
						throw new Error.PROTOCOL ("Invalid property dictionary pair");
					unowned string key = tokens[0];
					unowned string val = tokens[1];

					if (!properties.has_key (key)) {
						properties[key] = val;
					} else {
						properties[key] = properties[key] + "," + val;
					}
				}

				return dictionary;
			}

			public void foreach (Gee.ForallFunc<Gee.Map.Entry<string, string>> f) {
				properties.foreach (f);
			}

			public bool has (string name) {
				return properties.has_key (name);
			}

			public string get_string (string name) throws Error {
				var val = properties[name];
				if (val == null)
					throw new Error.PROTOCOL ("Property '%s' not found", name);
				return val;
			}

			public uint get_uint (string name) throws Error {
				return Protocol.parse_uint (get_string (name), 16);
			}

			public uint64 get_uint64 (string name) throws Error {
				return Protocol.parse_uint64 (get_string (name), 16);
			}

			public Gee.ArrayList<string> get_string_array (string name) throws Error {
				var result = new Gee.ArrayList<string> ();
				result.add_all_array (get_string (name).split (","));
				return result;
			}

			public Gee.ArrayList<uint> get_uint_array (string name) throws Error {
				var result = new Gee.ArrayList<uint> ();

				foreach (var element in get_string (name).split (","))
					result.add (Protocol.parse_uint (element, 16));

				return result;
			}

			public Gee.ArrayList<uint64?> get_uint64_array (string name) throws Error {
				var result = new Gee.ArrayList<uint64?> ();

				foreach (var element in get_string (name).split (","))
					result.add (Protocol.parse_uint64 (element, 16));

				return result;
			}
		}

		protected class Register {
			public string name {
				get;
				private set;
			}

			public string? altname {
				get;
				private set;
			}

			public uint id {
				get;
				private set;
			}

			public uint bitsize {
				get;
				private set;
			}

			public bool writable {
				get;
				private set;
			}

			public Register (string name, string? altname, uint id, uint bitsize, bool writable = true) {
				this.name = name;
				this.altname = altname;
				this.id = id;
				this.bitsize = bitsize;
				this.writable = writable;
			}
		}

		protected class TargetSpec {
			public TargetArch arch;
			public Gee.List<Register> registers;

			public TargetSpec (TargetArch arch, Gee.List<Register> registers) {
				this.arch = arch;
				this.registers = registers;
			}
		}

		private class FeatureDocument {
			public TargetArch arch = UNKNOWN;

			public Gee.List<Register> registers = new Gee.ArrayList<Register> ();
			public uint next_regnum;

			public Gee.List<string> includes = new Gee.ArrayList<string> ();

			public static FeatureDocument from_xml (string xml, uint next_regnum) throws Error {
				var doc = new FeatureDocument (next_regnum);

				var parser = new Parser (doc);
				parser.parse (xml);

				return doc;
			}

			private FeatureDocument (uint next_regnum) {
				this.next_regnum = next_regnum;
			}

			private class Parser {
				private FeatureDocument doc;

				private bool in_architecture = false;

				private const MarkupParser CALLBACKS = {
					on_start_element,
					on_end_element,
					on_text_element,
					null,
					null
				};

				public Parser (FeatureDocument doc) {
					this.doc = doc;
				}

				public void parse (string xml) throws Error {
					try {
						var context = new MarkupParseContext (CALLBACKS, 0, this, null);
						context.parse (xml, -1);
					} catch (MarkupError e) {
						throw new Error.PROTOCOL ("%s", e.message);
					}
				}

				private void on_start_element (MarkupParseContext context, string element_name, string[] attribute_names,
						string[] attribute_values) throws MarkupError {
					if (element_name == "reg") {
						on_reg_element (attribute_names, attribute_values);
						return;
					}

					if (element_name == "feature") {
						on_feature_element (attribute_names, attribute_values);
						return;
					}

					if (element_name == "architecture") {
						in_architecture = true;
						return;
					}

					if (element_name == "xi:include") {
						on_include_element (attribute_names, attribute_values);
						return;
					}
				}

				private void on_end_element (MarkupParseContext context, string element_name) throws MarkupError {
					in_architecture = false;
				}

				private void on_text_element (MarkupParseContext context, string text, size_t text_len) throws MarkupError {
					if (in_architecture)
						doc.arch = parse_gdb_arch (text);
				}

				private void on_feature_element (string[] attribute_names, string[] attribute_values) {
					uint i = 0;
					foreach (unowned string attribute_name in attribute_names) {
						unowned string val = attribute_values[i];

						if (attribute_name == "name") {
							if (val.has_prefix ("com.apple.debugserver."))
								doc.arch = parse_lldb_arch (val[22:]);
							return;
						}

						i++;
					}
				}

				private void on_reg_element (string[] attribute_names, string[] attribute_values) {
					string? name = null;
					string? altname = null;
					int regnum = -1;
					int bitsize = -1;
					uint i = 0;
					foreach (unowned string attribute_name in attribute_names) {
						unowned string val = attribute_values[i];

						if (attribute_name == "name")
							name = val.down ();
						else if (attribute_name == "altname")
							altname = val.down ();
						else if (attribute_name == "regnum")
							regnum = int.parse (val);
						else if (attribute_name == "bitsize")
							bitsize = int.parse (val);

						i++;
					}
					if (name == null)
						return;
					if (regnum == -1)
						regnum = (int) doc.next_regnum++;
					else
						doc.next_regnum = regnum + 1;

					doc.registers.add (new Register (name, altname, regnum, bitsize));
				}

				private void on_include_element (string[] attribute_names, string[] attribute_values) {
					uint i = 0;
					foreach (unowned string attribute_name in attribute_names) {
						unowned string val = attribute_values[i];

						if (attribute_name == "href") {
							doc.includes.add (val);
							return;
						}
					}
				}

				private static TargetArch parse_gdb_arch (string name) {
					switch (name) {
						case "i386":		return IA32;
						case "i386:x86-64":	return X64;
						case "arm":		return ARM;
						case "aarch64":		return ARM64;
						case "mips":		return MIPS;
						default:		return UNKNOWN;
					}
				}

				private static TargetArch parse_lldb_arch (string name) {
					if (name == "i386")
						return IA32;

					if (name.has_prefix ("x86_64"))
						return X64;

					if (name.has_prefix ("arm64"))
						return ARM64;

					if (name.has_prefix ("arm"))
						return ARM;

					return UNKNOWN;
				}
			}
		}
	}

	public enum TargetArch {
		UNKNOWN,
		IA32,
		X64,
		ARM,
		ARM64,
		MIPS;

		public static TargetArch from_nick (string nick) throws Error {
			return Marshal.enum_from_nick<TargetArch> (nick);
		}

		public string to_nick () {
			return Marshal.enum_to_nick<TargetArch> (this);
		}
	}

	public class Thread : Object {
		public string id {
			get;
			construct;
		}

		public string? name {
			get;
			construct;
		}

		public weak Client client {
			get;
			construct;
		}

		public Thread (string id, string? name, Client client) {
			Object (
				id: id,
				name: name,
				client: client
			);
		}

		public async void step (Cancellable? cancellable = null) throws Error, IOError {
			yield client._step_thread (this, cancellable);
		}

		public void step_and_continue () throws Error {
			client._step_thread_and_continue (this);
		}

		public async Gee.Map<string, Variant> read_registers (Cancellable? cancellable = null) throws Error, IOError {
			if (!client.bulk_registers)
				return yield read_registers_individually (cancellable);

			var response = yield client.query_simple ("g", cancellable);
			if (response.payload.length == 0) {
				client.bulk_registers = false;
				return yield read_registers_individually (cancellable);
			}

			var result = new Gee.HashMap<string, Variant> ();

			unowned string payload = response.payload;
			uint encoded_size = payload.length;

			uint hex_chars_per_byte = 2;
			uint bits_per_byte = 8;

			uint offset = 0;
			uint size = encoded_size / hex_chars_per_byte;
			uint pointer_size = client.pointer_size;
			ByteOrder byte_order = client.byte_order;
			for (uint i = 0; offset != size; i++) {
				GDB.Client.Register reg = client.get_register_by_index (i);

				uint reg_size = reg.bitsize / bits_per_byte;
				uint end_offset = offset + reg_size;
				if (end_offset > size)
					throw new Error.PROTOCOL ("Truncated register value");

				string raw_val = payload[offset * hex_chars_per_byte:end_offset * hex_chars_per_byte];

				try {
					if (reg_size == pointer_size) {
						result[reg.name] = GDB.Protocol.parse_pointer_value (raw_val, pointer_size, byte_order);
					} else if (reg_size < pointer_size) {
						result[reg.name] = (uint32) GDB.Protocol.parse_integer_value (raw_val, byte_order);
					} else {
						Bytes bytes = Protocol.parse_hex_bytes (raw_val);
						result[reg.name] = Variant.new_from_data (new VariantType ("ay"), bytes.get_data (), true, bytes);
					}
				} catch (Error e) {
					throw new Error.PROTOCOL ("Unexpected register value encoding");
				}

				offset = end_offset;
			}
			return result;
		}

		private async Gee.Map<string, Variant> read_registers_individually (Cancellable? cancellable) throws Error, IOError {
			var result = new Gee.HashMap<string, Variant> ();
			foreach (var reg in client.get_registers ()) {
				if (reg.bitsize != 64)
					continue;
				try {
					result[reg.name] = yield read_register (reg.name, cancellable);
				} catch (Error e) {
					// Some system registers are not readable in the current context; skip them.
				}
			}
			return result;
		}

		public async void write_registers (Gee.Map<string, Variant> regs, Cancellable? cancellable = null) throws Error, IOError {
			if (!client.bulk_registers) {
				foreach (var e in regs.entries) {
					var reg = client.get_register_by_name (e.key);
					if (reg.writable && e.value.is_of_type (VariantType.UINT64))
						yield write_register (e.key, e.value.get_uint64 (), cancellable);
				}
				return;
			}

			int n = int.min (regs.size, client.get_registers ().size);

			// A stub that keeps some of its registers out of the group packet -- SVE, on a
			// QEMU that has them -- is written to one register at a time instead.
			for (int i = 0; i != n; i++) {
				if (regs[client.get_register_by_index (i).name] == null) {
					foreach (var e in regs.entries) {
						if (e.value.is_of_type (VariantType.UINT64))
							yield write_register (e.key, e.value.get_uint64 (), cancellable);
					}
					return;
				}
			}

			var builder = client.make_packet_builder_sized (2048)
				.append_c ('G');

			ByteOrder byte_order = client.byte_order;
			for (int i = 0; i != n; i++) {
				GDB.Client.Register reg = client.get_register_by_index (i);
				Variant val = regs[reg.name];

				if (val.is_of_type (VariantType.UINT64)) {
					builder.append (Protocol.unparse_integer_value (val.get_uint64 (), reg.bitsize / 8,
						byte_order));
				} else if (val.is_of_type (VariantType.UINT32)) {
					builder.append (Protocol.unparse_integer_value (val.get_uint32 (), reg.bitsize / 8,
						byte_order));
				} else {
					builder.append (Protocol.unparse_hex_bytes (val.get_data_as_bytes ()));
				}
			}

			yield client.execute (builder.build (), cancellable);
		}

		public async uint64 read_register (string name, Cancellable? cancellable = null) throws Error, IOError {
			var reg = client.get_register_by_name (name);

			var request = client.make_packet_builder_sized (32);
			request
				.append_c ('p')
				.append_register_id (reg.id);
			if ("thread-suffix" in client.features) {
				request
					.append (";thread:")
					.append (id)
					.append_c (';');
			} else {
				yield client.execute_simple ("Hg" + id, cancellable);
			}

			var response = yield client.query (request.build (), cancellable);

			unowned string payload = response.payload;
			if (payload.length == 0 || payload[0] == 'E')
				throw new Error.NOT_SUPPORTED ("Unable to read register “%s”", name);

			return Protocol.parse_integer_value (payload, client.byte_order);
		}

		public async void write_register (string name, uint64 val, Cancellable? cancellable = null) throws Error, IOError {
			var reg = client.get_register_by_name (name);

			var command = client.make_packet_builder_sized (48);
			command
				.append_c ('P')
				.append_register_id (reg.id)
				.append_c ('=')
				.append (Protocol.unparse_integer_value (val, reg.bitsize / 8, client.byte_order));
			if ("thread-suffix" in client.features) {
				command
					.append (";thread:")
					.append (id)
					.append_c (';');
			} else {
				yield client.execute_simple ("Hg" + id, cancellable);
			}

			yield client.execute (command.build (), cancellable);
		}
	}

	public class Exception : Object {
		public uint signum {
			get;
			construct;
		}

		public Breakpoint? breakpoint {
			get;
			construct;
		}

		public Thread thread {
			get;
			construct;
		}

		public Exception (uint signum, Breakpoint? breakpoint, Thread thread) {
			Object (
				signum: signum,
				breakpoint: breakpoint,
				thread: thread
			);
		}

		public virtual string to_string () {
			return "signum=%u".printf (signum);
		}
	}

	public sealed class Breakpoint : Object {
		public signal void removed ();

		public Kind kind {
			get;
			construct;
		}

		public uint64 address {
			get;
			construct;
		}

		public size_t size {
			get;
			construct;
		}

		public weak Client client {
			get;
			construct;
		}

		public enum Kind {
			SOFT,
			HARD,
			WRITE,
			READ,
			ACCESS;

			public static Kind from_nick (string nick) throws Error {
				return Marshal.enum_from_nick<Kind> (nick);
			}

			public string to_nick () {
				return Marshal.enum_to_nick<Kind> (this);
			}
		}

		private enum State {
			DISABLED,
			ENABLED
		}

		private State state = DISABLED;

		public Breakpoint (Kind kind, uint64 address, size_t size, Client client) {
			Object (
				kind: kind,
				address: address,
				size: size,
				client: client
			);
		}

		public async void enable (Cancellable? cancellable = null) throws Error, IOError {
			if (state != DISABLED)
				throw new Error.INVALID_OPERATION ("Already enabled");

			var command = client.make_packet_builder_sized (16)
				.append ("Z%u,".printf (kind))
				.append_address (address)
				.append_c (',')
				.append_size (size)
				.build ();

			yield client.execute (command, cancellable);

			state = ENABLED;
		}

		public async void disable (Cancellable? cancellable = null) throws Error, IOError {
			if (state != ENABLED)
				throw new Error.INVALID_OPERATION ("Already disabled");

			var command = client.make_packet_builder_sized (16)
				.append ("z%u,".printf (kind))
				.append_address (address)
				.append_c (',')
				.append_size (size)
				.build ();

			yield client.execute (command, cancellable);

			state = DISABLED;
		}

		public async void remove (Cancellable? cancellable = null) throws Error, IOError {
			if (state == ENABLED)
				yield disable (cancellable);

			removed ();
		}
	}

	namespace Protocol {
#if HAVE_FRUITY_BACKEND
		internal uint64 parse_address (string raw_val) throws Error {
			return parse_uint64 (raw_val, 16);
		}
#endif

		internal uint parse_uint (string raw_val, uint radix) throws Error {
			uint64 val;

			try {
				uint64.from_string (raw_val, out val, radix, uint.MIN, uint.MAX);
			} catch (NumberParserError e) {
				throw new Error.PROTOCOL ("Invalid response: %s", e.message);
			}

			return (uint) val;
		}

		internal uint64 parse_uint64 (string raw_val, uint radix) throws Error {
			uint64 val;

			try {
				uint64.from_string (raw_val, out val, radix);
			} catch (NumberParserError e) {
				throw new Error.PROTOCOL ("Invalid response: %s", e.message);
			}

			return val;
		}

		internal uint64 parse_pointer_value (string raw_val, uint pointer_size, ByteOrder byte_order) throws Error {
			if (raw_val.length != pointer_size * 2)
				throw new Error.PROTOCOL ("Invalid pointer value: %s", raw_val);

			return parse_integer_value (raw_val, byte_order);
		}

		internal uint64 parse_integer_value (string raw_val, ByteOrder byte_order) throws Error {
			int length = raw_val.length;
			if (length % 2 != 0)
				throw new Error.PROTOCOL ("Invalid integer value: %s", raw_val);

			int start_offset, end_offset, step;
			if (byte_order == BIG_ENDIAN) {
				start_offset = 0;
				end_offset = length;
				step = 2;
			} else {
				start_offset = length - 2;
				end_offset = -2;
				step = -2;
			}

			uint64 val = 0;

			for (int hex_offset = start_offset; hex_offset != end_offset; hex_offset += step) {
				uint8 byte = parse_hex_byte (raw_val[hex_offset + 0], raw_val[hex_offset + 1]);
				val = (val << 8) | byte;
			}

			return val;
		}

		internal string unparse_integer_value (uint64 val, size_t size, ByteOrder byte_order) {
			char * result = malloc ((size * 2) + 1);

			int start_byte_offset, end_byte_offset, byte_step;
			if (byte_order == LITTLE_ENDIAN) {
				start_byte_offset = 0;
				end_byte_offset = (int) size;
				byte_step = 1;
			} else {
				start_byte_offset = (int) size - 1;
				end_byte_offset = -1;
				byte_step = -1;
			}

			int hex_offset = 0;
			for (int byte_offset = start_byte_offset;
					byte_offset != end_byte_offset;
					byte_offset += byte_step, hex_offset += 2) {
				uint8 byte = (uint8) ((val >> (byte_offset * 8)) & 0xff);
				result[hex_offset + 0] = NIBBLE_TO_HEX_CHAR[byte >> 4];
				result[hex_offset + 1] = NIBBLE_TO_HEX_CHAR[byte & 0xf];
			}
			result[hex_offset] = '\0';

			return (string) (owned) result;
		}

		internal static string parse_hex_encoded_utf8_string (string hex_str) throws Error {
			Bytes bytes = parse_hex_bytes (hex_str);
			unowned string str = (string) bytes.get_data ();
			return str.make_valid ((ssize_t) bytes.get_size ());
		}

		internal static Bytes parse_hex_bytes (string hex_bytes) throws Error {
			int size = hex_bytes.length / 2;
			uint8[] data = new uint8[size];

			for (int byte_offset = 0, hex_offset = 0; byte_offset != size; byte_offset++, hex_offset += 2) {
				data[byte_offset] = parse_hex_byte (hex_bytes[hex_offset + 0], hex_bytes[hex_offset + 1]);
			}

			return new Bytes.take ((owned) data);
		}

		internal static string unparse_hex_bytes (Bytes bytes) throws Error {
			unowned uint8[] data = bytes.get_data ();
			uint size = data.length;

			char * result = malloc ((size * 2) + 1);
			int hex_offset = 0;
			for (int byte_offset = 0; byte_offset != size; byte_offset++, hex_offset += 2) {
				uint8 byte = data[byte_offset];
				result[hex_offset + 0] = NIBBLE_TO_HEX_CHAR[byte >> 4];
				result[hex_offset + 1] = NIBBLE_TO_HEX_CHAR[byte & 0xf];
			}
			result[hex_offset] = '\0';

			return (string) (owned) result;
		}

		internal uint8 parse_hex_byte (char upper_ch, char lower_ch) throws Error {
			int8 upper = HEX_CHAR_TO_NIBBLE[upper_ch];
			int8 lower = HEX_CHAR_TO_NIBBLE[lower_ch];
			if (upper == -1 || lower == -1)
				throw new Error.PROTOCOL ("Invalid hex byte");
			return (upper << 4) | lower;
		}

		internal const int8[] HEX_CHAR_TO_NIBBLE = {
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
		};

		internal const char[] NIBBLE_TO_HEX_CHAR = {
			'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
		};
	}
}
