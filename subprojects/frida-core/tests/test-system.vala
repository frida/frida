namespace Frida.SystemTest {
	public static void add_tests () {
		GLib.Test.add_func ("/System/enumerate-processes-returns-processes-with-icons", () => {
			var options = new ProcessQueryOptions ();
			options.scope = FULL;

			var timer = new Timer ();
			var processes = System.enumerate_processes (options);
			var time_spent_on_first_run = timer.elapsed ();

			assert_true (processes.length > 0);

			switch (Frida.Test.os ()) {
				case Frida.Test.OS.WINDOWS:
					int num_icons_seen = 0;
					foreach (var p in processes) {
						if (p.parameters.contains ("icons"))
							num_icons_seen++;
					}
					assert_true (num_icons_seen > 0);
					break;
				default:
					break;
			}

			timer.start ();
			processes = System.enumerate_processes (options);
			var time_spent_on_second_run = timer.elapsed ();

			if (GLib.Test.verbose ())
				stdout.printf (" [spent %f and %f] ", time_spent_on_first_run, time_spent_on_second_run);
		});
	}
}
