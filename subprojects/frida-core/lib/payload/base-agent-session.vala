namespace Frida {
	public abstract class BaseAgentSession : Object, AgentSession {
		public signal void closed ();
		public signal void script_eternalized (Gum.Script script);

		public weak ProcessInvader invader {
			get;
			construct;
		}

		public AgentSessionId id {
			get;
			construct;
		}

		public uint persist_timeout {
			get;
			construct;
		}

		public AgentMessageSink? message_sink {
			get { return transmitter.message_sink; }
			set { transmitter.message_sink = value; }
		}

		public MainContext frida_context {
			get;
			construct;
		}

		public MainContext dbus_context {
			get;
			construct;
		}

		private Promise<bool>? close_request;
		private Promise<bool> flush_complete = new Promise<bool> ();

		private bool child_gating_enabled = false;

		private ScriptEngine script_engine;
		private AgentMessageTransmitter transmitter;

		construct {
			assert (invader != null);
			assert (frida_context != null);
			assert (dbus_context != null);

			script_engine = new ScriptEngine (invader);
			script_engine.message_from_script.connect (on_message_from_script);
			script_engine.message_from_debugger.connect (on_message_from_debugger);

			transmitter = new AgentMessageTransmitter (this, persist_timeout, frida_context, dbus_context);
			transmitter.closed.connect (on_transmitter_closed);
			transmitter.new_candidates.connect (on_transmitter_new_candidates);
			transmitter.candidate_gathering_done.connect (on_transmitter_candidate_gathering_done);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			while (close_request != null) {
				try {
					yield close_request.future.wait_async (cancellable);
					return;
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					cancellable.set_error_if_cancelled ();
				}
			}
			close_request = new Promise<bool> ();

			try {
				yield disable_child_gating (cancellable);
			} catch (GLib.Error e) {
				assert (e is IOError.CANCELLED);
				close_request.reject (e);
				throw (IOError) e;
			}

			yield script_engine.flush ();
			flush_complete.resolve (true);

			yield script_engine.close ();
			script_engine.message_from_script.disconnect (on_message_from_script);
			script_engine.message_from_debugger.disconnect (on_message_from_debugger);

			yield transmitter.close (cancellable);

			close_request.resolve (true);
		}

		public async void interrupt (Cancellable? cancellable) throws Error, IOError {
			transmitter.interrupt ();
		}

		public async void resume (uint rx_batch_id, Cancellable? cancellable, out uint tx_batch_id) throws Error, IOError {
			transmitter.resume (rx_batch_id, out tx_batch_id);
		}

		public async void flush () {
			if (close_request == null)
				close.begin (null);

			try {
				yield flush_complete.future.wait_async (null);
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
		}

		public async void prepare_for_termination (TerminationReason reason) {
			yield script_engine.prepare_for_termination (reason);
		}

		public void unprepare_for_termination () {
			script_engine.unprepare_for_termination ();
		}

		public async void enable_child_gating (Cancellable? cancellable) throws Error, IOError {
			check_open ();

			if (child_gating_enabled)
				return;

			invader.acquire_child_gating ();

			child_gating_enabled = true;
		}

		public async void disable_child_gating (Cancellable? cancellable) throws Error, IOError {
			if (!child_gating_enabled)
				return;

			invader.release_child_gating ();

			child_gating_enabled = false;
		}

		public async AgentScriptId create_script (string source, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			check_open ();

			var instance = yield script_engine.create_script (source, null, ScriptOptions._deserialize (options));
			return instance.script_id;
		}

		public async AgentScriptId create_script_from_bytes (uint8[] bytes, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			check_open ();

			var instance = yield script_engine.create_script (null, new Bytes (bytes), ScriptOptions._deserialize (options));
			return instance.script_id;
		}

		public async uint8[] compile_script (string source, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			check_open ();

			var bytes = yield script_engine.compile_script (source, ScriptOptions._deserialize (options));
			return bytes.get_data ();
		}

		public async uint8[] snapshot_script (string embed_script, HashTable<string, Variant> options, Cancellable? cancellable)
				throws Error, IOError {
			check_open ();

			var bytes = yield script_engine.snapshot_script (embed_script, SnapshotOptions._deserialize (options));
			return bytes.get_data ();
		}

		public async void destroy_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			yield script_engine.destroy_script (script_id);
		}

		public async void load_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			yield script_engine.load_script (script_id);
		}

		public async void interrupt_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			script_engine.interrupt_script (script_id);
		}

		public async void terminate_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			yield script_engine.terminate_script (script_id);
		}

		public async void eternalize_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			var script = script_engine.eternalize_script (script_id);
			script_eternalized (script);
		}

		public async void enable_debugger (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			script_engine.enable_debugger (script_id);
		}

		public async void disable_debugger (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			script_engine.disable_debugger (script_id);
		}

		public async void post_messages (AgentMessage[] messages, uint batch_id,
				Cancellable? cancellable) throws Error, IOError {
			transmitter.check_okay_to_receive ();

			foreach (var m in messages) {
				switch (m.kind) {
					case SCRIPT:
						script_engine.post_to_script (m.script_id, m.text, m.has_data ? new Bytes (m.data) : null);
						break;
					case DEBUGGER:
						script_engine.post_to_debugger (m.script_id, m.text);
						break;
				}
			}

			transmitter.notify_rx_batch_id (batch_id);
		}

		public async PortalMembershipId join_portal (string address, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return yield invader.join_portal (address, PortalOptions._deserialize (options), cancellable);
		}

		public async void leave_portal (PortalMembershipId membership_id, Cancellable? cancellable) throws Error, IOError {
			yield invader.leave_portal (membership_id, cancellable);
		}

		public async void offer_peer_connection (string offer_sdp, HashTable<string, Variant> peer_options,
				Cancellable? cancellable, out string answer_sdp) throws Error, IOError {
			yield transmitter.offer_peer_connection (offer_sdp, peer_options, cancellable, out answer_sdp);
		}

		public async void add_candidates (string[] candidate_sdps, Cancellable? cancellable) throws Error, IOError {
			transmitter.add_candidates (candidate_sdps);
		}

		public async void notify_candidate_gathering_done (Cancellable? cancellable) throws Error, IOError {
			transmitter.notify_candidate_gathering_done ();
		}

		public async void begin_migration (Cancellable? cancellable) throws Error, IOError {
			transmitter.begin_migration ();
		}

		public async void commit_migration (Cancellable? cancellable) throws Error, IOError {
			transmitter.commit_migration ();
		}

		private void check_open () throws Error {
			if (close_request != null)
				throw new Error.INVALID_OPERATION ("Session is closing");
		}

		private void on_message_from_script (AgentScriptId script_id, string json, Bytes? data) {
			transmitter.post_message_from_script (script_id, json, data);
		}

		private void on_message_from_debugger (AgentScriptId script_id, string message) {
			transmitter.post_message_from_debugger (script_id, message);
		}

		private void on_transmitter_closed () {
			transmitter.closed.disconnect (on_transmitter_closed);
			transmitter.new_candidates.disconnect (on_transmitter_new_candidates);
			transmitter.candidate_gathering_done.disconnect (on_transmitter_candidate_gathering_done);

			closed ();
		}

		private void on_transmitter_new_candidates (string[] candidate_sdps) {
			new_candidates (candidate_sdps);
		}

		private void on_transmitter_candidate_gathering_done () {
			candidate_gathering_done ();
		}
	}
}
