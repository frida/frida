namespace Frida {
	private sealed class BareboneScript : Object {
		public signal void message (string json, Bytes? data);

		public AgentScriptId id {
			get;
			construct;
		}

		public Barebone.Services services {
			get;
			construct;
		}

		private GDB.Client gdb;

		private QuickJS.Runtime rt;
		private QuickJS.Context ctx;

		private QuickJS.Atom address_key;
		private QuickJS.Atom base_key;
		private QuickJS.Atom breakpoint_key;
		private QuickJS.Atom coalesce_key;
		private QuickJS.Atom dependencies_key;
		private QuickJS.Atom handle_key;
		private QuickJS.Atom invoke_key;
		private QuickJS.Atom length_key;
		private QuickJS.Atom line_number_key;
		private QuickJS.Atom message_key;
		private QuickJS.Atom on_complete_key;
		private QuickJS.Atom on_enter_key;
		private QuickJS.Atom on_error_key;
		private QuickJS.Atom on_leave_key;
		private QuickJS.Atom on_match_key;
		private QuickJS.Atom protection_key;
		private QuickJS.Atom prototype_key;
		private QuickJS.Atom signum_key;
		private QuickJS.Atom size_key;
		private QuickJS.Atom thread_key;
		private QuickJS.Atom type_key;
		private QuickJS.Atom v_key;

		private Gee.Queue<QuickJS.Value?> tick_callbacks = new Gee.ArrayQueue<QuickJS.Value?> ();

		private Gee.Set<Barebone.Callback> native_callbacks = new Gee.HashSet<Barebone.Callback> ();

		private static QuickJS.ClassID cpu_context_class;
		private static QuickJS.ClassExoticMethods cpu_context_exotic_methods;

		private static QuickJS.ClassID invocation_listener_class;
		private Gee.Set<Barebone.InvocationListener> invocation_listeners = new Gee.HashSet<Barebone.InvocationListener> ();
		private static QuickJS.ClassID invocation_context_class;
		private static QuickJS.ClassID invocation_args_class;
		private static QuickJS.ClassExoticMethods invocation_args_exotic_methods;
		private static QuickJS.ClassID invocation_retval_class;

		private static QuickJS.ClassID c_module_class;
		private Gee.Set<Barebone.CModule> c_modules = new Gee.HashSet<Barebone.CModule> ();

		private static QuickJS.ClassID rust_module_class;
		private Gee.Set<Barebone.RustModule> rust_modules = new Gee.HashSet<Barebone.RustModule> ();

		private static QuickJS.ClassID gdb_thread_class;

		private static QuickJS.ClassID gdb_breakpoint_class;
		private Gee.Map<GDB.Breakpoint, QuickJS.Value?> gdb_breakpoints = new Gee.HashMap<GDB.Breakpoint, QuickJS.Value?> ();

		private QuickJS.Value global = QuickJS.Undefined;
		private QuickJS.Value runtime_obj = QuickJS.Undefined;
		private QuickJS.Value dispatch_exception_func = QuickJS.Undefined;
		private QuickJS.Value dispatch_message_func = QuickJS.Undefined;
		private QuickJS.Value ptr_func = QuickJS.Undefined;
		private QuickJS.Value int64_func = QuickJS.Undefined;
		private QuickJS.Value uint64_func = QuickJS.Undefined;

		private Gee.Queue<Entrypoint> entrypoints = new Gee.ArrayQueue<Entrypoint> ();
		private Gee.Map<string, Asset> assets = new Gee.HashMap<string, Asset> ();

		private Cancellable io_cancellable = new Cancellable ();

		private const uint64 MAX_ASSET_SIZE = 100 * 1024 * 1024;
		private const uint32 MAX_JS_BYTE_ARRAY_LENGTH = 100 * 1024 * 1024;

		public static BareboneScript create (AgentScriptId id, string source, Barebone.Services services) throws Error {
			var script = new BareboneScript (id, services);

			unowned string runtime_js = (string) Frida.Data.Barebone.get_script_runtime_js_blob ().data;
			script.add_program (runtime_js, "/_frida.js");
			script.add_program (source, "/agent.js");

			return script;
		}

		private BareboneScript (AgentScriptId id, Barebone.Services services) {
			Object (id: id, services: services);
		}

		static construct {
			cpu_context_exotic_methods.get_own_property = on_cpu_context_get_own_property;
			cpu_context_exotic_methods.get_own_property_names = on_cpu_context_get_own_property_names;
			cpu_context_exotic_methods.has_property = on_cpu_context_has_property;
			cpu_context_exotic_methods.get_property = on_cpu_context_get_property;
			cpu_context_exotic_methods.set_property = on_cpu_context_set_property;

			invocation_args_exotic_methods.get_property = on_invocation_args_get_property;
			invocation_args_exotic_methods.set_property = on_invocation_args_set_property;
		}

		construct {
			gdb = services.machine.gdb;

			rt = QuickJS.Runtime.make ();
			rt.set_opaque (this);

			ctx = QuickJS.Context.make (rt);
			ctx.set_opaque (this);

			address_key = ctx.make_atom ("address");
			base_key = ctx.make_atom ("base");
			breakpoint_key = ctx.make_atom ("breakpoint");
			coalesce_key = ctx.make_atom ("coalesce");
			dependencies_key = ctx.make_atom ("dependencies");
			handle_key = ctx.make_atom ("handle");
			invoke_key = ctx.make_atom ("_invoke");
			length_key = ctx.make_atom ("length");
			line_number_key = ctx.make_atom ("lineNumber");
			message_key = ctx.make_atom ("message");
			on_complete_key = ctx.make_atom ("onComplete");
			on_enter_key = ctx.make_atom ("onEnter");
			on_error_key = ctx.make_atom ("onError");
			on_leave_key = ctx.make_atom ("onLeave");
			on_match_key = ctx.make_atom ("onMatch");
			protection_key = ctx.make_atom ("protection");
			prototype_key = ctx.make_atom ("prototype");
			signum_key = ctx.make_atom ("signum");
			size_key = ctx.make_atom ("size");
			thread_key = ctx.make_atom ("thread");
			type_key = ctx.make_atom ("type");
			v_key = ctx.make_atom ("$v");

			global = ctx.get_global_object ();
			add_cfunc (global, "_send", on_send, 2);
			add_cfunc (global, "_invoke", on_invoke, 1);
			add_cfunc (global, "_installNativeCallback", on_install_native_callback, 3);

			var script_obj = ctx.make_object ();
			add_cfunc (script_obj, "evaluate", on_evaluate, 2);
			add_cfunc (script_obj, "nextTick", on_next_tick, 1);
			global.set_property_str (ctx, "Script", script_obj);

			QuickJS.ClassDef cc;
			cc.class_name = "CpuContext";
			cc.finalizer = on_cpu_context_finalize;
			cc.exotic = &cpu_context_exotic_methods;
			rt.make_class (QuickJS.make_class_id (ref cpu_context_class), cc);

			var memory_obj = ctx.make_object ();
			add_cfunc (memory_obj, "alloc", on_memory_alloc, 1);
			add_cfunc (memory_obj, "protect", on_memory_protect, 3);
			add_cfunc (memory_obj, "scan", on_memory_scan, 4);
			add_cfunc (memory_obj, "scanSync", on_memory_scan_sync, 3);
			global.set_property_str (ctx, "Memory", memory_obj);

			var process_obj = ctx.make_object ();
			process_obj.set_property_str (ctx, "arch", ctx.make_string (gdb.arch.to_nick ()));
			process_obj.set_property_str (ctx, "pageSize", ctx.make_uint32 ((uint32) services.allocator.page_size));
			process_obj.set_property_str (ctx, "pointerSize", ctx.make_uint32 (gdb.pointer_size));
			add_cfunc (process_obj, "enumerateRanges", on_process_enumerate_ranges, 1);
			global.set_property_str (ctx, "Process", process_obj);

			var file_obj = ctx.make_object ();
			add_cfunc (file_obj, "readAllBytes", on_file_read_all_bytes, 1);
			add_cfunc (file_obj, "readAllText", on_file_read_all_text, 1);
			add_cfunc (file_obj, "writeAllBytes", on_file_write_all_bytes, 2);
			add_cfunc (file_obj, "writeAllText", on_file_write_all_text, 2);
			global.set_property_str (ctx, "File", file_obj);

			var interceptor_obj = ctx.make_object ();
			add_property (interceptor_obj, "breakpointKind", on_interceptor_get_breakpoint_kind,
				on_interceptor_set_breakpoint_kind);
			add_cfunc (interceptor_obj, "attach", on_interceptor_attach, 2);
			global.set_property_str (ctx, "Interceptor", interceptor_obj);

			QuickJS.ClassDef il;
			il.class_name = "InvocationListener";
			rt.make_class (QuickJS.make_class_id (ref invocation_listener_class), il);
			var il_proto = ctx.make_object ();
			add_cfunc (il_proto, "detach", on_invocation_listener_detach, 0);
			ctx.set_class_proto (invocation_listener_class, il_proto);

			QuickJS.ClassDef ic;
			ic.class_name = "InvocationContext";
			rt.make_class (QuickJS.make_class_id (ref invocation_context_class), ic);
			var ic_proto = ctx.make_object ();
			add_getter (ic_proto, "returnAddress", on_invocation_context_get_return_address);
			add_getter (ic_proto, "context", on_invocation_context_get_context);
			ic_proto.set_property_str (ctx, "errno", ctx.make_int32 (-1));
			add_getter (ic_proto, "threadId", on_invocation_context_get_thread_id);
			add_getter (ic_proto, "depth", on_invocation_context_get_depth);
			ctx.set_class_proto (invocation_context_class, ic_proto);

			QuickJS.ClassDef ia;
			ia.class_name = "InvocationArguments";
			ia.exotic = &invocation_args_exotic_methods;
			rt.make_class (QuickJS.make_class_id (ref invocation_args_class), ia);

			QuickJS.ClassDef ir;
			ir.class_name = "InvocationReturnValue";
			rt.make_class (QuickJS.make_class_id (ref invocation_retval_class), ir);

			QuickJS.ClassDef cm;
			cm.class_name = "CModule";
			cm.finalizer = on_c_module_finalize;
			rt.make_class (QuickJS.make_class_id (ref c_module_class), cm);
			var cm_proto = ctx.make_object ();
			add_cfunc (cm_proto, "dispose", on_c_module_dispose, 0);
			ctx.set_class_proto (c_module_class, cm_proto);
			var cm_ctor = ctx.make_cfunction2 (on_c_module_construct, cm.class_name, 1, constructor, 0);
			cm_ctor.set_constructor (ctx, cm_proto);
			global.set_property_str (ctx, "CModule", cm_ctor);

			QuickJS.ClassDef rm;
			rm.class_name = "RustModule";
			rm.finalizer = on_rust_module_finalize;
			rt.make_class (QuickJS.make_class_id (ref rust_module_class), rm);
			var rm_proto = ctx.make_object ();
			add_cfunc (rm_proto, "dispose", on_rust_module_dispose, 0);
			ctx.set_class_proto (rust_module_class, rm_proto);
			var rm_ctor = ctx.make_cfunction2 (on_rust_module_construct, rm.class_name, 3, constructor, 0);
			rm_ctor.set_constructor (ctx, rm_proto);
			global.set_property_str (ctx, "RustModule", rm_ctor);

			var gdb_obj = ctx.make_object ();
			add_getter (gdb_obj, "state", on_gdb_get_state);
			add_getter (gdb_obj, "exception", on_gdb_get_exception);
			add_cfunc (gdb_obj, "continue", on_gdb_continue, 0);
			add_cfunc (gdb_obj, "stop", on_gdb_stop, 0);
			add_cfunc (gdb_obj, "restart", on_gdb_restart, 0);
			add_cfunc (gdb_obj, "readPointer", on_gdb_read_pointer, 1);
			add_cfunc (gdb_obj, "writePointer", on_gdb_write_pointer, 2);
			add_cfunc (gdb_obj, "readS8", on_gdb_read_s8, 1);
			add_cfunc (gdb_obj, "writeS8", on_gdb_write_s8, 2);
			add_cfunc (gdb_obj, "readU8", on_gdb_read_u8, 1);
			add_cfunc (gdb_obj, "writeU8", on_gdb_write_u8, 2);
			add_cfunc (gdb_obj, "readS16", on_gdb_read_s16, 1);
			add_cfunc (gdb_obj, "writeS16", on_gdb_write_s16, 2);
			add_cfunc (gdb_obj, "readU16", on_gdb_read_u16, 1);
			add_cfunc (gdb_obj, "writeU16", on_gdb_write_u16, 2);
			add_cfunc (gdb_obj, "readS32", on_gdb_read_s32, 1);
			add_cfunc (gdb_obj, "writeS32", on_gdb_write_s32, 2);
			add_cfunc (gdb_obj, "readU32", on_gdb_read_u32, 1);
			add_cfunc (gdb_obj, "writeU32", on_gdb_write_u32, 2);
			add_cfunc (gdb_obj, "readS64", on_gdb_read_s64, 1);
			add_cfunc (gdb_obj, "writeS64", on_gdb_write_s64, 2);
			add_cfunc (gdb_obj, "readU64", on_gdb_read_u64, 1);
			add_cfunc (gdb_obj, "writeU64", on_gdb_write_u64, 2);
			add_cfunc (gdb_obj, "readFloat", on_gdb_read_float, 1);
			add_cfunc (gdb_obj, "writeFloat", on_gdb_write_float, 2);
			add_cfunc (gdb_obj, "readDouble", on_gdb_read_double, 1);
			add_cfunc (gdb_obj, "writeDouble", on_gdb_write_double, 2);
			add_cfunc (gdb_obj, "readByteArray", on_gdb_read_byte_array, 2);
			add_cfunc (gdb_obj, "writeByteArray", on_gdb_write_byte_array, 2);
			add_cfunc (gdb_obj, "readCString", on_gdb_read_c_string, 2);
			add_cfunc (gdb_obj, "readUtf8String", on_gdb_read_utf8_string, 2);
			add_cfunc (gdb_obj, "writeUtf8String", on_gdb_write_utf8_string, 2);
			add_cfunc (gdb_obj, "addBreakpoint", on_gdb_add_breakpoint, 3);
			add_cfunc (gdb_obj, "runRemoteCommand", on_gdb_run_remote_command, 1);
			add_cfunc (gdb_obj, "execute", on_gdb_execute, 1);
			add_cfunc (gdb_obj, "query", on_gdb_query, 1);
			global.set_property_str (ctx, "$gdb", gdb_obj);

			QuickJS.ClassDef th;
			th.class_name = "GDBThread";
			th.finalizer = on_gdb_thread_finalize;
			rt.make_class (QuickJS.make_class_id (ref gdb_thread_class), th);
			var th_proto = ctx.make_object ();
			add_getter (th_proto, "id", on_gdb_thread_get_id);
			add_getter (th_proto, "name", on_gdb_thread_get_name);
			add_cfunc (th_proto, "step", on_gdb_thread_step, 0);
			add_cfunc (th_proto, "stepAndContinue", on_gdb_thread_step_and_continue, 0);
			add_cfunc (th_proto, "readRegisters", on_gdb_thread_read_registers, 0);
			add_cfunc (th_proto, "readRegister", on_gdb_thread_read_register, 1);
			add_cfunc (th_proto, "writeRegister", on_gdb_thread_write_register, 2);
			ctx.set_class_proto (gdb_thread_class, th_proto);

			QuickJS.ClassDef bp;
			bp.class_name = "GDBBreakpoint";
			bp.finalizer = on_gdb_breakpoint_finalize;
			rt.make_class (QuickJS.make_class_id (ref gdb_breakpoint_class), bp);
			var bp_proto = ctx.make_object ();
			add_getter (bp_proto, "kind", on_gdb_breakpoint_get_kind);
			add_getter (bp_proto, "address", on_gdb_breakpoint_get_address);
			add_getter (bp_proto, "size", on_gdb_breakpoint_get_size);
			add_cfunc (bp_proto, "enable", on_gdb_breakpoint_enable, 0);
			add_cfunc (bp_proto, "disable", on_gdb_breakpoint_disable, 0);
			add_cfunc (bp_proto, "remove", on_gdb_breakpoint_remove, 0);
			ctx.set_class_proto (gdb_breakpoint_class, bp_proto);
		}

		private void add_cfunc (QuickJS.Value ns, string name, QuickJS.CFunction func, int arity) {
			ns.set_property_str (ctx, name, ctx.make_cfunction (func, name, arity));
		}

		private void add_getter (QuickJS.Value ns, string name, QuickJS.CFunction func) {
			add_property (ns, name, func, null);
		}

		private void add_property (QuickJS.Value ns, string name, QuickJS.CFunction getter_func, QuickJS.CFunction? setter_func) {
			QuickJS.Atom prop = ctx.make_atom (name);
			var val = QuickJS.Undefined;

			QuickJS.PropertyFlags flags = HAS_GET | HAS_ENUMERABLE | ENUMERABLE;
			var getter = ctx.make_cfunction (getter_func, name, 0);

			QuickJS.Value setter = QuickJS.Undefined;
			if (setter_func != null) {
				flags |= HAS_SET;
				setter = ctx.make_cfunction (setter_func, name, 1);
			}

			ns.define_property (ctx, prop, val, getter, setter, flags);

			ctx.free_value (setter);
			ctx.free_value (getter);
			ctx.free_atom (prop);
		}

		~BareboneScript () {
			rust_modules.clear ();
			native_callbacks.clear ();

			QuickJS.Value[] values = {
				global,
				runtime_obj,
				dispatch_exception_func,
				dispatch_message_func,
				ptr_func,
				int64_func,
				uint64_func,
			};
			foreach (var val in values)
				ctx.free_value (val);

			entrypoints.clear ();

			QuickJS.Atom atoms[] = {
				address_key,
				base_key,
				breakpoint_key,
				coalesce_key,
				dependencies_key,
				handle_key,
				invoke_key,
				length_key,
				line_number_key,
				message_key,
				on_complete_key,
				on_enter_key,
				on_error_key,
				on_leave_key,
				on_match_key,
				protection_key,
				signum_key,
				size_key,
				thread_key,
				type_key,
				v_key,
			};
			foreach (var atom in atoms)
				ctx.free_atom (atom);

			ctx = null;
			rt = null;
		}

		public async void destroy (Cancellable? cancellable) throws IOError {
			io_cancellable.cancel ();

			var interceptor = services.interceptor;
			foreach (var listener in invocation_listeners.to_array ()) {
				try {
					yield interceptor.detach (listener, cancellable);
				} catch (Error e) {
				}
			}
			invocation_listeners.clear ();

			var source = new IdleSource ();
			source.set_callback (destroy.callback);
			source.attach (MainContext.get_thread_default ());
			yield;
		}

		public async void load (Cancellable? cancellable) throws IOError {
			Entrypoint? entrypoint;
			while ((entrypoint = entrypoints.poll ()) != null) {
				var result = ctx.eval_function (ctx.dup_value (entrypoint.callable));
				if (result.is_exception ()) {
					catch_and_emit ();
					continue;
				}

				if (entrypoint.kind == ESM) {
					var op = new PromiseWaitOperation (this, result);
					var r = yield op.perform (cancellable);
					if (!r.error.is_null ())
						on_unhandled_exception (r.error);
				} else {
					ctx.free_value (result);
				}

				maybe_bind_runtime ();
			}

			perform_pending_io ();
		}

		private void maybe_bind_runtime () {
			if (!runtime_obj.is_undefined ())
				return;

			runtime_obj = global.get_property_str (ctx, "$rt");
			if (runtime_obj.is_undefined ())
				return;

			dispatch_exception_func = runtime_obj.get_property_str (ctx, "dispatchException");
			assert (!dispatch_exception_func.is_undefined ());

			dispatch_message_func = runtime_obj.get_property_str (ctx, "dispatchMessage");
			assert (!dispatch_message_func.is_undefined ());

			var native_pointer_instance = global.get_property_str (ctx, "NULL");
			assert (!native_pointer_instance.is_undefined ());
			var native_pointer_proto = native_pointer_instance.get_prototype (ctx);

			var ir_proto = ctx.make_object_proto (native_pointer_proto);
			add_cfunc (ir_proto, "replace", on_invocation_retval_replace, 1);
			ctx.set_class_proto (invocation_retval_class, ir_proto);

			ctx.free_value (native_pointer_proto);
			ctx.free_value (native_pointer_instance);

			ptr_func = global.get_property_str (ctx, "ptr");
			assert (!ptr_func.is_undefined ());

			int64_func = global.get_property_str (ctx, "int64");
			assert (!int64_func.is_undefined ());

			uint64_func = global.get_property_str (ctx, "uint64");
			assert (!uint64_func.is_undefined ());
		}

		public void post (string json, Bytes? data) {
			var json_val = ctx.make_string (json);
			var data_val = (data != null) ? ctx.make_array_buffer (data.get_data ()) : QuickJS.Null;
			invoke_void (dispatch_message_func, { json_val, data_val }, runtime_obj);
			ctx.free_value (data_val);
			ctx.free_value (json_val);

			perform_pending_io ();
		}

		private void add_program (string source, string name) throws Error {
			unowned string package_marker = "📦\n";
			unowned string delimiter_marker = "\n✄\n";
			unowned string alias_marker = "↻ ";

			if (source.has_prefix (package_marker)) {
				rt.set_module_loader_func (normalize_module_name, load_module);

				string pending = source[package_marker.length:];
				while (true) {
					string[] pkg_tokens = pending.split (delimiter_marker, 2);
					if (pkg_tokens.length != 2)
						throw_malformed_package ();

					unowned string header = pkg_tokens[0];
					unowned string raw_assets = pkg_tokens[1];

					uint assets_offset = 0;
					uint assets_size = raw_assets.length;

					Asset? entrypoint = null;

					string[] header_lines = header.split ("\n");
					Asset? current_asset = null;
					for (uint i = 0; i != header_lines.length && assets_offset != assets_size; i++) {
						unowned string header_line = header_lines[i];

						if (header_line.has_prefix (alias_marker)) {
							if (current_asset == null)
								throw_malformed_package ();
							string alias = header_line[alias_marker.length:];
							assets[alias] = current_asset;
							continue;
						}

						unowned string assets_cursor = (string *) raw_assets + assets_offset;
						if (i != 0) {
							if (!assets_cursor.has_prefix (delimiter_marker))
								throw_malformed_package ();
							assets_offset += delimiter_marker.length;
						}

						string[] tokens = header_line.split (" ", 2);
						if (tokens.length != 2)
							throw_malformed_package ();

						uint64 size = uint64.parse (tokens[0]);
						if (size == 0 || size > MAX_ASSET_SIZE || size > assets_size - assets_offset)
							throw_malformed_package ();

						unowned string asset_name = tokens[1];
						string asset_data = raw_assets[assets_offset:assets_offset + (uint) size];

						var asset = new Asset (asset_name, (owned) asset_data);
						assets[asset_name] = asset;
						current_asset = asset;

						if (entrypoint == null && asset_name.has_suffix (".js"))
							entrypoint = asset;

						assets_offset += (uint) size;
					}

					if (entrypoint == null)
						throw_malformed_package ();

					var val = compile_module (entrypoint);
					entrypoints.offer (new Entrypoint (this, val, ESM));

					string rest = raw_assets[assets_offset:];
					if (rest.has_prefix (delimiter_marker))
						pending = rest[delimiter_marker.length:];
					else if (rest.length == 0)
						break;
					else
						throw_malformed_package ();
				}
			} else {
				var val = compile_script (source, name);
				entrypoints.offer (new Entrypoint (this, val, PLAIN));
			}
		}

		[NoReturn]
		private static void throw_malformed_package () throws Error {
			throw new Error.INVALID_ARGUMENT ("Malformed package");
		}

		private string * normalize_module_name (QuickJS.Context ctx, string base_name, string name) {
			if (name[0] != '.') {
				Asset? asset = assets[name];
				if (asset != null)
					return ctx.strdup (asset.name);
				return ctx.strdup (name);
			}

			var result = new StringBuilder ();

			int offset = base_name.last_index_of_char ('/');
			if (offset != -1)
				result.append (base_name[:offset]);

			string * cursor = name;
			while (true) {
				if (cursor->has_prefix ("./")) {
					cursor += 2;
				} else if (cursor->has_prefix ("../")) {
					if (result.len == 0)
						break;

					int last_slash_offset = result.str.last_index_of_char ('/');

					string * rest;
					if (last_slash_offset != -1)
						rest = (string *) result.str + last_slash_offset + 1;
					else
						rest = result.str;
					if (rest == "." || rest == "..")
						break;

					result.truncate ((last_slash_offset != -1) ? last_slash_offset : 0);

					cursor += 3;
				} else {
					break;
				}
			}

			result
				.append_c ('/')
				.append (cursor);

			return ctx.strdup (result.str);
		}

		private unowned QuickJS.ModuleDef? load_module (QuickJS.Context ctx, string module_name) {
			QuickJS.Value val;
			try {
				Asset? asset = assets[module_name];
				if (asset == null)
					throw new Error.INVALID_ARGUMENT ("Could not load module '%s'", module_name);

				val = compile_module (asset);
			} catch (Error e) {
				throw_js_error (error_message_to_js (e.message));
				return null;
			}

			unowned QuickJS.ModuleDef mod = (QuickJS.ModuleDef) val.get_ptr ();
			ctx.free_value (val);

			return mod;
		}

		private QuickJS.Value compile_module (Asset asset) throws Error {
			var val = ctx.eval (asset.data, asset.data.length, asset.name,
				QuickJS.EvalType.MODULE |
				QuickJS.EvalFlag.STRICT |
				QuickJS.EvalFlag.COMPILE_ONLY);

			if (val.is_exception ()) {
				JSError e = catch_js_error ();
				throw new Error.INVALID_ARGUMENT ("Could not parse '%s' line %u: %s", asset.name, e.line, e.message);
			}

			return val;
		}

		private QuickJS.Value compile_script (string source, string name) throws Error {
			var val = ctx.eval (source, source.length, name,
				QuickJS.EvalType.GLOBAL |
				QuickJS.EvalFlag.STRICT |
				QuickJS.EvalFlag.COMPILE_ONLY);

			if (val.is_exception ()) {
				JSError e = catch_js_error ();
				throw new Error.INVALID_ARGUMENT ("Script(line %u): %s", e.line, e.message);
			}

			return val;
		}

		private static QuickJS.Value on_send (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string message;
			if (!script->unparse_string (argv[0], out message))
				return QuickJS.Exception;

			Bytes? data = null;
			if (!argv[1].is_undefined () && !argv[1].is_null () && !script->unparse_bytes (argv[1], out data))
				return QuickJS.Exception;

			script->message (message, data);

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_invoke (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			uint64 impl;
			if (!script->unparse_uint64 (argv[0], out impl))
				return QuickJS.Exception;

			uint64[] args = {};
			for (uint i = 1; i != argv.length; i++) {
				uint64 v;
				if (!script->unparse_uint64 (argv[i], out v))
					return QuickJS.Exception;
				args += v;
			}

			var promise = new Promise<uint64?> ();
			script->do_invoke.begin (impl, args, promise);

			uint64? retval = script->process_events_until_ready (promise);
			if (retval == null)
				return QuickJS.Exception;

			return ctx.make_biguint64 (retval);
		}

		private async void do_invoke (uint64 impl, uint64[] args, Promise<uint64?> promise) {
			try {
				uint64 retval = yield services.machine.invoke (impl, args, io_cancellable);

				promise.resolve (retval);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_install_native_callback (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			uint64 code;
			if (!script->unparse_uint64 (argv[0], out code))
				return QuickJS.Exception;

			QuickJS.Value wrapper, method;
			var scope = new ValueScope (script);
			wrapper = scope.retain (argv[1]);
			if (!scope.unparse_callback (wrapper, script->invoke_key, out method))
				return QuickJS.Exception;

			uint arity;
			if (!script->unparse_uint (argv[2], out arity))
				return QuickJS.Exception;

			var handler = new NativeCallbackHandler (script, wrapper, method, arity, scope);

			var promise = new Promise<Barebone.Callback> ();
			script->do_install_native_callback.begin (code, handler, promise);

			Barebone.Callback? callback = script->process_events_until_ready (promise);
			if (callback == null)
				return QuickJS.Exception;

			script->native_callbacks.add (callback);

			return QuickJS.Undefined;
		}

		private async void do_install_native_callback (uint64 code, Barebone.CallbackHandler handler,
				Promise<Barebone.Callback> promise) {
			try {
				var callback = yield new Barebone.Callback (code, handler, services.machine, io_cancellable);

				promise.resolve (callback);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private class Entrypoint {
			private weak BareboneScript script;
			public QuickJS.Value callable;
			public Kind kind;

			public enum Kind {
				PLAIN,
				ESM
			}

			public Entrypoint (BareboneScript script, QuickJS.Value callable, Kind kind) {
				this.script = script;
				this.callable = callable;
				this.kind = kind;
			}

			~Entrypoint () {
				script.ctx.free_value (callable);
			}
		}

		private class PromiseWaitOperation {
			private BareboneScript script;
			private QuickJS.Value promise;

			private enum Magic {
				ON_SUCCESS,
				ON_FAILURE,
			}

			public PromiseWaitOperation (BareboneScript script, QuickJS.Value promise) {
				this.script = script;
				this.promise = promise;
			}

			~PromiseWaitOperation () {
				script.ctx.free_value (promise);
			}

			public async Result perform (Cancellable? cancellable) throws IOError {
				var res = new Result (script, perform.callback);

				unowned QuickJS.Context ctx = script.ctx;

				var data = ctx.make_object ();
				data.set_opaque (res);

				var on_success = ctx.make_cfunction_data (on_settled, 1, Magic.ON_SUCCESS, { data });
				var on_failure = ctx.make_cfunction_data (on_settled, 1, Magic.ON_FAILURE, { data });

				var then_func = promise.get_property_str (ctx, "then");
				var catch_func = promise.get_property_str (ctx, "catch");

				ctx.free_value (then_func.call (ctx, promise, { on_success }));
				ctx.free_value (catch_func.call (ctx, promise, { on_failure }));

				ctx.free_value (catch_func);
				ctx.free_value (then_func);

				ctx.free_value (on_failure);
				ctx.free_value (on_success);

				script.perform_pending_io ();
				yield;

				return res;
			}

			private static QuickJS.Value on_settled (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv, int magic,
					QuickJS.Value[] data) {
				QuickJS.ClassID cid;
				unowned Result res = (Result) data[0].get_any_opaque (out cid);

				if (magic == Magic.ON_SUCCESS)
					res.val = ctx.dup_value (argv[0]);
				else
					res.error = ctx.dup_value (argv[0]);

				var source = new IdleSource ();
				source.set_callback ((owned) res.on_complete);
				source.attach (MainContext.get_thread_default ());

				return QuickJS.Undefined;
			}

			public class Result {
				public BareboneScript script;
				public SourceFunc on_complete;

				public QuickJS.Value val = QuickJS.Null;
				public QuickJS.Value error = QuickJS.Null;

				public Result (BareboneScript script, owned SourceFunc on_complete) {
					this.script = script;
					this.on_complete = (owned) on_complete;
				}

				~Result () {
					unowned QuickJS.Context ctx = script.ctx;
					ctx.free_value (val);
					ctx.free_value (error);
				}
			}
		}

		private class NativeCallbackHandler : Object, Barebone.CallbackHandler {
			public uint arity {
				get { return _arity; }
			}

			private weak BareboneScript script;
			private QuickJS.Value wrapper;
			private QuickJS.Value method;
			private uint _arity;

			private ValueScope scope;

			public NativeCallbackHandler (BareboneScript script, QuickJS.Value wrapper, QuickJS.Value method, uint arity,
					ValueScope scope) {
				this.script = script;
				this.wrapper = wrapper;
				this.method = method;
				this._arity = arity;

				this.scope = scope;
			}

			public async uint64 handle_invocation (uint64[] args, Barebone.CallFrame frame, Cancellable? cancellable)
					throws Error, IOError {
				var scope = new ValueScope (script);
				unowned QuickJS.Context ctx = scope.ctx;

				var js_args = scope.take (ctx.make_array ());
				for (uint32 i = 0; i != args.length; i++)
					js_args.set_property_uint32 (ctx, i, ctx.make_biguint64 (args[i]));

				var return_address = scope.take (script.make_native_pointer (frame.return_address));

				var context = scope.take (script.make_cpu_context (frame.registers));

				var js_retval = script.invoke (method, { js_args, return_address, context }, wrapper);
				if (js_retval.is_exception ())
					return 0;
				scope.take (js_retval);

				uint64 retval;
				if (!script.unparse_uint64 (js_retval, out retval)) {
					script.catch_and_emit ();
					return 0;
				}

				return retval;
			}
		}

		private static QuickJS.Value on_evaluate (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string name;
			if (!script->unparse_string (argv[0], out name))
				return QuickJS.Exception;

			string source;
			if (!script->unparse_string (argv[1], out source))
				return QuickJS.Exception;

			var func = ctx.eval (source, source.length, name,
				QuickJS.EvalType.GLOBAL |
				QuickJS.EvalFlag.STRICT |
				QuickJS.EvalFlag.COMPILE_ONLY);

			if (func.is_exception ()) {
				JSError e = script->catch_js_error ();
				script->throw_js_error ("could not parse '%s' line %u: %s".printf (name, e.line, e.message));
				return QuickJS.Exception;
			}

			return ctx.eval_function (func);
		}

		private static QuickJS.Value on_next_tick (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			var callback = argv[0];
			if (!callback.is_function (ctx)) {
				script->throw_js_error ("expected a function");
				return QuickJS.Exception;
			}

			script->tick_callbacks.offer (ctx.dup_value (callback));

			return QuickJS.Undefined;
		}

		private QuickJS.Value make_native_pointer (uint64 val) {
			var jsval = ctx.make_biguint64 (val);
			var result = ptr_func.call (ctx, QuickJS.Undefined, { jsval });
			ctx.free_value (jsval);
			return result;
		}

		private QuickJS.Value make_int64 (int64 val) {
			var jsval = ctx.make_bigint64 (val);
			var result = int64_func.call (ctx, QuickJS.Undefined, { jsval });
			ctx.free_value (jsval);
			return result;
		}

		private QuickJS.Value make_uint64 (uint64 val) {
			var jsval = ctx.make_biguint64 (val);
			var result = uint64_func.call (ctx, QuickJS.Undefined, { jsval });
			ctx.free_value (jsval);
			return result;
		}

		private QuickJS.Value make_array_buffer_take (owned uint8[] contents) {
			return ctx.make_array_buffer_with_free_func ((owned) contents, free_array_buffer, false);
		}

		private static void free_array_buffer (QuickJS.Runtime rt, void * ptr) {
			free (ptr);
		}

		private QuickJS.Value make_cpu_context (Gee.Map<string, Variant> regs) {
			var wrapper = ctx.make_object_class (cpu_context_class);
			wrapper.set_opaque (regs.ref ());
			return wrapper;
		}

		private static void on_cpu_context_finalize (QuickJS.Runtime rt, QuickJS.Value val) {
			Gee.Map<string, Variant> * map = val.get_opaque (cpu_context_class);
			map->unref ();
		}

		private static int on_cpu_context_get_own_property (QuickJS.Context ctx, QuickJS.PropertyDescriptor desc, QuickJS.Value obj,
				QuickJS.Atom prop) {
			BareboneScript * script = ctx.get_opaque ();

			var val = script->read_cpu_context_field (obj, prop);
			if (val.is_undefined ())
				return 0;

			desc.flags = ENUMERABLE;
			desc.value = val;
			desc.getter = QuickJS.Undefined;
			desc.setter = QuickJS.Undefined;
			return 1;
		}

		private static int on_cpu_context_get_own_property_names (QuickJS.Context ctx, out QuickJS.PropertyEnum * tab,
				out uint32 len, QuickJS.Value obj) {
			Gee.Map<string, Variant> * map = obj.get_opaque (cpu_context_class);

			var keys = map->keys;
			int n = keys.size;
			tab = ctx.malloc (n * sizeof (QuickJS.PropertyEnum));
			len = n;

			int i = 0;
			foreach (var key in keys) {
				QuickJS.PropertyEnum * p = tab + i;
				p->is_enumerable = true;
				p->atom = ctx.make_atom (key);
				i++;
			}

			return 0;
		}

		private static int on_cpu_context_has_property (QuickJS.Context ctx, QuickJS.Value obj, QuickJS.Atom atom) {
			Gee.Map<string, Variant> * map = obj.get_opaque (cpu_context_class);

			string * name = atom.to_cstring (ctx);
			int result = map->has_key (name) ? 1 : 0;
			ctx.free_cstring (name);

			return result;
		}

		private static QuickJS.Value on_cpu_context_get_property (QuickJS.Context ctx, QuickJS.Value obj, QuickJS.Atom atom,
				QuickJS.Value receiver) {
			BareboneScript * script = ctx.get_opaque ();

			return script->read_cpu_context_field (obj, atom);
		}

		private static int on_cpu_context_set_property (QuickJS.Context ctx, QuickJS.Value obj, QuickJS.Atom atom,
				QuickJS.Value val, QuickJS.Value receiver, QuickJS.PropertyFlags flags) {
			BareboneScript * script = ctx.get_opaque ();

			return script->write_cpu_context_field (obj, atom, val) ? 0 : -1;
		}

		private QuickJS.Value read_cpu_context_field (QuickJS.Value obj, QuickJS.Atom atom) {
			Gee.Map<string, Variant> * map = obj.get_opaque (cpu_context_class);

			QuickJS.Value result = QuickJS.Undefined;

			string * name = atom.to_cstring (ctx);

			Variant? val = map->get (name);
			if (val != null) {
				if (val.is_of_type (VariantType.UINT64)) {
					result = make_native_pointer (val.get_uint64 ());
				} else if (val.is_of_type (VariantType.UINT32)) {
					result = ctx.make_uint32 (val.get_uint32 ());
				} else {
					unowned uint8[] data = (uint8[]) val.get_data ();
					result = ctx.make_array_buffer (data[:val.get_size ()]);
				}
			}

			ctx.free_cstring (name);

			return result;
		}

		private bool write_cpu_context_field (QuickJS.Value obj, QuickJS.Atom atom, QuickJS.Value val) {
			Gee.Map<string, Variant> * map = obj.get_opaque (cpu_context_class);

			string * name = atom.to_cstring (ctx);
			try {
				Variant? existing_val = map->get (name);
				if (existing_val == null) {
					throw_js_error ("invalid register name");
					return false;
				}

				Variant new_val;
				if (existing_val.is_of_type (VariantType.UINT64)) {
					uint64 raw_val;
					if (!unparse_uint64 (val, out raw_val))
						return false;
					new_val = raw_val;
				} else if (existing_val.is_of_type (VariantType.UINT32)) {
					uint32 raw_val;
					if (!unparse_uint32 (val, out raw_val))
						return false;
					new_val = raw_val;
				} else {
					Bytes raw_val;
					if (!unparse_bytes (val, out raw_val))
						return false;
					new_val = Variant.new_from_data (new VariantType ("ay"), raw_val.get_data (), true,
						(owned) raw_val);
				}

				map->set (name, new_val);
				map->set_data ("dirty", true);
			} finally {
				ctx.free_cstring (name);
			}

			return true;
		}

		private static QuickJS.Value on_memory_alloc (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			uint size;
			if (!script->unparse_uint (argv[0], out size))
				return QuickJS.Exception;
			if (size == 0 || size > 0x7fffffff) {
				script->throw_js_error ("invalid size");
				return QuickJS.Exception;
			}

			var promise = new Promise<Barebone.Allocation> ();
			script->do_memory_alloc.begin (size, promise);

			Barebone.Allocation? allocation = script->process_events_until_ready (promise);
			if (allocation == null)
				return QuickJS.Exception;

			// TODO: Monitor lifetime and deallocate().

			return script->make_native_pointer (allocation.virtual_address);
		}

		private async void do_memory_alloc (size_t size, Promise<Barebone.Allocation> promise) {
			try {
				var allocator = services.allocator;
				size_t page_size = allocator.page_size;
				size_t alignment = (size % page_size) == 0 ? page_size : 16;
				var allocation = yield allocator.allocate (size, alignment, io_cancellable);

				Bytes zeroes = gdb.make_buffer_builder ()
					.skip (size)
					.build ();
				yield gdb.write_byte_array (allocation.virtual_address, zeroes, io_cancellable);

				promise.resolve (allocation);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_memory_protect (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			uint64 address;
			if (!script->unparse_native_pointer (argv[0], out address))
				return QuickJS.Exception;

			uint size;
			if (!script->unparse_uint (argv[1], out size))
				return QuickJS.Exception;

			Gum.PageProtection prot;
			if (!script->unparse_page_protection (argv[2], out prot))
				return QuickJS.Exception;

			var promise = new Promise<bool?> ();
			script->do_memory_protect.begin (address, size, prot, promise);

			bool? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return ctx.make_bool (result);
		}

		private async void do_memory_protect (uint64 address, size_t size, Gum.PageProtection prot, Promise<bool?> promise) {
			try {
				yield services.machine.protect_pages (address, size, prot, io_cancellable);

				promise.resolve (true);
			} catch (GLib.Error e) {
				promise.resolve (false);
			}
		}

		private static QuickJS.Value on_memory_scan (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			ScanArgs? args = ScanArgs.try_parse (argv, ASYNC, script);
			if (args == null)
				return QuickJS.Exception;

			script->do_memory_scan.begin (args);

			return QuickJS.Undefined;
		}

		private async void do_memory_scan (ScanArgs args) {
			try {
				var matches =
					yield services.machine.scan_ranges (args.ranges, args.pattern, args.max_matches, io_cancellable);

				var size_val = ctx.make_uint32 ((uint32) args.pattern.size);

				foreach (uint64 address in matches) {
					var address_val = make_native_pointer (address);
					var result = invoke (args.on_match, { address_val, size_val });

					bool proceed = true;
					if (result.is_string ()) {
						string * cstr = result.to_cstring (ctx);
						if (cstr == "stop")
							proceed = false;
						ctx.free_cstring (cstr);
					}
					ctx.free_value (result);

					ctx.free_value (address_val);

					if (!proceed)
						break;
				}
			} catch (GLib.Error e) {
				if (!args.on_error.is_undefined ()) {
					var reason_val = ctx.make_string (error_message_to_js (e.message));
					invoke_void (args.on_error, { reason_val });
					ctx.free_value (reason_val);
				}
			} finally {
				if (!args.on_complete.is_undefined ())
					invoke_void (args.on_complete, {});

				perform_pending_io ();
			}
		}

		private static QuickJS.Value on_memory_scan_sync (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			ScanArgs? args = ScanArgs.try_parse (argv, SYNC, script);
			if (args == null)
				return QuickJS.Exception;

			var promise = new Promise<QuickJS.Value?> ();
			script->do_memory_scan_sync.begin (args, promise);

			QuickJS.Value? matches = script->process_events_until_ready (promise);
			if (matches == null)
				return QuickJS.Exception;

			return matches;
		}

		private async void do_memory_scan_sync (ScanArgs args, Promise<QuickJS.Value?> promise) {
			try {
				var raw_matches =
					yield services.machine.scan_ranges (args.ranges, args.pattern, args.max_matches, io_cancellable);

				var matches = ctx.make_array ();
				uint32 i = 0;
				var size_val = ctx.make_uint32 ((uint32) args.pattern.size);
				foreach (uint64 address in raw_matches) {
					var match = ctx.make_object ();
					match.set_property (ctx, address_key, make_native_pointer (address));
					match.set_property (ctx, size_key, size_val);
					matches.set_property_uint32 (ctx, i++, match);
				}

				promise.resolve (matches);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private class ScanArgs {
			public Gee.List<Gum.MemoryRange?> ranges = new Gee.ArrayList<Gum.MemoryRange?> ();
			public Barebone.MatchPattern pattern;
			public uint max_matches = 250;
			public QuickJS.Value on_match;
			public QuickJS.Value on_error;
			public QuickJS.Value on_complete;

			private ValueScope scope;

			public enum Flavor {
				ASYNC,
				SYNC
			}

			private ScanArgs (BareboneScript script) {
				scope = new ValueScope (script);
			}

			public static ScanArgs? try_parse (QuickJS.Value[] argv, Flavor flavor, BareboneScript script) {
				var args = new ScanArgs (script);

				uint64 address;
				if (!script.unparse_native_pointer (argv[0], out address))
					return null;
				uint size;
				if (!script.unparse_uint (argv[1], out size))
					return null;
				// TODO: Support passing multiple ranges
				args.ranges.add ({ address, size });

				// TODO: Handle string | MatchPattern
				string raw_pattern;
				if (!script.unparse_string (argv[2], out raw_pattern))
					return null;
				try {
					args.pattern = new Barebone.MatchPattern.from_string (raw_pattern);
				} catch (Error e) {
					script.throw_js_error (error_message_to_js (e.message));
					return null;
				}

				// TODO: Make max_matches configurable

				if (flavor == ASYNC) {
					var callbacks = argv[3];
					var scope = args.scope;

					if (!scope.unparse_callback (callbacks, script.on_match_key, out args.on_match))
						return null;

					if (!scope.unparse_optional_callback (callbacks, script.on_error_key, out args.on_error))
						return null;

					if (!scope.unparse_optional_callback (callbacks, script.on_complete_key, out args.on_complete))
						return null;
				}

				return args;
			}
		}

		private static QuickJS.Value on_process_enumerate_ranges (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			bool coalesce = false; // TODO: Propagate to Machine.enumerate_ranges()
			Gum.PageProtection prot;

			var specifier = argv[0];
			if (specifier.is_string ()) {
				if (!script->unparse_page_protection (specifier, out prot))
					return QuickJS.Exception;
			} else {
				if (!script->unparse_page_protection (specifier.get_property (ctx, script->protection_key), out prot))
					return QuickJS.Exception;
				if (!script->unparse_bool (specifier.get_property (ctx, script->coalesce_key), out coalesce))
					return QuickJS.Exception;
			}

			var promise = new Promise<QuickJS.Value?> ();
			script->do_process_enumerate_ranges.begin (prot, promise);

			QuickJS.Value? ranges = script->process_events_until_ready (promise);
			if (ranges == null)
				return QuickJS.Exception;

			return ranges;
		}

		private async void do_process_enumerate_ranges (Gum.PageProtection prot, Promise<QuickJS.Value?> promise) {
			try {
				var ranges = ctx.make_array ();

				uint32 i = 0;
				yield services.machine.enumerate_ranges (prot, r => {
					var range = ctx.make_object ();
					range.set_property (ctx, base_key, make_native_pointer (r.base_va));
					range.set_property (ctx, size_key, ctx.make_uint32 ((uint32) r.size));
					range.set_property (ctx, protection_key, parse_page_protection (r.protection));
					if (r.type != UNKNOWN)
						range.set_property (ctx, type_key, ctx.make_string (r.type.to_nick ()));
					ranges.set_property_uint32 (ctx, i++, range);
					return true;
				}, io_cancellable);

				promise.resolve (ranges);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_file_read_all_bytes (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string filename;
			if (!script->unparse_string (argv[0], out filename))
				return QuickJS.Exception;

			uint8[] contents;
			try {
				FileUtils.get_data (filename, out contents);
			} catch (FileError e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			return script->make_array_buffer_take ((owned) contents);
		}

		private static QuickJS.Value on_file_read_all_text (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string filename;
			if (!script->unparse_string (argv[0], out filename))
				return QuickJS.Exception;

			string contents;
			size_t length;
			try {
				FileUtils.get_contents (filename, out contents, out length);
			} catch (FileError e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			char * end;
			if (!contents.validate ((ssize_t) length, out end)) {
				script->throw_js_error ("can't decode byte 0x%02x in position %u".printf (
					*end,
					(uint) (end - (char *) contents)));
				return QuickJS.Exception;
			}

			return ctx.make_string (contents);
		}

		private static QuickJS.Value on_file_write_all_bytes (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string filename;
			if (!script->unparse_string (argv[0], out filename))
				return QuickJS.Exception;

			Bytes bytes;
			if (!script->unparse_bytes (argv[1], out bytes))
				return QuickJS.Exception;

			try {
				FileUtils.set_data (filename, bytes.get_data ());
			} catch (FileError e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_file_write_all_text (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string filename;
			if (!script->unparse_string (argv[0], out filename))
				return QuickJS.Exception;

			string text;
			if (!script->unparse_string (argv[1], out text))
				return QuickJS.Exception;

			try {
				FileUtils.set_contents (filename, text);
			} catch (FileError e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_interceptor_get_breakpoint_kind (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			return script->ctx.make_string (script->services.interceptor.breakpoint_kind.to_nick ());
		}

		private static QuickJS.Value on_interceptor_set_breakpoint_kind (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string kind;
			if (!script->unparse_string (argv[0], out kind))
				return QuickJS.Exception;

			try {
				script->services.interceptor.breakpoint_kind = GDB.Breakpoint.Kind.from_nick (kind);
			} catch (Error e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_interceptor_attach (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			uint64 target;
			if (!script->unparse_native_pointer (argv[0], out target))
				return QuickJS.Exception;

			Barebone.InvocationListener? listener = null;

			var scope = new ValueScope (script);

			QuickJS.Value callbacks_or_probe = scope.take (ctx.dup_value (argv[1]));

			if (callbacks_or_probe.is_function (ctx)) {
				listener = new ScriptableBreakpointInvocationListener (script, PROBE, callbacks_or_probe, QuickJS.Undefined,
					scope);
			}

			if (listener == null) {
				uint64 cb;
				if (script->unparse_native_pointer (callbacks_or_probe, out cb))
					listener = new ScriptableInlineInvocationListener (PROBE, cb, 0, scope);
				else
					script->catch_and_ignore ();
			}

			if (listener == null) {
				QuickJS.Value on_enter_js, on_leave_js;
				uint64 on_enter_ptr, on_leave_ptr;

				if (!scope.unparse_optional_callback_or_pointer (callbacks_or_probe, script->on_enter_key, out on_enter_js,
						out on_enter_ptr)) {
					return QuickJS.Exception;
				}
				if (!scope.unparse_optional_callback_or_pointer (callbacks_or_probe, script->on_leave_key, out on_leave_js,
						out on_leave_ptr)) {
					return QuickJS.Exception;
				}

				bool any_js_style = !on_enter_js.is_undefined () || !on_leave_js.is_undefined ();
				bool any_ptr_style = on_enter_ptr != 0 || on_leave_ptr != 0;
				if (any_js_style && any_ptr_style) {
					script->throw_js_error ("callbacks must be either both functions or both pointers");
					return QuickJS.Exception;
				}

				if (any_js_style) {
					listener = new ScriptableBreakpointInvocationListener (script, CALL, on_enter_js, on_leave_js,
						scope);
				} else if (any_ptr_style) {
					listener = new ScriptableInlineInvocationListener (CALL, on_enter_ptr, on_leave_ptr, scope);
				}
			}

			if (listener == null) {
				script->throw_js_error ("expected one or more callbacks");
				return QuickJS.Exception;
			}

			var promise = new Promise<Barebone.Interceptor> ();
			script->do_interceptor_attach.begin (target, listener, promise);

			Barebone.Interceptor? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return script->wrap_invocation_listener (listener);
		}

		private async void do_interceptor_attach (uint64 target, Barebone.InvocationListener listener,
				Promise<Barebone.Interceptor> promise) {
			try {
				var interceptor = services.interceptor;

				var bpl = listener as Barebone.BreakpointInvocationListener;
				if (bpl != null) {
					yield interceptor.attach (target, bpl, io_cancellable);
				} else {
					yield interceptor.attach_inline (target, (Barebone.InlineInvocationListener) listener,
						io_cancellable);
				}

				promise.resolve (interceptor);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private class ScriptableBreakpointInvocationListener
				: Object, Barebone.InvocationListener, Barebone.BreakpointInvocationListener {
			public Kind kind {
				get { return _kind; }
			}

			private weak BareboneScript script;
			private Kind _kind;
			private QuickJS.Value _on_enter;
			private QuickJS.Value _on_leave;
			private ValueScope scope;

			public ScriptableBreakpointInvocationListener (BareboneScript script, Kind kind, QuickJS.Value on_enter,
					QuickJS.Value on_leave, ValueScope scope) {
				this.script = script;
				this._kind = kind;
				this._on_enter = on_enter;
				this._on_leave = on_leave;
				this.scope = scope;
			}

			private void on_enter (Barebone.InvocationContext ic) {
				if (_on_enter.is_undefined ())
					return;

				var closure = new InvocationClosure (script, script.wrap_invocation_context (ic));
				var args_val = script.make_invocation_args (ic);

				script.invoke_void (_on_enter, { args_val }, closure.ic_val);

				script.destroy_wrapper (args_val);

				if (!_on_leave.is_undefined ())
					ic.user_data[this] = closure;
			}

			private void on_leave (Barebone.InvocationContext ic) {
				if (_on_leave.is_undefined ())
					return;

				var closure = (InvocationClosure?) ic.user_data[this];
				if (closure == null)
					closure = new InvocationClosure (script, script.wrap_invocation_context (ic));

				var rv_val = script.make_invocation_retval (ic);

				script.invoke_void (_on_leave, { rv_val }, closure.ic_val);

				script.destroy_wrapper (rv_val);
			}

			private class InvocationClosure : Object {
				private weak BareboneScript script;
				public QuickJS.Value ic_val;

				public InvocationClosure (BareboneScript script, QuickJS.Value ic_val) {
					this.script = script;
					this.ic_val = ic_val;
				}

				~InvocationClosure () {
					script.destroy_wrapper (ic_val);
				}
			}
		}

		private class ScriptableInlineInvocationListener
				: Object, Barebone.InvocationListener, Barebone.InlineInvocationListener {
			public Kind kind {
				get { return _kind; }
			}

			public uint64 on_enter {
				get { return _on_enter; }
			}

			public uint64 on_leave {
				get { return _on_leave; }
			}

			private Kind _kind;
			private uint64 _on_enter;
			private uint64 _on_leave;
			private ValueScope scope;

			public ScriptableInlineInvocationListener (Kind kind, uint64 on_enter, uint64 on_leave, ValueScope scope) {
				this._kind = kind;
				this._on_enter = on_enter;
				this._on_leave = on_leave;
				this.scope = scope;
			}
		}

		private QuickJS.Value wrap_invocation_listener (Barebone.InvocationListener listener) {
			var wrapper = ctx.make_object_class (invocation_listener_class);
			wrapper.set_opaque (listener);
			invocation_listeners.add (listener);
			return wrapper;
		}

		private static QuickJS.Value on_invocation_listener_detach (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			Barebone.InvocationListener * listener = this_val.get_opaque (invocation_listener_class);
			if (listener == null)
				return QuickJS.Undefined;

			var promise = new Promise<Barebone.Interceptor> ();
			script->do_invocation_listener_detach.begin (listener, promise);

			Barebone.Interceptor? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			this_val.set_opaque (null);
			script->invocation_listeners.remove (listener);

			return QuickJS.Undefined;
		}

		private async void do_invocation_listener_detach (Barebone.InvocationListener listener,
				Promise<Barebone.Interceptor> promise) {
			try {
				var interceptor = services.interceptor;

				yield interceptor.detach (listener, io_cancellable);

				promise.resolve (interceptor);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private QuickJS.Value wrap_invocation_context (Barebone.InvocationContext ic) {
			var wrapper = ctx.make_object_class (invocation_context_class);
			wrapper.set_opaque (ic);
			return wrapper;
		}

		private bool try_unwrap_invocation_context (QuickJS.Value this_val, out Barebone.InvocationContext * ic) {
			return try_unwrap (this_val, invocation_context_class, out ic);
		}

		private static QuickJS.Value on_invocation_context_get_return_address (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap_invocation_context (this_val, out ic))
				return QuickJS.Exception;

			return script->make_native_pointer (ic->return_address);
		}

		private static QuickJS.Value on_invocation_context_get_context (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap_invocation_context (this_val, out ic))
				return QuickJS.Exception;

			return script->make_cpu_context (ic->registers);
		}

		private static QuickJS.Value on_invocation_context_get_thread_id (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap_invocation_context (this_val, out ic))
				return QuickJS.Exception;

			return script->ctx.make_string (ic->thread_id);
		}

		private static QuickJS.Value on_invocation_context_get_depth (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap_invocation_context (this_val, out ic))
				return QuickJS.Exception;

			return script->ctx.make_uint32 (ic->depth);
		}

		private QuickJS.Value make_invocation_args (Barebone.InvocationContext ic) {
			var wrapper = ctx.make_object_class (invocation_args_class);
			wrapper.set_opaque (ic);
			return wrapper;
		}

		private bool try_unwrap_invocation_args (QuickJS.Value this_val, out Barebone.InvocationContext * ic) {
			return try_unwrap (this_val, invocation_args_class, out ic);
		}

		private static QuickJS.Value on_invocation_args_get_property (QuickJS.Context ctx, QuickJS.Value obj, QuickJS.Atom atom,
				QuickJS.Value receiver) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap_invocation_args (obj, out ic))
				return QuickJS.Exception;

			QuickJS.Value result = QuickJS.Undefined;

			string * name = atom.to_cstring (ctx);
			uint n;
			if (uint.try_parse (name, out n))
				result = script->make_native_pointer (ic->get_nth_argument (n));
			ctx.free_cstring (name);

			return result;
		}

		private static int on_invocation_args_set_property (QuickJS.Context ctx, QuickJS.Value obj, QuickJS.Atom atom,
				QuickJS.Value val, QuickJS.Value receiver, QuickJS.PropertyFlags flags) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap_invocation_args (obj, out ic))
				return -1;

			string * name = atom.to_cstring (ctx);
			try {
				uint n;
				if (uint.try_parse (name, out n)) {
					uint64 raw_val;
					if (!script->unparse_native_pointer (val, out raw_val))
						return -1;
					ic->replace_nth_argument (n, raw_val);
				}
			} finally {
				ctx.free_cstring (name);
			}

			return 0;
		}

		private QuickJS.Value make_invocation_retval (Barebone.InvocationContext ic) {
			var wrapper = ctx.make_object_class (invocation_retval_class);
			wrapper.set_opaque (ic);
			wrapper.set_property (ctx, v_key, ctx.make_biguint64 (ic.get_return_value ()));
			return wrapper;
		}

		private static QuickJS.Value on_invocation_retval_replace (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			Barebone.InvocationContext * ic;
			if (!script->try_unwrap (this_val, invocation_retval_class, out ic))
				return QuickJS.Exception;

			uint64 raw_val;
			if (!script->unparse_native_pointer_coercible (argv[0], out raw_val))
				return QuickJS.Exception;

			this_val.set_property (ctx, script->v_key, ctx.make_biguint64 (raw_val));

			ic->replace_return_value (raw_val);

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_c_module_construct (QuickJS.Context ctx, QuickJS.Value new_target,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			Bytes blob;
			if (!script->unparse_bytes (argv[0], out blob))
				return QuickJS.Exception;

			var promise = new Promise<Barebone.CModule> ();
			script->load_c_module.begin (blob, promise);

			Barebone.CModule? mod = script->process_events_until_ready (promise);
			if (mod == null)
				return QuickJS.Exception;

			var proto = new_target.get_property (ctx, script->prototype_key);
			var wrapper = ctx.make_object_with_proto_and_class (proto, c_module_class);
			ctx.free_value (proto);

			wrapper.set_opaque (mod);
			script->c_modules.add (mod);

			foreach (var e in mod.exports)
				wrapper.set_property_str (ctx, e.name, script->make_native_pointer (e.address));

			mod.console_output.connect (script->on_console_output);

			return wrapper;
		}

		private async void load_c_module (Bytes blob, Promise<Barebone.CModule> promise) {
			try {
				var mod = yield new Barebone.CModule.from_blob (blob, services.machine, services.allocator, io_cancellable);

				promise.resolve (mod);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static void on_c_module_finalize (QuickJS.Runtime rt, QuickJS.Value val) {
			Barebone.CModule * mod = val.get_opaque (c_module_class);
			if (mod == null)
				return;

			BareboneScript * script = rt.get_opaque ();
			script->c_modules.remove (mod);
		}

		private static QuickJS.Value on_c_module_dispose (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			Barebone.CModule * mod = this_val.get_opaque (c_module_class);

			if (mod != null) {
				this_val.set_opaque (null);
				BareboneScript * script = ctx.get_opaque ();
				script->c_modules.remove (mod);
			}

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_rust_module_construct (QuickJS.Context ctx, QuickJS.Value new_target,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			var scope = new ValueScope (script);

			string source;
			if (!script->unparse_string (argv[0], out source))
				return QuickJS.Exception;

			var symbols = new Gee.HashMap<string, uint64?> ();
			var symbols_obj = argv[1];
			if (!symbols_obj.is_undefined ()) {
				QuickJS.PropertyEnum * tab;
				uint32 n;
				if (symbols_obj.get_own_property_names (ctx, out tab, out n, STRING_MASK | ENUM_ONLY) != 0)
					return QuickJS.Exception;
				unowned QuickJS.PropertyEnum[] entries = ((QuickJS.PropertyEnum[]) tab)[:n];

				try {
					foreach (var e in entries) {
						string * name = scope.take_cstring (e.atom.to_cstring (ctx));

						uint64 address;
						QuickJS.Value val = scope.take (symbols_obj.get_property (ctx, e.atom));
						if (!script->unparse_native_pointer (val, out address))
							return QuickJS.Exception;

						symbols[name] = address;

						scope.release_cstring (name);
					}
				} finally {
					foreach (var e in entries)
						ctx.free_atom (e.atom);
					ctx.free (tab);
				}
			}

			Gee.List<string> dependencies = new Gee.ArrayList<string> ();
			var options_obj = argv[2];
			if (!options_obj.is_undefined ()) {
				QuickJS.Value dependencies_val = options_obj.get_property (ctx, script->dependencies_key);
				if (dependencies_val.is_exception ())
					return QuickJS.Exception;
				if (!dependencies_val.is_undefined () && !script->unparse_string_array (dependencies_val, out dependencies))
					return QuickJS.Exception;
			}

			var promise = new Promise<Barebone.RustModule> ();
			script->load_rust_module.begin (source, symbols, dependencies, promise);

			Barebone.RustModule? mod = script->process_events_until_ready (promise);
			if (mod == null)
				return QuickJS.Exception;

			if (!symbols.is_empty)
				mod.set_data ("value-scope", (owned) scope);

			var proto = new_target.get_property (ctx, script->prototype_key);
			var wrapper = ctx.make_object_with_proto_and_class (proto, rust_module_class);
			ctx.free_value (proto);

			wrapper.set_opaque (mod);
			script->rust_modules.add (mod);

			foreach (var e in mod.exports)
				wrapper.set_property_str (ctx, e.name, script->make_native_pointer (e.address));

			mod.console_output.connect (script->on_console_output);

			return wrapper;
		}

		private async void load_rust_module (string source, Gee.Map<string, uint64?> symbols, Gee.List<string> dependencies,
				Promise<Barebone.RustModule> promise) {
			try {
				var mod = yield new Barebone.RustModule.from_string (source, symbols, dependencies, services.machine,
					services.allocator, io_cancellable);

				promise.resolve (mod);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static void on_rust_module_finalize (QuickJS.Runtime rt, QuickJS.Value val) {
			Barebone.RustModule * mod = val.get_opaque (rust_module_class);
			if (mod == null)
				return;

			BareboneScript * script = rt.get_opaque ();
			script->rust_modules.remove (mod);
		}

		private static QuickJS.Value on_rust_module_dispose (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			Barebone.RustModule * mod = this_val.get_opaque (rust_module_class);

			if (mod != null) {
				this_val.set_opaque (null);
				BareboneScript * script = ctx.get_opaque ();
				script->rust_modules.remove (mod);
			}

			return QuickJS.Undefined;
		}

		private void on_console_output (string message) {
			var builder = new Json.Builder ();
			builder
				.begin_object ()
					.set_member_name ("type")
					.add_string_value ("log")
					.set_member_name ("level")
					.add_string_value ("info")
					.set_member_name ("payload")
					.add_string_value (message)
				.end_object ();
			this.message (Json.to_string (builder.get_root (), false), null);
		}

		private static QuickJS.Value on_gdb_get_state (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			return ctx.make_string (script->gdb.state.to_nick ());
		}

		private static QuickJS.Value on_gdb_get_exception (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			GDB.Exception? exception = script->gdb.exception;
			if (exception == null)
				return QuickJS.Null;

			var result = ctx.make_object ();
			result.set_property (ctx, script->signum_key, ctx.make_uint32 (exception.signum));
			result.set_property (ctx, script->breakpoint_key, script->wrap_gdb_breakpoint_nullable (exception.breakpoint));
			result.set_property (ctx, script->thread_key, script->wrap_gdb_thread (exception.thread));
			return result;
		}

		private static QuickJS.Value on_gdb_continue (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			var promise = new Promise<GDB.Client> ();
			script->do_gdb_continue.begin (promise);

			GDB.Client? client = script->process_events_until_ready (promise);
			if (client == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_continue (Promise<GDB.Client> promise) {
			try {
				yield gdb.continue (io_cancellable);

				promise.resolve (gdb);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_stop (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			var promise = new Promise<GDB.Client> ();
			script->do_gdb_stop.begin (promise);

			GDB.Client? client = script->process_events_until_ready (promise);
			if (client == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_stop (Promise<GDB.Client> promise) {
			try {
				yield gdb.stop (io_cancellable);

				promise.resolve (gdb);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_restart (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			try {
				script->gdb.restart ();
			} catch (Error e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_gdb_read_pointer (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, script->gdb.pointer_size, script->parse_raw_pointer);
		}

		private static QuickJS.Value on_gdb_write_pointer (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_pointer);
		}

		private static QuickJS.Value on_gdb_read_s8 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 1, script->parse_raw_s8);
		}

		private static QuickJS.Value on_gdb_write_s8 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_s8);
		}

		private static QuickJS.Value on_gdb_read_u8 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 1, script->parse_raw_u8);
		}

		private static QuickJS.Value on_gdb_write_u8 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_u8);
		}

		private static QuickJS.Value on_gdb_read_s16 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 2, script->parse_raw_s16);
		}

		private static QuickJS.Value on_gdb_write_s16 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_s16);
		}

		private static QuickJS.Value on_gdb_read_u16 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 2, script->parse_raw_u16);
		}

		private static QuickJS.Value on_gdb_write_u16 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_u16);
		}

		private static QuickJS.Value on_gdb_read_s32 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 4, script->parse_raw_s32);
		}

		private static QuickJS.Value on_gdb_write_s32 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_s32);
		}

		private static QuickJS.Value on_gdb_read_u32 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 4, script->parse_raw_u32);
		}

		private static QuickJS.Value on_gdb_write_u32 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_u32);
		}

		private static QuickJS.Value on_gdb_read_s64 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 8, script->parse_raw_s64);
		}

		private static QuickJS.Value on_gdb_write_s64 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_s64);
		}

		private static QuickJS.Value on_gdb_read_u64 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 8, script->parse_raw_u64);
		}

		private static QuickJS.Value on_gdb_write_u64 (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_u64);
		}

		private static QuickJS.Value on_gdb_read_float (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 4, script->parse_raw_float);
		}

		private static QuickJS.Value on_gdb_write_float (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_float);
		}

		private static QuickJS.Value on_gdb_read_double (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_read (ctx, this_val, argv, 8, script->parse_raw_double);
		}

		private static QuickJS.Value on_gdb_write_double (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_double);
		}

		private static QuickJS.Value on_gdb_read_byte_array (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			uint size;
			if (!script->unparse_uint (argv[1], out size))
				return QuickJS.Exception;

			return script->do_gdb_read (ctx, this_val, argv, size, script->parse_raw_byte_array);
		}

		private static QuickJS.Value on_gdb_write_byte_array (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_byte_array);
		}

		private static QuickJS.Value on_gdb_read_c_string (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			ByteArray? bytes = script->do_gdb_read_null_terminated_string (ctx, this_val, argv);
			if (bytes == null)
				return QuickJS.Exception;

			unowned string raw_str = (string) bytes.data;
			string str = raw_str.make_valid ();

			return ctx.make_string (str);
		}

		private static QuickJS.Value on_gdb_read_utf8_string (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			ByteArray? bytes = script->do_gdb_read_null_terminated_string (ctx, this_val, argv);
			if (bytes == null)
				return QuickJS.Exception;

			unowned string str = (string) bytes.data;
			char * end;
			if (!str.validate (-1, out end)) {
				script->throw_js_error ("can't decode byte 0x%02x in position %u".printf (
					*((uint8 *) end),
					(uint) (end - (char *) str)));
				return QuickJS.Exception;
			}

			return ctx.make_string (str);
		}

		private ByteArray? do_gdb_read_null_terminated_string (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			uint64 address;
			if (!unparse_uint64 (argv[0], out address))
				return null;

			uint limit = 0;
			if (!argv[1].is_undefined () && !unparse_uint (argv[1], out limit))
				return null;

			var result = new ByteArray ();

			bool reached_terminator = false;
			uint offset = 0;
			uint chunk_size = 16;
			uint page_size = 4096;
			do {
				uint64 chunk_start = address + offset;

				uint64 next_page_start = (address & ~((uint64) page_size - 1)) + page_size;
				uint distance_to_next_page = (uint) (next_page_start - chunk_start);
				uint n = uint.min (chunk_size, distance_to_next_page);

				Bytes? chunk = read_memory (chunk_start, n);
				if (chunk == null)
					return null;

				foreach (uint8 byte in chunk.get_data ()) {
					if (byte == 0 || (limit != 0 && result.len == limit)) {
						reached_terminator = true;
						break;
					}
					result.append ({ byte });
					offset++;
				}

				chunk_size = uint.min (chunk_size * 2, 1024);
			} while (!reached_terminator);

			result.append ({ 0 });
			result.len--;

			return result;
		}

		private static QuickJS.Value on_gdb_write_utf8_string (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			return script->do_gdb_write (ctx, this_val, argv, script->unparse_raw_utf8_string);
		}

		private QuickJS.Value parse_raw_pointer (Buffer buffer) {
			return make_native_pointer (buffer.read_pointer (0));
		}

		private BufferBuilder? unparse_raw_pointer (QuickJS.Value val, BufferBuilder builder) {
			uint64 v;
			if (!unparse_uint64 (val, out v))
				return null;
			return builder.append_pointer (v);
		}

		private QuickJS.Value parse_raw_s8 (Buffer buffer) {
			return ctx.make_int32 (buffer.read_int8 (0));
		}

		private BufferBuilder? unparse_raw_s8 (QuickJS.Value val, BufferBuilder builder) {
			int8 v;
			if (!unparse_int8 (val, out v))
				return null;
			return builder.append_int8 (v);
		}

		private QuickJS.Value parse_raw_u8 (Buffer buffer) {
			return ctx.make_uint32 (buffer.read_uint8 (0));
		}

		private BufferBuilder? unparse_raw_u8 (QuickJS.Value val, BufferBuilder builder) {
			uint8 v;
			if (!unparse_uint8 (val, out v))
				return null;
			return builder.append_uint8 (v);
		}

		private QuickJS.Value parse_raw_s16 (Buffer buffer) {
			return ctx.make_int32 (buffer.read_int16 (0));
		}

		private BufferBuilder? unparse_raw_s16 (QuickJS.Value val, BufferBuilder builder) {
			int16 v;
			if (!unparse_int16 (val, out v))
				return null;
			return builder.append_int16 (v);
		}

		private QuickJS.Value parse_raw_u16 (Buffer buffer) {
			return ctx.make_uint32 (buffer.read_uint16 (0));
		}

		private BufferBuilder? unparse_raw_u16 (QuickJS.Value val, BufferBuilder builder) {
			uint16 v;
			if (!unparse_uint16 (val, out v))
				return null;
			return builder.append_uint16 (v);
		}

		private QuickJS.Value parse_raw_s32 (Buffer buffer) {
			return ctx.make_int32 (buffer.read_int32 (0));
		}

		private BufferBuilder? unparse_raw_s32 (QuickJS.Value val, BufferBuilder builder) {
			int32 v;
			if (!unparse_int32 (val, out v))
				return null;
			return builder.append_int32 (v);
		}

		private QuickJS.Value parse_raw_u32 (Buffer buffer) {
			return ctx.make_uint32 (buffer.read_uint32 (0));
		}

		private BufferBuilder? unparse_raw_u32 (QuickJS.Value val, BufferBuilder builder) {
			uint32 v;
			if (!unparse_uint32 (val, out v))
				return null;
			return builder.append_uint32 (v);
		}

		private QuickJS.Value parse_raw_s64 (Buffer buffer) {
			return make_int64 (buffer.read_int64 (0));
		}

		private BufferBuilder? unparse_raw_s64 (QuickJS.Value val, BufferBuilder builder) {
			int64 v;
			if (!unparse_int64 (val, out v))
				return null;
			return builder.append_int64 (v);
		}

		private QuickJS.Value parse_raw_u64 (Buffer buffer) {
			return make_uint64 (buffer.read_uint64 (0));
		}

		private BufferBuilder? unparse_raw_u64 (QuickJS.Value val, BufferBuilder builder) {
			uint64 v;
			if (!unparse_uint64 (val, out v))
				return null;
			return builder.append_uint64 (v);
		}

		private QuickJS.Value parse_raw_float (Buffer buffer) {
			return ctx.make_float64 (buffer.read_float (0));
		}

		private BufferBuilder? unparse_raw_float (QuickJS.Value val, BufferBuilder builder) {
			double d;
			if (!unparse_double (val, out d))
				return null;
			return builder.append_float ((float) d);
		}

		private QuickJS.Value parse_raw_double (Buffer buffer) {
			return ctx.make_float64 (buffer.read_double (0));
		}

		private BufferBuilder? unparse_raw_double (QuickJS.Value val, BufferBuilder builder) {
			double d;
			if (!unparse_double (val, out d))
				return null;
			return builder.append_double (d);
		}

		private QuickJS.Value parse_raw_byte_array (Buffer buffer) {
			return ctx.make_array_buffer (buffer.bytes.get_data ());
		}

		private BufferBuilder? unparse_raw_byte_array (QuickJS.Value val, BufferBuilder builder) {
			Bytes bytes;
			if (!unparse_bytes (val, out bytes))
				return null;
			return builder.append_bytes (bytes);
		}

		private BufferBuilder? unparse_raw_utf8_string (QuickJS.Value val, BufferBuilder builder) {
			string str;
			if (!unparse_string (val, out str))
				return null;
			return builder.append_string (str);
		}

		private QuickJS.Value do_gdb_read (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv, uint size,
				GdbReadResultParseFunc parse) {
			uint64 address;
			if (!unparse_uint64 (argv[0], out address))
				return QuickJS.Exception;

			Bytes? bytes = read_memory (address, size);
			if (bytes == null)
				return QuickJS.Exception;

			return parse (gdb.make_buffer (bytes));
		}

		private delegate QuickJS.Value GdbReadResultParseFunc (Buffer buffer);

		private QuickJS.Value do_gdb_write (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv,
				GdbWriteUnparseFunc unparse) {
			uint64 address;
			if (!unparse_uint64 (argv[0], out address))
				return QuickJS.Exception;

			BufferBuilder? builder = unparse (argv[1], gdb.make_buffer_builder ());
			if (builder == null)
				return QuickJS.Exception;
			Bytes bytes = builder.build ();

			if (!write_memory (address, bytes))
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private delegate BufferBuilder? GdbWriteUnparseFunc (QuickJS.Value val, BufferBuilder builder);

		private Bytes? read_memory (uint64 address, uint size) {
			var promise = new Promise<Bytes> ();
			do_read_memory.begin (address, size, promise);
			return process_events_until_ready<Bytes> (promise);
		}

		private async void do_read_memory (uint64 address, uint size, Promise<Bytes> promise) {
			try {
				Bytes bytes = yield gdb.read_byte_array (address, size, io_cancellable);

				promise.resolve (bytes);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private bool write_memory (uint64 address, Bytes bytes) {
			var promise = new Promise<GDB.Client> ();
			do_write_memory.begin (address, bytes, promise);
			return process_events_until_ready<GDB.Client> (promise) != null;
		}

		private async void do_write_memory (uint64 address, Bytes bytes, Promise<GDB.Client> promise) {
			try {
				yield gdb.write_byte_array (address, bytes, io_cancellable);

				promise.resolve (gdb);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_add_breakpoint (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			GDB.Breakpoint.Kind kind;
			if (!script->unparse_enum<GDB.Breakpoint.Kind> (argv[0], out kind))
				return QuickJS.Exception;

			uint64 address;
			if (!script->unparse_uint64 (argv[1], out address))
				return QuickJS.Exception;

			uint size;
			if (!script->unparse_uint (argv[2], out size))
				return QuickJS.Exception;

			var promise = new Promise<GDB.Breakpoint> ();
			script->do_gdb_add_breakpoint.begin (kind, address, size, promise);

			GDB.Breakpoint? bp = script->process_events_until_ready (promise);
			if (bp == null)
				return QuickJS.Exception;

			return script->wrap_gdb_breakpoint (bp);
		}

		private async void do_gdb_add_breakpoint (GDB.Breakpoint.Kind kind, uint64 address, uint size,
				Promise<GDB.Breakpoint> promise) {
			try {
				GDB.Breakpoint bp = yield gdb.add_breakpoint (kind, address, size, io_cancellable);

				promise.resolve (bp);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_run_remote_command (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string command;
			if (!script->unparse_string (argv[0], out command))
				return QuickJS.Exception;

			var promise = new Promise<string> ();
			script->do_gdb_run_remote_command.begin (command, promise);

			string? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return ctx.make_string (result);
		}

		private async void do_gdb_run_remote_command (string command, Promise<string> promise) {
			try {
				string result = yield gdb.run_remote_command (command, io_cancellable);

				promise.resolve (result);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_execute (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string command;
			if (!script->unparse_string (argv[0], out command))
				return QuickJS.Exception;

			var promise = new Promise<GDB.Client> ();
			script->do_gdb_execute.begin (command, promise);

			GDB.Client? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_execute (string command, Promise<GDB.Client> promise) {
			try {
				yield gdb.execute_simple (command, io_cancellable);

				promise.resolve (gdb);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_query (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();

			string request;
			if (!script->unparse_string (argv[0], out request))
				return QuickJS.Exception;

			var promise = new Promise<GDB.Client.Packet> ();
			script->do_gdb_query.begin (request, promise);

			GDB.Client.Packet? response = script->process_events_until_ready (promise);
			if (response == null)
				return QuickJS.Exception;

			return ctx.make_string (response.payload);
		}

		private async void do_gdb_query (string request, Promise<GDB.Client.Packet> promise) {
			try {
				GDB.Client.Packet packet = yield gdb.query_simple (request, io_cancellable);

				promise.resolve (packet);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private QuickJS.Value wrap_gdb_thread (GDB.Thread thread) {
			var wrapper = ctx.make_object_class (gdb_thread_class);
			wrapper.set_opaque (thread.ref ());
			return wrapper;
		}

		private static void on_gdb_thread_finalize (QuickJS.Runtime rt, QuickJS.Value val) {
			GDB.Thread * thread = val.get_opaque (gdb_thread_class);
			thread->unref ();
		}

		private static QuickJS.Value on_gdb_thread_get_id (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);
			return ctx.make_string (thread->id);
		}

		private static QuickJS.Value on_gdb_thread_get_name (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);
			unowned string? name = thread->name;
			if (name == null)
				return QuickJS.Null;
			return ctx.make_string (name);
		}

		private static QuickJS.Value on_gdb_thread_step (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);

			var promise = new Promise<GDB.Thread> ();
			script->do_gdb_thread_step.begin (thread, promise);

			GDB.Thread? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_thread_step (GDB.Thread thread, Promise<GDB.Thread> promise) {
			try {
				yield thread.step (io_cancellable);

				promise.resolve (thread);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_thread_step_and_continue (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);

			try {
				thread->step_and_continue ();
			} catch (Error e) {
				script->throw_js_error (error_message_to_js (e.message));
				return QuickJS.Exception;
			}

			return QuickJS.Undefined;
		}

		private static QuickJS.Value on_gdb_thread_read_registers (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);

			var promise = new Promise<Gee.Map<string, Variant>> ();
			script->do_gdb_thread_read_registers.begin (thread, promise);

			Gee.Map<string, Variant> regs = script->process_events_until_ready (promise);
			if (regs == null)
				return QuickJS.Exception;

			return script->make_cpu_context (regs);
		}

		private async void do_gdb_thread_read_registers (GDB.Thread thread, Promise<Gee.Map<string, Variant>> promise) {
			try {
				Gee.Map<string, Variant> regs = yield thread.read_registers (io_cancellable);

				promise.resolve (regs);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_thread_read_register (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);

			string name;
			if (!script->unparse_string (argv[0], out name))
				return QuickJS.Exception;

			var promise = new Promise<uint64?> ();
			script->do_gdb_thread_read_register.begin (thread, name, promise);

			uint64? val = script->process_events_until_ready (promise);
			if (val == null)
				return QuickJS.Exception;

			return script->make_native_pointer (val);
		}

		private async void do_gdb_thread_read_register (GDB.Thread thread, string name, Promise<uint64?> promise) {
			try {
				uint64 val = yield thread.read_register (name, io_cancellable);

				promise.resolve (val);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_thread_write_register (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Thread * thread = this_val.get_opaque (gdb_thread_class);

			string name;
			if (!script->unparse_string (argv[0], out name))
				return QuickJS.Exception;

			uint64 val;
			if (!script->unparse_uint64 (argv[1], out val))
				return QuickJS.Exception;

			var promise = new Promise<GDB.Thread> ();
			script->do_gdb_thread_write_register.begin (thread, name, val, promise);

			GDB.Thread? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_thread_write_register (GDB.Thread thread, string name, uint64 val, Promise<GDB.Thread> promise) {
			try {
				yield thread.write_register (name, val, io_cancellable);

				promise.resolve (thread);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private QuickJS.Value wrap_gdb_breakpoint_nullable (GDB.Breakpoint? bp) {
			if (bp == null)
				return QuickJS.Null;
			return wrap_gdb_breakpoint (bp);
		}

		private QuickJS.Value wrap_gdb_breakpoint (GDB.Breakpoint bp) {
			QuickJS.Value? existing_wrapper = gdb_breakpoints[bp];
			if (existing_wrapper != null)
				return ctx.dup_value (existing_wrapper);

			var wrapper = ctx.make_object_class (gdb_breakpoint_class);
			wrapper.set_opaque (bp.ref ());
			gdb_breakpoints[bp] = wrapper;

			return wrapper;
		}

		private static void on_gdb_breakpoint_finalize (QuickJS.Runtime rt, QuickJS.Value val) {
			GDB.Breakpoint * bp = val.get_opaque (gdb_breakpoint_class);
			bp->unref ();
		}

		private static QuickJS.Value on_gdb_breakpoint_get_kind (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			GDB.Breakpoint * bp = this_val.get_opaque (gdb_breakpoint_class);
			return ctx.make_string (bp->kind.to_nick ());
		}

		private static QuickJS.Value on_gdb_breakpoint_get_address (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Breakpoint * bp = this_val.get_opaque (gdb_breakpoint_class);
			return script->make_native_pointer (bp->address);
		}

		private static QuickJS.Value on_gdb_breakpoint_get_size (QuickJS.Context ctx, QuickJS.Value this_val,
				QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Breakpoint * bp = this_val.get_opaque (gdb_breakpoint_class);
			return script->ctx.make_uint32 ((uint32) bp->size);
		}

		private static QuickJS.Value on_gdb_breakpoint_enable (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Breakpoint * bp = this_val.get_opaque (gdb_breakpoint_class);

			var promise = new Promise<GDB.Breakpoint> ();
			script->do_gdb_breakpoint_enable.begin (bp, promise);

			GDB.Breakpoint? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_breakpoint_enable (GDB.Breakpoint bp, Promise<GDB.Breakpoint> promise) {
			try {
				yield bp.enable (io_cancellable);

				promise.resolve (bp);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_breakpoint_disable (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Breakpoint * bp = this_val.get_opaque (gdb_breakpoint_class);

			var promise = new Promise<GDB.Breakpoint> ();
			script->do_gdb_breakpoint_disable.begin (bp, promise);

			GDB.Breakpoint? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_breakpoint_disable (GDB.Breakpoint bp, Promise<GDB.Breakpoint> promise) {
			try {
				yield bp.disable (io_cancellable);

				promise.resolve (bp);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private static QuickJS.Value on_gdb_breakpoint_remove (QuickJS.Context ctx, QuickJS.Value this_val, QuickJS.Value[] argv) {
			BareboneScript * script = ctx.get_opaque ();
			GDB.Breakpoint * bp = this_val.get_opaque (gdb_breakpoint_class);

			var promise = new Promise<GDB.Breakpoint> ();
			script->do_gdb_breakpoint_remove.begin (bp, promise);

			GDB.Breakpoint? result = script->process_events_until_ready (promise);
			if (result == null)
				return QuickJS.Exception;

			return QuickJS.Undefined;
		}

		private async void do_gdb_breakpoint_remove (GDB.Breakpoint bp, Promise<GDB.Breakpoint> promise) {
			try {
				yield bp.remove (io_cancellable);

				promise.resolve (bp);
			} catch (GLib.Error e) {
				promise.reject (e);
			}
		}

		private T? process_events_until_ready<T> (Promise<T> promise) {
			var future = promise.future;
			var main_context = MainContext.get_thread_default ();
			while (!future.ready)
				main_context.iteration (true);

			GLib.Error? error = future.error;
			if (error != null) {
				throw_js_error (error_message_to_js (error.message));
				return null;
			}

			return future.value;
		}

		private void perform_pending_io () {
			bool io_performed = false;
			do {
				io_performed = false;

				unowned QuickJS.Context? c = null;
				do {
					int res = rt.execute_pending_job (out c);
					if (res == -1)
						catch_and_emit ();
				} while (c != null);

				QuickJS.Value? cb;
				while ((cb = tick_callbacks.poll ()) != null) {
					invoke_void (cb);
					ctx.free_value (cb);

					io_performed = true;
				}
			} while (io_performed);
		}

		private bool unparse_string (QuickJS.Value val, out string str) {
			string * cstr = val.to_cstring (ctx);
			if (cstr == null) {
				str = null;
				return false;
			}
			str = cstr;
			ctx.free_cstring (cstr);
			return true;
		}

		private bool unparse_string_array (QuickJS.Value val, out Gee.List<string> strings) {
			strings = new Gee.ArrayList<string> ();

			if (!val.is_array (ctx)) {
				throw_js_error ("expected an array of strings");
				return false;
			}

			var length_val = val.get_property (ctx, length_key);
			if (length_val.is_exception ())
				return false;
			uint32 length;
			if (length_val.to_uint32 (ctx, out length) != 0)
				return false;
			ctx.free_value (length_val);

			for (uint32 i = 0; i != length; i++) {
				var element = val.get_property_uint32 (ctx, i);
				if (element.is_exception ())
					return false;
				try {
					string * cstr = element.to_cstring (ctx);
					if (cstr == null)
						return false;
					strings.add (cstr);
					ctx.free_cstring (cstr);
				} finally {
					ctx.free_value (element);
				}
			}

			return true;
		}

		private bool unparse_bool (QuickJS.Value val, out bool b) {
			b = false;

			int result = val.to_bool (ctx);
			if (result == -1)
				return false;

			b = (bool) result;
			return true;
		}

		private bool unparse_bytes (QuickJS.Value val, out Bytes bytes) {
			bytes = null;

			unowned uint8[]? data = val.get_array_buffer (ctx);

			var exception = ctx.get_exception ();
			bool buffer_is_empty = data == null && exception.is_null ();
			ctx.free_value (exception);

			bool is_array_buffer = data != null || buffer_is_empty;
			if (is_array_buffer) {
				bytes = new Bytes (data);
				return true;
			}

			size_t byte_offset = 0;
			size_t byte_length = 0;
			var buf = val.get_typed_array_buffer (ctx, &byte_offset, &byte_length);
			if (!buf.is_exception ()) {
				unowned uint8[]? whole_buf = buf.get_array_buffer (ctx);
				bytes = new Bytes (whole_buf[byte_offset:byte_offset + byte_length]);
				ctx.free_value (buf);
				return true;
			} else {
				ctx.free_value (ctx.get_exception ());
			}

			if (!val.is_array (ctx)) {
				throw_js_error ("expected a buffer-like object");
				return false;
			}

			var length_val = val.get_property (ctx, length_key);
			if (length_val.is_exception ())
				return false;
			uint32 length;
			if (length_val.to_uint32 (ctx, out length) != 0)
				return false;
			ctx.free_value (length_val);
			if (length > MAX_JS_BYTE_ARRAY_LENGTH) {
				throw_js_error ("array too large, use ArrayBuffer instead");
				return false;
			}

			var elements = new uint8[length];
			for (uint32 i = 0; i != length; i++) {
				var element = val.get_property_uint32 (ctx, i);
				if (element.is_exception ())
					return false;
				try {
					uint8 byte;
					if (!unparse_uint8 (element, out byte))
						return false;
					elements[i] = byte;
				} finally {
					ctx.free_value (element);
				}
			}
			bytes = new Bytes (elements);
			return true;
		}

		private bool unparse_uint (QuickJS.Value val, out uint uval) {
			uval = uint.MAX;

			uint32 v;
			if (val.to_uint32 (ctx, out v) != 0)
				return false;

			uval = v;
			return true;
		}

		private bool unparse_int8 (QuickJS.Value val, out int8 result) {
			result = -1;

			int32 v;
			if (!unparse_int32 (val, out v))
				return false;

			if (v < int8.MIN || v > int8.MAX) {
				throw_js_error ("expected a signed 8-bit integer");
				return false;
			}

			result = (int8) v;
			return true;
		}

		private bool unparse_uint8 (QuickJS.Value val, out uint8 result) {
			result = uint8.MAX;

			uint32 v;
			if (!unparse_uint32 (val, out v))
				return false;

			if (v > uint8.MAX) {
				throw_js_error ("expected an unsigned 8-bit integer");
				return false;
			}

			result = (uint8) v;
			return true;
		}

		private bool unparse_int16 (QuickJS.Value val, out int16 result) {
			result = -1;

			int32 v;
			if (!unparse_int32 (val, out v))
				return false;

			if (v < int16.MIN || v > int16.MAX) {
				throw_js_error ("expected a signed 16-bit integer");
				return false;
			}

			result = (int16) v;
			return true;
		}

		private bool unparse_uint16 (QuickJS.Value val, out uint16 result) {
			result = uint16.MAX;

			uint32 v;
			if (!unparse_uint32 (val, out v))
				return false;

			if (v > uint16.MAX) {
				throw_js_error ("expected an unsigned 16-bit integer");
				return false;
			}

			result = (uint16) v;
			return true;
		}

		private bool unparse_int32 (QuickJS.Value val, out int32 result) {
			return val.to_int32 (ctx, out result) == 0;
		}

		private bool unparse_uint32 (QuickJS.Value val, out uint32 result) {
			return val.to_uint32 (ctx, out result) == 0;
		}

		private bool unparse_int64 (QuickJS.Value val, out int64 result) {
			string * cstr = val.to_cstring (ctx);
			if (cstr == null) {
				result = -1;
				return false;
			}

			result = int64.parse (cstr);

			ctx.free_cstring (cstr);

			return true;
		}

		private bool unparse_uint64 (QuickJS.Value val, out uint64 result) {
			string * cstr = val.to_cstring (ctx);
			if (cstr == null) {
				result = uint64.MAX;
				return false;
			}

			result = uint64.parse (cstr);

			ctx.free_cstring (cstr);

			return true;
		}

		private bool unparse_double (QuickJS.Value val, out double result) {
			return val.to_float64 (ctx, out result) == 0;
		}

		private bool unparse_enum<T> (QuickJS.Value val, out int result) {
			result = -1;

			string * nick = val.to_cstring (ctx);
			if (nick == null)
				return false;

			try {
				result = (int) Marshal.enum_from_nick<T> (nick);
			} catch (Error e) {
				throw_js_error (error_message_to_js (e.message));
				return false;
			} finally {
				ctx.free_cstring (nick);
			}

			return true;
		}

		private bool unparse_native_pointer (QuickJS.Value val, out uint64 address) {
			address = 0;

			var v = val.get_property (ctx, v_key);
			if (v.is_exception ())
				return false;

			if (v.is_undefined ()) {
				var handle = val.get_property (ctx, handle_key);
				if (handle.is_exception ())
					return false;
				v = handle.get_property (ctx, v_key);
				if (v.is_undefined ()) {
					throw_js_error ("expected a NativePointer value");
					return false;
				}
			}

			bool success = unparse_uint64 (v, out address);

			ctx.free_value (v);

			return success;
		}

		private bool unparse_native_pointer_coercible (QuickJS.Value val, out uint64 address) {
			if (val.is_object ())
				return unparse_native_pointer (val, out address);

			var np_val = ptr_func.call (ctx, QuickJS.Undefined, { val });
			if (np_val.is_exception ()) {
				address = 0;
				return false;
			}
			bool success = unparse_native_pointer (np_val, out address);
			ctx.free_value (np_val);

			return success;
		}

		private class ValueScope {
			public unowned QuickJS.Context ctx;

			private weak BareboneScript script;
			private Gee.List<QuickJS.Value?>? values;
			private Gee.List<string *>? cstrings;

			public ValueScope (BareboneScript script) {
				this.ctx = script.ctx;
				this.script = script;
			}

			~ValueScope () {
				if (values != null) {
					foreach (var v in values)
						ctx.free_value (v);
				}
				if (cstrings != null) {
					foreach (var s in cstrings)
						ctx.free_cstring (s);
				}
			}

			public QuickJS.Value retain (QuickJS.Value v) {
				var result = ctx.dup_value (v);
				take (result);
				return result;
			}

			public QuickJS.Value take (QuickJS.Value v) {
				if (values == null)
					values = new Gee.ArrayList<QuickJS.Value?> ();
				values.add (v);
				return v;
			}

			public void release (QuickJS.Value v) {
				values.remove (v);
				ctx.free_value (v);
			}

			public string * take_cstring (string * s) {
				if (cstrings == null)
					cstrings = new Gee.ArrayList<string *> ();
				cstrings.add (s);
				return s;
			}

			public void release_cstring (string * s) {
				cstrings.remove (s);
				ctx.free_cstring (s);
			}

			public bool unparse_callback (QuickJS.Value obj, QuickJS.Atom name, out QuickJS.Value cb) {
				return do_unparse_callback (obj, name, true, out cb);
			}

			public bool unparse_optional_callback (QuickJS.Value obj, QuickJS.Atom name, out QuickJS.Value cb) {
				return do_unparse_callback (obj, name, false, out cb);
			}

			private bool do_unparse_callback (QuickJS.Value obj, QuickJS.Atom name, bool required, out QuickJS.Value cb) {
				cb = QuickJS.Undefined;

				QuickJS.Value val;
				if (!do_unparse_property (obj, name, required, out val))
					return false;

				if (required && !val.is_function (ctx)) {
					release (val);

					var name_str = name.to_cstring (ctx);
					script.throw_js_error ("expected %s to be a function".printf (name_str));
					ctx.free_cstring (name_str);

					return false;
				}

				cb = val;
				return true;
			}

			public bool unparse_optional_callback_or_pointer (QuickJS.Value obj, QuickJS.Atom name, out QuickJS.Value cb,
					out uint64 ptr) {
				return do_unparse_callback_or_pointer (obj, name, false, out cb, out ptr);
			}

			private bool do_unparse_callback_or_pointer (QuickJS.Value obj, QuickJS.Atom name, bool required,
					out QuickJS.Value cb, out uint64 ptr) {
				cb = QuickJS.Undefined;
				ptr = 0;

				QuickJS.Value val;
				if (!do_unparse_property (obj, name, required, out val))
					return false;

				if (!required && val.is_undefined ())
					return true;

				if (val.is_function (ctx)) {
					cb = val;
				} else if (!script.unparse_native_pointer (val, out ptr)) {
					script.catch_and_ignore ();

					release (val);

					var name_str = name.to_cstring (ctx);
					script.throw_js_error ("expected %s to be either a function or a pointer".printf (name_str));
					ctx.free_cstring (name_str);

					return false;
				}

				return true;
			}

			private bool do_unparse_property (QuickJS.Value obj, QuickJS.Atom name, bool required, out QuickJS.Value prop) {
				prop = QuickJS.Undefined;

				var val = obj.get_property (ctx, name);
				if (val.is_exception ())
					return false;

				if (val.is_undefined ()) {
					if (!required)
						return true;
					var name_str = name.to_cstring (ctx);
					script.throw_js_error ("missing %s".printf (name_str));
					ctx.free_cstring (name_str);
					return false;
				}

				prop = take (val);

				return true;
			}
		}

		private QuickJS.Value parse_page_protection (Gum.PageProtection prot) {
			char str[4] = {
				((prot & Gum.PageProtection.READ) != 0) ? 'r' : '-',
				((prot & Gum.PageProtection.WRITE) != 0) ? 'w' : '-',
				((prot & Gum.PageProtection.EXECUTE) != 0) ? 'x' : '-',
				'\0'
			};
			return ctx.make_string ((string) str);
		}

		private bool unparse_page_protection (QuickJS.Value val, out Gum.PageProtection prot) {
			prot = NO_ACCESS;

			string * str = val.to_cstring (ctx);
			if (str == null)
				return false;

			try {
				uint n = str->length;
				for (uint i = 0; i != n; i++) {
					switch (str->get (i)) {
						case 'r':
							prot |= READ;
							break;
						case 'w':
							prot |= WRITE;
							break;
						case 'x':
							prot |= EXECUTE;
							break;
						case '-':
							break;
						default:
							throw_js_error ("expected a string specifying memory protection");
							return false;
					}
				}
			} finally {
				ctx.free_cstring (str);
			}

			return true;
		}

		private QuickJS.Value invoke (QuickJS.Value callback, QuickJS.Value[] argv = {}, QuickJS.Value thiz = QuickJS.Undefined) {
			var result = callback.call (ctx, thiz, argv);
			if (result.is_exception ())
				catch_and_emit ();
			return result;
		}

		private void invoke_void (QuickJS.Value callback, QuickJS.Value[] argv = {}, QuickJS.Value thiz = QuickJS.Undefined) {
			var result = invoke (callback, argv, thiz);
			ctx.free_value (result);
		}

		private QuickJS.Value make_js_error (string message) {
			var err = ctx.make_error ();
			err.set_property (ctx, message_key, ctx.make_string (message));
			return err;
		}

		private void throw_js_error (string message) {
			ctx.throw (make_js_error (message));
		}

		private JSError catch_js_error () {
			var exception_val = ctx.get_exception ();
			var message_val = exception_val.get_property (ctx, message_key);
			var message_str = message_val.to_cstring (ctx);
			var line_val = exception_val.get_property (ctx, line_number_key);

			uint32 raw_line;
			line_val.to_uint32 (ctx, out raw_line);

			JSError err = new JSError (message_str, raw_line + 1);

			ctx.free_value (line_val);
			ctx.free_cstring (message_str);
			ctx.free_value (message_val);
			ctx.free_value (exception_val);

			return err;
		}

		private void catch_and_emit () {
			var val = ctx.get_exception ();
			on_unhandled_exception (val);
			ctx.free_value (val);
		}

		private void catch_and_ignore () {
			ctx.free_value (ctx.get_exception ());
		}

		private void on_unhandled_exception (QuickJS.Value e) {
			if (runtime_obj.is_undefined ()) {
				emit_early_exception (e);
				return;
			}

			var result = dispatch_exception_func.call (ctx, runtime_obj, { e });
			if (result.is_exception ()) {
				var val = ctx.get_exception ();
				emit_early_exception (val);
				ctx.free_value (val);
			}
			ctx.free_value (result);
		}

		private void emit_early_exception (QuickJS.Value val) {
			string * description = val.to_cstring (ctx);

			var builder = new Json.Builder ();
			builder
				.begin_object ()
					.set_member_name ("type")
					.add_string_value ("error")
					.set_member_name ("description")
					.add_string_value (description)
				.end_object ();
			message (Json.to_string (builder.get_root (), false), null);

			ctx.free_cstring (description);
		}

		private static string error_message_to_js (string message) {
			return "%c%s".printf (message[0].tolower (), message.substring (1));
		}

		private bool try_unwrap<T> (QuickJS.Value this_val, QuickJS.ClassID class_id, out T * handle) {
			handle = this_val.get_opaque (class_id);
			if (handle == null) {
				throw_js_error ("invalid operation");
				return false;
			}
			return true;
		}

		private void destroy_wrapper (QuickJS.Value val) {
			val.set_opaque (null);
			ctx.free_value (val);
		}

		private class Asset {
			public string name;
			public string data;

			public Asset (string name, owned string data) {
				this.name = name;
				this.data = (owned) data;
			}
		}

		private class JSError {
			public string message;
			public uint line;

			public JSError (owned string message, uint line) {
				this.message = (owned) message;
				this.line = line;
			}
		}
	}
}
