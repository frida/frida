[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	internal sealed class UsbNcmDriver : Object, AsyncInitable {
		public UsbDevice device {
			get;
			construct;
		}

		public UsbNcmConfig config {
			get;
			construct;
		}

		public VirtualNetworkStack netstack {
			get {
				return _netstack;
			}
		}

		public InetAddress? remote_ipv6_address {
			get {
				return _remote_ipv6_address;
			}
		}

		private uint32 ntb_in_max_size;
		private uint32 ntb_out_max_size;
		private uint16 ndp_out_divisor;
		private uint16 ndp_out_payload_remainder;
		private uint16 ndp_out_alignment;
		private uint16 ntb_out_max_datagrams;
		private size_t max_in_transfers;
		private size_t max_out_transfers;
		private size_t max_out_bytes_in_flight;
		private uint16 ndp_reserved_slots;
		private uint16 ndp_fixed_header_size;

		private VirtualNetworkStack? _netstack;
		private Gee.Queue<Bytes> pending_output = new Gee.ArrayQueue<Bytes> ();
		private size_t out_in_flight_count = 0;
		private size_t out_in_flight_bytes = 0;
		private Gee.Map<Future<uint>, uint16> out_in_flight_sizes = new Gee.HashMap<Future<uint>, uint16> ();
		private bool out_topup_scheduled = false;
		private uint16 next_outgoing_sequence = 1;

		private InetAddress? _remote_ipv6_address;

		private Cancellable io_cancellable = new Cancellable ();

		private const uint16 TRANSFER_HEADER_SIZE = 4 + 2 + 2 + 2 + 2;
		private const size_t MAX_TRANSFER_MEMORY = 60 * 1518;

		private enum CdcRequest {
			GET_NTB_PARAMETERS = 0x80,
			SET_NTB_INPUT_SIZE = 0x86,
		}

		private const uint16 NTB_PARAMETERS_MIN_SIZE = 28;

		private enum EtherType {
			IPV6 = 0x86dd,
		}

		private enum IPV6NextHeader {
			UDP = 0x11,
		}

		public static async UsbNcmDriver open (UsbDevice device, UsbNcmConfig config, Cancellable? cancellable = null)
				throws Error, IOError {
			var driver = new UsbNcmDriver (device, config);

			try {
				yield driver.init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}

			return driver;
		}

		private UsbNcmDriver (UsbDevice device, UsbNcmConfig config) {
			Object (device: device, config: config);
		}

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			var language_id = yield device.query_default_language_id (cancellable);

			uint8 mac_address[6];
			string mac_address_str = yield device.read_string_descriptor (config.mac_address_index, language_id, cancellable);
			if (mac_address_str.length != 12)
				throw new Error.PROTOCOL ("Invalid MAC address");
			for (uint i = 0; i != 6; i++) {
				uint v;
				mac_address_str.substring (i * 2, 2).scanf ("%02X", out v);
				mac_address[i] = (uint8) v;
			}

			unowned LibUSB.DeviceHandle handle = device.handle;
			try {
				Usb.check (handle.claim_interface (config.ctrl_iface), "Failed to claim control interface");
				Usb.check (handle.claim_interface (config.data_iface), "Failed to claim data interface");
			} catch (Error e) {
				throw new Error.PERMISSION_DENIED ("%s",
					make_user_error_message (@"Unable to claim USB CDC-NCM interface ($(e.message))"));
			}

			uint8 raw_ntb_params[NTB_PARAMETERS_MIN_SIZE];
			var raw_ntb_params_size = yield device.control_transfer (
				LibUSB.RequestRecipient.INTERFACE | LibUSB.RequestType.CLASS | LibUSB.EndpointDirection.IN,
				CdcRequest.GET_NTB_PARAMETERS,
				0,
				config.ctrl_iface,
				raw_ntb_params,
				1000,
				cancellable);
			if (raw_ntb_params_size < NTB_PARAMETERS_MIN_SIZE)
				throw new Error.PROTOCOL ("Truncated NTB parameters response");
			var ntb_params = new Buffer (new Bytes (raw_ntb_params[:raw_ntb_params_size]), LITTLE_ENDIAN);
			uint32 device_ntb_in_max_size = ntb_params.read_uint32 (4);
			ntb_in_max_size = uint32.min (device_ntb_in_max_size, 16384);
			ntb_out_max_size = uint32.min (ntb_params.read_uint32 (16), 16384);
			ndp_out_divisor = ntb_params.read_uint16 (20);
			ndp_out_payload_remainder = ntb_params.read_uint16 (22);
			ndp_out_alignment = ntb_params.read_uint16 (24);
			ntb_out_max_datagrams = ntb_params.read_uint16 (26);

			if (ntb_in_max_size != device_ntb_in_max_size) {
				var ntb_size_buf = new BufferBuilder (LITTLE_ENDIAN)
					.append_uint32 (ntb_in_max_size)
					.build ();
				yield device.control_transfer (
					LibUSB.RequestRecipient.INTERFACE | LibUSB.RequestType.CLASS | LibUSB.EndpointDirection.OUT,
					CdcRequest.SET_NTB_INPUT_SIZE,
					0,
					config.ctrl_iface,
					ntb_size_buf.get_data (),
					1000,
					cancellable);
			}

			var speed = device.raw_device.get_device_speed ();
			if (speed >= LibUSB.Speed.SUPER) {
				max_in_transfers = (5 * MAX_TRANSFER_MEMORY) / ntb_in_max_size;
				max_out_transfers = (5 * MAX_TRANSFER_MEMORY) / ntb_out_max_size;
			} else if (speed == LibUSB.Speed.HIGH) {
				max_in_transfers = MAX_TRANSFER_MEMORY / ntb_in_max_size;
				max_out_transfers = (3 * MAX_TRANSFER_MEMORY) / ntb_out_max_size;
				max_out_transfers = size_t.max (max_out_transfers, 6);
				max_out_transfers = size_t.min (max_out_transfers, 16);
			} else {
				max_in_transfers = 4;
				max_out_transfers = 4;
			}

			max_out_bytes_in_flight = 4 * MAX_TRANSFER_MEMORY;

			ndp_reserved_slots = (uint16) uint16.min (21, ntb_out_max_datagrams);
			ndp_fixed_header_size = (uint16) align (8 + (ndp_reserved_slots * 4) + 4, ndp_out_alignment);

			Usb.check (handle.set_interface_alt_setting (config.data_iface, config.data_altsetting),
				"Failed to set USB interface alt setting");

			_netstack = new VirtualNetworkStack (new Bytes (mac_address), null, 1500);
			_netstack.outgoing_datagrams.connect (on_netif_outgoing_datagrams);

			process_incoming_datagrams.begin ();

			return true;
		}

		public void close () {
			io_cancellable.cancel ();
			_netstack.stop ();
		}

		private async void process_incoming_datagrams () {
			var pending = new Gee.ArrayQueue<Promise<Bytes>> ();
			while (true) {
				for (uint i = pending.size; i != max_in_transfers; i++) {
					var request = transfer_next_input_batch ();
					pending.offer (request);
				}

				try {
					var datagrams = new Gee.ArrayList<Bytes> ();
					while (true) {
						var request = pending.peek ();

						if (request == null)
							break;
						if (!datagrams.is_empty && !request.future.ready)
							break;

						var frame = yield pending.poll ().future.wait_async (io_cancellable);
						datagrams.add_all (handle_ncm_frame (frame));
					}

					yield _netstack.handle_incoming_datagrams (datagrams);
				} catch (GLib.Error e) {
					return;
				}
			}
		}

		private Promise<Bytes> transfer_next_input_batch () {
			var request = new Promise<Bytes> ();
			do_transfer_input_batch.begin (request);
			return request;
		}

		private async void do_transfer_input_batch (Promise<Bytes> request) {
			var data = new uint8[ntb_in_max_size];
			try {
				size_t n = yield device.bulk_transfer (config.rx_address, data, uint.MAX, io_cancellable);
				request.resolve (new Bytes (data[:n]));
			} catch (GLib.Error e) {
				request.reject (e);
			}
		}

		private Gee.List<Bytes> handle_ncm_frame (Bytes frame) throws GLib.Error {
			var input = new DataInputStream (new MemoryInputStream.from_bytes (frame));
			input.byte_order = LITTLE_ENDIAN;

			uint8 raw_signature[4 + 1];
			unowned string signature = (string) raw_signature;
			size_t bytes_read;

			input.read_all (raw_signature[:4], out bytes_read);
			if (signature != "NCMH")
				throw new Error.PROTOCOL ("Invalid NTH16 signature");
			input.skip (6);
			var ndp_index = input.read_uint16 ();

			var datagrams = new Gee.ArrayList<Bytes> ();

			do {
				input.seek (ndp_index, SET);
				input.read_all (raw_signature[:4], out bytes_read);
				if (signature != "NCM0")
					throw new Error.PROTOCOL ("Invalid NDP16 signature");
				input.skip (2);
				var next_ndp_index = input.read_uint16 ();

				while (true) {
					var datagram_index = input.read_uint16 ();
					var datagram_length = input.read_uint16 ();
					if (datagram_index == 0 || datagram_length == 0)
						break;

					int64 previous_offset = input.tell ();
					input.seek (datagram_index, SET);
					var datagram_buf = new uint8[datagram_length];
					input.read_all (datagram_buf, out bytes_read);
					input.seek (previous_offset, SET);

					var datagram = new Bytes.take ((owned) datagram_buf);

					if (_remote_ipv6_address == null) {
						_remote_ipv6_address = try_infer_remote_address_from_datagram (datagram);
						if (_remote_ipv6_address != null)
							notify_property ("remote-ipv6-address");
					}

					datagrams.add (datagram);
				}

				ndp_index = next_ndp_index;
			} while (ndp_index != 0);

			return datagrams;
		}

		private void on_netif_outgoing_datagrams (Gee.Collection<Bytes> datagrams) {
			foreach (var d in datagrams)
				pending_output.offer (d);

			schedule_top_up ();
		}

		private void schedule_top_up () {
			if (out_topup_scheduled)
				return;
			out_topup_scheduled = true;

			var source = new IdleSource ();
			source.set_callback (() => {
				out_topup_scheduled = false;
				top_up_output_window ();
				return Source.REMOVE;
			});
			source.attach (MainContext.get_thread_default ());
		}

		private void top_up_output_window () {
			while (!pending_output.is_empty && out_in_flight_count < max_out_transfers) {
				size_t max_datagrams = size_t.min (pending_output.size, (size_t) ndp_reserved_slots);
				var layout = TransferLayout.compute (pending_output, max_datagrams, ntb_out_max_size, ndp_fixed_header_size,
					ndp_out_alignment, ndp_out_divisor, ndp_out_payload_remainder);
				if ((out_in_flight_bytes + layout.size) > max_out_bytes_in_flight)
					break;

				var batch = new Gee.ArrayList<Bytes> ();
				for (var i = 0; i != layout.offsets.length; i++)
					batch.add (pending_output.poll ());
				var transfer = build_output_transfer (batch, layout, next_outgoing_sequence++);

				var request = new Promise<uint> ();
				do_transfer_output_batch.begin (transfer, request);

				out_in_flight_count++;
				out_in_flight_bytes += layout.size;

				out_in_flight_sizes.set (request.future, layout.size);

				request.future.then (fut => {
					on_output_completed (fut);
				});
			}
		}

		private void on_output_completed (Future<uint> fut) {
			uint16 size;
			out_in_flight_sizes.unset (fut, out size);

			out_in_flight_count--;
			out_in_flight_bytes -= size;

			if (fut.error != null)
				return;

			top_up_output_window ();
		}

		private async void do_transfer_output_batch (Bytes transfer, Promise<uint> request) {
			try {
				var size = yield device.bulk_transfer (config.tx_address, transfer.get_data (), uint.MAX, io_cancellable);
				request.resolve ((uint) size);
			} catch (GLib.Error e) {
				request.reject (e);
			}
		}

		private class TransferLayout {
			public uint16 size;
			public uint16 ndp_header_offset;
			public uint16 ndp_header_size;
			public uint16[] offsets;

			public static TransferLayout compute (Gee.Collection<Bytes> datagrams, size_t max_datagrams,
					uint32 max_transfer_size, uint16 ndp_fixed_header_size, size_t ndp_alignment,
					size_t datagram_modulus, size_t datagram_remainder) {
				size_t ethernet_header_size = 14;

				size_t ndp_header_offset = align (TRANSFER_HEADER_SIZE, ndp_alignment, 0);

				size_t current_transfer_size = ndp_header_offset + ndp_fixed_header_size;
				var offsets = new uint16[max_datagrams];

				int i = 0;
				foreach (var datagram in datagrams) {
					var size = (uint16) datagram.get_size ();

					size_t start_offset =
						align (current_transfer_size + ethernet_header_size, datagram_modulus, datagram_remainder);
					size_t end_offset = start_offset + size;
					if (end_offset > uint16.MAX || end_offset > max_transfer_size)
						break;

					current_transfer_size = end_offset;
					offsets[i] = (uint16) start_offset;

					i++;
					if (i == max_datagrams)
						break;
				}

				offsets.resize (i);

				return new TransferLayout () {
					size = (uint16) current_transfer_size,
					ndp_header_offset = (uint16) ndp_header_offset,
					ndp_header_size = ndp_fixed_header_size,
					offsets = (owned) offsets,
				};
			}
		}

		private static Bytes build_output_transfer (Gee.List<Bytes> datagrams, TransferLayout layout, uint16 sequence_number) {
			var builder = new BufferBuilder (LITTLE_ENDIAN)
				.append_string ("NCMH", StringTerminator.NONE)
				.append_uint16 (TRANSFER_HEADER_SIZE)
				.append_uint16 (sequence_number)
				.append_uint16 (layout.size)
				.append_uint16 (layout.ndp_header_offset);

			uint16 next_ndp_index = 0;
			builder
				.seek (layout.ndp_header_offset)
				.append_string ("NCM0", StringTerminator.NONE)
				.append_uint16 (layout.ndp_header_size)
				.append_uint16 (next_ndp_index);

			int i;

			i = 0;
			foreach (var datagram in datagrams) {
				builder
					.append_uint16 (layout.offsets[i])
					.append_uint16 ((uint16) datagram.get_size ());
				i++;
			}

			i = 0;
			foreach (var datagram in datagrams) {
				builder
					.seek (layout.offsets[i])
					.append_bytes (datagram);
				i++;
			}

			builder.seek (layout.size);

			return builder.build ();
		}

		private static InetAddress? try_infer_remote_address_from_datagram (Bytes datagram) {
			if (datagram.get_size () < 0x3e)
				return null;

			var buf = new Buffer (datagram, BIG_ENDIAN);

			var ethertype = (EtherType) buf.read_uint16 (12);
			if (ethertype != IPV6)
				return null;

			var next_header = (IPV6NextHeader) buf.read_uint8 (20);
			if (next_header != UDP)
				return null;

			return new InetAddress.from_bytes (datagram[22:22 + 16].get_data (), IPV6);
		}

		private static size_t align (size_t val, size_t modulus, size_t remainder = 0) {
			var delta = val % modulus;
			if (delta != remainder)
				return val + modulus - delta + remainder;
			return val;
		}
	}

	internal sealed class UsbNcmConfig {
		public uint8 ctrl_iface;
		public uint8 data_iface;
		public int data_altsetting;
		public uint8 rx_address;
		public uint8 tx_address;
		public uint8 mac_address_index;

		private enum UsbDescriptorType {
			INTERFACE = 0x04,
		}

		private enum UsbCommSubclass {
			NCM = 0x0d,
		}

		private enum UsbDataSubclass {
			UNDEFINED = 0x00,
		}

		private enum UsbCdcDescriptorSubtype {
			ETHERNET = 0x0f,
		}

		public static UsbNcmConfig prepare (UsbDevice device, out bool device_configuration_changed) throws Error {
			unowned LibUSB.Device raw_device = device.raw_device;

			var dev_desc = LibUSB.DeviceDescriptor (raw_device);

			LibUSB.ConfigDescriptor current_config;
			Usb.check (raw_device.get_active_config_descriptor (out current_config), "Failed to get active config descriptor");

			var config = new UsbNcmConfig ();
			int desired_config_value = -1;
			bool found_cdc_header = false;
			bool found_data_interface = false;
			for (uint8 config_value = dev_desc.bNumConfigurations; config_value != 0; config_value--) {
				LibUSB.ConfigDescriptor config_desc;
				Usb.check (raw_device.get_config_descriptor_by_value (config_value, out config_desc),
					"Failed to get config descriptor");

				foreach (var iface in config_desc.@interface) {
					foreach (var setting in iface.altsetting) {
						if (setting.bInterfaceClass == LibUSB.ClassCode.COMM &&
								setting.bInterfaceSubClass == UsbCommSubclass.NCM) {
							config.ctrl_iface = setting.bInterfaceNumber;

							try {
								parse_cdc_header (setting.extra, out config.mac_address_index);
								found_cdc_header = true;
							} catch (Error e) {
								break;
							}
						} else if (setting.bInterfaceClass == LibUSB.ClassCode.DATA &&
								setting.bInterfaceSubClass == UsbDataSubclass.UNDEFINED &&
								setting.endpoint.length == 2) {
							found_data_interface = true;

							config.data_iface = setting.bInterfaceNumber;
							config.data_altsetting = setting.bAlternateSetting;

							foreach (var ep in setting.endpoint) {
								if ((ep.bEndpointAddress & LibUSB.EndpointDirection.MASK) ==
										LibUSB.EndpointDirection.IN) {
									config.rx_address = ep.bEndpointAddress;
								} else {
									config.tx_address = ep.bEndpointAddress;
								}
							}
						}
					}
				}

				if (found_cdc_header || found_data_interface) {
					desired_config_value = config_value;
					break;
				}
			}
			if (!found_cdc_header || !found_data_interface)
				throw new Error.NOT_SUPPORTED ("%s", make_user_error_message ("No USB CDC-NCM interface found"));

			if (current_config.bConfigurationValue != desired_config_value) {
				unowned LibUSB.DeviceHandle handle = device.handle;
				foreach (var iface in current_config.@interface) {
					unowned LibUSB.InterfaceDescriptor setting = iface.altsetting[0];
					var res = handle.kernel_driver_active (setting.bInterfaceNumber);
					if (res == 1)
						handle.detach_kernel_driver (setting.bInterfaceNumber);
				}
				Usb.check (handle.set_configuration (desired_config_value), "Failed to set configuration");
				device_configuration_changed = true;
			} else {
				device_configuration_changed = false;
			}

			return config;
		}

		private static void parse_cdc_header (uint8[] header, out uint8 mac_address_index) throws Error {
			var input = new DataInputStream (new MemoryInputStream.from_data (header));
			input.set_byte_order (LITTLE_ENDIAN);

			try {
				for (int offset = 0; offset != header.length;) {
					uint8 length = input.read_byte ();
					if (length < 3)
						throw new Error.PROTOCOL ("Invalid descriptor length");

					uint8 descriptor_type = input.read_byte ();
					if (descriptor_type != (LibUSB.RequestType.CLASS | UsbDescriptorType.INTERFACE))
						throw new Error.PROTOCOL ("Invalid descriptor type");

					uint8 descriptor_subtype = input.read_byte ();
					if (descriptor_subtype == UsbCdcDescriptorSubtype.ETHERNET) {
						mac_address_index = input.read_byte ();
						return;
					}

					input.skip (length - 3);
					offset += length;
				}
			} catch (IOError e) {
				throw new Error.PROTOCOL ("%s", e.message);
			}

			throw new Error.PROTOCOL ("CDC Ethernet descriptor not found");
		}
	}

	private string make_user_error_message (string message) {
#if WINDOWS
			return message + "; use https://zadig.akeo.ie to switch from Apple's official driver onto Microsoft's WinUSB " +
				"driver, so libusb can access it";
#else
			return message;
#endif
	}
}
