[CCode (gir_namespace = "FridaJDWP", gir_version = "1.0")]
namespace Frida.JDWP {
	public sealed class Client : GLib.Object, AsyncInitable {
		public signal void closed ();
		public signal void events_received (Events events);

		public IOStream stream {
			get;
			construct;
		}

		public State state {
			get {
				return _state;
			}
		}

		private InputStream input;
		private OutputStream output;
		private Cancellable io_cancellable = new Cancellable ();

		private State _state = CREATED;
		private uint32 next_id = 1;
		private IDSizes id_sizes = new IDSizes.unknown ();
		private ReferenceTypeID java_lang_object;
		private MethodID java_lang_object_to_string;
		private Gee.ArrayQueue<Bytes> pending_writes = new Gee.ArrayQueue<Bytes> ();
		private Gee.Map<uint32, PendingReply> pending_replies = new Gee.HashMap<uint32, PendingReply> ();

		public enum State {
			CREATED,
			READY,
			CLOSED
		}

		private const uint32 MAX_PACKET_SIZE = 10 * 1024 * 1024;

		public static async Client open (IOStream stream, Cancellable? cancellable = null) throws Error, IOError {
			var session = new Client (stream);

			try {
				yield session.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return session;
		}

		private Client (IOStream stream) {
			GLib.Object (stream: stream);
		}

		construct {
			input = stream.get_input_stream ();
			output = stream.get_output_stream ();
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			yield handshake (cancellable);
			process_incoming_packets.begin ();

			id_sizes = yield get_id_sizes (cancellable);

			change_state (READY);

			var object_class = yield get_class_by_signature ("Ljava/lang/Object;", cancellable);
			java_lang_object = object_class.ref_type.id;

			var object_methods = yield get_methods (object_class.ref_type.id, cancellable);
			foreach (var method in object_methods) {
				if (method.name == "toString") {
					java_lang_object_to_string = method.id;
					break;
				}
			}

			return true;
		}

		private void change_state (State new_state) {
			bool state_differs = new_state != _state;
			if (state_differs)
				_state = new_state;

			if (state_differs)
				notify_property ("state");
		}

		public async void close (Cancellable? cancellable) throws IOError {
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

		public async void suspend (Cancellable? cancellable = null) throws Error, IOError {
			yield execute (make_command (VM, VMCommand.SUSPEND), cancellable);
		}

		public async void resume (Cancellable? cancellable = null) throws Error, IOError {
			yield execute (make_command (VM, VMCommand.RESUME), cancellable);
		}

		public async String create_string (string str, Cancellable? cancellable = null) throws Error, IOError {
			var command = make_command (VM, VMCommand.CREATE_STRING);
			command.append_utf8_string (str);

			var reply = yield execute (command, cancellable);

			return new String (reply.read_object_id ());
		}

		public async string read_string (ObjectID id, Cancellable? cancellable = null) throws Error, IOError {
			var command = make_command (STRING_REFERENCE, StringReferenceCommand.VALUE);
			command.append_object_id (id);

			var reply = yield execute (command, cancellable);

			return reply.read_utf8_string ();
		}

		public async ClassInfo get_class_by_signature (string signature, Cancellable? cancellable = null) throws Error, IOError {
			var candidates = yield get_classes_by_signature (signature, cancellable);
			if (candidates.is_empty)
				throw new Error.INVALID_ARGUMENT ("Class %s not found", signature);
			if (candidates.size > 1)
				throw new Error.INVALID_ARGUMENT ("Class %s is ambiguous", signature);
			return candidates.get (0);
		}

		public async Gee.List<ClassInfo> get_classes_by_signature (string signature, Cancellable? cancellable = null)
				throws Error, IOError {
			var command = make_command (VM, VMCommand.CLASSES_BY_SIGNATURE);
			command.append_utf8_string (signature);

			var reply = yield execute (command, cancellable);

			var result = new Gee.ArrayList<ClassInfo> ();
			int32 n = reply.read_int32 ();
			for (int32 i = 0; i != n; i++)
				result.add (ClassInfo.deserialize (reply));
			return result;
		}

		public async Gee.List<MethodInfo> get_methods (ReferenceTypeID type, Cancellable? cancellable = null)
				throws Error, IOError {
			var command = make_command (REFERENCE_TYPE, ReferenceTypeCommand.METHODS);
			command.append_reference_type_id (type);

			var reply = yield execute (command, cancellable);

			var result = new Gee.ArrayList<MethodInfo> ();
			int32 n = reply.read_int32 ();
			for (int32 i = 0; i != n; i++)
				result.add (MethodInfo.deserialize (reply));
			return result;
		}

		public async Value invoke_static_method (TaggedReferenceTypeID ref_type, ThreadID thread, MethodID method,
				Value[] arguments = {}, InvokeOptions options = 0, Cancellable? cancellable = null) throws Error, IOError {
			var command = (ref_type.tag == CLASS)
				? make_command (CLASS_TYPE, ClassTypeCommand.INVOKE_METHOD)
				: make_command (INTERFACE_TYPE, InterfaceTypeCommand.INVOKE_METHOD);
			command
				.append_reference_type_id (ref_type.id)
				.append_thread_id (thread)
				.append_method_id (method)
				.append_int32 (arguments.length);
			foreach (var arg in arguments)
				command.append_value (arg);
			command.append_int32 (options);

			var reply = yield execute (command, cancellable);

			return yield handle_invoke_reply (reply, thread, cancellable);
		}

		public async Value invoke_instance_method (ObjectID object, ThreadID thread, ReferenceTypeID clazz, MethodID method,
				Value[] arguments = {}, InvokeOptions options = 0, Cancellable? cancellable = null) throws Error, IOError {
			var command = make_command (OBJECT_REFERENCE, ObjectReferenceCommand.INVOKE_METHOD);
			command
				.append_object_id (object)
				.append_thread_id (thread)
				.append_reference_type_id (clazz)
				.append_method_id (method)
				.append_int32 (arguments.length);
			foreach (var arg in arguments)
				command.append_value (arg);
			command.append_int32 (options);

			var reply = yield execute (command, cancellable);

			return yield handle_invoke_reply (reply, thread, cancellable);
		}

		private async Value handle_invoke_reply (PacketReader reply, ThreadID thread, Cancellable? cancellable) throws Error, IOError {
			var retval = reply.read_value ();

			var exception = reply.read_tagged_object_id ();
			if (!exception.id.is_null) {
				String description = (String) yield invoke_instance_method (exception.id, thread,
					java_lang_object, java_lang_object_to_string, {}, 0, cancellable);
				string description_str = yield read_string (description.val, cancellable);
				throw new Error.PROTOCOL ("%s", description_str);
			}

			return retval;
		}

		public async EventRequestID set_event_request (EventKind kind, SuspendPolicy suspend_policy, EventModifier[] modifiers = {},
				Cancellable? cancellable = null) throws Error, IOError {
			var command = make_command (EVENT_REQUEST, EventRequestCommand.SET);
			command
				.append_uint8 (kind)
				.append_uint8 (suspend_policy)
				.append_int32 (modifiers.length);
			foreach (var modifier in modifiers)
				modifier.serialize (command);

			var reply = yield execute (command, cancellable);

			return EventRequestID (reply.read_int32 ());
		}

		public async void clear_event_request (EventKind kind, EventRequestID request_id, Cancellable? cancellable = null)
				throws Error, IOError {
			var command = make_command (EVENT_REQUEST, EventRequestCommand.CLEAR);
			command
				.append_uint8 (kind)
				.append_int32 (request_id.handle);

			yield execute (command, cancellable);
		}

		public async void clear_all_breakpoints (Cancellable? cancellable = null) throws Error, IOError {
			var command = make_command (EVENT_REQUEST, EventRequestCommand.CLEAR_ALL_BREAKPOINTS);

			yield execute (command, cancellable);
		}

		private async void handshake (Cancellable? cancellable) throws Error, IOError {
			try {
				size_t n;

				string magic = "JDWP-Handshake";

				unowned uint8[] raw_handshake = magic.data;
				yield output.write_all_async (raw_handshake, Priority.DEFAULT, cancellable, out n);

				var raw_reply = new uint8[magic.length];
				yield input.read_all_async (raw_reply, Priority.DEFAULT, cancellable, out n);

				if (Memory.cmp (raw_reply, raw_handshake, raw_reply.length) != 0)
					throw new Error.PROTOCOL ("Unexpected handshake reply");
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s".printf (e.message));
			}
		}

		private async IDSizes get_id_sizes (Cancellable? cancellable) throws Error, IOError {
			var command = make_command (VM, VMCommand.ID_SIZES);

			var reply = yield execute (command, cancellable);

			return IDSizes.deserialize (reply);
		}

		private CommandBuilder make_command (CommandSet command_set, uint8 command) {
			return new CommandBuilder (next_id++, command_set, command, id_sizes);
		}

		private async PacketReader execute (CommandBuilder command, Cancellable? cancellable) throws Error, IOError {
			if (state == CLOSED)
				throw new Error.INVALID_OPERATION ("Unable to perform command; connection is closed");

			var pending = new PendingReply (execute.callback);
			pending_replies[command.id] = pending;

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				pending.complete_with_error (new IOError.CANCELLED ("Operation was cancelled"));
				return false;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			write_bytes (command.build ());

			yield;

			cancel_source.destroy ();

			cancellable.set_error_if_cancelled ();

			var reply = pending.reply;
			if (reply == null)
				throw_local_error (pending.error);

			return reply;
		}

		private async void process_incoming_packets () {
			while (true) {
				try {
					var packet = yield read_packet ();

					dispatch_packet (packet);
				} catch (GLib.Error error) {
					change_state (CLOSED);

					foreach (var pending in pending_replies.values)
						pending.complete_with_error (error);
					pending_replies.clear ();

					closed ();

					return;
				}
			}
		}

		private async void process_pending_writes () {
			while (!pending_writes.is_empty) {
				Bytes current = pending_writes.peek_head ();

				try {
					size_t n;
					yield output.write_all_async (current.get_data (), Priority.DEFAULT, io_cancellable, out n);
				} catch (GLib.Error e) {
					return;
				}

				pending_writes.poll_head ();
			}
		}

		private void dispatch_packet (PacketReader packet) throws Error {
			packet.skip (sizeof (uint32));
			var id = packet.read_uint32 ();
			var flags = (PacketFlags) packet.read_uint8 ();

			if ((flags & PacketFlags.REPLY) != 0)
				handle_reply (packet, id);
			else
				handle_command (packet, id);
		}

		private void handle_reply (PacketReader packet, uint32 id) throws Error {
			PendingReply? pending;
			if (!pending_replies.unset (id, out pending))
				return;

			var error_code = packet.read_uint16 ();
			if (error_code == 0)
				pending.complete_with_reply (packet);
			else
				pending.complete_with_error (new Error.NOT_SUPPORTED ("Command failed: %u", error_code));
		}

		private void handle_command (PacketReader packet, uint32 id) throws Error {
			var command_set = (CommandSet) packet.read_uint8 ();
			var command = packet.read_uint8 ();
			switch (command_set) {
				case EVENT:
					handle_event ((EventCommand) command, packet);
					break;
				default:
					break;
			}
		}

		private void handle_event (EventCommand command, PacketReader packet) throws Error {
			switch (command) {
				case COMPOSITE:
					handle_event_composite (packet);
					break;
				default:
					break;
			}
		}

		private void handle_event_composite (PacketReader packet) throws Error {
			var suspend_policy = (SuspendPolicy) packet.read_uint8 ();

			var items = new Gee.ArrayList<Event> ();
			int32 n = packet.read_int32 ();
			for (int32 i = 0; i != n; i++) {
				Event? event = null;
				var kind = (EventKind) packet.read_uint8 ();
				switch (kind) {
					case SINGLE_STEP:
						event = SingleStepEvent.deserialize (packet);
						break;
					case BREAKPOINT:
						event = BreakpointEvent.deserialize (packet);
						break;
					case FRAME_POP:
						event = FramePopEvent.deserialize (packet);
						break;
					case EXCEPTION:
						event = ExceptionEvent.deserialize (packet);
						break;
					case USER_DEFINED:
						event = UserDefinedEvent.deserialize (packet);
						break;
					case THREAD_START:
						event = ThreadStartEvent.deserialize (packet);
						break;
					case THREAD_DEATH:
						event = ThreadDeathEvent.deserialize (packet);
						break;
					case CLASS_PREPARE:
						event = ClassPrepareEvent.deserialize (packet);
						break;
					case CLASS_UNLOAD:
						event = ClassUnloadEvent.deserialize (packet);
						break;
					case CLASS_LOAD:
						event = ClassLoadEvent.deserialize (packet);
						break;
					case FIELD_ACCESS:
						event = FieldAccessEvent.deserialize (packet);
						break;
					case FIELD_MODIFICATION:
						event = FieldModificationEvent.deserialize (packet);
						break;
					case EXCEPTION_CATCH:
						event = ExceptionCatchEvent.deserialize (packet);
						break;
					case METHOD_ENTRY:
						event = MethodEntryEvent.deserialize (packet);
						break;
					case METHOD_EXIT:
						event = MethodExitEvent.deserialize (packet);
						break;
					case METHOD_EXIT_WITH_RETURN_VALUE:
						event = MethodExitWithReturnValueEvent.deserialize (packet);
						break;
					case MONITOR_CONTENDED_ENTER:
						event = MonitorContendedEnterEvent.deserialize (packet);
						break;
					case MONITOR_CONTENDED_ENTERED:
						event = MonitorContendedEnteredEvent.deserialize (packet);
						break;
					case MONITOR_WAIT:
						event = MonitorWaitEvent.deserialize (packet);
						break;
					case MONITOR_WAITED:
						event = MonitorWaitedEvent.deserialize (packet);
						break;
					case VM_START:
						event = VMStartEvent.deserialize (packet);
						break;
					case VM_DEATH:
						event = VMDeathEvent.deserialize (packet);
						break;
					case VM_DISCONNECTED:
						event = VMDisconnectedEvent.deserialize (packet);
						break;
				}
				if (event != null)
					items.add (event);
			}

			events_received (new Events (suspend_policy, items));
		}

		private async PacketReader read_packet () throws Error, IOError {
			try {
				size_t n;

				int header_size = 11;
				var raw_reply = new uint8[header_size];
				yield input.read_all_async (raw_reply, Priority.DEFAULT, io_cancellable, out n);
				if (n == 0)
					throw new Error.TRANSPORT ("Connection closed unexpectedly");

				uint32 reply_size = uint32.from_big_endian (*((uint32 *) raw_reply));
				if (reply_size != raw_reply.length) {
					if (reply_size < raw_reply.length)
						throw new Error.PROTOCOL ("Invalid packet length (too small)");
					if (reply_size > MAX_PACKET_SIZE)
						throw new Error.PROTOCOL ("Invalid packet length (too large)");

					raw_reply.resize ((int) reply_size);
					yield input.read_all_async (raw_reply[header_size:], Priority.DEFAULT, io_cancellable, out n);
					if (n == 0)
						throw new Error.TRANSPORT ("Connection closed unexpectedly");
				}

				return new PacketReader ((owned) raw_reply, id_sizes);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s".printf (e.message));
			}
		}

		private void write_bytes (Bytes bytes) {
			pending_writes.offer_tail (bytes);
			if (pending_writes.size == 1)
				process_pending_writes.begin ();
		}

		private static void throw_local_error (GLib.Error e) throws Error, IOError {
			if (e is Error)
				throw (Error) e;

			if (e is IOError)
				throw (IOError) e;

			assert_not_reached ();
		}

		private class PendingReply {
			private SourceFunc? handler;

			public PacketReader? reply {
				get;
				private set;
			}

			public GLib.Error? error {
				get;
				private set;
			}

			public PendingReply (owned SourceFunc handler) {
				this.handler = (owned) handler;
			}

			public void complete_with_reply (PacketReader? reply) {
				if (handler == null)
					return;
				this.reply = reply;
				handler ();
				handler = null;
			}

			public void complete_with_error (GLib.Error error) {
				if (handler == null)
					return;
				this.error = error;
				handler ();
				handler = null;
			}
		}
	}

	public enum TypeTag {
		CLASS     = 1,
		INTERFACE = 2,
		ARRAY     = 3;

		public string to_short_string () {
			return Marshal.enum_to_nick<TypeTag> (this).up ();
		}
	}

	public enum ValueTag {
		BYTE         = 66,
		CHAR         = 67,
		DOUBLE       = 68,
		FLOAT        = 70,
		INT          = 73,
		LONG         = 74,
		OBJECT       = 76,
		SHORT        = 83,
		VOID         = 86,
		BOOLEAN      = 90,
		ARRAY        = 91,
		CLASS_OBJECT = 99,
		THREAD_GROUP = 103,
		CLASS_LOADER = 108,
		STRING       = 115,
		THREAD       = 116,
	}

	public abstract class Value : GLib.Object {
		public ValueTag tag {
			get;
			construct;
		}

		public abstract string to_string ();
	}

	public sealed class Byte : Value {
		public uint8 val {
			get;
			construct;
		}

		public Byte (uint8 val) {
			GLib.Object (tag: ValueTag.BYTE, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Char : Value {
		public string val {
			get;
			construct;
		}

		public Char (string val) {
			GLib.Object (tag: ValueTag.CHAR, val: val);
		}

		public override string to_string () {
			return val;
		}
	}

	public sealed class Double : Value {
		public double val {
			get;
			construct;
		}

		public Double (double val) {
			GLib.Object (tag: ValueTag.DOUBLE, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Float : Value {
		public float val {
			get;
			construct;
		}

		public Float (float val) {
			GLib.Object (tag: ValueTag.FLOAT, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Int : Value {
		public int32 val {
			get;
			construct;
		}

		public Int (int32 val) {
			GLib.Object (tag: ValueTag.INT, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Long : Value {
		public int64 val {
			get;
			construct;
		}

		public Long (int64 val) {
			GLib.Object (tag: ValueTag.LONG, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public class Object : Value {
		public ObjectID val {
			get;
			construct;
		}

		public Object (ObjectID val) {
			GLib.Object (tag: ValueTag.OBJECT, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Short : Value {
		public int16 val {
			get;
			construct;
		}

		public Short (int16 val) {
			GLib.Object (tag: ValueTag.SHORT, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Void : Value {
		public Void () {
			GLib.Object (tag: ValueTag.VOID);
		}

		public override string to_string () {
			return "void";
		}
	}

	public sealed class Boolean : Value {
		public bool val {
			get;
			construct;
		}

		public Boolean (bool val) {
			GLib.Object (tag: ValueTag.BOOLEAN, val: val);
		}

		public override string to_string () {
			return val.to_string ();
		}
	}

	public sealed class Array : Object {
		public Array (ObjectID val) {
			GLib.Object (tag: ValueTag.ARRAY, val: val);
		}
	}

	public sealed class ClassObject : Object {
		public ClassObject (ObjectID val) {
			GLib.Object (tag: ValueTag.CLASS_OBJECT, val: val);
		}
	}

	public sealed class ThreadGroup : Object {
		public ThreadGroup (ObjectID val) {
			GLib.Object (tag: ValueTag.THREAD_GROUP, val: val);
		}
	}

	public sealed class ClassLoader : Object {
		public ClassLoader (ObjectID val) {
			GLib.Object (tag: ValueTag.CLASS_LOADER, val: val);
		}
	}

	public sealed class String : Object {
		public String (ObjectID val) {
			GLib.Object (tag: ValueTag.STRING, val: val);
		}
	}

	public sealed class Thread : Object {
		public Thread (ObjectID val) {
			GLib.Object (tag: ValueTag.THREAD, val: val);
		}
	}

	public sealed class ClassInfo : GLib.Object {
		public TaggedReferenceTypeID ref_type {
			get;
			construct;
		}

		public ClassStatus status {
			get;
			construct;
		}

		public ClassInfo (TaggedReferenceTypeID ref_type, ClassStatus status) {
			GLib.Object (
				ref_type: ref_type,
				status: status
			);
		}

		public string to_string () {
			return "ClassInfo(ref_type: %s, status: %s)".printf (ref_type.to_string (), status.to_short_string ());
		}

		internal static ClassInfo deserialize (PacketReader packet) throws Error {
			var ref_type = packet.read_tagged_reference_type_id ();
			var status = (ClassStatus) packet.read_int32 ();
			return new ClassInfo (ref_type, status);
		}
	}

	[Flags]
	public enum ClassStatus {
		VERIFIED    = (1 << 0),
		PREPARED    = (1 << 1),
		INITIALIZED = (1 << 2),
		ERROR       = (1 << 3);

		public string to_short_string () {
			return this.to_string ().replace ("FRIDA_JDWP_CLASS_STATUS_", "");
		}
	}

	public sealed class MethodInfo : GLib.Object {
		public MethodID id {
			get;
			construct;
		}

		public string name {
			get;
			construct;
		}

		public string signature {
			get;
			construct;
		}

		public int32 mod_bits {
			get;
			construct;
		}

		public MethodInfo (MethodID id, string name, string signature, int32 mod_bits) {
			GLib.Object (
				id: id,
				name: name,
				signature: signature,
				mod_bits: mod_bits
			);
		}

		public string to_string () {
			return "MethodInfo(id: %s, name: \"%s\", signature: \"%s\", mod_bits: 0x%08x)".printf (
				id.to_string (),
				name,
				signature,
				mod_bits
			);
		}

		internal static MethodInfo deserialize (PacketReader packet) throws Error {
			var id = packet.read_method_id ();
			var name = packet.read_utf8_string ();
			var signature = packet.read_utf8_string ();
			var mod_bits = packet.read_int32 ();
			return new MethodInfo (id, name, signature, mod_bits);
		}
	}

	[Flags]
	public enum InvokeOptions {
		INVOKE_SINGLE_THREADED = 0x01,
		INVOKE_NONVIRTUAL      = 0x02,
	}

	public struct ObjectID {
		public int64 handle {
			get;
			private set;
		}

		public bool is_null {
			get {
				return handle == 0;
			}
		}

		public ObjectID (int64 handle) {
			this.handle = handle;
		}

		public string to_string () {
			return (handle != 0) ? handle.to_string () : "null";
		}
	}

	public struct TaggedObjectID {
		public TypeTag tag {
			get;
			private set;
		}

		public ObjectID id {
			get;
			private set;
		}

		public TaggedObjectID (TypeTag tag, ObjectID id) {
			this.tag = tag;
			this.id = id;
		}

		public string to_string () {
			return "TaggedObjectID(tag: %s, id: %s)".printf (tag.to_short_string (), id.to_string ());
		}
	}

	public struct ThreadID {
		public int64 handle {
			get;
			private set;
		}

		public ThreadID (int64 handle) {
			this.handle = handle;
		}

		public string to_string () {
			return handle.to_string ();
		}
	}

	public struct ReferenceTypeID {
		public int64 handle {
			get;
			private set;
		}

		public ReferenceTypeID (int64 handle) {
			this.handle = handle;
		}

		public string to_string () {
			return (handle != 0) ? handle.to_string () : "null";
		}
	}

	public struct TaggedReferenceTypeID {
		public TypeTag tag {
			get;
			private set;
		}

		public ReferenceTypeID id {
			get;
			private set;
		}

		public TaggedReferenceTypeID (TypeTag tag, ReferenceTypeID id) {
			this.tag = tag;
			this.id = id;
		}

		public string to_string () {
			return "TaggedReferenceTypeID(tag: %s, id: %s)".printf (tag.to_short_string (), id.to_string ());
		}
	}

	public struct MethodID {
		public int64 handle {
			get;
			private set;
		}

		public MethodID (int64 handle) {
			this.handle = handle;
		}

		public string to_string () {
			return handle.to_string ();
		}
	}

	public struct FieldID {
		public int64 handle {
			get;
			private set;
		}

		public FieldID (int64 handle) {
			this.handle = handle;
		}

		public string to_string () {
			return handle.to_string ();
		}
	}

	public sealed class Location : GLib.Object {
		public TaggedReferenceTypeID declaring {
			get;
			construct;
		}

		public MethodID method {
			get;
			construct;
		}

		public uint64 index {
			get;
			construct;
		}

		public Location (TaggedReferenceTypeID declaring, MethodID method, uint64 index = 0) {
			GLib.Object (
				declaring: declaring,
				method: method,
				index: index
			);
		}

		public string to_string () {
			return "Location(declaring: %s, method: %s, index: %s)".printf (
				declaring.to_string (),
				method.to_string (),
				index.to_string ()
			);
		}

		internal void serialize (PacketBuilder builder) {
			builder
				.append_tagged_reference_type_id (declaring)
				.append_method_id (method)
				.append_uint64 (index);
		}

		internal static Location deserialize (PacketReader packet) throws Error {
			var declaring = packet.read_tagged_reference_type_id ();
			var method = packet.read_method_id ();
			var index = packet.read_uint64 ();
			return new Location (declaring, method, index);
		}
	}

	public enum EventKind {
		SINGLE_STEP                   = 1,
		BREAKPOINT                    = 2,
		FRAME_POP                     = 3,
		EXCEPTION                     = 4,
		USER_DEFINED                  = 5,
		THREAD_START                  = 6,
		THREAD_DEATH                  = 7,
		CLASS_PREPARE                 = 8,
		CLASS_UNLOAD                  = 9,
		CLASS_LOAD                    = 10,
		FIELD_ACCESS                  = 20,
		FIELD_MODIFICATION            = 21,
		EXCEPTION_CATCH               = 30,
		METHOD_ENTRY                  = 40,
		METHOD_EXIT                   = 41,
		METHOD_EXIT_WITH_RETURN_VALUE = 42,
		MONITOR_CONTENDED_ENTER       = 43,
		MONITOR_CONTENDED_ENTERED     = 44,
		MONITOR_WAIT                  = 45,
		MONITOR_WAITED                = 46,
		VM_START                      = 90,
		VM_DEATH                      = 99,
		VM_DISCONNECTED               = 100,
	}

	public enum SuspendPolicy {
		NONE         = 0,
		EVENT_THREAD = 1,
		ALL          = 2,
	}

	public sealed class Events : GLib.Object {
		public SuspendPolicy suspend_policy {
			get;
			construct;
		}

		public Gee.List<Event> items {
			get;
			construct;
		}

		public Events (SuspendPolicy suspend_policy, Gee.List<Event> items) {
			GLib.Object (
				suspend_policy: suspend_policy,
				items: items
			);
		}

		public string to_string () {
			var result = new StringBuilder ("Events(\n");

			foreach (var event in items) {
				result
					.append_c ('\t')
					.append (event.to_string ())
					.append_c ('\n');
			}

			result.append_c (')');

			return result.str;
		}
	}

	public abstract class Event : GLib.Object {
		public EventKind kind {
			get;
			construct;
		}

		public EventRequestID request {
			get;
			construct;
		}

		public abstract string to_string ();
	}

	public sealed class SingleStepEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}

		public SingleStepEvent (EventRequestID request, ThreadID thread, Location location) {
			GLib.Object (
				kind: EventKind.SINGLE_STEP,
				request: request,
				thread: thread,
				location: location
			);
		}

		public override string to_string () {
			return "SingleStepEvent(request: %s, thread: %s, location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string ()
			);
		}

		internal static SingleStepEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			return new SingleStepEvent (request, thread, location);
		}
	}

	public sealed class BreakpointEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}

		public BreakpointEvent (EventRequestID request, ThreadID thread, Location location) {
			GLib.Object (
				kind: EventKind.BREAKPOINT,
				request: request,
				thread: thread,
				location: location
			);
		}

		public override string to_string () {
			return "BreakpointEvent(request: %s, thread: %s, location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string ()
			);
		}

		internal static BreakpointEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			return new BreakpointEvent (request, thread, location);
		}
	}

	public sealed class FramePopEvent : Event {
		public override string to_string () {
			return "FramePopEvent()";
		}

		internal static FramePopEvent deserialize (PacketReader packet) throws Error {
			throw new Error.NOT_SUPPORTED ("FRAME_POP event not supported");
		}
	}

	public sealed class ExceptionEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}

		public TaggedObjectID exception {
			get;
			construct;
		}

		public Location? catch_location {
			get;
			construct;
		}

		public ExceptionEvent (EventRequestID request, ThreadID thread, Location location, TaggedObjectID exception,
				Location? catch_location) {
			GLib.Object (
				kind: EventKind.EXCEPTION,
				request: request,
				thread: thread,
				location: location,
				exception: exception,
				catch_location: catch_location
			);
		}

		public override string to_string () {
			return "ExceptionEvent(request: %s, thread: %s, location: %s, exception: %s, catch_location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string (),
				exception.to_string (),
				(catch_location != null) ? catch_location.to_string () : "null"
			);
		}

		internal static ExceptionEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			var exception = packet.read_tagged_object_id ();
			var catch_location = Location.deserialize (packet);
			return new ExceptionEvent (request, thread, location, exception, catch_location);
		}
	}

	public sealed class UserDefinedEvent : Event {
		public override string to_string () {
			return "UserDefinedEvent()";
		}

		internal static UserDefinedEvent deserialize (PacketReader packet) throws Error {
			throw new Error.NOT_SUPPORTED ("USER_DEFINED event not supported");
		}
	}

	public sealed class ThreadStartEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public ThreadStartEvent (EventRequestID request, ThreadID thread) {
			GLib.Object (
				kind: EventKind.THREAD_START,
				request: request,
				thread: thread
			);
		}

		public override string to_string () {
			return "ThreadStartEvent(request: %s, thread: %s)".printf (
				request.to_string (),
				thread.to_string ()
			);
		}

		internal static ThreadStartEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			return new ThreadStartEvent (request, thread);
		}
	}

	public sealed class ThreadDeathEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public ThreadDeathEvent (EventRequestID request, ThreadID thread) {
			GLib.Object (
				kind: EventKind.THREAD_DEATH,
				request: request,
				thread: thread
			);
		}

		public override string to_string () {
			return "ThreadDeathEvent(request: %s, thread: %s)".printf (
				request.to_string (),
				thread.to_string ()
			);
		}

		internal static ThreadDeathEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			return new ThreadDeathEvent (request, thread);
		}
	}

	public sealed class ClassPrepareEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public TaggedReferenceTypeID ref_type {
			get;
			construct;
		}

		public string signature {
			get;
			construct;
		}

		public ClassStatus status {
			get;
			construct;
		}

		public ClassPrepareEvent (EventRequestID request, ThreadID thread, TaggedReferenceTypeID ref_type, string signature,
				ClassStatus status) {
			GLib.Object (
				kind: EventKind.CLASS_PREPARE,
				request: request,
				thread: thread,
				ref_type: ref_type,
				signature: signature,
				status: status
			);
		}

		public override string to_string () {
			return "ClassPrepareEvent(request: %s, thread: %s, ref_type: %s, signature: \"%s\", status: %s)".printf (
				request.to_string (),
				thread.to_string (),
				ref_type.to_string (),
				signature,
				status.to_short_string ()
			);
		}

		internal static ClassPrepareEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var ref_type = packet.read_tagged_reference_type_id ();
			var signature = packet.read_utf8_string ();
			var status = (ClassStatus) packet.read_int32 ();
			return new ClassPrepareEvent (request, thread, ref_type, signature, status);
		}
	}

	public sealed class ClassUnloadEvent : Event {
		public string signature {
			get;
			construct;
		}

		public ClassUnloadEvent (EventRequestID request, string signature) {
			GLib.Object (
				kind: EventKind.CLASS_UNLOAD,
				request: request,
				signature: signature
			);
		}

		public override string to_string () {
			return "ClassUnloadEvent(request: %s, signature: \"%s\")".printf (request.to_string (), signature);
		}

		internal static ClassUnloadEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var signature = packet.read_utf8_string ();
			return new ClassUnloadEvent (request, signature);
		}
	}

	public sealed class ClassLoadEvent : Event {
		public override string to_string () {
			return "ClassLoadEvent()";
		}

		internal static ClassLoadEvent deserialize (PacketReader packet) throws Error {
			throw new Error.NOT_SUPPORTED ("CLASS_LOAD event not supported");
		}
	}

	public abstract class FieldEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}

		public TaggedReferenceTypeID ref_type {
			get;
			construct;
		}

		public FieldID field {
			get;
			construct;
		}

		public TaggedObjectID object {
			get;
			construct;
		}
	}

	public sealed class FieldAccessEvent : FieldEvent {
		public FieldAccessEvent (EventRequestID request, ThreadID thread, Location location, TaggedReferenceTypeID ref_type,
				FieldID field, TaggedObjectID object) {
			GLib.Object (
				kind: EventKind.FIELD_ACCESS,
				request: request,
				thread: thread,
				location: location,
				ref_type: ref_type,
				field: field,
				object: object
			);
		}

		public override string to_string () {
			return "FieldAccessEvent(request: %s, thread: %s, location: %s, ref_type: %s, field: %s, object: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string (),
				ref_type.to_string (),
				field.to_string (),
				object.to_string ()
			);
		}

		internal static FieldAccessEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			var ref_type = packet.read_tagged_reference_type_id ();
			var field = packet.read_field_id ();
			var object = packet.read_tagged_object_id ();
			return new FieldAccessEvent (request, thread, location, ref_type, field, object);
		}
	}

