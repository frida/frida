namespace Frida {
	public sealed class XpcClient : Object {
		public signal void message (Darwin.Xpc.Object obj);

		public State state {
			get {
				return _state;
			}
		}

		public Darwin.Xpc.Connection connection {
			get;
			construct;
		}

		public Darwin.GCD.DispatchQueue queue {
			get;
			construct;
		}

		private State _state;
		private string? close_reason;
		private MainContext main_context;

		public enum State {
			OPEN,
			CLOSING,
			CLOSED,
		}

		public static XpcClient make_for_mach_service (string name, Darwin.GCD.DispatchQueue queue) {
			return new XpcClient (Darwin.Xpc.Connection.create_mach_service (name, queue, 0), queue);
		}

		public XpcClient (Darwin.Xpc.Connection connection, Darwin.GCD.DispatchQueue queue) {
			Object (connection: connection, queue: queue);
		}

		construct {
			main_context = MainContext.ref_thread_default ();

			connection.set_event_handler (on_event);
			connection.activate ();
		}

		public override void dispose () {
			if (close_reason != null) {
				change_state (CLOSED);
			} else {
				change_state (CLOSING);
				this.ref ();
				connection.cancel ();
			}

			base.dispose ();
		}

		public async Darwin.Xpc.Object request (Darwin.Xpc.Object message, Cancellable? cancellable) throws Error, IOError {
			Darwin.Xpc.Object? reply = null;
			connection.send_message_with_reply (message, queue, r => {
				schedule_on_frida_thread (() => {
					reply = r;
					request.callback ();
					return Source.REMOVE;
				});
			});

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				connection.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (main_context);

			yield;

			cancel_source.destroy ();

			if (reply.type == Darwin.Xpc.Error.TYPE) {
				var e = (Darwin.Xpc.Error) reply;
				throw new Error.NOT_SUPPORTED ("%s", e.get_string (Darwin.Xpc.Error.KEY_DESCRIPTION));
			}

			return reply;
		}

		public void post (Darwin.Xpc.Object message) throws Error, IOError {
			connection.send_message (message);
		}

		private void change_state (State new_state) {
			_state = new_state;
			notify_property ("state");
		}

		private void on_event (Darwin.Xpc.Object obj) {
			schedule_on_frida_thread (() => {
				if (obj.type == Darwin.Xpc.Error.TYPE) {
					var e = (Darwin.Xpc.Error) obj;
					close_reason = e.get_string (Darwin.Xpc.Error.KEY_DESCRIPTION);
					switch (state) {
						case OPEN:
							change_state (CLOSED);
							break;
						case CLOSING:
							change_state (CLOSED);
							unref ();
							break;
						case CLOSED:
							assert_not_reached ();
					}
				} else {
					message (obj);
				}
				return Source.REMOVE;
			});
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (main_context);
		}
	}

	public sealed class XpcObjectReader {
		public Darwin.Xpc.Object root_object {
			get {
				return scopes.peek_head ().object;
			}
		}

		public Darwin.Xpc.Object current_object {
			get {
				return scopes.peek_tail ().object;
			}
		}

		public delegate int TranslateErrorFunc (string domain, int code, string description);

		private Gee.Deque<Scope> scopes = new Gee.ArrayQueue<Scope> ();

		public XpcObjectReader (Darwin.Xpc.Object obj) {
			push_scope (obj);
		}

		public bool has_member (string name) throws Error {
			return peek_scope ().get_dictionary ().get_value (name) != null;
		}

		public bool try_read_member (string name) throws Error {
			var scope = peek_scope ();
			var dict = scope.get_dictionary ();
			var val = dict.get_value (name);
			if (val == null)
				return false;

			push_scope (val);

			return true;
		}

		public unowned XpcObjectReader read_member (string name) throws Error {
			var scope = peek_scope ();
			var dict = scope.get_dictionary ();
			var val = dict.get_value (name);
			if (val == null)
				throw new Error.PROTOCOL ("Key '%s' not found in dictionary: %s", name, scope.object.to_string ());

			push_scope (val);

			return this;
		}

		public unowned XpcObjectReader end_member () {
			pop_scope ();

			return this;
		}

		public size_t count_elements () throws Error {
			return peek_scope ().get_array ().count;
		}

		public unowned XpcObjectReader read_element (size_t index) throws Error {
			push_scope (peek_scope ().get_array ().get_value (index));

			return this;
		}

		public unowned XpcObjectReader end_element () throws Error {
			pop_scope ();

			return this;
		}

		public bool get_bool_value () throws Error {
			return peek_scope ().get_object<Darwin.Xpc.Bool> (Darwin.Xpc.Bool.TYPE).get_value ();
		}

		public int64 get_int64_value () throws Error {
			return peek_scope ().get_object<Darwin.Xpc.Int64> (Darwin.Xpc.Int64.TYPE).get_value ();
		}

		public uint64 get_uint64_value () throws Error {
			return peek_scope ().get_object<Darwin.Xpc.UInt64> (Darwin.Xpc.UInt64.TYPE).get_value ();
		}

		public unowned uint8[] get_data_value () throws Error {
			var data = peek_scope ().get_object<Darwin.Xpc.Data> (Darwin.Xpc.Data.TYPE);
			unowned uint8[] buf = (uint8[]) data.get_bytes_ptr ();
			buf.length = (int) data.get_length ();
			return buf;
		}

		public unowned string get_string_value () throws Error {
			return peek_scope ().get_object<Darwin.Xpc.String> (Darwin.Xpc.String.TYPE).get_string_ptr ();
		}

		public unowned uint8[] get_uuid_value () throws Error {
			return peek_scope ().get_object<Darwin.Xpc.Uuid> (Darwin.Xpc.Uuid.TYPE).get_bytes ()[:16];
		}

		public unowned string get_error_description () throws Error {
			var error = peek_scope ().get_object<Darwin.Xpc.Error> (Darwin.Xpc.Error.TYPE);
			return error.get_string (Darwin.Xpc.Error.KEY_DESCRIPTION);
		}

		public void check_nserror (TranslateErrorFunc translate_error) throws Error {
			if (!has_member ("error"))
				return;

			read_member ("error");

			unowned string domain = read_member ("domain").get_string_value ();
			end_member ();

			var code = (int) read_member ("code").get_int64_value ();
			end_member ();

			string description = read_member ("userInfo").read_member ("NSLocalizedDescription").get_string_value ();
			if (description.has_suffix ("."))
				description = description[:description.length - 1];

			throw (Error) new GLib.Error.literal (
				Quark.from_string ("frida-error-quark"),
				translate_error (domain, code, description),
				description);
		}

		public unowned Darwin.Xpc.Object get_object_value (Darwin.Xpc.Type expected_type) throws Error {
			return peek_scope ().get_object<Darwin.Xpc.Object> (expected_type);
		}

		private void push_scope (Darwin.Xpc.Object obj) {
			scopes.offer_tail (new Scope (obj));
		}

		private Scope peek_scope () {
			return scopes.peek_tail ();
		}

		private Scope pop_scope () {
			return scopes.poll_tail ();
		}

		private class Scope {
			public Darwin.Xpc.Object object;
			private unowned Darwin.Xpc.Type type;

			public Scope (Darwin.Xpc.Object obj) {
				object = obj;
				type = obj.type;
			}

			public unowned T get_object<T> (Darwin.Xpc.Type expected_type) throws Error {
				if (type != expected_type)
					throw new Error.PROTOCOL ("Expected type '%s', got '%s'", expected_type.name, type.name);
				return object;
			}

			public unowned Darwin.Xpc.Array get_array () throws Error {
				return get_object<Darwin.Xpc.Array> (Darwin.Xpc.Array.TYPE);
			}

			public unowned Darwin.Xpc.Dictionary get_dictionary () throws Error {
				return get_object<Darwin.Xpc.Dictionary> (Darwin.Xpc.Dictionary.TYPE);
			}
		}
	}
}
