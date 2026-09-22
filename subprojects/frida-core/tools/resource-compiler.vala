namespace Frida {
	public enum Toolchain {
		MICROSOFT,
		APPLE,
		GNU
	}

	public enum Machine {
		ANY,
		X86,
		X86_64,
		ARM,
		ARM64,
		MIPS,
	}

	public sealed class ResourceCompiler {
		private static Toolchain toolchain;
		private static Machine machine;
		private static string toolchain_name;
		private static string machine_name;
		private static string config_filename;
		private static string output_basename;
		[CCode (array_length = false, array_null_terminated = true)]
		private static string[] input_filenames;

		private const OptionEntry[] options = {
			{ "toolchain", 't', 0, OptionArg.STRING, ref toolchain_name, "Generate output for TOOLCHAIN", "TOOLCHAIN" },
			{ "machine", 'm', 0, OptionArg.STRING, ref machine_name, "Generate output for MACHINE", "MACHINE" },
			{ "config-filename", 'c', 0, OptionArg.FILENAME, ref config_filename, "Read configuration from CONFIGFILE", "CONFIGFILE" },
			{ "output-basename", 'o', 0, OptionArg.FILENAME, ref output_basename, "Place output in BASENAME", "BASENAME" },
			{ "", 0, 0, OptionArg.FILENAME_ARRAY, ref input_filenames, null, "FILE..." },
			{ null }
		};

		public static int main (string[] args) {
#if WINDOWS
			toolchain = Toolchain.MICROSOFT;
#elif DARWIN
			toolchain = Toolchain.APPLE;
#else
			toolchain = Toolchain.GNU;
#endif
			machine = Machine.ANY;

			try {
				var ctx = new OptionContext ("- Vala Resource Compiler");
				ctx.set_help_enabled (true);
				ctx.add_main_entries (options, null);
				ctx.parse (ref args);
			} catch (OptionError e) {
				stdout.printf ("%s\n", e.message);
				stdout.printf ("Run '%s --help' to see a full list of available command line options.\n", args[0]);
				return 1;
			}

			if (toolchain_name != null) {
				switch (toolchain_name) {
					case "microsoft":
						toolchain = Toolchain.MICROSOFT;
						break;
					case "apple":
						toolchain = Toolchain.APPLE;
						break;
					case "gnu":
						toolchain = Toolchain.GNU;
						break;
					default:
						stderr.printf ("Invalid toolchain. Specify either `microsoft`, `apple` or `gnu`.\n");
						return 1;
				}
			}

			if (machine_name != null) {
				switch (machine_name.down ()) {
					case "any":
						machine = Machine.ANY;
						break;
					case "x86":
						machine = Machine.X86;
						break;
					case "x86_64":
					case "x64":
						machine = Machine.X86_64;
						break;
					case "arm":
						machine = Machine.ARM;
						break;
					case "arm64":
						machine = Machine.ARM64;
						break;
					case "mips":
						machine = Machine.MIPS;
						break;
					default:
						stderr.printf ("Invalid machine. Must be one of: any, x86, x86_64, x64, arm, arm64, or mips.\n");
						return 1;
				}
			}

			if (toolchain == Toolchain.MICROSOFT && machine == Machine.ANY) {
				stderr.printf ("Machine must be specified.\n");
				return 1;
			}

			if (config_filename == null) {
				stderr.printf ("No config file specified.\n");
				return 1;
			}

			if (output_basename == null) {
				stderr.printf ("No output basename specified.\n");
				return 1;
			}

			var compiler = new ResourceCompiler ();
			try {
				compiler.run ();
			} catch (Error e) {
				stderr.printf ("%s\n", e.message);
				return 1;
			}

			return 0;
		}

		private void run () throws Error {
			var input_dir = File.new_for_path (Path.get_dirname (config_filename));
			string output_namespace;

			var categories = new Gee.ArrayList<ResourceCategory> ();

			var root_category = new ResourceCategory ("root");
			foreach (var filename in input_filenames) {
				var tokens = filename.split ("!", 2);
				if (tokens.length == 2) {
					root_category.files.add (new ResourceFile (tokens[1], tokens[0]));
				} else {
					root_category.files.add (new ResourceFile (Path.get_basename (tokens[0]), tokens[0]));
				}
			}
			root_category.files.sort ();
			categories.add (root_category);

			var config = new KeyFile ();
			config.load_from_file (config_filename, KeyFileFlags.NONE);

			output_namespace = config.get_string ("resource-compiler", "namespace");

			foreach (var group in config.get_groups ()) {
				if (group == "resource-compiler")
					continue;

				ResourceCategory category = (group == "root") ? root_category : new ResourceCategory (group);

				foreach (var input in config.get_string_list (group, "inputs")) {
					var regex = new Regex (input);

					var enumerator = input_dir.enumerate_children (FileAttribute.STANDARD_NAME, FileQueryInfoFlags.NONE);

					FileInfo file_info;
					while ((file_info = enumerator.next_file ()) != null) {
						var filename = file_info.get_name ();
						if (regex.match (filename)) {
							var source = Path.build_filename (input_dir.get_path (), filename);
							category.files.add (new ResourceFile (filename, source));
						}
					}
				}

				category.files.sort ();

				if (category != root_category)
					categories.add (category);
			}

			var compress_rules = new Gee.ArrayList<CompressRule> ();
			try {
				var raw_specs = config.get_string_list ("resource-compiler", "compress");
				foreach (unowned string s in raw_specs) {
					string[] tokens = s.strip ().split (":", 3);
					if (tokens.length != 3)
						throw new IOError.INVALID_ARGUMENT ("Compression must be specified as glob:mode:quality");

					PatternSpec pattern = new PatternSpec (tokens[0]);

					Brotli.EncoderMode mode;
					switch (tokens[1]) {
						case "generic":	mode = GENERIC;	break;
						case "text":	mode = TEXT;	break;
						case "font":	mode = FONT;	break;
						default:
							throw new IOError.INVALID_ARGUMENT ("Unsupported encoder mode");
					}

					int quality;
					bool is_integer = int.try_parse (tokens[2], out quality);
					if (!is_integer || quality < Brotli.MIN_QUALITY || quality > Brotli.MAX_QUALITY) {
						throw new IOError.INVALID_ARGUMENT ("Compression quality must be between %d and %d",
							Brotli.MIN_QUALITY, Brotli.MAX_QUALITY);
					}

					compress_rules.add (new CompressRule ((owned) pattern, mode, quality));
				}
			} catch (KeyFileError e) {
			}

			var prepared_resources = new Gee.HashMap<ResourceFile, Gee.Future<PreparedResource>> ();
			var compression_pool = new ThreadPool<CompressRequest>.with_owned_data (compress_file,
				(int) get_num_processors (), false);
			foreach (ResourceCategory category in categories) {
				foreach (ResourceFile input in category.files) {
					unowned string name = input.name;

					CompressRule? rule = null;
					uint name_length = name.length;
					string name_reversed = name.reverse ();
					foreach (CompressRule r in compress_rules) {
						if (r.pattern.match (name_length, name, name_reversed)) {
							rule = r;
							break;
						}
					}

					var input_file = File.new_for_commandline_arg (input.source);
					var input_stream = input_file.read ();
					FileInfo input_info = input_stream.query_info (FileAttribute.STANDARD_SIZE);
					uint64 input_size = input_info.get_attribute_uint64 (FileAttribute.STANDARD_SIZE);

					var promise = new Gee.Promise<PreparedResource> ();
					prepared_resources[input] = promise.future;

					if (rule != null) {
						var output_file = File.new_for_path (output_basename + "-" + name + ".br");
						var output_stream = output_file.replace (null, false, FileCreateFlags.REPLACE_DESTINATION);
						compression_pool.add (new CompressRequest (input_stream, input_size,
							output_file, output_stream, rule, promise));
					} else {
						input_stream.close ();
						promise.set_value (new PreparedResource (input_file, input_size, input_size));
					}
				}
			}

			var vapi_file = File.new_for_commandline_arg (output_basename + ".vapi");
			var cheader_file = File.new_for_commandline_arg (output_basename + ".h");
			var csource_file = File.new_for_commandline_arg (output_basename + ".c");
			var obj_file = File.new_for_commandline_arg (output_basename + "-blob.obj");
			var asm_blob_file = File.new_for_commandline_arg (output_basename + "-blob.S");
			MemoryOutputStream vapi_content = new MemoryOutputStream (null, realloc, free);
			MemoryOutputStream cheader_content = new MemoryOutputStream (null, realloc, free);

			DataOutputStream vapi = new DataOutputStream (vapi_content);
			DataOutputStream cheader = new DataOutputStream (cheader_content);
			DataOutputStream csource = new DataOutputStream (csource_file.replace (null, false, FileCreateFlags.REPLACE_DESTINATION));
			MSVC.ObjWriter obj = null;
			DataOutputStream asource = null;
			if (toolchain == Toolchain.MICROSOFT)
				obj = new MSVC.ObjWriter (machine, obj_file.get_path (), obj_file.replace (null, false, FileCreateFlags.REPLACE_DESTINATION));
			else
				asource = new DataOutputStream (asm_blob_file.replace (null, false, FileCreateFlags.REPLACE_DESTINATION));

			var incguard_name = "__" + output_namespace.up ().replace (".", "_") + "_H__";
			var blob_ctype = output_namespace.replace (".", "") + "Blob";
			var namespace_cprefix = c_namespace_from_vala (output_namespace);

			vapi.put_string (
				"// generated file, do not modify\n" +
				"\n" +
				"[CCode (cheader_filename = \"" + cheader_file.get_basename () + "\")]\n" +
				"namespace " + output_namespace +  " {\n" +
				"\n",
				null);

			cheader.put_string ((
				"/* generated file, do not modify */\n" +
				"\n" +
				"#ifndef %s\n" +
				"#define %s\n" +
				"\n" +
				"#include <glib.h>\n" +
				"\n" +
				"G_BEGIN_DECLS\n" +
				"\n" +
				"typedef struct _%s %s;\n" +
				"\n" +
				"struct _%s\n" +
				"{\n" +
				"  const gchar * name;\n" +
				"  gconstpointer data;\n" +
				"  guint data_length1;\n" +
				"  guint uncompressed_size;\n" +
				"};\n" +
				"\n").printf (incguard_name, incguard_name, blob_ctype, blob_ctype, blob_ctype),
				null);

			csource.put_string (
				"/* generated file, do not modify */\n" +
				"\n" +
				"#include \"" + cheader_file.get_basename () + "\"\n" +
				"\n",
				null);

			var compare_func_identifier = namespace_cprefix + "_blob_compare";

			if (categories.size > 1) {
				csource.put_string (
					"#include <stdlib.h>\n" +
					"#include <string.h>\n" +
					"\n" +
					"static int\n" +
					compare_func_identifier + " (const void * aptr, const void * bptr)\n" +
					"{\n" +
					"  const " + blob_ctype + " * a = aptr;\n" +
					"  const " + blob_ctype + " * b = bptr;\n" +
					"\n" +
					"  return strcmp (a->name, b->name);\n" +
					"}\n" +
					"\n",
					null);
			}

			if (toolchain != Toolchain.MICROSOFT) {
				asource.put_string (
					"#if (defined (__WIN32__) && defined (__i386__)) || defined (__APPLE__)\n" +
					"# define FRIDA_CSYM(x) _ ## x\n" +
					"#else\n" +
					"# define FRIDA_CSYM(x) x\n" +
					"#endif\n");
			}

			if (toolchain == Toolchain.APPLE)
				asource.put_string (".const\n");

			foreach (ResourceCategory category in categories) {
				bool is_root_category = (category.name == "root");
				var category_identifier = identifier_from_filename (category.name);
				var identifier_by_index = new Gee.ArrayList<string> ();
				var blob_identifier_by_index = new Gee.ArrayList<string> ();
				var size_by_index = new Gee.ArrayList<uint64?> ();
				var uncompressed_size_by_index = new Gee.ArrayList<uint64?> ();
				int file_count = category.files.size;

				foreach (ResourceFile input in category.files) {
					PreparedResource prepared_resource;
					Gee.Future<PreparedResource> future = prepared_resources[input];
					try {
						prepared_resource = future.wait ();
					} catch (Gee.FutureError e) {
						throw future.exception;
					}

					string identifier = identifier_from_filename (input.name);
					identifier_by_index.add (identifier);
					size_by_index.add (prepared_resource.size);
					uncompressed_size_by_index.add (prepared_resource.uncompressed_size);

					string blob_identifier = "_" + namespace_cprefix + "_" + identifier;
					blob_identifier_by_index.add (blob_identifier);

					csource.put_string ("extern const char " + blob_identifier + "[];\n");

					if (toolchain == Toolchain.MICROSOFT) {
						obj.write (blob_identifier, prepared_resource.file.read ());
					} else {
						if (toolchain == Toolchain.APPLE) {
							var allow_dead_strip_directive = ".subsections_via_symbols\n";
							asource.put_string (allow_dead_strip_directive);
						}

						var align_for_generic_simd_compatibility = ".align 4\n";
						var align_for_maximum_page_size_on_darwin = ".align 14\n";

						var is_dylib = input.name.has_suffix (".dylib");
						if (is_dylib)
							asource.put_string (align_for_maximum_page_size_on_darwin);
						else
							asource.put_string (align_for_generic_simd_compatibility);

						asource.put_string (".globl FRIDA_CSYM (" + blob_identifier + ")\n");
						asource.put_string ("FRIDA_CSYM (" + blob_identifier + "):\n");
						asource.put_string (".incbin " + quote (prepared_resource.file.get_path ()) + "\n");

						if (!is_dylib)
							asource.put_string (".byte 0\n");
					}
				}

				csource.put_string ("\n");

				var blob_list_identifier = category_identifier + "_blobs";

				csource.put_string ("static const " + blob_ctype + " " + blob_list_identifier + "[" + category.files.size.to_string () + "] =\n{");
				for (int file_index = 0; file_index != file_count; file_index++) {
					string filename = category.files[file_index].name;
					string blob_identifier = blob_identifier_by_index[file_index];
					uint64 size = size_by_index[file_index];
					uint64 uncompressed_size = uncompressed_size_by_index[file_index];
					csource.put_string ("\n  { \"%s\", %s, %s, %s },".printf (
							filename,
							blob_identifier,
							size.to_string (),
							uncompressed_size.to_string ()
						));
				}
				csource.put_string ("\n};\n\n");

				if (is_root_category) {
					for (int file_index = 0; file_index != file_count; file_index++) {
						var identifier = identifier_by_index[file_index];

						var func_name_and_arglist = namespace_cprefix + "_get_" + identifier + "_blob (" + blob_ctype + " * blob)";

						cheader.put_string (
							"void " + func_name_and_arglist + ";\n",
							null);

						csource.put_string (
							"void\n" +
							func_name_and_arglist + "\n" +
							"{\n" +
							"  *blob = " + blob_list_identifier + "[" + file_index.to_string () + "];\n" +
							"}\n" +
							"\n",
							null);

						vapi.put_string ("\tpublic static " + output_namespace + ".Blob get_" + identifier + "_blob ();\n");
					}

					if (categories.size > 1) {
						cheader.put_string ("\n");

						vapi.put_string ("\n");
					}
				} else {
					var func_name_and_arglist = namespace_cprefix + "_find_" + category_identifier + "_by_name (const char * name)";

					cheader.put_string (
						"const " + blob_ctype + " * " + func_name_and_arglist + ";\n",
						null);

					csource.put_string (
						"const " + blob_ctype + " *\n" +
						func_name_and_arglist + "\n" +
						"{\n" +
						"  " + blob_ctype + " needle;\n" +
						"\n" +
						"  needle.name = name;\n" +
						"  needle.data = NULL;\n" +
						"  needle.data_length1 = 0;\n" +
						"  needle.uncompressed_size = 0;\n" +
						"\n" +
						"  return bsearch (&needle, " + blob_list_identifier + ", G_N_ELEMENTS (" + blob_list_identifier + "), sizeof (" + blob_ctype + "), " + compare_func_identifier + ");\n" +
						"}\n",
						null);

					vapi.put_string ("\tpublic static unowned " + output_namespace + ".Blob? find_" + category_identifier + "_by_name (string name);\n");
				}
			}

			vapi.put_string (
				"\n" +
				"	public struct Blob {\n" +
				"		public unowned string name;\n" +
				"		public unowned uint8[] data;\n" +
				"		public uint uncompressed_size;\n" +
				"\n" +
				"		public Blob (string name, uint8[] data, uint uncompressed_size) {\n" +
				"			this.name = name;\n" +
				"			this.data = data;\n" +
				"			this.uncompressed_size = uncompressed_size;\n" +
				"		}\n" +
				"	}\n" +
				"\n" +
				"}\n",
				null);

			cheader.put_string ("\nG_END_DECLS\n\n#endif\n");

			replace_file_if_different (vapi_file, vapi_content);
			replace_file_if_different (cheader_file, cheader_content);
		}

		private void replace_file_if_different (File file, MemoryOutputStream new_content) throws Error {
			bool different = true;

			try {
				var input = file.read ();
				var info = input.query_info (FileAttribute.STANDARD_SIZE);
				var existing_size = (size_t) info.get_attribute_uint64 (FileAttribute.STANDARD_SIZE);

				if (existing_size == new_content.get_data_size ()) {
					uint8[] existing_content_data = new uint8[existing_size];
					var existing_content = new MemoryOutputStream (existing_content_data, null, free);
					size_t bytes_read;
					if (input.read_all (existing_content.get_data (), out bytes_read) && bytes_read == existing_size)
						different = Memory.cmp (existing_content.get_data (), new_content.get_data (), existing_size) != 0;
				}
			} catch (Error e) {
			}

			if (!different)
				return;

			var blob_size = new_content.data_size;
			uint8[] blob = new uint8[blob_size];
			Memory.copy(blob, new_content.data, blob_size);
			var output = file.replace (null, false, FileCreateFlags.REPLACE_DESTINATION);
			output.write_all (blob, null);
		}

		private string c_namespace_from_vala (string vala_ns) {
			var builder = new StringBuilder ();

			bool previous_was_lowercase = false;

			for (int i = 0; i != vala_ns.length; i++) {
				unichar c = vala_ns[i];
				if (c == '.')
					continue;

				if (previous_was_lowercase && c.isupper ())
					builder.append_c ('_');
				else
					previous_was_lowercase = true;

				builder.append_unichar (c.tolower ());
			}

			return builder.str;
		}

		private string identifier_from_filename (string filename) {
			var builder = new StringBuilder ();

			for (int i = 0; i != filename.length; i++) {
				unichar c = filename[i];
				if (c == '-' || c == '.')
					c = '_';
				builder.append_unichar (c.tolower ());
			}

			return builder.str;
		}

		private static string quote (string path) {
			string lit = path
				.replace ("\\", "\\\\")
				.replace ("\"", "\\\"");
			return "\"" + lit + "\"";
		}

		private static void compress_file (owned CompressRequest request) {
			try {
				InputStream input_stream = request.input_stream;
				OutputStream output_stream = request.output_stream;
				var input_buffer = new uint8[512 * 1024];
				var output_buffer = new uint8[512 * 1024];
				uint64 input_size = request.input_size;
				uint64 output_size = 0;

				var encoder = new Brotli.Encoder ();
				encoder.set_parameter (MODE, request.rule.mode);
				encoder.set_parameter (QUALITY, request.rule.quality);
				uint32 window_bits = Brotli.MIN_WINDOW_BITS;
				while (brotli_backward_limit_for (window_bits) < input_size)
					window_bits++;
				if (window_bits > Brotli.MAX_WINDOW_BITS)
					encoder.set_parameter (LARGE_WINDOW, 1);
				encoder.set_parameter (LGWIN, window_bits);
				encoder.set_parameter (SIZE_HINT, uint32.min ((uint32) input_size, 1 << 30));

				bool is_eof = false;
				size_t available_in = 0;
				size_t available_out = output_buffer.length;
				uint8 * next_in = null;
				uint8 * next_out = output_buffer;
				while (true) {
					if (available_in == 0 && !is_eof) {
						ssize_t n = request.input_stream.read (input_buffer);
						is_eof = n == 0;
						available_in = n;
						next_in = input_buffer;
					}

					var op = is_eof ? Brotli.EncoderOperation.FINISH : Brotli.EncoderOperation.PROCESS;
					if (!encoder.compress_stream (op, &available_in, &next_in, &available_out, &next_out)) {
						throw new IOError.FAILED ("Unable to compress");
					}

					size_t bytes_written;

					if (available_out == 0) {
						output_stream.write_all (output_buffer, out bytes_written);
						output_size += bytes_written;
						available_out = output_buffer.length;
						next_out = output_buffer;
					}

					if (encoder.is_finished ()) {
						output_stream.write_all (output_buffer[:next_out - (uint8 *) output_buffer],
							out bytes_written);
						output_size += bytes_written;
						break;
					}
				}

				output_stream.close ();
				input_stream.close ();

				request.promise.set_value (new PreparedResource (request.output_file, output_size, request.input_size));
			} catch (Error e) {
				request.promise.set_exception ((owned) e);
			}
		}

		private const size_t BROTLI_WINDOW_GAP = 16;

		private static size_t brotli_backward_limit_for (uint32 window_bits) {
			return ((size_t) 1 << window_bits) - BROTLI_WINDOW_GAP;
		}

		private class ResourceCategory {
			public string name;
			public Gee.ArrayList<ResourceFile> files;

			public ResourceCategory (string name) {
				this.name = name;
				this.files = new Gee.ArrayList<ResourceFile> ();
			}
		}

		private class ResourceFile {
			public string name;
			public string source;

			public ResourceFile (string name, string source) {
				this.name = name;
				this.source = source;
			}
		}

		private class PreparedResource {
			public File file;
			public uint64 size;
			public uint64 uncompressed_size;

			public PreparedResource (File file, uint64 size, uint64 uncompressed_size) {
				this.file = file;
				this.size = size;
				this.uncompressed_size = uncompressed_size;
			}
		}

		private class CompressRule {
			public PatternSpec pattern;
			public Brotli.EncoderMode mode;
			public int quality;

			public CompressRule (owned PatternSpec pattern, Brotli.EncoderMode mode, int quality) {
				this.pattern = (owned) pattern;
				this.quality = quality;
			}
		}

		private class CompressRequest {
			public InputStream input_stream;
			public uint64 input_size;
			public File output_file;
			public OutputStream output_stream;
			public CompressRule rule;
			public Gee.Promise<PreparedResource> promise;

			public CompressRequest (InputStream input_stream, uint64 input_size, File output_file, OutputStream output_stream,
					CompressRule rule, Gee.Promise<PreparedResource> promise) {
				this.input_stream = input_stream;
				this.input_size = input_size;
				this.output_file = output_file;
				this.output_stream = output_stream;
				this.rule = rule;
				this.promise = promise;
			}
		}
	}

	namespace MSVC {
		public sealed class ObjWriter {
			private const uint16 IMAGE_FILE_MACHINE_I386 = 0x14c;
			private const uint16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;
			private const uint16 IMAGE_FILE_MACHINE_ARM = 0x1c0;
			private const uint16 IMAGE_FILE_MACHINE_ARMNT = 0x1c4;
			private const uint16 IMAGE_FILE_MACHINE_ARM64 = 0xaa64;
			private const uint16 IMAGE_FILE_MACHINE_MIPS16 = 0x266;

			private const uint32 IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040;
			private const uint32 IMAGE_SCN_LNK_INFO = 0x00000200;
			private const uint32 IMAGE_SCN_LNK_REMOVE = 0x00000800;
			private const uint32 IMAGE_SCN_ALIGN_1BYTES = 0x00100000;
			private const uint32 IMAGE_SCN_ALIGN_2BYTES = 0x00200000;
			private const uint32 IMAGE_SCN_MEM_DISCARDABLE = 0x02000000;
			private const uint32 IMAGE_SCN_MEM_READ = 0x40000000;

			private const size_t SECTION_DATA_ALIGNMENT = 4;

			private const uint32 FILE_HEADER_SIZE = 20;
			private const uint32 SECTION_HEADER_SIZE = 40;

			private Machine machine;
			private DataOutputStream stream;
			private bool closed = false;
			private Gee.HashMap<string, int64?> section_header_offset_by_name = new Gee.HashMap<string, int64?> ();
			private string directive_data;
			private size_t rdata_size = 0;
			private uint32 rdata_crc = 0;
			private Gee.ArrayList<Blob> blobs = new Gee.ArrayList<Blob> ();
			private Gee.ArrayList<string> strings = new Gee.ArrayList<string> ();
			private size_t strings_size = 4;

			public ObjWriter (Machine machine, string path, OutputStream output) throws Error {
				this.machine = machine;

				stream = new DataOutputStream (output);
				stream.set_byte_order (DataStreamByteOrder.LITTLE_ENDIAN);

				/* directive_data = "   /DEFAULTLIB:\"MSVCRT\" /DEFAULTLIB:\"OLDNAMES\" "; */
				directive_data = "";

				write_headers (path);
			}

			~ObjWriter () {
				try {
					close ();
				} catch (Error e) {
				}
			}

			public void close () throws Error {
				if (!closed) {
					fill_placeholder_header_values ();
					write_footers ();
					closed = true;
				}
			}

			public void write (string name, InputStream data) throws Error {
				if (rdata_size % SECTION_DATA_ALIGNMENT != 0) {
					var padding = new uint8[SECTION_DATA_ALIGNMENT - (rdata_size % SECTION_DATA_ALIGNMENT)];
					stream.write_all (padding, null);
					rdata_size += padding.length;
					rdata_crc = Checksum.crc32 (padding, rdata_crc);
				}

				blobs.add (new Blob (name, rdata_size));

				var buf = new uint8[128 * 1024];
				while (true) {
					size_t bytes_read;
					data.read_all (buf, out bytes_read);
					if (bytes_read == 0)
						break;
					buf.resize ((int) bytes_read);

					stream.write_all (buf, null);
					rdata_size += bytes_read;
					rdata_crc = Checksum.crc32 (buf, rdata_crc);
				}
			}

			private void write_headers (string obj_path) throws Error {
				var file_header = FileHeader ();
				switch (machine) {
					case Machine.X86:
						file_header.machine = IMAGE_FILE_MACHINE_I386;
						break;
					case Machine.X86_64:
						file_header.machine = IMAGE_FILE_MACHINE_AMD64;
						break;
					case Machine.ARM:
						file_header.machine = IMAGE_FILE_MACHINE_ARMNT;
						break;
					case Machine.ARM64:
						file_header.machine = IMAGE_FILE_MACHINE_ARM64;
						break;
					case Machine.MIPS:
						file_header.machine = IMAGE_FILE_MACHINE_MIPS16;
						break;
					default:
						assert_not_reached ();
				}
				file_header.number_of_sections = 3;
				file_header.time_date_stamp = (uint32) (get_real_time () / 1000000);
				file_header.pointer_to_symbol_table = 0; /* filled out at the end */
				file_header.number_of_symbols = 0; /* filled out at the end */
				file_header.size_of_optional_header = 0;
				file_header.characteristics = 0;
				write_file_header (file_header);

				var directive = SectionHeader ();
				directive.name = ".drectve";
				directive.virtual_size = 0;
				directive.virtual_address = 0;
				directive.size_of_raw_data = directive_data.length;
				directive.pointer_to_raw_data = FILE_HEADER_SIZE + (file_header.number_of_sections * SECTION_HEADER_SIZE);
				directive.pointer_to_relocations = 0;
				directive.pointer_to_line_numbers = 0;
				directive.number_of_relocations = 0;
				directive.number_of_line_numbers = 0;
				directive.characteristics = IMAGE_SCN_ALIGN_1BYTES | IMAGE_SCN_LNK_REMOVE | IMAGE_SCN_LNK_INFO;
				write_section_header (directive);

				var compiler_name = "Microsoft (R) Optimizing Compiler";
				var debug = SectionHeader ();
				debug.name = ".debug$S";
				debug.virtual_size = 0;
				debug.virtual_address = 0;
				debug.size_of_raw_data = 48 + obj_path.length + compiler_name.length;
				debug.pointer_to_raw_data = directive.pointer_to_raw_data + directive.size_of_raw_data;
				debug.pointer_to_relocations = 0;
				debug.pointer_to_line_numbers = 0;
				debug.number_of_relocations = 0;
				debug.number_of_line_numbers = 0;
				debug.characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_DISCARDABLE | IMAGE_SCN_ALIGN_1BYTES | IMAGE_SCN_CNT_INITIALIZED_DATA;
				write_section_header (debug);

				var rdata = SectionHeader ();
				var rdata_align_shift = (obj_path.length + compiler_name.length) % 2 == 0 ? 0 : 1;
				rdata.name = ".rdata";
				rdata.virtual_size = 0;
				rdata.virtual_address = 0;
				rdata.size_of_raw_data = 0; /* filled out at the end */
				rdata.pointer_to_raw_data = debug.pointer_to_raw_data + debug.size_of_raw_data + rdata_align_shift;
				rdata.pointer_to_relocations = 0;
				rdata.pointer_to_line_numbers = 0;
				rdata.number_of_relocations = 0;
				rdata.number_of_line_numbers = 0;
				rdata.characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_ALIGN_2BYTES | IMAGE_SCN_ALIGN_1BYTES | IMAGE_SCN_CNT_INITIALIZED_DATA;
				write_section_header (rdata);

				/* Section 1: Directive */
				stream.put_string (directive_data);

				/* Section 2: Visual C++ debug information (symbolic information) */
				stream.put_uint32 (4);
				stream.put_uint32 (0xf1);
				stream.put_uint32 (69 + obj_path.length);
				stream.put_uint16 (7 + obj_path.length);
				stream.put_uint16 (0x1101);
				stream.put_uint32 (0);
				stream.put_string (obj_path);
				stream.put_byte (0);
				stream.put_uint32 (0x113c003a);
				stream.put_uint32 (0x00002200);
				stream.put_uint32 (0x00120007);
				stream.put_uint32 (0x520d0000);
				stream.put_uint32 (0x00120001);
				stream.put_uint32 (0x520d0000);
				stream.put_uint16 (1);
				stream.put_string (compiler_name);
				stream.put_byte (0);

				/* Section 3: Read-only initialized data: filled out by one or more write() calls */
				if (rdata_align_shift > 0) {
					var padding = new uint8[rdata_align_shift];
					stream.write_all (padding, null);
				}
			}

			private void fill_placeholder_header_values () throws Error {
				var output_stream = (Seekable) stream.get_base_stream ();
				var current_position = output_stream.tell ();

				uint32 pointer_to_symbol_table = (uint32) current_position;
				uint32 number_of_symbols = 8 + blobs.size;
				output_stream.seek (8, SeekType.SET);
				stream.put_uint32 (pointer_to_symbol_table);
				stream.put_uint32 (number_of_symbols);

				output_stream.seek (section_header_offset_by_name[".rdata"] + 16, SeekType.SET);
				stream.put_uint32 ((uint32) rdata_size);

				output_stream.seek (current_position, SeekType.SET);
			}

			private void write_footers () throws Error {
				/* Symbol table */
				var comp_id = Symbol ();
				comp_id.name = "@comp.id";
				comp_id.value = 0x00e0520d;
				comp_id.section_number = -1;
				comp_id.type = 0;
				comp_id.storage_class = 3;
				comp_id.number_of_aux_symbols = 0;
				write_symbol (comp_id);

				var feat = Symbol ();
				feat.name = "@feat.00";
				feat.value = (uint32) 0x80000191;
				feat.section_number = -1;
				feat.type = 0;
				feat.storage_class = 3;
				feat.number_of_aux_symbols = 0;
				write_symbol (feat);

				var ds = Symbol ();
				ds.name = ".drectve";
				ds.value = 0;
				ds.section_number = 1;
				ds.type = 0;
				ds.storage_class = 3;
				ds.number_of_aux_symbols = 1;
				write_symbol (ds);

				stream.put_uint32 (directive_data.length);
				stream.put_uint32 (0);
				stream.put_uint32 (0);
				stream.put_uint32 (0);
				stream.put_uint16 (0);

				var dbg = Symbol ();
				dbg.name = ".debug$S";
				dbg.value = 0;
				dbg.section_number = 2;
				dbg.type = 0;
				dbg.storage_class = 3;
				dbg.number_of_aux_symbols = 1;
				write_symbol (dbg);

				stream.put_uint32 (0xa8);
				stream.put_uint32 (0);
				stream.put_uint32 (0);
				stream.put_uint32 (0);
				stream.put_uint16 (0);

				var rs = Symbol ();
				rs.name = ".rdata";
				rs.value = 0;
				rs.section_number = 3;
				rs.type = 0;
				rs.storage_class = 3;
				rs.number_of_aux_symbols = 1;
				write_symbol (rs);

				stream.put_uint32 ((uint32) rdata_size);
				stream.put_uint32 (0);
				stream.put_uint32 (rdata_crc);
				stream.put_uint32 (0);
				stream.put_uint16 (0);

				var symbol_prefix = (machine == Machine.X86) ? "_" : "";
				foreach (var blob in blobs) {
					var hello = Symbol ();
					hello.name = symbol_prefix + blob.name;
					hello.value = (uint32) blob.offset;
					hello.section_number = 3;
					hello.type = 0;
					hello.storage_class = 2;
					hello.number_of_aux_symbols = 0;
					write_symbol (hello);
				}

				/* String table */
				stream.put_uint32 ((uint32) strings_size);
				foreach (var s in strings) {
					stream.put_string (s);
					stream.put_byte (0);
				}
			}

			private void write_file_header (FileHeader h) throws Error {
				stream.put_uint16 (h.machine);
				stream.put_uint16 (h.number_of_sections);
				stream.put_uint32 (h.time_date_stamp);
				stream.put_uint32 (h.pointer_to_symbol_table);
				stream.put_uint32 (h.number_of_symbols);
				stream.put_uint16 (h.size_of_optional_header);
				stream.put_uint16 (h.characteristics);
			}

			private void write_section_header (SectionHeader h) throws Error {
				section_header_offset_by_name[h.name] = ((Seekable) stream.get_base_stream ()).tell ();

				assert (h.name.length <= 8);
				stream.put_string (h.name);
				for (int i = 8 - h.name.length; i > 0; i--)
					stream.put_byte (0);
				stream.put_uint32 (h.virtual_size);
				stream.put_uint32 (h.virtual_address);
				stream.put_uint32 (h.size_of_raw_data);
				stream.put_uint32 (h.pointer_to_raw_data);
				stream.put_uint32 (h.pointer_to_relocations);
				stream.put_uint32 (h.pointer_to_line_numbers);
				stream.put_uint16 (h.number_of_relocations);
				stream.put_uint16 (h.number_of_line_numbers);
				stream.put_uint32 (h.characteristics);
			}

			private void write_symbol (Symbol s) throws Error {
				if (s.name.length <= 8) {
					stream.put_string (s.name);
					for (int i = 8 - s.name.length; i > 0; i--)
						stream.put_byte (0);
				} else {
					stream.put_uint32 (0);
					stream.put_uint32 (allocate_string (s.name));
				}
				stream.put_uint32 (s.value);
				stream.put_int16 (s.section_number);
				stream.put_uint16 (s.type);
				stream.put_byte (s.storage_class);
				stream.put_byte (s.number_of_aux_symbols);
			}

			private uint32 allocate_string (string s) {
				var offset = strings_size;
				strings.add (s);
				strings_size += s.length + 1;
				return (uint32) offset;
			}

			private struct FileHeader {
				public uint16 machine;
				public uint16 number_of_sections;
				public uint32 time_date_stamp;
				public uint32 pointer_to_symbol_table;
				public uint32 number_of_symbols;
				public uint16 size_of_optional_header;
				public uint16 characteristics;
			}

			private struct SectionHeader {
				public string name;
				public uint32 virtual_size;
				public uint32 virtual_address;
				public uint32 size_of_raw_data;
				public uint32 pointer_to_raw_data;
				public uint32 pointer_to_relocations;
				public uint32 pointer_to_line_numbers;
				public uint16 number_of_relocations;
				public uint16 number_of_line_numbers;
				public uint32 characteristics;
			}

			private struct Symbol {
				public string name;
				public uint32 value;
				public int16 section_number;
				public uint16 type;
				public uint8 storage_class;
				public uint8 number_of_aux_symbols;
			}

			private class Blob {
				public string name {
					get;
					private set;
				}

				public size_t offset {
					get;
					private set;
				}

				public Blob (string name, size_t offset) {
					this.name = name;
					this.offset = offset;
				}
			}
		}

		namespace Checksum {
			private static uint32[] _crc32_table = null;

			public static uint32 crc32 (uint8[] data, uint32 crc) {
				var table = get_crc32_table ();

				foreach (var b in data)
					crc = (crc >> 8) ^ table[b ^ (uint8) crc];

				return crc;
			}

			private uint32[] get_crc32_table () {
				if (_crc32_table == null) {
					_crc32_table = new uint32[256];
					for (var i = 0; i != _crc32_table.length; i++) {
						uint32 crc = i;
						for (var j = 0; j != 8; j++) {
							if ((crc & 1) != 0) {
								crc = (uint32) ((crc >> 1) ^ 0xedb88320);
							} else {
								crc = (crc >> 1);
							}
						}
						_crc32_table[i] = crc;
					}
				}

				return _crc32_table;
			}
		}
	}
}