	public sealed class FieldModificationEvent : FieldEvent {
		public Value value_to_be {
			get;
			construct;
		}

		public FieldModificationEvent (EventRequestID request, ThreadID thread, Location location, TaggedReferenceTypeID ref_type,
				FieldID field, TaggedObjectID object, Value value_to_be) {
			GLib.Object (
				kind: EventKind.FIELD_MODIFICATION,
				request: request,
				thread: thread,
				location: location,
				ref_type: ref_type,
				field: field,
				object: object,
				value_to_be: value_to_be
			);
		}

		public override string to_string () {
			return ("FieldModificationEvent(request: %s, thread: %s, location: %s, ref_type: %s, field: %s, object: %s, " +
					"value_to_be: %s)").printf (
				request.to_string (),
				thread.to_string (),
				location.to_string (),
				ref_type.to_string (),
				field.to_string (),
				object.to_string (),
				value_to_be.to_string ()
			);
		}

		internal static FieldModificationEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			var ref_type = packet.read_tagged_reference_type_id ();
			var field = packet.read_field_id ();
			var object = packet.read_tagged_object_id ();
			var value_to_be = packet.read_value ();
			return new FieldModificationEvent (request, thread, location, ref_type, field, object, value_to_be);
		}
	}

	public sealed class ExceptionCatchEvent : Event {
		public override string to_string () {
			return "ExceptionCatchEvent()";
		}

		internal static ExceptionCatchEvent deserialize (PacketReader packet) throws Error {
			throw new Error.NOT_SUPPORTED ("EXCEPTION_CATCH event not supported");
		}
	}

	public abstract class MethodEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}
	}

	public sealed class MethodEntryEvent : MethodEvent {
		public MethodEntryEvent (EventRequestID request, ThreadID thread, Location location) {
			GLib.Object (
				kind: EventKind.METHOD_ENTRY,
				request: request,
				thread: thread,
				location: location
			);
		}

		public override string to_string () {
			return "MethodEntryEvent(request: %s, thread: %s, location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string ()
			);
		}

		internal static MethodEntryEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			return new MethodEntryEvent (request, thread, location);
		}
	}

	public sealed class MethodExitEvent : MethodEvent {
		public MethodExitEvent (EventRequestID request, ThreadID thread, Location location) {
			GLib.Object (
				kind: EventKind.METHOD_EXIT,
				request: request,
				thread: thread,
				location: location
			);
		}

		public override string to_string () {
			return "MethodExitEvent(request: %s, thread: %s, location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string ()
			);
		}

		internal static MethodExitEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			return new MethodExitEvent (request, thread, location);
		}
	}

	public sealed class MethodExitWithReturnValueEvent : MethodEvent {
		public Value retval {
			get;
			construct;
		}

		public MethodExitWithReturnValueEvent (EventRequestID request, ThreadID thread, Location location, Value retval) {
			GLib.Object (
				kind: EventKind.METHOD_EXIT_WITH_RETURN_VALUE,
				request: request,
				thread: thread,
				location: location,
				retval: retval
			);
		}

		public override string to_string () {
			return "MethodExitWithReturnValueEvent(request: %s, thread: %s, location: %s, retval: %s)".printf (
				request.to_string (),
				thread.to_string (),
				location.to_string (),
				retval.to_string ()
			);
		}

		internal static MethodExitWithReturnValueEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var location = Location.deserialize (packet);
			var retval = packet.read_value ();
			return new MethodExitWithReturnValueEvent (request, thread, location, retval);
		}
	}

	public abstract class MonitorEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public TaggedObjectID object {
			get;
			construct;
		}

		public Location location {
			get;
			construct;
		}
	}

	public sealed class MonitorContendedEnterEvent : MonitorEvent {
		public MonitorContendedEnterEvent (EventRequestID request, ThreadID thread, TaggedObjectID object, Location location) {
			GLib.Object (
				kind: EventKind.MONITOR_CONTENDED_ENTER,
				request: request,
				thread: thread,
				object: object,
				location: location
			);
		}

		public override string to_string () {
			return "MonitorContendedEnterEvent(request: %s, thread: %s, object: %s, location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				object.to_string (),
				location.to_string ()
			);
		}

		internal static MonitorContendedEnterEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var object = packet.read_tagged_object_id ();
			var location = Location.deserialize (packet);
			return new MonitorContendedEnterEvent (request, thread, object, location);
		}
	}

	public sealed class MonitorContendedEnteredEvent : MonitorEvent {
		public MonitorContendedEnteredEvent (EventRequestID request, ThreadID thread, TaggedObjectID object, Location location) {
			GLib.Object (
				kind: EventKind.MONITOR_CONTENDED_ENTERED,
				request: request,
				thread: thread,
				object: object,
				location: location
			);
		}

		public override string to_string () {
			return "MonitorContendedEnteredEvent(request: %s, thread: %s, object: %s, location: %s)".printf (
				request.to_string (),
				thread.to_string (),
				object.to_string (),
				location.to_string ()
			);
		}

		internal static MonitorContendedEnteredEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var object = packet.read_tagged_object_id ();
			var location = Location.deserialize (packet);
			return new MonitorContendedEnteredEvent (request, thread, object, location);
		}
	}

	public sealed class MonitorWaitEvent : MonitorEvent {
		public int64 timeout {
			get;
			construct;
		}

		public MonitorWaitEvent (EventRequestID request, ThreadID thread, TaggedObjectID object, Location location, int64 timeout) {
			GLib.Object (
				kind: EventKind.MONITOR_CONTENDED_ENTER,
				request: request,
				thread: thread,
				object: object,
				location: location,
				timeout: timeout
			);
		}

		public override string to_string () {
			return ("MonitorWaitEvent(request: %s, thread: %s, object: %s, location: %s, timeout=%s)").printf (
				request.to_string (),
				thread.to_string (),
				object.to_string (),
				location.to_string (),
				timeout.to_string ()
			);
		}

		internal static MonitorWaitEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var object = packet.read_tagged_object_id ();
			var location = Location.deserialize (packet);
			var timeout = packet.read_int64 ();
			return new MonitorWaitEvent (request, thread, object, location, timeout);
		}
	}

	public sealed class MonitorWaitedEvent : MonitorEvent {
		public bool timed_out {
			get;
			construct;
		}

		public MonitorWaitedEvent (EventRequestID request, ThreadID thread, TaggedObjectID object, Location location,
				bool timed_out) {
			GLib.Object (
				kind: EventKind.MONITOR_CONTENDED_ENTER,
				request: request,
				thread: thread,
				object: object,
				location: location,
				timed_out: timed_out
			);
		}

		public override string to_string () {
			return ("MonitorWaitedEvent(request: %s, thread: %s, object: %s, location: %s, timed_out=%s)").printf (
				request.to_string (),
				thread.to_string (),
				object.to_string (),
				location.to_string (),
				timed_out.to_string ()
			);
		}

		internal static MonitorWaitedEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			var object = packet.read_tagged_object_id ();
			var location = Location.deserialize (packet);
			var timed_out = packet.read_boolean ();
			return new MonitorWaitedEvent (request, thread, object, location, timed_out);
		}
	}

	public sealed class VMStartEvent : Event {
		public ThreadID thread {
			get;
			construct;
		}

		public VMStartEvent (EventRequestID request, ThreadID thread) {
			GLib.Object (
				kind: EventKind.VM_START,
				request: request,
				thread: thread
			);
		}

		public override string to_string () {
			return "VMStartEvent(request: %s, thread: %s)".printf (request.to_string (), thread.to_string ());
		}

		internal static VMStartEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			var thread = packet.read_thread_id ();
			return new VMStartEvent (request, thread);
		}
	}

	public sealed class VMDeathEvent : Event {
		public VMDeathEvent (EventRequestID request) {
			GLib.Object (kind: EventKind.VM_DEATH, request: request);
		}

		public override string to_string () {
			return "VMDeathEvent(request: %s)".printf (request.to_string ());
		}

		internal static VMDeathEvent deserialize (PacketReader packet) throws Error {
			var request = EventRequestID (packet.read_int32 ());
			return new VMDeathEvent (request);
		}
	}

	public sealed class VMDisconnectedEvent : Event {
		public override string to_string () {
			return "VMDisconnectedEvent()";
		}

		internal static VMDisconnectedEvent deserialize (PacketReader packet) throws Error {
			throw new Error.NOT_SUPPORTED ("VM_DISCONNECTED event not supported");
		}
	}

	public abstract class EventModifier : GLib.Object {
		internal abstract void serialize (PacketBuilder builder);
	}

	public sealed class CountModifier : EventModifier {
		public int32 count {
			get;
			construct;
		}

		public CountModifier (int32 count) {
			GLib.Object (count: count);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.COUNT)
				.append_int32 (count);
		}
	}

	public sealed class ThreadOnlyModifier : EventModifier {
		public ThreadID thread {
			get;
			construct;
		}

		public ThreadOnlyModifier (ThreadID thread) {
			GLib.Object (thread: thread);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.THREAD_ONLY)
				.append_thread_id (thread);
		}
	}

	public sealed class ClassOnlyModifier : EventModifier {
		public ReferenceTypeID clazz {
			get;
			construct;
		}

		public ClassOnlyModifier (ReferenceTypeID clazz) {
			GLib.Object (clazz: clazz);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.CLASS_ONLY)
				.append_reference_type_id (clazz);
		}
	}

	public sealed class ClassMatchModifier : EventModifier {
		public string class_pattern {
			get;
			construct;
		}

		public ClassMatchModifier (string class_pattern) {
			GLib.Object (class_pattern: class_pattern);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.CLASS_MATCH)
				.append_utf8_string (class_pattern);
		}
	}

	public sealed class ClassExcludeModifier : EventModifier {
		public string class_pattern {
			get;
			construct;
		}

		public ClassExcludeModifier (string class_pattern) {
			GLib.Object (class_pattern: class_pattern);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.CLASS_EXCLUDE)
				.append_utf8_string (class_pattern);
		}
	}

	public sealed class LocationOnlyModifier : EventModifier {
		public Location location {
			get;
			construct;
		}

		public LocationOnlyModifier (TaggedReferenceTypeID declaring, MethodID method, uint64 index = 0) {
			GLib.Object (location: new Location (declaring, method, index));
		}

		internal override void serialize (PacketBuilder builder) {
			builder.append_uint8 (EventModifierKind.LOCATION_ONLY);
			location.serialize (builder);
		}
	}

	public sealed class ExceptionOnlyModifier : EventModifier {
		public ReferenceTypeID exception_or_null {
			get;
			construct;
		}

		public bool caught {
			get;
			construct;
		}

		public bool uncaught {
			get;
			construct;
		}

		public ExceptionOnlyModifier (ReferenceTypeID exception_or_null, bool caught, bool uncaught) {
			GLib.Object (
				exception_or_null: exception_or_null,
				caught: caught,
				uncaught: uncaught
			);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.EXCEPTION_ONLY)
				.append_reference_type_id (exception_or_null)
				.append_boolean (caught)
				.append_boolean (uncaught);
		}
	}

	public sealed class FieldOnlyModifier : EventModifier {
		public ReferenceTypeID declaring {
			get;
			construct;
		}

		public FieldID field {
			get;
			construct;
		}

		public FieldOnlyModifier (ReferenceTypeID declaring, FieldID field) {
			GLib.Object (
				declaring: declaring,
				field: field
			);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.FIELD_ONLY)
				.append_reference_type_id (declaring)
				.append_field_id (field);
		}
	}

	public sealed class StepModifier : EventModifier {
		public ThreadID thread {
			get;
			construct;
		}

		public StepSize step_size {
			get;
			construct;
		}

		public StepDepth step_depth {
			get;
			construct;
		}

		public StepModifier (ThreadID thread, StepSize step_size, StepDepth step_depth) {
			GLib.Object (
				thread: thread,
				step_size: step_size,
				step_depth: step_depth
			);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.STEP)
				.append_thread_id (thread)
				.append_int32 (step_size)
				.append_int32 (step_depth);
		}
	}

	public enum StepSize {
		MIN  = 0,
		LINE = 1,
	}

	public enum StepDepth {
		INTO = 0,
		OVER = 1,
		OUT  = 2,
	}

	public sealed class InstanceOnlyModifier : EventModifier {
		public ObjectID instance {
			get;
			construct;
		}

		public InstanceOnlyModifier (ObjectID instance) {
			GLib.Object (instance: instance);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.INSTANCE_ONLY)
				.append_object_id (instance);
		}
	}

	public sealed class SourceNameMatchModifier : EventModifier {
		public string source_name_pattern {
			get;
			construct;
		}

		public SourceNameMatchModifier (string source_name_pattern) {
			GLib.Object (source_name_pattern: source_name_pattern);
		}

		internal override void serialize (PacketBuilder builder) {
			builder
				.append_uint8 (EventModifierKind.SOURCE_NAME_MATCH)
				.append_utf8_string (source_name_pattern);
		}
	}

	private enum EventModifierKind {
		COUNT             = 1,
		THREAD_ONLY       = 3,
		CLASS_ONLY        = 4,
		CLASS_MATCH       = 5,
		CLASS_EXCLUDE     = 6,
		LOCATION_ONLY     = 7,
		EXCEPTION_ONLY    = 8,
		FIELD_ONLY        = 9,
		STEP              = 10,
		INSTANCE_ONLY     = 11,
		SOURCE_NAME_MATCH = 12,
	}

	public struct EventRequestID {
		public int32 handle {
			get;
			private set;
		}

		public EventRequestID (int32 handle) {
			this.handle = handle;
		}

		public string to_string () {
			return handle.to_string ();
		}
	}

	private enum CommandSet {
		VM               = 1,
		REFERENCE_TYPE   = 2,
		CLASS_TYPE       = 3,
		INTERFACE_TYPE   = 5,
		OBJECT_REFERENCE = 9,
		STRING_REFERENCE = 10,
		EVENT_REQUEST    = 15,
		EVENT            = 64,
	}

	private enum VMCommand {
		CLASSES_BY_SIGNATURE = 2,
		ID_SIZES             = 7,
		SUSPEND              = 8,
		RESUME               = 9,
		CREATE_STRING        = 11,
	}

	private enum ReferenceTypeCommand {
		METHODS = 5,
	}

	private enum ClassTypeCommand {
		INVOKE_METHOD = 3,
	}

	private enum InterfaceTypeCommand {
		INVOKE_METHOD = 1,
	}

	private enum ObjectReferenceCommand {
		INVOKE_METHOD = 6,
	}

	private enum StringReferenceCommand {
		VALUE = 1,
	}

	private enum EventRequestCommand {
		SET                   = 1,
		CLEAR                 = 2,
		CLEAR_ALL_BREAKPOINTS = 3,
	}

	private enum EventCommand {
		COMPOSITE = 100,
	}

	[Flags]
	private enum PacketFlags {
		REPLY = (1 << 7),
	}

	private sealed class CommandBuilder : PacketBuilder {
		public CommandBuilder (uint32 id, CommandSet command_set, uint8 command, IDSizes id_sizes) {
			base (id, 0, id_sizes);

			append_uint8 (command_set);
			append_uint8 (command);
		}
	}

	private class PacketBuilder {
		public uint32 id {
			get;
			private set;
		}

		public size_t offset {
			get {
				return cursor;
			}
		}

		private ByteArray buffer = new ByteArray.sized (64);
		private size_t cursor = 0;

		private IDSizes id_sizes;

		public PacketBuilder (uint32 id, uint8 flags, IDSizes id_sizes) {
			this.id = id;
			this.id_sizes = id_sizes;

			uint32 length_placeholder = 0;
			append_uint32 (length_placeholder);
			append_uint32 (id);
			append_uint8 (flags);
		}

		public unowned PacketBuilder append_uint8 (uint8 val) {
			*(get_pointer (cursor, sizeof (uint8))) = val;
			cursor += (uint) sizeof (uint8);
			return this;
		}

		public unowned PacketBuilder append_int16 (int16 val) {
			*(get_pointer (cursor, sizeof (int16))) = val.to_big_endian ();
			cursor += (uint) sizeof (int16);
			return this;
		}

		public unowned PacketBuilder append_uint16 (uint16 val) {
			*(get_pointer (cursor, sizeof (uint16))) = val.to_big_endian ();
			cursor += (uint) sizeof (uint16);
			return this;
		}

		public unowned PacketBuilder append_int32 (int32 val) {
			*((int32 *) get_pointer (cursor, sizeof (int32))) = val.to_big_endian ();
			cursor += (uint) sizeof (int32);
			return this;
		}

		public unowned PacketBuilder append_uint32 (uint32 val) {
			*((uint32 *) get_pointer (cursor, sizeof (uint32))) = val.to_big_endian ();
			cursor += (uint) sizeof (uint32);
			return this;
		}

		public unowned PacketBuilder append_int64 (int64 val) {
			*((int64 *) get_pointer (cursor, sizeof (int64))) = val.to_big_endian ();
			cursor += (uint) sizeof (int64);
			return this;
		}

		public unowned PacketBuilder append_uint64 (uint64 val) {
			*((uint64 *) get_pointer (cursor, sizeof (uint64))) = val.to_big_endian ();
			cursor += (uint) sizeof (uint64);
			return this;
		}

		public unowned PacketBuilder append_double (double val) {
			var bits = (uint64 *) &val;
			return append_uint64 (*bits);
		}

		public unowned PacketBuilder append_float (float val) {
			var bits = (uint32 *) &val;
			return append_uint32 (*bits);
		}

		public unowned PacketBuilder append_boolean (bool val) {
			return append_uint8 ((uint8) val);
		}

		public unowned PacketBuilder append_utf8_string (string str) {
			append_uint32 (str.length);

			uint size = str.length;
			Memory.copy (get_pointer (cursor, size), str, size);
			cursor += size;

			return this;
		}

		public unowned PacketBuilder append_value (Value v) {
			append_uint8 (v.tag);

			switch (v.tag) {
				case BYTE:
					append_uint8 (((Byte) v).val);
					break;
				case CHAR: {
					string16 s;
					try {
						s = ((Char) v).val.to_utf16 ();
					} catch (ConvertError e) {
						assert_not_reached ();
					}
					var c = (uint16 *) s;
					append_uint16 (*c);
					break;
				}
				case DOUBLE:
					append_double (((Double) v).val);
					break;
				case FLOAT:
					append_float (((Float) v).val);
					break;
				case INT:
					append_int32 (((Int) v).val);
					break;
				case LONG:
					append_int64 (((Long) v).val);
					break;
				case OBJECT:
				case ARRAY:
				case CLASS_OBJECT:
				case THREAD_GROUP:
				case CLASS_LOADER:
				case STRING:
				case THREAD:
					append_object_id (((Object) v).val);
					break;
				case SHORT:
					append_int16 (((Short) v).val);
					break;
				case VOID:
					break;
				case BOOLEAN:
					append_boolean (((Boolean) v).val);
					break;
			}

			return this;
		}

		public unowned PacketBuilder append_object_id (ObjectID object) {
			return append_handle (object.handle, id_sizes.get_object_id_size_or_die ());
		}

		public unowned PacketBuilder append_thread_id (ThreadID thread) {
			return append_handle (thread.handle, id_sizes.get_object_id_size_or_die ());
		}

		public unowned PacketBuilder append_reference_type_id (ReferenceTypeID type) {
			return append_handle (type.handle, id_sizes.get_reference_type_id_size_or_die ());
		}

		public unowned PacketBuilder append_tagged_reference_type_id (TaggedReferenceTypeID ref_type) {
			return this
				.append_uint8 (ref_type.tag)
				.append_reference_type_id (ref_type.id);
		}

		public unowned PacketBuilder append_method_id (MethodID method) {
			return append_handle (method.handle, id_sizes.get_method_id_size_or_die ());
		}

		public unowned PacketBuilder append_field_id (FieldID field) {
			return append_handle (field.handle, id_sizes.get_field_id_size_or_die ());
		}

		private unowned PacketBuilder append_handle (int64 val, size_t size) {
			switch (size) {
				case 4:
					return append_int32 ((int32) val);
				case 8:
					return append_int64 (val);
				default:
					assert_not_reached ();
			}
		}

		private uint8 * get_pointer (size_t offset, size_t n) {
			size_t minimum_size = offset + n;
			if (buffer.len < minimum_size)
				buffer.set_size ((uint) minimum_size);

			return (uint8 *) buffer.data + offset;
		}

		public Bytes build () {
			*((uint32 *) get_pointer (0, sizeof (uint32))) = buffer.len.to_big_endian ();
			return ByteArray.free_to_bytes ((owned) buffer);
		}
	}

	private sealed class PacketReader {
		public size_t available_bytes {
			get {
				return end - cursor;
			}
		}

		private uint8[] data;
		private uint8 * cursor;
		private uint8 * end;

		private IDSizes id_sizes;

		public PacketReader (owned uint8[] data, IDSizes id_sizes) {
			this.data = (owned) data;
			this.cursor = (uint8 *) this.data;
			this.end = cursor + this.data.length;

			this.id_sizes = id_sizes;
		}

		public void skip (size_t n) throws Error {
			check_available (n);
			cursor += n;
		}

		public uint8 read_uint8 () throws Error {
			const size_t n = sizeof (uint8);
			check_available (n);

			uint8 val = *cursor;
			cursor += n;

			return val;
		}

		public int16 read_int16 () throws Error {
			const size_t n = sizeof (int16);
			check_available (n);

			int16 val = int16.from_big_endian (*((int16 *) cursor));
			cursor += n;

			return val;
		}

		public uint16 read_uint16 () throws Error {
			const size_t n = sizeof (uint16);
			check_available (n);

			uint16 val = uint16.from_big_endian (*((uint16 *) cursor));
			cursor += n;

			return val;
		}

		public int32 read_int32 () throws Error {
			const size_t n = sizeof (int32);
			check_available (n);

			int32 val = int32.from_big_endian (*((int32 *) cursor));
			cursor += n;

			return val;
		}

		public uint32 read_uint32 () throws Error {
			const size_t n = sizeof (uint32);
			check_available (n);

			uint32 val = uint32.from_big_endian (*((uint32 *) cursor));
			cursor += n;

			return val;
		}

		public int64 read_int64 () throws Error {
			const size_t n = sizeof (int64);
			check_available (n);

			int64 val = int64.from_big_endian (*((int64 *) cursor));
			cursor += n;

			return val;
		}

		public uint64 read_uint64 () throws Error {
			const size_t n = sizeof (uint64);
			check_available (n);

			uint64 val = uint64.from_big_endian (*((uint64 *) cursor));
			cursor += n;

			return val;
		}

		public double read_double () throws Error {
			var bits = read_uint64 ();
			var val = (double *) &bits;
			return *val;
		}

		public float read_float () throws Error {
			var bits = read_uint32 ();
			var val = (float *) &bits;
			return *val;
		}

		public bool read_boolean () throws Error {
			return (bool) read_uint8 ();
		}

		public string read_utf8_string () throws Error {
			size_t size = read_uint32 ();
			check_available (size);

			unowned string data = (string) cursor;
			string str = data.substring (0, (long) size);
			cursor += size;

			return str;
		}

		public Value read_value () throws Error {
			var tag = (ValueTag) read_uint8 ();

			switch (tag) {
				case BYTE:
					return new Byte (read_uint8 ());
				case CHAR: {
					uint16 c = read_uint16 ();
					var s = (string16 *) &c;
					try {
						return new Char (s->to_utf8 (1));
					} catch (ConvertError e) {
						throw new Error.PROTOCOL ("%s", e.message);
					}
				}
				case DOUBLE:
					return new Double (read_double ());
				case FLOAT:
					return new Float (read_float ());
				case INT:
					return new Int (read_int32 ());
				case LONG:
					return new Long (read_int64 ());
				case OBJECT:
					return new Object (read_object_id ());
				case SHORT:
					return new Short (read_int16 ());
				case VOID:
					return new Void ();
				case BOOLEAN:
					return new Boolean (read_boolean ());
				case ARRAY:
					return new Array (read_object_id ());
				case CLASS_OBJECT:
					return new ClassObject (read_object_id ());
				case THREAD_GROUP:
					return new ThreadGroup (read_object_id ());
				case CLASS_LOADER:
					return new ClassLoader (read_object_id ());
				case STRING:
					return new String (read_object_id ());
				case THREAD:
					return new Thread (read_object_id ());
			}

			throw new Error.PROTOCOL ("Unexpected value tag");
		}

		public ObjectID read_object_id () throws Error {
			return ObjectID (read_handle (id_sizes.get_object_id_size ()));
		}

		public TaggedObjectID read_tagged_object_id () throws Error {
			var tag = (TypeTag) read_uint8 ();
			var id = read_object_id ();
			return TaggedObjectID (tag, id);
		}

		public ThreadID read_thread_id () throws Error {
			return ThreadID (read_handle (id_sizes.get_object_id_size ()));
		}

		public ReferenceTypeID read_reference_type_id () throws Error {
			return ReferenceTypeID (read_handle (id_sizes.get_reference_type_id_size ()));
		}

		public TaggedReferenceTypeID read_tagged_reference_type_id () throws Error {
			var tag = (TypeTag) read_uint8 ();
			var id = read_reference_type_id ();
			return TaggedReferenceTypeID (tag, id);
		}

		public MethodID read_method_id () throws Error {
			return MethodID (read_handle (id_sizes.get_method_id_size ()));
		}

		public FieldID read_field_id () throws Error {
			return FieldID (read_handle (id_sizes.get_field_id_size ()));
		}

		private int64 read_handle (size_t size) throws Error {
			switch (size) {
				case 4:
					return read_int32 ();
				case 8:
					return read_int64 ();
				default:
					assert_not_reached ();
			}
		}

		private void check_available (size_t n) throws Error {
			if (cursor + n > end)
				throw new Error.PROTOCOL ("Invalid JDWP packet");
		}
	}

	private sealed class IDSizes {
		private bool valid;
		private int field_id_size = -1;
		private int method_id_size = -1;
		private int object_id_size = -1;
		private int reference_type_id_size = -1;
		private int frame_id_size = -1;

		public IDSizes (int field_id_size, int method_id_size, int object_id_size, int reference_type_id_size, int frame_id_size) {
			this.field_id_size = field_id_size;
			this.method_id_size = method_id_size;
			this.object_id_size = object_id_size;
			this.reference_type_id_size = reference_type_id_size;
			this.frame_id_size = frame_id_size;

			valid = true;
		}

		public IDSizes.unknown () {
			valid = false;
		}

		public size_t get_field_id_size () throws Error {
			check ();
			return field_id_size;
		}

		public size_t get_field_id_size_or_die () {
			assert (valid);
			return field_id_size;
		}

		public size_t get_method_id_size () throws Error {
			check ();
			return method_id_size;
		}

		public size_t get_method_id_size_or_die () {
			assert (valid);
			return method_id_size;
		}

		public size_t get_object_id_size () throws Error {
			check ();
			return object_id_size;
		}

		public size_t get_object_id_size_or_die () {
			assert (valid);
			return object_id_size;
		}

		public size_t get_reference_type_id_size () throws Error {
			check ();
			return reference_type_id_size;
		}

		public size_t get_reference_type_id_size_or_die () {
			assert (valid);
			return reference_type_id_size;
		}

		private void check () throws Error {
			if (!valid)
				throw new Error.PROTOCOL ("ID sizes not known");
		}

		internal static IDSizes deserialize (PacketReader packet) throws Error {
			var field_id_size = packet.read_int32 ();
			var method_id_size = packet.read_int32 ();
			var object_id_size = packet.read_int32 ();
			var reference_type_id_size = packet.read_int32 ();
			var frame_id_size = packet.read_int32 ();
			return new IDSizes (field_id_size, method_id_size, object_id_size, reference_type_id_size, frame_id_size);
		}
	}
}
