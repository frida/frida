namespace Frida {
	public bool can_test_cross_arch_injection =
#if CROSS_ARCH
		true
#else
		false
#endif
		;
}

namespace Frida.Test {
	public static void run (string[] args) {
		Environment.init (ref args);

		if (can_test_cross_arch_injection) {
			try {
				switch (os ()) {
					case MACOS:
						switch (cpu ()) {
							case ARM_64:
								if (Gum.query_ptrauth_support () == UNSUPPORTED) {
									string output;
									GLib.Process.spawn_command_line_sync ("nvram boot-args", out output);

									string[] tokens = output.strip ().split ("\t");
									if (tokens.length == 2) {
										unowned string boot_args = tokens[1];
										can_test_cross_arch_injection = "-arm64e_preview_abi" in boot_args;
									} else {
										can_test_cross_arch_injection = false;
									}
								}
								break;
							case X86_64:
								string raw_version;
								GLib.Process.spawn_command_line_sync ("sw_vers -productVersion", out raw_version);

								string[] tokens = raw_version.strip ().split (".");
								assert (tokens.length >= 2);

								uint major = uint.parse (tokens[0]);
								uint minor = uint.parse (tokens[1]);

								bool newer_than_mojave = major > 10 || (major == 10 && minor > 4);
								can_test_cross_arch_injection = !newer_than_mojave;
								break;
							default:
								break;
						}
						break;
					case IOS:
					case TVOS:
						if (cpu () == ARM_64) {
							string output;
							GLib.Process.spawn_command_line_sync ("sysctl -nq hw.cpusubtype", out output);

							var cpu_subtype = uint.parse (output.strip ());

							uint subtype_arm64e = 2;
							can_test_cross_arch_injection = cpu_subtype == subtype_arm64e;
						}
						break;
					default:
						break;
				}
			} catch (GLib.Error e) {
				assert_not_reached ();
			}
		}

		Frida.SystemTest.add_tests ();

		Frida.GDBTest.add_tests ();

#if HAVE_LOCAL_BACKEND
		Frida.InjectorTest.add_tests ();

		Frida.AgentTest.add_tests ();
#endif
#if HAVE_GADGET && !WINDOWS
		Frida.GadgetTest.add_tests ();
#endif
		Frida.HostSessionTest.add_tests ();

#if HAVE_BAREBONE_BACKEND
		Frida.BareboneTest.add_tests ();
#endif

#if HAVE_COMPILER_BACKEND && !QNX
		Frida.CompilerTest.add_tests ();
#endif

		GLib.Test.run ();

		Environment.deinit ();
	}

	namespace Environment {
		public extern void init ([CCode (array_length_pos = 0.9)] ref unowned string[] args);
		public extern void deinit ();
	}

	public static string path_to_temporary_file (string name) {
		var prefix = "frida-tests-%u-".printf (Gum.Process.get_id ());
#if QNX
		return Path.build_filename (GLib.Environment.get_tmp_dir (), prefix + name);
#else
		var tests_dir = Path.get_dirname (Process.current.filename);
		return Path.build_filename (tests_dir, prefix + name);
#endif
	}

	public extern OS os ();

	public extern CPU cpu ();

	public extern Libc libc ();

	public string os_arch_suffix (Arch arch = Arch.CURRENT) {
		switch (os ()) {
			case OS.MACOS:
				return "-macos";
			case OS.IOS:
				return "-ios";
			case OS.TVOS:
				return "-tvos";
			default:
				break;
		}

		string os_name;
		switch (os ()) {
			case OS.WINDOWS:
				os_name = "windows";
				break;
			case OS.LINUX:
				os_name = "linux";
				break;
			case OS.ANDROID:
				os_name = "android";
				break;
			case OS.FREEBSD:
				os_name = "freebsd";
				break;
			case OS.QNX:
				os_name = "qnx";
				break;
			default:
				assert_not_reached ();
		}

		string abi_name;
		switch (Frida.Test.cpu ()) {
			case CPU.X86_32:
				abi_name = "x86";
				break;
			case CPU.X86_64:
				abi_name = "x86_64";
				break;
			case CPU.ARM_32:
				if (GLib.ByteOrder.HOST == GLib.ByteOrder.BIG_ENDIAN) {
					abi_name = "armbe8";
				} else {
#if ARMHF
					abi_name = "armhf";
#else
					abi_name = "arm";
#endif
				}
				break;
			case CPU.ARM_64:
				abi_name = (ByteOrder.HOST == ByteOrder.BIG_ENDIAN) ? "arm64be" : "arm64";
				break;
			case CPU.MIPS:
				abi_name = "mips";
				break;
			case CPU.MIPSEL:
				abi_name = "mipsel";
				break;
			default:
				assert_not_reached ();
		}

		return "-" + os_name + "-" + abi_name;
	}

	public string os_executable_suffix () {
		switch (os ()) {
			case OS.WINDOWS:
				return ".exe";
			case OS.MACOS:
			case OS.LINUX:
			case OS.IOS:
			case OS.TVOS:
			case OS.ANDROID:
			case OS.FREEBSD:
			case OS.QNX:
				return "";
			default:
				assert_not_reached ();
		}
	}

	public string os_library_suffix () {
		switch (os ()) {
			case OS.WINDOWS:
				return ".dll";
			case OS.MACOS:
			case OS.IOS:
			case OS.TVOS:
				return ".dylib";
			case OS.LINUX:
			case OS.ANDROID:
			case OS.FREEBSD:
			case OS.QNX:
				return ".so";
			default:
				assert_not_reached ();
		}
	}

	public enum OS {
		WINDOWS,
		MACOS,
		LINUX,
		IOS,
		TVOS,
		ANDROID,
		FREEBSD,
		QNX
	}

	public enum CPU {
		X86_32,
		X86_64,
		ARM_32,
		ARM_64,
		MIPS,
		MIPSEL,
		RISCV32,
		RISCV64
	}

	public enum Arch {
		CURRENT,
		OTHER
	}

	public enum Libc {
		MSVCRT,
		APPLE,
		GLIBC,
		MUSL,
		UCLIBC,
		BIONIC,
		FREEBSD,
		QNX
	}
}
