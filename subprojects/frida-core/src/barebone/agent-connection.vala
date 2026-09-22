[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	public sealed class AgentConnection : Object, AsyncInitable {
		public signal void progress (string status, double fraction);
		public signal void script_message (uint pid, AgentScriptId id, string json, Bytes? data);

		private Cancellable io_cancellable = new Cancellable ();

		private IOStream hostlink;
		private QmpClient? qmp;
		private BufferedInputStream input;
		private OutputStream output;

		private ByteOrder byte_order;
		private uint pointer_size;

		private BareboneInjectedAgentConfig agent_config;
		private string? hostlink_socket_path;
		private BareboneVsockPipeTransportConfig? pipe_vsock_transport;
		private SocketService? pipe_vsock_service;
		private IOStream? pipe_vsock_stream;
		private SourceFunc? pipe_vsock_accept_handler;
		private BareboneImageConfig? image_config;
		private BareboneKernelKind kernel_kind;
		private KernelRelocation? relocation;
		private uint64 kernel_base;
		private Machine machine;
		private Allocator allocator;
		private Gee.List<ModuleInfo> kernel_modules;
		private Gee.List<SymbolInfo> kernel_symbols;

		private KernelFlavor flavor;
		private Allocation elf_allocation;
		private uint64 left_flag;
		private uint64 fault_record;
		private Gee.List<AgentSymbol> agent_symbols = new Gee.ArrayList<AgentSymbol> ();
		private Gee.Map<string, SymbolInfo> resolved_symbols;
		private Allocation config_allocation;

		private Promise<bool>? listening;

		private Gee.Map<uint16, Promise<Variant>> pending_requests = new Gee.HashMap<uint16, Promise<Variant>> ();
		private uint16 next_request_id = 1;

		private const int COMMAND_TIMEOUT_MS = 25000;
		private const uint INJECT_MAX_ATTEMPTS = 100;
		private const uint INJECT_POLL_INTERVAL_MS = 100;
		private const uint LEAVE_MAX_ATTEMPTS = 40;
		private const uint LEAVE_INTERVAL_MS = 50;
		private const size_t FAULT_RECORD_SIZE = 32;
		private const uint GREETING_TIMEOUT_MS = 20000;

		public AgentConnection (BareboneInjectedAgentConfig agent_config, BareboneImageConfig? image_config,
				BareboneKernelKind kernel_kind, KernelRelocation? relocation, uint64 kernel_base, Machine machine,
				Allocator allocator, Gee.List<ModuleInfo> kernel_modules, Gee.List<SymbolInfo> kernel_symbols) {
			this.agent_config = agent_config;
			this.image_config = image_config;
			this.kernel_kind = kernel_kind;
			this.relocation = relocation;
			this.kernel_base = kernel_base;
			this.machine = machine;
			this.allocator = allocator;
			this.kernel_modules = kernel_modules;
			this.kernel_symbols = kernel_symbols;
		}

		public async void open (Cancellable? cancellable) throws Error, IOError {
			try {
				yield init_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
				throw_api_error (e);
			}
		}

		/**
		 * Attaches to an agent already resident in the target, which brought its own
		 * transport with it, such as the Linux kernel module exposing /dev/frida.
		 */
		public static async AgentConnection open_resident (IOStream stream, Cancellable? cancellable)
				throws Error, IOError {
			var connection = new AgentConnection.resident ();

			connection.adopt_hostlink_streams (stream);
			connection.process_incoming_messages.begin ();

			return connection;
		}

		private AgentConnection.resident () {
			byte_order = ByteOrder.HOST;
			pointer_size = (uint) sizeof (void *);
		}

		private const uint8 TRANSPORT_KIND_VIRTIO = 0;
		private const uint8 TRANSPORT_KIND_VSOCK = 1;
		private const uint8 TRANSPORT_KIND_VIRTIO_PCI = 2;
		private const uint8 TRANSPORT_KIND_SERIAL = 3;
		private const uint8 TRANSPORT_KIND_PIPE_VSOCK = 4;

		private async bool init_async (int io_priority, Cancellable? cancellable) throws Error, IOError {
			progress ("Preparing the agent", 0.0);

			var transport_tag = yield resolve_transport (cancellable);

			var gdb = machine.gdb;
			byte_order = gdb.byte_order;
			pointer_size = gdb.pointer_size;

			// Only a kernel shipped as an image carries its symbols in it; the rest name theirs
			// elsewhere, and the layout is collected before this.
			bool image_carries_symbols = image_config != null && kernel_kind != LINUX;

			Layout layout;
			uint64 preferred_base = 0;
			if (image_carries_symbols) {
				var payload = yield Img4.parse_file (File.new_for_path (image_config.file), cancellable);
				var kernelcache = Layout.parse_kernelcache (payload.data);
				preferred_base = kernelcache.preferred_address;
				if (relocation != null) {
					layout = Layout.load_from_module (kernelcache, payload.data, preferred_base, byte_order,
						pointer_size);
					rebase_layout (layout, preferred_base, relocation);
				} else {
					layout = Layout.load_from_module (kernelcache, payload.data, kernel_base, byte_order,
						pointer_size);
				}
			} else {
				layout = new Layout.empty ();
			}

			var symbols = new Gee.HashMap<string, SymbolInfo> ();
			var hash_builder = new SymbolHashBuilder ();
			foreach (var s in layout.symbols) {
				symbols[s.name] = s;
				hash_builder.add_symbol (s);
			}
			if (image_carries_symbols) {
				foreach (var e in image_config.symbols.entries) {
					unowned string name = e.key;
					if (!symbols.has_key (name)) {
						uint32 offset = (relocation != null)
							? relocation.runtime_offset (preferred_base + e.value)
							: (uint32) e.value;
						var s = new SymbolInfo () {
							name = name,
							offset = offset,
							symbol_type = 0xf,
							section = 0x10, // FIXME
						};
						symbols[name] = s;
						hash_builder.add_symbol (s);
					}
				}
			}

			BareboneKernelKind kind = kernel_kind;
			if (kind == AUTO)
				kind = (image_config != null) ? BareboneKernelKind.XNU : BareboneKernelKind.BARE;

			foreach (var s in kernel_symbols) {
				symbols[s.name] = s;
				hash_builder.add_symbol (s);
			}

			KernelFlavor flavor;
			if (kind == XNU) {
				if (kernel_base == 0)
					throw new Error.NOT_SUPPORTED ("Missing kernel_base");
				flavor = new XnuKernelFlavor (machine, kernel_base, symbols);
			} else if (kind == LINUX) {
				if (kernel_base == 0)
					throw new Error.NOT_SUPPORTED ("Missing kernel_base");
				flavor = new LinuxKernelFlavor (machine, kernel_base, allocator, symbols);
			} else if (kind == WIN9X) {
				flavor = new Win9xKernelFlavor (machine, symbols);
			} else if (kind == WINNT) {
				flavor = new WinNtKernelFlavor (machine, symbols);
			} else {
				flavor = new BareKernelFlavor (machine);
			}

			Bytes symbol_data = hash_builder.build (byte_order);

			Gum.ElfModule elf;
			try {
				elf = new Gum.ElfModule.from_blob (agent_config.image);
			} catch (Gum.Error e) {
				throw new Error.INVALID_ARGUMENT ("%s", e.message);
			}

			var raw_elf = gdb.make_buffer (new Bytes (elf.get_file_data ()));
			// Slots whose symbol is absent on this kernel are left zero; the agent's
			// xnu.rs uses Option<fn> and falls back across per-kernel name variants.
			elf.enumerate_symbols (s => {
				unowned Gum.ElfSectionDetails? sect = s.section;
				if (sect != null && sect.name == ".kernel_addrs") {
					string name = s.name[1:];
					SymbolInfo? info = symbols[name];
					if (info != null) {
						size_t file_offset = (size_t) (sect.offset + (s.address - sect.address));
						raw_elf.write_pointer (file_offset, kernel_base + info.offset);
					}
				}
				return true;
			});

			resolved_symbols = symbols;

			this.flavor = flavor;

			progress ("Waiting for the target to be ready", 0.1);
			yield flavor.prepare (cancellable);

			size_t page_size = yield machine.query_page_size (cancellable);

			progress ("Injecting the agent", 0.2);
			elf_allocation = yield inject_elf (elf, raw_elf.bytes, page_size, machine, allocator,
				uploaded => progress ("Injecting the agent", 0.2 + 0.5 * uploaded), cancellable);

			uint64 start_address = 0;
			uint64 base_va = elf_allocation.virtual_address;
			elf.enumerate_symbols (e => {
				if (e.name == "_start")
					start_address = base_va + e.address;
				else if (e.name == "frida_agent_left")
					left_flag = base_va + e.address;
				else if (e.name == "frida_agent_fault")
					fault_record = base_va + e.address;
				if (e.type == FUNC && e.size != 0)
					agent_symbols.add (new AgentSymbol (e.name, base_va + e.address, e.size));
				return true;
			});
			if (start_address == 0)
				throw new Error.INVALID_ARGUMENT ("Invalid agent: no _start symbol found");

			XnuLayout? xnu_layout = null;
			bool xnu_reads_its_own_processes = symbols.has_key ("proc_pid")
				&& symbols.has_key ("proc_best_name");
			if (kernel_kind == XNU && !xnu_reads_its_own_processes) {
				xnu_layout = yield collect_xnu_layout (machine,
					address_of (symbols, kernel_base, "proc_iterate"),
					address_of (symbols, kernel_base, "proc_find"),
					address_of (symbols, kernel_base, "proc_rele"),
					allocator, cancellable);
			}

			var config_builder = new VariantBuilder (new VariantType ("((tt)yvta(sstttt)aya(st)a(ttu))"));
			config_builder.add ("(tt)", base_va, (uint64) elf_allocation.size);
			config_builder.add_value (transport_tag.get_child_value (0));
			config_builder.add_value (transport_tag.get_child_value (1));
			config_builder.add ("t", kernel_base);

			config_builder.open (new VariantType ("a(sstttt)"));
			foreach (var m in kernel_modules) {
				config_builder.add ("(sstttt)", m.name, m.version, m.offset, m.size,
					(uint64) 0, (uint64) 0);
			}
			foreach (var m in layout.modules) {
				config_builder.add ("(sstttt)",
					m.name,
					m.version,
					m.offset,
					m.size,
					m.start_func_offset,
					m.stop_func_offset
				);
			}
			config_builder.close ();

			config_builder.add_value (Variant.new_from_data (new VariantType ("ay"), symbol_data.get_data (), true,
				symbol_data));

			// What a kernel does not say about itself and the agent cannot work out from where it
			// runs: the host looks once and passes on what it found.
			config_builder.open (new VariantType ("a(st)"));
			if (xnu_layout != null) {
				config_builder.add ("(st)", "process.number", xnu_layout.number_offset);
				config_builder.add ("(st)", "process.name", xnu_layout.name_offset);
			}
			config_builder.close ();

			config_builder.open (new VariantType ("a(ttu)"));
			if (relocation != null) {
				foreach (var segment in relocation.segments)
					config_builder.add ("(ttu)", segment.address, segment.size, segment.protection);
			}
			config_builder.close ();

			var config_blob = config_builder.end ().get_data_as_bytes ();
			config_allocation = yield allocator.allocate (config_blob.get_size (), 8, cancellable);

			yield machine.write_virtual (config_allocation.virtual_address, config_blob.get_data (),
				cancellable);

			yield protect_unless_already (machine, allocator, config_allocation.virtual_address,
				config_allocation.size, READ | WRITE, cancellable);

			var ia32 = machine as IA32Machine;
			uint kernel_arguments = (ia32 != null) ? ia32.arguments_in_registers : 0;
			if (ia32 != null)
				ia32.arguments_in_registers = 0;

			progress ("Starting the agent", 0.75);
			yield machine.invoke (start_address, {
					config_allocation.virtual_address,
					config_allocation.size
				},
				cancellable);

			if (ia32 != null)
				ia32.arguments_in_registers = kernel_arguments;

			progress ("Connecting to the agent", 0.9);
			yield flavor.settle (cancellable);
			if (qmp != null)
				yield qmp.wait_until_hostlink_is_open (cancellable);
			yield establish_hostlink (cancellable);

			process_incoming_messages.begin ();

			yield wait_to_be_greeted (cancellable);

			return true;
		}

		private async void wait_to_be_greeted (Cancellable? cancellable) throws Error, IOError {
			if (listening == null)
				return;

			var timeout = new TimeoutSource (GREETING_TIMEOUT_MS);
			timeout.set_callback (() => {
				if (!listening.future.ready)
					listening.reject (new Error.TIMED_OUT ("The agent never announced itself"));
				return false;
			});
			timeout.attach (MainContext.get_thread_default ());

			try {
				yield listening.future.wait_async (cancellable);
			} finally {
				timeout.destroy ();
			}
		}

		private async Variant resolve_transport (Cancellable? cancellable) throws Error, IOError {
			if (agent_config.transport is BareboneHostlinkTransportConfig)
				return yield connect_virtio_transport ((BareboneHostlinkTransportConfig) agent_config.transport, cancellable);
			if (agent_config.transport is BareboneVsockTransportConfig) {
				var config = (BareboneVsockTransportConfig) agent_config.transport;
				hostlink_socket_path = config.socket_path;
				return new Variant.tuple ({
					new Variant.byte (TRANSPORT_KIND_VSOCK),
					new Variant.variant (new Variant.uint32 (config.port))
				});
			}
			if (agent_config.transport is BareboneSerialTransportConfig) {
				yield connect_serial_transport ((BareboneSerialTransportConfig) agent_config.transport,
					cancellable);
				return new Variant.tuple ({
					new Variant.byte (TRANSPORT_KIND_SERIAL),
					new Variant.variant (new Variant.uint32 (0))
				});
			}
			if (agent_config.transport is BareboneVsockPipeTransportConfig) {
				var config = (BareboneVsockPipeTransportConfig) agent_config.transport;
				pipe_vsock_transport = config;
				yield open_pipe_vsock_listener (config.socket_path, cancellable);
				return new Variant.tuple ({
					new Variant.byte (TRANSPORT_KIND_PIPE_VSOCK),
					new Variant.variant (new Variant.string (config.socket_path))
				});
			}
			throw new Error.NOT_SUPPORTED ("Unsupported transport config");
		}

		private async void connect_serial_transport (BareboneSerialTransportConfig config,
				Cancellable? cancellable) throws Error, IOError {
#if WINDOWS
			throw new Error.NOT_SUPPORTED ("Serial transport is not available on this OS");
#else
			var client = new SocketClient ();
			try {
				adopt_hostlink_streams (yield client.connect_async (new UnixSocketAddress (config.path),
					cancellable));
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to reach the serial port of the guest: %s", e.message);
			}

			listening = new Promise<bool> ();
#endif
		}

		private async void open_pipe_vsock_listener (string path, Cancellable? cancellable) throws Error, IOError {
#if !WINDOWS
			Posix.unlink (path);
			var service = new SocketService ();
			try {
				SocketAddress effective;
				service.add_address (new UnixSocketAddress (path), STREAM, DEFAULT, null, out effective);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("Unable to listen on %s: %s", path, e.message);
			}
			service.incoming.connect ((connection) => {
				if (pipe_vsock_stream == null) {
					pipe_vsock_stream = connection;
					if (pipe_vsock_accept_handler != null) {
						var handler = (owned) pipe_vsock_accept_handler;
						pipe_vsock_accept_handler = null;
						handler ();
					}
				}
				return true;
			});
			service.start ();
			pipe_vsock_service = service;
#else
			throw new Error.NOT_SUPPORTED ("Pipe-vsock transport is not available on this OS");
#endif
		}

		private async void establish_hostlink (Cancellable? cancellable) throws Error, IOError {
			if (pipe_vsock_transport != null) {
				while (pipe_vsock_stream == null) {
					pipe_vsock_accept_handler = establish_hostlink.callback;
					yield;
				}
				adopt_hostlink_streams (pipe_vsock_stream);
				return;
			}

			if (hostlink_socket_path == null)
				return;

			var address = new UnixSocketAddress (hostlink_socket_path);
			var client = new SocketClient ();
			while (true) {
				try {
					adopt_hostlink_streams (yield client.connect_async (address, cancellable));
					return;
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED)
						throw (IOError) e;
					var source = new TimeoutSource (50);
					source.set_callback (establish_hostlink.callback);
					source.attach (MainContext.get_thread_default ());
					yield;
				}
			}
		}

		private async Variant connect_virtio_transport (BareboneHostlinkTransportConfig config, Cancellable? cancellable)
				throws Error, IOError {
			var qmp = yield QmpClient.open (config.qmp, 0, cancellable);
			var link = yield qmp.open_hostlink (config.bus, cancellable);
			this.qmp = qmp;
			adopt_hostlink_streams (link.connection);

			if (config.fabric is BareboneHostlinkMmioFabric)
				return describe_virtio (link.mmio, link.irq);

			uint64 ecam = 0;
			var ecam_fabric = config.fabric as BareboneHostlinkEcamFabric;
			if (ecam_fabric != null)
				ecam = ecam_fabric.ecam;

			Variant[] pci_cfg = { new Variant.uint64 (ecam) };
			return new Variant.tuple ({
				new Variant.byte (TRANSPORT_KIND_VIRTIO_PCI),
				new Variant.variant (new Variant.tuple (pci_cfg))
			});
		}

		private Variant describe_virtio (uint64 mmio, uint irq) {
			Variant[] virtio_cfg = { new Variant.uint64 (mmio), new Variant.uint32 (irq) };
			return new Variant.tuple ({
				new Variant.byte (TRANSPORT_KIND_VIRTIO),
				new Variant.variant (new Variant.tuple (virtio_cfg))
			});
		}

		private void adopt_hostlink_streams (IOStream connection) {
			hostlink = connection;
			input = (BufferedInputStream) Object.new (typeof (BufferedInputStream),
				"base-stream", hostlink.get_input_stream (),
				"close-base-stream", false,
				"buffer-size", 128 * 1024);
			output = hostlink.get_output_stream ();
		}

		private void rebase_layout (Layout layout, uint64 preferred_base, KernelRelocation reloc) throws Error {
			foreach (var s in layout.symbols)
				s.offset = reloc.runtime_offset (preferred_base + s.offset);
			foreach (var m in layout.modules) {
				m.offset = reloc.runtime_offset (preferred_base + m.offset);
				m.start_func_offset = reloc.runtime_offset (preferred_base + m.start_func_offset);
				m.stop_func_offset = reloc.runtime_offset (preferred_base + m.stop_func_offset);
			}
		}

		public async void close (Cancellable? cancellable) throws IOError {
			try {
				yield execute_command (Command.STOP, new Variant.boolean (true), cancellable);
				if (flavor == null || flavor.stays_attached)
					yield wait_for_agent_to_leave (cancellable);
			} catch (GLib.Error e) {
			}

			io_cancellable.cancel ();

			if (reading != null) {
				try {
					yield reading.future.wait_async (null);
				} catch (GLib.Error e) {
				}
			}

			try {
				yield hostlink.close_async (Priority.DEFAULT, cancellable);
			} catch (GLib.Error e) {
			}

			if (qmp != null) {
				var monitor = qmp;
				qmp = null;
				yield monitor.close (cancellable);
			}
		}

		private async void give_the_memory_back (Cancellable? cancellable) throws Error, IOError {
			var timeout = new TimeoutSource (LEAVE_INTERVAL_MS);
			timeout.set_callback (give_the_memory_back.callback);
			timeout.attach (MainContext.get_thread_default ());
			yield;
			timeout.destroy ();

			yield machine.gdb.stop (cancellable);

			yield flavor.prepare (cancellable);

			{
				try {
					yield protect_unless_already (machine, allocator, elf_allocation.virtual_address,
						elf_allocation.size, READ | WRITE, cancellable);

					yield elf_allocation.deallocate (cancellable);
				} catch (GLib.Error e) {
				}
			}

			yield flavor.settle (cancellable);
		}

		private async void wait_for_agent_to_leave (Cancellable? cancellable) throws Error, IOError {
			if (left_flag == 0)
				return;

			var gdb = machine.gdb;
			for (uint attempt = 0; attempt != LEAVE_MAX_ATTEMPTS; attempt++) {
				yield gdb.stop (cancellable);
				var flag = gdb.make_buffer (yield gdb.read_byte_array (left_flag, 4, cancellable));
				bool left = flag.read_uint32 (0) != 0;
				yield gdb.continue (cancellable);
				if (left) {
					yield give_the_memory_back (cancellable);
					return;
				}

				var timeout = new TimeoutSource (LEAVE_INTERVAL_MS);
				timeout.set_callback (wait_for_agent_to_leave.callback);
				timeout.attach (MainContext.get_thread_default ());
				yield;
				timeout.destroy ();
			}
		}

		/**
		 * Places a second, separately relocated copy of the agent where ring 3 can run it. The
		 * halves cannot share one image: its globals are one instance, so the user-mode side
		 * would tear down the state the kernel side is running on.
		 */
		// Each process needs its own copy, because the copies share the arena and would write the
		// same data. The process receives the code as it is, thus only the writable half needs a new
		// location, in memory that both sides can address.
		public async void place_winnt_user_agent (uint pid, Cancellable? cancellable)
				throws Error, IOError {
			var response = yield execute_command (Command.PLACE_AGENT_IN_PROCESS,
				new Variant.uint32 (pid), cancellable);
			if (!response.is_of_type (VariantType.UINT32))
				throw new Error.PROTOCOL ("Invalid place_agent_in_process response format");
			if (response.get_uint32 () == 0)
				throw new Error.NOT_SUPPORTED ("Unable to place the agent in the process");
		}

		/**
		 * Puts a copy of the agent in a process and starts it, answering with the process the
		 * copy reports having woken up in.
		 */
		public async uint inject_agent_into_process (uint pid, Cancellable? cancellable)
				throws Error, IOError {
			if (kernel_kind == WINNT) {
				yield place_winnt_user_agent (pid, cancellable);
				return yield start_winnt_agent_in_process (pid, cancellable);
			}

			return yield inject_into_process (pid, cancellable);
		}

		// The copy cannot answer from the thread that starts it. Thus ask again until it reports its
		// process id.
		private async uint start_winnt_agent_in_process (uint pid, Cancellable? cancellable)
				throws Error, IOError {
			for (uint attempt = 0; attempt != INJECT_MAX_ATTEMPTS; attempt++) {
				var response = yield execute_command (Command.START_AGENT_IN_PROCESS,
					new Variant.uint32 (pid), cancellable);
				if (!response.is_of_type (VariantType.UINT32))
					throw new Error.PROTOCOL ("Invalid start_agent_in_process response format");

				uint reached = response.get_uint32 ();
				if (reached != 0)
					return reached;

				var timeout = new TimeoutSource (INJECT_POLL_INTERVAL_MS);
				timeout.set_callback (start_winnt_agent_in_process.callback);
				timeout.attach (MainContext.get_thread_default ());
				yield;
				timeout.destroy ();
			}

			throw new Error.TIMED_OUT ("Timed out while starting the agent in the process");
		}

		public signal void spawn_added (uint pid, string command_line, uint holder_pid);

		public async Application[] enumerate_applications (string[] identifiers, Cancellable? cancellable)
				throws Error, IOError {
			var selected = new VariantBuilder (new VariantType ("as"));
			foreach (unowned string identifier in identifiers)
				selected.add ("s", identifier);
			var response = yield execute_command (Command.ENUMERATE_APPLICATIONS, selected.end (), cancellable);
			if (!response.check_format_string ("a(sss)", false))
				throw new Error.PROTOCOL ("Invalid enumerate_applications response format");

			var applications = new Application[response.n_children ()];
			for (size_t i = 0; i != applications.length; i++) {
				var entry = response.get_child_value (i);
				string identifier = entry.get_child_value (0).get_string ();
				string path = entry.get_child_value (1).get_string ();
				string description = entry.get_child_value (2).get_string ();

				applications[i] = new Application (identifier, path, description);
			}
			return applications;
		}

		public async Shortcut[] enumerate_shortcuts (uint helper_pid, Cancellable? cancellable)
				throws Error, IOError {
			var response = yield execute_command (Command.ENUMERATE_SHORTCUTS,
				new Variant.boolean (false), cancellable, helper_pid);
			if (!response.check_format_string ("a(ssss)", false))
				throw new Error.PROTOCOL ("Invalid enumerate_shortcuts response format");

			var shortcuts = new Shortcut[response.n_children ()];
			for (size_t i = 0; i != shortcuts.length; i++) {
				var entry = response.get_child_value (i);
				string identifier = entry.get_child_value (0).get_string ();
				string target = entry.get_child_value (1).get_string ();
				string name = entry.get_child_value (2).get_string ();
				string description = entry.get_child_value (3).get_string ();

				shortcuts[i] = new Shortcut (identifier, target, name, description);
			}
			return shortcuts;
		}

		public class Shortcut {
			public string identifier;
			public string target;
			public string name;
			public string description;

			public Shortcut (string identifier, string target, string name, string description) {
				this.identifier = identifier;
				this.target = target;
				this.name = name;
				this.description = description;
			}
		}

		public class Application {
			public string identifier;
			public string path;
			public string description;

			public Application (string identifier, string path, string description) {
				this.identifier = identifier;
				this.path = path;
				this.description = description;
			}
		}

		public bool spawns_by_itself {
			get {
				return kernel_kind == LINUX || kernel_kind == XNU;
			}
		}

		public async void gate_spawns (bool on, Cancellable? cancellable) throws Error, IOError {
			yield execute_command (Command.GATE_SPAWNS, new Variant.boolean (on), cancellable);
		}

		public async uint spawn_process (uint helper_pid, string command_line, Cancellable? cancellable)
				throws Error, IOError {
			return yield ask_for_a_spawn (helper_pid, new Variant.string (command_line), cancellable);
		}

		// A kernel that starts programs itself takes the words as they are, since a line to be
		// split again is what Windows asks for and nothing else does.
		public async uint spawn_program (string[] words, Cancellable? cancellable)
				throws Error, IOError {
			return yield ask_for_a_spawn (0, new Variant.strv (words), cancellable);
		}

		private async uint ask_for_a_spawn (uint helper_pid, Variant payload, Cancellable? cancellable)
				throws Error, IOError {
			var response = yield execute_command (Command.SPAWN_PROCESS, payload, cancellable,
				helper_pid);
			if (!response.is_of_type (VariantType.UINT32))
				throw new Error.PROTOCOL ("Invalid spawn_process response format");

			return response.get_uint32 ();
		}

		public async void resume_process (uint helper_pid, uint pid, Cancellable? cancellable)
				throws Error, IOError {
			yield execute_command (Command.RESUME_PROCESS, new Variant.uint32 (pid), cancellable,
				helper_pid);
		}

		public async void detach_from_process (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield execute_command (Command.DETACH_FROM_PROCESS, new Variant.uint32 (pid), cancellable);
		}

		public async uint inject_into_process (uint pid, Cancellable? cancellable)
				throws Error, IOError {
			// The agent cannot answer from the context that does the work. Thus it reports the result at
			// the next request.
			for (uint attempt = 0; attempt != INJECT_MAX_ATTEMPTS; attempt++) {
				var response = yield execute_command (Command.INJECT_INTO_PROCESS,
					new Variant.uint32 (pid), cancellable);
				if (!response.is_of_type (VariantType.UINT32))
					throw new Error.PROTOCOL ("Invalid inject_into_process response format");

				uint reached = response.get_uint32 ();
				if (reached != 0)
					return reached;

				var timeout = new TimeoutSource (INJECT_POLL_INTERVAL_MS);
				timeout.set_callback (inject_into_process.callback);
				timeout.attach (MainContext.get_thread_default ());
				yield;
				timeout.destroy ();
			}

			throw new Error.TIMED_OUT ("Timed out while injecting into process");
		}

		public async HostProcessInfo[] enumerate_processes (Scope scope, uint[] pids, Cancellable? cancellable)
				throws Error, IOError {
			bool include_icons = scope == FULL;
			var selected = new VariantBuilder (new VariantType ("au"));
			foreach (uint pid in pids)
				selected.add ("u", pid);
			var request = new Variant.tuple ({ new Variant.boolean (include_icons), selected.end () });
			var response = yield execute_command (Command.ENUMERATE_PROCESSES, request, cancellable);
			if (!response.check_format_string ("a(usssaay)", false))
				throw new Error.PROTOCOL ("Invalid enumerate_processes response format");

			var processes = new HostProcessInfo[response.n_children ()];
			for (size_t i = 0; i != processes.length; i++) {
				var entry = response.get_child_value (i);
				var parameters = new HashTable<string, Variant> (str_hash, str_equal);

				unowned string path = entry.get_child_value (1).get_string ();
				if (scope != MINIMAL) {
					parameters["path"] = path;

					unowned string command_line = entry.get_child_value (2).get_string ();
					if (command_line != "")
						parameters["argv"] = argv_from_command_line (command_line);
				}
				if (include_icons)
					parameters["icons"] = icons_from_resources (entry.get_child_value (4));

				unowned string description = entry.get_child_value (3).get_string ();
				string name = (description != "") ? description : basename_of (path);

				processes[i] = HostProcessInfo (entry.get_child_value (0).get_uint32 (), name, parameters);
			}
			return processes;
		}

		private static uint64 address_of (Gee.Map<string, SymbolInfo?> symbols, uint64 kernel_base,
				string name) {
			SymbolInfo? info = symbols[name];
			return (info != null) ? kernel_base + info.offset : 0;
		}

		private static string basename_of (string path) {
			int start = path.last_index_of_char ('\\');
			return (start != -1) ? path[start + 1:] : path;
		}

		// Windows quoting: a run of backslashes only escapes a quote, and then only half of
		// them survive.
		private static Variant argv_from_command_line (string command_line) {
			var argv = new VariantBuilder (new VariantType ("as"));
			var argument = new StringBuilder ();
			bool quoted = false;
			bool present = false;
			uint pending = 0;

			foreach (char c in (char[]) command_line.data) {
				if (c == '\\') {
					pending++;
					continue;
				}

				if (c == '"') {
					for (uint i = 0; i != pending / 2; i++)
						argument.append_c ('\\');
					if (pending % 2 == 1)
						argument.append_c ('"');
					else
						quoted = !quoted;
					pending = 0;
					present = true;
					continue;
				}

				for (uint i = 0; i != pending; i++)
					argument.append_c ('\\');
				pending = 0;

				if ((c == ' ' || c == '\t') && !quoted) {
					if (present)
						argv.add_value (new Variant.string (argument.str));
					argument.truncate ();
					present = false;
					continue;
				}

				argument.append_c (c);
				present = true;
			}

			for (uint i = 0; i != pending; i++)
				argument.append_c ('\\');
			if (present || argument.len != 0)
				argv.add_value (new Variant.string (argument.str));

			return argv.end ();
		}

		private static Variant icons_from_resources (Variant resources) {
			var icons = new VariantBuilder (new VariantType ("aa{sv}"));
			foreach (var resource in resources) {
				var icon = icon_from_resource (resource.get_data_as_bytes ().get_data ());
				if (icon != null)
					icons.add_value (icon);
			}
			return icons.end ();
		}

		private static Variant? icon_from_resource (uint8[] dib) {
			if (dib.length < BITMAP_INFO_HEADER_SIZE)
				return null;

			uint32 width = read_uint32 (dib, 4);
			uint32 height = read_uint32 (dib, 8) / 2;
			uint16 depth = read_uint16 (dib, 14);
			if (width == 0 || width > MAX_ICON_DIMENSION || height == 0 || height > MAX_ICON_DIMENSION)
				return null;

			uint32 palette_size = read_uint32 (dib, 32);
			if (palette_size == 0 && depth <= 8)
				palette_size = 1u << depth;
			size_t palette = BITMAP_INFO_HEADER_SIZE;
			size_t colors = palette + palette_size * 4;

			size_t color_stride = stride_of (width * depth);
			size_t mask = colors + color_stride * height;
			size_t mask_stride = stride_of (width);
			if (mask + mask_stride * height > dib.length)
				return null;

			var image = new uint8[width * height * 4];
			for (uint32 y = 0; y != height; y++) {
				size_t color_row = colors + color_stride * (height - 1 - y);
				size_t mask_row = mask + mask_stride * (height - 1 - y);
				for (uint32 x = 0; x != width; x++) {
					uint8 red, green, blue, alpha;
					read_color (dib, color_row, x, depth, palette, out red, out green, out blue,
						out alpha);

					bool masked = (dib[mask_row + x / 8] & (0x80 >> (int) (x % 8))) != 0;

					size_t pixel = (y * width + x) * 4;
					image[pixel + 0] = red;
					image[pixel + 1] = green;
					image[pixel + 2] = blue;
					image[pixel + 3] = masked ? 0 : alpha;
				}
			}

			var icon = new VariantBuilder (VariantType.VARDICT);
			icon.add ("{sv}", "format", new Variant.string ("rgba"));
			icon.add ("{sv}", "width", new Variant.uint16 ((uint16) width));
			icon.add ("{sv}", "height", new Variant.uint16 ((uint16) height));
			var pixels = new Bytes.take ((owned) image);
			icon.add ("{sv}", "image",
				Variant.new_from_data<Bytes> (new VariantType ("ay"), pixels.get_data (), true, pixels));
			return icon.end ();
		}

		// A 32-bit icon contains its own alpha channel. The one-bit mask of the older format contains
		// no such data.
		private static void read_color (uint8[] dib, size_t row, uint32 x, uint16 depth, size_t palette,
				out uint8 red, out uint8 green, out uint8 blue, out uint8 alpha) {
			if (depth >= 24) {
				size_t pixel = row + x * (depth / 8);
				blue = dib[pixel + 0];
				green = dib[pixel + 1];
				red = dib[pixel + 2];
				alpha = (depth == 32) ? dib[pixel + 3] : 255;
				return;
			}

			alpha = 255;

			size_t bit = x * depth;
			uint8 packed = dib[row + bit / 8];
			uint index = (packed >> (int) (8 - depth - (bit % 8))) & ((1 << depth) - 1);

			size_t entry = palette + index * 4;
			blue = dib[entry + 0];
			green = dib[entry + 1];
			red = dib[entry + 2];
		}

		private static size_t stride_of (uint32 bits) {
			return ((bits + 31) / 32) * 4;
		}

		private static uint32 read_uint32 (uint8[] data, size_t offset) {
			return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
		}

		private static uint16 read_uint16 (uint8[] data, size_t offset) {
			return (uint16) (data[offset] | (data[offset + 1] << 8));
		}

		private const size_t BITMAP_INFO_HEADER_SIZE = 40;
		private const uint32 MAX_ICON_DIMENSION = 256;

		public async AgentScriptId create_script (string source, uint destination, Cancellable? cancellable)
				throws Error, IOError {
			var payload = new Variant ("s", source);
			var response = yield execute_command (Command.CREATE_SCRIPT, payload, cancellable, destination);
			if (!response.check_format_string ("u", false))
				throw new Error.PROTOCOL ("Invalid create_script response format");
			uint32 script_handle;
			response.get ("u", out script_handle);
			return AgentScriptId (script_handle);
		}

		public async void load_script (AgentScriptId script_id, uint destination, Cancellable? cancellable)
				throws Error, IOError {
			var payload = new Variant ("u", script_id.handle);
			yield execute_command (Command.LOAD_SCRIPT, payload, cancellable, destination);
		}

		public async void destroy_script (AgentScriptId script_id, uint destination, Cancellable? cancellable)
				throws Error, IOError {
			var payload = new Variant ("u", script_id.handle);
			yield execute_command (Command.DESTROY_SCRIPT, payload, cancellable, destination);
		}

		public async void post_script_message (AgentScriptId script_id, string message, Bytes? data, uint destination,
				Cancellable? cancellable) throws Error, IOError {
			var payload = new Variant ("(us@ay)", script_id.handle, message,
				new Variant.from_bytes (new VariantType ("ay"), data ?? new Bytes (new uint8[0]), true));
			yield execute_command (Command.POST_SCRIPT_MESSAGE, payload, cancellable, destination);
		}

		// Zero selects the agent in the kernel. A different value selects a process that has a copy.
		private async Variant execute_command (Command command, Variant payload, Cancellable? cancellable,
				uint destination = 0) throws Error, IOError {
			uint16 request_id = next_request_id++;

			Bytes frame = frame_message (command, request_id, destination, payload);

			var promise = new Promise<Variant> ();
			pending_requests[request_id] = promise;

			try {
				yield write_frame (frame, cancellable);
			} catch (GLib.Error e) {
				pending_requests.unset (request_id);
				throw new Error.TRANSPORT ("%s", e.message);
			}

			var timeout_source = new TimeoutSource (COMMAND_TIMEOUT_MS);
			timeout_source.set_callback (() => {
				Promise<Variant>? p;
				if (pending_requests.unset (request_id, out p))
					p.reject (new Error.TIMED_OUT ("The %s command timed out", command.to_nick ()));
				return Source.REMOVE;
			});
			timeout_source.attach (MainContext.get_thread_default ());

			Variant response = null;
			try {
				response = yield promise.future.wait_async (cancellable);
			} catch (Error e) {
				if (!(e is Error.TIMED_OUT))
					throw e;
				string? fault = yield describe_agent_fault (cancellable);
				if (fault == null)
					throw e;
				throw new Error.TIMED_OUT ("%s; %s", e.message, fault);
			} finally {
				timeout_source.destroy ();
			}

			if (!response.check_format_string ("(bv)", false))
				throw new Error.PROTOCOL ("Malformed reply from agent");
			bool succeeded;
			Variant detail;
			response.get ("(bv)", out succeeded, out detail);
			if (!succeeded)
				throw new Error.NOT_SUPPORTED ("%s", detail.get_string ());

			return detail;
		}

		private async string? describe_agent_fault (Cancellable? cancellable) throws Error, IOError {
			if (fault_record == 0)
				return null;

			var gdb = machine.gdb;
			Buffer record;
			try {
				yield gdb.stop (cancellable);
				record = gdb.make_buffer (yield gdb.read_byte_array (fault_record, FAULT_RECORD_SIZE,
					cancellable));
				yield gdb.continue (cancellable);
			} catch (GLib.Error e) {
				return null;
			}

			if (record.read_uint64 (0) == 0)
				return null;

			return ("the agent took an unhandled fault: vector 0x%" + uint64.FORMAT_MODIFIER
				+ "x at %s, accessing 0x%" + uint64.FORMAT_MODIFIER + "x").printf (
					record.read_uint64 (8), describe_program_counter (record.read_uint64 (16)),
					record.read_uint64 (24));
		}

		private string describe_program_counter (uint64 pc) {
			foreach (var symbol in agent_symbols) {
				if (pc >= symbol.address && pc - symbol.address < symbol.size) {
					return ("%s+0x%" + uint64.FORMAT_MODIFIER + "x").printf (symbol.name,
						pc - symbol.address);
				}
			}
			return ("pc 0x%" + uint64.FORMAT_MODIFIER + "x").printf (pc);
		}

		private class AgentSymbol {
			public string name;
			public uint64 address;
			public uint64 size;

			public AgentSymbol (string name, uint64 address, uint64 size) {
				this.name = name;
				this.address = address;
				this.size = size;
			}
		}

		private async void process_incoming_messages () {
			reading = new Promise<bool> ();
			try {
				while (true) {
					size_t header_size = 4;
					if (input.get_available () < header_size)
						yield fill_until_n_bytes_available (header_size);

					uint32 body_size = 0;
					unowned uint8[] size_buf = ((uint8[]) &body_size)[:4];
					input.peek (size_buf);
					body_size = uint32.from_little_endian (body_size);

					size_t full_size = header_size + body_size;
					if (input.get_available () < full_size)
						yield fill_until_n_bytes_available (full_size);

					var body = new uint8[body_size];
					input.peek (body, header_size);

					input.skip (full_size, io_cancellable);

					var raw_message = new Bytes.take ((owned) body);

					var message = Variant.new_from_data (new VariantType ("(yquv)"), raw_message.get_data (), false,
						raw_message);
					if (byte_order != ByteOrder.HOST)
						message = message.byteswap ();
					if (!message.check_format_string ("(yquv)", false))
						throw new Error.PROTOCOL ("Invalid message format");

					uint8 command_code;
					uint16 request_id;
					uint32 destination;
					Variant payload;
					message.get ("(yquv)", out command_code, out request_id, out destination, out payload);

					if (command_code == Command.READY) {
						if (listening != null && !listening.future.ready)
							listening.resolve (true);
					} else if (command_code == Command.SPAWN_ADDED) {
						if (!payload.check_format_string ("(us)", false))
							throw new Error.PROTOCOL ("Invalid spawn added payload format");

						uint32 pid;
						unowned string command_line;
						payload.get ("(u&s)", out pid, out command_line);

						spawn_added (pid, command_line, destination);
					} else if (command_code == Command.SCRIPT_MESSAGE) {
						if (!payload.check_format_string ("(usay)", false))
							throw new Error.PROTOCOL ("Invalid script message payload format");

						uint32 script_handle;
						unowned string json;
						Variant blob;
						payload.get ("(u&s@ay)", out script_handle, out json, out blob);

						Bytes? data = null;
						if (blob.get_size () != 0)
							data = blob.get_data_as_bytes ();

						script_message (destination, AgentScriptId (script_handle), json, data);
					} else if (command_code == Command.REMAP_WRITABLE_PAGES ||
							command_code == Command.MEMORY_PROTECT ||
							command_code == Command.PATCH_CODE) {
						bool was_running = yield halt_guest (machine.gdb, io_cancellable);

						Variant result;
						try {
							if (command_code == Command.REMAP_WRITABLE_PAGES)
								result = yield remap_writable_pages (payload, io_cancellable);
							else if (command_code == Command.MEMORY_PROTECT)
								result = yield protect_memory (payload, io_cancellable);
							else
								result = yield patch_code (payload, io_cancellable);
						} catch (Error e) {
							result = (command_code == Command.REMAP_WRITABLE_PAGES)
								? new Variant.uint64 (0)
								: new Variant.boolean (false);
						}

						yield resume_guest (machine.gdb, was_running, io_cancellable);

						yield send_reply (request_id, result);
					} else if (command_code == Command.REPLY) {
						Promise<Variant>? promise;
						if (pending_requests.unset (request_id, out promise))
							promise.resolve (payload);
					}
				}
			} catch (GLib.Error e) {
			}

			reading.resolve (true);
		}

		private Promise<bool>? reading;

		private async void fill_until_n_bytes_available (size_t minimum) throws Error, IOError {
			size_t available = input.get_available ();
			while (available < minimum) {
				if (input.get_buffer_size () < minimum)
					input.set_buffer_size (minimum);

				ssize_t n;
				try {
					n = yield input.fill_async ((ssize_t) (input.get_buffer_size () - available), Priority.DEFAULT,
						io_cancellable);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("Connection closed");
				}

				if (n == 0)
					throw new Error.TRANSPORT ("Connection closed");

				available += n;
			}
		}

		private async Variant remap_writable_pages (Variant payload, Cancellable? cancellable) throws Error, IOError {
			var physical_addresses = new Gee.ArrayList<uint64?> ();
			for (size_t i = 0; i != payload.n_children (); i++) {
				uint64 va = payload.get_child_value (i).get_uint64 ();
				physical_addresses.add (yield machine.translate_address (va, cancellable));
			}

			Allocation allocation = yield machine.allocate_pages (physical_addresses, cancellable);

			return new Variant.uint64 (allocation.virtual_address);
		}

		private async Variant protect_memory (Variant payload, Cancellable? cancellable) throws Error, IOError {
			uint64 address;
			uint64 size;
			uint32 prot;
			payload.get ("(ttu)", out address, out size, out prot);

			yield machine.protect_pages_leaving_the_flush (address, (size_t) size, (Gum.PageProtection) prot,
				cancellable);

			return new Variant.boolean (true);
		}

		// CTRR/KTRR locks kernel text read-only against the guest CPU, so the agent cannot patch it
		// even through a writable alias. The physical-memory bridge writes the backing store directly,
		// which the lock does not cover, letting us land hooks in kernel and kext text.
		private async Variant patch_code (Variant payload, Cancellable? cancellable) throws Error, IOError {
			uint64 va;
			Variant bytes_value;
			payload.get ("(t@ay)", out va, out bytes_value);
			var data = (uint8[]) bytes_value.get_data_as_bytes ().get_data ();

			var arm64 = machine as Arm64Machine;
			if (arm64 == null) {
				yield machine.write_virtual (va, data, cancellable);
				return new Variant.boolean (true);
			}

			size_t page_size = yield machine.query_page_size (cancellable);
			size_t offset = 0;
			while (offset < data.length) {
				uint64 page_va = va + offset;
				uint64 pa = yield arm64.translate_address (page_va, cancellable);
				size_t chunk = size_t.min (page_size - (size_t) (page_va & (page_size - 1)), data.length - offset);
				yield arm64.write_physical (pa, data[offset : offset + chunk], cancellable);
				offset += chunk;
			}

			return new Variant.boolean (true);
		}

		private async void send_reply (uint16 request_id, Variant payload) throws GLib.Error {
			yield write_frame (frame_message (Command.REPLY, request_id, 0, payload), io_cancellable);
		}

		private async void write_frame (Bytes frame, Cancellable? cancellable) throws GLib.Error {
			var ours = new Promise<bool> ();
			var theirs = write_turn;
			write_turn = ours;

			if (theirs != null) {
				try {
					yield theirs.future.wait_async (null);
				} catch (GLib.Error e) {
				}
			}

			try {
				yield output.write_all_async (frame.get_data (), Priority.DEFAULT, cancellable,
					null);
			} finally {
				ours.resolve (true);
				if (write_turn == ours)
					write_turn = null;
			}
		}

		private Promise<bool>? write_turn;

		private Bytes frame_message (Command command, uint16 request_id, uint destination, Variant payload) {
			var message = new Variant ("(yquv)", (uint8) command, request_id, destination, payload);
			if (byte_order != ByteOrder.HOST)
				message = message.byteswap ();
			var message_bytes = message.get_data_as_bytes ();
			return new BufferBuilder (byte_order, pointer_size)
				.append_uint32 ((uint32) message_bytes.get_size ())
				.append_bytes (message_bytes)
				.build ();
		}

		private enum Command {
			CREATE_SCRIPT = 1,
			LOAD_SCRIPT = 2,
			DESTROY_SCRIPT = 3,
			POST_SCRIPT_MESSAGE = 4,
			REMAP_WRITABLE_PAGES = 5,
			MEMORY_PROTECT = 6,
			PATCH_CODE = 7,
			ENUMERATE_PROCESSES = 8,
			INJECT_INTO_PROCESS = 9,
			DETACH_FROM_PROCESS = 11,
			PLACE_AGENT_IN_PROCESS = 12,
			START_AGENT_IN_PROCESS = 13,
			SPAWN_PROCESS = 14,
			RESUME_PROCESS = 15,
			STOP = 16,
			GATE_SPAWNS = 17,
			ENUMERATE_APPLICATIONS = 18,
			ENUMERATE_SHORTCUTS = 19,
			REPLY = 128,
			SCRIPT_MESSAGE = 129,
			SPAWN_ADDED = 130,
			READY = 131;

			public string to_nick () {
				return Marshal.enum_to_nick<Command> (this);
			}
		}

		private enum Status {
			IDLE,
			BUSY,
			DATA_READY,
			ERROR
		}
	}

	private class SymbolHashBuilder : Object {
		private Gee.Map<string, Gee.List<SymbolInfo>> symbol_table = new Gee.TreeMap<string, Gee.List<SymbolInfo>> ();

		public void add_symbol (SymbolInfo symbol) {
			var symbol_list = symbol_table[symbol.name];
			if (symbol_list == null) {
				symbol_list = new Gee.ArrayList<SymbolInfo> ();
				symbol_table[symbol.name] = symbol_list;
			}
			symbol_list.add (symbol);
		}

		public Bytes build (ByteOrder byte_order) {
			var builder = new BufferBuilder (byte_order);

			var all_symbols = new Gee.ArrayList<SymbolInfo> ();
			foreach (var entry in symbol_table.entries) {
				foreach (var symbol in entry.value)
					all_symbols.add (symbol);
			}

			uint total_symbols = all_symbols.size;
			builder.append_uint32 (total_symbols);

			var name_index_offset = builder.offset;
			builder.skip (total_symbols * 4);

			var addr_index_offset = builder.offset;
			builder.skip (total_symbols * 4);

			var symbol_offsets = new uint32[total_symbols];
			for (uint i = 0; i != total_symbols; i++) {
				var symbol = all_symbols[(int) i];

				builder.align (8);
				symbol_offsets[i] = (uint32) builder.offset;

				builder.append_uint64 (symbol.offset);
				// TODO: Only include details we need.
				builder.append_uint8 (symbol.symbol_type);
				builder.append_uint8 (symbol.section);
				builder.append_uint16 (symbol.description);
				builder.append_string (symbol.name, StringTerminator.NUL);
			}

			for (uint i = 0; i != total_symbols; i++)
				builder.write_uint32 (name_index_offset + (i * 4), symbol_offsets[i]);

			var addr_sorted_symbols = new Gee.ArrayList<int> ();
			for (uint i = 0; i != total_symbols; i++)
				addr_sorted_symbols.add ((int) i);
			addr_sorted_symbols.sort ((a, b) => {
				var symbol_a = all_symbols[a];
				var symbol_b = all_symbols[b];
				if (symbol_a.offset < symbol_b.offset)
					return -1;
				if (symbol_a.offset > symbol_b.offset)
					return 1;
				return 0;
			});

			for (uint i = 0; i != total_symbols; i++) {
				int original_index = addr_sorted_symbols[(int) i];
				uint symbol_data_offset = symbol_offsets[original_index];
				builder.write_uint32 (addr_index_offset + (i * 4), symbol_data_offset);
			}

			return builder.build ();
		}
	}
}
