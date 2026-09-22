namespace Frida {
	/**
	 * Issues RPC calls over a {@link RpcPeer}, matching responses to requests.
	 */
	public sealed class RpcClient : Object {
		/**
		 * The peer used to send and receive RPC messages.
		 */
		public weak RpcPeer peer {
			get;
			construct;
		}

		private Gee.HashMap<string, PendingResponse> pending_responses = new Gee.HashMap<string, PendingResponse> ();

		/**
		 * Creates an RPC client that talks over @peer.
		 *
		 * @param peer the peer to send and receive messages through
		 */
		public RpcClient (RpcPeer peer) {
			Object (peer: peer);
		}

		/**
		 * Calls a remote method and waits for its result.
		 *
		 * @param method the method name to invoke
		 * @param args the JSON arguments
		 * @param data binary data to accompany the call, if any
		 * @return the method's JSON result
		 */
		public async Json.Node call (string method, Json.Node[] args, Bytes? data, Cancellable? cancellable) throws Error, IOError {
			string request_id = Uuid.string_random ();

			var request = new Json.Builder ();
			request
				.begin_array ()
				.add_string_value ("frida:rpc")
				.add_string_value (request_id)
				.add_string_value ("call")
				.add_string_value (method)
				.begin_array ();
			foreach (var arg in args)
				request.add_value (arg);
			request
				.end_array ()
				.end_array ();
			string raw_request = Json.to_string (request.get_root (), false);

			bool waiting = false;

			var pending = new PendingResponse (() => {
				if (waiting)
					call.callback ();
				return false;
			});
			pending_responses[request_id] = pending;

			try {
				yield peer.post_rpc_message (raw_request, data, cancellable);
			} catch (Error e) {
				if (pending_responses.unset (request_id))
					pending.complete_with_error (e);
			}

			if (!pending.completed) {
				var cancel_source = new CancellableSource (cancellable);
				cancel_source.set_callback (() => {
					if (pending_responses.unset (request_id))
						pending.complete_with_error (new IOError.CANCELLED ("Operation was cancelled"));
					return false;
				});
				cancel_source.attach (MainContext.get_thread_default ());

				waiting = true;
				yield;
				waiting = false;

				cancel_source.destroy ();
			}

			cancellable.set_error_if_cancelled ();

			if (pending.error != null)
				throw_api_error (pending.error);

			return pending.result;
		}

		/**
		 * Feeds an incoming message to the client, matching it against pending
		 * calls if it is an RPC response.
		 *
		 * @param json the incoming message as a JSON string
		 * @return true if the message was an RPC message and was handled
		 */
		public bool try_handle_message (string json) {
			if (json.index_of ("\"frida:rpc\"") == -1)
				return false;

			var parser = new Json.Parser ();
			try {
				parser.load_from_data (json);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
			var message = parser.get_root ().get_object ();

			bool handled = false;

			var type = message.get_string_member ("type");
			if (type == "send")
				handled = try_handle_rpc_message (message);

			return handled;
		}

		private bool try_handle_rpc_message (Json.Object message) {
			var payload = message.get_member ("payload");
			if (payload == null || payload.get_node_type () != Json.NodeType.ARRAY)
				return false;
			var rpc_message = payload.get_array ();
			if (rpc_message.get_length () < 4)
				return false;

			string? type = rpc_message.get_element (0).get_string ();
			if (type == null || type != "frida:rpc")
				return false;

			var request_id_value = rpc_message.get_element (1);
			if (request_id_value.get_value_type () != typeof (string))
				return false;
			string request_id = request_id_value.get_string ();

			PendingResponse response;
			if (!pending_responses.unset (request_id, out response))
				return false;

			var status = rpc_message.get_string_element (2);
			if (status == "ok")
				response.complete_with_result (rpc_message.get_element (3));
			else
				response.complete_with_error (new Error.NOT_SUPPORTED (rpc_message.get_string_element (3)));

			return true;
		}

		private class PendingResponse {
			private SourceFunc? handler;

			public bool completed {
				get {
					return result != null || error != null;
				}
			}

			public Json.Node? result {
				get;
				private set;
			}

			public GLib.Error? error {
				get;
				private set;
			}

			public PendingResponse (owned SourceFunc handler) {
				this.handler = (owned) handler;
			}

			public void complete_with_result (Json.Node result) {
				this.result = result;
				handler ();
				handler = null;
			}

			public void complete_with_error (GLib.Error error) {
				this.error = error;
				handler ();
				handler = null;
			}
		}
	}

	/**
	 * A transport over which an {@link RpcClient} sends and receives RPC
	 * messages.
	 */
	public interface RpcPeer : Object {
		/**
		 * Posts an RPC message to the peer.
		 *
		 * @param json the message as a JSON string
		 * @param data binary data to accompany the message, if any
		 */
		public abstract async void post_rpc_message (string json, Bytes? data, Cancellable? cancellable) throws Error, IOError;
	}
}
