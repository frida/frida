namespace Frida {
	public sealed class ServiceSessionRegistry : Object {
		public signal void session_closed (ServiceSessionId id);

		private Gee.Map<ServiceSessionId?, ServiceSession> sessions =
			new Gee.HashMap<ServiceSessionId?, ServiceSession> (ServiceSessionId.hash, ServiceSessionId.equal);

		~ServiceSessionRegistry () {
			clear ();
		}

		public void clear () {
			foreach (var session in sessions.values.to_array ())
				session.close ();
			sessions.clear ();
		}

		public void register (ServiceSessionId id, ServiceSession session) {
			sessions[id] = session;

			ServiceSessionId? boxed_id = id;
			session.set_data ("service-session-id", boxed_id);

			session.close.connect (on_session_close);
		}

		public ServiceSession link (ServiceSessionId id) throws Error {
			var session = sessions[id];
			if (session == null)
				throw new Error.INVALID_ARGUMENT ("Invalid session ID");
			return session;
		}

		public void unlink (ServiceSessionId id) {
			ServiceSession? session;
			if (sessions.unset (id, out session)) {
				session.close.disconnect (on_session_close);

				session_closed (id);
			}
		}

		private void on_session_close (ServiceSession session) {
			ServiceSessionId? id = session.get_data ("service-session-id");
			unlink (id);
		}
	}
}
