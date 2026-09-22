[CCode (gir_namespace = "FridaBarebone", gir_version = "1.0")]
namespace Frida.Barebone {
	private sealed class RustModule : Object {
		public signal void console_output (string message);

		public Gee.List<Export> exports {
			get;
			default = new Gee.ArrayList<Export> ();
		}

		public class Export {
			public string name;
			public uint64 address;

			internal Export (string name, uint64 address) {
				this.name = name;
				this.address = address;
			}
		}

		private Gum.ElfModule elf;
		private Allocation allocation;
		private Callback console_log_callback;

		public async RustModule.from_string (string str, Gee.Map<string, uint64?> symbols, Gee.List<string> dependencies,
				Machine machine, Allocator allocator, Cancellable? cancellable) throws Error, IOError {
			var assets = yield new CompilationAssets (str, symbols, dependencies, machine, cancellable);

			int exit_status;
			string output;
			try {
				var launcher = new SubprocessLauncher (STDIN_PIPE | STDOUT_PIPE | STDERR_MERGE);
				launcher.set_cwd (assets.workdir.get_path ());
				launcher.setenv ("TERM", "dumb", true);

				Subprocess tool;
				if (dependencies.is_empty) {
					var argv = new Gee.ArrayList<string?> ();

					argv.add_all_array ({
						"rustc",
						"--crate-type", "bin",
						"--crate-name", CRATE_NAME,
						"--edition", EDITION,
						"--target", machine.llvm_target,
					});

					foreach (unowned string opt in BASE_CODEGEN_OPTIONS) {
						argv.add ("--codegen");
						argv.add (opt.replace (" = ", "=").replace ("\"", ""));
					}
					argv.add_all_array ({ "--codegen", "code-model=" + machine.llvm_code_model });

					foreach (unowned string flag in BASE_LINKER_FLAGS)
						argv.add_all_array ({ "--codegen", "link-arg=" + flag });

					argv.add_all_array ({
						"-o", assets.workdir.get_relative_path (assets.output_elf),
						assets.workdir.get_relative_path (assets.main_rs),
					});

					argv.add (null);

					tool = launcher.spawnv (argv.to_array ());
				} else {
					tool = launcher.spawn (
						"cargo",
						"build",
						"--release",
						"--target", machine.llvm_target);
				}

				yield tool.communicate_utf8_async (null, cancellable, out output, null);
				exit_status = tool.get_exit_status ();
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
			if (exit_status != 0)
				throw new Error.INVALID_ARGUMENT ("Compilation failed: %s", output.chomp ());

			try {
				elf = new Gum.ElfModule.from_file (assets.output_elf.get_path ());
			} catch (Gum.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			var raw_elf = new Bytes.static (elf.get_file_data ());

			size_t page_size = yield machine.query_page_size (cancellable);

			allocation = yield inject_elf (elf, raw_elf, page_size, machine, allocator, uploaded => {}, cancellable);

			uint64 base_va = allocation.virtual_address;
			uint64 console_log_trap = 0;
			elf.enumerate_symbols (e => {
				if (e.name == "")
					return true;

				if (e.name[0] == '_') {
					if (e.name == "_console_log")
						console_log_trap = base_va + e.address;
					return true;
				}

				exports.add (new Export (e.name, base_va + e.address));

				return true;
			});

			if (console_log_trap != 0) {
				var handler = new ConsoleLogHandler (machine.gdb);
				handler.output.connect (on_console_output);
				console_log_callback = yield new Callback (console_log_trap, handler, machine, cancellable);
			}
		}

		private void on_console_output (string message) {
			console_output (message);
		}

		private const string CRATE_NAME = "rustmodule";
		private const string EDITION = "2021";

		private const string[] BASE_CODEGEN_OPTIONS = {
			"panic = \"abort\"",
			"opt-level = \"z\"",
			"overflow-checks = false",
			"lto = true",
			"codegen-units = 1",
		};

		private const string[] BASE_LINKER_FLAGS = {
			"--export-dynamic",
			"--emit-relocs",
			"--nmagic",
			"--discard-all",
			"--strip-debug",
			"--script=module.lds",
		};

		private class CompilationAssets {
			public File workdir;
			public File main_rs;
			public File output_elf;

			public async CompilationAssets (string code, Gee.Map<string, uint64?> symbols, Gee.List<string> dependencies,
					Machine machine, Cancellable? cancellable) throws Error, IOError {
				try {
					int io_priority = Priority.DEFAULT;

					workdir = yield File.new_tmp_dir_async (CRATE_NAME + "-XXXXXX", io_priority, cancellable);

					var src = workdir.resolve_relative_path ("src");
					yield src.make_directory_async (io_priority, cancellable);

					main_rs = yield write_text_file (src, "main.rs", make_main_rs (code, machine), cancellable);

					if (dependencies.is_empty) {
						output_elf = workdir.resolve_relative_path (CRATE_NAME + ".elf");
					} else {
						yield write_text_file (workdir, "Cargo.toml", make_cargo_toml (dependencies, machine),
							cancellable);
						yield write_text_file (workdir, "build.rs", make_build_rs (), cancellable);

						output_elf = workdir
							.resolve_relative_path ("target")
							.resolve_relative_path (machine.llvm_target)
							.resolve_relative_path ("release")
							.resolve_relative_path (CRATE_NAME);
					}

					yield write_text_file (workdir, "module.lds", make_linker_script (code, symbols), cancellable);
				} catch (GLib.Error e) {
					throw new Error.PERMISSION_DENIED ("%s", e.message);
				}
			}

			~CompilationAssets () {
				try {
					FS.rmtree (workdir);
				} catch (Error e) {
				}
			}

			private static string make_main_rs (string code, Machine machine) {
				var main_rs = new StringBuilder.sized (1024);

				main_rs
					.append (prettify_text_asset (BUILTINS))
					.append_c ('\n');

				if (machine.gdb.arch == ARM64)
					main_rs.append (prettify_text_asset (BUILTINS_ARM64));

				main_rs.append (code);

				return main_rs.str;
			}

			private static string make_cargo_toml (Gee.List<string> dependencies, Machine machine) {
				var toml = new StringBuilder.sized (512);

				toml
					.append ("[package]\n")
					.append ("name = \"").append (CRATE_NAME).append ("\"\n")
					.append ("version = \"1.0.0\"\n")
					.append ("edition = \"").append (EDITION).append ("\"\n")
					.append ("build = \"build.rs\"\n");

				toml.append ("\n[profile.release]\n");
				foreach (unowned string opt in BASE_CODEGEN_OPTIONS) {
					toml
						.append (opt)
						.append_c ('\n');
				}
				toml.append_printf ("code-model = \"%s\"\n", machine.llvm_code_model);

				if (!dependencies.is_empty) {
					toml.append ("\n[dependencies]\n");
					foreach (string dep in dependencies) {
						toml
							.append (dep)
							.append_c ('\n');
					}
				}

				return toml.str;
			}

			private static string make_build_rs () {
				var rs = new StringBuilder.sized (512);

				rs.append ("fn main() {\n");
				foreach (unowned string flag in BASE_LINKER_FLAGS)
					rs.append_printf ("    println!(\"cargo:rustc-link-arg={}\", \"%s\");\n", flag);
				rs.append ("}\n");

				return rs.str;
			}

			private static string make_linker_script (string code, Gee.Map<string, uint64?> symbols) {
				var script = new StringBuilder.sized (512);

				foreach (var e in symbols.entries) {
					unowned string name = e.key;
					uint64 address = e.value;
					script
						.append (name)
						.append (" = ")
						.append_printf ("0x%" + uint64.FORMAT_MODIFIER + "x;\n", address);
				}

				script.append ("""
				SECTIONS {
					.text : {
						*(.text*);
				""");

				foreach (string name in extract_extern_c_symbols (code))
					script.append_printf ("		KEEP(*(.text.%s))\n", name);

				script.append ("""
						_console_log = .;
						. += 8;
					}
					.rodata : {
						*(.rodata*)
					}
					.data.rel.ro : {
						*(.data.rel.ro*)
					}
					.got : {
						*(.got*)
					}
					.bss : {
						*(.bss*)
					}
				}
				""");

				return prettify_text_asset (script.str);
			}

			private static Gee.List<string> extract_extern_c_symbols (string code) {
				var names = new Gee.ArrayList<string> ();

				var export_pattern = /\#\[\s*no_mangle\s*\]\s*pub\s+(?:unsafe\s+)?extern\s*"C"\s+fn\s+([A-Za-z_]\w*)\s*\(/;

				try {
					MatchInfo info;
					for (export_pattern.match (code, 0, out info); info.matches (); info.next ())
						names.add (info.fetch (1));
				} catch (RegexError e) {
				}

				return names;
			}

			private const string BUILTINS = """
				#![no_main]
				#![no_std]

				#[macro_use]
				mod console {
					use core::str;

					macro_rules! println {
						() => {
							$crate::println!("")
						};
						( $( $arg:tt )* ) => {
							use core::fmt::Write;
							let mut sink = $crate::console::MessageBuffer::new();
							sink.write_fmt(format_args!($($arg)*)).ok();
							$crate::console::log(&sink.message())
						}
					}

					pub fn log(message: &str) {
						unsafe { _console_log(message.as_ptr(), message.as_bytes().len()) }
					}

					extern "C" {
						fn _console_log(message: *const u8, len: usize);
					}

					pub struct MessageBuffer {
						buf: [u8; 128],
						len: usize,
					}

					impl MessageBuffer {
						pub const fn new() -> Self {
							Self {
								buf: [0_u8; 128],
								len: 0,
							}
						}

						pub fn message(&self) -> &str {
							unsafe { str::from_utf8_unchecked(&self.buf[..self.len]) }
						}
					}

					impl core::fmt::Write for MessageBuffer {
						fn write_str(&mut self, s: &str) -> core::fmt::Result {
							let data = s.as_bytes();
							let capacity = self.buf.len() - self.len;
							let n = core::cmp::min(data.len(), capacity);
							let region = match n {
								0 => return Ok(()),
								_ => &mut self.buf[self.len..self.len + n],
							};
							region.copy_from_slice(data);
							self.len += n;
							Ok(())
						}
					}
				}

				#[panic_handler]
				fn panic(info: &core::panic::PanicInfo<'_>) -> ! {
					println!("{}", info);
					loop {}
				}
			""";

			private const string BUILTINS_ARM64 = """
				mod gum {
					#[repr(C)]
					pub struct InvocationContext {
						pub cpu_context: Arm64CpuContext,
					}

					#[repr(C)]
					pub struct Arm64CpuContext {
						pub pc: u64,
						pub sp: u64,
						pub nzcv: u64,

						pub x: [u64; 29],
						pub fp: u64,
						pub lr: u64,

						pub v: [Arm64VectorReg; 32],
					}

					#[repr(C)]
					pub union Arm64VectorReg {
						pub q: [u8; 16],
						pub d: f64,
						pub s: f32,
						pub h: u16,
						pub b: u8,
					}
				}
			""";
		}
	}

	private string prettify_text_asset (string text) {
		var result = new StringBuilder.sized (1024);

		foreach (unowned string line in text.strip ().split ("\n")) {
			if (line.has_prefix ("\t\t\t\t"))
				result.append (line[4:]);
			else
				result.append (line);
			result.append_c ('\n');
		}

		return result.str;
	}

	private async File write_text_file (File parent_dir, string filename, string content, Cancellable? cancellable)
			throws GLib.Error {
		File file = parent_dir.resolve_relative_path (filename);
		yield file.replace_contents_async (content.data, null, false, FileCreateFlags.NONE, cancellable, null);
		return file;
	}
}
