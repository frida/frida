namespace Frida {
	public sealed class FreebsdHostSessionBackend : LocalHostSessionBackend {
		protected override LocalHostSessionProvider make_provider () {
			return new FreebsdHostSessionProvider ();
		}
	}

	public sealed class FreebsdHostSessionProvider : LocalHostSessionProvider {
		protected override LocalHostSession make_host_session (HostSessionOptions? options) throws Error {
			return new FreebsdHostSession ();
		}
	}

	public sealed class FreebsdHostSession : LocalHostSession {
		private AgentContainer system_session_container;

		private AgentDescriptor agent_desc;

		private ApplicationEnumerator application_enumerator = new ApplicationEnumerator ();
		private ProcessEnumerator process_enumerator = new ProcessEnumerator ();

		construct {
			var binjector = new Binjector ();
			binjector.output.connect (on_output);
			binjector.uninjected.connect (on_uninjected);
			injector = binjector;

			var blob = Frida.Data.Agent.get_frida_agent_so_blob ();
			agent_desc = new AgentDescriptor (blob.name, new MemoryInputStream.from_data (blob.data, null));
		}

		public override async void close (Cancellable? cancellable) throws IOError {
			yield base.close (cancellable);

			var binjector = (Binjector) injector;

			yield wait_for_uninject (injector, cancellable, () => {
				return binjector.any_still_injected ();
			});

			binjector.uninjected.disconnect (on_uninjected);
			binjector.output.disconnect (on_output);
			yield injector.close (cancellable);

			if (system_session_container != null) {
				yield system_session_container.destroy (cancellable);
				system_session_container = null;
			}
		}

		protected override async AgentSessionProvider create_system_session_provider (Cancellable? cancellable,
				out DBusConnection connection) throws Error, IOError {
			var binjector = (Binjector) injector;

			PipeTransport.set_temp_directory (binjector.temp_directory);

			var agent_filename = binjector.resource_store.ensure_copy_of (agent_desc);
			system_session_container = yield AgentContainer.create (agent_filename, cancellable);

			connection = system_session_container.connection;

			return system_session_container;
		}

		public override async HostApplicationInfo get_frontmost_application (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return System.get_frontmost_application (FrontmostQueryOptions._deserialize (options));
		}

		public override async HostApplicationInfo[] enumerate_applications (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return yield application_enumerator.enumerate_applications (ApplicationQueryOptions._deserialize (options));
		}

		public override async HostProcessInfo[] enumerate_processes (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			return yield process_enumerator.enumerate_processes (ProcessQueryOptions._deserialize (options));
		}

		public override async void enable_spawn_gating_with_options (HashTable<string, Variant> options,
				Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public override async void disable_spawn_gating (Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public override async HostSpawnInfo[] enumerate_pending_spawn (Cancellable? cancellable) throws Error, IOError {
			throw new Error.NOT_SUPPORTED ("Not yet supported on this OS");
		}

		public override async uint spawn (string program, HostSpawnOptions options, Cancellable? cancellable)
				throws Error, IOError {
			return yield ((Binjector) injector).spawn (program, options, cancellable);
		}

		protected override async void prepare_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield ((Binjector) injector).prepare_exec_transition (pid, cancellable);
		}

		protected override async void await_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield ((Binjector) injector).await_exec_transition (pid, cancellable);
		}

		protected override async void cancel_exec_transition (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield ((Binjector) injector).cancel_exec_transition (pid, cancellable);
		}

		protected override bool process_is_alive (uint pid) {
			return Posix.kill ((Posix.pid_t) pid, 0) == 0 || Posix.errno == Posix.EPERM;
		}

		public override async void input (uint pid, uint8[] data, Cancellable? cancellable) throws Error, IOError {
			yield ((Binjector) injector).input (pid, data, cancellable);
		}

		protected override async void perform_resume (uint pid, Cancellable? cancellable) throws Error, IOError {
			yield ((Binjector) injector).resume (pid, cancellable);
		}

		public override async void kill (uint pid, Cancellable? cancellable) throws Error, IOError {
			System.kill (pid);
		}

		protected override async Future<IOStream> perform_attach_to (uint pid, HashTable<string, Variant> options,
				Cancellable? cancellable, out Object? transport) throws Error, IOError {
			var binjector = (Binjector) injector;

			PipeTransport.set_temp_directory (binjector.temp_directory);

			var t = new PipeTransport ();

			var stream_request = Pipe.open (t.local_address, cancellable);

			var id = yield binjector.inject_library_resource (pid, agent_desc, "frida_agent_main",
				make_agent_parameters (pid, t.remote_address, options), cancellable);
			injectee_by_pid[pid] = id;

			transport = t;

			return stream_request;
		}

		private void on_output (uint pid, int fd, uint8[] data) {
			output (pid, fd, data);
		}
	}
}
