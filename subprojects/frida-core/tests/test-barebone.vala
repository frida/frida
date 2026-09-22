namespace Frida.BareboneTest {
	public void add_tests () {
		GLib.Test.add_func ("/Barebone/IA32/enumerate-ranges-walks-legacy-tables", () => {
			var h = new Harness ((h) => enumerate_ranges_walks_legacy_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/enumerate-ranges-walks-pae-tables", () => {
			var h = new Harness ((h) => enumerate_ranges_walks_pae_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/enumerate-ranges-honors-protection-filter", () => {
			var h = new Harness ((h) => enumerate_ranges_honors_protection_filter.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/control-registers-read-from-target-description", () => {
			var h = new Harness ((h) => control_registers_read_from_target_description.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/translate-address-resolves-leaf-mappings", () => {
			var h = new Harness ((h) => translate_address_resolves_leaf_mappings.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/protect-pages-updates-legacy-entries", () => {
			var h = new Harness ((h) => protect_pages_updates_legacy_entries.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/protect-pages-widens-pae-parents", () => {
			var h = new Harness ((h) => protect_pages_widens_pae_parents.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/protect-pages-rejects-large-pages", () => {
			var h = new Harness ((h) => protect_pages_rejects_large_pages.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/relocations-apply-load-bias", () => {
			var h = new Harness ((h) => relocations_apply_load_bias.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/allocate-pages-spans-leaf-tables", () => {
			var h = new Harness ((h) => allocate_pages_spans_leaf_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/protect-pages-spans-leaf-tables", () => {
			var h = new Harness ((h) => protect_pages_spans_leaf_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/enumerate-ranges-walks-short-descriptor-tables", () => {
			var h = new Harness ((h) => enumerate_ranges_walks_short_descriptor_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/enumerate-ranges-honors-protection-filter", () => {
			var h = new Harness ((h) => arm_enumerate_ranges_honors_protection_filter.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/enumerate-ranges-rejects-lpae-tables", () => {
			var h = new Harness ((h) => enumerate_ranges_rejects_lpae_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/allocate-pages-takes-free-entries", () => {
			var h = new Harness ((h) => arm_allocate_pages_takes_free_entries.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/allocate-pages-spans-leaf-tables", () => {
			var h = new Harness ((h) => arm_allocate_pages_spans_leaf_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/protect-pages-updates-page-entries", () => {
			var h = new Harness ((h) => arm_protect_pages_updates_page_entries.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/protect-pages-rejects-sections", () => {
			var h = new Harness ((h) => arm_protect_pages_rejects_sections.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/relocations-apply-load-bias", () => {
			var h = new Harness ((h) => arm_relocations_apply_load_bias.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/services-resolve-from-descriptor-block", () => {
			var h = new Harness ((h) => services_resolve_from_descriptor_block.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/scans-ranges", () => {
			var h = new Harness ((h) => ia32_scans_ranges.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => agent_runs_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => enumerates_processes_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/hooks-again-after-letting-go-in-live-guest", () => {
			var h = new Harness ((h) => hooks_again_after_letting_go_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/disassembles-add-ddb-in-live-guest", () => {
			var h = new Harness ((h) => disassembles_add_ddb_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/watches-its-own-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) => win9x_watches_its_own_threads_in_live_guest.begin (
				h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/moves-a-lot-of-data-in-live-guest", () => {
			var h = new SlowHarness ((h) => moves_a_lot_of_data_in_live_guest.begin (
				h as SlowHarness, win9x_config_from_environment (h as SlowHarness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/reports-the-platform-in-live-guest", () => {
			var h = new SlowHarness ((h) => reports_the_platform_in_live_guest.begin (
				h as SlowHarness, win9x_config_from_environment (h as SlowHarness), "windows"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/stalks-a-thread-in-live-guest", () => {
			var h = new SlowHarness ((h) => stalks_a_thread_in_live_guest.begin (
				h as SlowHarness, win9x_config_from_environment (h as SlowHarness),
				"KERNEL32.DLL"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/follows-a-thread-in-live-guest", () => {
			var h = new SlowHarness ((h) => follows_a_thread_in_live_guest.begin (h as SlowHarness,
				win9x_config_from_environment (h as SlowHarness), "KERNEL32.DLL"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/finds-a-thread-by-id-in-live-guest", () => {
			var h = new SlowHarness ((h) => finds_a_thread_by_id_in_live_guest.begin (h as SlowHarness,
				win9x_config_from_environment (h as SlowHarness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-its-own-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) => enumerates_its_own_threads_in_live_guest.begin (
				h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/watches-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) => watches_threads_in_live_guest.begin (h as SlowHarness,
				win9x_config_from_environment (h as SlowHarness), "C:\\WINDOWS\\NOTEPAD.EXE"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-applications-in-live-guest", () => {
			var h = new SlowHarness ((h) => enumerates_applications_in_live_guest.begin (
				h as SlowHarness, win9x_config_from_environment (h as SlowHarness), "Notepad",
				"com.microsoft.notepad"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/gates-two-spawns-at-once-in-live-guest", () => {
			var h = new SlowHarness ((h) => gates_two_spawns_at_once_in_live_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/gates-spawns-in-live-guest", () => {
			var h = new SlowHarness ((h) => gates_spawns_in_live_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/spawns-a-16-bit-program-in-live-guest", () => {
			var h = new Harness ((h) => spawns_a_16_bit_program_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/names-a-16-bit-process-in-live-guest", () => {
			var h = new Harness ((h) => names_a_16_bit_process_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/takes-a-big-script-in-live-guest", () => {
			var h = new Harness ((h) => takes_a_big_script_in_live_guest.begin (h as Harness,
				win9x_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => injects_into_process_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/answers-at-a-steady-pace-in-live-guest", () => {
			var h = new Harness ((h) => answers_at_a_steady_pace_in_live_guest.begin (h as Harness,
				win9x_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-target-modules-in-live-guest", () => {
			var h = new Harness ((h) => enumerates_target_modules_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/keeps-two-processes-apart-in-live-guest", () => {
			var h = new Harness ((h) => keeps_two_processes_apart_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/sees-a-module-arrive-in-live-guest", () => {
			var h = new Harness ((h) => win9x_sees_a_module_arrive_in_live_guest.begin (
				h as Harness));
			h.run ();
		});
		GLib.Test.add_func ("/Barebone/Win9x/hooks-before-resume-in-live-guest", () => {
			var h = new Harness ((h) => hooks_before_resume_in_live_guest.begin (h as Harness,
				win9x_config_from_environment (h as Harness), "C:\\WINDOWS\\NOTEPAD.EXE"));
			h.run ();
		});
		GLib.Test.add_func ("/Barebone/Win9x/takes-a-snapshot-in-live-guest", () => {
			var h = new Harness ((h) => takes_a_snapshot_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/hooks-its-own-code-in-live-guest", () => {
			var h = new Harness ((h) => hooks_its_own_code_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/hooks-only-its-own-process-in-live-guest", () => {
			var h = new Harness ((h) => hooks_only_its_own_process_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/spawns-and-resumes-in-live-guest", () => {
			var h = new Harness ((h) => win9x_spawns_and_resumes_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/shares-one-agent-between-sessions-in-live-guest", () => {
			var h = new Harness ((h) => shares_one_agent_between_sessions_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-threads-in-live-guest", () => {
			var h = new Harness ((h) => enumerates_threads_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-ranges-in-live-guest", () => {
			var h = new Harness ((h) => win9x_enumerates_ranges_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/enumerates-modules-in-live-guest", () => {
			var h = new Harness ((h) => enumerates_modules_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/copy-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => copy_recovers_from_exception_in_live_guest.begin (h as Harness,
				win9x_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Win9x/agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => agent_recovers_from_exception_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/modules-resolve-from-loaded-module-list", () => {
			var h = new Harness ((h) => modules_resolve_from_loaded_module_list.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/maps-out-a-64-bit-kernel-in-live-guest", () => {
			var h = new Harness ((h) => winnt_maps_out_a_64_bit_kernel_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/maps-out-an-arm64-kernel-in-live-guest", () => {
			var h = new Harness ((h) => winnt_maps_out_an_arm64_kernel_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => winnt_arm64_agent_runs_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) =>
				winnt_arm64_agent_recovers_from_exception_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-fires-a-timer-in-live-guest", () => {
			var h = new Harness ((h) =>
				winnt_arm64_fires_a_timer_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-enumerates-threads-in-live-guest", () => {
			var h = new Harness ((h) =>
				winnt_arm64_enumerates_threads_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-hooks-kernel-function-in-live-guest", () => {
			var h = new Harness ((h) => winnt_arm64_hooks_kernel_function_in_live_guest.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-sees-target-modules-and-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) =>
				winnt_arm64_sees_target_modules_and_threads_in_live_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-injects-into-process-in-live-guest", () => {
			var h = new SlowHarness ((h) =>
				winnt_arm64_injects_into_process_in_live_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/arm64-keeps-kernel-hook-in-live-guest", () => {
			var h = new SlowHarness ((h) => winnt_arm64_keeps_kernel_hook_in_live_guest.begin (h as SlowHarness));
			h.run ();
		});

		// One suite for each word size. Each suite uses its own set of variables and its own guest.
		GLib.Test.add_func ("/Barebone/WinNt/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => winnt_agent_runs_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/enumerates-modules-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_modules_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/compiles-c-calling-kernel-in-live-guest", () => {
			var h = new Harness ((h) => winnt_compiles_c_calling_kernel_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/copy-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => copy_recovers_from_exception_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => winnt_agent_recovers_from_exception_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/hooks-kernel-function-in-live-guest", () => {
			var h = new Harness ((h) => winnt_hooks_kernel_function_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/enumerates-applications-in-live-guest", () => {
			var h = new SlowHarness ((h) => enumerates_applications_in_live_guest.begin (
				h as SlowHarness, winnt_config_from_environment (h as SlowHarness, "WINNT"),
				"Notepad", "com.microsoft.notepad"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/moves-a-lot-of-data-in-live-guest", () => {
			var h = new SlowHarness ((h) => moves_a_lot_of_data_in_live_guest.begin (
				h as SlowHarness, winnt_config_from_environment (h as SlowHarness, "WINNT")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/reports-the-platform-in-live-guest", () => {
			var h = new SlowHarness ((h) => reports_the_platform_in_live_guest.begin (
				h as SlowHarness, winnt_config_from_environment (h as SlowHarness, "WINNT"), "windows"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/stalks-a-thread-in-live-guest", () => {
			var h = new SlowHarness ((h) => stalks_a_thread_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT"), "kernel32.dll"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/follows-a-thread-in-live-guest", () => {
			var h = new SlowHarness ((h) => follows_a_thread_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT"), "kernel32.dll"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/finds-a-thread-by-id-in-live-guest", () => {
			var h = new SlowHarness ((h) => finds_a_thread_by_id_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/watches-its-own-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) => winnt_watches_its_own_threads_in_live_guest.begin (
				h as SlowHarness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/answers-at-a-steady-pace-in-live-guest", () => {
			var h = new Harness ((h) => answers_at_a_steady_pace_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => winnt_injects_into_process_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/enumerates-target-modules-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_target_modules_in_live_guest.begin (
				h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/sees-a-module-arrive-in-live-guest", () => {
			var h = new Harness ((h) => winnt_sees_a_module_arrive_in_live_guest.begin (
				h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/watches-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) => watches_threads_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT"),
				"C:\\WINDOWS\\system32\\notepad.exe"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/takes-a-big-script-in-live-guest", () => {
			var h = new Harness ((h) => takes_a_big_script_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/attaches-again-after-detach-in-live-guest", () => {
			var h = new Harness ((h) => attaches_again_after_detach_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/calls-system-functions-in-live-guest", () => {
			var h = new Harness ((h) => winnt_calls_system_functions_in_live_guest.begin (
				h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/hooks-before-resume-in-live-guest", () => {
			var h = new Harness ((h) => hooks_before_resume_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT"),
				"C:\\WINDOWS\\system32\\notepad.exe"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/spawns-and-resumes-in-live-guest", () => {
			var h = new Harness ((h) => winnt_spawns_and_resumes_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_processes_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/reads-and-writes-memory-in-live-guest", () => {
			var h = new Harness ((h) => winnt_reads_and_writes_memory_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/enumerates-threads-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_threads_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/enumerates-ranges-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_ranges_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt/resolves-symbols-in-live-guest", () => {
			var h = new Harness ((h) => winnt_resolves_symbols_in_live_guest.begin (h as Harness, "WINNT"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/moves-a-lot-of-data-in-live-guest", () => {
			var h = new SlowHarness ((h) => moves_a_lot_of_data_in_live_guest.begin (
				h as SlowHarness, winnt_config_from_environment (h as SlowHarness, "WINNT64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/reports-the-platform-in-live-guest", () => {
			var h = new SlowHarness ((h) => reports_the_platform_in_live_guest.begin (
				h as SlowHarness, winnt_config_from_environment (h as SlowHarness, "WINNT64"), "windows"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/stalks-a-thread-in-live-guest", () => {
			var h = new SlowHarness ((h) => stalks_a_thread_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT64"), "kernel32.dll"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/follows-a-thread-in-live-guest", () => {
			var h = new SlowHarness ((h) => follows_a_thread_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT64"), "kernel32.dll"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/finds-a-thread-by-id-in-live-guest", () => {
			var h = new SlowHarness ((h) => finds_a_thread_by_id_in_live_guest.begin (h as SlowHarness,
				winnt_config_from_environment (h as SlowHarness, "WINNT64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/watches-its-own-threads-in-live-guest", () => {
			var h = new SlowHarness ((h) => winnt_watches_its_own_threads_in_live_guest.begin (
				h as SlowHarness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => winnt_agent_runs_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/enumerates-modules-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_modules_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/compiles-c-calling-kernel-in-live-guest", () => {
			var h = new Harness ((h) => winnt_compiles_c_calling_kernel_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/copy-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => copy_recovers_from_exception_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => winnt_agent_recovers_from_exception_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/hooks-kernel-function-in-live-guest", () => {
			var h = new Harness ((h) => winnt_hooks_kernel_function_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/takes-a-big-script-in-live-guest", () => {
			var h = new Harness ((h) => takes_a_big_script_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/answers-at-a-steady-pace-in-live-guest", () => {
			var h = new Harness ((h) => answers_at_a_steady_pace_in_live_guest.begin (h as Harness,
				winnt_config_from_environment (h as Harness, "WINNT64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => winnt_injects_into_process_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/enumerates-target-modules-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_target_modules_in_live_guest.begin (
				h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/calls-system-functions-in-live-guest", () => {
			var h = new Harness ((h) => winnt_calls_system_functions_in_live_guest.begin (
				h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/spawns-and-resumes-in-live-guest", () => {
			var h = new Harness ((h) => winnt_spawns_and_resumes_in_live_guest.begin (h as Harness,
				"WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_processes_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/reads-and-writes-memory-in-live-guest", () => {
			var h = new Harness ((h) => winnt_reads_and_writes_memory_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/enumerates-threads-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_threads_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/enumerates-ranges-in-live-guest", () => {
			var h = new Harness ((h) => winnt_enumerates_ranges_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/WinNt64/resolves-symbols-in-live-guest", () => {
			var h = new Harness ((h) => winnt_resolves_symbols_in_live_guest.begin (h as Harness, "WINNT64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => xnu_enumerates_processes_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => xnu_injects_into_process_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/enumerates-target-modules-in-live-guest", () => {
			var h = new Harness ((h) => xnu_enumerates_target_modules_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/hides-itself-from-the-process-in-live-guest", () => {
			var h = new Harness ((h) => xnu_hides_itself_from_the_process_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/enumerates-target-ranges-in-live-guest", () => {
			var h = new Harness ((h) => xnu_enumerates_target_ranges_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/enumerates-target-threads-in-live-guest", () => {
			var h = new Harness ((h) => xnu_enumerates_target_threads_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/keeps-two-processes-apart-in-live-guest", () => {
			var h = new Harness ((h) => xnu_keeps_two_processes_apart_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/probe", () => {
			var h = new Harness ((h) => xnu_probe.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/probe", () => {
			var h = new Harness ((h) => xnu_probe.begin (
				h as Harness, linux_config_from_environment (h as Harness, "LINUX_X86_64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/probe-user", () => {
			var h = new Harness ((h) => linux_probe_user.begin (
				h as Harness, linux_config_from_environment (h as Harness, "LINUX_X86_64")));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/hooks-its-own-process-in-live-guest", () => {
			var h = new Harness ((h) => xnu_hooks_its_own_process_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/leaves-one-image-in-a-process-in-live-guest", () => {
			var h = new Harness ((h) => xnu_leaves_one_image_in_a_process_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/finds-the-image-the-last-agent-left-in-live-guest", () => {
			var h = new Harness ((h) => xnu_leaves_one_image_in_a_process_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/leaves-nothing-behind-in-live-guest", () => {
			var h = new Harness ((h) => xnu_leaves_nothing_behind_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/answers-at-a-steady-pace-in-live-guest", () => {
			var h = new Harness ((h) => xnu_answers_at_a_steady_pace_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/enumerates-applications-in-live-guest", () => {
			var h = new SlowHarness ((h) => enumerates_applications_in_live_guest.begin (
				h as SlowHarness, xnu_config_from_environment (h as SlowHarness), "Magnifier",
				"com.apple.Magnifier"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/hooks-before-resume-in-live-guest", () => {
			var h = new Harness ((h) => xnu_hooks_before_resume_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness),
				"com.apple.Preferences"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Xnu/moves-frames-at-a-good-rate-in-live-guest", () => {
			var h = new Harness ((h) => xnu_moves_frames_at_a_good_rate_in_live_guest.begin (
				h as Harness, xnu_config_from_environment (h as Harness)));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/Config/parses-kernel-kind", () => {
			assert_true (parse_config ("{}").kernel == BareboneKernelKind.AUTO);
			assert_true (parse_config ("{ \"kernel\": \"bare\" }").kernel == BareboneKernelKind.BARE);
			assert_true (parse_config ("{ \"kernel\": \"xnu\" }").kernel == BareboneKernelKind.XNU);
			assert_true (parse_config ("{ \"kernel\": \"win9x\" }").kernel == BareboneKernelKind.WIN9X);
			assert_true (parse_config ("{ \"kernel\": \"winnt\" }").kernel == BareboneKernelKind.WINNT);
		});

		GLib.Test.add_func ("/Barebone/Config/parses-allocator-arguments", () => {
			var allocator = (BareboneTargetFunctionsAllocatorConfig) parse_config (
				"{ \"allocator\": { \"mode\": \"target-functions\", \"alloc_function\": \"804e1000\", \"free_function\": \"804e2000\", \"alloc_arguments\": [ \"0\", \"size\", \"64697246\" ], \"free_arguments\": [ \"address\", \"64697246\" ] } }").allocator;

			var alloc = allocator._effective_alloc_arguments ();
			assert_true (alloc.size == 3);
			assert_argument (alloc[0], BareboneCallArgumentRole.LITERAL, 0);
			assert_argument (alloc[1], BareboneCallArgumentRole.SIZE, 0);
			assert_argument (alloc[2], BareboneCallArgumentRole.LITERAL, 0x64697246);

			var free = allocator._effective_free_arguments ();
			assert_true (free.size == 2);
			assert_argument (free[0], BareboneCallArgumentRole.ADDRESS, 0);
			assert_argument (free[1], BareboneCallArgumentRole.LITERAL, 0x64697246);

			// Without a template, the flags still give the argument list.
			var shorthand = (BareboneTargetFunctionsAllocatorConfig) parse_config (
				"{ \"allocator\": { \"mode\": \"target-functions\", \"alloc_function\": \"1000\", \"free_function\": \"2000\", \"alloc_flags\": 3 } }").allocator;

			var inferred = shorthand._effective_alloc_arguments ();
			assert_true (inferred.size == 2);
			assert_argument (inferred[0], BareboneCallArgumentRole.SIZE, 0);
			assert_argument (inferred[1], BareboneCallArgumentRole.LITERAL, 3);
		});

		GLib.Test.add_func ("/Barebone/X64/enumerate-ranges-walks-long-mode-tables", () => {
			var h = new Harness ((h) => enumerate_ranges_walks_long_mode_tables.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/translate-address-resolves-leaf-mappings", () => {
			var h = new Harness ((h) => x64_translate_address_resolves_leaf_mappings.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/protect-pages-updates-long-mode-entries", () => {
			var h = new Harness ((h) => protect_pages_updates_long_mode_entries.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/protect-pages-copes-with-large-pages", () => {
			var h = new Harness ((h) => x64_protect_pages_copes_with_large_pages.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/relocations-apply-load-bias", () => {
			var h = new Harness ((h) => x64_relocations_apply_load_bias.begin (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/QEMU/walk-matches-guest", () => {
			var h = new SlowHarness ((h) => QEMU.walk_matches_x86_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/QEMU/allocate-pages-maps-into-guest", () => {
			var h = new SlowHarness ((h) => QEMU.allocate_pages_maps_into_x86_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/QEMU/invoke-calls-into-guest", () => {
			var h = new SlowHarness ((h) => QEMU.invoke_calls_into_x86_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/QEMU/walk-finds-the-kernel", () => {
			var h = new SlowHarness ((h) => QEMU.walk_finds_the_arm_kernel.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/QEMU/whole-agent-loads-into-guest", () => {
			var h = new SlowHarness ((h) => QEMU.whole_agent_loads_into_arm_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_runs_in_live_guest.begin (h as Harness, "LINUX_ARM"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_recovers_from_exception_in_live_guest.begin (h as Harness,
				"LINUX_ARM"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/rpc-replies-in-live-guest", () => {
			var h = new Harness ((h) => linux_rpc_replies_in_live_guest.begin (h as Harness, "LINUX_ARM"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => linux_injects_into_process_in_live_guest.begin (h as Harness,
				"LINUX_ARM"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => linux_enumerates_processes_in_live_guest.begin (h as Harness,
				"LINUX_ARM"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM64/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_runs_in_live_guest.begin (h as Harness, "LINUX_ARM64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM64/rpc-replies-in-live-guest", () => {
			var h = new Harness ((h) => linux_rpc_replies_in_live_guest.begin (h as Harness, "LINUX_ARM64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/ARM64/rpc-replies-in-process-in-live-guest", () => {
			var h = new Harness ((h) => linux_rpc_replies_in_process_in_live_guest.begin (h as Harness, "LINUX_ARM64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_runs_in_live_guest.begin (h as Harness, "LINUX_X86"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_recovers_from_exception_in_live_guest.begin (h as Harness,
				"LINUX_X86"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/rpc-replies-in-live-guest", () => {
			var h = new Harness ((h) => linux_rpc_replies_in_live_guest.begin (h as Harness, "LINUX_X86"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => linux_injects_into_process_in_live_guest.begin (h as Harness,
				"LINUX_X86"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => linux_enumerates_processes_in_live_guest.begin (h as Harness,
				"LINUX_X86"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/agent-runs-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_runs_in_live_guest.begin (h as Harness, "LINUX_X86_64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/rpc-replies-in-live-guest", () => {
			var h = new Harness ((h) => linux_rpc_replies_in_live_guest.begin (h as Harness, "LINUX_X86_64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/injects-into-process-in-live-guest", () => {
			var h = new Harness ((h) => linux_injects_into_process_in_live_guest.begin (h as Harness,
				"LINUX_X86_64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/enumerates-processes-in-live-guest", () => {
			var h = new Harness ((h) => linux_enumerates_processes_in_live_guest.begin (h as Harness,
				"LINUX_X86_64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/agent-recovers-from-exception-in-live-guest", () => {
			var h = new Harness ((h) => linux_agent_recovers_from_exception_in_live_guest.begin (h as Harness,
				"LINUX_X86_64"));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/QEMU/walk-matches-guest", () => {
			var h = new SlowHarness ((h) => QEMU.walk_matches_x86_64_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/QEMU/allocate-pages-maps-into-guest", () => {
			var h = new SlowHarness ((h) => QEMU.allocate_pages_maps_into_x86_64_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/QEMU/invoke-calls-into-guest", () => {
			var h = new SlowHarness ((h) => QEMU.invoke_calls_into_x86_64_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/QEMU/inline-hook-fires-in-guest", () => {
			var h = new SlowHarness ((h) => QEMU.inline_hook_fires_in_x86_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/QEMU/injected-elf-runs-in-guest", () => {
			var h = new SlowHarness ((h) => QEMU.injected_elf_runs_in_x86_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/IA32/QEMU/whole-agent-loads-into-guest", () => {
			var h = new SlowHarness ((h) => QEMU.whole_agent_loads_into_x86_guest.begin (h as SlowHarness));
			h.run ();
		});

		GLib.Test.add_func ("/Barebone/X64/QEMU/inline-hook-fires-in-guest", () => {
			var h = new SlowHarness ((h) => QEMU.inline_hook_fires_in_x86_64_guest.begin (h as SlowHarness));
			h.run ();
		});
	}

	private BareboneConfig? xnu_config_from_environment (Frida.Test.AsyncHarness h) {
		string? config_path = Environment.get_variable ("FRIDA_BAREBONE_CONFIG");
		if (config_path == null) {
			h.done ();
			return null;
		}

		string contents;
		try {
			FileUtils.get_contents (config_path, out contents);
		} catch (FileError e) {
			h.done ();
			return null;
		}

		var config = parse_config (contents);
		if (config.kernel != BareboneKernelKind.XNU) {
			h.done ();
			return null;
		}

		return config;
	}

	private async void xnu_enumerates_processes_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var options = new ProcessQueryOptions ();
			options.scope = FULL;
			var processes = yield device.enumerate_processes (options, null);

			assert_true (processes.size () > 10);

			Process? first = null;
			Process? board = null;
			for (int i = 0; i != processes.size (); i++) {
				var one = processes.get (i);
				assert_true (one.name != "");
				if (one.pid == 1)
					first = one;
				if (one.name == "backboardd")
					board = one;
			}

			assert_nonnull (first);
			assert_true (first.name == "launchd");
			assert_nonnull (board);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_injects_into_process_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				send([Process.id, Process.platform, Process.arch, Process.pointerSize]);
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			assert_true (said.contains ("darwin"));
			assert_true (said.contains ("arm64"));
			assert_true (said.contains (pid.to_string ()));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_enumerates_target_modules_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const modules = Process.enumerateModules();
				const first = modules[0];
				const dyld = modules.find(m => m.name === 'dyld');
				const system = modules.find(m => m.name.startsWith('libsystem_kernel'));
				send([modules.length, first.name, dyld !== undefined, system !== undefined,
					Module.getGlobalExportByName('mach_task_self') !== null]);
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			assert_true (said.contains ("backboardd"));
			assert_true (said.contains ("true,true,true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_hides_itself_from_the_process_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const p = (n) => Module.getGlobalExportByName(n);
				const taskSelf = new NativeFunction(p('mach_task_self'), 'uint', []);
				const threadSelf = new NativeFunction(p('mach_thread_self'), 'uint', []);
				const taskThreads = new NativeFunction(p('task_threads'), 'int',
					['uint', 'pointer', 'pointer']);
				const suspend = new NativeFunction(p('thread_suspend'), 'int', ['uint']);
				const resume = new NativeFunction(p('thread_resume'), 'int', ['uint']);
				const procPidinfo = new NativeFunction(p('proc_pidinfo'), 'int',
					['int', 'int', 'uint64', 'pointer', 'int']);
				const getpid = new NativeFunction(p('getpid'), 'int', []);
				const fromPort = new NativeFunction(p('pthread_from_mach_thread_np'), 'pointer',
					['uint']);
				const threadId = new NativeFunction(p('pthread_threadid_np'), 'int',
					['pointer', 'pointer']);
				const portNames = new NativeFunction(p('mach_port_names'), 'int',
					['uint', 'pointer', 'pointer', 'pointer', 'pointer']);

				const ours = threadSelf();
				const ourId = Memory.alloc(8);
				threadId(ptr(0), ourId);

				let asked = false;
				Interceptor.attach(p('mach_msg'), function () {
					if (asked)
						return;
					asked = true;

					const list = Memory.alloc(8), count = Memory.alloc(4);
					taskThreads(taskSelf(), list, count);
					const listed = count.readU32();

					const info = Memory.alloc(256);
					procPidinfo(getpid(), 4, 0, info, 256);
					const counted = info.add(84).readU32();

					const here = threadSelf();
					let theirs = 0;
					for (let i = 0; i !== listed; i++) {
						const name = list.readPointer().add(i * 4).readU32();
						if (name !== here && name !== ours)
							theirs = name;
					}
					const refused = suspend(ours);
					const allowed = suspend(theirs);
					resume(theirs);

					const names = Memory.alloc(8), nameCount = Memory.alloc(4);
					const types = Memory.alloc(8), typeCount = Memory.alloc(4);
					portNames(taskSelf(), names, nameCount, types, typeCount);
					let onTheList = false;
					const at = names.readPointer();
					for (let i = 0; i !== nameCount.readU32(); i++) {
						const thread = fromPort(at.add(i * 4).readU32());
						if (thread.isNull())
							continue;
						const id = Memory.alloc(8);
						threadId(thread, id);
						if (id.readU64().equals(ourId.readU64()))
							onTheList = true;
					}

					send([listed, counted, refused, allowed, onTheList]);
				});
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			var payload = said.substring (said.index_of ("[") + 1);
			var parts = payload.substring (0, payload.index_of ("]")).split (",");
			assert_true (parts.length == 5);

			assert_true (parts[0] == parts[1]);
			assert_true (parts[2] != "0");
			assert_true (parts[3] == "0");
			assert_true (parts[4] == "false");
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_enumerates_target_ranges_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const ranges = Process.enumerateRanges('r--');
				const image = Process.mainModule;
				const holdingIt = ranges.find(r => r.base.compare(image.base) <= 0
					&& r.base.add(r.size).compare(image.base) > 0);
				const ordered = ranges.every((r, i) =>
					i === 0 || ranges[i - 1].base.compare(r.base) < 0);
				send([ranges.length, holdingIt !== undefined, ordered,
					image.base.readU32() === 0xfeedfacf]);
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			assert_true (said.contains ("true,true,true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_enumerates_target_threads_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const threads = Process.enumerateThreads();
				const here = Process.getCurrentThreadId();
				const holdingIt = threads.find(t => t.id === here);
				send([threads.length, holdingIt !== undefined,
					threads.every(t => t.id !== 0)]);
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			assert_true (said.contains ("true,true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_keeps_two_processes_apart_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint board = yield find_program (device, "backboardd");
			uint watcher = yield find_program (device, "fseventsd");
			assert_true (board != 0 && watcher != 0 && board != watcher);

			string? said_here = null, said_there = null;
			var here = yield device.attach (board, null, null);
			var there = yield device.attach (watcher, null, null);

			var one = yield here.create_script (
				"send([Process.id, Process.mainModule.name]);", null, null);
			one.message.connect ((json, data) => {
				said_here = json;
			});
			yield one.load (null);

			var other = yield there.create_script (
				"send([Process.id, Process.mainModule.name]);", null, null);
			other.message.connect ((json, data) => {
				said_there = json;
			});
			yield other.load (null);

			while (said_here == null || said_there == null)
				yield h.process_events ();

			assert_true (said_here.contains (board.to_string ()));
			assert_true (said_here.contains ("backboardd"));
			assert_true (said_there.contains (watcher.to_string ()));
			assert_true (said_there.contains ("fseventsd"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void linux_probe_user (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		string? path = Environment.get_variable ("FRIDA_PROBE_JS");
		if (path == null)
			return;

		string source;
		try {
			FileUtils.get_contents (path, out source);
		} catch (GLib.Error e) {
			return;
		}

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_program (device, "busybox");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script (source, null, null);

			bool finished = false;
			script.message.connect ((json, data) => {
				printerr ("\nPROBE %s\n", json);
				if (json.contains ("done"))
					finished = true;
			});
			yield script.load (null);

			while (!finished)
				yield h.process_events ();
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
		}

		yield h.process_events ();
		h.done ();
	}

	private async void xnu_probe (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		string? path = Environment.get_variable ("FRIDA_PROBE_JS");
		if (path == null)
			return;

		string source;
		try {
			FileUtils.get_contents (path, out source);
		} catch (GLib.Error e) {
			return;
		}

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var session = yield device.attach (0, null, null);
			var script = yield session.create_script (source, null, null);

			bool finished = false;
			script.message.connect ((json, data) => {
				printerr ("\nPROBE %s\n", json);
				if (json.contains ("done"))
					finished = true;
			});
			yield script.load (null);

			while (!finished)
				yield h.process_events ();
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
		}

		yield h.process_events ();
		h.done ();
	}

	private async void xnu_hooks_its_own_process_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const open = Module.getGlobalExportByName('open');
				let caught = 0;
				Interceptor.attach(open, {
					onEnter(args) {
						if (caught === 0)
							send(['caught', args[0].readUtf8String()]);
						caught++;
					}
				});

				const openIt = new NativeFunction(open, 'int', ['pointer', 'int']);
				const close = new NativeFunction(Module.getGlobalExportByName('close'), 'int',
					['int']);
				const path = Memory.allocUtf8String('/dev/null');
				const fd = openIt(path, 0);
				if (fd !== -1)
					close(fd);
				send(['done', caught !== 0, fd !== -1]);
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				if (json.contains ("done"))
					said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			assert_true (said.contains ("true,true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_leaves_one_image_in_a_process_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const loose = Process.enumerateRanges('r-x').filter(r => r.file === undefined
					&& r.protection === 'r-x');
				const biggest = loose.reduce((most, r) => Math.max(most, r.size), 0);
				send({ images: loose.filter(r => r.size === biggest).length,
					biggest: biggest.toString(16), of: loose.length });
			""", null, null);

			string? said = null;
			script.message.connect ((json, data) => {
				said = json;
			});
			yield script.load (null);

			while (said == null)
				yield h.process_events ();

			printerr ("\nIMAGES: %s\n", said);

			var counted = said.substring (said.index_of ("\"images\":") + 9);
			uint64 images = uint64.parse (counted.substring (0, counted.index_of (",")));
			assert_true (images == 1);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_leaves_nothing_behind_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		DeviceManager? manager = null;
		try {
			manager = new DeviceManager ();
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "backboardd");
			assert_true (pid != 0);

			uint64 first = 0, last = 0, images_left = 0;
			for (uint round = 0; round != 4; round++) {
				var session = yield device.attach (pid, null, null);
				var script = yield session.create_script ("""
					const p = (n) => Module.getGlobalExportByName(n);
					const procPidinfo = new NativeFunction(p('proc_pidinfo'), 'int',
						['int', 'int', 'uint64', 'pointer', 'int']);
					const getpid = new NativeFunction(p('getpid'), 'int', []);
					const info = Memory.alloc(256);
					procPidinfo(getpid(), 4, 0, info, 256);
					const left = Process.enumerateRanges('---').filter(r =>
						r.file === undefined && r.protection === '---' && r.size >= 0x100000);
					send({ mapped: info.readU64().toNumber(), left: left.length });
				""", null, null);

				string? said = null;
				script.message.connect ((json, data) => {
					said = json;
				});
				yield script.load (null);

				while (said == null)
					yield h.process_events ();

				var digits = said.substring (said.index_of ("\"mapped\":") + 9);
				uint64 mapped = uint64.parse (digits.substring (0, digits.index_of (",")));
				var counted = said.substring (said.index_of ("\"left\":") + 7);
				uint64 left = uint64.parse (counted.substring (0, counted.index_of ("}")));
				if (round == 0)
					images_left = left;
				assert_true (left <= images_left);
				printerr ("\nMAPPED after round %u: %s\n", round, said);
				if (round == 0)
					first = mapped;
				last = mapped;

				yield session.detach (null);
			}

			printerr ("\nGREW by %" + uint64.FORMAT + " over three rounds, %" + uint64.FORMAT
				+ " images left\n", last - first, images_left);
			assert_true (last - first < ROOM_FOR_WHAT_THE_PROCESS_DOES);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			if (manager != null) {
				try {
					yield manager.close (null);
				} catch (GLib.Error e) {
				}
			}
		}

		h.done ();
	}

	private const uint64 ROOM_FOR_WHAT_THE_PROCESS_DOES = 3 * 1024 * 1024;

	private async void xnu_answers_at_a_steady_pace_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			yield measure_the_pace (h, yield device.attach (0, null, null), "kernel");

			uint pid = yield find_program (device, "fseventsd");
			assert_true (pid != 0);
			yield measure_the_pace (h, yield device.attach (pid, null, null), "fseventsd");
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_moves_frames_at_a_good_rate_in_live_guest (Harness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			yield measure_the_flow (h, yield device.attach (0, null, null), "kernel");

			uint pid = yield find_program (device, "fseventsd");
			assert_true (pid != 0);
			yield measure_the_flow (h, yield device.attach (pid, null, null), "fseventsd");
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void xnu_hooks_before_resume_in_live_guest (Harness h, BareboneConfig? config,
			string program) {
		if (config == null)
			return;

		h.disable_timeout ();

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield device.spawn (program, null, null);
			var session = yield device.attach (pid, null, null);

			var script = yield session.create_script ("""
				const image = Process.enumerateModules()[0];
				let at = image.base.add(32);
				let entry = null;
				const howMany = image.base.add(16).readU32();
				for (let i = 0; i !== howMany && entry === null; i++) {
					const kind = at.readU32();
					const length = at.add(4).readU32();
					if (kind === 0x80000028)
						entry = image.base.add(at.add(8).readU64());
					at = at.add(length);
				}
				Interceptor.attach(entry, {
					onEnter() {
						send(['entered', image.name]);
					}
				});
				send(['armed', image.name, entry.toString(), Process.enumerateModules().length]);
			""", null, null);

			bool armed = false;
			bool entered = false;
			script.message.connect ((json, data) => {
				printerr ("\nHELD: %s\n", json);
				if (json.contains ("armed"))
					armed = true;
				else if (json.contains ("entered"))
					entered = true;
			});
			yield script.load (null);

			while (!armed)
				yield h.process_events ();

			yield device.resume (pid, null);

			while (!entered)
				yield h.process_events ();
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void measure_the_flow (Harness h, Session session, string where)
			throws GLib.Error {
		var script = yield session.create_script ("""
			const chunk = 'x'.repeat(%u);
			let taken = 0;
			recv('go', function onGo () {
				for (let i = 0; i !== %u; i++)
					send(chunk);
				send('sent');
				recv(function onChunk (message) {
					taken++;
					send('taken ' + taken);
					recv(onChunk);
				});
			});
			send('ready');
		""".printf (A_FRAME, FRAMES), null, null);

		string? said = null;
		uint received = 0;
		bool waiting = false;
		script.message.connect ((json, data) => {
			received++;
			said = json;
			if (waiting) {
				waiting = false;
				measure_the_flow.callback ();
			}
		});

		yield script.load (null);
		while (received < 1) {
			waiting = true;
			yield;
		}
		uint before = received;
		int64 started = get_monotonic_time ();
		script.post ("""{"type":"go"}""");
		while (received < before + FRAMES + 1) {
			waiting = true;
			yield;
		}
		int64 out_of_it = get_monotonic_time () - started;

		var chunk = new StringBuilder ();
		while (chunk.len < A_FRAME)
			chunk.append_c ('x');
		var message = """{"type":"chunk","payload":"""" + chunk.str + """"}""";

		before = received;
		started = get_monotonic_time ();
		for (uint i = 0; i != FRAMES; i++)
			script.post (message);

		uint gave_up_at = 0;
		var deadline = new TimeoutSource (LONG_ENOUGH_TO_ARRIVE_MS);
		deadline.set_callback (() => {
			gave_up_at = received;
			if (waiting) {
				waiting = false;
				measure_the_flow.callback ();
			}
			return Source.REMOVE;
		});
		deadline.attach (MainContext.get_thread_default ());

		while (received < before + FRAMES && gave_up_at == 0) {
			waiting = true;
			yield;
		}
		int64 into_it = get_monotonic_time () - started;
		deadline.destroy ();

		uint arrived = received - before;

		printerr ("\nFLOW in %s over %u frames of %u bytes: out %.1f MB/s (%.0f frames/s), " +
				"in %.1f MB/s (%.0f frames/s)\n\n",
			where, FRAMES, A_FRAME,
			rate_of (FRAMES * A_FRAME, out_of_it), (double) FRAMES / seconds_of (out_of_it),
			rate_of (arrived * A_FRAME, into_it), (double) arrived / seconds_of (into_it));

		assert_true (arrived == FRAMES);
		assert_true ("taken %u".printf (FRAMES) in said);

		yield script.unload (null);
	}

	private double seconds_of (int64 microseconds) {
		return (double) microseconds / 1000000.0;
	}

	private double rate_of (uint bytes, int64 microseconds) {
		return ((double) bytes / (1024.0 * 1024.0)) / seconds_of (microseconds);
	}

	private const uint LONG_ENOUGH_TO_ARRIVE_MS = 10000;
	private const uint A_FRAME = 8192;
	private const uint FRAMES = 64;

	private BareboneConfig parse_config (string json) {
		try {
			return (BareboneConfig) Json.gobject_from_data (typeof (BareboneConfig), json);
		} catch (GLib.Error e) {
			assert_not_reached ();
		}
	}

	private async void enumerate_ranges_walks_short_descriptor_tables (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);

			assert_true (ranges.size == 5);

			assert_range (ranges[0], 0x00000000, 0x00100000, 0x1000, READ | WRITE | EXECUTE);
			assert_range (ranges[1], 0x00001000, 0x00101000, 0x1000, READ | WRITE);
			assert_range (ranges[2], 0x00010000, 0x00110000, 0x10000, READ | WRITE | EXECUTE);
			assert_range (ranges[3], 0x00100000, 0x00800000, 0x100000, READ | WRITE | EXECUTE);
			assert_range (ranges[4], 0x00200000, 0x00900000, 0x100000, READ);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void arm_enumerate_ranges_honors_protection_filter (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.EXECUTE);

			assert_true (ranges.size == 3);

			assert_range (ranges[0], 0x00000000, 0x00100000, 0x1000, READ | WRITE | EXECUTE);
			assert_range (ranges[1], 0x00010000, 0x00110000, 0x10000, READ | WRITE | EXECUTE);
			assert_range (ranges[2], 0x00100000, 0x00800000, 0x100000, READ | WRITE | EXECUTE);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void enumerate_ranges_rejects_lpae_tables (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_lpae_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			try {
				yield collect_ranges (machine, Gum.PageProtection.READ);
				assert_not_reached ();
			} catch (GLib.Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void arm_allocate_pages_takes_free_entries (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			var wanted = new Gee.ArrayList<uint64?> ();
			wanted.add (0x00300000);
			wanted.add (0x00301000);

			var allocation = yield machine.allocate_pages (wanted, null);

			assert_true (allocation.virtual_address == 0x00002000);
			assert_true (allocation.size == 0x2000);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.WRITE);
			assert_range (ranges[2], 0x00002000, 0x00300000, 0x2000, READ | WRITE | EXECUTE);

			yield allocation.deallocate (null);

			var after = yield collect_ranges (machine, Gum.PageProtection.WRITE);
			assert_true (after.size == ranges.size - 1);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void arm_allocate_pages_spans_leaf_tables (Harness h) {
		var target = new FakeTarget (ARM, adjacent_arm_leaf_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			var wanted = new Gee.ArrayList<uint64?> ();
			for (uint i = 0; i != 4; i++)
				wanted.add (0x00300000 + (i * 0x1000));

			var allocation = yield machine.allocate_pages (wanted, null);

			assert_true (allocation.virtual_address == 0x000fe000);
			assert_true (allocation.size == 0x4000);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.WRITE);
			assert_range (ranges[1], 0x000fe000, 0x00300000, 0x4000, READ | WRITE | EXECUTE);

			yield allocation.deallocate (null);

			var after = yield collect_ranges (machine, Gum.PageProtection.WRITE);
			assert_true (after.size == ranges.size - 1);
		} catch (GLib.Error e) {
			printerr ("
FAIL: %s
", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void arm_protect_pages_updates_page_entries (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			yield machine.protect_pages (0x00000000, 0x1000, READ | EXECUTE, null);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);
			assert_range (ranges[0], 0x00000000, 0x00100000, 0x1000, READ | EXECUTE);

			yield machine.protect_pages (0x00001000, 0x1000, READ, null);

			ranges = yield collect_ranges (machine, Gum.PageProtection.READ);
			assert_range (ranges[1], 0x00001000, 0x00101000, 0x1000, READ);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void arm_protect_pages_rejects_sections (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			yield machine.protect_pages (0x00100000, 0x1000, READ, null);

			try {
				yield machine.protect_pages (0x00200000, 0x1000, READ | WRITE, null);
				assert_not_reached ();
			} catch (GLib.Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void arm_relocations_apply_load_bias (Harness h) {
		var target = new FakeTarget (ARM, short_descriptor_page_tables (), null, HIDE_CONTROL_REGISTERS,
			arm_short_descriptor_state ());
		try {
			yield target.open ();
			var machine = new Barebone.ArmMachine (target.client);

			var buf = target.client.make_buffer_builder ()
				.append_uint32 (0x1000)
				.append_uint32 (0x2000)
				.build ();
			var relocated = target.client.make_buffer (buf);

			var relative = Gum.ElfRelocationDetails ();
			relative.address = 0;
			relative.type = Gum.ElfArmRelocation.RELATIVE;
			machine.apply_relocation (relative, 0x40000000, relocated);

			var absolute = Gum.ElfRelocationDetails ();
			absolute.address = 4;
			absolute.type = Gum.ElfArmRelocation.ABS32;
			machine.apply_relocation (absolute, 0x40000000, relocated);

			assert_true (relocated.read_uint32 (0) == 0x40001000);
			assert_true (relocated.read_uint32 (4) == 0x40002000);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void enumerate_ranges_walks_legacy_tables (Harness h) {
		var target = new FakeTarget (IA32, legacy_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);

			assert_true (ranges.size == 3);

			assert_range (ranges[0], 0x00000000, 0x00100000, 0x2000, READ | WRITE | EXECUTE);
			assert_range (ranges[1], 0x00002000, 0x00102000, 0x1000, READ | EXECUTE);

			assert_range (ranges[2], 0x00400000, 0x00800000, 0x400000, READ | EXECUTE);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void enumerate_ranges_walks_pae_tables (Harness h) {
		var target = new FakeTarget (IA32, pae_page_tables (), PAE_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);

			assert_true (ranges.size == 3);

			assert_range (ranges[0], 0x00000000, 0x00100000, 0x1000, READ | WRITE);
			assert_range (ranges[1], 0x00001000, 0x00101000, 0x1000, READ | WRITE | EXECUTE);

			assert_range (ranges[2], 0x00200000, 0x140000000, 0x200000, READ);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void enumerate_ranges_honors_protection_filter (Harness h) {
		var target = new FakeTarget (IA32, legacy_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.WRITE);

			assert_true (ranges.size == 1);
			assert_range (ranges[0], 0x00000000, 0x00100000, 0x2000, READ | WRITE | EXECUTE);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void control_registers_read_from_target_description (Harness h) {
		// No monitor command here: the machine must prefer the exposed registers.
		var target = new FakeTarget (IA32, legacy_page_tables (), null, EXPOSE_CONTROL_REGISTERS);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);

			assert_true (ranges.size == 3);
			assert_range (ranges[0], 0x00000000, 0x00100000, 0x2000, READ | WRITE | EXECUTE);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void translate_address_resolves_leaf_mappings (Harness h) {
		var target = new FakeTarget (IA32, legacy_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			assert_true ((yield machine.translate_address (0x00002010, null)) == 0x00102010);
			assert_true ((yield machine.translate_address (0x00400123, null)) == 0x00800123);

			try {
				yield machine.translate_address (0x00003000, null);
				assert_not_reached ();
			} catch (Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void protect_pages_updates_legacy_entries (Harness h) {
		var target = new FakeTarget (IA32, legacy_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			yield machine.protect_pages (0x00002000, 4096, READ | WRITE, null);

			assert_true (target.read_uint32 (PT_PA + (2 * 4)) == 0x00102003);

			assert_true (target.read_uint32 (PT_PA + (0 * 4)) == 0x00100003);
			assert_true (target.read_uint32 (PD_PA + (0 * 4)) == (PT_PA | 0x3));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void protect_pages_widens_pae_parents (Harness h) {
		var target = new FakeTarget (IA32, pae_page_tables (), PAE_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			yield machine.protect_pages (0x00000000, 4096, READ | EXECUTE, null);

			assert_true (target.read_uint64 (PAE_PT_PA + (0 * 8)) == 0x00100001);

			// The levels above already grant what was asked for, so they must be left alone.
			assert_true (target.read_uint64 (PAE_PD_PA + (0 * 8)) == (PAE_PT_PA | 0x3));
			assert_true (target.read_uint64 (PAE_PDPT_PA + (0 * 8)) == (PAE_PD_PA | 0x1));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void protect_pages_rejects_large_pages (Harness h) {
		var target = new FakeTarget (IA32, legacy_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			try {
				yield machine.protect_pages (0x00400000, 4096, READ | WRITE, null);
				assert_not_reached ();
			} catch (Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}

			assert_true (target.read_uint32 (PD_PA + (1 * 4)) == 0x00800081);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void relocations_apply_load_bias (Harness h) {
		var target = new FakeTarget (IA32, legacy_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			uint64 base_va = 0xc0010000;

			var image = target.client.make_buffer (new Bytes (new uint8[12]));
			image.write_uint32 (0, 0x00000040);
			image.write_uint32 (4, 0x00000080);
			image.write_uint32 (8, 0x11223344);

			machine.apply_relocation (make_relocation (Gum.ElfIA32Relocation.@32, 0), base_va, image);
			machine.apply_relocation (make_relocation (Gum.ElfIA32Relocation.RELATIVE, 4), base_va, image);

			machine.apply_relocation (make_relocation (Gum.ElfIA32Relocation.PC32, 8), base_va, image);

			assert_true (image.read_uint32 (0) == 0xc0010040);
			assert_true (image.read_uint32 (4) == 0xc0010080);
			assert_true (image.read_uint32 (8) == 0x11223344);

			try {
				machine.apply_relocation (make_relocation (Gum.ElfIA32Relocation.TLS_DESC, 0), base_va, image);
				assert_not_reached ();
			} catch (Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void allocate_pages_spans_leaf_tables (Harness h) {
		var target = new FakeTarget (IA32, adjacent_leaf_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var physical_addresses = new Gee.ArrayList<uint64?> ();
			for (uint i = 0; i != 3; i++)
				physical_addresses.add (0x00200000 + (i * 4096));

			var allocation = yield machine.allocate_pages (physical_addresses, null);

			// The last two of the first table, then the first of the second.
			uint64 first_free = SPAN_BASE_VA
				+ ((uint64) (SPAN_ENTRIES_PER_TABLE - SPAN_FREE_IN_FIRST) * 4096);
			assert_true (allocation.virtual_address == first_free);
			assert_true (allocation.size == 3 * 4096);

			uint slot = SPAN_ENTRIES_PER_TABLE - SPAN_FREE_IN_FIRST;
			assert_true (target.read_uint32 (SPAN_PT0_PA + (slot * 4)) == 0x00200003);
			assert_true (target.read_uint32 (SPAN_PT0_PA + ((slot + 1) * 4)) == 0x00201003);
			assert_true (target.read_uint32 (SPAN_PT1_PA + (0 * 4)) == 0x00202003);

			yield allocation.deallocate (null);

			assert_true (target.read_uint32 (SPAN_PT0_PA + (slot * 4)) == 0);
			assert_true (target.read_uint32 (SPAN_PT1_PA + (0 * 4)) == 0);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void protect_pages_spans_leaf_tables (Harness h) {
		var target = new FakeTarget (IA32, adjacent_leaf_tables (), LEGACY_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			// The last mapped page of the first table, and the first page of the second.
			uint last_slot = SPAN_ENTRIES_PER_TABLE - SPAN_FREE_IN_FIRST - 1;
			uint64 start_va = SPAN_BASE_VA + ((uint64) last_slot * 4096);
			yield machine.protect_pages (start_va, 4096, READ, null);

			uint32 entry = target.read_uint32 (SPAN_PT0_PA + (last_slot * 4));
			assert_true ((entry & 0x2) == 0);
			assert_true ((entry & 0x1) != 0);

			assert_true ((target.read_uint32 (SPAN_PT0_PA + ((last_slot - 1) * 4)) & 0x2) != 0);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void enumerate_ranges_walks_long_mode_tables (Harness h) {
		var target = new FakeTarget (X64, long_mode_page_tables (), X64_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.X64Machine (target.client);

			var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);

			assert_true (ranges.size == 4);

			assert_range (ranges[0], 0x00000000, 0x00100000, 0x1000, READ | WRITE);
			assert_range (ranges[1], 0x00001000, 0x00101000, 0x1000, READ | WRITE | EXECUTE);
			assert_range (ranges[2], 0x00200000, 0x140000000, 0x200000, READ);

			// A 1 GiB mapping in the kernel half, which only comes out right if the walker
			// sign-extends what it composes from the indices.
			assert_range (ranges[3], 0xffffc00000000000, 0x40000000, 0x40000000,
				READ | WRITE | EXECUTE);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void x64_translate_address_resolves_leaf_mappings (Harness h) {
		var target = new FakeTarget (X64, long_mode_page_tables (), X64_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.X64Machine (target.client);

			assert_true ((yield machine.translate_address (0x00001010, null)) == 0x00101010);
			assert_true ((yield machine.translate_address (0x00200123, null)) == 0x140000123);
			assert_true ((yield machine.translate_address (0xffffc00000000123, null)) == 0x40000123);

			try {
				yield machine.translate_address (0x00003000, null);
				assert_not_reached ();
			} catch (Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void protect_pages_updates_long_mode_entries (Harness h) {
		var target = new FakeTarget (X64, long_mode_page_tables (), X64_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.X64Machine (target.client);

			yield machine.protect_pages (0x00000000, 4096, READ | EXECUTE, null);

			assert_true (target.read_uint64 (X64_PT_PA + (0 * 8)) == 0x00100001);

			// The levels above already grant what was asked for, so they must be left alone.
			assert_true (target.read_uint64 (X64_PD_PA + (0 * 8)) == (X64_PT_PA | 0x3));
			assert_true (target.read_uint64 (X64_PML4_PA + (0 * 8)) == (X64_PDPT_PA | 0x3));

			yield machine.protect_pages (0x00001000, 4096, READ, null);
			assert_true (target.read_uint64 (X64_PT_PA + (1 * 8)) == (0x00101001 | (1ULL << 63)));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void x64_protect_pages_copes_with_large_pages (Harness h) {
		var target = new FakeTarget (X64, long_mode_page_tables (), X64_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.X64Machine (target.client);

			// This gigabyte page is already writable, thus there is nothing to do. Do not make it more
			// restrictive, because the kernel put other data in the same mapping.
			yield machine.protect_pages (0xffffc00000000000, 4096, READ | WRITE, null);
			assert_true (target.read_uint64 (X64_KERNEL_PDPT_PA + (0 * 8)) == 0x400000e3);

			// This range is read-only, and you cannot make one part of it writable.
			try {
				yield machine.protect_pages (0x200000, 4096, READ | WRITE, null);
				assert_not_reached ();
			} catch (Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}

			assert_true (target.read_uint64 (X64_PD_PA + (1 * 8)) == 0x8000000140000081);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void x64_relocations_apply_load_bias (Harness h) {
		var target = new FakeTarget (X64, long_mode_page_tables (), X64_MONITOR_DUMP);
		try {
			yield target.open ();
			var machine = new Barebone.X64Machine (target.client);

			uint64 base_va = 0xffffffff81000000;

			var image = target.client.make_buffer (new Bytes (new uint8[20]));
			image.write_uint64 (0, 0x40);
			image.write_uint64 (8, 0x80);
			image.write_uint32 (16, 0x11223344);

			machine.apply_relocation (make_relocation (Gum.ElfX64Relocation.@64, 0), base_va, image);
			machine.apply_relocation (make_relocation (Gum.ElfX64Relocation.RELATIVE, 8, 0x80), base_va, image);
			machine.apply_relocation (make_relocation (Gum.ElfX64Relocation.PC32, 16), base_va, image);

			assert_true (image.read_uint64 (0) == 0xffffffff81000040);
			assert_true (image.read_uint64 (8) == 0xffffffff81000080);
			assert_true (image.read_uint32 (16) == 0x11223344);

			try {
				machine.apply_relocation (make_relocation (Gum.ElfX64Relocation.TLSGD, 0), base_va, image);
				assert_not_reached ();
			} catch (Error e) {
				assert_true (e is Error.NOT_SUPPORTED);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	/**
	 * The fake-stub tests above pin down the bit-level behaviour; this one checks the walker
	 * against a real MMU, using a stock Linux guest whose page tables QEMU can describe to us
	 * independently. The guest is non-PAE with PSE, so it covers the legacy two-level walk and
	 * 4 MiB pages; PAE and NX remain the fake stub's job.
	 */
	namespace QEMU {
		private async void walk_finds_the_arm_kernel (SlowHarness h) {
			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (ARM);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (ARM);
				assert_true (guest != null);

				var machine = new Barebone.ArmMachine (guest.client);

				var ranges = yield collect_ranges (machine, Gum.PageProtection.READ);
				assert_true (ranges.size != 0);

				uint64 pc = yield guest.client.exception.thread.read_register ("pc", null);
				Barebone.RangeDetails? parked_in = find_range_containing (ranges, pc);
				assert_true (parked_in != null);
				assert_true ((parked_in.protection & Gum.PageProtection.EXECUTE) != 0);

				foreach (Barebone.RangeDetails r in ranges) {
					assert_true (r.size != 0);
					assert_true (r.size % 4096 == 0);
					assert_true (r.base_va % 4096 == 0);
					assert_true (r.base_pa % 4096 == 0);
				}
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private async void walk_matches_x86_guest (SlowHarness h) {
			yield walk_matches_guest (h, X86);
		}

		private async void walk_matches_x86_64_guest (SlowHarness h) {
			yield walk_matches_guest (h, X86_64);
		}

		private async void walk_matches_guest (SlowHarness h, GuestArch arch) {
			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (arch);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (arch);
				assert_true (guest != null);

				Barebone.Machine machine = (arch == X86)
					? (Barebone.Machine) new Barebone.IA32Machine (guest.client)
					: (Barebone.Machine) new Barebone.X64Machine (guest.client);

				var ours = yield collect_ranges (machine, Gum.PageProtection.READ);
				assert_true (ours.size != 0);

				Gee.List<Interval> mapped_by_guest = yield guest.query_mapped_intervals ();
				assert_intervals_equal (merge_by_virtual_address (ours), mapped_by_guest);

				Gee.List<GuestPage> pages = yield guest.query_pages ();
				assert_true (pages.size != 0);

				bool saw_large_page = false;
				foreach (GuestPage page in pages) {
					Barebone.RangeDetails? r = find_range_containing (ours, page.va);
					assert_true (r != null);

					assert_true (r.virtual_to_physical (page.va) == page.pa);

					// QEMU reports the leaf entry's own bits, while the walker ANDs in every level
					// above it, so the walker's rights can only ever be the narrower of the two.
					if ((r.protection & Gum.PageProtection.WRITE) != 0)
						assert_true (page.writable);
					if ((r.protection & Gum.PageProtection.EXECUTE) != 0)
						assert_true (!page.no_execute);

					if (page.large) {
						assert_true (r.size >= 0x200000);
						saw_large_page = true;
					}
				}

				assert_true (saw_large_page);

				yield check_protect_pages_takes_effect (machine, guest, pages);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private async void allocate_pages_maps_into_x86_guest (SlowHarness h) {
			yield allocate_pages_maps_into_guest (h, X86);
		}

		private async void allocate_pages_maps_into_x86_64_guest (SlowHarness h) {
			yield allocate_pages_maps_into_guest (h, X86_64);
		}

		private async void allocate_pages_maps_into_guest (SlowHarness h, GuestArch arch) {
			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (arch);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (arch);

				Barebone.Machine machine = make_machine (arch, guest);

				Gee.List<GuestPage> pages = yield guest.query_pages ();
				assert_true (pages.size != 0);

				uint64 first_pa = pages[0].pa;
				var physical_addresses = new Gee.ArrayList<uint64?> ();
				physical_addresses.add (first_pa);
				physical_addresses.add (first_pa + 4096);

				Barebone.Allocation allocation = yield machine.allocate_pages (physical_addresses, null);
				uint64 va = allocation.virtual_address;
				assert_true (va != 0);
				assert_true (allocation.size == 2 * 4096);

				GuestPage first = yield guest.query_page (va);
				assert_true (first.pa == first_pa);
				assert_true (first.writable);
				assert_true (!first.no_execute);

				GuestPage second = yield guest.query_page (va + 4096);
				assert_true (second.pa == first_pa + 4096);

				yield allocation.deallocate (null);
				foreach (GuestPage page in yield guest.query_pages ())
					assert_true (page.va != va);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private async void invoke_calls_into_x86_guest (SlowHarness h) {
			yield invoke_calls_into_guest (h, X86);
		}

		private async void invoke_calls_into_x86_64_guest (SlowHarness h) {
			yield invoke_calls_into_guest (h, X86_64);
		}

		private async void invoke_calls_into_guest (SlowHarness h, GuestArch arch) {
			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (arch);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (arch);

				Barebone.Machine machine = make_machine (arch, guest);

				uint64 scratch_pa = ((uint64) QemuGuest.MEMORY_SIZE_IN_MB << 20) - (1024 * 1024);

				var physical_addresses = new Gee.ArrayList<uint64?> ();
				physical_addresses.add (scratch_pa);

				Barebone.Allocation allocation = yield machine.allocate_pages (physical_addresses, null);
				uint64 va = allocation.virtual_address;

				uint8[] sum_of_two_args;
				if (arch == X86) {
					sum_of_two_args = {
						0x8b, 0x44, 0x24, 0x04,	// mov eax, [esp+4]
						0x03, 0x44, 0x24, 0x08,	// add eax, [esp+8]
						0xc3			// ret
					};
				} else {
					sum_of_two_args = {
						0x48, 0x89, 0xf8,	// mov rax, rdi
						0x48, 0x01, 0xf0,	// add rax, rsi
						0xc3			// ret
					};
				}

				Buffer displaced = yield guest.client.read_buffer (va, sum_of_two_args.length, null);
				yield guest.client.write_byte_array (va, new Bytes (sum_of_two_args), null);

				uint64[] args = { 40, 2 };
				assert_true ((yield machine.invoke (va, args, null)) == 42);

				yield guest.client.write_byte_array (va, displaced.bytes, null);
				yield allocation.deallocate (null);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private async void inline_hook_fires_in_x86_guest (SlowHarness h) {
			yield inline_hook_fires_in_guest (h, X86);
		}

		private async void inline_hook_fires_in_x86_64_guest (SlowHarness h) {
			yield inline_hook_fires_in_guest (h, X86_64);
		}

		private async void inline_hook_fires_in_guest (SlowHarness h, GuestArch arch) {
			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (arch);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (arch);

				Barebone.Machine machine = make_machine (arch, guest);
				Barebone.Allocator allocator = make_scratch_allocator (machine);

				Barebone.Allocation target = yield allocator.allocate (4096, 4096, null);
				Barebone.Allocation handler = yield allocator.allocate (4096, 4096, null);
				Barebone.Allocation marker = yield allocator.allocate (4096, 4096, null);

				uint8[] answer_forty_two = {
					0xb8, 0x2a, 0x00, 0x00, 0x00,	// mov eax, 42
					0xc3				// ret
				};
				yield guest.client.write_byte_array (target.virtual_address, new Bytes (answer_forty_two), null);

				var hb = guest.client.make_buffer_builder ();
				if (arch == X86) {
					hb
						.append_uint8 (0xc7).append_uint8 (0x05)	// mov dword [marker], ...
						.append_uint32 ((uint32) marker.virtual_address)
						.append_uint32 (MARKER_VALUE)
						.append_uint8 (0xc3);				// ret
				} else {
					hb
						.append_uint8 (0x48).append_uint8 (0xb8)	// mov rax, marker
						.append_uint64 (marker.virtual_address)
						.append_uint8 (0xc7).append_uint8 (0x00)	// mov dword [rax], ...
						.append_uint32 (MARKER_VALUE)
						.append_uint8 (0xc3);				// ret
				}
				yield guest.client.write_byte_array (handler.virtual_address, hb.build (), null);

				Barebone.InlineHook hook = yield machine.create_inline_hook (target.virtual_address,
					handler.virtual_address, allocator, null);

				yield clear_marker (guest, marker.virtual_address);
				assert_true ((yield machine.invoke (target.virtual_address, {}, null)) == 42);
				assert_true ((yield read_marker (guest, marker.virtual_address)) == 0);

				yield hook.enable (null);
				assert_true ((yield machine.invoke (target.virtual_address, {}, null)) == 42);
				assert_true ((yield read_marker (guest, marker.virtual_address)) == MARKER_VALUE);

				yield hook.disable (null);
				yield clear_marker (guest, marker.virtual_address);
				assert_true ((yield machine.invoke (target.virtual_address, {}, null)) == 42);
				assert_true ((yield read_marker (guest, marker.virtual_address)) == 0);

				yield hook.destroy (null);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private const uint64 ARM_RAM_BASE = 0x40000000U;

		private const uint32 MARKER_VALUE = 0xdeadbeefU;

		private Barebone.Allocator make_scratch_allocator (Barebone.Machine machine,
				uint mb_below_top = 1, uint64 ram_base = 0) {
			var config = new BarebonePhysicalAllocatorConfig ();
			config.physical_base = new BareboneNonNullMemoryAddress ("scratch",
				ram_base + (((uint64) (QemuGuest.MEMORY_SIZE_IN_MB - mb_below_top)) << 20));
			return new Barebone.PhysicalAllocator (machine, 4096, config);
		}

		private async void clear_marker (QemuGuest guest, uint64 va) throws Error, IOError {
			yield guest.client.write_byte_array (va, new Bytes (new uint8[4]), null);
		}

		private async uint32 read_marker (QemuGuest guest, uint64 va) throws Error, IOError {
			return (yield guest.client.read_buffer (va, 4, null)).read_uint32 (0);
		}

		private async void injected_elf_runs_in_x86_guest (SlowHarness h) {
			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (X86);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (X86);

				Barebone.Machine machine = make_machine (X86, guest);
				Barebone.Allocator allocator = make_scratch_allocator (machine);

				var elf = new Gum.ElfModule.from_file (marker_path ());
				size_t page_size = yield machine.query_page_size (null);
				Barebone.Allocation image = yield Barebone.inject_elf (elf, new Bytes (elf.get_file_data ()),
					page_size, machine, allocator, uploaded => {}, null);
				uint64 base_va = image.virtual_address;

				uint64 start = 0;
				elf.enumerate_symbols (e => {
					if (e.name == "_start")
						start = base_va + e.address;
					return true;
				});
				assert_true (start != 0);

				Barebone.Allocation reported = yield allocator.allocate (8, 4, null);
				yield guest.client.write_byte_array (reported.virtual_address, new Bytes (new uint8[8]), null);

				uint64[] args = { reported.virtual_address, 8 };
				yield machine.invoke (start, args, null);

				Buffer answer = yield guest.client.read_buffer (reported.virtual_address, 8, null);

				uint64 answer_address = answer.read_uint32 (0);
				assert_true (answer_address >= base_va);
				assert_true (answer_address < base_va + image.size);

				assert_true (answer.read_uint32 (4) == MARKER_ANSWER);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private const uint32 MARKER_ANSWER = 0x1234abcdU;

		private string marker_path () {
			return Path.build_filename (TESTS_SRCDIR, "..", "src", "barebone", "helpers", "marker-x86.elf");
		}

		private async void whole_agent_loads_into_arm_guest (SlowHarness h) {
			string? agent_path = Environment.get_variable ("FRIDA_BAREBONE_AGENT_ARM");
			if (agent_path == null) {
				stdout.printf ("<skipping: set FRIDA_BAREBONE_AGENT_ARM to an agent blob> ");
				h.done ();
				return;
			}

			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (ARM);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (ARM);

				var machine = new Barebone.ArmMachine (guest.client);
				Barebone.Allocator allocator = make_scratch_allocator (machine, 32, ARM_RAM_BASE);

				var elf = new Gum.ElfModule.from_file (agent_path);
				size_t page_size = yield machine.query_page_size (null);

				var timer = new Timer ();
				Barebone.Allocation image = yield Barebone.inject_elf (elf, new Bytes (elf.get_file_data ()),
					page_size, machine, allocator, uploaded => {}, null);
				stdout.printf ("<%u KiB in %.1fs> ", (uint) (image.size / 1024), timer.elapsed ());

				uint64 base_va = image.virtual_address;
				assert_true (base_va != 0);
				assert_true (image.size >= elf.mapped_size);

				uint64 start = 0;
				elf.enumerate_symbols (e => {
					if (e.name == "_start")
						start = base_va + e.address;
					return true;
				});
				assert_true (start != 0);

				assert_true ((start & 1) != 0);

				Buffer entry = yield guest.client.read_buffer (start & ~((uint64) 1), 16, null);
				bool entry_populated = false;
				for (uint i = 0; i != 16; i++)
					entry_populated |= entry.read_uint8 (i) != 0;
				assert_true (entry_populated);

				yield guest.client.read_buffer (base_va + image.size - 16, 16, null);
			} catch (GLib.Error e) {
				printerr ("
FAIL: %s
", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private async void whole_agent_loads_into_x86_guest (SlowHarness h) {
			string? agent_path = Environment.get_variable ("FRIDA_BAREBONE_AGENT_X86");
			if (agent_path == null) {
				stdout.printf ("<skipping: set FRIDA_BAREBONE_AGENT_X86 to an agent blob> ");
				h.done ();
				return;
			}

			QemuGuest? guest = null;
			try {
				string? unavailable_reason = yield QemuGuest.check_availability (X86);
				if (unavailable_reason != null) {
					stdout.printf ("<skipping: %s> ", unavailable_reason);
					h.done ();
					return;
				}

				guest = yield QemuGuest.boot (X86);

				Barebone.Machine machine = make_machine (X86, guest);
				Barebone.Allocator allocator = make_scratch_allocator (machine, 32);

				var elf = new Gum.ElfModule.from_file (agent_path);
				size_t page_size = yield machine.query_page_size (null);

				var timer = new Timer ();
				Barebone.Allocation image = yield Barebone.inject_elf (elf, new Bytes (elf.get_file_data ()),
					page_size, machine, allocator, uploaded => {}, null);
				stdout.printf ("<%u KiB in %.1fs> ", (uint) (image.size / 1024), timer.elapsed ());

				uint64 base_va = image.virtual_address;
				assert_true (base_va != 0);
				assert_true (image.size >= elf.mapped_size);

				uint64 start = 0;
				elf.enumerate_symbols (e => {
					if (e.name == "_start")
						start = base_va + e.address;
					return true;
				});
				assert_true (start != 0);

				Buffer entry = yield guest.client.read_buffer (start, 16, null);
				bool entry_populated = false;
				for (uint i = 0; i != 16; i++)
					entry_populated |= entry.read_uint8 (i) != 0;
				assert_true (entry_populated);

				// The far end has to be mapped too, not just the first pages.
				yield guest.client.read_buffer (base_va + image.size - 16, 16, null);
			} catch (GLib.Error e) {
				printerr ("\nFAIL: %s\n", e.message);
				assert_not_reached ();
			} finally {
				if (guest != null)
					guest.stop ();
			}

			h.done ();
		}

		private Barebone.Machine make_machine (GuestArch arch, QemuGuest guest) {
			if (arch == X86)
				return new Barebone.IA32Machine (guest.client);
			return new Barebone.X64Machine (guest.client);
		}

		private async void check_protect_pages_takes_effect (Barebone.Machine machine, QemuGuest guest,
				Gee.List<GuestPage> pages) throws Error, IOError {
			GuestPage? victim = null;
			foreach (GuestPage page in pages) {
				if (!page.writable && !page.large) {
					victim = page;
					break;
				}
			}
			assert_true (victim != null);

			yield machine.protect_pages (victim.va, 4096, READ | WRITE, null);
			assert_true ((yield guest.query_page (victim.va)).writable);

			yield machine.protect_pages (victim.va, 4096, READ, null);
			assert_true (!(yield guest.query_page (victim.va)).writable);
		}
	}

	private async Gee.List<Barebone.RangeDetails> collect_ranges (Barebone.Machine machine,
			Gum.PageProtection prot) throws Error, IOError {
		var result = new Gee.ArrayList<Barebone.RangeDetails> ();
		yield machine.enumerate_ranges (prot, r => {
			result.add (r.clone ());
			return true;
		}, null);
		return result;
	}

	private void assert_range (Barebone.RangeDetails r, uint64 base_va, uint64 base_pa, uint64 size,
			Gum.PageProtection prot) {
		assert_true (r.base_va == base_va);
		assert_true (r.base_pa == base_pa);
		assert_true (r.size == size);
		assert_true (r.protection == prot);
	}

	private Gum.ElfRelocationDetails make_relocation (uint32 type, uint64 address, int64 addend = 0) {
		var r = Gum.ElfRelocationDetails ();
		r.address = address;
		r.type = type;
		r.addend = addend;
		return r;
	}

	private const uint64 PD_PA = 0x1000;
	private const uint64 PT_PA = 0x2000;

	private const string LEGACY_MONITOR_DUMP =
		"EAX=00000000 EBX=00000000 ECX=00000000 EDX=00000000\n" +
		"CR0=8005003b CR2=00000000 CR3=00001000 CR4=00000010\n";

	// Paging on, PSE on, PAE off: 32-bit entries, one of them a 4 MiB page.
	private async void ia32_scans_ranges (Harness h) {
		var target = new FakeTarget (IA32, arena_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			target.map_virtual (ARENA_VA, arena_with_needles ());
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var ranges = new Gee.ArrayList<Gum.MemoryRange?> ();
			ranges.add (Gum.MemoryRange () { base_address = ARENA_VA, size = ARENA_SIZE });

			var exact = yield machine.scan_ranges (ranges, new Barebone.MatchPattern.from_string ("1deadbeef1"),
				10, null);
			assert_true (exact.size == 1);
			assert_true (exact[0] == ARENA_VA + FIRST_NEEDLE_OFFSET);

			// The wildcard must match the byte that is different in the two patterns, and the mask must
			// cover only its high nibble.
			var wildcard = yield machine.scan_ranges (ranges,
				new Barebone.MatchPattern.from_string ("1dea??eef1"), 10, null);
			assert_true (wildcard.size == 2);
			assert_true (wildcard[0] == ARENA_VA + FIRST_NEEDLE_OFFSET);
			assert_true (wildcard[1] == ARENA_VA + SECOND_NEEDLE_OFFSET);

			var masked = yield machine.scan_ranges (ranges,
				new Barebone.MatchPattern.from_string ("1dead0eef1:fffff0ffff"), 10, null);
			assert_true (masked.size == 2);

			var capped = yield machine.scan_ranges (ranges,
				new Barebone.MatchPattern.from_string ("1dea??eef1"), 1, null);
			assert_true (capped.size == 1);

			var missing = yield machine.scan_ranges (ranges,
				new Barebone.MatchPattern.from_string ("0badc0ffee"), 10, null);
			assert_true (missing.is_empty);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private Bytes arena_with_needles () {
		var arena = new uint8[ARENA_SIZE];

		uint8[] first = { 0x1d, 0xea, 0xdb, 0xee, 0xf1 };
		uint8[] second = { 0x1d, 0xea, 0xd0, 0xee, 0xf1 };
		for (uint i = 0; i != first.length; i++) {
			arena[FIRST_NEEDLE_OFFSET + i] = first[i];
			arena[SECOND_NEEDLE_OFFSET + i] = second[i];
		}

		return new Bytes.take ((owned) arena);
	}

	private async void services_resolve_from_descriptor_block (Harness h) {
		var target = new FakeTarget (IA32, arena_page_tables (), LEGACY_MONITOR_DUMP);
		try {
			target.map_virtual (ARENA_VA, arena_with_vmm_block ());
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var layout = yield Barebone.collect_win9x_layout (machine, null);

			var symbols = layout.symbols;
			assert_true (symbols.size == IMPLEMENTED_SERVICES);
			assert_symbol (symbols[0], "Get_VMM_Version", (uint32) 0xc0001000);
			assert_symbol (symbols[1], "Get_Cur_VM_Handle", (uint32) 0xc0001010);
			assert_symbol (symbols[2], "Get_Sys_VM_Handle", (uint32) 0xc0001030);
			assert_symbol (symbols[6], "Begin_Reentrant_Execution", (uint32) 0xc0001070);

			assert_true (layout.modules.size == 1);
			var vmm = layout.modules[0];
			assert_true (vmm.name == "VMM.VXD");
			assert_true (vmm.offset == 0xc0001000);
			assert_true (vmm.size == 0xc0001070 - 0xc0001000);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private async void enumerates_processes_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var options = new ProcessQueryOptions ();
			options.scope = FULL;
			var processes = yield device.enumerate_processes (options, null);

			Process? shell = null;
			for (int i = 0; i != processes.size (); i++) {
				var path = processes.get (i).parameters["path"];
				if (path != null && path.get_string ().down ().has_suffix ("explorer.exe"))
					shell = processes.get (i);
			}
			assert_nonnull (shell);

			// An unobfuscated id would still be the process database pointer, which lives in
			// the shared arena.
			assert_true (shell.pid < 0x80000000 || shell.pid >= 0x83000000);

			for (int i = 0; i != processes.size (); i++)
				assert_true (processes.get (i).name != "");

			assert_true (shell.parameters["path"].get_string ().has_suffix ("EXPLORER.EXE"));

			var argv = shell.parameters["argv"];
			assert_nonnull (argv);
			assert_true (argv.n_children () == 1);
			assert_true (argv.get_child_value (0).get_string ().down ().has_suffix ("explorer.exe"));

			var icons = shell.parameters["icons"];
			assert_nonnull (icons);
			assert_true (icons.n_children () != 0);

			var icon = icons.get_child_value (0);
			assert_true (icon.lookup_value ("format", VariantType.STRING).get_string () == "rgba");
			uint16 width = icon.lookup_value ("width", VariantType.UINT16).get_uint16 ();
			uint16 height = icon.lookup_value ("height", VariantType.UINT16).get_uint16 ();
			assert_true (width != 0 && height != 0);
			assert_true (icon.lookup_value ("image", new VariantType ("ay")).get_size () == width * height * 4);

			// The image gives each size one time, at its best color depth.
			var seen = new Gee.HashSet<uint32> ();
			for (size_t i = 0; i != icons.n_children (); i++) {
				var one = icons.get_child_value (i);
				uint32 shape = ((uint32) one.lookup_value ("width", VariantType.UINT16).get_uint16 () << 16)
					| one.lookup_value ("height", VariantType.UINT16).get_uint16 ();
				assert_false (seen.contains (shape));
				seen.add (shape);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void takes_a_snapshot_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			var session = yield device.attach (pid, null, null);

			var script = yield session.create_script ("""
				const kernel32 = Process.getModuleByName('KERNEL32.DLL');
				const named = (name, ret, args) =>
					new NativeFunction(kernel32.getExportByName(name), ret, args);

				const snapshot = named('CreateToolhelp32Snapshot', 'pointer', ['uint', 'uint']);
				const first = named('Process32First', 'int', ['pointer', 'pointer']);
				const next = named('Process32Next', 'int', ['pointer', 'pointer']);
				const close = named('CloseHandle', 'int', ['pointer']);

				const snap = snapshot(0x2, 0);
				const entry = Memory.alloc(556);
				entry.writeU32(556);

				const names = [];
				for (let more = first(snap, entry); more !== 0; more = next(snap, entry))
					names.push(entry.add(36).readUtf8String().toLowerCase());
				close(snap);

				send(['processes', names.length,
					names.some(name => name.endsWith('explorer.exe'))]);
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();
			assert_true (messages[0].contains ("true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void hooks_its_own_code_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			var session = yield device.attach (pid, null, null);

			var script = yield session.create_script ("""
				const mine = Memory.alloc(Process.pageSize);
				Memory.patchCode(mine, 16, code => {
					const cw = new X86Writer(code, { pc: mine });
					for (let i = 0; i !== 8; i++)
						cw.putNop();
					cw.putRet();
					cw.flush();
				});

				Interceptor.attach(mine, {
					onEnter() {
						send('entered');
					}
				});
				send('hooked');

				recv('call', () => {
					new NativeFunction(mine, 'void', [])();
					send('called');
				});
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();

			script.post ("""{"type":"call"}""");
			while (messages.size < 3)
				yield h.process_events ();
			assert_true (messages[1].contains ("entered"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void hooks_only_its_own_process_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint spawned = yield device.spawn ("C:\\WINDOWS\\NOTEPAD.EXE", null, null);
			var watcher = yield device.attach (spawned, null, null);

			uint other = yield find_program (device, "explorer.exe");
			assert_true (other != 0);
			var bystander = yield device.attach (other, null, null);

			var hooking = yield watcher.create_script ("""
				const kernel32 = Process.getModuleByName('KERNEL32.DLL');
				const named = name => kernel32.getExportByName(name);

				const target = named('GetACP');
				const seen = Memory.alloc(4);
				seen.writeU32(0);
				Interceptor.attach(target, {
					onEnter() {
						seen.writeU32(seen.readU32() + 1);
					}
				});

				const stub = Memory.alloc(Process.pageSize);
				Memory.patchCode(stub, 32, code => {
					const cw = new X86Writer(code, { pc: stub });
					cw.putCallAddress(target);
					cw.putRet();
					cw.flush();
				});
				const createThread = new NativeFunction(named('CreateThread'), 'pointer',
					['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);

				recv('call', () => {
					createThread(NULL, 0, stub, NULL, 0, NULL);
				});
				recv('poll', () => { send(['seen', seen.readU32()]); });
				send('hooked');
			""", null, null);

			var hits = new Gee.ArrayList<string> ();
			hooking.message.connect ((json, data) => {
				hits.add (json);
			});
			yield hooking.load (null);
			while (hits.size < 1)
				yield h.process_events ();

			var calling = yield bystander.create_script ("""
				const call = new NativeFunction(Process.getModuleByName('KERNEL32.DLL')
					.getExportByName('GetACP'), 'uint32', []);

				recv('call', () => {
					for (let i = 0; i !== 100; i++)
						call();
					send('called');
				});
			""", null, null);

			var elsewhere = new Gee.ArrayList<string> ();
			calling.message.connect ((json, data) => {
				elsewhere.add (json);
			});
			yield calling.load (null);

			calling.post ("""{"type":"call"}""");
			while (elsewhere.size < 1)
				yield h.process_events ();
			assert_true (hits.size == 1);

			hooking.post ("""{"type":"call"}""");
			for (uint i = 0; i != 300; i++)
				yield h.process_events ();
			hooking.post ("""{"type":"poll"}""");
			while (hits.size < 2)
				yield h.process_events ();
			assert_true (hits[1].contains ("\"seen\",1"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void hooks_again_after_letting_go_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint spawned = yield device.spawn ("C:\\WINDOWS\\NOTEPAD.EXE", null, null);
			var watcher = yield device.attach (spawned, null, null);

			uint other = yield find_program (device, "explorer.exe");
			assert_true (other != 0);
			var bystander = yield device.attach (other, null, null);

			var early = yield bystander.create_script ("""
				const target = Process.getModuleByName('KERNEL32.DLL').getExportByName('GetACP');
				Interceptor.attach(target, { onEnter() {} });
				send('hooked');
			""", null, null);
			var announced = new Gee.ArrayList<string> ();
			early.message.connect ((json, data) => {
				announced.add (json);
			});
			yield early.load (null);
			while (announced.size < 1)
				yield h.process_events ();
			yield early.unload (null);

			var hooking = yield watcher.create_script ("""
				const kernel32 = Process.getModuleByName('KERNEL32.DLL');
				const named = name => kernel32.getExportByName(name);

				const target = named('GetACP');
				const seen = Memory.alloc(4);
				seen.writeU32(0);
				Interceptor.attach(target, {
					onEnter() {
						seen.writeU32(seen.readU32() + 1);
					}
				});

				const stub = Memory.alloc(Process.pageSize);
				Memory.patchCode(stub, 32, code => {
					const cw = new X86Writer(code, { pc: stub });
					cw.putCallAddress(target);
					cw.putRet();
					cw.flush();
				});
				const createThread = new NativeFunction(named('CreateThread'), 'pointer',
					['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);

				recv('call', () => {
					createThread(NULL, 0, stub, NULL, 0, NULL);
				});
				recv('poll', () => { send(['seen', seen.readU32()]); });
				send('hooked');
			""", null, null);

			var hits = new Gee.ArrayList<string> ();
			hooking.message.connect ((json, data) => {
				hits.add (json);
			});
			yield hooking.load (null);
			while (hits.size < 1)
				yield h.process_events ();

			hooking.post ("""{"type":"call"}""");
			for (uint i = 0; i != 300; i++)
				yield h.process_events ();
			hooking.post ("""{"type":"poll"}""");
			while (hits.size < 2)
				yield h.process_events ();
			assert_true (hits[1].contains ("\"seen\",1"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private static async uint find_program (Device device, string file_name) throws GLib.Error {
		var options = new ProcessQueryOptions ();
		options.scope = METADATA;

		var processes = yield device.enumerate_processes (options, null);
		for (int i = 0; i != processes.size (); i++) {
			var p = processes.get (i);
			var path = p.parameters["path"];
			if (path != null && path.get_string ().down ().has_suffix (file_name))
				return p.pid;
		}

		return 0;
	}

	private async void disassembles_add_ddb_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var session = yield device.attach (0, null, null);
			var script = yield session.create_script ("""
				const out = {};
				for (const name of ['VMM_Add_DDB', 'VMM_Remove_DDB', 'Get_DDB']) {
					const address = DebugSymbol.getFunctionByName(name);
					const lines = [];
					let cursor = address;
					for (let i = 0; i !== 24; i++) {
						const insn = Instruction.parse(cursor);
						lines.push(insn.address.sub(address).toInt32() + ' ' + insn.mnemonic +
							' ' + insn.opStr);
						cursor = insn.next;
						if (insn.mnemonic === 'ret')
							break;
					}
					out[name] = address.toString() + ' | ' + lines.join(' ; ');
				}
				send(out);
			""", null, null);

			string? said = null;
			bool waiting = false;
			var handler = script.message.connect ((json, data) => {
				said = json;
				if (waiting) {
					waiting = false;
					disassembles_add_ddb_in_live_guest.callback ();
				}
			});
			yield script.load (null);
			if (said == null) {
				waiting = true;
				yield;
			}
			script.disconnect (handler);
			printerr ("\nDDB %s\n", said);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void win9x_watches_its_own_threads_in_live_guest (SlowHarness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_program (device, "explorer.exe");
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const seen = [];
				const others = Process.enumerateThreads()
					.filter(t => t.id !== Process.getCurrentThreadId());
				send(['registers', others.length,
					others.every(t => t.context !== undefined && !t.context.pc.isNull())]);
				Process.attachThreadObserver({
					onAdded(thread) {
						seen.push(['added', thread.id.toString(),
							Process.getCurrentThreadId() === thread.id,
							thread.entrypoint === undefined
								? 'nowhere'
								: thread.entrypoint.routine.toString() + '/' +
									thread.entrypoint.parameter.toString()]);
					},
					onRemoved(thread) {
						seen.push(['removed', thread.id.toString()]);
					}
				});

				const k32 = Process.getModuleByName('KERNEL32.DLL');
				const fn = (name, ret, args) =>
					new NativeFunction(k32.getExportByName(name), ret, args, { abi: 'stdcall' });
				const createThread = fn('CreateThread', 'pointer',
					['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);
				const closeHandle = fn('CloseHandle', 'int', ['pointer']);

				const body = Memory.alloc(Process.pageSize);
				Memory.protect(body, Process.pageSize, 'rwx');
				Memory.patchCode(body, 16, code => {
					const w = new X86Writer(code, { pc: body });
					w.putXorRegReg('eax', 'eax');
					w.putRetImm(4);
					w.flush();
				});

				const out = Memory.alloc(4);
				const handle = createThread(NULL, 0, body, ptr('0xdeadbeef'), 0, out);
				const made = out.readU32();
				closeHandle(handle);

				recv('poll', () => {
					const mine = seen.filter(e => e[1] === made.toString());
					send(['saw', mine.some(e => e[0] === 'added' && e[2]),
						mine.some(e => e[0] === 'removed'), seen.length,
						mine.some(e => e[0] === 'added' &&
							e[3] === body.toString() + '/0xdeadbeef')]);
				});
				send('ready');
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();

			var settle = new TimeoutSource.seconds (3);
			settle.set_callback (win9x_watches_its_own_threads_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			script.post ("""{"type":"poll"}""");
			while (said.size < 3)
				yield h.process_events ();
			printerr ("\nREGISTERS %s\nSAW %s\n", said[0], said[said.size - 1]);

			assert_true (said[0].contains ("\"registers\","));
			assert_true (!said[0].contains (",false]"));
			assert_true (said[said.size - 1].contains ("\"saw\",true,true,"));
			assert_true (said[said.size - 1].has_suffix ("true]}"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void enumerates_its_own_threads_in_live_guest (SlowHarness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_program (device, "explorer.exe");
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const mine = Process.enumerateThreads().map(t => t.id >>> 0);
				const here = Process.getCurrentThreadId() >>> 0;
				send(['threads', mine.length, mine.includes(here)]);
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();
			printerr ("\nTHREADS %s\n", said[0]);

			assert_true (said[0].contains ("true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void watches_threads_in_live_guest (SlowHarness h, BareboneConfig? config,
			string program) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var session = yield device.attach (0, null, null);
			var script = yield session.create_script ("""
				const seen = { added: 0, removed: 0 };
				const known = new Set(Process.enumerateThreads().map(t => t.id.toString()));
				Process.attachThreadObserver({
					onAdded(thread) {
						seen.added++;
						const here = Process.getCurrentThreadId();
						send(['added', thread.id.toString(), here.toString(),
							here === thread.id]);
					},
					onRemoved(thread) {
						seen.removed++;
					}
				});
				recv('poll', () => { send(['seen', seen.added, seen.removed, known.size]); });
				send(['ready', known.size]);
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();
			printerr ("\nREADY %s\n", said[0]);

			uint pid = yield device.spawn (program, null, null);
			yield device.resume (pid, null);

			var settle = new TimeoutSource.seconds (5);
			settle.set_callback (watches_threads_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			script.post ("""{"type":"poll"}""");
			while (said.size < 3)
				yield h.process_events ();
			printerr ("\nSEEN %s\n", said[said.size - 1]);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void enumerates_applications_in_live_guest (SlowHarness h, BareboneConfig? config,
			string program, string identity) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			var options = new ApplicationQueryOptions ();
			options.scope = METADATA;
			var applications = yield device.enumerate_applications (options, null);
			var show = new Gee.ArrayList<string> ();
			for (int i = 0; i != applications.size (); i++) {
				var app = applications.get (i);
				var path = app.parameters["path"];
				show.add ("%s [%s] %s".printf (app.identifier, app.name,
					(path != null) ? path.get_string () : "?"));
			}
			printerr ("\nAPPS %s\n", string.joinv (" | ", show.to_array ()));
			assert_true (applications.size () > 0);

			for (int i = 0; i != applications.size (); i++) {
				var app = applications.get (i);
				assert_true (app.identifier != "");
				assert_true (app.parameters["path"] != null);
			}

			string? identifier = null;
			for (int i = 0; i != applications.size (); i++) {
				var app = applications.get (i);
				if (app.name == program)
					identifier = app.identifier;
			}
			assert_nonnull (identifier);
			assert_true (identifier == identity);

			uint pid = yield device.spawn (identifier, null, null);
			assert_true (pid != 0);
			yield device.resume (pid, null);

			var settle = new TimeoutSource.seconds (3);
			settle.set_callback (enumerates_applications_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			bool running = false;
			var again = yield device.enumerate_applications (null, null);
			for (int i = 0; i != again.size (); i++) {
				var app = again.get (i);
				if (app.identifier == identifier && app.pid == pid)
					running = true;
			}
			assert_true (running);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void gates_two_spawns_at_once_in_live_guest (SlowHarness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			var held = new Gee.ArrayList<uint> ();
			device.spawn_added.connect (spawn => {
				held.add (spawn.pid);
			});
			yield device.enable_spawn_gating (null);

			uint helper = yield find_program (device, "explorer.exe");
			var session = yield device.attach (helper, null, null);
			var starter = yield session.create_script ("""
				const kernel32 = Process.getModuleByName('KERNEL32.DLL');
				const named = n => kernel32.getExportByName(n);
				const createProcess = new NativeFunction(named('CreateProcessA'), 'uint32',
					['pointer', 'pointer', 'pointer', 'pointer', 'uint32', 'uint32', 'pointer',
						'pointer', 'pointer', 'pointer'], { scheduling: 'exclusive' });
				const createThread = new NativeFunction(named('CreateThread'), 'pointer',
					['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);

				const start = program => {
					const startup = Memory.alloc(72);
					startup.writeU32(72);
					const information = Memory.alloc(16);
					createProcess(NULL, Memory.allocUtf8String(program), NULL, NULL, 0, 0, NULL,
						NULL, startup, information);
				};

				const gate = Memory.alloc(4);
				gate.writeU32(0);
				const second = new NativeCallback(() => {
					while (gate.readU32() === 0)
						;
					start('C:\\WINDOWS\\NOTEPAD.EXE');
					return 0;
				}, 'uint32', ['pointer']);

				createThread(NULL, 0, second, NULL, 0, NULL);
				gate.writeU32(1);
				start('C:\\WINDOWS\\NOTEPAD.EXE');
				send('started');
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			starter.message.connect ((json, data) => { said.add (json); });
			yield starter.load (null);
			while (said.is_empty)
				yield h.process_events ();

			for (uint i = 0; i != 600 && held.size < 2; i++)
				yield h.process_events ();
			printerr ("\nHELD %u programs\n", held.size);
			assert_true (held.size == 2);

			yield device.resume (held[0], null);

			var pending = yield device.enumerate_pending_spawn (null);
			assert_true (pending.size () == 1);
			assert_true (pending.get (0).pid == held[1]);

			var waiting = yield where_it_waits (h, device, held[1]);
			printerr ("\nSTILL-HELD %s\n", waiting);
			assert_true (waiting.contains ("true"));

			yield device.resume (held[1], null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async string where_it_waits (Frida.Test.AsyncHarness h, Device device, uint pid)
			throws GLib.Error {
		var session = yield device.attach (pid, null, null);
		var script = yield session.create_script ("""
			const image = Process.enumerateModules()[0];
			const headers = image.base.add(image.base.add(0x3c).readU32());
			const entry = image.base.add(headers.add(0x28).readU32());
			const waiting = Process.enumerateThreads()
				.some(t => t.context.pc.equals(entry));
			send([image.name, entry.toString(), waiting]);
		""", null, null);

		var said = new Gee.ArrayList<string> ();
		script.message.connect ((json, data) => { said.add (json); });
		yield script.load (null);
		while (said.is_empty)
			yield h.process_events ();

		return said[0];
	}

	private async void gates_spawns_in_live_guest (SlowHarness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			var spawned = new Gee.ArrayList<string> ();
			uint held_pid = 0;
			device.spawn_added.connect (spawn => {
				spawned.add (spawn.identifier);
				held_pid = spawn.pid;
			});
			yield device.enable_spawn_gating (null);

			uint helper = yield find_program (device, "explorer.exe");
			var session = yield device.attach (helper, null, null);
			var starter = yield session.create_script ("""
				const kernel32 = Process.getModuleByName('KERNEL32.DLL');
				const createProcess = new NativeFunction(kernel32.getExportByName('CreateProcessA'),
					'uint32', ['pointer', 'pointer', 'pointer', 'pointer', 'uint32', 'uint32',
						'pointer', 'pointer', 'pointer', 'pointer']);
				const startup = Memory.alloc(72);
				startup.writeU32(72);
				const information = Memory.alloc(16);
				const ok = createProcess(NULL, Memory.allocUtf8String('C:\\WINDOWS\\NOTEPAD.EXE'),
					NULL, NULL, 0, 0, NULL, NULL, startup, information);
				send(['started', ok, information.add(8).readU32()]);
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			starter.message.connect ((json, data) => { said.add (json); });
			yield starter.load (null);
			while (said.is_empty)
				yield h.process_events ();

			while (spawned.is_empty)
				yield h.process_events ();
			assert_true (spawned[0].down ().has_suffix ("notepad.exe"));
			assert_true (held_pid != 0);

			var pending = yield device.enumerate_pending_spawn (null);
			assert_true (pending.size () == 1);
			assert_true (pending.get (0).pid == held_pid);

			yield device.resume (held_pid, null);

			assert_true ((yield device.enumerate_pending_spawn (null)).size () == 0);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void spawns_a_16_bit_program_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield device.spawn ("C:\\WINDOWS\\SOL.EXE", null, null);
			assert_true (pid != 0);

			var settle = new TimeoutSource.seconds (3);
			settle.set_callback (spawns_a_16_bit_program_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			assert_true ((yield find_program (device, "sol.exe")) == pid);

			yield device.resume (pid, null);

			var running = new TimeoutSource.seconds (2);
			running.set_callback (spawns_a_16_bit_program_in_live_guest.callback);
			running.attach (MainContext.get_thread_default ());
			yield;
			running.destroy ();

			assert_true ((yield find_program (device, "sol.exe")) == pid);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void names_a_16_bit_process_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint helper = yield find_program (device, "explorer.exe");
			assert_true (helper != 0);

			var session = yield device.attach (helper, null, null);
			var starter = yield session.create_script ("""
				const winExec = new NativeFunction(Process.getModuleByName('KERNEL32.DLL')
					.getExportByName('WinExec'), 'uint32', ['pointer', 'uint32']);
				send(['started', winExec(Memory.allocUtf8String('C:\\WINDOWS\\SOL.EXE'), 1)]);
			""", null, null);

			var announced = new Gee.ArrayList<string> ();
			starter.message.connect ((json, data) => {
				announced.add (json);
			});
			yield starter.load (null);
			while (announced.size < 1)
				yield h.process_events ();

			var settle = new TimeoutSource.seconds (3);
			settle.set_callback (names_a_16_bit_process_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			var options = new ProcessQueryOptions ();
			options.scope = METADATA;

			Process? game = null;
			var list = yield device.enumerate_processes (options, null);
			for (int i = 0; i != list.size (); i++) {
				var path = list.get (i).parameters["path"];
				if (path != null && path.get_string ().down ().has_suffix ("sol.exe"))
					game = list.get (i);
			}
			assert_nonnull (game);

			assert_true (game.name.down ().contains ("solitaire"));
			assert_true (game.parameters["path"].get_string ().down ().has_suffix ("sol.exe"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void takes_a_big_script_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			assert_true (pid != 0);
			var session = yield device.attach (pid, null, null);

			var sizes = new uint[] { 4096, 24576, 65536 };
			foreach (var size in sizes) {
				var padding = new StringBuilder ();
				while (padding.len < size)
					padding.append ("0123456789abcdef");

				var source = "const padding = '" + padding.str + "';\n" +
					"send(['size', padding.length]);";

				printerr ("\nBIG trying %u\n", size);
				var script = yield session.create_script (source, null, null);

				string? received = null;
				bool waiting = false;
				var handler = script.message.connect ((json, data) => {
					received = json;
					if (waiting) {
						waiting = false;
						takes_a_big_script_in_live_guest.callback ();
					}
				});
				yield script.load (null);
				if (received == null) {
					waiting = true;
					yield;
				}
				script.disconnect (handler);
				yield script.unload (null);

				printerr ("\nBIG ok %u -> %s\n", size, received);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void injects_into_process_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			assert_true (pid != 0);

			// Success shows that the injected agent started and reported this process.
			var session = yield device.attach (pid, null, null);
			assert_nonnull (session);

			// The script runs in the target process, thus Process.id gives the id of the target.
			var script = yield session.create_script ("""
				recv('ping', () => { send(Process.id >>> 0); });
				send('ready');
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();
			assert_true (messages[0].contains ("ready"));

			// The script answers only if the message arrives, thus this tests the reverse direction.
			script.post ("""{"type":"ping"}""");
			while (messages.size < 2)
				yield h.process_events ();
			assert_true (messages[1].contains (pid.to_string ()));

			try {
				yield device.attach (pid ^ 0x1234, null, null);
				assert_not_reached ();
			} catch (GLib.Error e) {
			}

		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void answers_at_a_steady_pace_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null) {
			h.done ();
			return;
		}

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			yield measure_the_pace (h, yield device.attach (0, null, null), "kernel");

			uint pid = yield find_program (device, "explorer.exe");
			assert_true (pid != 0);
			yield measure_the_pace (h, yield device.attach (pid, null, null), "explorer.exe");
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void measure_the_pace (Harness h, Session session, string where) throws GLib.Error {
		var script = yield session.create_script ("""
			function ask () {
				recv('ask', () => {
					send(Process.arch);
					ask();
				});
			}
			ask();
			send('ready');
		""", null, null);

		uint received = 0;
		bool waiting = false;
		script.message.connect ((json, data) => {
			received++;
			if (waiting) {
				waiting = false;
				measure_the_pace.callback ();
			}
		});

		yield script.load (null);
		while (received < 1) {
			waiting = true;
			yield;
		}

		var samples = new int64[ROUND_TRIPS];
		for (uint i = 0; i != ROUND_TRIPS; i++) {
			uint expected = received + 1;
			int64 started = get_monotonic_time ();
			script.post ("""{"type":"ask"}""");
			while (received < expected) {
				waiting = true;
				yield;
			}
			samples[i] = get_monotonic_time () - started;
		}

		var sorted = new Gee.ArrayList<int64?> ();
		foreach (int64 sample in samples)
			sorted.add (sample);
		sorted.sort ((a, b) => (a < b) ? -1 : ((a > b) ? 1 : 0));

		int64 quickest = sorted[0];
		int64 middle = sorted[sorted.size / 2];
		int64 slowest = sorted[sorted.size - 1];
		printerr ("\nPACE in %s over %u round trips: quickest %" + int64.FORMAT + " us, median %"
				+ int64.FORMAT + " us, slowest %" + int64.FORMAT + " us, jitter %"
				+ int64.FORMAT + " us\n\n",
			where, ROUND_TRIPS, quickest, middle, slowest, slowest - quickest);

		assert_true (middle < PACE_LIMIT_US);
		assert_true (slowest < PACE_LIMIT_US * 4);
	}

	private const uint ROUND_TRIPS = 20;
	private const int64 PACE_LIMIT_US = 250000;

	private async void agent_runs_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, win9x_config_from_environment (h), "send(1 + 1);", "\"payload\":2");
	}

	// This test injects nothing. The agent is 32-bit, and the test examines only the host, which
	// must read a kernel of twice the width.
	private async void winnt_maps_out_a_64_bit_kernel_in_live_guest (Harness h) {
		string? port = Environment.get_variable ("FRIDA_TEST_WINNT64_GDB_PORT");
		if (port == null) {
			h.done ();
			return;
		}

		try {
			var client = new SocketClient ();
			var connection = yield client.connect_to_host_async ("127.0.0.1", (uint16) uint.parse (port), null);
			var gdb = yield GDB.Client.open (connection, null);
			assert_true (gdb.pointer_size == 8);

			var machine = new Barebone.X64Machine (gdb);
			var layout = yield Barebone.collect_winnt_layout (machine, null);

			Barebone.ModuleInfo? kernel = null;
			foreach (var m in layout.modules) {
				if (m.name.down () == "ntoskrnl.exe")
					kernel = m;
			}
			assert_nonnull (kernel);

			// All of these addresses are above the limit of a 32-bit kernel.
			assert_true (kernel.offset > uint32.MAX);
			assert_true (layout.modules.size > 20);

			bool named = false;
			bool wide = true;
			foreach (var sym in layout.symbols) {
				if (sym.name == "ExAllocatePoolWithTag")
					named = true;
				if (sym.offset <= uint32.MAX)
					wide = false;
			}
			assert_true (named);
			assert_true (wide);

			yield gdb.close (null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		}

		h.done ();
	}

	private async void winnt_maps_out_an_arm64_kernel_in_live_guest (Harness h) {
		string? port = Environment.get_variable ("FRIDA_TEST_WINNT_ARM64_GDB_PORT");
		if (port == null) {
			h.done ();
			return;
		}

		try {
			var client = new SocketClient ();
			var connection = yield client.connect_to_host_async ("127.0.0.1", (uint16) uint.parse (port), null);
			var gdb = yield Barebone.ParallelsStubClient.open (connection, null);
			assert_true (gdb.arch == GDB.TargetArch.ARM64);
			assert_true (gdb.pointer_size == 8);

			var machine = new Barebone.Arm64Machine (gdb);
			var layout = yield Barebone.collect_winnt_layout (machine, null);

			Barebone.ModuleInfo? kernel = null;
			foreach (var m in layout.modules) {
				if (m.name.down () == "ntoskrnl.exe")
					kernel = m;
			}
			assert_nonnull (kernel);

			assert_true (kernel.offset > uint32.MAX);
			assert_true (layout.modules.size > 20);

			bool named = false;
			bool process_list = false;
			foreach (var sym in layout.symbols) {
				if (sym.name == "KeBugCheckEx")
					named = true;
				if (sym.name == Barebone.PROCESS_LIST_HEAD)
					process_list = true;
			}
			assert_true (named);
			assert_true (process_list);

			yield gdb.close (null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		}

		h.done ();
	}

	private async void winnt_arm64_agent_runs_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, parallels_config_from_environment (h, "WINNT_ARM64"),
			"send(1 + 1);", "\"payload\":2");
	}

	private async void winnt_arm64_fires_a_timer_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, parallels_config_from_environment (h, "WINNT_ARM64"), """
			const before = Date.now();
			setTimeout(() => { send({ late: Date.now() - before >= 2000 }); }, 3000);
		""", "\"late\":true");
	}

	private async void winnt_arm64_enumerates_threads_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, parallels_config_from_environment (h, "WINNT_ARM64"), """
			const threads = Process.enumerateThreads();
			const sane = threads.length > 0 && threads.every(t => t.id !== 0);
			send({ sane });
		""", "\"sane\":true");
	}

	private async void winnt_arm64_agent_recovers_from_exception_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, parallels_config_from_environment (h, "WINNT_ARM64"), """
			let caught = 'no';
			try {
				ptr('0xfffff000').readU32();
			} catch (e) {
				caught = 'yes';
			}
			send({ caught: caught });
		""", "\"caught\":\"yes\"");
	}

	private async void winnt_arm64_sees_target_modules_and_threads_in_live_guest (SlowHarness h) {
		var config = parallels_config_from_environment (h, "WINNT_ARM64");
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const mods = Process.enumerateModules();
				const ntdll = mods.find(m => m.name.toLowerCase() === 'ntdll.dll');
				const named = ntdll.enumerateExports().some(e => e.name === 'NtClose');
				send({ modules: mods.length > 1, exports: named });

				const threads = Process.enumerateThreads();
				send({ threads: threads.length > 0 && threads.every(t => t.id !== 0) });
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 2)
				yield h.process_events ();
			yield session.detach (null);

			assert_true (messages[0].contains ("\"modules\":true"));
			assert_true (messages[0].contains ("\"exports\":true"));
			assert_true (messages[1].contains ("\"threads\":true"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_arm64_injects_into_process_in_live_guest (SlowHarness h) {
		var config = parallels_config_from_environment (h, "WINNT_ARM64");
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			assert_nonnull (session);

			var script = yield session.create_script ("""
				recv('ping', () => { send(Process.id >>> 0); });
				send('ready');
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();
			assert_true (messages[0].contains ("ready"));

			script.post ("""{"type":"ping"}""");
			while (messages.size < 2)
				yield h.process_events ();
			assert_true (messages[1].contains (pid.to_string ()));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_arm64_hooks_kernel_function_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, parallels_config_from_environment (h, "WINNT_ARM64"), """
			const kernel = Process.enumerateModules().find(m => m.name === 'ntoskrnl.exe');
			const upper = kernel.enumerateExports().find(e => e.name === 'RtlUpperChar').address;

			let seen = null;
			Interceptor.attach(upper, {
				onEnter(args) {
					seen = args[0].toInt32();
				}
			});
			Interceptor.flush();

			const call = new NativeFunction(upper, 'uint8', ['uint8']);
			const result = call(0x61);
			send({ seen: seen, result: result });
		""", "\"seen\":97,\"result\":65");
	}

	private async void winnt_arm64_keeps_kernel_hook_in_live_guest (SlowHarness h) {
		yield run_script_in_live_guest (h, parallels_config_from_environment (h, "WINNT_ARM64"), """
			const kernel = Process.enumerateModules().find(m => m.name === 'ntoskrnl.exe');
			const upper = kernel.enumerateExports().find(e => e.name === 'RtlUpperChar').address;

			let seen = null;
			Interceptor.attach(upper, {
				onEnter(args) {
					seen = args[0].toInt32();
				}
			});
			Interceptor.flush();

			const call = new NativeFunction(upper, 'uint8', ['uint8']);
			setTimeout(() => {
				const result = call(0x61);
				send({ seen: seen, result: result });
			}, 180000);
		""", "\"seen\":97,\"result\":65");
	}

	private async void linux_agent_runs_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, linux_config_from_environment (h, prefix), "send(1 + 1);",
			"\"payload\":2");
	}

	private async void linux_rpc_replies_in_live_guest (Harness h, string prefix) {
		yield call_rpc_in_live_guest (h, linux_config_from_environment (h, prefix));
	}

	private async void linux_rpc_replies_in_process_in_live_guest (Harness h, string prefix) {
		var config = linux_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			string target = Environment.get_variable (@"FRIDA_TEST_$(prefix)_REPL_TARGET") ?? "app_process64";
			uint pid = yield find_program (device, target);
			if (pid == 0)
				printerr ("\nNo %s among the guest's processes\n", target);
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script (repl_agent_source (prefix), null, null);

			string? received = null;
			bool waiting = false;
			var handler = script.message.connect ((json, data) => {
				if (!json.contains ("frida:rpc"))
					return;
				received = json;
				if (waiting) {
					waiting = false;
					linux_rpc_replies_in_process_in_live_guest.callback ();
				}
			});
			yield script.load (null);

			script.post ("""["frida:rpc",1,"call","evaluate",["1 + 1",{"raw":false}]]""");

			if (received == null) {
				waiting = true;
				yield;
			}
			script.disconnect (handler);

			if (received == null || !received.contains ("\"ok\""))
				printerr ("\nexpected an ok reply in: %s\n", (received != null) ? received : "(froze -- no reply)");
			assert_true (received != null && received.contains ("\"ok\""));

			yield session.detach (null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private string repl_agent_source (string prefix) throws GLib.Error {
		string? agent = Environment.get_variable (@"FRIDA_TEST_$(prefix)_REPL_AGENT");
		if (agent == null)
			return "rpc.exports.evaluate = code => eval(code);";
		string contents;
		FileUtils.get_contents (agent, out contents);
		return contents;
	}

	private async void linux_agent_recovers_from_exception_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, linux_config_from_environment (h, prefix), """
			let caught = 'no';
			try {
				ptr('0x1000').readU32();
			} catch (e) {
				caught = 'yes';
			}
			send({ caught: caught });
		""", "\"caught\":\"yes\"");
	}

	private async void linux_enumerates_processes_in_live_guest (Harness h, string prefix) {
		var config = linux_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var options = new ProcessQueryOptions ();
			options.scope = FULL;
			var processes = yield device.enumerate_processes (options, null);

			assert_true (processes.size () != 0);

			Process? init = null;
			for (int i = 0; i != processes.size (); i++) {
				var process = processes.get (i);
				assert_true (process.name != "");
				if (process.pid == 1)
					init = process;
			}
			assert_nonnull (init);
		} catch (GLib.Error e) {
			printerr ("
FAIL: %s

", e.message);
			assert_not_reached ();
		}

		yield manager.close (null);

		h.done ();
	}

	private async void linux_injects_into_process_in_live_guest (Harness h, string prefix) {
		var config = linux_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "busybox");
			if (pid == 0) {
				var options = new ProcessQueryOptions ();
				options.scope = METADATA;
				var processes = yield device.enumerate_processes (options, null);
				var seen = new StringBuilder ();
				for (int i = 0; i != processes.size (); i++) {
					var p = processes.get (i);
					var path = p.parameters["path"];
					seen.append_printf ("\n  %u %s %s", p.pid, p.name,
						(path != null) ? path.get_string () : "(no path)");
				}
				printerr ("\nNo busybox among:%s\n\n", seen.str);
			}
			assert_true (pid != 0);

			var session = yield device.attach (pid, null, null);
			assert_nonnull (session);

			var script = yield session.create_script ("""
				recv('ping', () => { send(Process.id >>> 0); });
				send('ready');
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();
			assert_true (messages[0].contains ("ready"));

			script.post ("""{"type":"ping"}""");
			while (messages.size < 2)
				yield h.process_events ();
			assert_true (messages[1].contains (pid.to_string ()));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_agent_runs_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), "send(1 + 1);", "\"payload\":2");
	}

	private async void winnt_enumerates_modules_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const mods = Process.enumerateModules();
			const kernel = mods.find(m => m.name === 'ntoskrnl.exe');
			const named = kernel.enumerateExports().some(e => e.name === 'ExAllocatePoolWithTag');
			const hal = mods.some(m => m.name === 'hal.dll');
			const drivers = mods.some(m => m.name === 'atapi.sys');
			send({ named, hal, drivers });
		""", "\"named\":true,\"hal\":true,\"drivers\":true");
	}

	private async void winnt_watches_its_own_threads_in_live_guest (SlowHarness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_program (device, "explorer.exe");
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const seen = [];
				const others = Process.enumerateThreads()
					.filter(t => t.id !== Process.getCurrentThreadId());
				send(['registers', others.length,
					others.every(t => t.context !== undefined && !t.context.pc.isNull())]);
				Process.attachThreadObserver({
					onAdded(thread) {
						seen.push(['added', thread.id.toString(),
							Process.getCurrentThreadId() === thread.id,
							thread.entrypoint === undefined
								? 'nowhere'
								: thread.entrypoint.routine.toString()]);
					},
					onRemoved(thread) {
						seen.push(['removed', thread.id.toString()]);
					}
				});

				const k32 = Process.getModuleByName('kernel32.dll');
				const createThread = new NativeFunction(k32.getExportByName('CreateThread'),
					'pointer', ['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer'],
					{ abi: Process.arch === 'ia32' ? 'stdcall' : 'win64' });
				const closeHandle = new NativeFunction(k32.getExportByName('CloseHandle'),
					'int', ['pointer'], { abi: Process.arch === 'ia32' ? 'stdcall' : 'win64' });

				const body = Memory.alloc(Process.pageSize);
				Memory.protect(body, Process.pageSize, 'rwx');
				Memory.patchCode(body, 16, code => {
					const w = new X86Writer(code, { pc: body });
					w.putXorRegReg('eax', 'eax');
					if (Process.arch === 'ia32')
						w.putRetImm(4);
					else
						w.putRet();
					w.flush();
				});

				const out = Memory.alloc(4);
				const handle = createThread(NULL, 0, body, NULL, 0, out);
				const made = out.readU32();
				closeHandle(handle);

				recv('poll', () => {
					const mine = seen.filter(e => e[1] === made.toString());
					send(['saw', mine.some(e => e[0] === 'added' && e[2] &&
							e[3] === body.toString()),
						mine.some(e => e[0] === 'removed'), seen.length]);
				});
				send('ready');
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();

			var settle = new TimeoutSource.seconds (3);
			settle.set_callback (winnt_watches_its_own_threads_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			script.post ("""{"type":"poll"}""");
			while (said.size < 3)
				yield h.process_events ();
			printerr ("\nREGISTERS %s\nSAW %s\n", said[0], said[said.size - 1]);

			assert_true (said[0].contains ("\"registers\","));
			assert_true (!said[0].contains (",false]"));
			assert_true (said[said.size - 1].contains ("\"saw\",true,true,"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_injects_into_process_in_live_guest (Harness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");
			assert_true (pid != 0);

			// Success shows that the placed copy started and reported this process.
			var session = yield device.attach (pid, null, null);
			assert_nonnull (session);

			// The script runs in the target process, thus Process.id gives the id of the target.
			var script = yield session.create_script ("""
				recv('ping', () => { send(Process.id >>> 0); });
				send('ready');
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();
			assert_true (messages[0].contains ("ready"));

			// The script answers only if the message arrives, thus this tests the reverse direction.
			script.post ("""{"type":"ping"}""");
			while (messages.size < 2)
				yield h.process_events ();
			assert_true (messages[1].contains (pid.to_string ()));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_enumerates_processes_in_live_guest (Harness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var options = new ProcessQueryOptions ();
			options.scope = FULL;
			var processes = yield device.enumerate_processes (options, null);

			Process? shell = null;
			Process? system = null;
			for (int i = 0; i != processes.size (); i++) {
				var process = processes.get (i);
				var path = process.parameters["path"];
				var file = (path != null) ? path.get_string ().down () : process.name.down ();
				if (file.has_suffix ("explorer.exe"))
					shell = process;
				else if (file == "system")
					system = process;
			}
			assert_nonnull (shell);
			assert_nonnull (system);

			// Each of these is an id from the handle table, but the idle process is different.
			assert_true (system.pid == 4);
			assert_true (shell.pid > 4);

			for (int i = 0; i != processes.size (); i++)
				assert_true (processes.get (i).name != "");

			// A process gives its path and its command line. The processes of the kernel have neither.
			assert_true (shell.parameters["path"].get_string ().down ().has_suffix ("explorer.exe"));

			var argv = shell.parameters["argv"];
			assert_nonnull (argv);
			assert_true (argv.n_children () >= 1);

			var icons = shell.parameters["icons"];
			assert_nonnull (icons);
			assert_true (icons.n_children () != 0);

			var icon = icons.get_child_value (0);
			assert_true (icon.lookup_value ("format", VariantType.STRING).get_string () == "rgba");
			uint16 width = icon.lookup_value ("width", VariantType.UINT16).get_uint16 ();
			uint16 height = icon.lookup_value ("height", VariantType.UINT16).get_uint16 ();
			assert_true (width != 0 && height != 0);
			assert_true (icon.lookup_value ("image", new VariantType ("ay")).get_size () == width * height * 4);

			// This icon is drawn for 32 bits, thus its edges and its shadow are partly transparent. A
			// one-bit mask gives only full transparency or none.
			var pixels = icon.lookup_value ("image", new VariantType ("ay"));
			unowned uint8[] rgba = (uint8[]) pixels.get_data ();
			rgba.length = (int) pixels.get_size ();
			uint partly = 0;
			for (int i = 3; i < rgba.length; i += 4) {
				if (rgba[i] != 0 && rgba[i] != 255)
					partly++;
			}
			assert_true (partly != 0);

			// The image gives each size one time, at its best color depth.
			var seen = new Gee.HashSet<uint32> ();
			for (size_t i = 0; i != icons.n_children (); i++) {
				var one = icons.get_child_value (i);
				uint32 shape = ((uint32) one.lookup_value ("width", VariantType.UINT16).get_uint16 () << 16)
					| one.lookup_value ("height", VariantType.UINT16).get_uint16 ();
				assert_false (seen.contains (shape));
				seen.add (shape);
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_resolves_symbols_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const alloc = Module.getGlobalExportByName('ExAllocatePoolWithTag');

			const exact = DebugSymbol.fromAddress(alloc);
			const inside = DebugSymbol.fromAddress(alloc.add(4));
			const byName = DebugSymbol.getFunctionByName('ExFreePoolWithTag');
			const matching = DebugSymbol.findFunctionsMatching('ExAllocatePool*');

			send({
				named: exact.name === 'ExAllocatePoolWithTag',
				module: exact.moduleName === 'ntoskrnl.exe',
				closest: inside.name === 'ExAllocatePoolWithTag',
				byName: !byName.isNull(),
				matching: matching.length > 0
			});
		""", "\"named\":true,\"module\":true,\"closest\":true,\"byName\":true,\"matching\":true");
	}

	private async void winnt_enumerates_ranges_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const kernel = Process.enumerateModules().find(m => m.name === 'ntoskrnl.exe');
			const ranges = Process.enumerateRanges('r--');

			const covering = ranges.find(r => r.base.compare(kernel.base) <= 0 &&
				r.base.add(r.size).compare(kernel.base) > 0);
			const kernelSpace = ranges.every(r => r.base.compare(ptr('0x80000000')) >= 0);
			const sorted = ranges.every((r, i) =>
				i === 0 || ranges[i - 1].base.add(ranges[i - 1].size).compare(r.base) <= 0);

			send({
				found: ranges.length > 1,
				covering: covering !== undefined,
				kernelSpace: kernelSpace,
				sorted: sorted,
				executable: covering.protection.indexOf('x') !== -1
			});
		""", "\"found\":true,\"covering\":true,\"kernelSpace\":true,\"sorted\":true,\"executable\":true");
	}

	private async void winnt_enumerates_threads_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const threads = Process.enumerateThreads();
			const mine = Process.getCurrentThreadId();

			// The kernel gives the registers of a thread only if the thread has a user-mode part. Thus
			// the threads that run only in the kernel give none.
			const pc = (Process.pointerSize === 4) ? 'eip' : 'rip';
			const sp = (Process.pointerSize === 4) ? 'esp' : 'rsp';
			const contextual = threads.filter(t => t.context !== undefined);

			send({
				several: threads.length > 10,
				listed: threads.some(t => t.id === mine),
				distinct: new Set(threads.map(t => t.id)).size === threads.length,
				reported: contextual.length > 0,
				quiet: contextual.length < threads.length,
				somewhere: contextual.every(t => !t.context[pc].isNull() && !t.context[sp].isNull()),
				apart: new Set(contextual.map(t => t.context[sp].toString())).size > 1
			});
		""", "\"several\":true,\"listed\":true,\"distinct\":true," +
			"\"reported\":true,\"quiet\":true,\"somewhere\":true,\"apart\":true");
	}

	private async void winnt_reads_and_writes_memory_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const kernel = Process.enumerateModules().find(m => m.name === 'ntoskrnl.exe');
			const header = new Uint8Array(kernel.base.readByteArray(2));

			const scratch = Memory.alloc(8);
			scratch.writeByteArray([1, 2, 3, 4]);
			const written = new Uint8Array(scratch.readByteArray(4));

			let missing = 'no';
			try {
				if (ptr('0xfffff000').readByteArray(16) === null)
					missing = 'yes';
			} catch (e) {
				missing = 'yes';
			}

			send({
				mz: header[0] === 0x4d && header[1] === 0x5a,
				roundtrip: written[0] === 1 && written[3] === 4,
				missing: missing
			});
		""", "\"mz\":true,\"roundtrip\":true,\"missing\":\"yes\"");
	}

	// RtlUpperChar has no side effects and the kernel almost never calls it. Thus the hook sees
	// only the calls from this test.
	private async void winnt_hooks_kernel_function_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const kernel = Process.enumerateModules().find(m => m.name === 'ntoskrnl.exe');
			const upper = kernel.enumerateExports().find(e => e.name === 'RtlUpperChar').address;

			let seen = null;
			Interceptor.attach(upper, {
				onEnter(args) {
					seen = args[0].toInt32();
				}
			});
			Interceptor.flush();

			const abi = (Process.pointerSize === 4) ? 'stdcall' : 'win64';
			const call = new NativeFunction(upper, 'uint8', ['uint8'], { abi });
			const result = call(0x61);
			send({ seen: seen, result: result });
		""", "\"seen\":97,\"result\":65");
	}

	// The compiler backend and the kernel must agree about the position of the arguments. This
	// answer is different for each word size.
	private async void winnt_compiles_c_calling_kernel_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			const kernel = Process.enumerateModules().find(m => m.name === 'ntoskrnl.exe');
			const upper = kernel.enumerateExports().find(e => e.name === 'RtlUpperChar').address;

			const convention = (Process.pointerSize === 4) ? '__attribute__((stdcall))' : '';
			const cm = new CModule(`
				extern unsigned char ${convention} RtlUpperChar (unsigned char c);

				unsigned char
				shout (unsigned char c)
				{
				  return RtlUpperChar (c);
				}
			`, { RtlUpperChar: upper });

			const shout = new NativeFunction(cm.shout, 'uint8', ['uint8']);
			send({ result: shout(0x62) });
		""", "\"result\":66");
	}

	private async void winnt_agent_recovers_from_exception_in_live_guest (Harness h, string prefix) {
		yield run_script_in_live_guest (h, winnt_config_from_environment (h, prefix), """
			let caught = 'no';
			try {
				ptr('0xfffff000').readU32();
			} catch (e) {
				caught = 'yes';
			}
			send({ caught: caught });
		""", "\"caught\":\"yes\"");
	}

	// Two sessions on one process use the same copy. Thus one session can detach and the other
	// keeps a working agent and its own scripts.
	private async void enumerates_target_modules_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const names = Process.enumerateModules().map(m => m.name.toUpperCase());
				const kernel32 = Module.getGlobalExportByName('GetProcessHeap');
				send([names.includes('KERNEL32.DLL'), names.length > 2, kernel32 !== null]);
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();

			// The process has its own modules, thus a script finds them and what they export.
			assert_true (messages[0].contains ("[true,true,true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void keeps_two_processes_apart_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint first_pid = yield find_explorer (device);
			uint second_pid = yield device.spawn ("C:\\WINDOWS\\NOTEPAD.EXE", null, null);

			var first = yield device.attach (first_pid, null, null);
			var second = yield device.attach (second_pid, null, null);

			// Each copy numbers its scripts from one, thus both of these have the same name.
			var here = yield first.create_script ("send(Process.id >>> 0);", null, null);
			var there = yield second.create_script ("send(Process.id >>> 0);", null, null);

			var from_here = new Gee.ArrayList<string> ();
			var from_there = new Gee.ArrayList<string> ();
			here.message.connect ((json, data) => {
				from_here.add (json);
			});
			there.message.connect ((json, data) => {
				from_there.add (json);
			});

			yield here.load (null);
			yield there.load (null);
			while (from_here.size < 1 || from_there.size < 1)
				yield h.process_events ();

			// Each session hears its own process, and only that one.
			assert_true (from_here.size == 1);
			assert_true (from_here[0].contains (first_pid.to_string ()));
			assert_true (from_there.size == 1);
			assert_true (from_there[0].contains (second_pid.to_string ()));

			yield device.resume (second_pid, null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_enumerates_target_modules_in_live_guest (Harness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const names = Process.enumerateModules().map(m => m.name.toLowerCase());
				const known = Module.getGlobalExportByName('NtQueryInformationProcess');
				send([names.includes('ntdll.dll'), names.length > 2, known !== null]);
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();

			// The process has its own modules, thus a script finds them and what they export.
			assert_true (messages[0].contains ("[true,true,true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_sees_a_module_arrive_in_live_guest (Harness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const arrivals = [];
				Process.attachModuleObserver({
					onAdded(m) { arrivals.push(m.name.toLowerCase()); }
				});

				const load = new NativeFunction(
					Module.getGlobalExportByName('LoadLibraryExA'), 'pointer',
					['pointer', 'pointer', 'uint']);
				const name = Memory.alloc(16);
				name.writeUtf8String('ole32.dll');
				const handle = load(name, NULL, 0);

				send([handle.isNull(), arrivals.includes('winmm.dll')]);
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();

			// The library arrived while the script watched, thus the script heard about it.
			assert_true (messages[0].contains ("[false,true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void attaches_again_after_detach_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_explorer (device);

			for (uint round = 0; round != 2; round++) {
				var session = yield device.attach (pid, null, null);
				var script = yield session.create_script ("""
					send(Process.id >>> 0);
				""", null, null);

				string? received = null;
				bool waiting = false;
				var handler = script.message.connect ((json, data) => {
					received = json;
					if (waiting) {
						waiting = false;
						attaches_again_after_detach_in_live_guest.callback ();
					}
				});
				yield script.load (null);
				if (received == null) {
					waiting = true;
					yield;
				}
				script.disconnect (handler);

				assert_true (received.contains (pid.to_string ()));

				yield session.detach (null);

				for (uint i = 0; i != 100; i++)
					yield h.process_events ();
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_calls_system_functions_in_live_guest (Harness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);

			var script = yield session.create_script ("""
				const createThread = new NativeFunction(
					Module.getGlobalExportByName('CreateThread'),
					'pointer', ['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);
				const waitFor = new NativeFunction(
					Module.getGlobalExportByName('WaitForSingleObject'),
					'uint', ['pointer', 'uint']);
				const sleep = Module.getGlobalExportByName('Sleep');

				const createProcess = new NativeFunction(
					Module.getGlobalExportByName('CreateProcessW'), 'int',
					['pointer', 'pointer', 'pointer', 'pointer', 'int', 'uint', 'pointer',
						'pointer', 'pointer', 'pointer']);
				const terminate = new NativeFunction(
					Module.getGlobalExportByName('TerminateProcess'), 'int', ['pointer', 'uint']);

				recv('go', () => {
					const thread = createThread(NULL, 0, sleep, ptr(1), 0, NULL);
					const threadRan = waitFor(thread, 5000);

					const line = Memory.allocUtf16String('C:\\WINDOWS\\system32\\notepad.exe');
					const startup = Memory.alloc(72);
					startup.writeU32(72);
					const created = Memory.alloc(16);
					const madeProcess = createProcess(NULL, line, NULL, NULL, 0, 0, NULL, NULL,
						startup, created);
					if (madeProcess !== 0)
						terminate(created.readPointer(), 0);

					send([thread.isNull(), threadRan, madeProcess !== 0]);
				});
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			script.post ("""{"type":"go"}""");
			while (messages.size < 1)
				yield h.process_events ();

			assert_true (messages[0].contains ("[false,0,true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void winnt_spawns_and_resumes_in_live_guest (Harness h, string prefix) {
		var config = winnt_config_from_environment (h, prefix);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield device.spawn ("C:\\WINDOWS\\system32\\notepad.exe", null, null);
			assert_true (pid != 0);

			// The process is held, thus it is there but has run none of its own code.
			var processes = yield device.enumerate_processes (null, null);
			bool present = false;
			for (int i = 0; i != processes.size (); i++) {
				if (processes.get (i).pid == pid)
					present = true;
			}
			assert_true (present);

			yield device.resume (pid, null);

			// The process is no longer held, thus there is nothing left to resume.
			try {
				yield device.resume (pid, null);
				assert_not_reached ();
			} catch (GLib.Error e) {
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void win9x_spawns_and_resumes_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield device.spawn ("C:\\WINDOWS\\NOTEPAD.EXE", null, null);
			assert_true (pid != 0);

			// The process is held, thus it is there to attach to and has run none of its own code.
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("send(Process.id >>> 0);", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();

			// The script answers from the process that was spawned, thus it runs there before the
			// process has run any of its own code.
			assert_true (messages[0].contains (pid.to_string ()));

			yield device.resume (pid, null);

			// The process is no longer held, thus there is nothing left to resume.
			try {
				yield device.resume (pid, null);
				assert_not_reached ();
			} catch (GLib.Error e) {
			}
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void shares_one_agent_between_sessions_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_explorer (device);

			var first = yield device.attach (pid, null, null);
			var second = yield device.attach (pid, null, null);

			var survivor = yield second.create_script ("recv('ping', () => { send('alive'); });", null, null);
			var messages = new Gee.ArrayList<string> ();
			survivor.message.connect ((json, data) => {
				messages.add (json);
			});
			yield survivor.load (null);

			var doomed = yield first.create_script ("send('doomed');", null, null);
			yield doomed.load (null);
			yield first.detach (null);

			// The copy stays for the session that is still attached.
			survivor.post ("""{"type":"ping"}""");
			while (messages.size < 1)
				yield h.process_events ();
			assert_true (messages[0].contains ("alive"));

			yield second.detach (null);

			// No session is attached now, thus the next attach places a new copy.
			var again = yield device.attach (pid, null, null);
			assert_nonnull (again);
			yield again.detach (null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void hooks_before_resume_in_live_guest (Harness h,
			BareboneConfig? config, string program) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield device.spawn (program, null, null);
			var session = yield device.attach (pid, null, null);

			var script = yield session.create_script ("""
				recv('arm', () => {
					const shapes = {};
					for (const name of ['LoadLibraryExA', 'LoadLibraryExW', 'LoadLibraryA',
							'FreeLibrary', 'GetProcAddress']) {
						const a = Module.findGlobalExportByName(name);
						shapes[name] = (a !== null)
							? [a.toString(), Array.from(new Uint8Array(a.readByteArray(12)))
								.map(b => b.toString(16).padStart(2, '0')).join('')] : null;
					}
					send(['shapes', shapes]);

					const image = Process.enumerateModules()[0];
					const headers = image.base.add(image.base.add(0x3c).readU32());
					const entry = image.base.add(headers.add(0x28).readU32());

					Interceptor.attach(entry, {
						onEnter() {
							send(['entered']);
						}
					});

					send(['armed', image.name, entry.toString()]);
				});
			""", null, null);

			bool armed = false;
			bool entered = false;
			script.message.connect ((json, data) => {
				printerr ("\nHELD: %s\n", json);
				if (json.contains ("armed"))
					armed = true;
				else if (json.contains ("entered"))
					entered = true;
			});
			yield script.load (null);

			script.post ("""{"type":"arm"}""");
			while (!armed)
				yield h.process_events ();

			yield device.resume (pid, null);

			while (!entered)
				yield h.process_events ();
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void win9x_sees_a_module_arrive_in_live_guest (Harness h) {
		var config = win9x_config_from_environment (h);
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			uint pid = yield find_program (device, "explorer.exe");

			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const arrivals = [];
				Process.attachModuleObserver({
					onAdded(m) { arrivals.push(m.name.toLowerCase()); }
				});

				const load = new NativeFunction(
					Module.getGlobalExportByName('LoadLibraryA'), 'pointer', ['pointer']);
				const name = Memory.alloc(32);
				name.writeUtf8String('ole32.dll');
				const handle = load(name);

				send([handle.isNull(), arrivals.some(n => n.indexOf('ole32') !== -1)]);
			""", null, null);

			var messages = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				messages.add (json);
			});
			yield script.load (null);
			while (messages.size < 1)
				yield h.process_events ();

			assert_true (messages[0].contains ("[false,true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void finds_a_thread_by_id_in_live_guest (Frida.Test.AsyncHarness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			yield expect_thread_lookup (h, device, 0);
			yield expect_thread_lookup (h, device, yield find_explorer (device));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void expect_thread_lookup (Frida.Test.AsyncHarness h, Device device, uint pid)
			throws GLib.Error {
		var session = yield device.attach (pid, null, null);
		var script = yield session.create_script ("""
			const mine = Process.getCurrentThreadId();
			const other = Process.enumerateThreads()
				.find(t => t.id !== mine && t.context !== undefined);
			const found = Process.findThreadById(other.id);
			send([found !== null, found !== null && found.id === other.id,
				found !== null && !found.context.pc.isNull(),
				Process.findThreadById(0x7ffffff0) === null]);
		""", null, null);

		var messages = new Gee.ArrayList<string> ();
		script.message.connect ((json, data) => {
			messages.add (json);
		});
		yield script.load (null);
		while (messages.is_empty)
			yield h.process_events ();

		if (!messages[0].contains ("[true,true,true,true]"))
			printerr ("\nLOOKUP in %u: %s\n", pid, messages[0]);
		assert_true (messages[0].contains ("[true,true,true,true]"));

		yield session.detach (null);
	}

	private async void follows_a_thread_in_live_guest (Frida.Test.AsyncHarness h,
			BareboneConfig? config, string library) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const lib = Process.getModuleByName('LIBRARY');
				const fn = (name, ret, args) =>
					new NativeFunction(lib.getExportByName(name), ret, args,
						Process.pointerSize === 4 ? { abi: 'stdcall' } : {});
				const createThread = fn('CreateThread', 'pointer',
					['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);
				const terminateThread = fn('TerminateThread', 'int', ['pointer', 'uint']);
				const closeHandle = fn('CloseHandle', 'int', ['pointer']);

				const body = Memory.alloc(Process.pageSize);
				Memory.protect(body, Process.pageSize, 'rwx');
				Memory.patchCode(body, 16, code => {
					const w = new X86Writer(code, { pc: body });
					w.putXorRegReg('eax', 'eax');
					if (Process.pointerSize === 8)
						w.putRet();
					else
						w.putRetImm(4);
					w.flush();
				});

				const out = Memory.alloc(4);
				const handle = createThread(NULL, 0, body, NULL, CREATE_SUSPENDED, out);
				const id = out.readU32();

				const before = Process.findThreadById(id).context.pc;
				Stalker.follow(id, { events: { block: true } });
				const after = Process.findThreadById(id).context.pc;
				Stalker.unfollow(id);

				terminateThread(handle, 0);
				closeHandle(handle);

				const elsewhere = after.compare(lib.base) < 0 ||
					after.compare(lib.base.add(lib.size)) >= 0;
				send(['followed', !before.equals(after), elsewhere]);
			""".replace ("LIBRARY", library).replace ("CREATE_SUSPENDED", "4"), null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();

			if (!said[0].contains ("[\"followed\",true,true]"))
				printerr ("\nFOLLOWED %s\n", said[0]);
			assert_true (said[0].contains ("[\"followed\",true,true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void copy_recovers_from_exception_in_live_guest (Frida.Test.AsyncHarness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				let caught = 'no';
				try {
					ptr('0xfffff000').readU32();
				} catch (e) {
					caught = 'yes';
				}
				send({ caught: caught });
			""", null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();

			if (!said[0].contains ("\"caught\":\"yes\""))
				printerr ("\nCAUGHT %s\n", said[0]);
			assert_true (said[0].contains ("\"caught\":\"yes\""));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void stalks_a_thread_in_live_guest (Frida.Test.AsyncHarness h,
			BareboneConfig? config, string library) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				const lib = Process.getModuleByName('LIBRARY');
				const fn = (name, ret, args) =>
					new NativeFunction(lib.getExportByName(name), ret, args,
						Process.pointerSize === 4 ? { abi: 'stdcall' } : {});
				const createThread = fn('CreateThread', 'pointer',
					['pointer', 'uint', 'pointer', 'pointer', 'uint', 'pointer']);
				const terminateThread = fn('TerminateThread', 'int', ['pointer', 'uint']);
				const closeHandle = fn('CloseHandle', 'int', ['pointer']);

				const flag = Memory.alloc(4);
				const body = Memory.alloc(Process.pageSize);
				Memory.protect(body, Process.pageSize, 'rwx');

				const sleep = lib.getExportByName('Sleep');
				const little = value => [value & 0xff, (value >>> 8) & 0xff,
					(value >>> 16) & 0xff, (value >>> 24) & 0xff];
				const wide = value => {
					const bytes = [];
					for (let i = 0; i !== 8; i++)
						bytes.push(value.shr(i * 8).and(0xff).toUInt32());
					return bytes;
				};
				Memory.patchCode(body, 64, code => {
					code.writeByteArray((Process.pointerSize === 4)
						? [0x6a, 0x64, 0xe8, ...little(sleep.sub(body.add(7)).toInt32()),
							0xa1, ...little(flag.toUInt32()), 0x85, 0xc0, 0x74, 0xf0,
							0x33, 0xc0, 0xc2, 0x04, 0x00]
						: [0x48, 0x83, 0xec, 0x28, 0xb9, 0x64, 0x00, 0x00, 0x00,
							0x48, 0xb8, ...wide(sleep), 0xff, 0xd0,
							0x48, 0x83, 0xc4, 0x28,
							0x48, 0xb8, ...wide(flag), 0x8b, 0x00, 0x85, 0xc0, 0x74, 0xd7,
							0x33, 0xc0, 0xc3]);
				});

				const out = Memory.alloc(4);
				const handle = createThread(NULL, 0, body, NULL, 0, out);
				const id = out.readU32();

				let blocks = 0;
				recv('follow', () => {
					// A service of this system is reached by a call through a gate, which the
					// Stalker cannot follow. Thus leave the library that holds them alone.
					Stalker.exclude(lib);
					Stalker.follow(id, {
						events: { block: true },
						onReceive(events) {
							blocks += Stalker.parse(events, { annotate: false }).length;
						}
					});
					send('followed');
				});

				recv('poll', () => {
					send(['stalked', blocks, Process.findThreadById(id) !== null]);
				});

				recv('stop', () => {
					Stalker.unfollow(id);
					flag.writeU32(1);
					terminateThread(handle, 0);
					closeHandle(handle);
					send('stopped');
				});

				send('ready');
			""".replace ("LIBRARY", library), null, null);

			var said = new Gee.ArrayList<string> ();
			script.message.connect ((json, data) => {
				said.add (json);
			});
			yield script.load (null);
			while (said.is_empty)
				yield h.process_events ();

			script.post ("""{"type":"follow"}""");
			while (said.size < 2)
				yield h.process_events ();

			var settle = new TimeoutSource.seconds (5);
			settle.set_callback (stalks_a_thread_in_live_guest.callback);
			settle.attach (MainContext.get_thread_default ());
			yield;
			settle.destroy ();

			script.post ("""{"type":"poll"}""");
			while (said.size < 3)
				yield h.process_events ();

			script.post ("""{"type":"stop"}""");
			while (said.size < 4)
				yield h.process_events ();

			if (said[2].contains ("[\"stalked\",0,") || !said[2].contains (",true]"))
				printerr ("\nSTALKED %s\n", said[2]);
			assert_true (!said[2].contains ("[\"stalked\",0,"));
			assert_true (said[2].contains (",true]"));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void reports_the_platform_in_live_guest (Frida.Test.AsyncHarness h,
			BareboneConfig? config, string platform) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);

			yield expect_platform (h, device, 0, platform);
			yield expect_platform (h, device, yield find_explorer (device), platform);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void expect_platform (Frida.Test.AsyncHarness h, Device device, uint pid,
			string platform) throws GLib.Error {
		var session = yield device.attach (pid, null, null);
		var script = yield session.create_script ("send(Process.platform);", null, null);

		var said = new Gee.ArrayList<string> ();
		script.message.connect ((json, data) => {
			said.add (json);
		});
		yield script.load (null);
		while (said.is_empty)
			yield h.process_events ();

		if (!said[0].contains ("\"" + platform + "\""))
			printerr ("\nPLATFORM in %u: %s\n", pid, said[0]);
		assert_true (said[0].contains ("\"" + platform + "\""));

		yield session.detach (null);
	}

	private async void moves_a_lot_of_data_in_live_guest (Frida.Test.AsyncHarness h,
			BareboneConfig? config) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			uint pid = yield find_explorer (device);
			var session = yield device.attach (pid, null, null);
			var script = yield session.create_script ("""
				rpc.exports = {
					take(expected, data) {
						if (data === null || data === undefined)
							return [-1, 0, 0, 0];
						const bytes = new Uint8Array(data);
						const last = bytes.length - 1;
						return [bytes.length, bytes[0], bytes[last], bytes[last >> 1]];
					}
				};
			""", null, null);

			var peer = new ScriptPeer (script);
			script.message.connect ((json, data) => {
				peer.client.try_handle_message (json);
			});
			yield script.load (null);

			uint chunk_size = CHUNK_SIZE;
			string? wanted = Environment.get_variable ("FRIDA_TEST_CHUNK_SIZE");
			if (wanted != null)
				chunk_size = uint.parse (wanted);
			var chunk = new uint8[chunk_size];
			for (uint i = 0; i != chunk.length; i++)
				chunk[i] = (uint8) i;
			var bytes = new Bytes (chunk);

			var size = new Json.Node.alloc ().init_int (chunk_size);
			var timer = new Timer ();
			for (uint round = 0; round != CHUNK_COUNT; round++) {
				var answer = yield peer.client.call ("take", new Json.Node[] { size }, bytes, null);

				var seen = answer.get_array ();
				assert_true (seen.get_int_element (0) == chunk_size);
				assert_true (seen.get_int_element (1) == chunk[0]);
				assert_true (seen.get_int_element (2) == chunk[chunk_size - 1]);
				assert_true (seen.get_int_element (3) == chunk[(chunk_size - 1) >> 1]);
			}
			double spent = timer.elapsed ();

			printerr ("\n%s: %u chunks of %u KiB in %.1f s, %.2f MiB/s\n", config.kernel.to_string (),
				CHUNK_COUNT, chunk_size / 1024, spent,
				(CHUNK_COUNT * (double) chunk_size) / (1024.0 * 1024.0) / spent);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private const uint CHUNK_SIZE = 4 * 1024 * 1024;
	private const uint CHUNK_COUNT = 4;

	private sealed class ScriptPeer : Object, RpcPeer {
		public RpcClient client;

		private weak Script script;

		public ScriptPeer (Script script) {
			this.script = script;
			client = new RpcClient (this);
		}

		public async void post_rpc_message (string json, Bytes? data, Cancellable? cancellable)
				throws Error, IOError {
			script.post (json, data);
		}
	}

	private async uint find_explorer (Device device) throws GLib.Error {
		return yield find_program (device, "explorer.exe");
	}

	private async void enumerates_threads_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, win9x_config_from_environment (h), """
			const threads = Process.enumerateThreads();
			const mine = Process.getCurrentThreadId();
			const contextual = threads.filter(t => t.context !== undefined);
			const distinct = new Set(contextual.map(t => t.context.esp.toString())).size > 1;
			send({
				several: threads.length > 1,
				listed: threads.some(t => t.id === mine),
				contextual: contextual.length > 1,
				distinct
			});
		""", "\"several\":true,\"listed\":true,\"contextual\":true,\"distinct\":true");
	}

	private async void win9x_enumerates_ranges_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, win9x_config_from_environment (h), """
			const vmm = Process.enumerateModules().find(m => m.name === 'VMM.VXD');
			const ranges = Process.enumerateRanges('r--');

			const covering = ranges.find(r => r.base.compare(vmm.base) <= 0 &&
				r.base.add(r.size).compare(vmm.base) > 0);
			const arena = ranges.every(r => r.base.compare(ptr('0xc0000000')) >= 0);

			send({ found: ranges.length > 1, covering: covering !== undefined, arena: arena });
		""", "\"found\":true,\"covering\":true,\"arena\":true");
	}

	private async void enumerates_modules_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, win9x_config_from_environment (h), """
			const mods = Process.enumerateModules();
			const vmm = mods.find(m => m.name === 'VMM.VXD');
			const named = vmm.enumerateExports().some(e => e.name === 'Get_Sys_VM_Handle');
			const serviceless = mods.some(m => m.name === 'VFAT.VXD');
			const mixedCase = mods.some(m => m.name === 'VNetSup.VXD');
			send({ named, serviceless, mixedCase });
		""", "\"named\":true,\"serviceless\":true,\"mixedCase\":true");
	}

	private async void agent_recovers_from_exception_in_live_guest (Harness h) {
		yield run_script_in_live_guest (h, win9x_config_from_environment (h), """
			let caught = 'no';
			try {
				ptr('0xfffff000').readU32();
			} catch (e) {
				caught = 'yes';
			}
			send({ caught: caught });
		""", "\"caught\":\"yes\"");
	}

	private BareboneConfig? win9x_config_from_environment (Frida.Test.AsyncHarness h) {
		string? agent_path = Environment.get_variable ("FRIDA_TEST_WIN9X_AGENT");
		string? qmp_path = Environment.get_variable ("FRIDA_TEST_WIN9X_QMP");
		string? stub_port = Environment.get_variable ("FRIDA_TEST_WIN9X_GDB_PORT");
		if (agent_path == null || qmp_path == null || stub_port == null) {
			h.done ();
			return null;
		}

		var config = new BareboneConfig ();
		config.connection.host = "127.0.0.1";
		config.connection.port = (uint16) uint.parse (stub_port);
		config.kernel = WIN9X;
		var transport = new BareboneHostlinkTransportConfig () {
			qmp = "unix:" + qmp_path,
			bus = Environment.get_variable ("FRIDA_TEST_WIN9X_BUS"),
		};
		try {
			config.agent = new BareboneInjectedAgentConfig.from_file (agent_path, transport);
		} catch (Error e) {
			h.done ();
			return null;
		}

		return config;
	}

	private BareboneConfig? linux_config_from_environment (Frida.Test.AsyncHarness h, string prefix) {
		string? agent_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_AGENT");
		string? qmp_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_QMP");
		string? stub_port = Environment.get_variable (@"FRIDA_TEST_$(prefix)_GDB_PORT");
		string? system_map = Environment.get_variable (@"FRIDA_TEST_$(prefix)_SYSTEM_MAP");
		if (agent_path == null || qmp_path == null || stub_port == null || system_map == null) {
			h.done ();
			return null;
		}

		var config = new BareboneConfig ();
		config.connection.host = "127.0.0.1";
		config.connection.port = (uint16) uint.parse (stub_port);
		string? stub_pid = Environment.get_variable (@"FRIDA_TEST_$(prefix)_STUB_PID");
		if (stub_pid != null) {
			config.connection.pid = (uint) uint.parse (stub_pid);
			config.connection.flavor = ANDROID_EMULATOR;
		}
		config.kernel = LINUX;
		config.image = new BareboneImageConfig () {
			file = system_map,
		};
		BareboneInjectingTransportConfig transport;
		string? vsock_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_VSOCK_PATH");
		if (vsock_path != null) {
			transport = new BareboneVsockPipeTransportConfig () {
				socket_path = vsock_path,
			};
		} else {
			string? ecam = Environment.get_variable (@"FRIDA_TEST_$(prefix)_ECAM");
			string? mmio = Environment.get_variable (@"FRIDA_TEST_$(prefix)_MMIO");
			transport = new BareboneHostlinkTransportConfig () {
				qmp = "unix:" + qmp_path,
				bus = Environment.get_variable (@"FRIDA_TEST_$(prefix)_BUS"),
				fabric = hostlink_fabric_from_environment (ecam, mmio),
			};
		}
		try {
			config.agent = new BareboneInjectedAgentConfig.from_file (agent_path, transport);
		} catch (Error e) {
			h.done ();
			return null;
		}

		return config;
	}

	private BareboneHostlinkFabric hostlink_fabric_from_environment (string? ecam, string? mmio) {
		if (ecam != null)
			return new BareboneHostlinkEcamFabric (uint64.parse (ecam));
		if (mmio != null)
			return new BareboneHostlinkMmioFabric ();
		return new BareboneHostlinkPortsFabric ();
	}

	private BareboneConfig? winnt_config_from_environment (Frida.Test.AsyncHarness h, string prefix) {
		string? agent_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_AGENT");
		string? qmp_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_QMP");
		string? stub_port = Environment.get_variable (@"FRIDA_TEST_$(prefix)_GDB_PORT");
		if (agent_path == null || qmp_path == null || stub_port == null) {
			h.done ();
			return null;
		}

		var config = new BareboneConfig ();
		config.connection.host = "127.0.0.1";
		config.connection.port = (uint16) uint.parse (stub_port);
		config.kernel = WINNT;
		var transport = new BareboneHostlinkTransportConfig () {
			qmp = "unix:" + qmp_path,
			bus = Environment.get_variable (@"FRIDA_TEST_$(prefix)_BUS"),
		};
		try {
			config.agent = new BareboneInjectedAgentConfig.from_file (agent_path, transport);
		} catch (Error e) {
			h.done ();
			return null;
		}

		return config;
	}

	private BareboneConfig? parallels_config_from_environment (Frida.Test.AsyncHarness h, string prefix) {
		string? agent_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_AGENT");
		string? serial_path = Environment.get_variable (@"FRIDA_TEST_$(prefix)_SERIAL");
		string? stub_port = Environment.get_variable (@"FRIDA_TEST_$(prefix)_GDB_PORT");
		if (agent_path == null || serial_path == null || stub_port == null) {
			h.done ();
			return null;
		}

		var config = new BareboneConfig ();
		config.connection.host = "127.0.0.1";
		config.connection.port = (uint16) uint.parse (stub_port);
		config.connection.flavor = PARALLELS;
		config.kernel = WINNT;
		var transport = new BareboneSerialTransportConfig () {
			path = serial_path,
		};
		try {
			config.agent = new BareboneInjectedAgentConfig.from_file (agent_path, transport);
		} catch (Error e) {
			h.done ();
			return null;
		}

		return config;
	}

	private async void call_rpc_in_live_guest (Harness h, BareboneConfig? config) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var session = yield device.attach (0, null, null);
			var script = yield session.create_script ("""
				rpc.exports.evaluate = code => eval(code);
			""", null, null);

			string? received = null;
			bool waiting = false;
			var handler = script.message.connect ((json, data) => {
				if (!json.contains ("frida:rpc"))
					return;
				received = json;
				if (waiting) {
					waiting = false;
					call_rpc_in_live_guest.callback ();
				}
			});
			yield script.load (null);

			script.post ("""["frida:rpc",1,"call","evaluate",["1 + 1"]]""");

			if (received == null) {
				waiting = true;
				yield;
			}
			script.disconnect (handler);

			if (!received.contains ("\"ok\""))
				printerr ("\nexpected an ok reply in: %s\n", received);
			assert_true (received.contains ("\"ok\""));

			yield session.detach (null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void run_script_in_live_guest (Harness h, BareboneConfig? config, string source,
			string expected) {
		if (config == null)
			return;

		var manager = new DeviceManager ();
		try {
			var device = yield manager.add_barebone_device (config);
			var session = yield device.attach (0, null, null);
			var script = yield session.create_script (source, null, null);

			string? received = null;
			bool waiting = false;
			var handler = script.message.connect ((json, data) => {
				received = json;
				if (waiting) {
					waiting = false;
					run_script_in_live_guest.callback ();
				}
			});
			yield script.load (null);
			if (received == null) {
				waiting = true;
				yield;
			}
			script.disconnect (handler);

			if (!received.contains (expected))
				printerr ("\nexpected %s in: %s\n", expected, received);
			assert_true (received.contains (expected));

			yield session.detach (null);
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			try {
				yield manager.close (null);
			} catch (GLib.Error e) {
			}
		}

		h.done ();
	}

	private async void modules_resolve_from_loaded_module_list (Harness h) {
		var target = new FakeTarget (IA32, new Ram ().steal (), LEGACY_MONITOR_DUMP);
		try {
			target.map_virtual (PCR_VA, pcr_page ());
			target.map_virtual (NT_STRUCTS_VA, nt_kernel_structs ());
			target.map_virtual (NT_KERNEL_VA, nt_kernel_image ());
			target.map_virtual (NT_HAL_VA, nt_hal_image ());
			yield target.open ();
			var machine = new Barebone.IA32Machine (target.client);

			var layout = yield Barebone.collect_winnt_layout (machine, null);

			var modules = layout.modules;
			assert_true (modules.size == 2);
			assert_true (modules[0].name == "ntoskrnl.exe");
			assert_true (modules[0].offset == NT_KERNEL_VA);
			assert_true (modules[0].size == NT_IMAGE_SIZE);
			assert_true (modules[1].name == "hal.dll");
			assert_true (modules[1].offset == NT_HAL_VA);

			// The forwarded export is skipped, and the module without an export directory
			// contributes nothing.
			var symbols = layout.symbols;
			assert_true (symbols.size == 1);
			assert_symbol (symbols[0], "NtWaitForSingleObject", (uint32) (NT_KERNEL_VA + NT_WAIT_RVA));
		} catch (GLib.Error e) {
			printerr ("\nFAIL: %s\n\n", e.message);
			assert_not_reached ();
		} finally {
			target.stop ();
		}

		h.done ();
	}

	private void assert_argument (BareboneCallArgument argument, BareboneCallArgumentRole role,
			uint64 value) {
		assert_true (argument.role == role);
		assert_true (argument.value == value);
	}

	private void assert_symbol (Barebone.SymbolInfo symbol, string name, uint32 address) {
		assert_true (symbol.name == name);
		assert_true (symbol.offset == address);
	}

	private const size_t FIRST_NEEDLE_OFFSET = 0x40;
	private const size_t SECOND_NEEDLE_OFFSET = 0x1c0;
	private const uint64 ARENA_VA = 0xc0000000;
	private const uint64 ARENA_PA = 0x3000;
	private const size_t ARENA_SIZE = 0x1000;

	private const size_t SERVICE_TABLE_OFFSET = 0x100;
	private const size_t VMM_BLOCK_OFFSET = 0x200;
	private const size_t DECOY_BLOCK_OFFSET = 0x300;

	private const uint32 UNIMPLEMENTED_SERVICE = 0;
	private const uint32 SERVICE_TABLE_OUTSIDE_ARENA = 0x00001000;
	private const int IMPLEMENTED_SERVICES = 7;

	private uint8[] arena_page_tables () {
		var ram = new Ram ();

		ram.write_uint32 (PD_PA + ((ARENA_VA >> 22) * 4), (uint32) PT_PA | 0x3);
		ram.write_uint32 (PT_PA + (0 * 4), (uint32) ARENA_PA | 0x3);

		return ram.steal ();
	}

	private Bytes arena_with_vmm_block () {
		var arena = new uint8[ARENA_SIZE];

		uint32[] services = { (uint32) 0xc0001000, (uint32) 0xc0001010, UNIMPLEMENTED_SERVICE,
			(uint32) 0xc0001030, (uint32) 0xc0001040, (uint32) 0xc0001050, (uint32) 0xc0001060,
			(uint32) 0xc0001070 };
		for (uint i = 0; i != services.length; i++)
			put_uint32 (arena, SERVICE_TABLE_OFFSET + (i * 4), services[i]);

		put_descriptor_block (arena, VMM_BLOCK_OFFSET, "VMM     ", (uint32) (ARENA_VA + SERVICE_TABLE_OFFSET),
			services.length);
		put_descriptor_block (arena, DECOY_BLOCK_OFFSET, "VCACHE  ", SERVICE_TABLE_OUTSIDE_ARENA,
			services.length);

		return new Bytes.take ((owned) arena);
	}

	private void put_descriptor_block (uint8[] arena, size_t offset, string name, uint32 service_table,
			uint service_count) {
		for (uint i = 0; i != name.length; i++)
			arena[offset + 0x0c + i] = (uint8) name[i];
		put_uint32 (arena, offset + 0x18, (uint32) ARENA_VA);
		put_uint32 (arena, offset + 0x30, service_table);
		put_uint32 (arena, offset + 0x34, service_count);
	}

	private void put_uint32 (uint8[] buf, size_t offset, uint32 val) {
		for (uint i = 0; i != 4; i++)
			buf[offset + i] = (uint8) (val >> (i * 8));
	}

	private void put_uint16 (uint8[] buf, size_t offset, uint16 val) {
		for (uint i = 0; i != 2; i++)
			buf[offset + i] = (uint8) (val >> (i * 8));
	}

	private void put_utf16 (uint8[] buf, size_t offset, string text) {
		for (uint i = 0; i != text.length; i++)
			put_uint16 (buf, offset + (i * 2), (uint16) text[i]);
	}

	private void put_ascii (uint8[] buf, size_t offset, string text) {
		for (uint i = 0; i != text.length; i++)
			buf[offset + i] = (uint8) text[i];
	}

	private const uint64 PCR_VA = 0xffdff000;
	private const uint64 NT_STRUCTS_VA = 0x80500000;
	private const uint64 NT_KERNEL_VA = 0x80400000;
	private const uint64 NT_HAL_VA = 0x80410000;
	private const size_t NT_PAGE_SIZE = 0x1000;

	private const size_t VERSION_BLOCK_OFFSET = 0x000;
	private const size_t MODULE_LIST_OFFSET = 0x100;
	private const size_t KERNEL_ENTRY_OFFSET = 0x200;
	private const size_t HAL_ENTRY_OFFSET = 0x280;
	private const size_t KERNEL_NAME_OFFSET = 0x300;
	private const size_t HAL_NAME_OFFSET = 0x340;

	private const uint32 NT_IMAGE_SIZE = 0x200000;
	private const uint32 NT_WAIT_RVA = 0x1000;
	private const size_t PE_HEADERS_OFFSET = 0xc0;
	private const uint32 EXPORT_DIRECTORY_RVA = 0x400;
	private const uint32 EXPORT_DIRECTORY_SIZE = 0x100;
	private const uint32 FORWARDED_EXPORT_RVA = 0x420;
	private const uint32 FUNCTION_TABLE_RVA = 0x430;
	private const uint32 NAME_TABLE_RVA = 0x440;
	private const uint32 ORDINAL_TABLE_RVA = 0x450;
	private const uint32 WAIT_NAME_RVA = 0x460;
	private const uint32 FORWARDED_NAME_RVA = 0x480;

	private Bytes pcr_page () {
		var page = new uint8[NT_PAGE_SIZE];

		put_uint32 (page, 0x1c, (uint32) PCR_VA);
		put_uint32 (page, 0x34, (uint32) (NT_STRUCTS_VA + VERSION_BLOCK_OFFSET));

		return new Bytes.take ((owned) page);
	}

	private Bytes nt_kernel_structs () {
		var structs = new uint8[NT_PAGE_SIZE];

		put_uint16 (structs, VERSION_BLOCK_OFFSET + 0x08, 0x014c);
		put_uint32 (structs, VERSION_BLOCK_OFFSET + 0x10, (uint32) NT_KERNEL_VA);
		put_uint32 (structs, VERSION_BLOCK_OFFSET + 0x18, (uint32) (NT_STRUCTS_VA + MODULE_LIST_OFFSET));

		put_uint32 (structs, MODULE_LIST_OFFSET, (uint32) (NT_STRUCTS_VA + KERNEL_ENTRY_OFFSET));

		put_table_entry (structs, KERNEL_ENTRY_OFFSET, NT_STRUCTS_VA + HAL_ENTRY_OFFSET, NT_KERNEL_VA,
			NT_IMAGE_SIZE, NT_STRUCTS_VA + KERNEL_NAME_OFFSET, "ntoskrnl.exe");
		put_table_entry (structs, HAL_ENTRY_OFFSET, NT_STRUCTS_VA + MODULE_LIST_OFFSET, NT_HAL_VA,
			0x20000, NT_STRUCTS_VA + HAL_NAME_OFFSET, "hal.dll");

		put_utf16 (structs, KERNEL_NAME_OFFSET, "ntoskrnl.exe");
		put_utf16 (structs, HAL_NAME_OFFSET, "hal.dll");

		return new Bytes.take ((owned) structs);
	}

	private void put_table_entry (uint8[] structs, size_t offset, uint64 next, uint64 dll_base, uint32 size,
			uint64 name_buffer, string name) {
		put_uint32 (structs, offset + 0x00, (uint32) next);
		put_uint32 (structs, offset + 0x18, (uint32) dll_base);
		put_uint32 (structs, offset + 0x20, size);
		put_uint16 (structs, offset + 0x2c, (uint16) (name.length * 2));
		put_uint16 (structs, offset + 0x2e, (uint16) (name.length * 2));
		put_uint32 (structs, offset + 0x30, (uint32) name_buffer);
	}

	private Bytes nt_kernel_image () {
		var image = new uint8[NT_PAGE_SIZE];

		put_pe_headers (image, EXPORT_DIRECTORY_RVA, EXPORT_DIRECTORY_SIZE);

		put_uint32 (image, EXPORT_DIRECTORY_RVA + 0x18, 2);
		put_uint32 (image, EXPORT_DIRECTORY_RVA + 0x1c, FUNCTION_TABLE_RVA);
		put_uint32 (image, EXPORT_DIRECTORY_RVA + 0x20, NAME_TABLE_RVA);
		put_uint32 (image, EXPORT_DIRECTORY_RVA + 0x24, ORDINAL_TABLE_RVA);

		put_uint32 (image, FUNCTION_TABLE_RVA, NT_WAIT_RVA);
		put_uint32 (image, FUNCTION_TABLE_RVA + 4, FORWARDED_EXPORT_RVA);
		put_uint32 (image, NAME_TABLE_RVA, WAIT_NAME_RVA);
		put_uint32 (image, NAME_TABLE_RVA + 4, FORWARDED_NAME_RVA);
		put_uint16 (image, ORDINAL_TABLE_RVA, 0);
		put_uint16 (image, ORDINAL_TABLE_RVA + 2, 1);
		put_ascii (image, WAIT_NAME_RVA, "NtWaitForSingleObject");
		put_ascii (image, FORWARDED_NAME_RVA, "Forwarded");

		return new Bytes.take ((owned) image);
	}

	private Bytes nt_hal_image () {
		var image = new uint8[NT_PAGE_SIZE];

		put_pe_headers (image, 0, 0);

		return new Bytes.take ((owned) image);
	}

	private void put_pe_headers (uint8[] image, uint32 export_directory_rva, uint32 export_directory_size) {
		put_uint16 (image, 0, 0x5a4d);
		put_uint32 (image, 0x3c, (uint32) PE_HEADERS_OFFSET);
		put_uint32 (image, PE_HEADERS_OFFSET, 0x00004550);
		put_uint16 (image, PE_HEADERS_OFFSET + 0x18, 0x010b);
		put_uint32 (image, PE_HEADERS_OFFSET + 0x78, export_directory_rva);
		put_uint32 (image, PE_HEADERS_OFFSET + 0x7c, export_directory_size);
	}

	private const uint64 ARM_TT0_PA = 0x0000;
	private const uint64 ARM_L2_PA = 0x4000;

	private uint8[] short_descriptor_page_tables () {
		var ram = new Ram ();

		ram.write_uint32 (ARM_TT0_PA + (0 * 4), (uint32) ARM_L2_PA | 0x1);
		ram.write_uint32 (ARM_TT0_PA + (1 * 4), 0x00800000 | (3 << 10) | 0x2);
		ram.write_uint32 (ARM_TT0_PA + (2 * 4), 0x00900000 | (1 << 10) | (1 << 15) | (1 << 4) | 0x2);

		ram.write_uint32 (ARM_L2_PA + (0 * 4), 0x00100000 | (3 << 4) | 0x2);
		ram.write_uint32 (ARM_L2_PA + (1 * 4), 0x00101000 | (3 << 4) | 0x3);
		ram.write_uint32 (ARM_L2_PA + (16 * 4), 0x00110000 | (3 << 4) | 0x1);

		return ram.steal ();
	}

	private ArmSystemState arm_short_descriptor_state () {
		return new ArmSystemState (0x00c5387d, 0, ARM_TT0_PA, ARM_TT0_PA);
	}

	private ArmSystemState arm_lpae_state () {
		return new ArmSystemState (0x00c5387d, 0x80000000, ARM_TT0_PA, ARM_TT0_PA);
	}

	private const uint64 ARM_SPAN_L2_0_PA = 0x4400;
	private const uint64 ARM_SPAN_L2_1_PA = 0x4800;
	private const uint ARM_L2_ENTRIES = 256;
	private const uint ARM_SPAN_FREE_IN_FIRST = 2;

	private uint8[] adjacent_arm_leaf_tables () {
		var ram = new Ram ();

		ram.write_uint32 (ARM_TT0_PA + (0 * 4), (uint32) ARM_SPAN_L2_0_PA | 0x1);
		ram.write_uint32 (ARM_TT0_PA + (1 * 4), (uint32) ARM_SPAN_L2_1_PA | 0x1);

		uint occupied = ARM_L2_ENTRIES - ARM_SPAN_FREE_IN_FIRST;
		for (uint i = 0; i != occupied; i++) {
			ram.write_uint32 (ARM_SPAN_L2_0_PA + (i * 4),
				(uint32) (0x00100000 + (i * 0x1000)) | (3 << 4) | 0x2);
		}

		return ram.steal ();
	}

	private uint8[] legacy_page_tables () {
		var ram = new Ram ();

		ram.write_uint32 (PD_PA + (0 * 4), (uint32) PT_PA | 0x3);
		ram.write_uint32 (PD_PA + (1 * 4), 0x00800081);

		ram.write_uint32 (PT_PA + (0 * 4), 0x00100003);
		ram.write_uint32 (PT_PA + (1 * 4), 0x00101003);
		ram.write_uint32 (PT_PA + (2 * 4), 0x00102001);

		return ram.steal ();
	}

	private const uint64 SPAN_PT0_PA = 0x4000;
	private const uint64 SPAN_PT1_PA = 0x5000;
	private const uint SPAN_ENTRIES_PER_TABLE = 1024;
	private const uint SPAN_FREE_IN_FIRST = 2;
	private const uint SPAN_FREE_IN_SECOND = 1;
	private const uint SPAN_PD_SLOT = 1022;
	private const uint64 SPAN_BASE_VA = (uint64) SPAN_PD_SLOT << 22;

	// The two topmost leaf tables, each all but full, so a run longer than what the
	// first has left has to carry on into the second.
	private uint8[] adjacent_leaf_tables () {
		var ram = new Ram ();

		ram.write_uint32 (PD_PA + (SPAN_PD_SLOT * 4), (uint32) SPAN_PT0_PA | 0x7);
		ram.write_uint32 (PD_PA + ((SPAN_PD_SLOT + 1) * 4), (uint32) SPAN_PT1_PA | 0x7);

		uint occupied = SPAN_ENTRIES_PER_TABLE - SPAN_FREE_IN_FIRST;
		for (uint i = 0; i != occupied; i++)
			ram.write_uint32 (SPAN_PT0_PA + (i * 4), (uint32) (0x00100000 + (i * 0x1000)) | 0x3);

		for (uint i = SPAN_FREE_IN_SECOND; i != SPAN_ENTRIES_PER_TABLE; i++)
			ram.write_uint32 (SPAN_PT1_PA + (i * 4), (uint32) (0x00300000 + (i * 0x1000)) | 0x3);

		return ram.steal ();
	}

	private const uint64 PAE_PDPT_PA = 0x1000;
	private const uint64 PAE_PD_PA = 0x2000;
	private const uint64 PAE_PT_PA = 0x3000;

	private const string PAE_MONITOR_DUMP =
		"CR0=80050033 CR2=00000000 CR3=00001000 CR4=00000020\n" +
		"EFER=0000000000000800\n";

	// Paging on with PAE and NX: 64-bit entries, one of them a 2 MiB page above the 4 GiB line.
	private uint8[] pae_page_tables () {
		var ram = new Ram ();

		ram.write_uint64 (PAE_PDPT_PA + (0 * 8), PAE_PD_PA | 0x1);

		ram.write_uint64 (PAE_PD_PA + (0 * 8), PAE_PT_PA | 0x3);
		ram.write_uint64 (PAE_PD_PA + (1 * 8), 0x8000000140000081);

		ram.write_uint64 (PAE_PT_PA + (0 * 8), 0x8000000000100003);
		ram.write_uint64 (PAE_PT_PA + (1 * 8), 0x0000000000101003);

		return ram.steal ();
	}

	private const uint64 X64_PML4_PA = 0x1000;
	private const uint64 X64_PDPT_PA = 0x2000;
	private const uint64 X64_PD_PA = 0x3000;
	private const uint64 X64_PT_PA = 0x4000;
	private const uint64 X64_KERNEL_PDPT_PA = 0x5000;

	private const string X64_MONITOR_DUMP =
		"CR0=80050033 CR2=0000000000000000 CR3=0000000000001000 CR4=00000020\n" +
		"EFER=0000000000000d00\n";

	// Long mode with NX: a four-level walk, one 2 MiB page, and a 1 GiB page in the kernel half.
	private uint8[] long_mode_page_tables () {
		var ram = new Ram ();

		ram.write_uint64 (X64_PML4_PA + (0 * 8), X64_PDPT_PA | 0x3);
		ram.write_uint64 (X64_PML4_PA + (384 * 8), X64_KERNEL_PDPT_PA | 0x3);

		ram.write_uint64 (X64_PDPT_PA + (0 * 8), X64_PD_PA | 0x3);

		ram.write_uint64 (X64_PD_PA + (0 * 8), X64_PT_PA | 0x3);
		ram.write_uint64 (X64_PD_PA + (1 * 8), 0x8000000140000081);

		ram.write_uint64 (X64_PT_PA + (0 * 8), 0x8000000000100003);
		ram.write_uint64 (X64_PT_PA + (1 * 8), 0x0000000000101003);

		ram.write_uint64 (X64_KERNEL_PDPT_PA + (0 * 8), 0x400000e3);

		return ram.steal ();
	}

	private class Ram {
		public const size_t SIZE = 0x6000;

		private uint8[] data = new uint8[SIZE];

		public void write_uint32 (uint64 offset, uint32 val) {
			for (uint i = 0; i != 4; i++)
				data[offset + i] = (uint8) (val >> (i * 8));
		}

		public void write_uint64 (uint64 offset, uint64 val) {
			for (uint i = 0; i != 8; i++)
				data[offset + i] = (uint8) (val >> (i * 8));
		}

		public uint8[] steal () {
			uint8[] result = data;
			data = new uint8[SIZE];
			return result;
		}
	}

	private enum TargetArch {
		IA32,
		X64,
		ARM
	}

	private class ArmSystemState {
		public uint64 sctlr;
		public uint64 ttbcr;
		public uint64 ttbr0;
		public uint64 ttbr1;

		public ArmSystemState (uint64 sctlr, uint64 ttbcr, uint64 ttbr0, uint64 ttbr1) {
			this.sctlr = sctlr;
			this.ttbcr = ttbcr;
			this.ttbr0 = ttbr0;
			this.ttbr1 = ttbr1;
		}
	}

	private enum ControlRegisterExposure {
		HIDE_CONTROL_REGISTERS,
		EXPOSE_CONTROL_REGISTERS
	}

	/**
	 * A minimal i386 GDB stub whose entire address space is a flat block of RAM, so that the
	 * machine's "physical" reads land on the page tables we planted.
	 */
	private class FakeTarget : Object {
		public GDB.Client? client {
			get;
			private set;
		}

		private TargetArch arch;
		private uint8[] ram;
		private Gee.List<Region> regions = new Gee.ArrayList<Region> ();
		private string? monitor_dump;
		private ControlRegisterExposure exposure;
		private ArmSystemState? arm_state;

		private class Region {
			public uint64 address;
			public Bytes data;
		}

		private SocketService service;
		private Cancellable cancellable = new Cancellable ();

		private const string IA32_CORE_REGISTERS =
			"<reg name=\"eax\" bitsize=\"32\" regnum=\"0\"/>" +
			"<reg name=\"ecx\" bitsize=\"32\" regnum=\"1\"/>" +
			"<reg name=\"edx\" bitsize=\"32\" regnum=\"2\"/>" +
			"<reg name=\"ebx\" bitsize=\"32\" regnum=\"3\"/>" +
			"<reg name=\"esp\" bitsize=\"32\" regnum=\"4\"/>" +
			"<reg name=\"ebp\" bitsize=\"32\" regnum=\"5\"/>" +
			"<reg name=\"esi\" bitsize=\"32\" regnum=\"6\"/>" +
			"<reg name=\"edi\" bitsize=\"32\" regnum=\"7\"/>" +
			"<reg name=\"eip\" bitsize=\"32\" regnum=\"8\"/>" +
			"<reg name=\"eflags\" bitsize=\"32\" regnum=\"9\"/>";

		private const string X64_CORE_REGISTERS =
			"<reg name=\"rax\" bitsize=\"64\" regnum=\"0\"/>" +
			"<reg name=\"rbx\" bitsize=\"64\" regnum=\"1\"/>" +
			"<reg name=\"rcx\" bitsize=\"64\" regnum=\"2\"/>" +
			"<reg name=\"rdx\" bitsize=\"64\" regnum=\"3\"/>" +
			"<reg name=\"rsi\" bitsize=\"64\" regnum=\"4\"/>" +
			"<reg name=\"rdi\" bitsize=\"64\" regnum=\"5\"/>" +
			"<reg name=\"rbp\" bitsize=\"64\" regnum=\"6\"/>" +
			"<reg name=\"rsp\" bitsize=\"64\" regnum=\"7\"/>" +
			"<reg name=\"rip\" bitsize=\"64\" regnum=\"8\"/>";

		private const string ARM_CORE_REGISTERS =
			"<reg name=\"r0\" bitsize=\"32\" regnum=\"0\"/>" +
			"<reg name=\"r1\" bitsize=\"32\" regnum=\"1\"/>" +
			"<reg name=\"r2\" bitsize=\"32\" regnum=\"2\"/>" +
			"<reg name=\"r3\" bitsize=\"32\" regnum=\"3\"/>" +
			"<reg name=\"r4\" bitsize=\"32\" regnum=\"4\"/>" +
			"<reg name=\"r5\" bitsize=\"32\" regnum=\"5\"/>" +
			"<reg name=\"r6\" bitsize=\"32\" regnum=\"6\"/>" +
			"<reg name=\"r7\" bitsize=\"32\" regnum=\"7\"/>" +
			"<reg name=\"r8\" bitsize=\"32\" regnum=\"8\"/>" +
			"<reg name=\"r9\" bitsize=\"32\" regnum=\"9\"/>" +
			"<reg name=\"r10\" bitsize=\"32\" regnum=\"10\"/>" +
			"<reg name=\"r11\" bitsize=\"32\" regnum=\"11\"/>" +
			"<reg name=\"r12\" bitsize=\"32\" regnum=\"12\"/>" +
			"<reg name=\"sp\" bitsize=\"32\" regnum=\"13\"/>" +
			"<reg name=\"lr\" bitsize=\"32\" regnum=\"14\"/>" +
			"<reg name=\"pc\" bitsize=\"32\" regnum=\"15\"/>" +
			"<reg name=\"cpsr\" bitsize=\"32\" regnum=\"16\"/>";

		private const string ARM_SYSTEM_REGISTERS =
			"<reg name=\"TTBCR\" bitsize=\"32\" regnum=\"17\"/>" +
			"<reg name=\"TTBR0\" bitsize=\"32\" regnum=\"18\"/>" +
			"<reg name=\"TTBR1\" bitsize=\"32\" regnum=\"19\"/>" +
			"<reg name=\"SCTLR\" bitsize=\"32\" regnum=\"20\"/>";

		private const string CONTROL_REGISTERS =
			"<reg name=\"cr0\" bitsize=\"32\" regnum=\"10\"/>" +
			"<reg name=\"cr3\" bitsize=\"32\" regnum=\"11\"/>" +
			"<reg name=\"cr4\" bitsize=\"32\" regnum=\"12\"/>";

		public FakeTarget (TargetArch arch, owned uint8[] ram, string? monitor_dump,
				ControlRegisterExposure exposure = HIDE_CONTROL_REGISTERS, ArmSystemState? arm_state = null) {
			this.arch = arch;
			this.ram = (owned) ram;
			this.monitor_dump = monitor_dump;
			this.exposure = exposure;
			this.arm_state = arm_state;
		}

		public async void open () throws Error, IOError {
			service = new SocketService ();
			uint16 port;
			try {
				port = service.add_any_inet_port (null);
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}

			service.incoming.connect ((connection) => {
				serve.begin (connection);
				return true;
			});
			service.start ();

			IOStream stream;
			var socket_client = new SocketClient ();
			try {
				stream = yield socket_client.connect_to_host_async ("127.0.0.1", port, cancellable);
			} catch (GLib.Error e) {
				throw new Error.TRANSPORT ("%s", e.message);
			}

			client = yield GDB.Client.open (stream, cancellable);
		}

		public void stop () {
			cancellable.cancel ();
			if (service != null)
				service.stop ();
		}

		public void map_virtual (uint64 address, Bytes data) {
			regions.add (new Region () {
				address = address,
				data = data,
			});
		}

		public uint32 read_uint32 (uint64 address) {
			uint32 result = 0;
			for (uint i = 0; i != 4; i++)
				result |= ((uint32) ram[address + i]) << (i * 8);
			return result;
		}

		public uint64 read_uint64 (uint64 address) {
			uint64 result = 0;
			for (uint i = 0; i != 8; i++)
				result |= ((uint64) ram[address + i]) << (i * 8);
			return result;
		}

		private async void serve (IOStream connection) {
			var input = connection.get_input_stream ();
			var output = connection.get_output_stream ();
			try {
				string? request;
				while ((request = yield read_packet (input)) != null) {
					yield write_ack (output);
					foreach (string reply in compute_replies (request))
						yield write_packet (output, reply);
				}
			} catch (GLib.Error e) {
			}
		}

		private string[] compute_replies (string request) {
			if (request.has_prefix ("qSupported"))
				return { "PacketSize=1000;qXfer:features:read+" };
			if (request.has_prefix ("qXfer:features:read:target.xml:"))
				return { "l" + target_xml () };
			if (request == "qAttached")
				return { "1" };
			if (request == "?")
				return { "T05thread:1;" };
			if (request == "qC")
				return { "QC1" };
			// A no-op on a flat address space, but the machine insists the stub can do it.
			if (request == "qqemu.PhyMemMode")
				return { "0" };
			if (request.has_prefix ("Qqemu.PhyMemMode:"))
				return { "OK" };
			if (request[0] == 'H')
				return { "OK" };
			if (request[0] == 'p')
				return { read_register (request) };
			if (request[0] == 'm')
				return { read_memory (request) };
			if (request[0] == 'M')
				return { write_memory (request) };
			if (request.has_prefix ("qRcmd,"))
				return run_remote_command (request);
			return { "" };
		}

		private string target_xml () {
			if (arch == ARM) {
				return "<?xml version=\"1.0\"?>" +
					"<target version=\"1.0\">" +
					"<architecture>arm</architecture>" +
					"<feature name=\"org.gnu.gdb.arm.core\">" +
					ARM_CORE_REGISTERS +
					"</feature>" +
					"<feature name=\"org.qemu.gdb.arm.sys.regs\">" +
					ARM_SYSTEM_REGISTERS +
					"</feature>" +
					"</target>";
			}

			var registers = new StringBuilder ((arch == X64) ? X64_CORE_REGISTERS : IA32_CORE_REGISTERS);
			if (exposure == EXPOSE_CONTROL_REGISTERS)
				registers.append (CONTROL_REGISTERS);

			return "<?xml version=\"1.0\"?>" +
				"<target version=\"1.0\">" +
				"<architecture>" + ((arch == X64) ? "i386:x86-64" : "i386") + "</architecture>" +
				"<feature name=\"org.gnu.gdb.i386.core\">" +
				registers.str +
				"</feature>" +
				"</target>";
		}

		private string read_register (string request) {
			uint regnum = uint.parse (request[1:].split (";")[0], 16);

			if (arch == ARM) {
				uint64 arm_val = 0;
				switch (regnum) {
					case 17:
						arm_val = arm_state.ttbcr;
						break;
					case 18:
						arm_val = arm_state.ttbr0;
						break;
					case 19:
						arm_val = arm_state.ttbr1;
						break;
					case 20:
						arm_val = arm_state.sctlr;
						break;
					default:
						break;
				}

				var arm_result = new StringBuilder ();
				for (uint i = 0; i != 4; i++)
					arm_result.append_printf ("%02x", (uint8) (arm_val >> (i * 8)));
				return arm_result.str;
			}

			uint num_core_registers = (arch == X64) ? 9 : 10;
			size_t width = (arch == X64) ? 8 : 4;

			uint64 val = 0;
			if (regnum >= num_core_registers) {
				if (exposure != EXPOSE_CONTROL_REGISTERS)
					return "E01";

				switch (regnum - num_core_registers) {
					case 0:
						val = 0x8005003b;
						break;
					case 1:
						val = PD_PA;
						break;
					case 2:
						val = 0x00000010;
						break;
					default:
						return "E01";
				}
			}

			var result = new StringBuilder ();
			for (uint i = 0; i != width; i++)
				result.append_printf ("%02x", (uint8) (val >> (i * 8)));
			return result.str;
		}

		private string read_memory (string request) {
			string[] tokens = request[1:].split (",");
			uint64 address = uint64.parse (tokens[0], 16);
			size_t size = (size_t) uint64.parse (tokens[1], 16);

			uint8[]? data = read_span (address, size);
			if (data == null)
				return "E01";

			var result = new StringBuilder ();
			foreach (uint8 b in data)
				result.append_printf ("%02x", b);
			return result.str;
		}

		private uint8[]? read_span (uint64 address, size_t size) {
			foreach (Region r in regions) {
				unowned uint8[] mapped = r.data.get_data ();
				if (address >= r.address && address + size <= r.address + mapped.length) {
					size_t start = (size_t) (address - r.address);
					return mapped[start:start + size];
				}
			}

			if (address + size > ram.length)
				return null;
			return ram[(size_t) address:(size_t) address + size];
		}

		private string write_memory (string request) {
			string[] tokens = request[1:].split (":");
			string[] location = tokens[0].split (",");
			uint64 address = uint64.parse (location[0], 16);
			size_t size = (size_t) uint64.parse (location[1], 16);
			if (address + size > ram.length)
				return "E01";

			unowned string payload = tokens[1];
			for (size_t i = 0; i != size; i++)
				ram[address + i] = (uint8) uint.parse (payload[(long) (i * 2):(long) ((i * 2) + 2)], 16);
			return "OK";
		}

		private string[] run_remote_command (string request) {
			if (monitor_dump == null)
				return { "" };

			var output = new StringBuilder ("O");
			foreach (uint8 byte in monitor_dump.data)
				output.append_printf ("%02x", byte);
			return { output.str, "OK" };
		}

		private async string? read_packet (InputStream input) throws GLib.Error {
			while (true) {
				int c = yield read_byte (input);
				if (c < 0)
					return null;
				if (c != '$')
					continue;

				var payload = new StringBuilder ();
				while (true) {
					c = yield read_byte (input);
					if (c < 0)
						return null;
					if (c == '#')
						break;
					payload.append_c ((char) c);
				}
				yield read_byte (input);
				yield read_byte (input);
				return payload.str;
			}
		}

		private async int read_byte (InputStream input) throws GLib.Error {
			uint8 buf[1];
			size_t n;
			yield input.read_all_async (buf, Priority.DEFAULT, cancellable, out n);
			if (n == 0)
				return -1;
			return buf[0];
		}

		private async void write_ack (OutputStream output) throws GLib.Error {
			yield write_all (output, "+".data);
		}

		private async void write_packet (OutputStream output, string payload) throws GLib.Error {
			uint8 checksum = 0;
			for (int i = 0; i != payload.length; i++)
				checksum += (uint8) payload[i];
			var frame = "$%s#%02x".printf (payload, checksum);
			yield write_all (output, frame.data);
		}

		private async void write_all (OutputStream output, uint8[] data) throws GLib.Error {
			size_t written;
			yield output.write_all_async (data, Priority.DEFAULT, cancellable, out written);
		}
	}

	private Barebone.RangeDetails? find_range_containing (Gee.List<Barebone.RangeDetails> ranges, uint64 va) {
		int lo = 0;
		int hi = ranges.size - 1;
		while (lo <= hi) {
			int mid = (lo + hi) / 2;
			Barebone.RangeDetails r = ranges[mid];
			if (va < r.base_va)
				hi = mid - 1;
			else if (va >= r.end)
				lo = mid + 1;
			else
				return r;
		}
		return null;
	}

	private Gee.List<Interval> merge_by_virtual_address (Gee.List<Barebone.RangeDetails> ranges) {
		var result = new Gee.ArrayList<Interval> ();

		foreach (Barebone.RangeDetails r in ranges) {
			if (result.size != 0 && result[result.size - 1].end == r.base_va) {
				result[result.size - 1].end = r.end;
				continue;
			}
			result.add (new Interval (r.base_va, r.end));
		}

		return result;
	}

	private void assert_intervals_equal (Gee.List<Interval> actual, Gee.List<Interval> expected) {
		assert_true (actual.size == expected.size);
		for (int i = 0; i != actual.size; i++) {
			assert_true (actual[i].start == expected[i].start);
			assert_true (actual[i].end == expected[i].end);
		}
	}

	// A class rather than a struct: the merging below updates entries in place, and a Gee list
	// hands back copies of struct elements.
	private class Interval {
		public uint64 start;
		public uint64 end;

		public Interval (uint64 start, uint64 end) {
			this.start = start;
			this.end = end;
		}
	}

	private enum GuestArch {
		X86,
		X86_64,
		ARM;

		public string to_nick () {
			switch (this) {
				case X86:
					return "x86";
				case X86_64:
					return "x86_64";
				default:
					return "arm";
			}
		}
	}

	private class GuestPage {
		public uint64 va;
		public uint64 pa;
		public bool writable;
		public bool no_execute;
		public bool large;
	}

	/**
	 * A QEMU guest with the GDB stub attached, plus the monitor commands that let us check our
	 * own answers against QEMU's view of the very same tables.
	 */
	private class QemuGuest : Object {
		public GDB.Client client {
			get;
			private set;
		}

		private Subprocess process;
		private Cancellable cancellable = new Cancellable ();

		private const uint CONNECT_TIMEOUT_MSEC = 10000;
		private const uint BOOT_TIMEOUT_SEC = 180;

		public static async string? check_availability (GuestArch arch) {
			try {
				var checker = new Subprocess (SubprocessFlags.STDOUT_SILENCE | SubprocessFlags.STDERR_PIPE,
					"python3", script_path (), "check-" + arch.to_nick ());

				string stderr_buf;
				yield checker.communicate_utf8_async (null, null, null, out stderr_buf);
				if (checker.get_exit_status () == 0)
					return null;

				return (stderr_buf != null) ? stderr_buf.strip () : "unable to prepare the guest";
			} catch (GLib.Error e) {
				return e.message;
			}
		}

		public const uint MEMORY_SIZE_IN_MB = 256;

		public static async QemuGuest? boot (GuestArch arch) throws Error, IOError {
			uint16 port = pick_unused_port ();

			Subprocess process;
			try {
				process = new Subprocess (SubprocessFlags.STDOUT_PIPE,
					"python3", script_path (), "boot-" + arch.to_nick (), "--gdb-port",
					port.to_string (), "--memory", MEMORY_SIZE_IN_MB.to_string ());
			} catch (GLib.Error e) {
				return null;
			}

			var guest = new QemuGuest ();
			guest.process = process;

			if (!yield guest.wait_until_booted ()) {
				guest.stop ();
				return null;
			}

			IOStream? stream = yield guest.connect_to_stub (port);
			if (stream == null) {
				guest.stop ();
				return null;
			}

			// Attaching is what pauses the guest, and it stays paused for the rest of the test.
			guest.client = yield GDB.Client.open (stream, guest.cancellable);

			return guest;
		}

		public void stop () {
			cancellable.cancel ();
			process.force_exit ();
		}

		private async IOStream? connect_to_stub (uint16 port) throws Error, IOError {
			var timer = new Timer ();
			var socket_client = new SocketClient ();

			do {
				try {
					return yield socket_client.connect_to_host_async ("127.0.0.1", port, cancellable);
				} catch (GLib.Error e) {
					if (e is IOError.CANCELLED)
						throw (IOError) e;
				}

				yield sleep (250);
			} while ((uint) (timer.elapsed () * 1000.0) < CONNECT_TIMEOUT_MSEC);

			return null;
		}

		private async bool wait_until_booted () throws IOError {
			var input = new DataInputStream (process.get_stdout_pipe ());

			var timeout_source = new TimeoutSource.seconds (BOOT_TIMEOUT_SEC);
			timeout_source.set_callback (() => {
				cancellable.cancel ();
				return false;
			});
			timeout_source.attach (MainContext.get_thread_default ());

			try {
				string? line = yield input.read_line_async (Priority.DEFAULT, cancellable);
				return line != null && line.strip () == "ready";
			} catch (GLib.Error e) {
				return false;
			} finally {
				timeout_source.destroy ();
			}
		}

		public async Gee.List<Interval> query_mapped_intervals () throws Error, IOError {
			var result = new Gee.ArrayList<Interval> ();

			// E.g.: 0000000000000000-0000800000000000 0000800000000000 -rw
			string dump = yield client.run_remote_command ("info mem", cancellable);
			foreach (string line in dump.split ("\n")) {
				string[] tokens = tokenize (line);
				if (tokens.length != 3)
					continue;

				string[] bounds = tokens[0].split ("-");
				if (bounds.length != 2)
					continue;

				var interval = new Interval (uint64.parse (bounds[0], 16), uint64.parse (bounds[1], 16));

				if (result.size != 0 && result[result.size - 1].end == interval.start) {
					result[result.size - 1].end = interval.end;
					continue;
				}
				result.add (interval);
			}

			return result;
		}

		public async Gee.List<GuestPage> query_pages () throws Error, IOError {
			var result = new Gee.ArrayList<GuestPage> ();

			// E.g.: 00000000d07ea000: 0000000001150000 -G--A----
			string dump = yield client.run_remote_command ("info tlb", cancellable);
			foreach (string line in dump.split ("\n")) {
				string[] tokens = tokenize (line);
				if (tokens.length != 3 || !tokens[0].has_suffix (":"))
					continue;
				unowned string flags = tokens[2];
				if (flags.length != NUM_PAGE_FLAGS)
					continue;

				result.add (new GuestPage () {
					va = uint64.parse (tokens[0][0:-1], 16),
					pa = uint64.parse (tokens[1], 16),
					writable = flags[WRITABLE_FLAG] == 'W',
					no_execute = flags[NO_EXECUTE_FLAG] == 'X',
					large = flags[LARGE_PAGE_FLAG] == 'P'
				});
			}

			return result;
		}

		public async GuestPage query_page (uint64 va) throws Error, IOError {
			foreach (GuestPage page in yield query_pages ()) {
				if (page.va == va)
					return page;
			}

			throw new Error.INVALID_ARGUMENT ("Guest no longer maps 0x%08x", (uint32) va);
		}

		private static string[] tokenize (string text) {
			var result = new Gee.ArrayList<string> ();
			foreach (string token in text.split_set (" \t\r\n")) {
				if (token.length != 0)
					result.add (token);
			}
			return result.to_array ();
		}

		private static string script_path () {
			return Path.build_filename (TESTS_SRCDIR, "vm.py");
		}

		private static uint16 pick_unused_port () throws Error {
			var service = new SocketService ();
			try {
				uint16 port = service.add_any_inet_port (null);
				service.stop ();
				return port;
			} catch (GLib.Error e) {
				throw new Error.NOT_SUPPORTED ("%s", e.message);
			}
		}

		private async void sleep (uint msec) {
			var source = new TimeoutSource (msec);
			source.set_callback (sleep.callback);
			source.attach (MainContext.get_thread_default ());
			yield;
		}

		private const int NUM_PAGE_FLAGS = 9;
		private const int NO_EXECUTE_FLAG = 0;
		private const int LARGE_PAGE_FLAG = 2;
		private const int WRITABLE_FLAG = 8;
	}

	[CCode (cname = "FRIDA_TESTS_SRCDIR")]
	private extern const string TESTS_SRCDIR;

	private class Harness : Frida.Test.AsyncHarness {
		public Harness (owned Frida.Test.AsyncHarness.TestSequenceFunc func) {
			base ((owned) func);
		}
	}

	// The first run downloads the kernel before it can boot the guest.
	private class SlowHarness : Harness {
		public SlowHarness (owned Frida.Test.AsyncHarness.TestSequenceFunc func) {
			base ((owned) func);
		}

		protected override uint provide_timeout () {
			return 900;
		}
	}
}
