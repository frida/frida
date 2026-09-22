[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	public sealed class WindowsPairingBrowser : Object, PairingBrowser {
		private Gee.Map<string, Monitor> monitors = new Gee.HashMap<string, Monitor> ();

		private MainContext main_context = MainContext.ref_thread_default ();

		public async void start (Cancellable? cancellable) throws IOError {
			_enumerate_network_interfaces ((index, name, address) => {
				var monitor = new Monitor (this, index, address);
				monitors[name] = monitor;
			});
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			monitors.clear ();
		}

		private void on_result (WinDns.QueryResult * result, InetSocketAddress interface_address) {
			try {
				PairingServiceDetails service = parse_result (result, interface_address);

				var source = new IdleSource ();
				source.set_callback (() => {
					service_discovered (service);
					return Source.REMOVE;
				});
				source.attach (main_context);
			} catch (GLib.Error e) {
			}
		}

		private static PairingServiceDetails parse_result (WinDns.QueryResult * res,
				InetSocketAddress interface_address) throws GLib.Error {
			PairingServiceMetadata? meta = null;
			InetAddress? ip = null;
			uint16 port = 0;
			for (WinDns.Record * r = res->query_records; r != null; r = r->next) {
				switch (r->type) {
					case TEXT:
						WinDns.TxtData * txt = &r->txt;
						var txt_record = new Gee.ArrayList<string> ();
						foreach (unowned string16 str in txt->strings)
							txt_record.add (str.to_utf8 ());
						meta = PairingServiceMetadata.from_txt_record (txt_record);
						break;
					case AAAA:
						ip = new InetAddress.from_bytes (r->aaaa.ip.data, IPV6);
						break;
					case SRV:
						port = r->srv.port;
						break;
					default:
						break;
				}
			}
			if (meta == null || ip == null || port == 0)
				throw new Error.PROTOCOL ("Incomplete result");

			return new PairingServiceDetails () {
				identifier = meta.identifier,
				auth_tag = meta.auth_tag,
				endpoint = (InetSocketAddress) Object.new (typeof (InetSocketAddress),
					address: ip,
					port: port,
					scope_id: ip.get_is_link_local () ? interface_address.get_scope_id () : 0
				),
				interface_address = interface_address,
			};
		}

		private class Monitor {
			private weak WindowsPairingBrowser parent;
			private InetSocketAddress interface_address;

			private void * backend;

			public Monitor (WindowsPairingBrowser parent, ulong interface_index, InetSocketAddress interface_address) {
				this.parent = parent;
				this.interface_address = interface_address;

				backend = _create_backend (interface_index, on_result);
			}

			~Monitor () {
				_destroy_backend (backend);
			}

			private void on_result (void * result) {
				parent.on_result (result, interface_address);
			}

			public extern static void * _create_backend (ulong interface_index, ResultCallback callback);
			public extern static void _destroy_backend (void * backend);
		}

		public extern static void _enumerate_network_interfaces (NetifFoundFunc func);

		public delegate void NetifFoundFunc (ulong index, string identifier, owned InetSocketAddress address);
		public delegate void ResultCallback (void * result);
	}
}
