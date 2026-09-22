namespace Frida {
	public const uint16 DEFAULT_CONTROL_PORT = 27042;
	public const uint16 DEFAULT_CLUSTER_PORT = 27052;

	public SocketConnectable parse_control_address (string? address, uint16 port = 0) throws Error {
		return parse_socket_address (address, port, "127.0.0.1", DEFAULT_CONTROL_PORT);
	}

	public SocketConnectable parse_cluster_address (string? address, uint16 port = 0) throws Error {
		return parse_socket_address (address, port, "127.0.0.1", DEFAULT_CLUSTER_PORT);
	}

	public SocketConnectable parse_socket_address (string? address, uint16 port, string default_address,
			uint16 default_port) throws Error {
		if (address == null)
			address = default_address;
		if (port == 0)
			port = default_port;

#if !WINDOWS
		if (address.has_prefix ("unix:")) {
			string path = address.substring (5);

			UnixSocketAddressType type = UnixSocketAddress.abstract_names_supported ()
				? UnixSocketAddressType.ABSTRACT
				: UnixSocketAddressType.PATH;

			return new UnixSocketAddress.with_type (path, -1, type);
		}
#endif

		try {
			return NetworkAddress.parse (address, port);
		} catch (GLib.Error e) {
			throw new Error.INVALID_ARGUMENT ("%s", e.message);
		}
	}

	namespace UnixSocket {
		public extern void tune_buffer_sizes (int fd);
	}

	namespace Tcp {
		public extern void enable_nodelay (Socket socket);
	}

	namespace Udp {
		public size_t recv (uint8[] data, DatagramBased source, Cancellable? cancellable, out InetSocketAddress remote_address)
				throws Error, IOError {
			var v = InputVector ();
			v.buffer = data;
			v.size = data.length;

			InputVector[] vectors = { v };

			var m = InputMessage ();
			remote_address = null;
			m.address = &remote_address;
			m.vectors = vectors;
			m.num_vectors = vectors.length;

			InputMessage[] messages = { m };

			try {
				source.receive_messages (messages, 0, 0, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}

			return messages[0].bytes_received;
		}

		public void send (uint8[] data, DatagramBased sink, Cancellable? cancellable) throws Error, IOError {
			send_to (data, null, sink, cancellable);
		}

		public void send_to (uint8[] data, InetSocketAddress? dest_addr, DatagramBased sink, Cancellable? cancellable)
				throws Error, IOError {
			var v = OutputVector ();
			v.buffer = data;
			v.size = data.length;

			OutputVector[] vectors = { v };

			var m = OutputMessage ();
			m.address = dest_addr;
			m.vectors = vectors;
			m.num_vectors = vectors.length;

			OutputMessage[] messages = { m };

			try {
				sink.send_messages (messages, 0, 0, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}
		}
	}

	/**
	 * Configures the network endpoint a {@link PortalService} or
	 * {@link ControlService} listens on.
	 */
	public sealed class EndpointParameters : Object {
		/**
		 * The address to bind to, or null for the default.
		 */
		public string? address {
			get;
			construct;
		}

		/**
		 * The port to listen on, or 0 for the default.
		 */
		public uint16 port {
			get;
			construct;
		}

		/**
		 * TLS certificate to serve, enabling TLS when set.
		 */
		public TlsCertificate? certificate {
			get;
			construct;
		}

		/**
		 * Origin to require from connecting clients, if any.
		 */
		public string? origin {
			get;
			construct;
		}

		/**
		 * Service used to authenticate connecting clients, if any.
		 */
		public AuthenticationService? auth_service {
			get;
			construct;
		}

		/**
		 * Directory whose files are served as static web assets, if any.
		 */
		public File? asset_root {
			get;
			set;
		}

		/**
		 * Handler invoked for web requests, if any.
		 */
		public WebRequestHandler? request_handler {
			get;
			set;
		}

		/**
		 * Creates endpoint parameters.
		 *
		 * @param address the address to bind to, or null for the default
		 * @param port the port to listen on, or 0 for the default
		 * @param certificate TLS certificate to serve, or null for plaintext
		 * @param origin origin to require from clients, or null
		 * @param auth_service service used to authenticate clients, or null
		 * @param asset_root directory of static assets to serve, or null
		 */
		public EndpointParameters (string? address = null, uint16 port = 0, TlsCertificate? certificate = null,
				string? origin = null, AuthenticationService? auth_service = null, File? asset_root = null) {
			Object (
				address: address,
				port: port,
				certificate: certificate,
				origin: origin,
				auth_service: auth_service,
				asset_root: asset_root
			);
		}
	}

	/**
	 * Handles web requests made to an endpoint's served assets.
	 */
	public interface WebRequestHandler : Object {
		/**
		 * Handles a single web request.
		 *
		 * @param request the incoming request
		 * @return the response to send, or null to fall through to the default
		 *   handling
		 */
		public abstract async WebResponse? handle_request (WebRequest request, Cancellable? cancellable) throws Error, IOError;
	}

	/**
	 * An incoming web request passed to a {@link WebRequestHandler}.
	 */
	public sealed class WebRequest : Object {
		/**
		 * The HTTP method, such as `GET` or `POST`.
		 */
		public string method {
			get;
			construct;
		}

		/**
		 * The request path.
		 */
		public string path {
			get;
			construct;
		}

		/**
		 * The raw query string, if any.
		 */
		public string? query_string {
			get;
			construct;
		}

		/**
		 * The request body, if any.
		 */
		public Bytes? body {
			get;
			construct;
		}

		private Soup.MessageHeaders raw_headers;

		internal WebRequest (string method, string path, string? query_string, Soup.MessageHeaders headers, Bytes? body) {
			Object (method: method, path: path, query_string: query_string, body: body);
			raw_headers = headers;
		}

		/**
		 * Invokes @func for each request header.
		 *
		 * @param func function called with each header's name and value
		 */
		public void foreach_header (WebRequestHeaderFunc func) {
			raw_headers.foreach ((name, val) => {
				func (name, val);
			});
		}
	}

	/**
	 * Callback invoked for each header while iterating a {@link WebRequest}.
	 *
	 * @param name the header name
	 * @param val the header value
	 */
	public delegate void WebRequestHeaderFunc (string name, string val);

	/**
	 * A response returned from a {@link WebRequestHandler}.
	 */
	public sealed class WebResponse : Object {
		/**
		 * The HTTP status code.
		 */
		public uint status {
			get;
			construct;
		}

		/**
		 * The response body.
		 */
		public Bytes body {
			get;
			construct;
		}

		private Soup.MessageHeaders raw_headers = new Soup.MessageHeaders (RESPONSE);

		/**
		 * Creates a response.
		 *
		 * @param status the HTTP status code
		 * @param body the response body
		 */
		public WebResponse (uint status, Bytes body) {
			Object (status: status, body: body);
		}

		/**
		 * Adds a header to the response.
		 *
		 * @param name the header name
		 * @param val the header value
		 */
		public void add_header (string name, string val) {
			raw_headers.append (name, val);
		}

		internal void write_headers_to (Soup.MessageHeaders target) {
			raw_headers.foreach ((name, val) => {
				target.replace (name, val);
			});
		}
	}

	public async IOStream negotiate_connection (IOStream stream, WebServiceTransport transport, string host, string? origin,
			Cancellable? cancellable) throws Error, IOError {
		var input = (DataInputStream) Object.new (typeof (DataInputStream),
			"base-stream", stream.get_input_stream (),
			"close-base-stream", false,
			"newline-type", DataStreamNewlineType.CR_LF);
		OutputStream output = stream.get_output_stream ();

		var request = new StringBuilder.sized (256);
		request.append ("GET /ws HTTP/1.1\r\n");
		string protocol = (transport == TLS) ? "wss" : "ws";
		NetworkAddress addr;
		try {
			addr = NetworkAddress.parse (host, 0);
		} catch (GLib.Error e) {
			throw new Error.INVALID_ARGUMENT ("%s", e.message);
		}
		uint16 port = addr.get_port ();
		var uri = Uri.build (UriFlags.NONE, protocol, null, addr.get_hostname (), (port != 0) ? port : -1, "/ws", null, null);
		var msg = new Soup.Message.from_uri ("GET", uri);
		Soup.websocket_client_prepare_handshake (msg, origin, null, null);
		msg.request_headers.replace ("Host", make_host_header_value (uri));
		msg.request_headers.replace ("User-Agent", "Frida/" + _version_string ());
		msg.request_headers.foreach ((name, val) => {
			request.append (name + ": " + val + "\r\n");
		});
		request.append ("\r\n");

		var response = new StringBuilder.sized (256);
		try {
			size_t bytes_written;
			yield output.write_all_async (request.str.data, Priority.DEFAULT, cancellable, out bytes_written);

			string? line = null;
			do {
				size_t length;
				line = yield input.read_line_async (Priority.DEFAULT, cancellable, out length);
				if (line == null)
					throw new Error.TRANSPORT ("Connection closed");
				if (line != "")
					response.append (line + "\r\n");
			} while (line != "");
		} catch (GLib.Error e) {
			if (e is IOError.CANCELLED)
				throw (IOError) e;
			throw new Error.TRANSPORT ("%s", e.message);
		}

		var headers = new Soup.MessageHeaders (RESPONSE);
		Soup.HTTPVersion ver;
		uint status_code;
		string reason_phrase;
		if (!Soup.headers_parse_response (response.str, (int) response.len, headers, out ver, out status_code,
				out reason_phrase)) {
			throw new Error.PROTOCOL ("Invalid response");
		}

		if (status_code != Soup.Status.SWITCHING_PROTOCOLS) {
			if (status_code == Soup.Status.FORBIDDEN)
				throw new Error.INVALID_ARGUMENT ("Incorrect origin");
			else
				throw new Error.PROTOCOL ("%s", reason_phrase);
		}

		WebConnection connection = null;
		var frida_context = MainContext.ref_thread_default ();
		var dbus_context = yield get_dbus_context ();
		var dbus_source = new IdleSource ();
		dbus_source.set_callback (() => {
			var websocket = new Soup.WebsocketConnection (stream, msg.uri, CLIENT, origin, protocol,
				new List<Soup.WebsocketExtension> ());
			connection = new WebConnection (websocket);

			var frida_source = new IdleSource ();
			frida_source.set_callback (negotiate_connection.callback);
			frida_source.attach (frida_context);

			return Source.REMOVE;
		});
		dbus_source.attach (dbus_context);
		yield;

		return connection;
	}

	private string make_host_header_value (Uri uri) {
		unowned string host = uri.get_host ();
		if (!Hostname.is_ip_address (host))
			return host;
		var inet_addr = new InetAddress.from_string (host);
		return (inet_addr.get_family () == IPV6) ? @"[$host]" : host;
	}

	public sealed class WebService : Object {
		public signal void incoming (IOStream connection, SocketAddress remote_address, DynamicInterface? dynamic_iface);

		public EndpointParameters endpoint_params {
			get;
			construct;
		}

		public WebServiceFlavor flavor {
			get;
			construct;
		}

		public PortConflictBehavior on_port_conflict {
			get;
			construct;
			default = FAIL;
		}

		public DynamicInterfaceObserver? dynamic_interface_observer {
			get;
			construct;
		}

		public SocketAddress? listen_address {
			get {
				return _listen_address;
			}
		}

		private ConnectionHandler main_handler;
		private SocketAddress? _listen_address;
		private Gee.Map<string, ConnectionHandler> dynamic_interface_handlers = new Gee.HashMap<string, ConnectionHandler> ();

		private Cancellable io_cancellable = new Cancellable ();

		private MainContext? frida_context;
		private MainContext? dbus_context;

		public WebService (EndpointParameters endpoint_params, WebServiceFlavor flavor,
				PortConflictBehavior on_port_conflict = FAIL, DynamicInterfaceObserver? dynif_observer = null) {
			Object (
				endpoint_params: endpoint_params,
				flavor: flavor,
				on_port_conflict: on_port_conflict,
				dynamic_interface_observer: dynif_observer
			);
		}

		public async void start (Cancellable? cancellable) throws Error, IOError {
			frida_context = MainContext.ref_thread_default ();
			dbus_context = yield get_dbus_context ();

			cancellable.set_error_if_cancelled ();

			var start_request = new Promise<SocketAddress> ();
			schedule_on_dbus_thread (() => {
				handle_start_request.begin (start_request, cancellable);
				return Source.REMOVE;
			});

			_listen_address = yield start_request.future.wait_async (cancellable);
		}

		private async void handle_start_request (Promise<SocketAddress> start_request, Cancellable? cancellable) {
			try {
				SocketAddress effective_address = yield do_start (cancellable);
				schedule_on_frida_thread (() => {
					start_request.resolve (effective_address);
					return Source.REMOVE;
				});
			} catch (GLib.Error e) {
				GLib.Error start_error = e;
				schedule_on_frida_thread (() => {
					start_request.reject (start_error);
					return Source.REMOVE;
				});
			}
		}

		private async SocketAddress do_start (Cancellable? cancellable) throws Error, IOError {
			main_handler = make_connection_handler (null);
			SocketAddress? first_effective_address = null;
			SocketConnectable connectable = (flavor == CONTROL)
				? parse_control_address (endpoint_params.address, endpoint_params.port)
				: parse_cluster_address (endpoint_params.address, endpoint_params.port);
			var enumerator = connectable.enumerate ();
			while (true) {
				SocketAddress? address;
				try {
					address = yield enumerator.next_async (io_cancellable);
				} catch (GLib.Error e) {
					throw new Error.NOT_SUPPORTED ("%s", e.message);
				}
				if (address == null)
					break;

				SocketAddress? effective_address = null;
				InetSocketAddress? inet_address = address as InetSocketAddress;
				if (inet_address != null) {
					effective_address = main_handler.listen_on_inet_address (inet_address);
				} else {
					main_handler.listen_on_socket_address (address);
					effective_address = address;
				}

				if (first_effective_address == null)
					first_effective_address = effective_address;
			}

			if (first_effective_address == null)
				throw new Error.NOT_SUPPORTED ("Unable to resolve listening address");

			if (dynamic_interface_observer != null) {
				dynamic_interface_observer.interface_attached.connect (on_dynamic_interface_attached);
				dynamic_interface_observer.interface_detached.connect (on_dynamic_interface_detached);
				dynamic_interface_observer.start ();
			}

			return first_effective_address;
		}

		private void on_dynamic_interface_attached (DynamicInterface iface) {
			unowned string name = iface.name;

			uint16 port = endpoint_params.port;
			if (port == 0)
				port = (flavor == CONTROL) ? DEFAULT_CONTROL_PORT : DEFAULT_CLUSTER_PORT;

			var handler = make_connection_handler (iface);
			try {
				handler.listen_on_inet_address (new InetSocketAddress (iface.ip, port));
			} catch (Error e) {
				return;
			}
			dynamic_interface_handlers[name] = handler;
		}

		private void on_dynamic_interface_detached (DynamicInterface iface) {
			ConnectionHandler handler;
			if (dynamic_interface_handlers.unset (iface.name, out handler))
				handler.close ();
		}

		private ConnectionHandler make_connection_handler (DynamicInterface? dynamic_iface) {
			var handler = new ConnectionHandler (dynamic_iface, endpoint_params, on_port_conflict);
			handler.incoming.connect (on_incoming_connection);
			return handler;
		}

		private void on_incoming_connection (ConnectionHandler handler, IOStream connection, SocketAddress remote_address) {
			schedule_on_frida_thread (() => {
				incoming (connection, remote_address, handler.dynamic_iface);
				return Source.REMOVE;
			});
		}

		public void stop () {
			io_cancellable.cancel ();

			schedule_on_dbus_thread (() => {
				do_stop ();
				return Source.REMOVE;
			});
		}

		private void do_stop () {
			foreach (var handler in dynamic_interface_handlers.values)
				handler.close ();
			dynamic_interface_handlers.clear ();

			if (dynamic_interface_observer != null) {
				dynamic_interface_observer.interface_attached.disconnect (on_dynamic_interface_attached);
				dynamic_interface_observer.interface_detached.disconnect (on_dynamic_interface_detached);
			}

			if (main_handler != null)
				main_handler.close ();
		}

		private void schedule_on_frida_thread (owned SourceFunc function) {
			assert (frida_context != null);

			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (frida_context);
		}

		private void schedule_on_dbus_thread (owned SourceFunc function) {
			assert (dbus_context != null);

			var source = new IdleSource ();
			source.set_callback ((owned) function);
			source.attach (dbus_context);
		}

		private class ConnectionHandler : Object {
			public signal void incoming (IOStream connection, SocketAddress remote_address);

			public DynamicInterface? dynamic_iface {
				get;
				construct;
			}

			public EndpointParameters endpoint_params {
				get;
				construct;
			}

			public PortConflictBehavior on_port_conflict {
				get;
				construct;
				default = FAIL;
			}

			private Soup.Server server;
			private Gee.Set<WebConnection> connections = new Gee.HashSet<WebConnection> ();
			private Cancellable io_cancellable = new Cancellable ();

			public ConnectionHandler (DynamicInterface? dynamic_iface, EndpointParameters endpoint_params,
					PortConflictBehavior on_port_conflict) {
				Object (
					dynamic_iface: dynamic_iface,
					endpoint_params: endpoint_params,
					on_port_conflict: on_port_conflict
				);
			}

			construct {
				server = (Soup.Server) Object.new (typeof (Soup.Server),
					"tls-certificate", endpoint_params.certificate);

				server.add_websocket_handler ("/ws", endpoint_params.origin, null, on_websocket_opened);

				if (endpoint_params.request_handler != null || endpoint_params.asset_root != null)
					server.add_handler (null, on_http_request);
			}

			public void close () {
				io_cancellable.cancel ();

				if (endpoint_params.request_handler != null || endpoint_params.asset_root != null)
					server.remove_handler ("/");
				server.remove_handler ("/ws");

				server.disconnect ();

				foreach (var connection in connections.to_array ()) {
					try {
						connection.close (null);
					} catch (IOError e) {
						assert_not_reached ();
					}
					remove_connection (connection);
				}
			}

			public InetSocketAddress listen_on_inet_address (InetSocketAddress address) throws Error {
				InetSocketAddress candidate_address = address;
				uint16 start_port = address.get_port ();
				uint16 candidate_port = start_port;
				do {
					try {
						server.listen (candidate_address, compute_listen_options ());
						return candidate_address;
					} catch (GLib.Error e) {
						if (e is IOError.ADDRESS_IN_USE && on_port_conflict == PICK_NEXT) {
							candidate_port++;
							if (candidate_port == start_port)
								throw new Error.ADDRESS_IN_USE ("Unable to bind to any port");
							if (candidate_port == 0)
								candidate_port = 1024;
							candidate_address = new InetSocketAddress (candidate_address.get_address (),
								candidate_port);
						} else {
							throw_listen_error (e);
						}
					}
				} while (true);
			}

			public void listen_on_socket_address (SocketAddress address) throws Error {
				try {
					server.listen (address, compute_listen_options ());
				} catch (GLib.Error e) {
					throw_listen_error (e);
				}
			}

			private Soup.ServerListenOptions compute_listen_options () {
				return (endpoint_params.certificate != null)
					? Soup.ServerListenOptions.HTTPS
					: 0;
			}

			[NoReturn]
			private static void throw_listen_error (GLib.Error e) throws Error {
				if (e is IOError.ADDRESS_IN_USE)
					throw new Error.ADDRESS_IN_USE ("%s", e.message);

				if (e is IOError.PERMISSION_DENIED)
					throw new Error.PERMISSION_DENIED ("%s", e.message);

				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			private void on_websocket_opened (Soup.Server server, Soup.ServerMessage msg, string path,
					Soup.WebsocketConnection connection) {
				var peer = new WebConnection (connection);
				peer.websocket_closed.connect (on_websocket_closed);
				connections.add (peer);

				IOStream soup_stream = connection.get_io_stream ();

				IOStream base_iostream;
				soup_stream.get ("base-iostream", out base_iostream);

				SocketAddress? remote_address = null;
				SocketConnection? sc = base_iostream as SocketConnection;
				if (sc == null) {
					IOStream tls_stream = base_iostream;
					tls_stream.get ("base-io-stream", out sc);
				}
				try {
					remote_address = sc.get_remote_address ();
				} catch (GLib.Error e) {
					assert_not_reached ();
				}

				incoming (peer, remote_address);
			}

			private void on_websocket_closed (WebConnection connection) {
				remove_connection (connection);
			}

			private void remove_connection (WebConnection connection) {
				connection.websocket_closed.disconnect (on_websocket_closed);
				connections.remove (connection);
			}

			private void on_http_request (Soup.Server server, Soup.ServerMessage msg, string path,
					HashTable<string, string>? query) {
				msg.get_response_headers ().replace ("Server", "Frida/" + _version_string ());

				msg.pause ();
				handle_http_request.begin (msg, path);
			}

			private async void handle_http_request (Soup.ServerMessage msg, string path) {
				WebRequestHandler? handler = endpoint_params.request_handler;
				if (handler != null) {
					if (yield try_request_handler (handler, msg, path, io_cancellable))
						return;
				}

				File? asset_root = endpoint_params.asset_root;
				if (asset_root != null) {
					unowned string method = msg.get_method ();
					if (method != "GET" && method != "HEAD") {
						msg.set_status (Soup.Status.METHOD_NOT_ALLOWED, null);
						msg.unpause ();
						return;
					}

					File location = asset_root.resolve_relative_path (path.next_char ());
					yield handle_asset_request (path, location, msg);
					return;
				}

				msg.set_status (Soup.Status.NOT_FOUND, null);
				msg.unpause ();
			}

			private static async bool try_request_handler (WebRequestHandler handler, Soup.ServerMessage msg, string path,
					Cancellable? cancellable) {
				var body = msg.get_request_body ();
				Bytes? request_body = (body.length != 0) ? body.flatten () : null;
				unowned Uri? uri = msg.get_uri ();
				string? query_string = (uri != null) ? uri.get_query () : null;
				var request = new WebRequest (msg.get_method (), path, query_string, msg.get_request_headers (),
					request_body);

				WebResponse? response;
				try {
					response = yield handler.handle_request (request, cancellable);
				} catch (Error e) {
					msg.set_status (Soup.Status.INTERNAL_SERVER_ERROR, null);
					msg.set_response ("text/plain; charset=utf-8", Soup.MemoryUse.COPY, e.message.data);
					msg.unpause ();
					return true;
				} catch (IOError e) {
					msg.unpause ();
					return true;
				}

				if (response == null)
					return false;

				apply_response (msg, response);
				msg.unpause ();
				return true;
			}

			private static void apply_response (Soup.ServerMessage msg, WebResponse response) {
				msg.set_status (response.status, null);

				response.write_headers_to (msg.get_response_headers ());

				Bytes body = response.body;
				if (body.length > 0)
					msg.get_response_body ().append_bytes (body);
			}

			private async void handle_asset_request (string path, File file, Soup.ServerMessage msg) {
				int priority = Priority.DEFAULT;

				string attributes = FileAttribute.STANDARD_TYPE + "," + FileAttribute.STANDARD_SIZE;

				FileInfo info;
				FileInputStream? stream = null;
				try {
					info = yield file.query_info_async (attributes, FileQueryInfoFlags.NONE, priority, io_cancellable);

					FileType type = info.get_file_type ();
					if (type == DIRECTORY) {
						if (!path.has_suffix ("/")) {
							handle_misplaced_request (path + "/", msg);
							return;
						}

						File index_file = file.get_child ("index.html");
						try {
							var index_info = yield index_file.query_info_async (attributes,
								FileQueryInfoFlags.NONE, priority, io_cancellable);
							file = index_file;
							info = index_info;
							type = index_info.get_file_type ();
						} catch (GLib.Error e) {
						}
					}

					if (type != DIRECTORY)
						stream = yield file.read_async (priority, io_cancellable);
				} catch (GLib.Error e) {
					msg.set_status (Soup.Status.NOT_FOUND, null);
					msg.unpause ();
					return;
				}

				if (stream == null)
					yield handle_directory_request (path, file, msg);
				else
					yield handle_file_request (file, info, stream, msg);
			}

			private async void handle_directory_request (string path, File file, Soup.ServerMessage msg) {
				var listing = new StringBuilder.sized (1024);

				string escaped_path = Markup.escape_text (path);
				listing.append ("""<html>
<head><title>Index of %s</title></head>
<body>
<h1>Index of %s</h1><hr><pre>""".printf (escaped_path, escaped_path));

				if (path != "/")
					listing.append ("<a href=\"../\">../</a>");

				listing.append_c ('\n');

				string attributes =
					FileAttribute.STANDARD_DISPLAY_NAME + "," +
					FileAttribute.STANDARD_TYPE + "," +
					FileAttribute.TIME_MODIFIED + "," +
					FileAttribute.STANDARD_SIZE;
				int priority = Priority.DEFAULT;

				try {
					var enumerator = yield file.enumerate_children_async (attributes, FileQueryInfoFlags.NONE, priority,
						io_cancellable);

					List<FileInfo> files = yield enumerator.next_files_async (int.MAX, priority, io_cancellable);

					files.sort ((a, b) => {
						bool a_is_dir = a.get_file_type () == DIRECTORY;
						bool b_is_dir = b.get_file_type () == DIRECTORY;
						if (a_is_dir == b_is_dir)
							return strcmp (a.get_display_name (), b.get_display_name ());
						else if (a_is_dir)
							return -1;
						else
							return 1;
					});

					foreach (FileInfo info in files) {
						string display_name = info.get_display_name ();
						FileType type = info.get_file_type ();
						DateTime modified = info.get_modification_date_time ().to_local ();

						string link = Markup.escape_text (display_name);
						if (type == DIRECTORY)
							link += "/";

						listing
							.append ("<a href=\"")
							.append (link)
							.append ("\">")
							.append (link)
							.append ("</a>");

						int padding_needed = 50 - link.length;
						while (padding_needed > 0) {
							listing.append_c (' ');
							padding_needed--;
						}

						listing
							.append_c (' ')
							.append (modified.format ("%d-%b-%Y %H:%M"))
							.append ("            ");

						string size_info;
						if (type != DIRECTORY)
							size_info = info.get_size ().to_string ();
						else
							size_info = "-";
						listing.append_printf ("%8s\n", size_info);
					}
				} catch (GLib.Error e) {
					msg.set_status (Soup.Status.NOT_FOUND, null);
					msg.unpause ();
					return;
				}

				listing.append ("</pre><hr></body>\n</html>");

				msg.set_status (Soup.Status.OK, null);

				if (msg.get_method () == "HEAD") {
					var headers = msg.get_response_headers ();
					headers.replace ("Content-Type", "text/html");
					headers.replace ("Content-Length", listing.len.to_string ());
				} else {
					msg.set_response ("text/html", Soup.MemoryUse.COPY, listing.str.data);
				}

				msg.unpause ();
			}

			private async void handle_file_request (File file, FileInfo info, FileInputStream stream, Soup.ServerMessage msg) {
				msg.set_status (Soup.Status.OK, null);

				var headers = msg.get_response_headers ();
				headers.replace ("Content-Type", guess_mime_type_for (file.get_path ()));
				headers.replace ("Content-Length", info.get_size ().to_string ());

				if (msg.get_method () == "HEAD") {
					msg.unpause ();
					return;
				}

				var body = msg.get_response_body ();
				body.set_accumulate (false);

				bool finished = false;
				bool waiting = false;
				ulong finished_handler = msg.finished.connect (() => {
					finished = true;
					if (waiting)
						handle_file_request.callback ();
				});
				ulong write_handler = msg.wrote_body_data.connect (chunk => {
					if (waiting)
						handle_file_request.callback ();
				});
				try {
					var buffer = new uint8[64 * 1024];
					while (true) {
						ssize_t n;
						try {
							n = yield stream.read_async (buffer, Priority.DEFAULT, io_cancellable);
						} catch (IOError e) {
							break;
						}
						if (n == 0 || finished)
							break;

						body.append_take (buffer[0:n]);

						msg.unpause ();

						waiting = true;
						yield;
						waiting = false;

						if (finished)
							break;

						msg.pause ();
					}
				} finally {
					msg.disconnect (write_handler);
					msg.disconnect (finished_handler);
					if (!finished)
						msg.unpause ();
				}
			}

			private void handle_misplaced_request (string redirect_uri, Soup.ServerMessage msg) {
				msg.set_redirect (Soup.Status.MOVED_PERMANENTLY, redirect_uri);

				string body = """<html>
<head><title>301 Moved Permanently</title></head>
<body>
<center><h1>301 Moved Permanently</h1></center>
<hr><center>%s</center>
</body>
</html>""".printf ("Frida/" + _version_string ());

				if (msg.get_method () == "HEAD") {
					var headers = msg.get_response_headers ();
					headers.replace ("Content-Type", "text/html");
					headers.replace ("Content-Length", body.length.to_string ());
				} else {
					msg.set_response ("text/html", Soup.MemoryUse.COPY, body.data);
				}

				msg.unpause ();
			}

			private static string guess_mime_type_for (string path) {
				if (path.has_suffix (".html"))
					return "text/html";

				if (path.has_suffix (".js"))
					return "text/javascript";

				if (path.has_suffix (".json"))
					return "application/json";

				if (path.has_suffix (".css"))
					return "text/css";

				if (path.has_suffix (".jpeg") || path.has_suffix (".jpg"))
					return "image/jpeg";

				if (path.has_suffix (".png"))
					return "image/png";

				if (path.has_suffix (".gif"))
					return "image/gif";

				bool uncertain;
				return ContentType.guess (path, null, out uncertain);
			}
		}
	}

	public enum WebServiceTransport {
		PLAIN,
		TLS
	}

	public enum WebServiceFlavor {
		CONTROL,
		CLUSTER
	}

	public enum PortConflictBehavior {
		FAIL,
		PICK_NEXT
	}

	public interface DynamicInterfaceObserver : Object {
		public signal void interface_attached (DynamicInterface iface);
		public signal void interface_detached (DynamicInterface iface);

		public abstract void start ();
	}

	public sealed class DynamicInterface : Object {
		public string name {
			get;
			construct;
		}

		public InetAddress ip {
			get;
			construct;
		}

		public DynamicInterface (string name, InetAddress ip) {
			Object (name: name, ip: ip);
		}
	}

	public extern static unowned string _version_string ();

	private sealed class WebConnection : VirtualStream {
		public signal void websocket_closed ();

		public Soup.WebsocketConnection websocket {
			get;
			construct;
		}

		private ByteArray recv_queue = new ByteArray ();
		private ByteArray send_queue = new ByteArray ();

		public WebConnection (Soup.WebsocketConnection websocket) {
			Object (websocket: websocket);
		}

		construct {
			websocket.max_incoming_payload_size = (256 * 1024) + 1; // XXX: There's an off-by-one error in libsoup

			websocket.closed.connect (on_closed);
			websocket.message.connect (on_message);
		}

		~WebConnection () {
			websocket.message.disconnect (on_message);
			websocket.closed.disconnect (on_closed);
		}

		protected override VirtualStream.State query_initial_state () {
			switch (websocket.state) {
				case OPEN:
				case CLOSING:
					return OPEN;
				case CLOSED:
					return CLOSED;
			}
			assert_not_reached ();
		}

		protected override IOCondition query_events () {
			IOCondition new_events = 0;

			if (recv_queue.len > 0 || state != OPEN)
				new_events |= IN;

			if (state == OPEN)
				new_events |= OUT;

			return new_events;
		}

		protected override void handle_close () {
			if (websocket.state != OPEN)
				return;

			websocket.close (1000, "Closing");
		}

		public override ssize_t read (uint8[] buffer) throws IOError {
			ssize_t n = 0;
			with_state_lock (() => {
				n = ssize_t.min (recv_queue.len, buffer.length);
				if (n > 0) {
					Memory.copy (buffer, recv_queue.data, n);
					recv_queue.remove_range (0, (uint) n);

					if (recv_queue.len == 0) {
#if HAVE_SOUP_WEBSOCKET_BACKPRESSURE
						websocket.resume_input ();
#endif
					}

					update_pending_io ();
				} else {
					if (state == OPEN)
						n = -1;
				}
			});

			if (n == -1)
				throw new IOError.WOULD_BLOCK ("Resource temporarily unavailable");

			return n;
		}

		public override ssize_t write (uint8[] buffer) {
			with_state_lock (() => {
				send_queue.append (buffer);
			});

			if (main_context.is_owner ()) {
				process_send_queue ();
			} else {
				var source = new IdleSource ();
				source.set_callback (() => {
					process_send_queue ();
					return Source.REMOVE;
				});
				source.attach (main_context);
			}

			return buffer.length;
		}

		private void process_send_queue () {
			if (websocket.state != OPEN)
				return;

			size_t max_message_size = (size_t) websocket.max_incoming_payload_size - 1;

			while (true) {
				uint8[]? chunk = null;
				with_state_lock (() => {
					size_t n = size_t.min (send_queue.len, max_message_size);
					if (n == 0)
						return;
					chunk = send_queue.data[0:n];
					send_queue.remove_range (0, (uint) n);
				});
				if (chunk == null)
					return;

				websocket.send_binary (chunk);
			}
		}

		private void on_closed () {
			with_state_lock (() => {
				state = CLOSED;
				update_pending_io ();
			});

			websocket_closed ();
		}

		private void on_message (int type, Bytes message) {
			with_state_lock (() => {
				recv_queue.append (message.get_data ());
#if HAVE_SOUP_WEBSOCKET_BACKPRESSURE
				websocket.pause_input ();
#endif
				update_pending_io ();
			});
		}
	}
}
