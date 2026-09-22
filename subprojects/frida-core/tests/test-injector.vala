namespace Frida.InjectorTest {
	public static void add_tests () {
		GLib.Test.add_func ("/Injector/inject-dynamic-current-arch", () => {
			test_dynamic_injection (Frida.Test.Arch.CURRENT);
		});

		if (can_test_cross_arch_injection) {
			GLib.Test.add_func ("/Injector/inject-dynamic-other-arch", () => {
				test_dynamic_injection (Frida.Test.Arch.OTHER);
			});
		}

		GLib.Test.add_func ("/Injector/inject-resident-current-arch", () => {
			test_resident_injection (Frida.Test.Arch.CURRENT);
		});

		if (can_test_cross_arch_injection) {
			GLib.Test.add_func ("/Injector/inject-resident-other-arch", () => {
				test_resident_injection (Frida.Test.Arch.OTHER);
			});
		}

		GLib.Test.add_func ("/Injector/resource-leaks", test_resource_leaks);

#if DARWIN
		GLib.Test.add_func ("/Injector/suspended-injection-current-arch", () => {
			test_suspended_injection (Frida.Test.Arch.CURRENT);
		});

		if (can_test_cross_arch_injection) {
			GLib.Test.add_func ("/Injector/suspended-injection-other-arch", () => {
				test_suspended_injection (Frida.Test.Arch.OTHER);
			});
		}
#endif
	}

	private static void test_dynamic_injection (Frida.Test.Arch arch) {
		var logfile = File.new_for_path (Frida.Test.path_to_temporary_file ("dynamic-injection.log"));
		try {
			logfile.delete ();
		} catch (GLib.Error delete_error) {
		}
		var envp = new string[] {
			"FRIDA_LABRAT_LOGFILE=" + logfile.get_path ()
		};

		var rat = new Labrat ("sleeper", envp, arch);

		rat.inject ("simple-agent", "", arch);
		rat.wait_for_uninject ();
		assert_true (content_of (logfile) == ">m<");

		var requested_exit_code = 43;
		rat.inject ("simple-agent", requested_exit_code.to_string (), arch);
		rat.wait_for_uninject ();

		switch (Frida.Test.os ()) {
			case Frida.Test.OS.MACOS:   // Gum.Darwin.Mapper
			case Frida.Test.OS.IOS:     // Gum.Darwin.Mapper
			case Frida.Test.OS.TVOS:    // Gum.Darwin.Mapper
			case Frida.Test.OS.ANDROID: // Bionic's behavior
				assert_true (content_of (logfile) == ">m<>m");
				break;
			case Frida.Test.OS.LINUX:
				if (Frida.Test.libc () == Frida.Test.Libc.UCLIBC) {
					assert_true (content_of (logfile) == ">m<>m");
				} else {
					assert_true (content_of (logfile) == ">m<>m<");
				}
				break;
			default:
				assert_true (content_of (logfile) == ">m<>m<");
				break;
		}

		var exit_code = rat.wait_for_process_to_exit ();
		assert_true (exit_code == requested_exit_code);

		try {
			logfile.delete ();
		} catch (GLib.Error delete_error) {
			assert_not_reached ();
		}

		rat.close ();
	}

	private static void test_resident_injection (Frida.Test.Arch arch) {
		var logfile = File.new_for_path (Frida.Test.path_to_temporary_file ("resident-injection.log"));
		try {
			logfile.delete ();
		} catch (GLib.Error delete_error) {
		}
		var envp = new string[] {
			"FRIDA_LABRAT_LOGFILE=" + logfile.get_path ()
		};

		var rat = new Labrat ("sleeper", envp, arch);

		rat.inject ("resident-agent", "", arch);
		rat.wait_for_uninject ();
		assert_true (content_of (logfile) == ">m");

		try {
			rat.process.kill ();

			logfile.delete ();
		} catch (GLib.Error e) {
			assert_not_reached ();
		}

		rat.close ();
	}

	private static void test_resource_leaks () {
		var logfile = File.new_for_path (Frida.Test.path_to_temporary_file ("leaks.log"));
		var envp = new string[] {
			"FRIDA_LABRAT_LOGFILE=" + logfile.get_path ()
		};

		var rat = new Labrat ("sleeper", envp);

		/* Warm up static allocations */
		for (int i = 0; i != 2; i++) {
			rat.inject ("simple-agent", "");
			rat.wait_for_uninject ();
			rat.wait_for_cleanup ();
		}

		var usage_before = rat.process.snapshot_resource_usage ();

		rat.inject ("simple-agent", "");
		rat.wait_for_uninject ();
		rat.wait_for_cleanup ();

		var usage_after = rat.process.snapshot_resource_usage ();

		usage_after.assert_equals (usage_before);

		rat.inject ("simple-agent", "0");
		rat.wait_for_uninject ();
		rat.wait_for_process_to_exit ();

		rat.close ();
	}

#if DARWIN
	private static void test_suspended_injection (Frida.Test.Arch arch) {
		var logfile = File.new_for_path (Frida.Test.path_to_temporary_file ("suspended-injection.log"));
		try {
			logfile.delete ();
		} catch (GLib.Error delete_error) {
		}
		var envp = new string[] {
			"FRIDA_LABRAT_LOGFILE=" + logfile.get_path ()
		};

		var rat = new Labrat.suspended ("sleeper", envp, arch);

		rat.inject ("simple-agent", "", arch);
		rat.wait_for_uninject ();
		assert_true (content_of (logfile) == ">m<");

		rat.close ();
	}
#endif

	private static string content_of (File file) {
		try {
			uint8[] contents;
			file.load_contents (null, out contents, null);
			unowned string str = (string) contents;
			return str;
		} catch (GLib.Error load_error) {
			stderr.printf ("%s: %s\n", file.get_path (), load_error.message);
			assert_not_reached ();
		}
	}

	private sealed class Labrat {
		public Frida.Test.Process? process {
			get;
			private set;
		}

		private Injector? injector;
		private Gee.Queue<uint> uninjections = new Gee.ArrayQueue<uint> ();
		private PendingUninject? pending_uninject;

		public Labrat (string name, string[] envp, Frida.Test.Arch arch = Frida.Test.Arch.CURRENT) {
			try {
				process = Frida.Test.Process.start (Frida.Test.Labrats.path_to_executable (name), null, envp, arch);
			} catch (Error e) {
				printerr ("\nFAIL: %s\n\n", e.message);
				assert_not_reached ();
			}

#if !WINDOWS
			/* TODO: improve injectors to handle injection into a process that hasn't yet finished initializing */
			Thread.usleep (50000);
#endif
		}

		public Labrat.suspended (string name, string[] envp, Frida.Test.Arch arch = Frida.Test.Arch.CURRENT) {
			try {
				process = Frida.Test.Process.create (Frida.Test.Labrats.path_to_executable (name), null, envp, arch);
			} catch (Error e) {
				printerr ("\nFAIL: %s\n\n", e.message);
				assert_not_reached ();
			}
		}

		public void close () {
			var loop = new MainLoop ();
			Idle.add (() => {
				do_close.begin (loop);
				return false;
			});
			loop.run ();
		}

		private async void do_close (MainLoop loop) {
			if (injector != null) {
				try {
					yield injector.close ();
				} catch (IOError e) {
					assert_not_reached ();
				}
				injector.uninjected.disconnect (on_uninjected);
				injector = null;
			}
			process = null;

			/* Queue an idle handler, allowing MainContext to perform any outstanding completions, in turn cleaning up resources */
			Idle.add (() => {
				loop.quit ();
				return false;
			});
		}

		public void inject (string name, string data, Frida.Test.Arch arch = Frida.Test.Arch.CURRENT) {
			var loop = new MainLoop ();
			Idle.add (() => {
				perform_injection.begin (name, data, arch, loop);
				return false;
			});
			loop.run ();
		}

		private async void perform_injection (string name, string data, Frida.Test.Arch arch, MainLoop loop) {
			if (injector == null) {
				injector = Injector.new ();
				injector.uninjected.connect (on_uninjected);
			}

			try {
				var path = Frida.Test.Labrats.path_to_library (name, arch);
				assert_true (FileUtils.test (path, FileTest.EXISTS));

				yield injector.inject_library_file (process.id, path, "frida_agent_main", data);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n\n", e.message);
				assert_not_reached ();
			}

			loop.quit ();
		}

		public void wait_for_uninject () {
			var success = try_wait_for_uninject (5000);
			assert_true (success);
		}

		public bool try_wait_for_uninject (uint timeout) {
			if (!uninjections.is_empty) {
				uninjections.poll ();
				return true;
			}

			var loop = new MainLoop ();

			assert (pending_uninject == null);
			pending_uninject = new PendingUninject (loop);

			bool timed_out = false;
			var timeout_id = Timeout.add (timeout, () => {
				timed_out = true;
				loop.quit ();
				return false;
			});

			loop.run ();

			if (!timed_out) {
				uninjections.poll ();

				Source.remove (timeout_id);
			}

			pending_uninject = null;

			return !timed_out;
		}

		public void wait_for_cleanup () {
			var loop = new MainLoop ();

			/* The Darwin injector does cleanup 50ms after detecting that the remote thread is dead */
			Timeout.add (100, () => {
				loop.quit ();
				return false;
			});

			loop.run ();
		}

		public int wait_for_process_to_exit () {
			int exitcode = -1;

			try {
				exitcode = process.join (1000);
			} catch (Error e) {
				stdout.printf ("\n\nunexpected error: %s\n", e.message);
				assert_not_reached ();
			}

			return exitcode;
		}

		private void on_uninjected (uint id) {
			uninjections.offer (id);

			if (pending_uninject != null)
				pending_uninject.complete ();
		}

		private class PendingUninject {
			private MainLoop loop;

			public PendingUninject (MainLoop loop) {
				this.loop = loop;
			}

			public void complete () {
				loop.quit ();
			}
		}
	}
}
