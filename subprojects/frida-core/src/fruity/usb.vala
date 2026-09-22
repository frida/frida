[CCode (gir_namespace = "FridaFruity", gir_version = "1.0")]
namespace Frida.Fruity {
	internal sealed class UsbDevice : Object {
		public string udid {
			get;
			construct;
		}

		public LibUSB.Device? raw_device {
			get {
				return _raw_device;
			}
		}

		public UsbDeviceBackend backend {
			get;
			construct;
		}

		public LibUSB.DeviceHandle? handle {
			get {
				return _handle;
			}
		}

		private LibUSB.Device? _raw_device;
		private LibUSB.DeviceHandle? _handle;
		private uint num_pending_operations;
		private Promise<bool>? pending_operations_completed;

		private enum AppleSpecificRequest {
			GET_MODE = 0x45,
			SET_MODE = 0x52,
		}

		private const string MODE_INITIAL_UNTETHERED	= "3:3:3:0"; // => 5:3:3:0
		private const string MODE_INITIAL_TETHERED	= "4:4:3:4"; // => 5:4:3:4

		public UsbDevice (LibUSB.Device raw_device, UsbDeviceBackend backend) throws Error {
			char serial[LibUSB.DEVICE_STRING_BYTES_MAX + 1];
			var res = raw_device.get_device_string (SERIAL_NUMBER, serial);
			Usb.check (res, "Failed to get serial number");
			serial[res] = '\0';

			Object (
				udid: udid_from_serial_number ((string) serial),
				backend: backend
			);

			_raw_device = raw_device;
		}

		public void ensure_open (Cancellable? cancellable = null) throws Error {
			if (_handle != null)
				return;
			Usb.check (_raw_device.open (out _handle), "Failed to open USB device");
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (num_pending_operations != 0) {
				pending_operations_completed = new Promise<bool> ();
				try {
					yield pending_operations_completed.future.wait_async (cancellable);
				} catch (Error e) {
					assert_not_reached ();
				}
				pending_operations_completed = null;
			}

			_handle = null;
			_raw_device = null;
		}

		public async bool maybe_modeswitch (Cancellable? cancellable) throws Error, IOError {
			uint8 current_mode[4];
			var n = yield control_transfer (
				LibUSB.RequestRecipient.DEVICE | LibUSB.RequestType.VENDOR | LibUSB.EndpointDirection.IN,
				AppleSpecificRequest.GET_MODE,
				0,
				0,
				current_mode,
				1000,
				cancellable);
			string mode = parse_mode (current_mode[:n]);
			bool is_initial_mode = mode == MODE_INITIAL_UNTETHERED || mode == MODE_INITIAL_TETHERED;
			if (!is_initial_mode)
				return false;

			uint8 set_mode_result[1];
			var set_mode_result_size = yield control_transfer (
				LibUSB.RequestRecipient.DEVICE | LibUSB.RequestType.VENDOR | LibUSB.EndpointDirection.IN,
				AppleSpecificRequest.SET_MODE,
				0,
				3,
				set_mode_result,
				1000,
				cancellable);
			if (set_mode_result_size != 1 || set_mode_result[0] != 0x00)
				return false;

			return true;
		}

		private static string parse_mode (uint8[] mode) throws Error {
			var result = new StringBuilder.sized (7);
			foreach (uint8 byte in mode) {
				if (result.len != 0)
					result.append_c (':');
				result.append_printf ("%u", byte);
			}
			return result.str;
		}

		public static string udid_from_serial_number (string serial) {
			if (serial.length == 24)
				return serial[:8] + "-" + serial[8:];
			return serial;
		}

		public async uint16 query_default_language_id (Cancellable? cancellable) throws Error, IOError {
			Bytes language_ids_response = yield read_string_descriptor_bytes (0, 0, cancellable);
			if (language_ids_response.get_size () < sizeof (uint16))
				throw new Error.PROTOCOL ("Invalid language IDs response");
			Buffer language_ids = new Buffer (language_ids_response, LITTLE_ENDIAN);
			return language_ids.read_uint16 (0);
		}

		public async string read_string_descriptor (uint8 index, uint16 language_id, Cancellable? cancellable)
				throws Error, IOError {
			var response = yield read_string_descriptor_bytes (index, language_id, cancellable);
			try {
				var input = new DataInputStream (new MemoryInputStream.from_bytes (response));
				input.byte_order = LITTLE_ENDIAN;

				size_t size = response.get_size ();
				if (size % sizeof (unichar2) != 0)
					throw new Error.PROTOCOL ("Invalid string descriptor");
				size_t n = size / sizeof (unichar2);
				var chars = new unichar2[n];
				for (size_t i = 0; i != n; i++)
					chars[i] = input.read_uint16 ();

				unowned string16 str = (string16) chars;
				return str.to_utf8 ((long) n);
			} catch (GLib.Error e) {
				throw new Error.PROTOCOL ("%s", e.message);
			}
		}

