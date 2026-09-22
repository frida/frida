namespace Frida {
	public sealed class BareboneHostSessionBackend : Object, HostSessionBackend {
		private BareboneHostSessionProvider? provider;

		public async void start (Cancellable? cancellable) throws IOError {
			provider = new BareboneHostSessionProvider ();
			provider_available (provider);
		}

		public async void stop (Cancellable? cancellable) throws IOError {
			provider = null;
		}
	}

	public sealed class BareboneHostSessionProvider : Object, HostSessionProvider {
		public string id {
			get {
				return "barebone";
			}
		}

		public string name {
			get {
				return "GDB Remote Stub";
			}
		}

		public Variant? icon {
			get {
				return _icon;
			}
		}

		public HostSessionProviderKind kind {
			get {
				return HostSessionProviderKind.REMOTE;
			}
		}

		private static Variant _icon;
		private BareboneHostSession? host_session;
		private EmulatorInstrumentation? emulator_instrumentation;

		static construct {
			_icon = make_provider_icon (Frida.Data.Icons.get_barebone_png_blob ().data);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			if (host_session != null) {
				yield host_session.close (cancellable);
				host_session = null;
			}
			if (emulator_instrumentation != null) {
				yield emulator_instrumentation.tear_down (cancellable);
				emulator_instrumentation = null;
			}
		}

		public async HostSession create (HostSessionHub hub, HostSessionOptions? options, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != null)
				throw new Error.INVALID_OPERATION ("Already created");

			BareboneConfig? config = null;
			if (options != null) {
				Value? val = options.map["config"];
				if (val != null) {
					config = (BareboneConfig) val.get_object ();
					config.check ();
				}
			}

			unowned string? config_path = Environment.get_variable ("FRIDA_BAREBONE_CONFIG");
			if (config == null && config_path != null) {
				try {
					var config_data = yield FS.read_all_text (File.new_for_path (config_path), cancellable);
					var cfg = (BareboneConfig) Json.gobject_from_data (typeof (BareboneConfig), config_data);
					cfg.check ();
					config = cfg;
				} catch (GLib.Error e) {
					throw new Error.INVALID_ARGUMENT ("Unable to load %s: %s", config_path, e.message);
				}
			}

			if (config == null)
				config = new BareboneConfig ();

			var resident_agent = config.agent as BareboneResidentAgentConfig;
			if (resident_agent != null) {
				host_session = yield attach_to_resident_agent (resident_agent.transport, cancellable);
				host_session.agent_session_detached.connect (on_agent_session_detached);

				return host_session;
			}

			SocketConnectable connectable;
			try {
				BareboneConnectionConfig c = config.connection;
				connectable = NetworkAddress.parse (c.host, c.port);
			} catch (GLib.Error e) {
				throw new Error.INVALID_ARGUMENT ("Unable to load %s: %s", config_path, e.message);
			}

			connecting ("Connecting to the GDB remote stub", 0.0);
			IOStream stream;
			try {
				var client = new SocketClient ();
				var connection = yield client.connect_async (connectable, cancellable);

				Tcp.enable_nodelay (connection.socket);

				stream = connection;
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("The specified GDB remote stub cannot be reached: %s", e.message);
			}

			if (config.connection.flavor == BareboneStubFlavor.ANDROID_EMULATOR) {
				connecting ("Instrumenting the emulator", 0.05);
				emulator_instrumentation = yield EmulatorInstrumentation.apply (config, cancellable);
			}

			GDB.Client gdb;
			switch (config.connection.flavor) {
				case BareboneStubFlavor.VZ:
					gdb = yield Barebone.VzStubClient.open (stream, cancellable);
					break;
				case BareboneStubFlavor.PARALLELS:
					gdb = yield Barebone.ParallelsStubClient.open (stream, cancellable);
					break;
				default:
					gdb = yield GDB.Client.open (stream, cancellable);
					break;
			}

			try {
				host_session = yield establish (config, gdb, (owned) connecting, cancellable);
			} catch (GLib.Error e) {
				if (gdb.state == STOPPED) {
					try {
						yield gdb.continue (cancellable);
					} catch (GLib.Error ee) {
					}
				}
				try {
					yield gdb.close (cancellable);
				} catch (IOError ee) {
				}
				throw_api_error (e);
			}
			host_session.agent_session_detached.connect (on_agent_session_detached);

			return host_session;
		}

		private async BareboneHostSession establish (BareboneConfig config, GDB.Client gdb, owned ConnectingFunc connecting,
				Cancellable? cancellable) throws Error, IOError {
			// The arm64 MMU system registers (TTBR1_EL1/TCR_EL1) are exposed by some stubs and not
			// others (the Android emulator's does not); detect it from the advertised register set,
			// which selects host page-table walking versus the kernel's set_memory_* helpers.
			bool mmu_registers_available = gdb.arch != GDB.TargetArch.ARM64
				|| (gdb.has_register ("ttbr1_el1") && gdb.has_register ("tcr_el1"));

			Barebone.Machine machine;
			switch (gdb.arch) {
				case IA32:
					machine = new Barebone.IA32Machine (gdb);
					break;
				case X64:
					machine = new Barebone.X64Machine (gdb) {
						calling_convention = (config.kernel == WINNT)
							? Barebone.CallingConvention.MICROSOFT
							: Barebone.CallingConvention.SYSTEM_V,
					};
					break;
				case ARM:
					machine = new Barebone.ArmMachine (gdb);
					break;
				case ARM64:
					machine = new Barebone.Arm64Machine (gdb) {
						mmu_registers_available = mmu_registers_available,
					};
					break;
				default:
					machine = new Barebone.UnknownMachine (gdb);
					break;
			}

			// Read the daemon's guest-RAM map only once the stub has halted the VM: reading the
			// high-VA backing while the vCPUs run blocks mach_vm_read. The map is dynamic (the daemon
			// reclaims idle RAM), so VzPhysicalMemory re-reads it on demand as the agent maps pages.
#if DARWIN
			if (config.connection.flavor == VZ && machine is Barebone.Arm64Machine)
				((Barebone.Arm64Machine) machine).physical_memory = Barebone.VzPhysicalMemory.open (config.connection.pid);
#endif

			size_t page_size;
			try {
				page_size = yield machine.query_page_size (cancellable);
			} catch (Error e) {
				page_size = 0;
			}

			connecting ("Resolving the kernel layout", 0.1);

			// Resolve the kernel's runtime layout before building the allocator: on a scattered
			// SPTM kernel collection the config-supplied addresses are static and must be translated.
			Barebone.KernelRelocation? relocation = null;
			uint64 kernel_base = 0;
			BareboneImageConfig? image = config.image;
			if (image != null && config.kernel != LINUX) {
				if (image.base != null) {
					kernel_base = image.base.address;
				} else {
					var payload = yield Barebone.Img4.parse_file (File.new_for_path (image.file), cancellable);
					uint64? summaries = image.symbols["gLoadedKextSummaries"];
					relocation = yield Barebone.KernelRelocation.compute (machine, payload.data,
						summaries ?? 0, cancellable);
					kernel_base = relocation.reference_base;
				}
			}

			Gee.List<Barebone.ModuleInfo> kernel_modules = new Gee.ArrayList<Barebone.ModuleInfo> ();
			Gee.List<Barebone.SymbolInfo> kernel_symbols = new Gee.ArrayList<Barebone.SymbolInfo> ();
			if (config.kernel == WIN9X) {
				var win9x_layout = yield Barebone.collect_win9x_layout (machine, cancellable);
				kernel_modules = win9x_layout.modules;
				kernel_symbols = win9x_layout.symbols;
			} else if (config.kernel == WINNT) {
				var winnt_layout = yield Barebone.collect_winnt_layout (machine, cancellable);
				kernel_modules = winnt_layout.modules;
				kernel_symbols = winnt_layout.symbols;
			} else if (config.kernel == LINUX) {
				if (image == null)
					throw new Error.INVALID_ARGUMENT ("Missing image.file naming the kernel's System.map");
				var linux_layout = yield Barebone.collect_linux_layout (machine, image.file, cancellable);
				kernel_base = linux_layout.base_address;
				kernel_modules = linux_layout.modules;
				kernel_symbols = linux_layout.symbols;
			}

			Barebone.Allocator allocator;
			BareboneAllocatorConfig? ac = config.allocator;
			if (ac == null)
				ac = infer_allocator_config (config.kernel, kernel_symbols, kernel_base,
					mmu_registers_available);
			if (ac == null) {
				allocator = new Barebone.NullAllocator (page_size);
			} else if (ac is BarebonePhysicalAllocatorConfig) {
				allocator = new Barebone.PhysicalAllocator (machine, page_size,
					(BarebonePhysicalAllocatorConfig) ac);
			} else if (ac is BareboneTargetFunctionsAllocatorConfig) {
				var tfa = (BareboneTargetFunctionsAllocatorConfig) ac;
				uint64 alloc_function = tfa.alloc_function.address;
				uint64 free_function = tfa.free_function.address;
				if (relocation != null) {
					alloc_function = relocation.translate (alloc_function);
					free_function = relocation.translate (free_function);
				}
				Gum.PageProtection pool_protection = (config.kernel == WINNT || config.kernel == WIN9X)
					? Gum.PageProtection.READ | Gum.PageProtection.WRITE | Gum.PageProtection.EXECUTE
					: Gum.PageProtection.READ | Gum.PageProtection.WRITE;
				allocator = new Barebone.TargetFunctionsAllocator (machine, page_size, pool_protection, tfa,
					alloc_function, free_function);
			} else {
				assert_not_reached ();
			}

			var arm64_machine = machine as Barebone.Arm64Machine;
			if (arm64_machine != null)
				arm64_machine.code_allocator = allocator;

			var arm_machine = machine as Barebone.ArmMachine;
			if (arm_machine != null) {
				Barebone.SymbolInfo? swapper = find_symbol (kernel_symbols, "swapper_pg_dir");
				if (swapper != null)
					arm_machine.kernel_page_table = kernel_base + swapper.offset;
			}

			Barebone.AgentConnection? agent_connection = null;
			var agent_config = config.agent as BareboneInjectedAgentConfig;
			if (agent_config != null) {
				agent_connection = new Barebone.AgentConnection (agent_config, config.image, config.kernel, relocation,
					kernel_base, machine, allocator, kernel_modules, kernel_symbols);
				agent_connection.progress.connect ((status, fraction) => connecting (status, 0.2 + 0.8 * fraction));
				yield agent_connection.open (cancellable);
			}

			var interceptor = new Barebone.Interceptor (machine, allocator);

			var services = new Barebone.Services (machine, allocator, interceptor);

			return new BareboneHostSession (agent_connection, services);
		}

		private async BareboneHostSession attach_to_resident_agent (BareboneResidentTransportConfig transport,
				Cancellable? cancellable) throws Error, IOError {
#if WINDOWS
			throw new Error.NOT_SUPPORTED ("Resident agents are not available on this OS");
#else
			IOStream stream;
			if (transport is BareboneSocketTransportConfig) {
				string path = ((BareboneSocketTransportConfig) transport).path;
				var client = new SocketClient ();
				try {
					stream = yield client.connect_async (new UnixSocketAddress (path), cancellable);
				} catch (GLib.Error e) {
					throw new Error.TRANSPORT ("Unable to connect to %s: %s", path, e.message);
				}
			} else {
				string path = ((BareboneDeviceTransportConfig) transport).path;
				int fd = Posix.open (path, Posix.O_RDWR);
				if (fd == -1)
					throw new Error.TRANSPORT ("Unable to open %s: %s", path, Posix.strerror (Posix.errno));
				stream = new SimpleIOStream (new UnixInputStream (fd, true), new UnixOutputStream (fd, false));
			}

			var connection = yield Barebone.AgentConnection.open_resident (stream, cancellable);

			return new BareboneHostSession (connection, null);
#endif
		}

		private static BareboneAllocatorConfig? infer_allocator_config (BareboneKernelKind kind,
				Gee.List<Barebone.SymbolInfo> kernel_symbols, uint64 kernel_base, bool mmu_registers_available) {
			if (kind == WIN9X)
				return infer_win9x_allocator_config (kernel_symbols);
			if (kind == WINNT)
				return infer_winnt_allocator_config (kernel_symbols);
			if (kind == LINUX)
				return infer_linux_allocator_config (kernel_symbols, kernel_base, mmu_registers_available);
			return null;
		}

		private static BareboneAllocatorConfig? infer_linux_allocator_config (
				Gee.List<Barebone.SymbolInfo> kernel_symbols, uint64 kernel_base, bool mmu_registers_available) {
			Barebone.SymbolInfo? alloc = find_symbol (kernel_symbols, "execmem_alloc");
			Barebone.SymbolInfo? free = find_symbol (kernel_symbols, "execmem_free");
			if (alloc != null && free != null) {
				var alloc_arguments = new Gee.ArrayList<BareboneCallArgument> ();
				uint64 execmem_type = mmu_registers_available ? EXECMEM_MODULE_DATA : EXECMEM_MODULE_TEXT;
				alloc_arguments.add (new BareboneCallArgument (LITERAL, execmem_type));
				alloc_arguments.add (new BareboneCallArgument (SIZE, 0));

				var free_arguments = new Gee.ArrayList<BareboneCallArgument> ();
				free_arguments.add (new BareboneCallArgument (ADDRESS, 0));

				return new BareboneTargetFunctionsAllocatorConfig () {
					alloc_function = new BareboneNonNullMemoryAddress ("allocator.alloc_function",
						kernel_base + alloc.offset),
					free_function = new BareboneNonNullMemoryAddress ("allocator.free_function",
						kernel_base + free.offset),
					alloc_arguments = alloc_arguments,
					free_arguments = free_arguments,
				};
			}

			// Kernels before execmem hand out executable memory through module_alloc, whose only
			// argument is the size; module_memfree frees it.
			alloc = find_symbol (kernel_symbols, "module_alloc");
			free = find_symbol (kernel_symbols, "module_memfree");
			if (alloc == null || free == null)
				return null;

			var alloc_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			alloc_arguments.add (new BareboneCallArgument (SIZE, 0));

			var free_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			free_arguments.add (new BareboneCallArgument (ADDRESS, 0));

			return new BareboneTargetFunctionsAllocatorConfig () {
				alloc_function = new BareboneNonNullMemoryAddress ("allocator.alloc_function", kernel_base + alloc.offset),
				free_function = new BareboneNonNullMemoryAddress ("allocator.free_function", kernel_base + free.offset),
				alloc_arguments = alloc_arguments,
				free_arguments = free_arguments,
			};
		}

		private static BareboneAllocatorConfig? infer_win9x_allocator_config (
				Gee.List<Barebone.SymbolInfo> kernel_symbols) {
			Barebone.SymbolInfo? alloc = find_symbol (kernel_symbols, "_HeapAllocate");
			Barebone.SymbolInfo? free = find_symbol (kernel_symbols, "_HeapFree");
			if (alloc == null || free == null)
				return null;

			var free_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			free_arguments.add (new BareboneCallArgument (ADDRESS, 0));
			free_arguments.add (new BareboneCallArgument (LITERAL, 0));

			return new BareboneTargetFunctionsAllocatorConfig () {
				alloc_function = new BareboneNonNullMemoryAddress ("allocator.alloc_function", alloc.offset),
				free_function = new BareboneNonNullMemoryAddress ("allocator.free_function", free.offset),
				free_arguments = free_arguments,
			};
		}

		// A driver uses the pool, and on NT the pool is executable. The paged and session allocators
		// are not.
		private static BareboneAllocatorConfig? infer_winnt_allocator_config (
				Gee.List<Barebone.SymbolInfo> kernel_symbols) {
			Barebone.SymbolInfo? alloc = find_symbol (kernel_symbols, "ExAllocatePoolWithTag");
			Barebone.SymbolInfo? free = find_symbol (kernel_symbols, "ExFreePoolWithTag");
			if (alloc == null || free == null)
				return null;

			var alloc_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			alloc_arguments.add (new BareboneCallArgument (LITERAL, NON_PAGED_POOL));
			alloc_arguments.add (new BareboneCallArgument (SIZE, 0));
			alloc_arguments.add (new BareboneCallArgument (LITERAL, POOL_TAG));

			var free_arguments = new Gee.ArrayList<BareboneCallArgument> ();
			free_arguments.add (new BareboneCallArgument (ADDRESS, 0));
			free_arguments.add (new BareboneCallArgument (LITERAL, POOL_TAG));

			return new BareboneTargetFunctionsAllocatorConfig () {
				alloc_function = new BareboneNonNullMemoryAddress ("allocator.alloc_function", alloc.offset),
				free_function = new BareboneNonNullMemoryAddress ("allocator.free_function", free.offset),
				alloc_arguments = alloc_arguments,
				free_arguments = free_arguments,
			};
		}

		private const uint64 EXECMEM_MODULE_TEXT = 0;
		private const uint64 EXECMEM_MODULE_DATA = 4;

		private static Barebone.SymbolInfo? find_symbol (Gee.List<Barebone.SymbolInfo> symbols, string name) {
			foreach (var s in symbols) {
				if (s.name == name)
					return s;
			}
			return null;
		}

		private const uint64 NON_PAGED_POOL = 0;
		private const uint64 POOL_TAG = 0x64697246;

		public async void destroy (HostSession session, Cancellable? cancellable) throws Error, IOError {
			if (session != host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			yield host_session.close (cancellable);
			host_session.agent_session_detached.disconnect (on_agent_session_detached);
			host_session = null;
		}

		public async AgentSession link_agent_session (HostSession host_session, AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			if (host_session != this.host_session)
				throw new Error.INVALID_ARGUMENT ("Invalid host session");

			return yield this.host_session.link_agent_session (id, sink, cancellable);
		}

		public void unlink_agent_session (HostSession host_session, AgentSessionId id) {
			if (host_session != this.host_session)
				return;

			this.host_session.unlink_agent_session (id);
		}

		public async IOStream link_channel (HostSession host_session, ChannelId id, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
		}

		public void unlink_channel (HostSession host_session, ChannelId id) {
		}

		public async ServiceSession link_service_session (HostSession host_session, ServiceSessionId id, Cancellable? cancellable)
				throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Services are not supported by this backend");
		}

		public void unlink_service_session (HostSession host_session, ServiceSessionId id) {
		}

		private void on_agent_session_detached (AgentSessionId id, SessionDetachReason reason, CrashInfo crash) {
			agent_session_detached (id, reason, crash);
		}
	}

	public sealed class BareboneHostSession : Object, HostSession {
		public Barebone.AgentConnection? connection {
			get;
			construct;
		}

		/** Absent for a resident agent: every service here is built on a debugger stub. */
		public Barebone.Services? services {
			get;
			construct;
		}

		private Gee.Map<uint, uint> injected_agents = new Gee.HashMap<uint, uint> ();
		private uint spawn_helper_pid = 0;
		private Gee.HashMap<uint, HeldSpawn> pending_spawn = new Gee.HashMap<uint, HeldSpawn> ();
		private SpawnGatingWatchdog watchdog = new SpawnGatingWatchdog ();

		private class HeldSpawn {
			public string identifier;
			public uint holder_pid;

			public HeldSpawn (string identifier, uint holder_pid) {
				this.identifier = identifier;
				this.holder_pid = holder_pid;
			}
		}

		private const string SPAWN_HELPER_NAME = "explorer.exe";
		private Gee.Map<AgentSessionId?, BareboneAgentSession> agent_sessions =
			new Gee.HashMap<AgentSessionId?, BareboneAgentSession> (AgentSessionId.hash, AgentSessionId.equal);

		public BareboneHostSession (Barebone.AgentConnection? connection, Barebone.Services? services) {
			Object (connection: connection, services: services);
		}

		construct {
			if (connection != null)
				connection.spawn_added.connect (on_spawn_added);
			watchdog.expired.connect (on_watchdog_expired);
		}

		public async void close (Cancellable? cancellable) throws IOError {
			watchdog.clear ();

			foreach (BareboneAgentSession session in agent_sessions.values.to_array ()) {
				try {
					yield session.close (cancellable);
				} catch (GLib.Error e) {
					assert (e is IOError.CANCELLED);
					throw (IOError) e;
				}

				yield release_injected_agent (session.pid);
			}

			if (connection != null)
				yield connection.close (cancellable);

			if (services != null) {
				var gdb = services.machine.gdb;
				if (gdb.state == STOPPED) {
					try {
						yield gdb.continue (cancellable);
					} catch (GLib.Error e) {
					}
				}
				yield gdb.close (cancellable);
			}
		}

		public async void ping (uint interval_seconds, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		public async HashTable<string, Variant> query_system_parameters (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			if (connection == null)
				throw_not_supported ();

			var query = ApplicationQueryOptions._deserialize (options);
			var selected = new Gee.HashSet<string> ();
			query.enumerate_selected_identifiers (identifier => {
				selected.add (identifier);
			});

			var applications = yield list_applications (selected.to_array (), cancellable);

			var running = new Gee.HashMap<string, uint> ();
			foreach (HostProcessInfo p in yield connection.enumerate_processes (Scope.METADATA, {},
					cancellable)) {
				var path = p.parameters["path"];
				if (path != null)
					running[basename_of (path.get_string ()).down ()] = p.pid;
			}

			var scope = query.scope;
			var result = new HostApplicationInfo[applications.length];
			for (int i = 0; i != applications.length; i++) {
				var app = applications[i];
				var parameters = make_parameters_dict ();
				if (scope != MINIMAL)
					parameters["path"] = app.path;

				uint pid;
				if (!running.unset (basename_of (app.path).down (), out pid))
					pid = 0;

				var name = (app.description != "") ? app.description : basename_of (app.path);
				result[i] = HostApplicationInfo (app.identifier, name, pid, (owned) parameters);
			}
			return result;
		}

		private async Barebone.AgentConnection.Application[] list_applications (
				string[] identifiers, Cancellable? cancellable) throws Error, IOError {
			var listed = new Gee.ArrayList<Barebone.AgentConnection.Application> ();
			listed.add_all_array (yield connection.enumerate_applications (identifiers, cancellable));

			var known = new Gee.HashSet<string> ();
			foreach (var app in listed)
				known.add (basename_of (app.path).down ());

			var wanted = new Gee.HashSet<string> ();
			foreach (unowned string identifier in identifiers)
				wanted.add (identifier);

			try {
				uint helper = yield acquire_spawn_helper (cancellable);
				foreach (var shortcut in yield connection.enumerate_shortcuts (helper, cancellable)) {
					var file = basename_of (shortcut.target);
					if (file == "" || !known.add (file.down ()))
						continue;

					var identifier = (shortcut.identifier != "") ? shortcut.identifier : file;
					if (!wanted.is_empty && !wanted.contains (identifier))
						continue;

					var name = (shortcut.name != "") ? shortcut.name : shortcut.description;
					listed.add (new Barebone.AgentConnection.Application (identifier,
						shortcut.target, name));
				}
			} catch (GLib.Error e) {
			}

			return listed.to_array ();
		}

		private static string basename_of (string path) {
			int start = int.max (path.last_index_of_char ('\\'), path.last_index_of_char ('/'));
			return (start != -1) ? path[start + 1:] : path;
		}

		public async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			if (connection == null)
				throw_not_supported ();
			var query = ProcessQueryOptions._deserialize (options);
			uint[] pids = {};
			query.enumerate_selected_pids (pid => {
				pids += pid;
			});
			return yield connection.enumerate_processes (query.scope, pids, cancellable);
		}

		public async void enable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			yield start_gating (cancellable);
		}

		public async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			yield start_gating (cancellable);
		}

		private async void start_gating (Cancellable? cancellable) throws Error, IOError {
			if (connection == null)
				throw_not_supported ();

			if (!connection.spawns_by_itself)
				yield acquire_spawn_helper (cancellable);
			yield connection.gate_spawns (true, cancellable);
		}

		public async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			if (connection == null)
				throw_not_supported ();

			yield connection.gate_spawns (false, cancellable);
			watchdog.clear ();
			pending_spawn.clear ();
		}

		public async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			var result = new HostSpawnInfo[pending_spawn.size];
			var index = 0;
			foreach (var entry in pending_spawn.entries)
				result[index++] = HostSpawnInfo (entry.key, entry.value.identifier);
			return result;
		}

		public async HostChildInfo[] enumerate_pending_children (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable)
				throws Error, IOError {
			if (connection == null)
				throw_not_supported ();

			if (connection.spawns_by_itself)
				return yield connection.spawn_program (words_of (program, options), cancellable);

			uint helper = yield acquire_spawn_helper (cancellable);

			return yield connection.spawn_process (helper,
				command_line_of (yield file_of (program, cancellable), options), cancellable);
		}

		public async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			if (connection.spawns_by_itself) {
				pending_spawn.unset (pid);
				watchdog.cancel (pid);

				yield connection.resume_process (0, pid, cancellable);

				return;
			}

			uint holder = spawn_helper_pid;

			HeldSpawn? held = pending_spawn[pid];
			if (held != null) {
				holder = held.holder_pid;
				pending_spawn.unset (pid);
				watchdog.cancel (pid);
			}

			if (holder == 0)
				throw new Error.INVALID_ARGUMENT ("Process %u was not spawned by us", pid);

			yield connection.resume_process (holder, pid, cancellable);
		}

		public async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async AgentSessionId attach (uint pid, HashTable<string, Variant> options, Cancellable? cancellable)
				throws Error, IOError {
			if (pid != 0) {
				if (connection == null)
					throw_not_supported ();

				yield acquire_injected_agent (pid, cancellable);
			}

			var opts = SessionOptions._deserialize (options);
			if (opts.realm == EMULATED)
				throw new Error.NOT_SUPPORTED ("Emulated realm is not supported on Barebone targets");

			var session_id = AgentSessionId.generate ();

			MainContext dbus_context = yield get_dbus_context ();

			BareboneAgentSession session;
			if (connection != null) {
				session = new RemoteBareboneAgentSession (connection, pid, session_id, opts.persist_timeout,
					dbus_context);
			} else {
				if (services == null)
					throw new Error.NOT_SUPPORTED ("Barebone target has neither an agent nor a debugger stub");
				session = new LocalBareboneAgentSession (services, session_id, opts.persist_timeout, dbus_context);
			}
			agent_sessions[session_id] = session;
			session.closed.connect (on_agent_session_closed);

			return session_id;
		}

		public async void reattach (AgentSessionId id, Cancellable? cancellable) throws Error, IOError {
			throw new Error.INVALID_OPERATION ("Only meant to be implemented by services");
		}

		public async AgentSession link_agent_session (AgentSessionId id, AgentMessageSink sink,
				Cancellable? cancellable) throws Error, IOError {
			BareboneAgentSession? session = agent_sessions[id];
			if (session == null)
				throw new Error.INVALID_ARGUMENT ("Invalid session ID");

			session.message_sink = sink;

			return session;
		}

		public void unlink_agent_session (AgentSessionId id) {
			BareboneAgentSession? session = agent_sessions[id];
			if (session == null)
				return;

			session.message_sink = null;
		}

		public async InjectorPayloadId inject_library_file (uint pid, string path, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async InjectorPayloadId inject_library_blob (uint pid, uint8[] blob, string entrypoint, string data,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async ChannelId open_channel (string address, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Channels are not supported by this backend");
		}

		public async ServiceSessionId open_service (string address, Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Services are not supported by this backend");
		}

		private void on_agent_session_closed (BareboneAgentSession session) {
			AgentSessionId id = session.id;

			session.closed.disconnect (on_agent_session_closed);
			agent_sessions.unset (id);

			tear_down.begin (session);

			SessionDetachReason reason = APPLICATION_REQUESTED;
			var no_crash = CrashInfo.empty ();
			agent_session_detached (id, reason, no_crash);
		}

		private async void tear_down (BareboneAgentSession session) {
			yield session.discard_scripts ();
			yield release_injected_agent (session.pid);
		}

		// Each process has one copy for all its sessions, and the copy stays until the last session
		// ends.
		// Only ring 3 can make a process. Thus one process holds a copy of the agent, and that copy
		// makes each new process and holds it.
		private void on_spawn_added (uint pid, string command_line, uint holder_pid) {
			var identifier = program_of (command_line);
			pending_spawn[pid] = new HeldSpawn (identifier, holder_pid);
			watchdog.arm (pid);
			spawn_added (HostSpawnInfo (pid, identifier));
		}

		private void on_watchdog_expired () {
			let_the_held_go.begin ();
		}

		private async void let_the_held_go () {
			foreach (uint pid in pending_spawn.keys.to_array ()) {
				try {
					yield resume (pid, null);
				} catch (GLib.Error e) {
				}
			}

			try {
				yield disable_spawn_gating (null);
			} catch (GLib.Error e) {
			}
		}

		private static string program_of (string command_line) {
			bool quoted = command_line.has_prefix ("\"");
			var line = quoted ? command_line[1:] : command_line;
			int end = line.index_of_char (quoted ? '"' : ' ');
			return (end != -1) ? line[:end] : line;
		}

		private async uint acquire_spawn_helper (Cancellable? cancellable) throws Error, IOError {
			if (spawn_helper_pid != 0)
				return spawn_helper_pid;

			uint pid = 0;
			var processes = yield connection.enumerate_processes (Scope.METADATA, {}, cancellable);
			foreach (HostProcessInfo p in processes) {
				var path = p.parameters["path"];
				if (path != null && path.get_string ().down ().has_suffix (SPAWN_HELPER_NAME)) {
					pid = p.pid;
					break;
				}
			}
			if (pid == 0)
				throw new Error.NOT_SUPPORTED ("Found no %s to spawn from", SPAWN_HELPER_NAME);

			yield acquire_injected_agent (pid, cancellable);
			spawn_helper_pid = pid;

			return pid;
		}

		private async string file_of (string program, Cancellable? cancellable) throws Error, IOError {
			if (connection.spawns_by_itself || program.contains ("\\") || program.contains (":"))
				return program;

			foreach (var app in yield list_applications ({}, cancellable)) {
				if (app.identifier.down () == program.down ())
					return app.path;
			}

			throw new Error.INVALID_ARGUMENT ("Found no program named %s", program);
		}

		private static string[] words_of (string program, HostSpawnOptions options) {
			if (!options.has_argv)
				return new string[] { program };

			var words = new string[options.argv.length];
			words[0] = program;
			for (int i = 1; i != options.argv.length; i++)
				words[i] = options.argv[i];

			return words;
		}

		private static string command_line_of (string program, HostSpawnOptions options) {
			var line = new StringBuilder ();
			line.append (quoted (program));
			if (options.has_argv) {
				foreach (unowned string argument in options.argv[1:])
					line.append_c (' ').append (quoted (argument));
			}

			return line.str;
		}

		private static string quoted (string token) {
			if (!token.contains (" "))
				return token;

			return "\"" + token + "\"";
		}

		private async void acquire_injected_agent (uint pid, Cancellable? cancellable) throws Error, IOError {
			uint users = injected_agents[pid];
			if (users != 0) {
				injected_agents[pid] = users + 1;
				return;
			}

			// The agent reports the process that it started in. A different value shows that the copy
			// went to the incorrect process.
			uint reached = yield connection.inject_agent_into_process (pid, cancellable);
			if (reached != pid)
				throw new Error.NOT_SUPPORTED ("Agent landed in process %u, not %u", reached, pid);

			injected_agents[pid] = 1;
		}

		private async void release_injected_agent (uint pid) {
			uint users = injected_agents[pid];
			if (users == 0)
				return;

			if (users > 1) {
				injected_agents[pid] = users - 1;
				return;
			}

			injected_agents.unset (pid);
			try {
				yield connection.detach_from_process (pid, null);
			} catch (GLib.Error e) {
			}
		}
	}

	private sealed class LocalBareboneAgentSession : BareboneAgentSession {
		public Barebone.Services services {
			get;
			construct;
		}

		private Gee.Map<AgentScriptId?, BareboneScript> scripts =
			new Gee.HashMap<AgentScriptId?, BareboneScript> (AgentScriptId.hash, AgentScriptId.equal);
		private uint next_script_id = 1;

		public LocalBareboneAgentSession (Barebone.Services services, AgentSessionId id, uint persist_timeout,
				MainContext dbus_context) {
			Object (
				services: services,
				id: id,
				persist_timeout: persist_timeout,
				frida_context: MainContext.ref_thread_default (),
				dbus_context: dbus_context
			);
		}

		public override async AgentScriptId create_script (string source, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			check_open ();

			var opts = ScriptOptions._deserialize (options);
			if (opts.runtime == V8)
				throw new Error.INVALID_ARGUMENT ("The V8 runtime is not supported by the Barebone backend");

			var id = AgentScriptId (next_script_id++);

			var script = BareboneScript.create (id, source, services);
			scripts[id] = script;
			script.message.connect (on_message_from_script);

			return id;
		}

		public override async void destroy_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			BareboneScript script = get_script (script_id);
			yield script.destroy (cancellable);
			script.message.disconnect (on_message_from_script);

			scripts.unset (script_id);
		}

		public override async void load_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();
			var script = get_script (script_id);
			yield script.load (cancellable);
		}

		public override async void post_messages (AgentMessage[] messages, uint batch_id, Cancellable? cancellable)
				throws Error, IOError {
			transmitter.check_okay_to_receive ();

			foreach (var m in messages) {
				switch (m.kind) {
					case SCRIPT: {
						BareboneScript? script = scripts[m.script_id];
						if (script != null)
							script.post (m.text, m.has_data ? new Bytes (m.data) : null);
						break;
					}
					case DEBUGGER:
						break;
				}
			}

			transmitter.notify_rx_batch_id (batch_id);
		}

		private void on_message_from_script (BareboneScript script, string json, Bytes? data) {
			transmitter.post_message_from_script (script.id, json, data);
		}

		private BareboneScript get_script (AgentScriptId script_id) throws Error {
			var script = scripts[script_id];
			if (script == null)
				throw new Error.INVALID_ARGUMENT ("Invalid script ID");
			return script;
		}
	}

	private sealed class RemoteBareboneAgentSession : BareboneAgentSession {
		public Barebone.AgentConnection connection {
			get;
			construct;
		}

		public RemoteBareboneAgentSession (Barebone.AgentConnection connection, uint pid, AgentSessionId id,
				uint persist_timeout, MainContext dbus_context) {
			Object (
				connection: connection,
				pid: pid,
				id: id,
				persist_timeout: persist_timeout,
				frida_context: MainContext.ref_thread_default (),
				dbus_context: dbus_context
			);
		}

		private Gee.Set<AgentScriptId?> own_scripts =
			new Gee.HashSet<AgentScriptId?> (AgentScriptId.hash, AgentScriptId.equal);

		construct {
			connection.script_message.connect (on_message_from_script);
		}

		public override async void discard_scripts () {
			foreach (AgentScriptId script_id in own_scripts.to_array ()) {
				try {
					yield connection.destroy_script (script_id, pid, null);
				} catch (GLib.Error e) {
				}
			}
			own_scripts.clear ();
		}

		public override async AgentScriptId create_script (string source, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			check_open ();

			var opts = ScriptOptions._deserialize (options);
			if (opts.runtime == V8)
				throw new Error.INVALID_ARGUMENT ("The V8 runtime is not supported by the Barebone backend");

			var script_id = yield connection.create_script (source, pid, cancellable);
			own_scripts.add (script_id);

			return script_id;
		}

		public override async void destroy_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			yield connection.destroy_script (script_id, pid, cancellable);
			own_scripts.remove (script_id);
		}

		public override async void load_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			check_open ();

			yield connection.load_script (script_id, pid, cancellable);
		}

		public override async void post_messages (AgentMessage[] messages, uint batch_id, Cancellable? cancellable)
				throws Error, IOError {
			transmitter.check_okay_to_receive ();

			foreach (var m in messages) {
				switch (m.kind) {
					case SCRIPT:
						yield connection.post_script_message (m.script_id, m.text,
							m.has_data ? new Bytes (m.data) : null, pid, cancellable);
						break;
					case DEBUGGER:
						break;
				}
			}

			transmitter.notify_rx_batch_id (batch_id);
		}

		// Each copy numbers its scripts from one, thus take only what comes from our own process.
		private void on_message_from_script (uint source, AgentScriptId id, string json, Bytes? data) {
			if (source != pid)
				return;

			transmitter.post_message_from_script (id, json, data);
		}
	}

	private abstract class BareboneAgentSession : Object, AgentSession {
		public signal void closed ();

		// Zero selects the kernel, which is the only target that runs scripts without a copy.
		public uint pid {
			get;
			construct;
		}

		// A session owns the scripts it created, and takes only those with it.
		public virtual async void discard_scripts () {
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
			get {
				return transmitter.message_sink;
			}
			set {
				transmitter.message_sink = value;
			}
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

		protected AgentMessageTransmitter transmitter;

		construct {
			assert (frida_context != null);
			assert (dbus_context != null);

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

			yield transmitter.close (cancellable);

			close_request.resolve (true);
		}

		public async void interrupt (Cancellable? cancellable) throws Error, IOError {
			transmitter.interrupt ();
		}

		public async void resume (uint rx_batch_id, Cancellable? cancellable, out uint tx_batch_id) throws Error, IOError {
			transmitter.resume (rx_batch_id, out tx_batch_id);
		}

		public async void enable_child_gating (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void disable_child_gating (Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public abstract async AgentScriptId create_script (string source, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError;

		public async AgentScriptId create_script_from_bytes (uint8[] bytes, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint8[] compile_script (string source, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async uint8[] snapshot_script (string embed_script, HashTable<string, Variant> options, Cancellable? cancellable)
				throws Error, IOError {
			throw_not_supported ();
		}

		public abstract async void destroy_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError;

		public abstract async void load_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError;

		public async void interrupt_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void terminate_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void eternalize_script (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void enable_debugger (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void disable_debugger (AgentScriptId script_id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public abstract async void post_messages (AgentMessage[] messages, uint batch_id, Cancellable? cancellable)
			throws Error, IOError;

		public async PortalMembershipId join_portal (string address, HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
		}

		public async void leave_portal (PortalMembershipId membership_id, Cancellable? cancellable) throws Error, IOError {
			throw_not_supported ();
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

		protected void check_open () throws Error {
			if (close_request != null)
				throw new Error.INVALID_OPERATION ("Session is closing");
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

	namespace Barebone {
		public sealed class Services : Object {
			public Machine machine {
				get;
				construct;
			}

			public Allocator allocator {
				get;
				construct;
			}

			public Interceptor interceptor {
				get;
				construct;
			}

			public Services (Machine machine, Allocator allocator, Interceptor interceptor) {
				Object (
					machine: machine,
					allocator: allocator,
					interceptor: interceptor
				);
			}
		}
	}

	[NoReturn]
	private static void throw_not_supported () throws Error {
		throw new Error.NOT_SUPPORTED ("Not yet supported");
	}
}