		public async Bytes read_string_descriptor_bytes (uint8 index, uint16 language_id, Cancellable? cancellable)
				throws Error, IOError {
			uint8 response[1024];
			var response_size = yield control_transfer (
				LibUSB.RequestRecipient.DEVICE | LibUSB.RequestType.STANDARD | LibUSB.EndpointDirection.IN,
				LibUSB.StandardRequest.GET_DESCRIPTOR,
				(LibUSB.DescriptorType.STRING << 8) | index,
				language_id,
				response,
				1000,
				cancellable);
			try {
				var input = new DataInputStream (new MemoryInputStream.from_data (response[:response_size]));
				input.byte_order = LITTLE_ENDIAN;

				uint8 length = input.read_byte ();
				if (length < 2)
					throw new Error.PROTOCOL ("Invalid string descriptor length");

				uint8 type = input.read_byte ();
				if (type != LibUSB.DescriptorType.STRING)
					throw new Error.PROTOCOL ("Invalid string descriptor type");

				size_t remainder = response_size - 2;
				length -= 2;
				if (length > remainder)
					throw new Error.PROTOCOL ("Invalid string descriptor length");

				return new Bytes (response[2:2 + length]);
			} catch (GLib.Error e) {
				throw new Error.PROTOCOL ("%s", e.message);
			}
		}

		public async size_t control_transfer (uint8 request_type, uint8 request, uint16 val, uint16 index, uint8[] buffer,
				uint timeout, Cancellable? cancellable) throws Error, IOError {
			var op = backend.allocate_usb_operation ();
			unowned LibUSB.Transfer transfer = op.transfer;
			var ready_closure = new TransferReadyClosure (control_transfer.callback);

			size_t control_setup_size = 8;
			var transfer_buffer = new uint8[control_setup_size + buffer.length];
			LibUSB.Transfer.fill_control_setup (transfer_buffer, request_type, request, val, index, (uint16) buffer.length);
			if ((request_type & LibUSB.EndpointDirection.IN) == 0)
				Memory.copy ((uint8 *) transfer_buffer + control_setup_size, buffer, buffer.length);
			transfer.fill_control_transfer (_handle, transfer_buffer, on_transfer_ready, ready_closure, timeout);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				transfer.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			try {
				Usb.check (transfer.submit (), "Failed to submit control transfer");
				on_operation_started ();
				yield;
				on_operation_ended ();
			} finally {
				cancel_source.destroy ();
			}

			Usb.check_transfer (transfer.status, "Control transfer failed");

			var n = transfer.actual_length;

			if ((request_type & LibUSB.EndpointDirection.IN) != 0)
				Memory.copy (buffer, transfer.control_get_data (), n);

			return n;
		}

		public async size_t bulk_transfer (uint8 endpoint, uint8[] buffer, uint timeout, Cancellable? cancellable)
				throws Error, IOError {
			var op = backend.allocate_usb_operation ();
			unowned LibUSB.Transfer transfer = op.transfer;
			var ready_closure = new TransferReadyClosure (bulk_transfer.callback);

			transfer.fill_bulk_transfer (_handle, endpoint, buffer, on_transfer_ready, ready_closure, timeout);

			var cancel_source = new CancellableSource (cancellable);
			cancel_source.set_callback (() => {
				transfer.cancel ();
				return Source.REMOVE;
			});
			cancel_source.attach (MainContext.get_thread_default ());

			try {
				Usb.check (transfer.submit (), "Failed to submit bulk transfer");
				on_operation_started ();
				yield;
				on_operation_ended ();
			} finally {
				cancel_source.destroy ();
			}

			Usb.check_transfer (transfer.status, "Bulk transfer failed");

			return transfer.actual_length;
		}

		private void on_operation_started () {
			num_pending_operations++;
		}

		private void on_operation_ended () {
			num_pending_operations--;
			if (num_pending_operations == 0 && pending_operations_completed != null)
				pending_operations_completed.resolve (true);
		}

		private static void on_transfer_ready (LibUSB.Transfer transfer) {
			TransferReadyClosure * closure = transfer.user_data;
			closure->schedule ();
		}

		private class TransferReadyClosure {
			private SourceFunc? handler;
			private MainContext main_context;

			public TransferReadyClosure (owned SourceFunc handler) {
				this.handler = (owned) handler;
				main_context = MainContext.ref_thread_default ();
			}

			public void schedule () {
				var source = new IdleSource ();
				source.set_callback (() => {
					handler ();
					handler = null;
					return Source.REMOVE;
				});
				source.attach (main_context);
			}
		}
	}

	internal interface UsbDeviceBackend : Object {
		public abstract UsbOperation allocate_usb_operation () throws Error;
	}

	internal interface UsbOperation : Object {
		public abstract LibUSB.Transfer transfer {
			get;
		}
	}

	namespace Usb {
		internal static void check (LibUSB.Error error, string prefix) throws Error {
			if (error >= LibUSB.Error.SUCCESS)
				return;

			string message = @"$prefix: $(error.get_description ())";

			switch (error) {
				case ACCESS:
					throw new Error.PERMISSION_DENIED ("%s", message);
				case NOT_FOUND:
					throw new Error.INVALID_OPERATION ("%s", message);
				case TIMEOUT:
					throw new Error.TIMED_OUT ("%s", message);
				default:
					throw new Error.TRANSPORT ("%s", message);
			}
		}

		internal static void check_transfer (LibUSB.TransferStatus status, string prefix) throws Error, IOError {
			if (status == COMPLETED)
				return;

			string message = @"$prefix: $(status.to_string ())";

			switch (status) {
				case TIMED_OUT:
					throw new Error.TIMED_OUT ("%s", message);
				case CANCELLED:
					throw new IOError.CANCELLED ("%s", message);
				default:
					throw new Error.TRANSPORT ("%s", message);
			}
		}
	}
}
