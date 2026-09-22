/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2015 Marc Hartmayer <hello@hartmayer.com>
 * Copyright (C) 2020-2021 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2020 Marcus Mengs <mame8282@googlemail.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 * Copyright (C) 2023 Grant Douglas <me@hexplo.it>
 * Copyright (C) 2024 Hillel Pinto <hillelpinto3@gmail.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2024 Simon Zuckerbraun <Simon_Zuckerbraun@trendmicro.com>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "script-fixture.c"

TESTLIST_BEGIN (script)
  TESTENTRY (invalid_script_should_return_null)
  TESTENTRY (strict_mode_should_be_enforced)
  TESTENTRY (array_buffer_can_be_created)
  TESTENTRY (message_can_be_sent)
  TESTENTRY (message_can_be_sent_with_data)
  TESTENTRY (message_can_be_received)
  TESTENTRY (message_can_be_received_with_data)
  TESTENTRY (recv_may_specify_desired_message_type)
  TESTENTRY (recv_can_be_waited_for_from_an_application_thread)
  TESTENTRY (recv_can_be_waited_for_from_two_application_threads)
  TESTENTRY (recv_wait_in_an_application_thread_should_not_deadlock)
  TESTENTRY (recv_can_be_waited_for_from_our_js_thread)
  TESTENTRY (recv_wait_in_an_application_thread_should_throw_on_unload)
  TESTENTRY (recv_wait_in_our_js_thread_should_throw_on_unload)
  TESTENTRY (recv_wait_should_not_leak)
  TESTENTRY (message_can_be_logged)
  TESTENTRY (timeout_can_be_scheduled)
  TESTENTRY (timeout_can_be_cancelled)
  TESTENTRY (interval_can_be_scheduled)
  TESTENTRY (interval_can_be_cancelled)
  TESTENTRY (callback_can_be_scheduled)
  TESTENTRY (callback_can_be_scheduled_from_a_scheduled_callback)
  TESTENTRY (callback_can_be_cancelled)
  TESTENTRY (callback_can_be_scheduled_on_next_tick)
  TESTENTRY (timer_cancellation_apis_should_be_forgiving)
#ifndef HAVE_WINDOWS
  TESTENTRY (crash_on_thread_holding_js_lock_should_not_deadlock)
  TESTENTRY (recovered_crash_should_not_unbalance_interceptor)
#endif

  TESTENTRY (script_can_be_interrupted)
  TESTENTRY (script_interrupt_is_idempotent)
  TESTENTRY (script_can_be_terminated)
  TESTENTRY (script_interrupt_while_idle_is_ignored)

  TESTGROUP_BEGIN ("Thread")
    TESTENTRY (thread_can_be_forced_to_sleep)
    TESTENTRY (thread_backtrace_can_be_captured_with_limit)
    TESTENTRY (hardware_breakpoint_can_be_set)
    TESTENTRY (hardware_watchpoint_can_be_set)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("RPC")
    TESTENTRY (method_can_be_called_sync)
    TESTENTRY (method_can_be_called_async)
    TESTENTRY (method_can_throw_sync)
    TESTENTRY (method_can_throw_async)
    TESTENTRY (method_can_return_null)
    TESTENTRY (method_can_receive_binary_data)
    TESTENTRY (method_can_return_binary_data)
    TESTENTRY (method_can_return_value_and_binary_data)
    TESTENTRY (method_list_can_be_queried)
    TESTENTRY (calling_inexistent_method_should_throw_error)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("WeakRef")
    TESTENTRY (weak_ref_api_should_be_supported)
    TESTENTRY (weak_callback_is_triggered_on_gc)
    TESTENTRY (weak_callback_is_triggered_on_unload)
    TESTENTRY (weak_callback_is_triggered_on_unbind)
    TESTENTRY (weak_callback_should_not_be_exclusive)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Interceptor")
    TESTENTRY (argument_can_be_read)
    TESTENTRY (argument_can_be_replaced)
    TESTENTRY (return_value_can_be_read)
    TESTENTRY (return_value_can_be_replaced)
    TESTENTRY (return_address_can_be_read)
    TESTENTRY (general_purpose_register_can_be_read)
    TESTENTRY (general_purpose_register_can_be_written)
    TESTENTRY (vector_register_can_be_read)
    TESTENTRY (double_register_can_be_read)
    TESTENTRY (float_register_can_be_read)
    TESTENTRY (status_register_can_be_read)
    TESTENTRY (system_error_can_be_read_from_interceptor_listener)
    TESTENTRY (system_error_can_be_read_from_replacement_function)
    TESTENTRY (system_error_can_be_replaced_from_interceptor_listener)
    TESTENTRY (system_error_can_be_replaced_from_replacement_function)
    TESTENTRY (system_error_unaffected_by_replacement_if_set_to_original_value)
    TESTENTRY (system_error_unaffected_by_replacement_if_untouched)
    TESTENTRY (invocations_are_bound_on_tls_object)
    TESTENTRY (invocations_provide_thread_id)
    TESTENTRY (invocations_provide_call_depth)
#ifndef HAVE_MIPS
    TESTENTRY (invocations_provide_context_for_backtrace)
#endif
    TESTENTRY (invocations_provide_context_serializable_to_json)
    TESTENTRY (listener_can_be_detached)
    TESTENTRY (listener_can_be_detached_by_destruction_mid_call)
    TESTENTRY (all_listeners_can_be_detached)
    TESTENTRY (function_can_be_replaced)
    TESTENTRY (function_can_be_replaced_and_called_immediately)
    TESTENTRY (function_can_be_reverted)
    TESTENTRY (replaced_function_should_have_invocation_context)
    TESTENTRY (instructions_can_be_probed)
    TESTENTRY (interceptor_should_support_native_pointer_values)
    TESTENTRY (interceptor_should_support_instrumentation_options)
    TESTENTRY (interceptor_should_support_custom_redirect)
    TESTENTRY (interceptor_should_support_default_options)
    TESTENTRY (interceptor_should_handle_bad_pointers)
    TESTENTRY (interceptor_should_refuse_to_attach_without_any_callbacks)
#ifdef HAVE_DARWIN
    TESTENTRY (interceptor_and_js_should_not_deadlock)
#endif
  TESTGROUP_END ()
  TESTGROUP_BEGIN ("Interceptor/Fast")
    TESTENTRY (function_can_be_replaced_fast)
    TESTENTRY (function_can_be_replaced_fast_and_called_immediately)
    TESTENTRY (function_can_be_reverted_fast)
    TESTENTRY (interceptor_should_support_native_pointer_values_fast)
    TESTENTRY (interceptor_should_handle_bad_pointers_fast)
    TESTENTRY (function_can_be_replaced_and_call_original_fast)
    TESTENTRY (function_can_be_replaced_fast_performance)
    TESTENTRY (function_can_be_replaced_and_call_original_fast_performance)
  TESTGROUP_END ()
  TESTGROUP_BEGIN ("Interceptor/Performance")
    TESTENTRY (interceptor_on_enter_performance)
    TESTENTRY (interceptor_on_leave_performance)
    TESTENTRY (interceptor_on_enter_and_leave_performance)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Memory")
    TESTENTRY (pointer_can_be_read)
    TESTENTRY (pointer_can_be_written)
    TESTENTRY (memory_can_be_allocated_with_byte_granularity)
    TESTENTRY (memory_can_be_allocated_with_page_granularity)
    TESTENTRY (memory_can_be_allocated_near_address)
    TESTENTRY (memory_can_be_copied)
    TESTENTRY (memory_can_be_duped)
    TESTENTRY (memory_can_be_protected)
    TESTENTRY (memory_protection_can_be_queried)
    TESTENTRY (code_can_be_patched)
    TESTENTRY (utf8_string_can_be_allocated)
    TESTENTRY (utf16_string_can_be_allocated)
#ifdef HAVE_WINDOWS
    TESTENTRY (ansi_string_can_be_allocated_in_code_page_936)
    TESTENTRY (ansi_string_can_be_allocated_in_code_page_1252)
#endif
    TESTENTRY (memory_can_be_scanned_with_pattern_string)
    TESTENTRY (memory_can_be_scanned_with_match_pattern_object)
    TESTENTRY (memory_can_be_scanned_synchronously)
    TESTENTRY (memory_can_be_scanned_asynchronously)
    TESTENTRY (memory_scan_should_be_interruptible)
    TESTENTRY (memory_scan_handles_unreadable_memory)
    TESTENTRY (memory_scan_handles_bad_arguments)
    TESTENTRY (memory_pointers_can_be_found)
    TESTENTRY (memory_access_can_be_monitored)
    TESTENTRY (memory_access_can_be_monitored_one_range)
    TESTENTRY (memory_access_monitor_provides_thread_id)
    TESTENTRY (memory_access_monitor_provides_cpu_context)
    TESTENTRY (memory_access_monitor_cpu_context_can_be_modified)
  TESTGROUP_END ()

  TESTENTRY (frida_version_is_available)
  TESTENTRY (frida_heap_size_can_be_queried)

  TESTGROUP_BEGIN ("Process")
    TESTENTRY (process_arch_is_available)
    TESTENTRY (process_platform_is_available)
    TESTENTRY (process_page_size_is_available)
    TESTENTRY (process_pointer_size_is_available)
    TESTENTRY (process_should_support_nested_signal_handling)
    TESTENTRY (process_current_dir_can_be_queried)
    TESTENTRY (process_home_dir_can_be_queried)
    TESTENTRY (process_tmp_dir_can_be_queried)
    TESTENTRY (process_debugger_status_is_available)
    TESTENTRY (process_id_is_available)
    TESTENTRY (process_current_thread_id_is_available)
    TESTENTRY (process_thread_can_be_looked_up_by_id)
    TESTENTRY (process_threads_can_be_enumerated)
    TESTENTRY (process_threads_have_names)
    TESTENTRY (process_modules_can_be_enumerated)
    TESTENTRY (process_modules_can_be_observed)
    TESTENTRY (process_module_can_be_looked_up_from_address)
    TESTENTRY (process_module_can_be_looked_up_from_name)
    TESTENTRY (process_ranges_can_be_enumerated)
    TESTENTRY (process_ranges_can_be_enumerated_with_neighbors_coalesced)
    TESTENTRY (process_range_can_be_looked_up_from_address)
    TESTENTRY (process_function_range_can_be_looked_up_from_address)
    TESTENTRY (process_system_ranges_can_be_enumerated)
#ifdef HAVE_DARWIN
    TESTENTRY (process_malloc_ranges_can_be_enumerated)
#endif
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("RunOnThread")
    TESTENTRY (process_can_run_on_thread_with_success)
    TESTENTRY (process_can_run_on_thread_with_failure)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("ControlFlowGraph")
    TESTENTRY (cfg_can_be_built)
    TESTENTRY (cfg_can_be_serialized)
    TESTENTRY (cfg_dominating_sites_can_be_enumerated)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Module")
    TESTENTRY (module_imports_can_be_enumerated)
    TESTENTRY (module_exports_can_be_enumerated)
    TESTENTRY (module_exports_enumeration_performance)
    TESTENTRY (module_symbols_can_be_enumerated)
    TESTENTRY (module_ranges_can_be_enumerated)
    TESTENTRY (module_sections_can_be_enumerated)
    TESTENTRY (module_dependencies_can_be_enumerated)
    TESTENTRY (module_export_can_be_found_by_name)
    TESTENTRY (module_symbol_can_be_found_by_name)
    TESTENTRY (module_can_be_loaded)
    TESTENTRY (module_can_be_forcibly_initialized)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("ModuleMap")
    TESTENTRY (module_map_values_should_have_module_prototype)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("ApiResolver")
    TESTENTRY (api_resolver_can_be_used_to_find_functions)
    TESTENTRY (api_resolver_can_be_used_to_find_sections)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Socket")
    TESTENTRY (socket_connection_can_be_established)
    TESTENTRY (socket_connection_can_be_established_with_tls)
    TESTENTRY (socket_connection_should_not_leak_on_error)
    TESTENTRY (socket_type_can_be_inspected)
    TESTENTRY (socket_endpoints_can_be_inspected)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Stream")
#ifdef G_OS_UNIX
    TESTENTRY (unix_fd_can_be_read_from)
    TESTENTRY (unix_fd_can_be_written_to)
#endif
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Hexdump")
    TESTENTRY (basic_hexdump_functionality_is_available)
    TESTENTRY (hexdump_supports_native_pointer_conforming_object)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("NativePointer")
    TESTENTRY (native_pointer_provides_is_null)
    TESTENTRY (native_pointer_provides_arithmetic_operations)
    TESTENTRY (native_pointer_provides_uint32_conversion_functionality)
    TESTENTRY (native_pointer_provides_ptrauth_functionality)
    TESTENTRY (native_pointer_provides_arm_tbi_functionality)
    TESTENTRY (native_pointer_to_match_pattern)
    TESTENTRY (native_pointer_can_be_constructed_from_64bit_value)
    TESTENTRY (native_pointer_should_be_serializable_to_json)
    TESTENTRY (s8_can_be_read)
    TESTENTRY (s8_can_be_written)
    TESTENTRY (u8_can_be_read)
    TESTENTRY (u8_can_be_written)
    TESTENTRY (s16_can_be_read)
    TESTENTRY (s16_can_be_written)
    TESTENTRY (u16_can_be_read)
    TESTENTRY (u16_can_be_written)
    TESTENTRY (s32_can_be_read)
    TESTENTRY (s32_can_be_written)
    TESTENTRY (u32_can_be_read)
    TESTENTRY (u32_can_be_written)
    TESTENTRY (s64_can_be_read)
    TESTENTRY (s64_can_be_written)
    TESTENTRY (u64_can_be_read)
    TESTENTRY (u64_can_be_written)
    TESTENTRY (short_can_be_read)
    TESTENTRY (short_can_be_written)
    TESTENTRY (ushort_can_be_read)
    TESTENTRY (ushort_can_be_written)
    TESTENTRY (int_can_be_read)
    TESTENTRY (int_can_be_written)
    TESTENTRY (uint_can_be_read)
    TESTENTRY (uint_can_be_written)
    TESTENTRY (long_can_be_read)
    TESTENTRY (long_can_be_written)
    TESTENTRY (ulong_can_be_read)
    TESTENTRY (ulong_can_be_written)
    TESTENTRY (float_can_be_read)
    TESTENTRY (float_can_be_written)
    TESTENTRY (double_can_be_read)
    TESTENTRY (double_can_be_written)
    TESTENTRY (byte_array_can_be_read)
    TESTENTRY (byte_array_can_be_written)
    TESTENTRY (c_string_can_be_read)
    TESTENTRY (utf8_string_can_be_read)
    TESTENTRY (utf8_string_can_be_written)
    TESTENTRY (utf16_string_can_be_read)
    TESTENTRY (utf16_string_can_be_written)
#ifdef HAVE_WINDOWS
    TESTENTRY (ansi_string_can_be_read_in_code_page_936)
    TESTENTRY (ansi_string_can_be_read_in_code_page_1252)
    TESTENTRY (ansi_string_can_be_written_in_code_page_936)
    TESTENTRY (ansi_string_can_be_written_in_code_page_1252)
#endif
    TESTENTRY (read_from_unaccessible_memory_can_be_performed_safely)
    TESTENTRY (write_to_unaccessible_memory_can_be_performed_safely)
    TESTENTRY (invalid_read_results_in_exception)
    TESTENTRY (invalid_write_results_in_exception)
    TESTENTRY (invalid_read_write_execute_results_in_exception)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("ArrayBuffer")
    TESTENTRY (array_buffer_can_wrap_memory_region)
    TESTENTRY (array_buffer_can_be_unwrapped)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("UInt64")
    TESTENTRY (uint64_provides_arithmetic_operations)
    TESTENTRY (uint64_can_be_constructed_from_a_large_number)
    TESTENTRY (uint64_can_be_converted_to_a_large_number)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Int64")
    TESTENTRY (int64_provides_arithmetic_operations)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("NativeFunction")
    TESTENTRY (native_function_can_be_invoked)
    TESTENTRY (native_function_can_be_invoked_with_size_t)
    TESTENTRY (native_function_can_be_intercepted_when_thread_is_ignored)
    TESTENTRY (native_function_can_not_be_intercepted_when_traps_are_none)
    TESTENTRY (native_function_should_implement_call_and_apply)
    TESTENTRY (native_function_crash_results_in_exception)
    TESTENTRY (nested_native_function_crash_is_handled_gracefully)
    TESTENTRY (variadic_native_function_can_be_invoked)
    TESTENTRY (
        variadic_native_function_args_smaller_than_int_should_be_promoted)
    TESTENTRY (variadic_native_function_float_args_should_be_promoted_to_double)
#if defined (HAVE_WINDOWS) && GLIB_SIZEOF_VOID_P == 4
    TESTENTRY (native_function_should_support_fastcall)
    TESTENTRY (native_function_should_support_stdcall)
#endif
    TESTENTRY (native_function_is_a_native_pointer)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("SystemFunction")
    TESTENTRY (system_function_can_be_invoked)
    TESTENTRY (system_function_should_implement_call_and_apply)
    TESTENTRY (system_function_is_a_native_pointer)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("NativeCallback")
    TESTENTRY (native_callback_can_be_invoked)
    TESTENTRY (native_callback_should_provide_access_to_system_error)
    TESTENTRY (native_callback_is_a_native_pointer)
    TESTENTRY (native_callback_memory_should_be_eagerly_reclaimed)
    TESTENTRY (native_callback_should_be_kept_alive_during_calls)
    TESTENTRY (native_callback_can_return_struct)
    TESTENTRY (native_callback_can_return_large_nested_struct)
    TESTENTRY (native_callback_should_zero_retval_on_mismatching_return_value)
#ifdef HAVE_WINDOWS
# if GLIB_SIZEOF_VOID_P == 4
    TESTENTRY (native_callback_should_support_fastcall)
    TESTENTRY (native_callback_should_support_stdcall)
# endif
    TESTENTRY (native_callback_should_get_accurate_backtraces)
#endif
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("DebugSymbol")
    TESTENTRY (address_can_be_resolved_to_symbol)
    TESTENTRY (name_can_be_resolved_to_symbol)
    TESTENTRY (function_can_be_found_by_name)
    TESTENTRY (functions_can_be_found_by_name)
    TESTENTRY (functions_can_be_found_by_matching)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("CModule")
#ifdef HAVE_TINYCC
    TESTENTRY (cmodule_can_be_defined)
    TESTENTRY (cmodule_can_be_defined_with_toolchain)
    TESTENTRY (cmodule_can_be_created_from_prebuilt_binary)
    TESTENTRY (cmodule_symbols_can_be_provided)
    TESTENTRY (cmodule_should_report_parsing_errors)
    TESTENTRY (cmodule_should_report_linking_errors)
    TESTENTRY (cmodule_should_provide_lifecycle_hooks)
    TESTENTRY (cmodule_can_be_used_with_interceptor_attach)
    TESTENTRY (cmodule_can_be_used_with_interceptor_replace)
    TESTENTRY (cmodule_can_be_used_with_stalker_events)
    TESTENTRY (cmodule_can_be_used_with_stalker_transform)
    TESTENTRY (cmodule_can_be_used_with_stalker_callout)
    TESTENTRY (cmodule_can_be_used_with_stalker_call_probe)
    TESTENTRY (cmodule_can_be_used_with_module_map)
    TESTENTRY (cmodule_should_provide_some_builtin_string_functions)
    TESTENTRY (cmodule_should_provide_memory_access_apis)
    TESTENTRY (cmodule_should_support_memory_builtins)
    TESTENTRY (cmodule_should_support_arithmetic_builtins)
    TESTENTRY (cmodule_should_support_floating_point)
    TESTENTRY (cmodule_should_support_varargs)
    TESTENTRY (cmodule_should_support_global_callbacks)
    TESTENTRY (cmodule_should_provide_access_to_cpu_registers)
    TESTENTRY (cmodule_should_provide_access_to_system_error)
    TESTENTRY (system_error_unaffected_by_native_callback_from_cmodule)
#else
    TESTENTRY (cmodule_constructor_should_throw_not_available)
#endif
    TESTENTRY (cmodule_builtins_can_be_retrieved)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Instruction")
    TESTENTRY (instruction_can_be_parsed)
    TESTENTRY (instruction_can_be_generated)
    TESTENTRY (instruction_can_be_relocated)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("CodeWriter")
    TESTENTRY (code_writer_should_not_flush_on_gc)
    TESTENTRY (code_writer_should_flush_on_reset)
    TESTENTRY (code_writer_should_flush_on_dispose)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("CodeRelocator")
    TESTENTRY (code_relocator_should_expose_input_instruction)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("File")
    TESTENTRY (whole_file_can_be_read_as_bytes)
    TESTENTRY (whole_file_can_be_read_as_text)
    TESTENTRY (whole_file_can_be_read_as_text_with_validation)
    TESTENTRY (whole_file_can_be_written_from_bytes)
    TESTENTRY (whole_file_can_be_written_from_text)
    TESTENTRY (file_can_be_read_as_bytes_in_one_go)
    TESTENTRY (file_can_be_read_as_bytes_in_chunks)
    TESTENTRY (file_can_be_read_as_text_in_one_go)
    TESTENTRY (file_can_be_read_as_text_in_chunks)
    TESTENTRY (file_can_be_read_as_text_with_validation)
    TESTENTRY (file_can_be_read_line_by_line)
    TESTENTRY (file_can_be_read_line_by_line_with_validation)
    TESTENTRY (file_position_can_be_queried)
    TESTENTRY (file_position_can_be_updated_to_absolute_position_implicitly)
    TESTENTRY (file_position_can_be_updated_to_absolute_position_explicitly)
    TESTENTRY (file_position_can_be_updated_to_relative_position_from_current)
    TESTENTRY (file_position_can_be_updated_to_relative_position_from_end)
    TESTENTRY (file_can_be_written_to)
#ifndef HAVE_QNX
    TESTENTRY (file_apis_can_not_trigger_interceptor)
#endif
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Checksum")
    TESTENTRY (md5_can_be_computed_for_stream)
    TESTENTRY (md5_can_be_computed_for_string)
    TESTENTRY (md5_can_be_computed_for_bytes)
    TESTENTRY (sha1_can_be_computed_for_string)
    TESTENTRY (sha256_can_be_computed_for_string)
    TESTENTRY (sha384_can_be_computed_for_string)
    TESTENTRY (sha512_can_be_computed_for_string)
    TESTENTRY (requesting_unknown_checksum_for_string_should_throw)
    TESTENTRY (checksum_can_be_copied)
    TESTENTRY (checksum_can_be_peeked)
  TESTGROUP_END ()

#ifdef HAVE_SQLITE
  TESTGROUP_BEGIN ("Database")
    TESTENTRY (inline_sqlite_database_can_be_queried)
    TESTENTRY (sqlite_statement_has_column_and_params_metadata)
    TESTENTRY (external_sqlite_database_can_be_queried)
    TESTENTRY (external_sqlite_database_can_be_opened_with_flags)
# if !defined (HAVE_WINDOWS) && !defined (HAVE_QNX)
    TESTENTRY (sqlite_apis_can_not_trigger_interceptor)
# endif
  TESTGROUP_END ()
#endif

  TESTGROUP_BEGIN ("MatchPattern")
    TESTENTRY (match_pattern_can_be_constructed_from_string)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Stalker")
#if defined (HAVE_I386) || defined (HAVE_ARM) || defined (HAVE_ARM64)
    TESTENTRY (execution_can_be_traced)
    TESTENTRY (execution_can_be_traced_with_custom_transformer)
    TESTENTRY (execution_can_be_traced_with_faulty_transformer)
    TESTENTRY (execution_can_be_traced_during_immediate_native_function_call)
    TESTENTRY (execution_can_be_traced_during_scheduled_native_function_call)
    TESTENTRY (execution_can_be_traced_after_native_function_call_from_hook)
    TESTENTRY (basic_block_can_be_invalidated_for_current_thread)
    TESTENTRY (basic_block_can_be_invalidated_for_specific_thread)
#endif
#if defined (HAVE_I386) || defined (HAVE_ARM64)
    TESTENTRY (call_can_be_probed)
#endif
    TESTENTRY (stalker_events_can_be_parsed)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("ESM")
    TESTENTRY (esm_in_root_should_be_supported)
    TESTENTRY (esm_in_subdir_should_be_supported)
    TESTENTRY (esm_referencing_subdir_should_be_supported)
    TESTENTRY (esm_referencing_parent_should_be_supported)
    TESTENTRY (esm_throwing_on_load_should_emit_error)
    TESTENTRY (esm_throwing_after_toplevel_await_should_emit_error)
    TESTENTRY (esm_referencing_missing_module_should_fail_to_load)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Dynamic")
    TESTENTRY (dynamic_script_evaluation_should_be_supported)
    TESTENTRY (dynamic_script_evaluation_should_throw_on_syntax_error)
    TESTENTRY (dynamic_script_evaluation_should_throw_on_runtime_error)
    TESTENTRY (dynamic_script_loading_should_be_supported)
    TESTENTRY (dynamic_script_loading_should_throw_on_syntax_error)
    TESTENTRY (dynamic_script_loading_should_throw_on_runtime_error)
    TESTENTRY (dynamic_script_loading_should_throw_on_error_with_toplevel_await)
    TESTENTRY (dynamic_script_loading_should_throw_on_dupe_load_attempt)
    TESTENTRY (dynamic_script_should_support_imports_from_parent)
    TESTENTRY (dynamic_script_should_support_imports_from_other_dynamic_scripts)
    TESTENTRY (dynamic_script_evaluated_should_support_inline_source_map)
    TESTENTRY (dynamic_script_loaded_should_support_inline_source_map)
    TESTENTRY (dynamic_script_loaded_should_support_separate_source_map)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Worker")
    TESTENTRY (worker_basics_should_be_supported)
    TESTENTRY (worker_rpc_should_be_supported)
    TESTENTRY (worker_termination_should_be_supported)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Profiler")
    TESTENTRY (code_can_be_profiled)
  TESTGROUP_END ()

  TESTGROUP_BEGIN ("Sampler")
    TESTENTRY (cycles_can_be_sampled)
    TESTENTRY (busy_cycles_can_be_sampled)
    TESTENTRY (wall_clock_can_be_sampled)
    TESTENTRY (user_time_can_be_sampled)
    TESTENTRY (user_time_can_be_sampled_for_other_threads)
    TESTENTRY (user_time_find_busy_threads)
    TESTENTRY (malloc_count_can_be_sampled)
    TESTENTRY (call_count_can_be_sampled)
  TESTGROUP_END ()

  TESTENTRY (script_can_be_compiled_to_bytecode)
  TESTENTRY (script_should_not_leak_if_destroyed_before_load)
  TESTENTRY (script_memory_usage)
  TESTENTRY (source_maps_should_be_supported_for_our_runtime)
  TESTENTRY (source_maps_should_be_supported_for_user_scripts)
  TESTENTRY (types_handle_invalid_construction)
  TESTENTRY (globals_can_be_dynamically_generated)
  TESTENTRY (exceptions_can_be_handled)
  TESTENTRY (debugger_can_be_enabled)
  TESTENTRY (cloaked_items_can_be_queried_added_and_removed)
TESTLIST_END ()

typedef int (* TargetFunctionInt) (int arg);
typedef struct _GumInvokeTargetContext GumInvokeTargetContext;
typedef struct _GumNamedSleeperContext GumNamedSleeperContext;
typedef struct _TestRunOnThreadSyncContext TestRunOnThreadSyncContext;
typedef struct _GumCrashExceptorContext GumCrashExceptorContext;
typedef struct _GumRecoverExceptorContext GumRecoverExceptorContext;
typedef struct _TestTrigger TestTrigger;
typedef struct _TestNativeReturnStruct TestNativeReturnStruct;
typedef struct _TestNativeReturnNestedStruct TestNativeReturnNestedStruct;
typedef struct _GumHotNamedSleeperContext GumHotNamedSleeperContext;

struct _GumInvokeTargetContext
{
  GumScript * script;
  guint repeat_duration;
  volatile gint started;
  volatile gint finished;
};

struct _GumNamedSleeperContext
{
  GAsyncQueue * controller_messages;
  GAsyncQueue * sleeper_messages;
};

struct _TestRunOnThreadSyncContext
{
  GMutex mutex;
  GCond cond;
  gboolean started;
  GumThreadId thread_id;
  gboolean * done;
};

struct _GumCrashExceptorContext
{
  gboolean called;
  GumScriptBackend * backend;
};

struct _GumRecoverExceptorContext
{
  gboolean called;
  gpointer page;
};

struct _TestTrigger
{
  volatile gboolean ready;
  volatile gboolean fired;
  GMutex mutex;
  GCond cond;
};

struct _TestNativeReturnStruct
{
  uint32_t a;
  void * b;
};

struct _TestNativeReturnNestedStruct
{
  struct
  {
    int64_t a;
    int64_t b;
    int64_t c;
    int64_t d;
    struct
    {
      int64_t a;
      int64_t b;
      int64_t c;
    } e;
  } a;
  int64_t b;
};

struct _GumHotNamedSleeperContext
{
  GAsyncQueue * controller_messages;
  GAsyncQueue * sleeper_messages;
  GumThreadId id;
  gboolean hot;
};

static size_t gum_get_size_max (void);
static gboolean gum_test_size_max (size_t sz);
static size_t gum_add_size (size_t sz);
static size_t gum_pass_size (size_t u64);
#ifndef _MSC_VER
static size_t gum_pass_ssize (ssize_t ssz);
#endif

static gboolean ignore_thread (GumInterceptor * interceptor);
static gboolean unignore_thread (GumInterceptor * interceptor);

static gint gum_assert_variadic_uint8_values_are_sane (gpointer a, gpointer b,
    gpointer c, gpointer d, ...);
static gint gum_clobber_system_error (gint value);
static gint gum_get_answer_to_life_universe_and_everything (void);
static gint gum_toupper (gchar * str, gint limit);
static gint64 gum_classify_timestamp (gint64 timestamp);
static guint64 gum_square (guint64 value);
static gint gum_sum (gint count, ...);
static gint gum_add_pointers_and_float_fixed (gpointer a, gpointer b, float c);
static gint gum_add_pointers_and_float_variadic (gpointer a, ...);

static gboolean on_incoming_connection (GSocketService * service,
    GSocketConnection * connection, GObject * source_object,
    gpointer user_data);
static void on_read_ready (GObject * source_object, GAsyncResult * res,
    gpointer user_data);

#if defined (HAVE_I386) || defined (HAVE_ARM) || defined (HAVE_ARM64)
static gpointer run_stalked_through_hooked_function (gpointer data);
static gpointer run_stalked_through_block_invalidated_in_callout (
    gpointer data);
static gpointer run_stalked_through_block_invalidated_by_request (
    gpointer data);
static gpointer run_stalked_through_target_function (gpointer data);
#endif

static gpointer sleeping_dummy (gpointer data);
G_GNUC_UNUSED static gpointer named_sleeper (gpointer data);
static GThread * create_sleeping_dummy_thread_sync (gboolean * done,
    GumThreadId * thread_id);
static gpointer sleeping_dummy_func (gpointer data);
static const gchar * get_local_thread_string_value (void);

static gpointer invoke_target_function_int_worker (gpointer data);
static gpointer invoke_target_function_trigger (gpointer data);

#ifndef HAVE_WINDOWS
static void exit_on_sigsegv (int sig, siginfo_t * info, void * context);
static gboolean on_exceptor_called (GumExceptionDetails * details,
    gpointer user_data);
static gboolean on_exceptor_recover (GumExceptionDetails * details,
    gpointer user_data);
#ifdef HAVE_DARWIN
static gpointer simulate_crash_handler (gpointer user_data);
static gboolean suspend_all_threads (const GumThreadDetails * details,
    gpointer user_data);
static gboolean resume_all_threads (const GumThreadDetails * details,
    gpointer user_data);
#endif
#endif

static int target_function_int_replacement (int arg);

static void measure_target_function_int_overhead (void);
static int compare_measurements (gconstpointer element_a,
    gconstpointer element_b);

static void put_return_instruction (gpointer mem, gpointer user_data);

static gpointer user_time_thread_proc (gpointer data);
static gpointer user_time_find_busy_thread_proc (gpointer data);
static void do_work (void);
static gboolean check_cycles_testable (void);
static gboolean check_busy_cycles_testable (void);
static gboolean check_user_time_testable (void);

static gboolean check_exception_handling_testable (void);

static void on_script_message (const gchar * message, GBytes * data,
    gpointer user_data);
static void on_incoming_debug_message (GumInspectorServer * server,
    const gchar * message, gpointer user_data);
static void on_outgoing_debug_message (const gchar * message,
    gpointer user_data);

#ifdef HAVE_DARWIN
static gpointer interceptor_attacher_worker (gpointer data);
static void empty_invocation_callback (GumInvocationContext * context,
    gpointer user_data);
#endif

static int target_function_int (int arg);
G_GNUC_UNUSED static float target_function_float (float arg);
G_GNUC_UNUSED static double target_function_double (double arg);
static const guint8 * target_function_base_plus_offset (const guint8 * base,
    int offset);
static const gchar * target_function_string (const gchar * arg);
static void target_function_callbacks (const gint value,
    void (* first) (const gint * value), void (* second) (const gint * value));
static void target_function_trigger (TestTrigger * trigger);
static int target_function_nested_a (int arg);
static int target_function_nested_b (int arg);
static int target_function_nested_c (int arg);
static void target_function_variable_sleepyness (guint n);

static gpointer load_script_worker (gpointer data);
static GumScript * create_endless_script (TestScriptFixture * fixture);

static TargetFunctionInt target_function_original = NULL;
static GPrivate target_thread_string_value = G_PRIVATE_INIT (g_free);

gint gum_script_dummy_global_to_trick_optimizer = 0;

TESTCASE (instruction_can_be_parsed)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const first = Instruction.parse(" GUM_PTR_CONST ");"
      "const second = Instruction.parse(first.next);"
      "send(typeof first.toString());"
      "send(typeof second.toString());"
      "send(!second.toString().startsWith('[object'));"
      "send(first.address.toInt32() !== 0);"
      "send(first.size > 0);"
      "send(typeof first.mnemonic);"
      "send(typeof first.opStr);"
      "send(JSON.stringify(first) !== \"{}\");",
      target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  if (!gum_process_is_debugger_attached () && !RUNNING_ON_VALGRIND)
  {
    COMPILE_AND_LOAD_SCRIPT ("Instruction.parse(ptr(\"0x42\"));");
    EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x42");
  }

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  COMPILE_AND_LOAD_SCRIPT (
      "const code = Memory.alloc(Process.pageSize);"

      "const cw = new X86Writer(code, { pc: ptr(0x1000) });"
      "send(cw.pc);"
      "send(cw.offset);"
      "cw.putU8(0xab);" /* stosd */
      "send(cw.pc);"
      "send(cw.offset);"
      "send(cw.code.equals(cw.base.add(1)));"
      "cw.putMovRegU32('eax', 42);"
      "cw.putCallRegOffsetPtr('rax', 12);"
      "cw.flush();"

      "const stosd = Instruction.parse(code);"
      "send(stosd.mnemonic);"
      "send(stosd.regsAccessed.read);"
      "send(stosd.regsAccessed.written);"
      "send(stosd.regsRead);"
      "send(stosd.regsWritten);"
      "send(stosd.groups);"

      "const mov = Instruction.parse(stosd.next);"
      "send(mov.mnemonic);"
      "let operands = mov.operands;"
      "send(operands.length);"
      "send(operands[0].type);"
      "send(operands[0].value);"
      "send(operands[0].size);"
      "send(operands[0].access);"
      "send(operands[1].type);"
      "send(operands[1].value);"
      "send(operands[1].size);"
      "send(operands[1].access);"
      "send(mov.regsAccessed.read);"
      "send(mov.regsAccessed.written);"
      "send(mov.regsRead);"
      "send(mov.regsWritten);"
      "send(mov.groups);"

      "const call = Instruction.parse(mov.next);"
      "send(call.mnemonic);"
      "operands = call.operands;"
      "send(operands[0].type);"
      "const memProps = Object.keys(operands[0].value);"
      "memProps.sort();"
      "send(memProps);"
      "send(operands[0].value.base);"
      "send(operands[0].value.scale);"
      "send(operands[0].value.disp);"
      "send(call.groups);");

  EXPECT_SEND_MESSAGE_WITH ("\"0x1000\"");
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("\"0x1001\"");
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"stosd\"");
  EXPECT_SEND_MESSAGE_WITH ("[\"eax\",\"rdi\",\"rflags\"]");
  EXPECT_SEND_MESSAGE_WITH ("[\"rdi\"]");
  EXPECT_SEND_MESSAGE_WITH ("[\"eax\",\"rdi\",\"rflags\"]");
  EXPECT_SEND_MESSAGE_WITH ("[\"rdi\"]");
  EXPECT_SEND_MESSAGE_WITH ("[]");

  EXPECT_SEND_MESSAGE_WITH ("\"mov\"");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("\"reg\"");
  EXPECT_SEND_MESSAGE_WITH ("\"eax\"");
  EXPECT_SEND_MESSAGE_WITH ("4");
  EXPECT_SEND_MESSAGE_WITH ("\"w\"");
  EXPECT_SEND_MESSAGE_WITH ("\"imm\"");
  EXPECT_SEND_MESSAGE_WITH ("\"42\"");
  EXPECT_SEND_MESSAGE_WITH ("4");
  EXPECT_SEND_MESSAGE_WITH ("\"\"");
  EXPECT_SEND_MESSAGE_WITH ("[]");
  EXPECT_SEND_MESSAGE_WITH ("[\"eax\"]");
  EXPECT_SEND_MESSAGE_WITH ("[]");
  EXPECT_SEND_MESSAGE_WITH ("[]");
  EXPECT_SEND_MESSAGE_WITH ("[]");

  EXPECT_SEND_MESSAGE_WITH ("\"call\"");
  EXPECT_SEND_MESSAGE_WITH ("\"mem\"");
  EXPECT_SEND_MESSAGE_WITH ("[\"base\",\"disp\",\"scale\"]");
  EXPECT_SEND_MESSAGE_WITH ("\"rax\"");
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("12");
  EXPECT_SEND_MESSAGE_WITH ("[\"call\",\"mode64\"]");
#elif defined (HAVE_ARM)
  COMPILE_AND_LOAD_SCRIPT (
      "const code = Memory.alloc(Process.pageSize);"

      "const tw = new ThumbWriter(code);"
      "tw.putLdrRegU32('r0', 42);"
      "tw.putBlImm(code.add(64));"
      /* sxtb.w r3, r7, ror 16 */
      "tw.putInstruction(0xfa4f); tw.putInstruction(0xf3a7);"
      /* vdup.8 d3, d7[1] */
      "tw.putInstruction(0xffb3); tw.putInstruction(0x3c07);"
      "tw.flush();"

      "const ldr = Instruction.parse(code.or(1));"
      "send(ldr.mnemonic);"
      "let operands = ldr.operands;"
      "send(operands.length);"
      "send(operands[0].type);"
      "send(operands[0].value);"
      "send(operands[0].access);"
      "send(operands[1].type);"
      "send(operands[1].value.base);"
      "send(operands[1].value.scale);"
      "send(operands[1].access);"
      "const disp = operands[1].value.disp;"
      "send(ldr.address.add(4 + disp).readU32());"

      "const bl = Instruction.parse(ldr.next);"
      "send(bl.mnemonic);"
      "operands = bl.operands;"
      "send(operands[0].type);"
      "send(ptr(operands[0].value).equals(code.add(64)));"

      "const sxtb = Instruction.parse(bl.next);"
      "send(sxtb.mnemonic);"
      "operands = sxtb.operands;"
      "send(typeof operands[0].shift);"
      "send(operands[1].shift.type);"
      "send(operands[1].shift.value);"

      "const vdup = Instruction.parse(sxtb.next);"
      "send(vdup.mnemonic);"
      "operands = vdup.operands;"
      "send(typeof operands[0].vectorIndex);"
      "send(operands[1].vectorIndex);"

      "const aw = new ArmWriter(code);"
      "aw.putInstruction(0xe00380f7);" /* strd r8, sb, [r3], -r7 */
      "aw.flush();"

      "const strdeq = Instruction.parse(code);"
      "send(strdeq.mnemonic);"
      "operands = strdeq.operands;"
      "send(operands[0].subtracted);"
      "send(operands[1].subtracted);"
      "send(operands[2].subtracted);"
      "send(operands[3].subtracted);");

  EXPECT_SEND_MESSAGE_WITH ("\"ldr\"");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("\"reg\"");
  EXPECT_SEND_MESSAGE_WITH ("\"r0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"w\"");
  EXPECT_SEND_MESSAGE_WITH ("\"mem\"");
  EXPECT_SEND_MESSAGE_WITH ("\"pc\"");
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("\"r\"");
  EXPECT_SEND_MESSAGE_WITH ("42");

  EXPECT_SEND_MESSAGE_WITH ("\"bl\"");
  EXPECT_SEND_MESSAGE_WITH ("\"imm\"");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("\"sxtb.w\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("\"ror\"");
  EXPECT_SEND_MESSAGE_WITH ("16");

  EXPECT_SEND_MESSAGE_WITH ("\"vdup.8\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("1");

  EXPECT_SEND_MESSAGE_WITH ("\"strd\"");
  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("true");
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "const code = Memory.alloc(Process.pageSize);"

      "const cw = new Arm64Writer(code);"
      "cw.putLdrRegU64('x0', 42);"
      "cw.putStrRegRegOffset('x0', 'x7', 32);"
      "cw.putInstruction(0xcb422020);" /* sub x0, x1, x2, lsr #8 */
      "cw.putInstruction(0x8b230841);" /* add x1, x2, w3, uxtb #2 */
      "cw.putInstruction(0x4ee28420);" /* add.2d v0, v1, v2 */
      "cw.putInstruction(0x9eae00e5);" /* fmov.d x5, v7[1] */
      "cw.flush();"

      "const ldr = Instruction.parse(code);"
      "send(ldr.mnemonic);"
      "let operands = ldr.operands;"
      "send(operands.length);"
      "send(operands[0].type);"
      "send(operands[0].value);"
      "send(operands[0].access);"
      "send(operands[1].type);"
      "send(operands[1].access);"
      "send(ptr(operands[1].value).readU64().valueOf());"

      "const str = Instruction.parse(ldr.next);"
      "send(str.mnemonic);"
      "operands = str.operands;"
      "send(operands[1].type);"
      "const memProps = Object.keys(operands[1].value);"
      "memProps.sort();"
      "send(memProps);"
      "send(operands[1].value.base);"
      "send(operands[1].value.disp);"

      "const sub = Instruction.parse(str.next);"
      "send(sub.mnemonic);"
      "operands = sub.operands;"
      "send(typeof operands[0].shift);"
      "send(typeof operands[1].shift);"
      "send(operands[2].shift.type);"
      "send(operands[2].shift.value);"

      "const add = Instruction.parse(sub.next);"
      "send(add.mnemonic);"
      "operands = add.operands;"
      "send(typeof operands[0].ext);"
      "send(typeof operands[1].ext);"
      "send(operands[2].ext);"

      "const vadd = Instruction.parse(add.next);"
      "send(vadd.mnemonic);"
      "operands = vadd.operands;"
      "send(operands[0].vas);"
      "send(operands[1].vas);"
      "send(operands[2].vas);"

      "const fmov = Instruction.parse(vadd.next);"
      "send(fmov.mnemonic);"
      "operands = fmov.operands;"
      "send(typeof operands[0].vectorIndex);"
      "send(operands[1].vectorIndex);");

  EXPECT_SEND_MESSAGE_WITH ("\"ldr\"");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("\"reg\"");
  EXPECT_SEND_MESSAGE_WITH ("\"x0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"w\"");
  EXPECT_SEND_MESSAGE_WITH ("\"imm\"");
  EXPECT_SEND_MESSAGE_WITH ("\"r\"");
  EXPECT_SEND_MESSAGE_WITH ("42");

  EXPECT_SEND_MESSAGE_WITH ("\"str\"");
  EXPECT_SEND_MESSAGE_WITH ("\"mem\"");
  EXPECT_SEND_MESSAGE_WITH ("[\"base\",\"disp\"]");
  EXPECT_SEND_MESSAGE_WITH ("\"x7\"");
  EXPECT_SEND_MESSAGE_WITH ("32");

  EXPECT_SEND_MESSAGE_WITH ("\"sub\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("\"lsr\"");
  EXPECT_SEND_MESSAGE_WITH ("8");

  EXPECT_SEND_MESSAGE_WITH ("\"add\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("\"uxtb\"");

  EXPECT_SEND_MESSAGE_WITH ("\"add\"");
  EXPECT_SEND_MESSAGE_WITH ("\"2d\"");
  EXPECT_SEND_MESSAGE_WITH ("\"2d\"");
  EXPECT_SEND_MESSAGE_WITH ("\"2d\"");

  EXPECT_SEND_MESSAGE_WITH ("\"fmov\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("1");
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (instruction_can_be_generated)
{
#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "const callback = new NativeCallback((a, b) => {"
      "  return a * b;"
      "}, 'int', ['int', 'int']);"

      "const page = Memory.alloc(Process.pageSize);"

      "Memory.patchCode(page, 64, code => {"
        "const cw = new X86Writer(code, { pc: page });"

        "cw.putMovRegU32('eax', 42);"

        "const stackAlignOffset = Process.pointerSize;"
        "cw.putSubRegImm('xsp', stackAlignOffset);"

        "cw.putCallAddressWithArguments(callback, ['eax', 7]);"

        "cw.putAddRegImm('xsp', stackAlignOffset);"

        "cw.putJmpShortLabel('badger');"

        "cw.putMovRegU32('eax', 43);"

        "cw.putLabel('badger');"
        "cw.putRet();"

        "cw.flush();"
        "send(cw.offset > 30);"
      "});"

      "const f = new NativeFunction(page, 'int', []);"
      "send(f());");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("294");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const code = Memory.alloc(16);"
      "const cw = new X86Writer(code);"
      "cw.putMovRegU32('rax', 42);");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid argument");
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (instruction_can_be_relocated)
{
#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"

      "const impl1 = page.add(0);"
      "const impl2 = page.add(64);"

      "Memory.patchCode(impl1, 16, code => {"
        "const cw = new X86Writer(code, { pc: impl1 });"
        "cw.putMovRegU32('eax', 42);"
        "cw.putRet();"
        "cw.flush();"
      "});"

      "Memory.patchCode(impl2, 16, code => {"
        "const cw = new X86Writer(code, { pc: impl2 });"
        "const rl = new X86Relocator(impl1, cw);"

        "send(rl.input);"

        "send(rl.readOne());"
        "send(rl.input.toString());"
        "send(rl.writeOne());"

        "send(rl.eob);"
        "send(rl.eoi);"

        "send(rl.readOne());"
        "send(rl.input.toString());"
        "send(rl.writeOne());"

        "send(rl.readOne());"
        "send(rl.eob);"
        "send(rl.eoi);"

        "cw.flush();"
      "});"

      "const f = new NativeFunction(impl2, 'int', []);"
      "send(f());");

  EXPECT_SEND_MESSAGE_WITH ("null");

  EXPECT_SEND_MESSAGE_WITH ("5");
  EXPECT_SEND_MESSAGE_WITH ("\"mov eax, 0x2a\"");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("false");

  EXPECT_SEND_MESSAGE_WITH ("6");
  EXPECT_SEND_MESSAGE_WITH ("\"ret\"");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("42");

  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (code_writer_should_not_flush_on_gc)
{
#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "let writer = new X86Writer(page);"
      "writer.putJmpShortLabel('later');"
      "writer.putBreakpoint();"
      "writer.putLabel('later');"
      "writer.putRet();"
      "Memory.protect(page, Process.pageSize, '---');"
      "writer = null;"
      "gc();");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "let writer = new ArmWriter(page);"
      "writer.putBLabel('later');"
      "writer.putBrkImm(42);"
      "writer.putLabel('later');"
      "writer.putMovRegReg('pc', 'lr');"
      "Memory.protect(page, Process.pageSize, '---');"
      "writer = null;"
      "gc();");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "let writer = new ThumbWriter(page);"
      "writer.putBLabel('later');"
      "writer.putBkptImm(42);"
      "writer.putLabel('later');"
      "writer.putPopRegs(['pc']);"
      "Memory.protect(page, Process.pageSize, '---');"
      "writer = null;"
      "gc();");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "let writer = new Arm64Writer(page);"
      "writer.putBLabel('later');"
      "writer.putBrkImm(42);"
      "writer.putLabel('later');"
      "writer.putRet();"
      "Memory.protect(page, Process.pageSize, '---');"
      "writer = null;"
      "gc();");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_MIPS)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "let writer = new MipsWriter(page);"
      "writer.putJLabel('later');"
      "writer.putBreak(42);"
      "writer.putLabel('later');"
      "writer.putRet();"
      "Memory.protect(page, Process.pageSize, '---');"
      "writer = null;"
      "gc();");
  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (code_writer_should_flush_on_reset)
{
  const gchar * test_reset =
      "const size = writer.offset;"
      "const before = new Uint8Array(page.readByteArray(size));"
      "writer.reset(page);"
      "const after = new Uint8Array(page.readByteArray(size));"
      "send(after.join(',') !== before.join(','));";

#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new X86Writer(page);"
      "writer.putJmpShortLabel('later');"
      "writer.putBreakpoint();"
      "writer.putLabel('later');"
      "writer.putRet();"
      "%s",
      test_reset);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new ArmWriter(page);"
      "writer.putBLabel('later');"
      "writer.putBrkImm(13);"
      "writer.putBrkImm(37);"
      "writer.putLabel('later');"
      "writer.putMovRegReg('pc', 'lr');"
      "%s",
      test_reset);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new ThumbWriter(page);"
      "writer.putBLabel('later');"
      "writer.putBkptImm(13);"
      "writer.putBkptImm(37);"
      "writer.putLabel('later');"
      "writer.putPopRegs(['pc']);"
      "%s",
      test_reset);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new Arm64Writer(page);"
      "writer.putBLabel('later');"
      "writer.putBrkImm(42);"
      "writer.putLabel('later');"
      "writer.putRet();"
      "%s",
      test_reset);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_MIPS)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new MipsWriter(page);"
      "writer.putJLabel('later');"
      "writer.putBreak(42);"
      "writer.putLabel('later');"
      "writer.putRet();"
      "%s",
      test_reset);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (code_writer_should_flush_on_dispose)
{
  const gchar * test_dispose =
      "const size = writer.offset;"
      "const before = new Uint8Array(page.readByteArray(size));"
      "writer.dispose();"
      "const after = new Uint8Array(page.readByteArray(size));"
      "send(after.join(',') !== before.join(','));";

#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new X86Writer(page);"
      "writer.putJmpShortLabel('later');"
      "writer.putBreakpoint();"
      "writer.putLabel('later');"
      "writer.putRet();"
      "%s",
      test_dispose);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new ArmWriter(page);"
      "writer.putBLabel('later');"
      "writer.putBrkImm(13);"
      "writer.putBrkImm(37);"
      "writer.putLabel('later');"
      "writer.putMovRegReg('pc', 'lr');"
      "%s",
      test_dispose);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new ThumbWriter(page);"
      "writer.putBLabel('later');"
      "writer.putBkptImm(13);"
      "writer.putBkptImm(37);"
      "writer.putLabel('later');"
      "writer.putPopRegs(['pc']);"
      "%s",
      test_dispose);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new Arm64Writer(page);"
      "writer.putBLabel('later');"
      "writer.putBrkImm(42);"
      "writer.putLabel('later');"
      "writer.putRet();"
      "%s",
      test_dispose);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_MIPS)
  COMPILE_AND_LOAD_SCRIPT (
      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new MipsWriter(page);"
      "writer.putJLabel('later');"
      "writer.putBreak(42);"
      "writer.putLabel('later');"
      "writer.putRet();"
      "%s",
      test_dispose);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (code_relocator_should_expose_input_instruction)
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  COMPILE_AND_LOAD_SCRIPT (
      "const code = Memory.alloc(4);"
      "code.writeByteArray([0x55, 0x48, 0x8b, 0xec]);"

      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new X86Writer(page);"
      "const relocator = new X86Relocator(code, writer);"

      "send(relocator.input);"
      "send(relocator.peekNextWriteInsn());"

      "send(relocator.readOne());"
      "let insn = relocator.input;"
      "send(insn.toString());"
      "send(insn.address.equals(code));"
      "send(insn.next.equals(code.add(1)));"
      "relocator.writeOne();"

      "send(relocator.readOne());"
      "insn = relocator.peekNextWriteInsn();"
      "send(insn.toString());"
      "send(insn.address.equals(code.add(1)));"
      "send(insn.next.equals(code.add(4)));");

  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("null");

  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("\"push rbp\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("4");
  EXPECT_SEND_MESSAGE_WITH ("\"mov rbp, rsp\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "const code = Memory.alloc(8);"

      "const littleEndian = %s;"
      "if (littleEndian) {"
      "  code.writeU32(0xb9400ae8);"
      "  code.add(4).writeU32(0x3100051f);"
      "} else {"
      "  code.writeU32(0xe80a40b9);"
      "  code.add(4).writeU32(0x1f050031);"
      "}"

      "const page = Memory.alloc(Process.pageSize);"
      "const writer = new Arm64Writer(page);"
      "const relocator = new Arm64Relocator(code, writer);"

      "send(relocator.input);"
      "send(relocator.peekNextWriteInsn());"

      "send(relocator.readOne());"
      "let insn = relocator.input;"
      "send(insn.toString());"
      "send(insn.address.equals(code));"
      "send(insn.next.equals(code.add(4)));"
      "relocator.writeOne();"

      "send(relocator.readOne());"
      "insn = relocator.peekNextWriteInsn();"
      "send(insn.toString());"
      "send(insn.address.equals(code.add(4)));"
      "send(insn.next.equals(code.add(8)));",
      (G_BYTE_ORDER == G_LITTLE_ENDIAN) ? "true" : "false");

  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("null");

  EXPECT_SEND_MESSAGE_WITH ("4");
  EXPECT_SEND_MESSAGE_WITH ("\"ldr w8, [x23, #8]\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("8");
  EXPECT_SEND_MESSAGE_WITH ("\"cmn w8, #1\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (address_can_be_resolved_to_symbol)
{
#ifdef HAVE_ANDROID
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const sym = DebugSymbol.fromAddress(" GUM_PTR_CONST ");"
      "send(sym.name);"
      "send(sym.toString().indexOf(sym.name) !== -1);"
      "send(JSON.stringify(sym) !== \"{}\");",
      target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("\"target_function_int\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (name_can_be_resolved_to_symbol)
{
  gchar * expected;

#ifdef HAVE_ANDROID
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT ("send(DebugSymbol.fromName(\"gum_init\").address);");
  expected = g_strdup_printf ("\"0x%" G_GINT64_MODIFIER "x\"",
      GUM_ADDRESS (gum_init));
  EXPECT_SEND_MESSAGE_WITH (expected);
  g_free (expected);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_found_by_name)
{
#ifdef HAVE_ANDROID
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT ("send("
      "!DebugSymbol.getFunctionByName(\"g_thread_new\").isNull()"
  ");"
  "send("
      "DebugSymbol.getFunctionByName(\"g_thread_!@#$\")"
  ");");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: unable to find function with name 'g_thread_!@#$'");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (functions_can_be_found_by_name)
{
#ifdef HAVE_ANDROID
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT ("send("
      "DebugSymbol.findFunctionsNamed(\"g_thread_new\").length >= 1"
  ");");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (functions_can_be_found_by_matching)
{
#ifdef HAVE_ANDROID
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT ("send("
      "DebugSymbol.findFunctionsMatching(\"gum_symbol_details_from*\")"
          ".length >= 1"
  ");");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_function_can_be_invoked)
{
  gchar str[7];

  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'int', []);"
      "send(f());",
      gum_get_answer_to_life_universe_and_everything);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();

  strcpy (str, "badger");
  COMPILE_AND_LOAD_SCRIPT (
      "const toupper = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', 'int']);"
      "send(toupper(" GUM_PTR_CONST ", 3));"
      "send(toupper(" GUM_PTR_CONST ", -1));",
      gum_toupper, str, str);
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("-6");
  EXPECT_NO_MESSAGES ();
  g_assert_cmpstr (str, ==, "BADGER");

  COMPILE_AND_LOAD_SCRIPT (
      "const sum = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', 'pointer', 'float']);"
      "send(sum(ptr(3), ptr(4), 42.0));",
      gum_add_pointers_and_float_fixed);
  EXPECT_SEND_MESSAGE_WITH ("49");
  EXPECT_NO_MESSAGES ();

#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "const impl = Process.getModuleByName('user32.dll')"
        ".getExportByName('GetKeyState');"
      "const f = new NativeFunction(impl, 'int16', ['int']);"
      "const result = f(0x41);"
      "send(typeof result);");
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
  EXPECT_NO_MESSAGES ();
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const classify = new NativeFunction(" GUM_PTR_CONST ", "
          "'int64', ['int64']);"
      "send(classify(int64(\"-42\")));"
      "send(classify(int64(\"0\")));"
      "send(classify(int64(\"42\")));",
      gum_classify_timestamp);
  EXPECT_SEND_MESSAGE_WITH ("\"-1\"");
  EXPECT_SEND_MESSAGE_WITH ("\"0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"1\"");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const square = new NativeFunction(" GUM_PTR_CONST ", "
          "'uint64', ['uint64']);"
      "send(square(uint64(\"2\")));"
      "send(square(uint64(\"4\")));"
      "send(square(uint64(\"6\")));",
      gum_square);
  EXPECT_SEND_MESSAGE_WITH ("\"4\"");
  EXPECT_SEND_MESSAGE_WITH ("\"16\"");
  EXPECT_SEND_MESSAGE_WITH ("\"36\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_function_can_be_invoked_with_size_t)
{
  gchar arg[23];
  gchar ret[23];

  /*
   * Per specs “size_t” is an unsigned integer type defined in stddef.h.
   * Per recommendation “size_t” shall be able to represent the largest
   * possible object size:
   *
   *     The types used for “size_t” and “ptrdiff_t” should not have an
   *     integer conversion rank greater than that of “signed long int”
   *     unless the implementation supports objects large enough to make
   *     this necessary.
   *
   * The largest possible size is defined by SIZE_MAX (stddef.h).
   * The minimum value for SIZE_MAX definitions is 65535 (ref C99, 7.18.3),
   * which implies that the smallest possible “size_t” conversion would be
   * 16bit (depends on architecture implementation and compiler).
   *
   * Conclusion: If the maximum object size of an implementation corresponds to
   * the address-width, it could be assumed that SIZE_MAX will not exceed
   * UINT64_MAX, for architectures in Frida's scope. This again means, that for
   * the JavaScript runtimes all possible “size_t” values could be represented
   * as “uint64” (as 64bit SIZE_MAX of 1844674407370955161UL would exceed the
   * limits of JavaScript “Number.MAX_SAFE_INTEGER”).
   * For the native part, on the other hand, “size_t” values cannot be encoded
   * in uint64 per se, instead this has to be done depending on the
   * implementation's value of SIZE_MAX.
   *
   * SIZE_WIDTH    JS size_t        Native size_t
   * 64            uint64    <->    uint64
   * 32            uint64    <->    uint32 (temporary guint64 during conversion)
   * 16            uint64    <->    uint16 (temporary guint64 during conversion)
   *
   * For GLib, the definition of gsize is very simplified (compared to C99):
   *
   *     > usually 32 bit wide on a 32-bit platform and 64 bit wide on a 64-bit
   *     > platform”
   *
   * Ref: https://developer.gnome.org/glib/stable/glib-Basic-Types.html#gsize
   *
   * Implementation of “ssize_t” is analogous.
   *
   * SIZE_WIDTH    JS ssize_t       Native ssize_t
   * 64            int64     <->    int64
   * 32            int64     <->    int32 (temporary gint64 during conversion)
   * 16            int64     <->    int16 (temporary gint64 during conversion)
   *
   * Additional notes:
   *
   * 1) ssize_t seems to be POSIX defined, but not C99.
   * 2) ptrdiff_t is not implemented (but C99 defined) ... normally ssize_t
   *    should be able to store ptrdiff_t, but this requires further testing
   * 3) Focus was put on size_t implementation, which is tested and working.
   *    ssize_t/ptrdiff_t are not in main scope and require additional testing
   *    (+ implementation of ptrdiff_t, if not casted to size_t). The test for
   *    “ssize_t” uses a simple pass-through function which is called with
   *    a) PTRDIFF_MAX and b) PTRDIFF_MIN
   *
   * External:
   *
   * - Discussion on SSIZE_MAX weirdness:
   *   https://sourceware.org/bugzilla/show_bug.cgi?id=13575
   */

  g_sprintf (ret, "\"%" G_GSIZE_MODIFIER "u\"", (gsize) SIZE_MAX);
  COMPILE_AND_LOAD_SCRIPT (
      "const getSizeMax = new NativeFunction(" GUM_PTR_CONST ", 'size_t', []);"
      "send(getSizeMax());",
      gum_get_size_max);
  EXPECT_SEND_MESSAGE_WITH (ret);
  EXPECT_NO_MESSAGES ();

  g_sprintf (arg, "%" G_GSIZE_MODIFIER "u", (gsize) (SIZE_MAX - 1));
  COMPILE_AND_LOAD_SCRIPT (
      "const addSize = new NativeFunction(" GUM_PTR_CONST ", 'size_t', "
          "['size_t']);"
      "send(addSize(uint64(\"%s\")));",
      gum_add_size, arg);
  EXPECT_SEND_MESSAGE_WITH (ret);
  EXPECT_NO_MESSAGES ();

  g_sprintf (arg, "%" G_GSIZE_MODIFIER "u", (gsize) SIZE_MAX);
  COMPILE_AND_LOAD_SCRIPT (
      "const testSizeMax = new NativeFunction(" GUM_PTR_CONST ", 'bool', "
          "['size_t']);"
      "send(testSizeMax(uint64(\"%s\")) === 1);",
      gum_test_size_max, arg);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  g_sprintf (arg, "%" G_GSIZE_MODIFIER "u", (gsize) SIZE_MAX);
  g_sprintf (ret, "\"%" G_GSIZE_MODIFIER "u\"", (gsize) SIZE_MAX);
  COMPILE_AND_LOAD_SCRIPT (
      "const passSize = new NativeFunction(" GUM_PTR_CONST ", 'size_t', "
          "['size_t']);"
      "send(passSize(uint64(\"%s\")));",
      gum_pass_size, arg);
  EXPECT_SEND_MESSAGE_WITH (ret);
  EXPECT_NO_MESSAGES ();

#ifndef _MSC_VER
  g_sprintf (arg, "%td", (ptrdiff_t) PTRDIFF_MAX);
  g_sprintf (ret, "\"%td\"", (ptrdiff_t) PTRDIFF_MAX);
  COMPILE_AND_LOAD_SCRIPT (
      "const passSSize = new NativeFunction(" GUM_PTR_CONST ", 'ssize_t', "
          "['ssize_t']);"
      "send(passSSize(int64(\"%s\")));",
      gum_pass_ssize, arg);
  EXPECT_SEND_MESSAGE_WITH (ret);
  EXPECT_NO_MESSAGES ();

  g_sprintf (arg, "%" G_GSIZE_MODIFIER"d", (gsize) PTRDIFF_MIN);
  g_sprintf (ret, "\"%" G_GSIZE_MODIFIER "d\"", (gsize) PTRDIFF_MIN);
  COMPILE_AND_LOAD_SCRIPT (
      "const passSSize = new NativeFunction(" GUM_PTR_CONST ", 'ssize_t', "
          "['ssize_t']);"
      "send(passSSize(int64(\"%s\")));",
      gum_pass_ssize, arg);
  EXPECT_SEND_MESSAGE_WITH (ret);
  EXPECT_NO_MESSAGES ();
#endif
}

static size_t
gum_get_size_max (void)
{
  return SIZE_MAX;
}

static gboolean
gum_test_size_max (size_t sz)
{
  return SIZE_MAX == sz;
}

static size_t
gum_add_size (size_t sz)
{
  return sz + (size_t) 1;
}

static size_t
gum_pass_size (size_t sz)
{
  return sz;
}

#ifndef _MSC_VER

static size_t
gum_pass_ssize (ssize_t ssz)
{
  return ssz;
}

#endif

TESTCASE (native_function_can_be_intercepted_when_thread_is_ignored)
{
  GumInterceptor * interceptor;
  GMainContext * js_context;
  GSource * source;

  interceptor = gum_interceptor_obtain ();

  js_context = gum_script_scheduler_get_js_context (
      gum_script_backend_get_scheduler ());

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) ignore_thread,
      g_object_ref (interceptor), g_object_unref);
  g_source_attach (source, js_context);
  g_source_unref (source);

  COMPILE_AND_LOAD_SCRIPT (
      "const impl = " GUM_PTR_CONST ";"
      "Interceptor.attach(impl, {"
      "  onEnter(args) {"
      "    send('>');"
      "  },"
      "  onLeave(retval) {"
      "    send('<');"
      "  }"
      "});"
      "Interceptor.flush();"
      "const f = new NativeFunction(impl, 'int', ['int']);"
      "send(f(42));",
      target_function_nested_a);

  EXPECT_SEND_MESSAGE_WITH ("\">\"");
  EXPECT_SEND_MESSAGE_WITH ("\"<\"");
  EXPECT_SEND_MESSAGE_WITH ("16855020");
  EXPECT_NO_MESSAGES ();

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) unignore_thread,
      g_object_ref (interceptor), g_object_unref);
  g_source_attach (source, js_context);
  g_source_unref (source);

  g_object_unref (interceptor);
}

static gboolean
ignore_thread (GumInterceptor * interceptor)
{
  gum_interceptor_ignore_current_thread (interceptor);

  return FALSE;
}

static gboolean
unignore_thread (GumInterceptor * interceptor)
{
  gum_interceptor_unignore_current_thread (interceptor);

  return FALSE;
}

TESTCASE (native_function_can_not_be_intercepted_when_traps_are_none)
{
  GumInterceptor * interceptor;

  interceptor = gum_interceptor_obtain ();

  COMPILE_AND_LOAD_SCRIPT (
      "const aImpl = " GUM_PTR_CONST ";"
      "const bImpl = " GUM_PTR_CONST ";"
      "const cImpl = " GUM_PTR_CONST ";"
      "Interceptor.attach(aImpl, {"
      "  onEnter(args) {"
      "    send('a>');"
      "  },"
      "  onLeave(retval) {"
      "    send('a<');"
      "  }"
      "});"
      "Interceptor.attach(bImpl, {"
      "  onEnter(args) {"
      "    send('b>');"
      "  },"
      "  onLeave(retval) {"
      "    send('b<');"
      "  }"
      "});"
      "Interceptor.attach(cImpl, {"
      "  onEnter(args) {"
      "    send('c>');"
      "  },"
      "  onLeave(retval) {"
      "    send('c<');"
      "  }"
      "});"
      "Interceptor.flush();"
      "const f = new NativeFunction(aImpl, 'int', ['int'], { traps: 'none' });"
      "send(f(42));",
      target_function_nested_a,
      target_function_nested_b,
      target_function_nested_c);

  EXPECT_SEND_MESSAGE_WITH ("16855020");
  EXPECT_NO_MESSAGES ();

  g_object_unref (interceptor);
}

TESTCASE (native_function_should_implement_call_and_apply)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'int', []);"
      "send(f.call());"
      "send(f.call(f));"
      "send(f.apply(f));"
      "send(f.apply(f, undefined));"
      "send(f.apply(f, null));"
      "send(f.apply(f, []));",
      gum_get_answer_to_life_universe_and_everything);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'int', ['int']);"
      "send(NativeFunction.prototype.call(f, 42));"
      "send(NativeFunction.prototype.apply(f, [42]));"
      "send(f.call(undefined, 42));"
      "send(f.apply(undefined, [42]));"
      "send(f.call(null, 42));"
      "send(f.apply(null, [42]));"
      "send(f.call(f, 42));"
      "send(f.apply(f, [42]));"
      "send(f.call(ptr(" GUM_PTR_CONST "), 42));"
      "send(f.apply(ptr(" GUM_PTR_CONST "), [42]));",
      target_function_int, target_function_nested_a, target_function_nested_a);
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("16855020");
  EXPECT_SEND_MESSAGE_WITH ("16855020");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'pointer', "
      "    ['pointer', 'int']);"
      "send(f.call(null, ptr(4), 3));"
      "send(f.apply(null, [ptr(4), 3]));",
      target_function_base_plus_offset);
  EXPECT_SEND_MESSAGE_WITH ("\"0x7\"");
  EXPECT_SEND_MESSAGE_WITH ("\"0x7\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_function_crash_results_in_exception)
{
  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "const targetWithString = new NativeFunction(" GUM_PTR_CONST ", "
          "'pointer', ['pointer'], {"
          "abi: 'default',"
          "scheduling: 'exclusive',"
          "exceptions: 'steal',"
      "});"
      "try {"
      "  targetWithString(NULL);"
      "} catch (e) {"
      "  send(e.type);"
      "}",
      target_function_string);
  EXPECT_SEND_MESSAGE_WITH ("\"access-violation\"");
}

TESTCASE (nested_native_function_crash_is_handled_gracefully)
{
  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "const targetWithCallback = new NativeFunction(" GUM_PTR_CONST ", "
          "'pointer', ['int', 'pointer', 'pointer']);"
      "const callback = new NativeCallback(value => {"
      "  send(value.readInt());"
      "}, 'void', ['pointer']);"
      "try {"
      "  targetWithCallback(42, callback, NULL);"
      "} catch (e) {"
      "  send(e.type);"
      "}",
      target_function_callbacks);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("\"access-violation\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (variadic_native_function_can_be_invoked)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const sum = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['int', '...', 'int']);"
      "send(sum(0));"
      "send(sum(1, 1));"
      "send(sum(3, 1, 2, 3));",
      gum_sum);
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("6");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (variadic_native_function_args_smaller_than_int_should_be_promoted)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'int', "
          "['pointer', 'pointer', 'pointer', 'pointer', '...', "
          "'uint8', 'pointer', 'uint8']);"
      "const val = NULL.not();"
      "send(f(val, val, val, val, 13, val, 37));",
      gum_assert_variadic_uint8_values_are_sane);
  EXPECT_SEND_MESSAGE_WITH ("42");
}

static gint
gum_assert_variadic_uint8_values_are_sane (gpointer a,
                                           gpointer b,
                                           gpointer c,
                                           gpointer d,
                                           ...)
{
  va_list args;
  gint e;
  gint g;

  va_start (args, d);
  e = va_arg (args, gint);
  va_arg (args, gpointer);
  g = va_arg (args, gint);
  va_end (args);

  g_assert_cmphex (e, ==, 13);
  g_assert_cmphex (g, ==, 37);

  return 42;
}

TESTCASE (variadic_native_function_float_args_should_be_promoted_to_double)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const sum = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', '...', 'pointer', 'float']);"
      "send(sum(ptr(3), NULL));"
      "send(sum(ptr(3), ptr(4), 42.0, NULL));"
      "send(sum(ptr(3), ptr(4), 42.0, ptr(100), 200.0, NULL));",
      gum_add_pointers_and_float_variadic);
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("49");
  EXPECT_SEND_MESSAGE_WITH ("349");
  EXPECT_NO_MESSAGES ();
}

#if defined (HAVE_WINDOWS) && GLIB_SIZEOF_VOID_P == 4

static int __fastcall gum_sum_three_fastcall (int a, int b, int c);
static int __stdcall gum_divide_by_two_stdcall (int n);

TESTCASE (native_function_should_support_fastcall)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['int', 'int', 'int'], "
          "{ abi: 'fastcall', exceptions: 'propagate' });"
      "send(f(10, 20, 12));",
      gum_sum_three_fastcall);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_function_should_support_stdcall)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'int', ['int'], "
          "{ abi: 'stdcall', exceptions: 'propagate' });"
      "send(f(42));",
      gum_divide_by_two_stdcall);
  EXPECT_SEND_MESSAGE_WITH ("21");
  EXPECT_NO_MESSAGES ();
}

static int __fastcall
gum_sum_three_fastcall (int a,
                        int b,
                        int c)
{
  return a + b + c;
}

static int __stdcall
gum_divide_by_two_stdcall (int n)
{
  return n / 2;
}

#endif

TESTCASE (native_function_is_a_native_pointer)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const toupper = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', 'int']);"
      "send(toupper instanceof NativePointer);"
      "send(toupper.toString() === " GUM_PTR_CONST ".toString());",
      gum_toupper, gum_toupper);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (system_function_can_be_invoked)
{
#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new SystemFunction(" GUM_PTR_CONST ", 'int', ['int']);"

      "let result = f(13);"
      "send(result.value);"
      "send(result.lastError);"

      "result = f(37);"
      "send(result.value);"
      "send(result.lastError);", gum_clobber_system_error);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new SystemFunction(" GUM_PTR_CONST ", 'int', ['int']);"

      "let result = f(13);"
      "send(result.value);"
      "send(result.errno);"

      "result = f(37);"
      "send(result.value);"
      "send(result.errno);", gum_clobber_system_error);
#endif

  EXPECT_SEND_MESSAGE_WITH ("26");
  EXPECT_SEND_MESSAGE_WITH ("13");

  EXPECT_SEND_MESSAGE_WITH ("74");
  EXPECT_SEND_MESSAGE_WITH ("37");

  EXPECT_NO_MESSAGES ();
}

TESTCASE (system_function_should_implement_call_and_apply)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new SystemFunction(" GUM_PTR_CONST ", 'int', []);"
      "send(f.call().value);"
      "send(f.call(f).value);"
      "send(f.apply(f).value);"
      "send(f.apply(f, undefined).value);"
      "send(f.apply(f, null).value);"
      "send(f.apply(f, []).value);",
      gum_get_answer_to_life_universe_and_everything);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const f = new SystemFunction(" GUM_PTR_CONST ", 'int', ['int']);"
      "send(SystemFunction.prototype.call(f, 42).value);"
      "send(SystemFunction.prototype.apply(f, [42]).value);"
      "send(f.call(undefined, 42).value);"
      "send(f.apply(undefined, [42]).value);"
      "send(f.call(null, 42).value);"
      "send(f.apply(null, [42]).value);"
      "send(f.call(f, 42).value);"
      "send(f.apply(f, [42]).value);"
      "send(f.call(ptr(" GUM_PTR_CONST "), 42).value);"
      "send(f.apply(ptr(" GUM_PTR_CONST "), [42]).value);",
      target_function_int, target_function_nested_a, target_function_nested_a);
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("1890");
  EXPECT_SEND_MESSAGE_WITH ("16855020");
  EXPECT_SEND_MESSAGE_WITH ("16855020");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const f = new SystemFunction(" GUM_PTR_CONST ", 'pointer', "
      "    ['pointer', 'int']);"
      "send(f.call(null, ptr(4), 3).value);"
      "send(f.apply(null, [ptr(4), 3]).value);",
      target_function_base_plus_offset);
  EXPECT_SEND_MESSAGE_WITH ("\"0x7\"");
  EXPECT_SEND_MESSAGE_WITH ("\"0x7\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (system_function_is_a_native_pointer)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const toupper = new SystemFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', 'int']);"
      "send(toupper instanceof NativePointer);"
      "send(toupper.toString() === " GUM_PTR_CONST ".toString());",
      gum_toupper, gum_toupper);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

static gint
gum_clobber_system_error (gint value)
{
#ifdef HAVE_WINDOWS
  SetLastError (value);
#else
  errno = value;
#endif

  return value * 2;
}

TESTCASE (native_callback_can_be_invoked)
{
  gint (* toupper_impl) (gchar * str, gint limit);
  gchar str[7];

  COMPILE_AND_LOAD_SCRIPT (
      "const toupper = new NativeCallback((str, limit) => {"
      "  let count = 0;"
      "  while (count < limit || limit === -1) {"
      "    const p = str.add(count);"
      "    const b = p.readU8();"
      "    if (b === 0)"
      "      break;"
      "    p.writeU8(String.fromCharCode(b).toUpperCase().charCodeAt(0));"
      "    count++;"
      "  }"
      "  return (limit === -1) ? -count : count;"
      "}, 'int', ['pointer', 'int']);"
      "gc();"
      "send(toupper);");

  toupper_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (toupper_impl);

  strcpy (str, "badger");
  g_assert_cmpint (toupper_impl (str, 3), ==, 3);
  g_assert_cmpstr (str, ==, "BADger");
  g_assert_cmpint (toupper_impl (str, -1), ==, -6);
  g_assert_cmpstr (str, ==, "BADGER");
}

TESTCASE (native_callback_should_provide_access_to_system_error)
{
  void (* callback) (void);

#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "const cb = new NativeCallback(function () {"
      "  send(this.lastError);"
      "  this.lastError = this.lastError + 37;"
      "  return 0;"
      "}, 'void', []);"
      GUM_PTR_CONST ".writePointer(cb);", &callback);
  EXPECT_NO_MESSAGES ();

  SetLastError (1300);
  callback ();
  g_assert_cmpuint (GetLastError (), ==, 1337);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "const cb = new NativeCallback(function () {"
      "  send(this.errno);"
      "  this.errno = this.errno + 37;"
      "  return 0;"
      "}, 'void', []);"
      GUM_PTR_CONST ".writePointer(cb);", &callback);
  EXPECT_NO_MESSAGES ();

  errno = 1300;
  callback ();
  g_assert_cmpuint (errno, ==, 1337);
#endif

  EXPECT_SEND_MESSAGE_WITH ("1300");
}

TESTCASE (native_callback_is_a_native_pointer)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const cb = new NativeCallback(() => {}, 'void', []);"
      "send(cb instanceof NativePointer);");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (native_callback_memory_should_be_eagerly_reclaimed)
{
  guint usage_before, usage_after;
  gboolean difference_is_less_than_2x;

  COMPILE_AND_LOAD_SCRIPT (
      "let iterationsRemaining = null;"
      "recv('start', onStartRequest);"
      "function onStartRequest(message) {"
      "  iterationsRemaining = message.iterations;"
      "  processNext();"
      "}"
      "function processNext() {"
      "  const cb = new NativeCallback(() => {}, 'void', []);"
      "  if (--iterationsRemaining === 0) {"
      "    recv('start', onStartRequest);"
      "    gc();"
      "    send('done');"
      "  } else {"
      "    setTimeout(processNext, 0);"
      "  }"
      "}");
  EXPECT_NO_MESSAGES ();

  PUSH_TIMEOUT (20000);

  POST_MESSAGE ("{\"type\":\"start\",\"iterations\":5000}");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
  EXPECT_NO_MESSAGES ();

  usage_before = gum_peek_private_memory_usage ();

  POST_MESSAGE ("{\"type\":\"start\",\"iterations\":5000}");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
  EXPECT_NO_MESSAGES ();

  usage_after = gum_peek_private_memory_usage ();

  POP_TIMEOUT ();

  difference_is_less_than_2x = usage_after < usage_before * 2;
  if (!difference_is_less_than_2x)
  {
    g_printerr ("\n\n"
        "Oops, memory usage is not looking good:\n"
        "\tusage before: %u\n"
        "\t    vs after: %u\n\n",
        usage_before, usage_after);
    g_assert_true (difference_is_less_than_2x);
  }
}

TESTCASE (native_callback_should_be_kept_alive_during_calls)
{
  void (* cb) (void);

  COMPILE_AND_LOAD_SCRIPT (
      "let cb = new NativeCallback(() => {"
        "cb = null;"
        "gc();"
        "send('returning');"
      "}, 'void', []);"
      "Script.bindWeak(cb, () => { send('dead'); });"
      GUM_PTR_CONST ".writePointer(cb);",
      &cb);
  EXPECT_NO_MESSAGES ();

  cb ();
  EXPECT_SEND_MESSAGE_WITH ("\"returning\"");
  EXPECT_SEND_MESSAGE_WITH ("\"dead\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_callback_can_return_struct)
{
  TestNativeReturnStruct (* cb) (void);
  TestNativeReturnStruct retval;

  COMPILE_AND_LOAD_SCRIPT (
      "let cb = new NativeCallback(() => {"
        "return [1234, ptr(0x1234)];"
      "}, ['uint32', 'pointer'], []);"
      GUM_PTR_CONST ".writePointer(cb);",
      &cb);

  retval = cb ();

  g_assert_cmpuint (retval.a, ==, 1234);
  g_assert_cmphex (GPOINTER_TO_SIZE (retval.b), ==, 0x1234);
}

TESTCASE (native_callback_can_return_large_nested_struct)
{
  TestNativeReturnNestedStruct (* cb) (void);
  TestNativeReturnNestedStruct retval;

  COMPILE_AND_LOAD_SCRIPT (
      "let cb = new NativeCallback(() => {"
        "return [[1, 2, 3, 4, [5, 6, 7]], 8];"
      "}, [['int64', 'int64', 'int64', 'int64',"
            "['int64', 'int64', 'int64']], 'int64'], []);"
      GUM_PTR_CONST ".writePointer(cb);",
      &cb);

  retval = cb ();

  g_assert_cmpint (retval.a.a, ==, 1);
  g_assert_cmpint (retval.a.b, ==, 2);
  g_assert_cmpint (retval.a.c, ==, 3);
  g_assert_cmpint (retval.a.d, ==, 4);
  g_assert_cmpint (retval.a.e.a, ==, 5);
  g_assert_cmpint (retval.a.e.b, ==, 6);
  g_assert_cmpint (retval.a.e.c, ==, 7);
  g_assert_cmpint (retval.b, ==, 8);
}

TESTCASE (native_callback_should_zero_retval_on_mismatching_return_value)
{
  TestNativeReturnNestedStruct (* cb) (void);
  TestNativeReturnNestedStruct retval;
  const TestNativeReturnNestedStruct zero = { { 0, }, };

  COMPILE_AND_LOAD_SCRIPT (
      "let cb = new NativeCallback(() => [],"
          "[['int64', 'int64', 'int64', 'int64',"
            "['int64', 'int64', 'int64']], 'int64'], []);"
      GUM_PTR_CONST ".writePointer(cb);",
      &cb);

  retval = cb ();

  g_assert_cmpmem (&retval, sizeof (retval), &zero, sizeof (zero));
  EXPECT_ERROR_MESSAGE_MATCHING (ANY_LINE_NUMBER, "Error:.*array.*length");
}

#ifdef HAVE_WINDOWS

# if GLIB_SIZEOF_VOID_P == 4

TESTCASE (native_callback_should_support_fastcall)
{
  int (__fastcall * cb) (int, int, int);

  COMPILE_AND_LOAD_SCRIPT (
      "const cb = new NativeCallback((a, b, c) => {"
              "send([a, b, c]);"
              "return a + b + c;"
          "}, 'int', ['int', 'int', 'int'], 'fastcall');"
      GUM_PTR_CONST ".writePointer(cb);",
      &cb);
  EXPECT_NO_MESSAGES ();

  g_assert_cmpint (cb (10, 20, 12), ==, 42);
  EXPECT_SEND_MESSAGE_WITH ("[10,20,12]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_callback_should_support_stdcall)
{
  int (__stdcall * cb) (int);

  COMPILE_AND_LOAD_SCRIPT (
      "const cb = new NativeCallback(n => { send(n); return n / 2; }, 'int', "
          "['int'], 'stdcall');"
      GUM_PTR_CONST ".writePointer(cb);",
      &cb);
  EXPECT_NO_MESSAGES ();

  g_assert_cmpint (cb (42), ==, 21);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
}

# endif

GUM_NOINLINE static void *
sample_return_address (void)
{
# ifdef _MSC_VER
  return _ReturnAddress ();
# else
  return __builtin_return_address (0);
# endif
}

TESTCASE (native_callback_should_get_accurate_backtraces)
{
  void (* cb) (void);
  void * ret_address = sample_return_address ();

  COMPILE_AND_LOAD_SCRIPT (
      "const min = " GUM_PTR_CONST ";"
      "const max = min.add(128);"
      "const cb = new NativeCallback(function () {"
      "  if (this.returnAddress.compare(min) > 0 &&"
      "      this.returnAddress.compare(max) < 0) {"
      "    send('return address ok');"
      "  } else {"
      "    send('return address error');"
      "  }"
      "}, 'void', []);"
      GUM_PTR_CONST ".writePointer(cb);",
      ret_address, &cb);
  EXPECT_NO_MESSAGES ();

  cb ();
  EXPECT_SEND_MESSAGE_WITH ("\"return address ok\"");
  EXPECT_NO_MESSAGES ();
}

#endif

#ifdef G_OS_UNIX

#define GUM_TEMP_FAILURE_RETRY(expression) \
    ({ \
      gssize __result; \
      \
      do __result = (gssize) (expression); \
      while (__result == -EINTR); \
      \
      __result; \
    })

TESTCASE (unix_fd_can_be_read_from)
{
  gint fds[2];
  const guint8 message[7] = { 0x13, 0x37, 0xca, 0xfe, 0xba, 0xbe, 0xff };
  gssize res;

  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, fds), ==, 0);

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixInputStream(%d, { autoClose: false });"
      "    const buf = await stream.read(1337);"
      "    send(buf.byteLength, buf);"
      "  } catch (e) {"
      "    send(`oops: ${e.stack}`);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_NO_MESSAGES ();
  res = GUM_TEMP_FAILURE_RETRY (write (fds[1], message, 1));
  g_assert_cmpint (res, ==, 1);
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("1", "13");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixInputStream(%d, { autoClose: false });"
      "    const buf = await stream.readAll(7);"
      "    send(buf.byteLength, buf);"
      "  } catch (e) {"
      "    send(`oops: ${e.stack}`);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_NO_MESSAGES ();
  res = GUM_TEMP_FAILURE_RETRY (write (fds[1], message, 4));
  g_assert_cmpint (res, ==, 4);
  g_usleep (G_USEC_PER_SEC / 20);
  EXPECT_NO_MESSAGES ();
  res = GUM_TEMP_FAILURE_RETRY (write (fds[1], message + 4, 3));
  g_assert_cmpint (res, ==, 3);
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("7", "13 37 ca fe ba be ff");
  EXPECT_NO_MESSAGES ();

  res = GUM_TEMP_FAILURE_RETRY (write (fds[1], message, 2));
  g_assert_cmpint (res, ==, 2);
  close (fds[1]);
  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixInputStream(%d, { autoClose: false });"
      "    await stream.readAll(7);"
      "  } catch (e) {"
      "    send(e.toString(), e.partialData);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_SEND_MESSAGE_WITH ("\"Error: short read\"", "13 37");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixInputStream(%d, { autoClose: false });"
      "    const success = await stream.close();"
      "    send(success);"
      "    await stream.read(1337);"
      "  } catch (e) {"
      "    send(e.toString());"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"Error: stream is already closed\"");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixInputStream(%d, { autoClose: false });"
      "    let success = await stream.close();"
      "    send(success);"
      "    success = await stream.close();"
      "    send(success);"
      "  } catch (e) {"
      "    send(`oops: ${e.stack}`);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  close (fds[0]);
}

TESTCASE (unix_fd_can_be_written_to)
{
  gint fds[2];
  guint8 buffer[8];
  sig_t original_sigpipe_handler;

  if (gum_process_is_debugger_attached ())
  {
    g_print ("<skipping, debugger is attached> ");
    return;
  }

  original_sigpipe_handler = signal (SIGPIPE, SIG_IGN);

  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, fds), ==, 0);

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixOutputStream(%d, { autoClose: false });"
      "    const size = await stream.write([0x13]);"
      "    send(size);"
      "  } catch (e) {"
      "    send(`oops: ${e.stack}`);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (read (fds[1], buffer, sizeof (buffer)), ==, 1);
  g_assert_cmphex (buffer[0], ==, 0x13);

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixOutputStream(%d, { autoClose: false });"
      "    const size = await stream.writeAll(["
      "        0x13, 0x37,"
      "        0xca, 0xfe, 0xba, 0xbe,"
      "        0xff"
      "    ]);"
      "    send(size);"
      "  } catch (e) {"
      "    send(`oops: ${e.stack}`);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (read (fds[1], buffer, sizeof (buffer)), ==, 7);
  g_assert_cmphex (buffer[0], ==, 0x13);
  g_assert_cmphex (buffer[1], ==, 0x37);
  g_assert_cmphex (buffer[2], ==, 0xca);
  g_assert_cmphex (buffer[3], ==, 0xfe);
  g_assert_cmphex (buffer[4], ==, 0xba);
  g_assert_cmphex (buffer[5], ==, 0xbe);
  g_assert_cmphex (buffer[6], ==, 0xff);

  close (fds[1]);

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const stream = new UnixOutputStream(%d, { autoClose: false });"
      "    await stream.writeAll(["
      "        0x13, 0x37,"
      "        0xca, 0xfe, 0xba, 0xbe,"
      "        0xff"
      "    ]);"
      "  } catch (e) {"
      "    send(e.partialSize);"
      "  }"
      "}"
      "run();",
      fds[0]);
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_NO_MESSAGES ();

  close (fds[0]);

  signal (SIGPIPE, original_sigpipe_handler);
}

#endif

TESTCASE (basic_hexdump_functionality_is_available)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const str = Memory.allocUtf8String(\"Hello hex world! w00t\");"
      "const buf = str.readByteArray(22);"
      "send(hexdump(buf));");
  EXPECT_SEND_MESSAGE_WITH ("\""
      "           0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  "
          "0123456789ABCDEF\\n"
      "00000000  48 65 6c 6c 6f 20 68 65 78 20 77 6f 72 6c 64 21  "
          "Hello hex world!\\n"
      "00000010  20 77 30 30 74 00                                "
          " w00t.\"");

  COMPILE_AND_LOAD_SCRIPT (
      "const str = Memory.allocUtf8String(\"Hello hex world! w00t\");"
      "send(hexdump(str, { address: uint64('0x100000000'), length: 22 }));");
  EXPECT_SEND_MESSAGE_WITH ("\""
      "            0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  "
          "0123456789ABCDEF\\n"
      "100000000  48 65 6c 6c 6f 20 68 65 78 20 77 6f 72 6c 64 21  "
          "Hello hex world!\\n"
      "100000010  20 77 30 30 74 00                                "
          " w00t.\"");
}

TESTCASE (hexdump_supports_native_pointer_conforming_object)
{
  const gchar * message = "Hello hex world!";

  COMPILE_AND_LOAD_SCRIPT (
      "const obj = { handle: " GUM_PTR_CONST "  };"
      "send(hexdump(obj, { address: NULL, length: 16 }));", message);
  EXPECT_SEND_MESSAGE_WITH ("\""
      "           0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  "
          "0123456789ABCDEF\\n"
      "00000000  48 65 6c 6c 6f 20 68 65 78 20 77 6f 72 6c 64 21  "
          "Hello hex world!\"");
}

TESTCASE (native_pointer_provides_is_null)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(ptr(\"0\").isNull());"
      "send(ptr(\"1337\").isNull());");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("false");
}

TESTCASE (native_pointer_provides_arithmetic_operations)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(ptr(3).add(4).toInt32());"
      "send(ptr(7).sub(4).toInt32());"
      "send(ptr(6).and(3).toInt32());"
      "send(ptr(6).or(3).toInt32());"
      "send(ptr(6).xor(3).toInt32());"
      "send(ptr(63).shr(4).toInt32());"
      "send(ptr(1).shl(3).toInt32());"
      "send(ptr(0).not().toInt32());");
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_SEND_MESSAGE_WITH ("5");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("8");
  EXPECT_SEND_MESSAGE_WITH ("-1");
}

TESTCASE (native_pointer_provides_uint32_conversion_functionality)
{
  COMPILE_AND_LOAD_SCRIPT ("send(ptr(1).toUInt32());");
  EXPECT_SEND_MESSAGE_WITH ("1");
}

TESTCASE (native_pointer_provides_ptrauth_functionality)
{
#ifdef HAVE_PTRAUTH
  COMPILE_AND_LOAD_SCRIPT (
      "const original = ptr(1);"

      "const a = original.sign();"
      "send(a.equals(original));"
      "send(a.strip().equals(original));"

      "send(original.sign('ia').equals(a));"
      "send(original.sign('ib').equals(a));"
      "send(original.sign('da').equals(a));"
      "send(original.sign('db').equals(a));"

      "const b = original.sign('ia', ptr(1337));"
      "send(b.equals(a));"
      "const c = original.sign('ia', 1337);"
      "send(c.equals(b));"
      "const d = original.sign('ia', ptr(1337).blend(42));"
      "send(d.equals(b));"

      "try {"
          "original.sign('x');"
      "} catch (e) {"
          "send(e.message);"
      "}");

  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("false");

  EXPECT_SEND_MESSAGE_WITH ("false");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("false");

  EXPECT_SEND_MESSAGE_WITH ("\"invalid key\"");
#else
  COMPILE_AND_LOAD_SCRIPT (
      "const original = ptr(1);"
      "send(original.sign() === original);"
      "send(original.strip() === original);"
      "send(original.blend(42) === original);");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
#endif

  EXPECT_NO_MESSAGES ();
}

TESTCASE (native_pointer_provides_arm_tbi_functionality)
{
#if defined (HAVE_ANDROID) && defined (HAVE_ARM64)
  void * block = malloc (1);

  if (GUM_ADDRESS (block) >> 56 != 0)
  {
    COMPILE_AND_LOAD_SCRIPT (
        "const original = " GUM_PTR_CONST ";"
        "const expected = original.and(ptr(0xff).shl(56).not());"
        "send(original.strip().equals(expected));",
        block);
    EXPECT_SEND_MESSAGE_WITH ("true");
    EXPECT_NO_MESSAGES ();
  }
  else
  {
    g_print ("<skipping on this device> ");
  }

  free (block);
#else
  g_print ("<skipping on this platform> ");
#endif
}

TESTCASE (native_pointer_to_match_pattern)
{
  const gchar * extra_checks;

#if GLIB_SIZEOF_VOID_P == 4
  extra_checks = "";
#else
  extra_checks = "send(ptr(\"0xa1b2c3d4e5f6a7b8\").toMatchPattern());";
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "send(ptr(\"0x0\").toMatchPattern());"
      "send(ptr(\"0xa\").toMatchPattern());"
      "send(ptr(\"0xa1b\").toMatchPattern());"
      "send(ptr(\"0xa1b2\").toMatchPattern());"
      "send(ptr(\"0xa1b2c3\").toMatchPattern());"
      "send(ptr(\"0xa1b2c3d4\").toMatchPattern());"
      "%s",
      extra_checks);

#if GLIB_SIZEOF_VOID_P == 4
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"0a 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"1b 0a 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"b2 a1 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"c3 b2 a1 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"d4 c3 b2 a1\"");
# else
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 0a\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 0a 1b\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 a1 b2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 a1 b2 c3\"");
  EXPECT_SEND_MESSAGE_WITH ("\"a1 b2 c3 d4\"");
# endif
#else
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"0a 00 00 00 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"1b 0a 00 00 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"b2 a1 00 00 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"c3 b2 a1 00 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"d4 c3 b2 a1 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"b8 a7 f6 e5 d4 c3 b2 a1\"");
# else
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 00 00 00 00\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 00 00 00 0a\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 00 00 0a 1b\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 00 00 a1 b2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 00 a1 b2 c3\"");
  EXPECT_SEND_MESSAGE_WITH ("\"00 00 00 00 a1 b2 c3 d4\"");
  EXPECT_SEND_MESSAGE_WITH ("\"a1 b2 c3 d4 e5 f6 a7 b8\"");
# endif
#endif
}

TESTCASE (native_pointer_can_be_constructed_from_64bit_value)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(ptr(uint64(0x1ffffffff)).equals(ptr(0x1ffffffff)));"
      "send(ptr(int64(0x2ffffffff)).equals(ptr(0x2ffffffff)));");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");

#if GLIB_SIZEOF_VOID_P == 4
  COMPILE_AND_LOAD_SCRIPT (
      "send(ptr(int64(-150450112)).equals(ptr('0xf7085040')));");
  EXPECT_SEND_MESSAGE_WITH ("true");
#elif GLIB_SIZEOF_VOID_P == 8
  COMPILE_AND_LOAD_SCRIPT (
      "send(ptr(int64(-1)).equals(ptr('0xffffffffffffffff')));");
  EXPECT_SEND_MESSAGE_WITH ("true");
#endif
}

TESTCASE (native_pointer_should_be_serializable_to_json)
{
  COMPILE_AND_LOAD_SCRIPT ("send(ptr(1).toJSON());");
  EXPECT_SEND_MESSAGE_WITH ("\"0x1\"");
}

TESTCASE (s8_can_be_read)
{
  gint8 val = -42;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readS8());", &val);
  EXPECT_SEND_MESSAGE_WITH ("-42");
}

TESTCASE (s8_can_be_written)
{
  gint8 val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeS8(-42);", &val);
  g_assert_cmpint (val, ==, -42);
}

TESTCASE (u8_can_be_read)
{
  guint8 val = 42;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readU8());", &val);
  EXPECT_SEND_MESSAGE_WITH ("42");
}

TESTCASE (u8_can_be_written)
{
  guint8 val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeU8(42);", &val);
  g_assert_cmpint (val, ==, 42);
}

TESTCASE (s16_can_be_read)
{
  gint16 val = -12123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readS16());", &val);
  EXPECT_SEND_MESSAGE_WITH ("-12123");
}

TESTCASE (s16_can_be_written)
{
  gint16 val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeS16(-12123);", &val);
  g_assert_cmpint (val, ==, -12123);
}

TESTCASE (u16_can_be_read)
{
  guint16 val = 12123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readU16());", &val);
  EXPECT_SEND_MESSAGE_WITH ("12123");
}

TESTCASE (u16_can_be_written)
{
  guint16 val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeU16(12123);", &val);
  g_assert_cmpint (val, ==, 12123);
}

TESTCASE (s32_can_be_read)
{
  gint32 val = -120123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readS32());", &val);
  EXPECT_SEND_MESSAGE_WITH ("-120123");
}

TESTCASE (s32_can_be_written)
{
  gint32 val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeS32(-120123);", &val);
  g_assert_cmpint (val, ==, -120123);
}

TESTCASE (u32_can_be_read)
{
  guint32 val = 120123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readU32());", &val);
  EXPECT_SEND_MESSAGE_WITH ("120123");
}

TESTCASE (u32_can_be_written)
{
  guint32 val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeU32(120123);", &val);
  g_assert_cmpint (val, ==, 120123);
}

TESTCASE (s64_can_be_read)
{
  gint64 val = G_GINT64_CONSTANT (-1201239876783);
  COMPILE_AND_LOAD_SCRIPT (
      "const value = " GUM_PTR_CONST ".readS64();"
      "send(value instanceof Int64);"
      "send(value);",
      &val);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"-1201239876783\"");
}

TESTCASE (s64_can_be_written)
{
  gint64 val = 0;
  COMPILE_AND_LOAD_SCRIPT (
      GUM_PTR_CONST ".writeS64(int64('-1201239876783'));", &val);
  g_assert_cmpint (val, ==, G_GINT64_CONSTANT (-1201239876783));
}

TESTCASE (u64_can_be_read)
{
  guint64 val = G_GUINT64_CONSTANT (1201239876783);
  COMPILE_AND_LOAD_SCRIPT (
      "const value = " GUM_PTR_CONST ".readU64();"
      "send(value instanceof UInt64);"
      "send(value);",
      &val);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"1201239876783\"");
}

TESTCASE (u64_can_be_written)
{
  gint64 val = 0;
  COMPILE_AND_LOAD_SCRIPT (
      GUM_PTR_CONST ".writeU64(uint64('1201239876783'));", &val);
  g_assert_cmpint (val, ==, G_GUINT64_CONSTANT (1201239876783));
}

TESTCASE (short_can_be_read)
{
  short val = -12123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readShort());", &val);
  EXPECT_SEND_MESSAGE_WITH ("-12123");
}

TESTCASE (short_can_be_written)
{
  short val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeShort(-12123);", &val);
  g_assert_cmpint (val, ==, -12123);

  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeShort(int64(-1234));", &val);
  g_assert_cmpint (val, ==, -1234);
}

TESTCASE (ushort_can_be_read)
{
  unsigned short val = 12123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUShort());", &val);
  EXPECT_SEND_MESSAGE_WITH ("12123");
}

TESTCASE (ushort_can_be_written)
{
  unsigned short val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeUShort(12123);", &val);
  g_assert_cmpint (val, ==, 12123);

  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeUShort(uint64(1234));", &val);
  g_assert_cmpint (val, ==, 1234);
}

TESTCASE (int_can_be_read)
{
  int val = -120123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readInt());", &val);
  EXPECT_SEND_MESSAGE_WITH ("-120123");
}

TESTCASE (int_can_be_written)
{
  int val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeInt(-120123);", &val);
  g_assert_cmpint (val, ==, -120123);
}

TESTCASE (uint_can_be_read)
{
  unsigned int val = 120123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUInt());", &val);
  EXPECT_SEND_MESSAGE_WITH ("120123");
}

TESTCASE (uint_can_be_written)
{
  unsigned int val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeUInt(120123);", &val);
  g_assert_cmpint (val, ==, 120123);
}

TESTCASE (long_can_be_read)
{
  long val = -123;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readLong());", &val);
  EXPECT_SEND_MESSAGE_WITH ("\"-123\"");
}

TESTCASE (long_can_be_written)
{
  long val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeLong(1350966097);", &val);
  g_assert_cmpint (val, ==, 1350966097);
}

TESTCASE (ulong_can_be_read)
{
  unsigned long val = 4294967295UL;
  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readULong());", &val);
  EXPECT_SEND_MESSAGE_WITH ("\"4294967295\"");
}

TESTCASE (ulong_can_be_written)
{
  unsigned long val = 0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeULong(4294967295);", &val);
  g_assert_cmpint (val, ==, 4294967295UL);
}

TESTCASE (float_can_be_read)
{
  float val = 123.456f;
  COMPILE_AND_LOAD_SCRIPT ("send(Math.abs(" GUM_PTR_CONST ".readFloat()"
      " - 123.456) < 0.00001);", &val);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (float_can_be_written)
{
  float val = 0.f;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeFloat(123.456);", &val);
  g_assert_cmpfloat (ABS (val - 123.456f), <, 0.00001f);
}

TESTCASE (double_can_be_read)
{
  double val = 123.456;
  COMPILE_AND_LOAD_SCRIPT ("send(Math.abs(" GUM_PTR_CONST ".readDouble()"
      " - 123.456) < 0.00001);", &val);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (double_can_be_written)
{
  double val = 0.0;
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeDouble(123.456);", &val);
  g_assert_cmpfloat (ABS (val - 123.456), <, 0.00001);
}

TESTCASE (byte_array_can_be_read)
{
  guint8 buf[3] = { 0x13, 0x37, 0x42 };
  COMPILE_AND_LOAD_SCRIPT (
      "const buffer = " GUM_PTR_CONST ".readByteArray(3);"
      "send('badger', buffer);"
      "send('badger', " GUM_PTR_CONST ".readByteArray(int64(3)));"
      "send('badger', " GUM_PTR_CONST ".readByteArray(uint64(3)));"
      "const emptyBuffer = " GUM_PTR_CONST ".readByteArray(0);"
      "send('snake', emptyBuffer);"
      "send(buffer instanceof ArrayBuffer);"
      "send(emptyBuffer instanceof ArrayBuffer);",
      buf, buf, buf, buf);
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"badger\"", "13 37 42");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"badger\"", "13 37 42");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"badger\"", "13 37 42");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"snake\"", "");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (byte_array_can_be_written)
{
  guint8 val[4] = { 0x00, 0x00, 0x00, 0xff };
  const guint8 other[3] = { 0x01, 0x02, 0x03 };
  guint16 shorts[2] = { 0x1111, 0x2222 };

  COMPILE_AND_LOAD_SCRIPT (
      GUM_PTR_CONST ".writeByteArray([0x13, 0x37, 0x42]);",
      val);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (val[0], ==, 0x13);
  g_assert_cmpint (val[1], ==, 0x37);
  g_assert_cmpint (val[2], ==, 0x42);
  g_assert_cmpint (val[3], ==, 0xff);

  COMPILE_AND_LOAD_SCRIPT (
      "const other = " GUM_PTR_CONST ".readByteArray(3);"
      GUM_PTR_CONST ".writeByteArray(other);",
      other, val);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (val[0], ==, 0x01);
  g_assert_cmpint (val[1], ==, 0x02);
  g_assert_cmpint (val[2], ==, 0x03);
  g_assert_cmpint (val[3], ==, 0xff);

  COMPILE_AND_LOAD_SCRIPT (
      "const bytes = new Uint8Array(2);"
      "bytes[0] = 4;"
      "bytes[1] = 5;"
      GUM_PTR_CONST ".writeByteArray(bytes);",
      val);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (val[0], ==, 0x04);
  g_assert_cmpint (val[1], ==, 0x05);
  g_assert_cmpint (val[2], ==, 0x03);

  COMPILE_AND_LOAD_SCRIPT (
      "const shorts = new Uint16Array(1);"
      "shorts[0] = 0x4242;"
      GUM_PTR_CONST ".writeByteArray(shorts);",
      shorts);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (shorts[0], ==, 0x4242);
  g_assert_cmpint (shorts[1], ==, 0x2222);
}

TESTCASE (c_string_can_be_read)
{
  const gchar * str = "Hello";
  const gchar * uni = "Bjøærheimsbygd";

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString());",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Hello\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString(3));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Hel\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString(0));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString(-1));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Hello\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString(int64(-1)));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Hello\"");

  COMPILE_AND_LOAD_SCRIPT ("send(ptr('0').readCString());", str);
  EXPECT_SEND_MESSAGE_WITH ("null");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString(4));", uni);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjø\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readCString(3));", uni);
  EXPECT_SEND_MESSAGE_WITH ("\"Bj\357\277\275\"");
}

TESTCASE (utf8_string_can_be_read)
{
  const gchar * str = "Bjøærheimsbygd";

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf8String());", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjøærheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf8String(4));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjø\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf8String(0));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf8String(-1));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjøærheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf8String(int64(-1)));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjøærheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(ptr('0').readUtf8String());", str);
  EXPECT_SEND_MESSAGE_WITH ("null");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf8String(3));", str);
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: can't decode byte 0xc3 in position 2");
}

TESTCASE (utf8_string_can_be_written)
{
  gchar str[6];

  strcpy (str, "Hello");
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeUtf8String('Bye');", str);
  g_assert_cmpstr (str, ==, "Bye");
  g_assert_cmphex (str[4], ==, 'o');
  g_assert_cmphex (str[5], ==, '\0');
}

TESTCASE (utf16_string_can_be_read)
{
  const gchar * str_utf8 = "Bjørheimsbygd";
  gunichar2 * str = g_utf8_to_utf16 (str_utf8, -1, NULL, NULL, NULL);

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf16String());", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf16String(3));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjø\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf16String(0));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readUtf16String(-1));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send("
      GUM_PTR_CONST ".readUtf16String(int64(-1))"
      ");",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(ptr('0').readUtf16String());", str);
  EXPECT_SEND_MESSAGE_WITH ("null");

  g_free (str);
}

TESTCASE (utf16_string_can_be_written)
{
  gunichar2 * str = g_utf8_to_utf16 ("Hello", -1, NULL, NULL, NULL);

  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeUtf16String('Bye');", str);
  g_assert_cmphex (str[0], ==, 'B');
  g_assert_cmphex (str[1], ==, 'y');
  g_assert_cmphex (str[2], ==, 'e');
  g_assert_cmphex (str[3], ==, '\0');
  g_assert_cmphex (str[4], ==, 'o');
  g_assert_cmphex (str[5], ==, '\0');

  g_free (str);
}

#ifdef HAVE_WINDOWS

TESTCASE (ansi_string_can_be_read_in_code_page_936)
{
  CPINFOEX cpi;
  const gchar * str_utf8;
  WCHAR * str_utf16;
  gchar str[13 + 1];

  GetCPInfoEx (CP_THREAD_ACP, 0, &cpi);
  if (cpi.CodePage != 936)
  {
    g_print ("<skipping, only available on systems with ANSI code page 936> ");
    return;
  }

  str_utf8 = "test测试.";
  str_utf16 = g_utf8_to_utf16 (str_utf8, -1, NULL, NULL, NULL);
  WideCharToMultiByte (CP_THREAD_ACP, 0, str_utf16, -1, str, sizeof (str),
      NULL, NULL);

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(" GUM_PTR_CONST "));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"test测试.\"");

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(" GUM_PTR_CONST
      ", 5));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"test?\"");

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(" GUM_PTR_CONST
      ", 6));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"test测\"");

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(" GUM_PTR_CONST
      ", 0));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"\"");

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(" GUM_PTR_CONST
      ", -1));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"test测试.\"");

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(" GUM_PTR_CONST
      ", int64(-1)));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"test测试.\"");

  COMPILE_AND_LOAD_SCRIPT ("send(Memory.readAnsiString(ptr(\"0\")));", str);
  EXPECT_SEND_MESSAGE_WITH ("null");

  g_free (str_utf16);

  str_utf8 = "Bjørheimsbygd";
  str_utf16 = g_utf8_to_utf16 (str_utf8, -1, NULL, NULL, NULL);
  WideCharToMultiByte (CP_THREAD_ACP, 0, str_utf16, -1, str, sizeof (str),
      NULL, NULL);

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readAnsiString());", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bj?rheimsbygd\"");

  g_free (str_utf16);
}

TESTCASE (ansi_string_can_be_read_in_code_page_1252)
{
  CPINFOEX cpi;
  const gchar * str_utf8;
  WCHAR * str_utf16;
  gchar str[13 + 1];

  GetCPInfoEx (CP_THREAD_ACP, 0, &cpi);
  if (cpi.CodePage != 1252)
  {
    g_print ("<skipping, only available on systems with ANSI code page 1252> ");
    return;
  }

  str_utf8 = "Bjørheimsbygd";
  str_utf16 = g_utf8_to_utf16 (str_utf8, -1, NULL, NULL, NULL);
  WideCharToMultiByte (CP_THREAD_ACP, 0, str_utf16, -1, str, sizeof (str),
      NULL, NULL);

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readAnsiString());", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readAnsiString(3));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjø\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readAnsiString(0));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readAnsiString(-1));", str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(" GUM_PTR_CONST ".readAnsiString(int64(-1)));",
      str);
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");

  COMPILE_AND_LOAD_SCRIPT ("send(ptr('0').readAnsiString());", str);
  EXPECT_SEND_MESSAGE_WITH ("null");

  g_free (str_utf16);
}

TESTCASE (ansi_string_can_be_written_in_code_page_936)
{
  CPINFOEX cpi;
  gchar str_ansi[13 + 1];
  gunichar2 str_utf16[13 + 1];
  gchar * str_utf8;

  GetCPInfoEx (CP_THREAD_ACP, 0, &cpi);
  if (cpi.CodePage != 936)
  {
    g_print ("<skipping, only available on systems with ANSI code page 936> ");
    return;
  }

  strcpy (str_ansi, "truncate-plz");
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeAnsiString('test测试.');",
      str_ansi);
  MultiByteToWideChar (CP_THREAD_ACP, 0, str_ansi, -1,
      str_utf16, sizeof (str_utf16));
  str_utf8 = g_utf16_to_utf8 (str_utf16, -1, NULL, NULL, NULL);
  g_assert_cmpstr (str_utf8, ==, "test测试.");
  g_free (str_utf8);
  g_assert_cmphex (str_ansi[9], ==, '\0');
  g_assert_cmphex (str_ansi[10], ==, 'l');
  g_assert_cmphex (str_ansi[11], ==, 'z');
  g_assert_cmphex (str_ansi[12], ==, '\0');

  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeAnsiString('Bjørheimsbygd');",
      str_ansi);
  MultiByteToWideChar (CP_THREAD_ACP, 0, str_ansi, -1,
      str_utf16, sizeof (str_utf16));
  str_utf8 = g_utf16_to_utf8 (str_utf16, -1, NULL, NULL, NULL);
  g_assert_cmpstr (str_utf8, ==, "Bj?rheimsbygd");
  g_free (str_utf8);
}

TESTCASE (ansi_string_can_be_written_in_code_page_1252)
{
  CPINFOEX cpi;
  gchar str_ansi[16 + 1];
  gunichar2 str_utf16[16 + 1];
  gchar * str_utf8;

  GetCPInfoEx (CP_THREAD_ACP, 0, &cpi);
  if (cpi.CodePage != 1252)
  {
    g_print ("<skipping, only available on systems with ANSI code page 1252> ");
    return;
  }

  strcpy (str_ansi, "Kjempeforhaustar");
  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeAnsiString('Bjørheimsbygd');",
      str_ansi);
  MultiByteToWideChar (CP_THREAD_ACP, 0, str_ansi, -1,
      str_utf16, sizeof (str_utf16));
  str_utf8 = g_utf16_to_utf8 (str_utf16, -1, NULL, NULL, NULL);
  g_assert_cmpstr (str_utf8, ==, "Bjørheimsbygd");
  g_free (str_utf8);
  g_assert_cmphex (str_ansi[13], ==, '\0');
  g_assert_cmphex (str_ansi[14], ==, 'a');
  g_assert_cmphex (str_ansi[15], ==, 'r');
  g_assert_cmphex (str_ansi[16], ==, '\0');
}

#endif

TESTCASE (read_from_unaccessible_memory_can_be_performed_safely)
{
  const guint8 buf[3] = { 0x13, 0x37, 0x42 };

  COMPILE_AND_LOAD_SCRIPT ("send(0, " GUM_PTR_CONST ".readVolatile(3));", buf);
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("0", "13 37 42");

  COMPILE_AND_LOAD_SCRIPT ("ptr(1234).readVolatile(3);");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: memory read failed");
}

TESTCASE (write_to_unaccessible_memory_can_be_performed_safely)
{
  guint8 buf[3] = { 0x13, 0x37, 0x42 };

  COMPILE_AND_LOAD_SCRIPT (GUM_PTR_CONST ".writeVolatile([0x12, 0x27]);", buf);
  EXPECT_NO_MESSAGES ();
  g_assert_cmphex (buf[0], ==, 0x12);
  g_assert_cmphex (buf[1], ==, 0x27);
  g_assert_cmphex (buf[2], ==, 0x42);

  COMPILE_AND_LOAD_SCRIPT ("ptr(1234).writeVolatile([0x12, 0x27]);");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: memory write failed");
}

TESTCASE (invalid_read_results_in_exception)
{
  const gchar * type_name[] = {
      "Pointer",
      "S8",
      "U8",
      "S16",
      "U16",
      "S32",
      "U32",
      "Float",
      "Double",
      "S64",
      "U64",
      "Utf8String",
      "Utf16String",
#ifdef HAVE_WINDOWS
      "AnsiString"
#endif
  };
  guint i;

  if (!check_exception_handling_testable ())
    return;

  for (i = 0; i != G_N_ELEMENTS (type_name); i++)
  {
    gchar * source;

    source = g_strconcat ("ptr('1328').read", type_name[i], "();", NULL);
    COMPILE_AND_LOAD_SCRIPT (source);

#if GLIB_SIZEOF_VOID_P == 8
    EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x530");
#else
    /*
     * On 32-bit platforms, when reading 64-bit values we must read 32-bits at a
     * time. The compiler is at liberty to read either the high or low part
     * first, and hence we may not fault on the first part of the value, but
     * rather on the second. The ordering is likely dependent on endianness.
     */
    EXPECT_ERROR_MESSAGE_MATCHING (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x53(0|4)");
#endif

    g_free (source);
  }
}

TESTCASE (invalid_write_results_in_exception)
{
  const gchar * primitive_type_name[] = {
      "S8",
      "U8",
      "S16",
      "U16",
      "S32",
      "U32",
      "Float",
      "Double",
      "S64",
      "U64"
  };
  const gchar * string_type_name[] = {
      "Utf8String",
      "Utf16String"
  };
  guint i;

  if (!check_exception_handling_testable ())
    return;

  for (i = 0; i != G_N_ELEMENTS (primitive_type_name); i++)
  {
    gchar * source;

    source = g_strconcat ("ptr('1328').write", primitive_type_name[i], "(13);",
        NULL);
    COMPILE_AND_LOAD_SCRIPT (source);

#if GLIB_SIZEOF_VOID_P == 8
    EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x530");
#else
    /* See note in previous test. */
    EXPECT_ERROR_MESSAGE_MATCHING (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x53(0|4)");
#endif

    g_free (source);
  }

  for (i = 0; i != G_N_ELEMENTS (string_type_name); i++)
  {
    gchar * source;

    source = g_strconcat ("ptr('1328').write", string_type_name[i], "('Hey');",
        NULL);
    COMPILE_AND_LOAD_SCRIPT (source);

#if GLIB_SIZEOF_VOID_P == 8
    EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x530");
#else
    /* See note in previous test. */
    EXPECT_ERROR_MESSAGE_MATCHING (ANY_LINE_NUMBER,
        "Error: access violation accessing 0x53(0|4)");
#endif

    g_free (source);
  }
}

TESTCASE (invalid_read_write_execute_results_in_exception)
{
  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT ("ptr('1328').readU8();");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: access violation accessing 0x530");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT ("ptr('1328').writeU8(42);");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: access violation accessing 0x530");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT ("const data = Memory.alloc(Process.pageSize);"
      "const f = new NativeFunction(data.sign(), 'void', []);"
      "try {"
      "  f();"
      "} catch (e) {"
      "  send(e.toString().startsWith('Error: access violation accessing 0x'));"
      "}");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (array_buffer_can_wrap_memory_region)
{
  guint8 val[2] = { 13, 37 };

  COMPILE_AND_LOAD_SCRIPT (
      "const val = new Uint8Array(ArrayBuffer.wrap(" GUM_PTR_CONST ", 2));"
      "send(val.length);"
      "send(val[0]);"
      "send(val[1]);"
      "val[0] = 42;"
      "val[1] = 24;",
      val);
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("13");
  EXPECT_SEND_MESSAGE_WITH ("37");
  g_assert_cmpint (val[0], ==, 42);
  g_assert_cmpint (val[1], ==, 24);

  COMPILE_AND_LOAD_SCRIPT (
      "const val = new Uint8Array(ArrayBuffer.wrap(" GUM_PTR_CONST ", 0));"
      "send(val.length);"
      "send(typeof val[0]);",
      val);
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");

  COMPILE_AND_LOAD_SCRIPT (
      "const val = new Uint8Array(ArrayBuffer.wrap(NULL, 0));"
      "send(val.length);"
      "send(typeof val[0]);");
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
}

TESTCASE (array_buffer_can_be_unwrapped)
{
  gchar str[5 + 1];

  COMPILE_AND_LOAD_SCRIPT (
      "const toupper = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', 'int']);"
      "const buf = new ArrayBuffer(2 + 1);"
      "const bytes = new Uint8Array(buf);"
      "bytes[0] = 'h'.charCodeAt(0);"
      "bytes[1] = 'i'.charCodeAt(0);"
      "send(toupper(buf.unwrap(), -1));"
      "send(bytes[0]);"
      "send(bytes[1]);",
      gum_toupper, str);
  EXPECT_SEND_MESSAGE_WITH ("-2");
  EXPECT_SEND_MESSAGE_WITH ("72");
  EXPECT_SEND_MESSAGE_WITH ("73");
  EXPECT_NO_MESSAGES ();

  strcpy (str, "snake");
  COMPILE_AND_LOAD_SCRIPT (
      "const toupper = new NativeFunction(" GUM_PTR_CONST ", "
          "'int', ['pointer', 'int']);"
      "const buf = ArrayBuffer.wrap(" GUM_PTR_CONST ", 5 + 1);"
      "send(toupper(buf.unwrap(), -1));",
      gum_toupper, str);
  EXPECT_SEND_MESSAGE_WITH ("-5");
  EXPECT_NO_MESSAGES ();
  g_assert_cmpstr (str, ==, "SNAKE");
}

TESTCASE (uint64_provides_arithmetic_operations)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(uint64(3).add(4).toNumber());"
      "send(uint64(7).sub(4).toNumber());"
      "send(uint64(6).and(3).toNumber());"
      "send(uint64(6).or(3).toNumber());"
      "send(uint64(6).xor(3).toNumber());"
      "send(uint64(63).shr(4).toNumber());"
      "send(uint64(1).shl(3).toNumber());"
      "send(uint64(0).not().toString());");
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_SEND_MESSAGE_WITH ("5");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("8");
  EXPECT_SEND_MESSAGE_WITH ("\"18446744073709551615\"");
}

TESTCASE (uint64_can_be_constructed_from_a_large_number)
{
  COMPILE_AND_LOAD_SCRIPT ("send(uint64(Math.pow(2, 63)).toString(16));");
  EXPECT_SEND_MESSAGE_WITH ("\"8000000000000000\"");
}

TESTCASE (uint64_can_be_converted_to_a_large_number)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const a = Math.pow(2, 63);"
      "const b = uint64(a).toNumber();"
      "send(b === a);");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (int64_provides_arithmetic_operations)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(int64(3).add(4).toNumber());"
      "send(int64(7).sub(4).toNumber());"
      "send(int64(6).and(3).toNumber());"
      "send(int64(6).or(3).toNumber());"
      "send(int64(6).xor(3).toNumber());"
      "send(int64(63).shr(4).toNumber());"
      "send(int64(1).shl(3).toNumber());"
      "send(int64(0).not().toNumber());");
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_SEND_MESSAGE_WITH ("5");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("8");
  EXPECT_SEND_MESSAGE_WITH ("-1");
}

static gint
gum_get_answer_to_life_universe_and_everything (void)
{
  return 42;
}

static gint
gum_toupper (gchar * str,
             gint limit)
{
  gint count = 0;
  gchar * c;

  for (c = str; *c != '\0' && (count < limit || limit == -1); c++, count++)
  {
    *c = g_ascii_toupper (*c);
  }

  return (limit == -1) ? -count : count;
}

static gint64
gum_classify_timestamp (gint64 timestamp)
{
  if (timestamp < 0)
    return -1;
  else if (timestamp > 0)
    return 1;
  else
    return 0;
}

static guint64
gum_square (guint64 value)
{
  return value * value;
}

static gint
gum_sum (gint count,
         ...)
{
  gint total = 0;
  va_list args;
  gint i;

  va_start (args, count);
  for (i = 0; i != count; i++)
    total += va_arg (args, gint);
  va_end (args);

  return total;
}

static gint
gum_add_pointers_and_float_fixed (gpointer a,
                                  gpointer b,
                                  float c)
{
  return GPOINTER_TO_SIZE (a) + GPOINTER_TO_SIZE (b) + (int) c;
}

static gint
gum_add_pointers_and_float_variadic (gpointer a,
                                     ...)
{
  gint total = GPOINTER_TO_SIZE (a);
  va_list args;
  gpointer p;

  va_start (args, a);
  while ((p = va_arg (args, gpointer)) != NULL)
  {
    total += GPOINTER_TO_SIZE (p);
    total += (int) va_arg (args, double); /* float is promoted to double */
  }
  va_end (args);

  return total;
}

TESTCASE (whole_file_can_be_read_as_bytes)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "send(Array.from(new Uint8Array(File.readAllBytes('%s'))));",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("[97,98,99]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (whole_file_can_be_read_as_text)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT ("send(File.readAllText('%s'));", ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("\"abc\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (whole_file_can_be_read_as_text_with_validation)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("ab\xc3\x28" "c");
  COMPILE_AND_LOAD_SCRIPT ("send(File.readAllText('%s'));", ESCAPE_PATH (path));
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: can't decode byte 0xc3 in position 2");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (whole_file_can_be_written_from_bytes)
{
  const gchar * path;
  gchar * contents;

  path = MAKE_TEMPFILE_CONTAINING ("abc");

  COMPILE_AND_LOAD_SCRIPT (
      "File.writeAllBytes('%s', new Uint8Array([100,101,102]));",
      ESCAPE_PATH (path));
  EXPECT_NO_MESSAGES ();

  g_file_get_contents (path, &contents, NULL, NULL);
  g_assert_cmpstr (contents, ==, "def");
  g_free (contents);
}

TESTCASE (whole_file_can_be_written_from_text)
{
  const gchar * path;
  gchar * contents;

  path = MAKE_TEMPFILE_CONTAINING ("abc");

  COMPILE_AND_LOAD_SCRIPT ("File.writeAllText('%s', 'def');",
      ESCAPE_PATH (path));
  EXPECT_NO_MESSAGES ();

  g_file_get_contents (path, &contents, NULL, NULL);
  g_assert_cmpstr (contents, ==, "def");
  g_free (contents);
}

TESTCASE (file_can_be_read_as_bytes_in_one_go)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "const buf = file.readBytes();"
      "send(buf instanceof ArrayBuffer);"
      "send(Array.from(new Uint8Array(buf)));",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("[97,98,99]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_read_as_bytes_in_chunks)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "send(Array.from(new Uint8Array(file.readBytes(2))));"
      "send(Array.from(new Uint8Array(file.readBytes())));"
      "send(Array.from(new Uint8Array(file.readBytes(1))));",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("[97,98]");
  EXPECT_SEND_MESSAGE_WITH ("[99]");
  EXPECT_SEND_MESSAGE_WITH ("[]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_read_as_text_in_one_go)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "send(file.readText());",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("\"abc\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_read_as_text_in_chunks)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "send(file.readText(2));"
      "send(file.readText());"
      "send(file.readText(1));",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("\"ab\"");
  EXPECT_SEND_MESSAGE_WITH ("\"c\"");
  EXPECT_SEND_MESSAGE_WITH ("\"\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_read_as_text_with_validation)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("\xc3\x28yay");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "try {"
      "  send(file.readText(2));"
      "} catch (e) {"
      "  send(e.message);"
      "}"
      "send(file.tell());"
      "file.seek(2, File.SEEK_CUR);"
      "send(file.readText());",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("\"can't decode byte 0xc3 in position 0\"");
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("\"yay\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_read_line_by_line)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("first\nsecond");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');\n"
      "send(file.readLine());\n"
      "send(file.readLine());\n"
      "send(file.readLine());\n",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("\"first\\n\"");
  EXPECT_SEND_MESSAGE_WITH ("\"second\"");
  EXPECT_SEND_MESSAGE_WITH ("\"\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_read_line_by_line_with_validation)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("first\noops\xc3\x28\nlast");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');\n"
      "send(file.readLine());\n"
      "try {"
      "  send(file.readLine());"
      "} catch (e) {"
      "  send(e.message);"
      "}"
      "file.seek(7, File.SEEK_CUR);"
      "send(file.readLine());\n",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("\"first\\n\"");
  EXPECT_SEND_MESSAGE_WITH ("\"can't decode byte 0xc3 in position 4\"");
  EXPECT_SEND_MESSAGE_WITH ("\"last\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_position_can_be_queried)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "send(file.tell());"
      "file.readBytes(2);"
      "send(file.tell());",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("0");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_position_can_be_updated_to_absolute_position_implicitly)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "file.seek(2);"
      "send(file.tell());"
      "send(Array.from(new Uint8Array(file.readBytes())));"
      "send(file.tell());",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("[99]");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_position_can_be_updated_to_absolute_position_explicitly)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "file.seek(2, File.SEEK_SET);"
      "send(file.tell());"
      "send(Array.from(new Uint8Array(file.readBytes())));"
      "send(file.tell());",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("[99]");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_position_can_be_updated_to_relative_position_from_current)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "send(Array.from(new Uint8Array(file.readBytes(2))));"
      "file.seek(-1, File.SEEK_CUR);"
      "send(Array.from(new Uint8Array(file.readBytes())));",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("[97,98]");
  EXPECT_SEND_MESSAGE_WITH ("[98,99]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_position_can_be_updated_to_relative_position_from_end)
{
  const gchar * path = MAKE_TEMPFILE_CONTAINING ("abc");
  COMPILE_AND_LOAD_SCRIPT (
      "const file = new File('%s', 'rb');"
      "file.seek(-2, File.SEEK_END);"
      "send(Array.from(new Uint8Array(file.readBytes())));",
      ESCAPE_PATH (path));
  EXPECT_SEND_MESSAGE_WITH ("[98,99]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (file_can_be_written_to)
{
  const gchar * path;
  const gchar d00d[4] = { 0x64, 0x30, 0x30, 0x64 };
  gchar * contents;

  path = MAKE_TEMPFILE_CONTAINING ("abc");

  COMPILE_AND_LOAD_SCRIPT (
      "const log = new File('%s', 'wb');"
      "log.write(\"Hello \");"
      "log.write(" GUM_PTR_CONST ".readByteArray(4));"
      "log.write(\"!\\n\");"
      "log.close();",
      ESCAPE_PATH (path), d00d);
  EXPECT_NO_MESSAGES ();

  g_file_get_contents (path, &contents, NULL, NULL);
  g_assert_cmpstr (contents, ==, "Hello d00d!\n");
  g_free (contents);
}

#ifndef HAVE_QNX

TESTCASE (file_apis_can_not_trigger_interceptor)
{
  const gchar * path;
  GThread * worker_thread;
  GumInvokeTargetContext ctx;

  path = MAKE_TEMPFILE_CONTAINING ("abc");

  COMPILE_AND_LOAD_SCRIPT (
      "const referencePath = '%s';"
      "setTimeout(() => {"
      "  Interceptor.attach(Module.getGlobalExportByName('fopen'), {"
      "    onEnter(args) {"
      "      const path = args[0].readUtf8String();"
      "      if (path === referencePath) {"
      "        send('intercepted');"
      "      }"
      "    }"
      "  });"
      "  Interceptor.replace(" GUM_PTR_CONST ", new NativeCallback((arg) => {"
      "    const log = new File(referencePath, 'wb');"
      "    log.write('Hello!\\n');"
      "    log.close();"
      "    send('File written');"
      "    return arg;"
      "  }, 'int', ['int']));"
      "  send('Test scheduled');"
      "}, 0);",
      ESCAPE_PATH (path),
      target_function_int);

  EXPECT_SEND_MESSAGE_WITH ("\"Test scheduled\"");
  ctx.script = fixture->script;
  ctx.repeat_duration = 0;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  while (ctx.started == 0)
    g_usleep (G_USEC_PER_SEC / 200);

  g_usleep (G_USEC_PER_SEC / 25);
  EXPECT_SEND_MESSAGE_WITH ("\"File written\"");
  g_thread_join (worker_thread);
  g_assert_cmpint (ctx.finished, ==, 1);
  EXPECT_NO_MESSAGES ();
}

#endif

TESTCASE (md5_can_be_computed_for_stream)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const checksum = new Checksum('md5');"
      "checksum.update('ab').update('c');"

      "send(checksum.getString());"

      "const view = new DataView(checksum.getDigest());"
      "send(["
      "  view.getUint32(0).toString(16),"
      "  view.getUint32(4).toString(16),"
      "  view.getUint32(8).toString(16),"
      "  view.getUint32(12).toString(16)"
      "]);"

      "checksum.update('d');");

  EXPECT_SEND_MESSAGE_WITH ("\"900150983cd24fb0d6963f7d28e17f72\"");
  EXPECT_SEND_MESSAGE_WITH ("[\"90015098\",\"3cd24fb0\",\"d6963f7d\","
      "\"28e17f72\"]");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: checksum is closed");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (md5_can_be_computed_for_string)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Checksum.compute('md5', 'abc'));");
  EXPECT_SEND_MESSAGE_WITH ("\"900150983cd24fb0d6963f7d28e17f72\"");
}

TESTCASE (md5_can_be_computed_for_bytes)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const data = new Uint8Array([ 1, 2, 3 ]);"
      "send(Checksum.compute('md5', data.buffer));");
  EXPECT_SEND_MESSAGE_WITH ("\"5289df737df57326fcdd22597afb1fac\"");
}

TESTCASE (sha1_can_be_computed_for_string)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Checksum.compute('sha1', 'abc'));");
  EXPECT_SEND_MESSAGE_WITH ("\"a9993e364706816aba3e25717850c26c9cd0d89d\"");
}

TESTCASE (sha256_can_be_computed_for_string)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Checksum.compute('sha256', 'abc'));");
  EXPECT_SEND_MESSAGE_WITH ("\""
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
      "\"");
}

TESTCASE (sha384_can_be_computed_for_string)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Checksum.compute('sha384', 'abc'));");
  EXPECT_SEND_MESSAGE_WITH ("\""
      "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
      "8086072ba1e7cc2358baeca134c825a7"
      "\"");
}

TESTCASE (sha512_can_be_computed_for_string)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Checksum.compute('sha512', 'abc'));");
  EXPECT_SEND_MESSAGE_WITH ("\""
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
      "\"");
}

TESTCASE (requesting_unknown_checksum_for_string_should_throw)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Checksum.compute('bogus', 'abc'));");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: unsupported checksum type");
}

TESTCASE (checksum_can_be_copied)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const checksum = new Checksum('md5');"
      "checksum.update('ab');"
      "const copied = checksum.copy();"

      "send(checksum.update('c').getString());"
      "send(copied.update('c').getString());"

      "const closed = checksum.copy();"
      "closed.update('d');"
      );

  EXPECT_SEND_MESSAGE_WITH ("\"900150983cd24fb0d6963f7d28e17f72\"");
  EXPECT_SEND_MESSAGE_WITH ("\"900150983cd24fb0d6963f7d28e17f72\"");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: checksum is closed");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (checksum_can_be_peeked)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const checksum = new Checksum('md5');"
      "checksum.update('ab');"

      "const view = new DataView(checksum.peekDigest());"
      "send(["
      "  view.getUint32(0).toString(16),"
      "  view.getUint32(4).toString(16),"
      "  view.getUint32(8).toString(16),"
      "  view.getUint32(12).toString(16)"
      "].join(''));"

      "send(checksum.peekString());"
      "send(checksum.update('c').getString());"

      "checksum.update('d');"
      );

  EXPECT_SEND_MESSAGE_WITH ("\"187ef4436122d1cc2f40dc2b92f0eba0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"187ef4436122d1cc2f40dc2b92f0eba0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"900150983cd24fb0d6963f7d28e17f72\"");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: checksum is closed");
  EXPECT_NO_MESSAGES ();
}

#ifdef HAVE_SQLITE

TESTCASE (inline_sqlite_database_can_be_queried)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const db = SqliteDatabase.openInline('"
          "H4sIAMMIT1kAA+3ZsU7DMBAG4HMC7VChROpQut0IqGJhYCWJDAq4LbhGoqNRDYqgpIo"
          "CO8y8JM/AC+CKFNhgLfo/+U7n0/kBTp5cqKJ2fFNWc1vzAcUkBB0xE1HYxIrwsdHUYX"
          "P/TUj7m+nWcjhy5A8AAAAAAADA//W8Ldq9fl+8dGp7fe8WrlyscphpmRjJJkmV5M8e7"
          "xQzzkdGnkjN5zofJnrKZ3LKySQb8IOdOzbyyvBo7ONSqQHbW/f14Lt7Z/1S7+uh1Hn2"
          "c/rJ1rbiVI3T3b8s8QAAAAAAAACw3pZ/80H0RtG7TwAAAAAAAACwnuKgRT0RxMdVMbN"
          "teu0edkSLukLQaen2Hj8AoNOJGgAwAAA="
      "');\n"

      /* 1: bindInteger() */
      "let s = db.prepare('SELECT name, age FROM people WHERE age = ?');\n"
      "s.bindInteger(1, 42);\n"
      "send(s.step());\n"
      "send(s.step());\n"
      "s.reset();\n"
      "s.bindInteger(1, 7);\n"
      "send(s.step());\n"

      /* 2: bindFloat() */
      "s = db.prepare('SELECT name FROM people WHERE karma <= ?');\n"
      "s.bindFloat(1, 117.5);\n"
      "send(s.step());\n"
      "send(s.step());\n"

      /* 3: bindText() */
      "s = db.prepare('SELECT age FROM people WHERE name = ?');\n"
      "s.bindText(1, 'Joe');\n"
      "send(s.step());\n"

      /* 4: bindBlob() */
      "s = db.prepare('SELECT name FROM people WHERE avatar = ?');\n"
      "s.bindBlob(1, [0x13, 0x37]);\n"
      "send(s.step());\n"
      "send(s.step());\n"

      /* 5: bindNull() */
      "s = db.prepare('INSERT INTO people VALUES (?, ?, ?, ?, ?)');\n"
      "s.bindInteger(1, 3);\n"
      "s.bindText(2, 'Alice');\n"
      "s.bindInteger(3, 40);\n"
      "s.bindInteger(4, 150);\n"
      "s.bindNull(5);\n"
      "send(s.step());\n"
      "s = db.prepare('SELECT * FROM people WHERE name = \"Alice\"');\n"
      "send(s.step());\n"
      "send(s.step());\n"

      /* 6: blob column */
      "s = db.prepare('SELECT avatar FROM people WHERE name = ?');\n"
      "s.bindText(1, 'Frida');\n"
      "send('avatar', s.step()[0]);\n"
      "send(s.step());\n"
      "s.reset();\n"
      "s.bindText(1, 'Joe');\n"
      "send(s.step());\n"
      "send(s.step());\n");

  /* 1: bindInteger() */
  EXPECT_SEND_MESSAGE_WITH ("[\"Joe\",42]");
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("[\"Frida\",7]");

  /* 2: bindFloat() */
  EXPECT_SEND_MESSAGE_WITH ("[\"Joe\"]");
  EXPECT_SEND_MESSAGE_WITH ("null");

  /* 3: bindText() */
  EXPECT_SEND_MESSAGE_WITH ("[42]");

  /* 4: bindBlob() */
  EXPECT_SEND_MESSAGE_WITH ("[\"Frida\"]");
  EXPECT_SEND_MESSAGE_WITH ("null");

  /* 5: bindNull() */
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("[3,\"Alice\",40,150,null]");
  EXPECT_SEND_MESSAGE_WITH ("null");

  /* 6: blob column */
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"avatar\"", "13 37");
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("[null]");
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (sqlite_statement_has_column_and_params_metadata)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const db = SqliteDatabase.openInline('"
          "H4sIAMMIT1kAA+3ZsU7DMBAG4HMC7VChROpQut0IqGJhYCWJDAq4LbhGoqNRDYqgpIo"
          "CO8y8JM/AC+CKFNhgLfo/+U7n0/kBTp5cqKJ2fFNWc1vzAcUkBB0xE1HYxIrwsdHUYX"
          "P/TUj7m+nWcjhy5A8AAAAAAADA//W8Ldq9fl+8dGp7fe8WrlyscphpmRjJJkmV5M8e7"
          "xQzzkdGnkjN5zofJnrKZ3LKySQb8IOdOzbyyvBo7ONSqQHbW/f14Lt7Z/1S7+uh1Hn2"
          "c/rJ1rbiVI3T3b8s8QAAAAAAAACw3pZ/80H0RtG7TwAAAAAAAACwnuKgRT0RxMdVMbN"
          "teu0edkSLukLQaen2Hj8AoNOJGgAwAAA="
      "');\n"

      /* 1: columnNames and paramsCount */
      "let s = db.prepare('SELECT name, age FROM people WHERE age = ?');\n"
      "send(s.columnNames);\n"
      "send(s.paramsCount);\n"

      /* 2: declaredTypes */
      "s = db.prepare('SELECT name, age, karma, avatar FROM people');\n"
      "send(s.declaredTypes);\n"

      /* 3: columnTypes after stepping a row with known non-null values */
      "s = db.prepare("
          "'SELECT name, age, karma FROM people WHERE name = ?');\n"
      "s.bindText(1, 'Joe');\n"
      "s.step();\n"
      "send(s.columnTypes);\n");

  EXPECT_SEND_MESSAGE_WITH ("[\"name\",\"age\"]");
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("[\"TEXT\",\"INTEGER\",\"NUMERIC\",\"BLOB\"]");
  EXPECT_SEND_MESSAGE_WITH ("[\"text\",\"integer\",\"integer\"]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (external_sqlite_database_can_be_queried)
{
  TestScriptMessageItem * item;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "const db = SqliteDatabase.open('/tmp/gum-test.db');\n"
      "db.exec(\""
          "PRAGMA foreign_keys=OFF;"
          "BEGIN TRANSACTION;"
          "CREATE TABLE people ("
              "id INTEGER PRIMARY KEY ASC,"
              "name TEXT NOT NULL,"
              "age INTEGER NOT NULL,"
              "karma NUMERIC NOT NULL,"
              "avatar BLOB"
          ");"
          "INSERT INTO people VALUES (1, 'Joe', 42, 117, NULL);"
          "INSERT INTO people VALUES (2, 'Frida', 7, 140, X'1337');"
          "COMMIT;"
      "\");\n"
      "send(db.dump());\n"
      "db.close();\n");

  item = test_script_fixture_pop_message (fixture);
  g_print ("%s\n", item->message);
  test_script_message_item_free (item);
}

TESTCASE (external_sqlite_database_can_be_opened_with_flags)
{
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "let db = null;\n"

      "try {\n"
          "db = SqliteDatabase.open('/tmp/gum-test-dont-create.db',"
            "{ flags: ['readwrite'] });\n"
          "send('fail');\n"
          "db.close();\n"
      "} catch (e) {\n"
          "send('not exists');\n"
      "}\n"

      "try {\n"
          "db = SqliteDatabase.open('/tmp/gum-test-dont-create2.db',"
            "{ flags: ['readonly'] });\n"
          "send('fail');\n"
          "db.close();\n"
      "} catch (e) {\n"
          "send('not exists again');\n"
      "}\n"

      "try {\n"
          "db = SqliteDatabase.open('/tmp/gum-test-dont-write.db',"
            "{ flags: ['readonly', 'create'] });\n"
          "send('fail');\n"
          "db.close();\n"
      "} catch (e) {\n"
          "send('invalid flags');\n"
      "}\n"

      "db = SqliteDatabase.open('/tmp/gum-test-can-write.db',"
        "{ flags: ['readwrite', 'create'] });\n"
      "try {\n"
          "db.exec(\""
              "PRAGMA foreign_keys=OFF;"
              "BEGIN TRANSACTION;"
              "CREATE TABLE people ("
                  "id INTEGER PRIMARY KEY ASC,"
                  "name TEXT NOT NULL,"
                  "age INTEGER NOT NULL,"
                  "karma NUMERIC NOT NULL,"
                  "avatar BLOB"
              ");"
              "INSERT INTO people VALUES (1, 'Joe', 42, 117, NULL);"
              "INSERT INTO people VALUES (2, 'Frida', 7, 140, X'1337');"
              "COMMIT;"
          "\");\n"
          "send('can write');\n"
      "} catch (e) {\n"
          "send('fail');\n"
      "}\n"
      "db.close();\n");

  EXPECT_SEND_MESSAGE_WITH ("\"not exists\"");
  EXPECT_SEND_MESSAGE_WITH ("\"not exists again\"");
  EXPECT_SEND_MESSAGE_WITH ("\"invalid flags\"");
  EXPECT_SEND_MESSAGE_WITH ("\"can write\"");
  EXPECT_NO_MESSAGES ();
}

# if !defined (HAVE_WINDOWS) && !defined (HAVE_QNX)

TESTCASE (sqlite_apis_can_not_trigger_interceptor)
{
  gchar * path;
  gint fd;
  GThread * worker_thread;
  GumInvokeTargetContext ctx;

  fd = g_file_open_tmp ("gum-tests.XXXXXX", &path, NULL);
  g_assert_cmpint (fd, !=, -1);
  close (fd);
  g_queue_push_tail (&fixture->tempfiles, path);

  COMPILE_AND_LOAD_SCRIPT (
      "const referencePath = '%s';"
      "let stat = Module.findGlobalExportByName('stat');"
      "if (stat === null) {"
      "  stat = Module.findGlobalExportByName('__xstat64');"
      "}"
      "if (stat === null) {"
      "  stat = Module.findGlobalExportByName('__xstat');"
      "}"
      "Interceptor.attach(stat, {"
      "  onEnter(args) {"
      "    const path = args[0].readUtf8String();"
      "    if (path.includes(referencePath)) {"
      "      send('intercepted stat');"
      "    }"
      "  }"
      "});"
      "Interceptor.attach(Module.getGlobalExportByName('open'), {"
      "  onEnter(args) {"
      "    const path = args[0].readUtf8String();"
      "    if (path.includes(referencePath)) {"
      "      send('intercepted open');"
      "    }"
      "  }"
      "});"
      "Interceptor.replace(" GUM_PTR_CONST ", new NativeCallback((arg) => {"
      "  const db = SqliteDatabase.open(referencePath);"
      "  send('Database created');"
      "  db.exec(`"
      "  CREATE TABLE IF NOT EXISTS test ("
      "     id TEXT PRIMARY KEY,"
      "     stuff TEXT"
      "  );`);"
      "  send('Table created');"
      "  const statement = db"
      "    .prepare('INSERT INTO test (id, stuff) VALUES (?, ?);');"
      "  send('Statement prepared');"
      "  statement.bindText(1, 'i am primary ' + Date.now());"
      "  statement.bindText(2, 'i am stuff');"
      "  statement.step();"
      "  statement.reset();"
      "  send('Query done');"
      "  db.close();"
      "  send('Database closed');"
      "  return arg;"
      "}, 'int', ['int']));",
      ESCAPE_PATH (path),
      target_function_int);

  ctx.script = fixture->script;
  ctx.repeat_duration = 0;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  while (ctx.started == 0)
    g_usleep (G_USEC_PER_SEC / 200);

  g_usleep (G_USEC_PER_SEC / 25);
  EXPECT_SEND_MESSAGE_WITH ("\"Database created\"");
  EXPECT_SEND_MESSAGE_WITH ("\"Table created\"");
  EXPECT_SEND_MESSAGE_WITH ("\"Statement prepared\"");
  EXPECT_SEND_MESSAGE_WITH ("\"Query done\"");
  EXPECT_SEND_MESSAGE_WITH ("\"Database closed\"");
  g_thread_join (worker_thread);
  g_assert_cmpint (ctx.finished, ==, 1);
  EXPECT_NO_MESSAGES ();
}

# endif

#endif

TESTCASE (match_pattern_can_be_constructed_from_string)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const p = new MatchPattern('13 37 ?? ff');"
      "send(JSON.stringify(p));"
  );
  EXPECT_SEND_MESSAGE_WITH ("\"{}\"");

  COMPILE_AND_LOAD_SCRIPT ("new MatchPattern('Some bad pattern');");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid match pattern");
}

TESTCASE (socket_connection_can_be_established)
{
#ifdef HAVE_ANDROID
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  PUSH_TIMEOUT (10000);

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const listener = await Socket.listen({ backlog: 1 });"
      "    launchClient({"
      "      family: 'ipv4',"
      "      host: 'localhost',"
      "      port: listener.port,"
      "    });"
      "    const client = await listener.accept();"
      "    const data = await client.input.readAll(5);"
      "    send('server read', data);"
      "    await client.close();"
      "    await listener.close();"
      "  } catch (e) {"
      "    send(`[server] ${e.stack}`);"
      "  }"
      "}"
      "async function launchClient(options) {"
      "  try {"
      "    const connection = await Socket.connect(options);"
      "    await connection.setNoDelay(true);"
      "    await connection.output.writeAll([0x31, 0x33, 0x33, 0x37, 0x0a]);"
      "    await connection.close();"
      "  } catch (e) {"
      "    send(`[client] ${e.stack}`);"
      "  }"
      "}"
      "run();");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"server read\"",
      "31 33 33 37 0a");

#ifdef G_OS_UNIX
  {
    const gchar * tmp_dir;

#ifdef HAVE_ANDROID
    tmp_dir = "/data/local/tmp";
#else
    tmp_dir = g_get_tmp_dir ();
#endif

    COMPILE_AND_LOAD_SCRIPT (
        "const unlink = new NativeFunction("
        "    Module.getGlobalExportByName('unlink'), 'int', ['pointer']);"
        "async function run() {"
        "  try {"
        "    const listener = await Socket.listen({"
        "      type: 'path',"
        "      path: '%s/frida-gum-test-listener-' + Process.id,"
        "      backlog: 1,"
        "    });"
        "    launchClient({"
        "      type: 'path',"
        "      path: listener.path,"
        "    });"
        "    const client = await listener.accept();"
        "    const data = await client.input.readAll(5);"
        "    send('server read', data);"
        "    await client.close();"
        "    await listener.close();"
        "  } catch (e) {"
        "    send(`[server] ${e.stack}`);"
        "  }"
        "}"
        "async function launchClient(options) {"
        "  try {"
        "    const connection = await Socket.connect(options);"
        "    unlink(Memory.allocUtf8String(options.path));"
        "    await connection.output.writeAll([0x31, 0x33, 0x33, 0x37, 0x0a]);"
        "    await connection.close();"
        "  } catch (e) {"
        "    send(`[client] ${e.stack}`);"
        "  }"
        "}"
        "run();",
      tmp_dir);
    EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"server read\"",
        "31 33 33 37 0a");
  }
#endif
}

TESTCASE (socket_connection_can_be_established_with_tls)
{
  gboolean done;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  PUSH_TIMEOUT (10000);

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    const connection = await Socket.connect({"
      "      family: 'ipv4',"
      "      host: 'www.google.com',"
      "      port: 443,"
      "      tls: true,"
      "    });"
      ""
      "    await connection.setNoDelay(true);"
      ""
      "    const request = ["
      "      'GET / HTTP/1.1',"
      "      'Connection: close',"
      "      'Host: www.google.com',"
      "      'Accept: text/html',"
      "      'User-Agent: Frida/" FRIDA_VERSION "',"
      "      '',"
      "      '',"
      "    ].join('\\r\\n');"
      "    const rawRequest = [];"
      "    for (let i = 0; i !== request.length; i++)"
      "      rawRequest.push(request.charCodeAt(i));"
      "    send('request', rawRequest);"
      "    await connection.output.writeAll(rawRequest);"
      ""
      "    const response = await connection.input.read(128 * 1024);"
      "    send('response', response);"
      "  } catch (e) {"
      "    send(`oops: ${e.stack}`);"
      "  }"
      "}"
      "run();");

  g_printerr ("\n\n");

  done = FALSE;
  while (!done)
  {
    TestScriptMessageItem * item;

    item = test_script_fixture_pop_message (fixture);

    if (item->raw_data != NULL)
    {
      gboolean is_request;
      const guint8 * raw_chunk;
      gsize size;
      gchar * chunk;

      is_request = strstr (item->message, "\"request\"") != NULL;

      raw_chunk = g_bytes_get_data (item->raw_data, &size);
      chunk = g_strndup ((const gchar *) raw_chunk, size);

      g_printerr ("*** %s %" G_GSIZE_MODIFIER "u bytes\n%s",
          is_request ? "Sent" : "Received",
          size,
          chunk);

      g_free (chunk);

      done = !is_request;
    }
    else
    {
      g_printerr ("Got: %s\n", item->message);
    }

    test_script_message_item_free (item);
  }
}

TESTCASE (socket_connection_should_not_leak_on_error)
{
  if (!g_test_slow ())
  {
    g_print("<skipping, run in slow mode> ");
    return;
  }

  PUSH_TIMEOUT (5000);
  COMPILE_AND_LOAD_SCRIPT (
      "let tries = 0;"
      "let port = 28300;"
      "let firstErrorMessage = null;"
      ""
      "tryNext();"
      ""
      "function tryNext() {"
      "  tries++;"
      "  if (tries === 200) {"
      "    send('done');"
      "    return;"
      "  }"
      ""
      "  Socket.connect({"
      "    family: 'ipv4',"
      "    host: 'localhost',"
      "    port: port,"
      "  })"
      "  .then(connection => {"
      "    console.log('success');"
      "    tries--;"
      "    port++;"
      "    tryNext();"
      "  })"
      "  .catch(error => {"
      "    if (firstErrorMessage === null) {"
      "      firstErrorMessage = error.message;"
      "    } else if (error.message !== firstErrorMessage) {"
      "      send('Expected \"' + firstErrorMessage + '\" but got \"' +"
      "          error.message + '\"');"
      "      return;"
      "    }"
      "    console.log('tries=' + tries + ' error=\"' + error.message + '\"');"
      "    tryNext();"
      "  });"
      "}");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
}

TESTCASE (socket_type_can_be_inspected)
{
  int fd;
  struct sockaddr_in addr = { 0, };
  const guint port = 39876;

  fd = socket (AF_INET, SOCK_STREAM, 0);
  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
  EXPECT_SEND_MESSAGE_WITH ("\"tcp\"");
  addr.sin_family = AF_INET;
  addr.sin_port = GUINT16_TO_BE (port);
  addr.sin_addr.s_addr = INADDR_ANY;
  bind (fd, (struct sockaddr *) &addr, sizeof (addr));
  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
  EXPECT_SEND_MESSAGE_WITH ("\"tcp\"");
  GUM_CLOSE_SOCKET (fd);

  fd = socket (AF_INET, SOCK_DGRAM, 0);
  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
  EXPECT_SEND_MESSAGE_WITH ("\"udp\"");
  GUM_CLOSE_SOCKET (fd);

  fd = socket (AF_INET6, SOCK_STREAM, 0);
  if (fd != -1)
  {
    COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
    EXPECT_SEND_MESSAGE_WITH ("\"tcp6\"");
    GUM_CLOSE_SOCKET (fd);
  }

  fd = socket (AF_INET6, SOCK_DGRAM, 0);
  if (fd != -1)
  {
    COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
    EXPECT_SEND_MESSAGE_WITH ("\"udp6\"");
    GUM_CLOSE_SOCKET (fd);
  }

  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(-1));");
  EXPECT_SEND_MESSAGE_WITH ("null");

#ifndef HAVE_WINDOWS
  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
  EXPECT_SEND_MESSAGE_WITH ("\"unix:stream\"");
  close (fd);

  fd = socket (AF_UNIX, SOCK_DGRAM, 0);
  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
  EXPECT_SEND_MESSAGE_WITH ("\"unix:dgram\"");
  close (fd);

  fd = open (
# ifdef HAVE_QNX
      "/usr/lib/ldqnx.so.2",
# else
      "/etc/passwd",
# endif
      O_RDONLY);
  g_assert_cmpint (fd, >=, 0);
  COMPILE_AND_LOAD_SCRIPT ("send(Socket.type(%d));", fd);
  EXPECT_SEND_MESSAGE_WITH ("null");
  close (fd);
#endif
}

TESTCASE (socket_endpoints_can_be_inspected)
{
  GSocketFamily family[] = { G_SOCKET_FAMILY_IPV4, G_SOCKET_FAMILY_IPV6 };
  guint i;
  GMainContext * context;
  int fd;

  context = g_main_context_get_thread_default ();

  for (i = 0; i != G_N_ELEMENTS (family); i++)
  {
    GSocket * sock;
    GSocketService * service;
    GInetAddress * loopback;
    GSocketAddress * listen_address, * server_address, * client_address;
    guint16 server_port, client_port;

    sock = g_socket_new (family[i], G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP,
        NULL);
    if (sock == NULL)
      continue;
    fd = g_socket_get_fd (sock);

    service = g_socket_service_new ();
    g_signal_connect (service, "incoming", G_CALLBACK (on_incoming_connection),
        NULL);
    loopback = g_inet_address_new_loopback (family[i]);
    listen_address = g_inet_socket_address_new (loopback, 0);
    if (!g_socket_listener_add_address (G_SOCKET_LISTENER (service),
        listen_address, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL,
        &server_address, NULL))
      goto skip_unsupported_family;
    server_port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (
        server_address));
    g_socket_service_start (service);

    COMPILE_AND_LOAD_SCRIPT ("send(Socket.peerAddress(%d));", fd);
    EXPECT_SEND_MESSAGE_WITH ("null");

    while (g_main_context_pending (context))
      g_main_context_iteration (context, FALSE);

    g_assert_true (g_socket_connect (sock, server_address, NULL, NULL));

    g_object_get (sock, "local-address", &client_address, NULL);
    client_port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (
        client_address));

    while (g_main_context_pending (context))
      g_main_context_iteration (context, FALSE);

    COMPILE_AND_LOAD_SCRIPT (
        "const addr = Socket.localAddress(%d);"
        "send([typeof addr.ip, addr.port]);", fd);
    EXPECT_SEND_MESSAGE_WITH ("[\"string\",%u]", client_port);

    COMPILE_AND_LOAD_SCRIPT (
        "const addr = Socket.peerAddress(%d);"
        "send([typeof addr.ip, addr.port]);", fd);
    EXPECT_SEND_MESSAGE_WITH ("[\"string\",%u]", server_port);

    g_socket_close (sock, NULL);
    g_socket_service_stop (service);
    while (g_main_context_pending (context))
      g_main_context_iteration (context, FALSE);

    g_object_unref (client_address);
    g_object_unref (server_address);

skip_unsupported_family:
    g_object_unref (listen_address);
    g_object_unref (loopback);
    g_object_unref (service);

    g_object_unref (sock);
  }

#ifdef HAVE_DARWIN
  {
    struct sockaddr_un address;
    socklen_t len;

    fd = socket (AF_UNIX, SOCK_STREAM, 0);

    address.sun_family = AF_UNIX;
    strcpy (address.sun_path, "/tmp/gum-script-test");
    unlink (address.sun_path);
    address.sun_len = sizeof (address) - sizeof (address.sun_path) +
        strlen (address.sun_path) + 1;
    len = address.sun_len;
    bind (fd, (struct sockaddr *) &address, len);

    COMPILE_AND_LOAD_SCRIPT ("send(Socket.localAddress(%d));", fd);
    EXPECT_SEND_MESSAGE_WITH ("{\"path\":\"\"}");
    close (fd);

    unlink (address.sun_path);
  }
#endif
}

static gboolean
on_incoming_connection (GSocketService * service,
                        GSocketConnection * connection,
                        GObject * source_object,
                        gpointer user_data)
{
  GInputStream * input;
  void * buf;

  input = g_io_stream_get_input_stream (G_IO_STREAM (connection));
  buf = g_malloc (1);
  g_input_stream_read_async (input, buf, 1, G_PRIORITY_DEFAULT, NULL,
      on_read_ready, g_object_ref (connection));

  return TRUE;
}

static void
on_read_ready (GObject * source_object,
               GAsyncResult * res,
               gpointer user_data)
{
  GSocketConnection * connection = user_data;

  GError * error = NULL;
  g_input_stream_read_finish (G_INPUT_STREAM (source_object), res, &error);
  g_clear_error (&error);

  g_io_stream_close_async (G_IO_STREAM (connection), G_PRIORITY_LOW, NULL,
      NULL, NULL);
  g_object_unref (connection);
}

#if defined (HAVE_I386) || defined (HAVE_ARM) || defined (HAVE_ARM64)

#include "stalkerdummychannel.h"

TESTCASE (execution_can_be_traced)
{
  GumThreadId test_thread_id;

#ifdef __ARM_PCS_VFP
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "Stalker.queueDrainInterval = 0;"
      "const testsRange = Process.getModuleByName('%s');"
      "Stalker.exclude(testsRange);"

      "Stalker.follow(%" G_GSIZE_FORMAT ", {"
      "  events: {"
      "    call: true,"
      "    ret: false,"
      "    exec: false"
      "  },"
      "  onReceive(events) {"
      "    send('onReceive: ' + (events.byteLength > 0));"
      "  },"
      "  onCallSummary(summary) {"
      "    send('onCallSummary: ' + (Object.keys(summary).length > 0));"
      "  }"
      "});"

      "recv('stop', message => {"
      "  Stalker.unfollow(%" G_GSIZE_FORMAT ");"
      "  Stalker.flush();"
      "});",

      GUM_TESTS_MODULE_NAME,
      test_thread_id,
      test_thread_id);
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"onCallSummary: true\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onReceive: true\"");
}

TESTCASE (execution_can_be_traced_with_custom_transformer)
{
  GumThreadId test_thread_id;

#if defined (HAVE_QNX) || defined (__ARM_PCS_VFP)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "const testsRange = Process.getModuleByName('%s');"
      "Stalker.exclude(testsRange);"

      "let instructionsSeen = 0;"
      "Stalker.follow(%" G_GSIZE_FORMAT ", {"
      "  transform(iterator) {"
      "    let instruction;"

      "    while ((instruction = iterator.next()) !== null) {"
      "      if (instructionsSeen === 0) {"
      "        send(typeof iterator.memoryAccess);"
      "        iterator.putCallout(onBeforeFirstInstruction);"
      "      }"

      "      iterator.keep();"

      "      instructionsSeen++;"
      "    }"
      "  }"
      "});"

      "function onBeforeFirstInstruction (context) {"
      "  console.log(JSON.stringify(context, null, 2));"
      "}"

      "recv('stop', message => {"
      "  Stalker.unfollow(%" G_GSIZE_FORMAT ");"
      "  send(instructionsSeen > 0);"
      "});",

      GUM_TESTS_MODULE_NAME,
      test_thread_id,
      test_thread_id);
  g_usleep (1);
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (execution_can_be_traced_with_faulty_transformer)
{
  GumThreadId test_thread_id;

#ifdef HAVE_QNX
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "const testsRange = Process.getModuleByName('%s');"
      "Stalker.exclude(testsRange);"

      "Stalker.follow(%" G_GSIZE_FORMAT ", {"
      "  transform(iterator) {"
      "    throw new Error('oh no I am buggy');"
      "  }"
      "});",

      GUM_TESTS_MODULE_NAME,
      test_thread_id);
  g_usleep (1);
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: oh no I am buggy");
  EXPECT_NO_MESSAGES ();

  g_assert (
      !gum_stalker_is_following_me (gum_script_get_stalker (fixture->script)));
}

TESTCASE (execution_can_be_traced_during_immediate_native_function_call)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Stalker.queueDrainInterval = 0;"
      "const testsRange = Process.getModuleByName('%s');"
      "Stalker.exclude(testsRange);"

      "const a = new NativeFunction(" GUM_PTR_CONST ", 'int', ['int'], "
          "{ traps: 'all', exceptions: 'propagate' });"

      "Stalker.follow({"
      "  events: {"
      "    call: true,"
      "  },"
      "  onCallSummary(summary) {"
      "    const key = a.strip().toString();"
      "    send(key in summary);"
      "    send(summary[key]);"
      "  }"
      "});"

      "a(42);"
      "a(42);"

      "Stalker.unfollow();"
      "Stalker.flush();",

      GUM_TESTS_MODULE_NAME,
      target_function_nested_a);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (execution_can_be_traced_during_scheduled_native_function_call)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Stalker.queueDrainInterval = 0;"
      "const testsRange = Process.getModuleByName('%s');"
      "Stalker.exclude(testsRange);"

      "const a = new NativeFunction(" GUM_PTR_CONST ", 'int', ['int'], "
          "{ traps: 'all' });"

      "Stalker.follow({"
      "  events: {"
      "    call: true,"
      "  },"
      "  onCallSummary(summary) {"
      "    const key = a.strip().toString();"
      "    send(key in summary);"
      "    send(summary[key]);"
      "  }"
      "});"

      "setImmediate(() => {"
        "a(42);"
        "a(42);"

        "Stalker.unfollow();"
        "Stalker.flush();"
      "});",

      GUM_TESTS_MODULE_NAME,
      target_function_nested_a);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (execution_can_be_traced_after_native_function_call_from_hook)
{
  StalkerDummyChannel channel;
  GThread * thread;
  GumThreadId thread_id;

#ifdef __ARM_PCS_VFP
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  sdc_init (&channel);

  thread = g_thread_new ("stalker-test-target",
      run_stalked_through_hooked_function, &channel);
  thread_id = sdc_await_thread_id (&channel);

  COMPILE_AND_LOAD_SCRIPT (
      "Stalker.queueDrainInterval = 0;"
      "const testsRange = Process.getModuleByName('%s');"
      "Stalker.exclude(testsRange);"

      "const targetThreadId = %" G_GSIZE_FORMAT ";"
      "const targetFuncInt = " GUM_PTR_CONST ";"
      "const targetFuncNestedA = new NativeFunction(" GUM_PTR_CONST ", 'int', "
          "['int'], { traps: 'all' });"

      "Interceptor.attach(targetFuncInt, () => {"
      "  targetFuncNestedA(1337);"
      "});"

      "Stalker.follow(targetThreadId, {"
      "  events: {"
      "    call: true,"
      "  },"
      "  onCallSummary(summary) {"
      "    const key = targetFuncNestedA.strip().toString();"
      "    send(key in summary);"
      "    send(summary[key]);"
      "  }"
      "});"

      "recv('stop', message => {"
      "  Stalker.unfollow(targetThreadId);"
      "  Stalker.flush();"
      "});"

      "send('ready');",

      GUM_TESTS_MODULE_NAME,
      thread_id,
      target_function_int,
      target_function_nested_a);
  EXPECT_SEND_MESSAGE_WITH ("\"ready\"");

  EXPECT_NO_MESSAGES ();
  sdc_put_follow_confirmation (&channel);

  sdc_await_run_confirmation (&channel);

  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_NO_MESSAGES ();

  sdc_put_finish_confirmation (&channel);

  g_thread_join (thread);

  sdc_finalize (&channel);
}

static gpointer
run_stalked_through_hooked_function (gpointer data)
{
  StalkerDummyChannel * channel = data;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  target_function_int (42);

  target_function_nested_a (1338);

  sdc_put_run_confirmation (channel);

  sdc_await_finish_confirmation (channel);

  return NULL;
}

TESTCASE (basic_block_can_be_invalidated_for_current_thread)
{
  StalkerDummyChannel channel;
  GThread * thread;
  GumThreadId thread_id;
  gpointer target_function_int_addr;

#if (defined (HAVE_ANDROID) && defined (HAVE_ARM)) || defined (HAVE_QNX)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  sdc_init (&channel);

  thread = g_thread_new ("stalker-test-target",
      run_stalked_through_block_invalidated_in_callout, &channel);
  thread_id = sdc_await_thread_id (&channel);

#ifdef HAVE_ARM
  target_function_int_addr =
      GSIZE_TO_POINTER (GPOINTER_TO_SIZE (target_function_int) & ~1);
#else
  target_function_int_addr = target_function_int;
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const targetThreadId = %" G_GSIZE_FORMAT ";"
      "const targetFuncInt = " GUM_PTR_CONST ";"
      "const targetFuncIntAddr = " GUM_PTR_CONST ";"

      "let instrumentationVersion = 0;"
      "let calls = 0;"

      "Stalker.follow(targetThreadId, {"
      "  transform(iterator) {"
      "    let i = 0;"
      "    let instruction;"
      "    while ((instruction = iterator.next()) !== null) {"
      "      if (i === 0 && instruction.address.equals(targetFuncIntAddr)) {"
      "        const v = instrumentationVersion++;"
      "        iterator.putCallout(() => {"
      "          send(`f() version=${v}`);"
      "          if (++calls === 3) {"
      "            Stalker.invalidate(targetFuncInt);"
      "          }"
      "        });"
      "      }"

      "      iterator.keep();"

      "      i++;"
      "    }"
      "  }"
      "});"

      "recv('stop', message => {"
      "  Stalker.unfollow(targetThreadId);"
      "  Stalker.flush();"
      "});"

      "send('ready');",

      thread_id,
      target_function_int,
      target_function_int_addr);
  EXPECT_SEND_MESSAGE_WITH ("\"ready\"");

  EXPECT_NO_MESSAGES ();
  sdc_put_follow_confirmation (&channel);

  EXPECT_SEND_MESSAGE_WITH ("\"f() version=0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"f() version=0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"f() version=0\"");
  EXPECT_SEND_MESSAGE_WITH ("\"f() version=1\"");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_NO_MESSAGES ();

  sdc_put_finish_confirmation (&channel);

  g_thread_join (thread);

  sdc_finalize (&channel);
}

static gpointer
run_stalked_through_block_invalidated_in_callout (gpointer data)
{
  StalkerDummyChannel * channel = data;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  target_function_int (42);
  target_function_int (42);
  target_function_int (42);

  target_function_int (42);

  sdc_await_finish_confirmation (channel);

  return NULL;
}

TESTCASE (basic_block_can_be_invalidated_for_specific_thread)
{
  StalkerDummyChannel channel;
  GThread * thread;
  GumThreadId thread_id;
  gpointer target_function_int_addr;

#if (defined (HAVE_ANDROID) && defined (HAVE_ARM)) || defined (HAVE_QNX)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  sdc_init (&channel);

  thread = g_thread_new ("stalker-test-target",
      run_stalked_through_block_invalidated_by_request, &channel);
  thread_id = sdc_await_thread_id (&channel);

#ifdef HAVE_ARM
  target_function_int_addr =
      GSIZE_TO_POINTER (GPOINTER_TO_SIZE (target_function_int) & ~1);
#else
  target_function_int_addr = target_function_int;
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const targetThreadId = %" G_GSIZE_FORMAT ";"
      "const targetFuncInt = " GUM_PTR_CONST ";"
      "const targetFuncIntAddr = " GUM_PTR_CONST ";"

      "let instrumentationVersion = 0;"

      "Stalker.follow(targetThreadId, {"
      "  transform(iterator) {"
      "    let i = 0;"
      "    let instruction;"
      "    while ((instruction = iterator.next()) !== null) {"
      "      if (i === 0 && instruction.address.equals(targetFuncIntAddr)) {"
      "        const v = instrumentationVersion++;"
      "        iterator.putCallout(() => {"
      "          send(`f() version=${v}`);"
      "        });"
      "      }"

      "      iterator.keep();"

      "      i++;"
      "    }"
      "  }"
      "});"

      "recv('invalidate', message => {"
      "  Stalker.invalidate(targetThreadId, targetFuncInt);"
      "  send('invalidated');"
      "});"

      "recv('stop', message => {"
      "  Stalker.unfollow(targetThreadId);"
      "  Stalker.flush();"
      "});"

      "send('ready');",

      thread_id,
      target_function_int,
      target_function_int_addr);
  EXPECT_SEND_MESSAGE_WITH ("\"ready\"");

  EXPECT_NO_MESSAGES ();
  sdc_put_follow_confirmation (&channel);

  EXPECT_SEND_MESSAGE_WITH ("\"f() version=0\"");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"invalidate\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"invalidated\"");
  EXPECT_NO_MESSAGES ();

  sdc_put_run_confirmation (&channel);
  EXPECT_SEND_MESSAGE_WITH ("\"f() version=1\"");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_NO_MESSAGES ();

  sdc_put_finish_confirmation (&channel);

  g_thread_join (thread);

  sdc_finalize (&channel);
}

static gpointer
run_stalked_through_block_invalidated_by_request (gpointer data)
{
  StalkerDummyChannel * channel = data;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  target_function_int (42);

  sdc_await_run_confirmation (channel);

  target_function_int (42);

  sdc_await_finish_confirmation (channel);

  return NULL;
}

TESTCASE (call_can_be_probed)
{
  StalkerDummyChannel channel;
  GThread * thread;
  GumThreadId thread_id;

  sdc_init (&channel);

  thread = g_thread_new ("stalker-test-target",
      run_stalked_through_target_function, &channel);
  thread_id = sdc_await_thread_id (&channel);

  COMPILE_AND_LOAD_SCRIPT (
      "const targetThreadId = %" G_GSIZE_FORMAT ";"

      "Stalker.addCallProbe(" GUM_PTR_CONST ", args => {"
      "  send(args[0].toInt32());"
      "});"

      "Stalker.follow(targetThreadId);"

      "recv('stop', message => {"
      "  Stalker.unfollow(targetThreadId);"
      "});"

      "send('ready');",

      thread_id,
      target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("\"ready\"");

  EXPECT_NO_MESSAGES ();
  sdc_put_follow_confirmation (&channel);
  EXPECT_SEND_MESSAGE_WITH ("1337");

  POST_MESSAGE ("{\"type\":\"stop\"}");

  g_thread_join (thread);

  sdc_finalize (&channel);
}

static gpointer
run_stalked_through_target_function (gpointer data)
{
  StalkerDummyChannel * channel = data;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  target_function_int (1337);

  return NULL;
}

#endif

TESTCASE (stalker_events_can_be_parsed)
{
  GumEvent ev;

  ev.type = GUM_CALL;
  ev.call.location = GSIZE_TO_POINTER (7);
  ev.call.target = GSIZE_TO_POINTER (12);
  ev.call.depth = 42;
  COMPILE_AND_LOAD_SCRIPT ("send(Stalker.parse(" GUM_PTR_CONST ".readByteArray("
      "%" G_GSIZE_FORMAT ")));", &ev, sizeof (ev));
  EXPECT_SEND_MESSAGE_WITH ("[[\"call\",\"0x7\",\"0xc\",42]]");

  COMPILE_AND_LOAD_SCRIPT ("send(Stalker.parse(new ArrayBuffer(0)));");
  EXPECT_SEND_MESSAGE_WITH ("[]");

  COMPILE_AND_LOAD_SCRIPT ("send(Stalker.parse(new ArrayBuffer(1)));");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid buffer shape");

  COMPILE_AND_LOAD_SCRIPT ("send(Stalker.parse(new ArrayBuffer(%" G_GSIZE_FORMAT
      ")));", sizeof (GumEvent));
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid event type");
}

TESTCASE (frida_version_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(typeof Frida.version);");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
}

TESTCASE (frida_heap_size_can_be_queried)
{
  COMPILE_AND_LOAD_SCRIPT ("send(typeof Frida.heapSize);");
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
}

TESTCASE (process_arch_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Process.arch);");
#if defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 4
  EXPECT_SEND_MESSAGE_WITH ("\"ia32\"");
# else
  EXPECT_SEND_MESSAGE_WITH ("\"x64\"");
# endif
#elif defined (HAVE_ARM)
  EXPECT_SEND_MESSAGE_WITH ("\"arm\"");
#elif defined (HAVE_ARM64)
  EXPECT_SEND_MESSAGE_WITH ("\"arm64\"");
#endif
}

TESTCASE (process_platform_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Process.platform);");
#if defined (HAVE_LINUX)
  EXPECT_SEND_MESSAGE_WITH ("\"linux\"");
#elif defined (HAVE_DARWIN)
  EXPECT_SEND_MESSAGE_WITH ("\"darwin\"");
#elif defined (HAVE_WINDOWS)
  EXPECT_SEND_MESSAGE_WITH ("\"windows\"");
#endif
}

TESTCASE (process_page_size_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Process.pageSize);");
  EXPECT_SEND_MESSAGE_WITH ("%d", gum_query_page_size ());
}

TESTCASE (process_pointer_size_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Process.pointerSize);");
  EXPECT_SEND_MESSAGE_WITH (G_STRINGIFY (GLIB_SIZEOF_VOID_P));
}

TESTCASE (process_should_support_nested_signal_handling)
{
#ifdef HAVE_LINUX
  gsize page_size;
  gpointer page;

  if (!check_exception_handling_testable ())
    return;

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_NO_ACCESS);

  COMPILE_AND_LOAD_SCRIPT ("Process.setExceptionHandler(details => {"
          "Memory.protect(" GUM_PTR_CONST ", Process.pageSize, 'rw-');"
          "try {"
              "ptr(42).readU8();"
          "} catch (e) {"
              "send('error');"
          "};"
          "return true;"
      "});", page);

  *((guint8 *) page) = 1;
  EXPECT_SEND_MESSAGE_WITH ("\"error\"");

  gum_memory_free ((gpointer) page, page_size);
#else
  g_print ("<skipping, only supported on Linux for now> ");
#endif
}

TESTCASE (process_current_dir_can_be_queried)
{
  COMPILE_AND_LOAD_SCRIPT ("send(typeof Process.getCurrentDir());");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
}

TESTCASE (process_home_dir_can_be_queried)
{
  COMPILE_AND_LOAD_SCRIPT ("send(typeof Process.getHomeDir());");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
}

TESTCASE (process_tmp_dir_can_be_queried)
{
  COMPILE_AND_LOAD_SCRIPT ("send(typeof Process.getTmpDir());");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
}

TESTCASE (process_debugger_status_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(Process.isDebuggerAttached());");
  if (gum_process_is_debugger_attached ())
    EXPECT_SEND_MESSAGE_WITH ("true");
  else
    EXPECT_SEND_MESSAGE_WITH ("false");
}

TESTCASE (process_id_is_available)
{
  TestScriptMessageItem * item;
  gint pid;

  COMPILE_AND_LOAD_SCRIPT ("send(Process.id);");

  item = test_script_fixture_pop_message (fixture);
  pid = 0;
  sscanf (item->message, "{\"type\":\"send\",\"payload\":%d}", &pid);
  g_assert_cmpint (pid, ==, gum_process_get_id ());
  test_script_message_item_free (item);
}

TESTCASE (process_current_thread_id_is_available)
{
  COMPILE_AND_LOAD_SCRIPT ("send(typeof Process.getCurrentThreadId());");
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
}

TESTCASE (process_thread_can_be_looked_up_by_id)
{
#ifdef HAVE_LINUX
  if (!check_exception_handling_testable ())
    return;
#endif

#ifdef HAVE_MIPS
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const id = Process.getCurrentThreadId();"
      "const t = Process.getThreadById(id);"
      "send(t.id === id && t.context !== undefined);");
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "try {"
      "  Process.getThreadById(0x7fffffff);"
      "  send('no throw');"
      "} catch (e) {"
      "  send(e instanceof Error);"
      "}");
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "const id = Process.getCurrentThreadId();"
      "const t = Process.findThreadById(id);"
      "send(t !== null && t.id === id && t.context !== undefined);");
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "send(Process.findThreadById(0x7fffffff));");
  EXPECT_SEND_MESSAGE_WITH ("null");
}

TESTCASE (process_threads_can_be_enumerated)
{
  gboolean done = FALSE;
  GThread * thread_a, * thread_b;

#ifdef HAVE_LINUX
  if (!check_exception_handling_testable ())
    return;
#endif

#ifdef HAVE_MIPS
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const threads = Process.enumerateThreads();"
      "send(threads.length > 0);");
  EXPECT_SEND_MESSAGE_WITH ("true");

  thread_a = g_thread_new ("script-test-sleeping-dummy-a", sleeping_dummy,
      &done);
  thread_b = g_thread_new ("script-test-sleeping-dummy-b", sleeping_dummy,
      &done);

  COMPILE_AND_LOAD_SCRIPT ("send(Process.enumerateThreads().length >= 2);");
  EXPECT_SEND_MESSAGE_WITH ("true");

  done = TRUE;
  g_thread_join (thread_b);
  g_thread_join (thread_a);
}

static gpointer
sleeping_dummy (gpointer data)
{
  volatile gboolean * done = (gboolean *) data;

  while (!(*done))
    g_usleep (G_USEC_PER_SEC / 4);

  return NULL;
}

TESTCASE (process_threads_have_names)
{
#if defined (HAVE_LINUX) && !defined (HAVE_PTHREAD_SETNAME_NP)
  g_print ("<skipping, libc is too old> ");
#else
  GumNamedSleeperContext ctx;
  GThread * thread;

# ifdef HAVE_LINUX
  if (!check_exception_handling_testable ())
    return;
# endif

# ifdef HAVE_MIPS
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
# endif

  ctx.controller_messages = g_async_queue_new ();
  ctx.sleeper_messages = g_async_queue_new ();

  thread = g_thread_new ("named-sleeper", named_sleeper, &ctx);
  g_assert_cmpstr (g_async_queue_pop (ctx.controller_messages), ==, "ready");

  COMPILE_AND_LOAD_SCRIPT (
      "send(Process.enumerateThreads().some(t => t.name === 'named-sleeper'));"
  );
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  g_async_queue_push (ctx.sleeper_messages, "done");
  g_thread_join (thread);

  g_async_queue_unref (ctx.sleeper_messages);
  g_async_queue_unref (ctx.controller_messages);
#endif
}

static gpointer
named_sleeper (gpointer data)
{
  GumNamedSleeperContext * ctx = data;

  gum_ensure_current_thread_is_named ("named-sleeper");

  g_async_queue_push (ctx->controller_messages, "ready");

  g_assert_cmpstr (g_async_queue_pop (ctx->sleeper_messages), ==, "done");

  return NULL;
}

TESTCASE (process_modules_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const modules = Process.enumerateModules();"
      "send(modules.length > 0);"
      "const m = modules[0];"
      "send(typeof m.name === 'string');"
      "send(typeof m.path === 'string');"
      "send(m.base instanceof NativePointer);"
      "send(typeof m.size === 'number');"
      "send(JSON.stringify(m) !== \"{}\");");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (process_modules_can_be_observed)
{
  COMPILE_AND_LOAD_SCRIPT (
      "let addsObserved = 0;"
      "const observer = Process.attachModuleObserver({"
      "  onAdded(module) {"
      "    addsObserved++;"
      "  }"
      "});"
      "send(addsObserved === Process.enumerateModules().length);"
      "observer.detach();");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (process_module_can_be_looked_up_from_address)
{
#ifndef HAVE_LINUX
  GumModule * system_module;
  gpointer f;

  system_module = gum_process_find_module_by_name (SYSTEM_MODULE_NAME);
  g_assert_nonnull (system_module);

  f = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (system_module, SYSTEM_MODULE_EXPORT));
  g_assert_nonnull (f);

  COMPILE_AND_LOAD_SCRIPT (
      "send(Process.findModuleByAddress(" GUM_PTR_CONST ".strip()) !== null);",
      f);
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "send(Object.keys(Process.getModuleByAddress(" GUM_PTR_CONST
      ".strip()).toJSON()).length > 0);",
      f);
  EXPECT_SEND_MESSAGE_WITH ("true");

  g_object_unref (system_module);
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const someModule = Process.enumerateModules()[1];"
      "const foundModule = Process.findModuleByAddress(someModule.base);"
      "send(foundModule !== null);"
      "send(foundModule.name === someModule.name);");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  /*
   * We can hit problems here with targets where the ld.so library doesn't have
   * contiguous segments (such as on aarch64_be). On these systems, libraries
   * can be loaded within the gaps between these non-contiguous segments. The
   * VDSO, due to its small size, can be located within these gaps. Thus, when
   * attempting to find a module based upon the base address of the VDSO, we may
   * instead find the ld.so library.
   *
   * In order to accomodate this issue, rather than checking that we find the
   * module we expect by matching its name, we instead check that the returned
   * module simply contains the address which we queried.
   *
   * For APIS such as `findName` and `findPath`, we match these against the
   * values of the library found when calling `get` (since we expect at least
   * our implementation to return a consistent result for a requested address),
   * even though there may infact be more than one module which contains the
   * given address.
   */
  COMPILE_AND_LOAD_SCRIPT (
      "const map = new ModuleMap();"
      "const someModule = Process.enumerateModules()[1];"
      "const someBase = someModule.base;"

      "send(map.has(someBase));"
      "send(map.has(ptr(1)));"

      "let foundModule = map.find(someBase);"
      "send(foundModule !== null);"
      "send(foundModule.base.compare(someBase) <= 0);"
      "send(foundModule.base.add(foundModule.size).compare(someBase) > 0);"
      "send(map.find(ptr(1)));"

      "map.update();"
      "foundModule = map.get(someBase);"
      "send(foundModule.base.compare(someBase) <= 0);"
      "send(foundModule.base.add(foundModule.size).compare(someBase) > 0);"
      "try {"
      "  map.get(ptr(1));"
      "} catch (e) {"
      "  send(e.message);"
      "}"

      "send(map.findName(someBase) === foundModule.name);"
      "send(map.findName(ptr(1)));"
      "send(map.getName(someBase) === foundModule.name);"
      "try {"
      "  map.getName(ptr(1));"
      "} catch (e) {"
      "  send(e.message);"
      "}"

      "send(map.findPath(someBase) === foundModule.path);"
      "send(map.findPath(ptr(1)));"
      "send(map.getPath(someBase) === foundModule.path);"
      "try {"
      "  map.getPath(ptr(1));"
      "} catch (e) {"
      "  send(e.message);"
      "}");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("false");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("null");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"unable to find module containing 0x1\"");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"unable to find module containing 0x1\"");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"unable to find module containing 0x1\"");

  EXPECT_NO_MESSAGES ();

#ifdef HAVE_DARWIN
  COMPILE_AND_LOAD_SCRIPT (
      "const systemModule = Process.enumerateModules()"
      "  .filter(m => m.path.startsWith('/System/'))[0];"
      "const map = new ModuleMap(m => !m.path.startsWith('/System/'));"
      "const foundModule = map.find(systemModule.base);"
      "send(foundModule === null);");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
#endif
}

TESTCASE (process_module_can_be_looked_up_from_name)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(Process.findModuleByName('%s') !== null);",
      SYSTEM_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "send(Object.keys(Process.getModuleByName('%s').toJSON()).length > 0);",
      SYSTEM_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (process_ranges_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const ranges = Process.enumerateRanges('--x');"
      "send(ranges.length > 0);");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (process_ranges_can_be_enumerated_with_neighbors_coalesced)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const a = Process.enumerateRanges('--x');"
      "const b = Process.enumerateRanges({"
        "protection: '--x',"
        "coalesce: true"
      "});"
      "send(b.length <= a.length);");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (process_range_can_be_looked_up_from_address)
{
  GumModule * system_module;
  gpointer f;

  system_module = gum_process_find_module_by_name (SYSTEM_MODULE_NAME);
  g_assert_nonnull (system_module);

  f = GSIZE_TO_POINTER (gum_module_find_export_by_name (
      system_module, SYSTEM_MODULE_EXPORT));
  g_assert_nonnull (f);

  g_object_unref (system_module);

  COMPILE_AND_LOAD_SCRIPT (
      "send(Process.findRangeByAddress(" GUM_PTR_CONST ".strip()) !== null);",
      f);
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "const someRange = Process.enumerateRanges('r-x')[1];"
      "const foundRange = Process.findRangeByAddress(someRange.base);"
      "send(foundRange !== null);"
      "send(foundRange.base.equals(someRange.base));");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "send(Object.keys(Process.getRangeByAddress(" GUM_PTR_CONST
      ".strip())).length > 0);",
      f);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (process_function_range_can_be_looked_up_from_address)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = " GUM_PTR_CONST ".strip();"
      "const range = Process.findFunctionRange(f);"
      "send(range !== null);"
      "send(range.base.compare(f) <= 0 &&"
      "    f.compare(range.base.add(range.size)) < 0);",
      gum_get_answer_to_life_universe_and_everything);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

#ifdef HAVE_DARWIN

TESTCASE (process_malloc_ranges_can_be_enumerated)
{
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "const ranges = Process.enumerateMallocRanges();"
      "send(ranges.length > 0);");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

#endif

TESTCASE (process_can_run_on_thread_with_success)
{
  GThread * thread;
  GumThreadId thread_id;
  gboolean done = FALSE;

  thread = create_sleeping_dummy_thread_sync (&done, &thread_id);

  COMPILE_AND_LOAD_SCRIPT (
      "const getLocalThreadStringValue = new NativeFunction(" GUM_PTR_CONST ", "
        "'pointer', []);"
      "Process.runOnThread(0x%" G_GSIZE_MODIFIER "x, () => {"
      "  return getLocalThreadStringValue().readUtf8String();"
      "})"
      ".then(str => { send(str); });",
      get_local_thread_string_value,
      thread_id);

  EXPECT_SEND_MESSAGE_WITH ("\"53Cr3t\"");

  done = TRUE;
  g_thread_join (thread);

  EXPECT_NO_MESSAGES ();
}

TESTCASE (process_can_run_on_thread_with_failure)
{
  GThread * thread;
  GumThreadId thread_id;
  gboolean done = FALSE;

  thread = create_sleeping_dummy_thread_sync (&done, &thread_id);

  COMPILE_AND_LOAD_SCRIPT (
      "Process.runOnThread(0x%" G_GSIZE_MODIFIER "x, () => {"
      "  throw new Error('epic fail');"
      "})"
      ".catch(e => { send(e.message); });",
      thread_id);

  EXPECT_SEND_MESSAGE_WITH ("\"epic fail\"");

  done = TRUE;
  g_thread_join (thread);

  EXPECT_NO_MESSAGES ();
}

static GThread *
create_sleeping_dummy_thread_sync (gboolean * done,
                                   GumThreadId * thread_id)
{
  TestRunOnThreadSyncContext sync_data;
  GThread * thread;

  g_mutex_init (&sync_data.mutex);
  g_cond_init (&sync_data.cond);
  sync_data.started = FALSE;
  sync_data.thread_id = 0;
  sync_data.done = done;

  g_mutex_lock (&sync_data.mutex);

  thread = g_thread_new ("gumjs-test-sleeping-dummy-func", sleeping_dummy_func,
      &sync_data);

  while (!sync_data.started)
    g_cond_wait (&sync_data.cond, &sync_data.mutex);

  if (thread_id != NULL)
    *thread_id = sync_data.thread_id;

  g_mutex_unlock (&sync_data.mutex);

  g_cond_clear (&sync_data.cond);
  g_mutex_clear (&sync_data.mutex);

  return thread;
}

static gpointer
sleeping_dummy_func (gpointer data)
{
  TestRunOnThreadSyncContext * sync_data = data;
  gboolean * done = sync_data->done;

  g_private_replace (&target_thread_string_value, g_strdup ("53Cr3t"));

  g_mutex_lock (&sync_data->mutex);
  sync_data->started = TRUE;
  sync_data->thread_id = gum_process_get_current_thread_id ();
  g_cond_signal (&sync_data->cond);
  g_mutex_unlock (&sync_data->mutex);

  while (!(*done))
    g_thread_yield ();

  return NULL;
}

static const gchar *
get_local_thread_string_value (void)
{
  return g_private_get (&target_thread_string_value);
}

TESTCASE (process_system_ranges_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const ranges = Process.enumerateSystemRanges();"
      "console.log(JSON.stringify(ranges, null, 2));");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (module_imports_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const imports = Process.getModuleByName('%s').enumerateImports();"
      "send(imports.length > 0);",
      GUM_TESTS_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (module_exports_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const exports = Process.getModuleByName('%s').enumerateExports();"
      "send(exports.length > 0);"
      "const e = exports[0];"
      "send(typeof e.type === 'string');"
      "send(typeof e.name === 'string');"
      "send(e.address instanceof NativePointer);"
      "send(JSON.stringify(e) !== \"{}\");",
      SYSTEM_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (module_exports_enumeration_performance)
{
  TestScriptMessageItem * item;
  gint duration;

  COMPILE_AND_LOAD_SCRIPT (
      "const module = Process.getModuleByName('%s');"
      "const start = Date.now();"
      "module.enumerateExports();"
      "send(Date.now() - start);",
      SYSTEM_MODULE_NAME);
  item = test_script_fixture_pop_message (fixture);
  sscanf (item->message, "{\"type\":\"send\",\"payload\":%d}", &duration);
  g_print ("<%d ms> ", duration);
  test_script_message_item_free (item);
}

TESTCASE (module_symbols_can_be_enumerated)
{
#ifndef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "const symbols = Process.getModuleByName('%s').enumerateSymbols();"
      "send(symbols.length > 0);"
      "const s = symbols[0];"
      "send(typeof s.isGlobal === 'boolean');"
      "send(typeof s.type === 'string');"
      "send(typeof s.name === 'string');"
      "send(s.address instanceof NativePointer);"
      "send(JSON.stringify(s) !== \"{}\");",
      GUM_TESTS_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
#else
  g_print ("<skipping on this platform> ");
#endif
}

TESTCASE (module_ranges_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const ranges = Process.getModuleByName('%s').enumerateRanges('--x');"
      "send(ranges.length > 0);",
      SYSTEM_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (module_sections_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const sections = Process.getModuleByName('%s').enumerateSections();"
      "send(sections.length > 0);",
      SYSTEM_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (module_dependencies_can_be_enumerated)
{
#if defined (HAVE_DARWIN) || defined (HAVE_ELF)
  COMPILE_AND_LOAD_SCRIPT (
      "const deps = Process.getModuleByName('%s').enumerateDependencies();"
      "send(deps.length > 0);",
      GUM_TESTS_MODULE_NAME);
  EXPECT_SEND_MESSAGE_WITH ("true");
#endif
}

TESTCASE (module_export_can_be_found_by_name)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const sysModuleName = '%s';"
      "const sysModuleExport = '%s';"
      "const badModuleExport = sysModuleExport + '_does_not_exist';"

      "const sysModule = Process.getModuleByName(sysModuleName);"

      "const impl = sysModule.findExportByName(sysModuleExport);"
      "send(impl !== null);"

      "try {"
          "send(sysModule.getExportByName(sysModuleExport).equals(impl));"

          "sysModule.getExportByName(badModuleExport);"
          "send('should not get here');"
      "} catch (e) {"
          "send(/unable to find export/.test(e.message));"
      "}",
      SYSTEM_MODULE_NAME, SYSTEM_MODULE_EXPORT);

  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");

#ifdef HAVE_WINDOWS
  HMODULE mod;
  gpointer actual_address;
  char actual_address_str[32];

  mod = GetModuleHandleW (L"kernel32.dll");
  g_assert_nonnull (mod);
  actual_address = GetProcAddress (mod, "Sleep");
  g_assert_nonnull (actual_address);
  sprintf_s (actual_address_str, sizeof (actual_address_str),
      "\"%" G_GSIZE_MODIFIER "x\"", GPOINTER_TO_SIZE (actual_address));

  COMPILE_AND_LOAD_SCRIPT ("send(Process.getModuleByName('kernel32.dll')"
      ".getExportByName('Sleep').toString(16));");
  EXPECT_SEND_MESSAGE_WITH (actual_address_str);
#endif
}

TESTCASE (module_symbol_can_be_found_by_name)
{
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "const sysModuleName = '%s';"

      "const sysModule = Process.getModuleByName(sysModuleName);"

      "const allSymbols = sysModule.enumerateSymbols();"
      "const sysModuleExport = allSymbols.find(s => !s.address.isNull()).name;"
      "const badModuleExport = sysModuleExport + '_does_not_exist';"

      "const impl = sysModule.findSymbolByName(sysModuleExport);"
      "send(impl !== null);"

      "try {"
          "send(sysModule.getSymbolByName(sysModuleExport).equals(impl));"

          "sysModule.getSymbolByName(badModuleExport);"
          "send('should not get here');"
      "} catch (e) {"
          "send(/unable to find symbol/.test(e.message));"
      "}",
      SYSTEM_MODULE_NAME);

  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (module_can_be_loaded)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const moduleName = '%s';"
      "const moduleExport = '%s';"
      "const m = Module.load(moduleName);"
      "send(m.findExportByName(moduleExport) !== null);"
      "try {"
      "  Module.load(moduleName + '_nope');"
      "  send('success');"
      "} catch (e) {"
      "  send('error');"
      "}",
      SYSTEM_MODULE_NAME, SYSTEM_MODULE_EXPORT);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"error\"");
}

TESTCASE (module_can_be_forcibly_initialized)
{
  COMPILE_AND_LOAD_SCRIPT ("Process.getModuleByName('%s').ensureInitialized();",
      SYSTEM_MODULE_NAME);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (module_map_values_should_have_module_prototype)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const map = new ModuleMap();"
      "send(map.values()[0] instanceof Module);");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

#ifdef HAVE_WINDOWS
# define API_RESOLVER_TEST_QUERY "exports:*!_open*"
#else
# define API_RESOLVER_TEST_QUERY "exports:*!open*"
#endif

TESTCASE (api_resolver_can_be_used_to_find_functions)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const resolver = new ApiResolver('module');"
      "const matches = resolver.enumerateMatches('%s');"
      "send(matches.length > 0);"
      "const m = matches[0];"
      "send(typeof m.name);"
      "send(m.address instanceof NativePointer);"
      "send(typeof m.size);",
      API_RESOLVER_TEST_QUERY);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
}

TESTCASE (api_resolver_can_be_used_to_find_sections)
{
#if defined (HAVE_DARWIN) || defined (HAVE_ELF)
  COMPILE_AND_LOAD_SCRIPT (
      "const resolver = new ApiResolver('module');"
      "const matches = resolver.enumerateMatches('sections:*!*data*');"
      "send(matches.length > 0);"
      "const m = matches[0];"
      "send(typeof m.name);"
      "send(m.address instanceof NativePointer);"
      "send(typeof m.size);",
      API_RESOLVER_TEST_QUERY);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"string\"");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
#endif
}

TESTCASE (invalid_script_should_return_null)
{
  GError * err = NULL;

  g_assert_null (gum_script_backend_create_sync (fixture->backend, "testcase",
      "'", NULL, NULL, NULL));

  g_assert_null (gum_script_backend_create_sync (fixture->backend, "testcase",
      "'", NULL, NULL, &err));
  g_assert_nonnull (err);
  g_assert_true (g_str_has_prefix (err->message,
      "Script(line 1): SyntaxError: "));
}

TESTCASE (strict_mode_should_be_enforced)
{
  COMPILE_AND_LOAD_SCRIPT (
      "function run() {"
      "  oops = 1337;"
      "}"
      "run();");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
      ? "ReferenceError: 'oops' is not defined"
      : "ReferenceError: oops is not defined");
}

TESTCASE (array_buffer_can_be_created)
{
  COMPILE_AND_LOAD_SCRIPT ("new ArrayBuffer(16);");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (method_can_be_called_sync)
{
  COMPILE_AND_LOAD_SCRIPT ("rpc.exports.add = (a, b) => a + b;");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"add\",[1,2]]");
  EXPECT_SEND_MESSAGE_WITH ("[\"frida:rpc\",42,\"ok\",3]");
}

TESTCASE (method_can_be_called_async)
{
  COMPILE_AND_LOAD_SCRIPT ("rpc.exports.add = async (a, b) => a + b;");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"add\",[1,2]]");
  EXPECT_SEND_MESSAGE_WITH ("[\"frida:rpc\",42,\"ok\",3]");
}

TESTCASE (method_can_throw_sync)
{
  COMPILE_AND_LOAD_SCRIPT (
      "rpc.exports.raise = () => { throw new Error('no'); }");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"raise\",[]]");
  EXPECT_SEND_MESSAGE_WITH_PREFIX ("[\"frida:rpc\",42,\"error\",\"no\",");
}

TESTCASE (method_can_throw_async)
{
  COMPILE_AND_LOAD_SCRIPT (
      "rpc.exports.raise = async () => { throw new Error('no'); }");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"raise\",[]]");
  EXPECT_SEND_MESSAGE_WITH_PREFIX ("[\"frida:rpc\",42,\"error\",\"no\",");
}

TESTCASE (method_can_return_null)
{
  COMPILE_AND_LOAD_SCRIPT ("rpc.exports.returnNull = () => null;");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"returnNull\",[]]");
  EXPECT_SEND_MESSAGE_WITH ("[\"frida:rpc\",42,\"ok\",null]");
}

TESTCASE (method_can_receive_binary_data)
{
  const guint8 data_to_send[2] = { 0x13, 0x37 };
  GBytes * bytes;

  COMPILE_AND_LOAD_SCRIPT (
      "rpc.exports.eat = (str, data) => {"
          "send(str, data);"
      "}");

  bytes = g_bytes_new_static (data_to_send, sizeof (data_to_send));
  gum_script_post (fixture->script,
      "[\"frida:rpc\",42,\"call\",\"eat\",[\"yoghurt\"]]", bytes);
  g_bytes_unref (bytes);

  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"yoghurt\"", "13 37");
  EXPECT_SEND_MESSAGE_WITH ("[\"frida:rpc\",42,\"ok\",null]");
}

TESTCASE (method_can_return_binary_data)
{
  COMPILE_AND_LOAD_SCRIPT (
      "rpc.exports.read = () => {\n"
          "const buf = Memory.allocUtf8String(\"Yo\");\n"
          "return buf.readByteArray(2);\n"
      "};\n");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"read\",[]]");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("[\"frida:rpc\",42,\"ok\",null]",
      "59 6f");
}

TESTCASE (method_can_return_value_and_binary_data)
{
  COMPILE_AND_LOAD_SCRIPT (
      "rpc.exports.read = () => {"
          "const buf = Memory.allocUtf8String(\"Yo\");"
          "return [{meta: 'data'}, buf.readByteArray(2)];"
      "};");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"read\",[]]");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA (
      "[\"frida:rpc\",42,\"ok\",null,{\"meta\":\"data\"}]",
      "59 6f");
}

TESTCASE (method_list_can_be_queried)
{
  COMPILE_AND_LOAD_SCRIPT ("rpc.exports = { a() {}, b() {}, c() {} };");
  POST_MESSAGE ("[\"frida:rpc\",42,\"list\"]");
  EXPECT_SEND_MESSAGE_WITH ("[\"frida:rpc\",42,\"ok\",[\"a\",\"b\",\"c\"]]");
}

TESTCASE (calling_inexistent_method_should_throw_error)
{
  COMPILE_AND_LOAD_SCRIPT ("");
  POST_MESSAGE ("[\"frida:rpc\",42,\"call\",\"banana\",[]]");
  EXPECT_SEND_MESSAGE_WITH ("[\"frida:rpc\",42,\"error\","
      "\"unable to find method 'banana'\"]");
}

TESTCASE (message_can_be_sent)
{
  COMPILE_AND_LOAD_SCRIPT ("send(1234);");
  EXPECT_SEND_MESSAGE_WITH ("1234");
}

TESTCASE (message_can_be_sent_with_data)
{
  COMPILE_AND_LOAD_SCRIPT ("send(1234, [0x13, 0x37]);");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("1234", "13 37");
}

TESTCASE (message_can_be_received)
{
  COMPILE_AND_LOAD_SCRIPT (
      "recv(message => {"
      "  if (message.type === 'ping')"
      "    send('pong');"
      "});");
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"ping\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"pong\"");
}

TESTCASE (recv_wait_in_an_application_thread_should_not_deadlock)
{
  GThread * worker_thread;
  GumInvokeTargetContext ctx;
  guint i;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ", new NativeCallback(arg => {"
      "  let timeToRecv;"
      "  let shouldExit = false;"
      "  while (true) {"
      "    recv(message => {"
      "       if (message.type === 'stop')"
      "         shouldExit = true;"
      "       else if (message.type === 'wait-until')"
      "         timeToRecv = message.time;"
      "       else"
      "         throw new Error(`unexpected message: ${message.type}`);"
      "    }).wait();"
      "    if (shouldExit)"
      "      return 0;"
      "    while (Date.now() < timeToRecv) {}"
      "    recv(message => {"
      "      if (message.type !== 'ping')"
      "        throw new Error(`unexpected message: ${message.type}`);"
      "    }).wait();"
      "    send('pong');"
      "  }"
      "}, 'int', ['int']));", target_function_int);

  ctx.script = fixture->script;
  ctx.repeat_duration = 0;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  while (ctx.started == 0)
    g_usleep (G_USEC_PER_SEC / 200);

  for (i = 0; i != 100; i++)
  {
    gint64 time_now, time_to_schedule_recv, time_to_post;
    gchar * msg;

    time_now = g_get_real_time ();
    time_to_schedule_recv = time_now - (time_now % (20 * 1000)) + (50 * 1000);
    time_to_post = time_to_schedule_recv + i;

    msg = g_strdup_printf (
        "{\"type\":\"wait-until\",\"time\":%" G_GINT64_FORMAT "}",
        time_to_schedule_recv / 1000);
    POST_MESSAGE (msg);
    g_free (msg);

    while (g_get_real_time () < time_to_post)
      ;
    POST_MESSAGE ("{\"type\":\"ping\"}");
    EXPECT_SEND_MESSAGE_WITH ("\"pong\"");
  }

  POST_MESSAGE ("{\"type\":\"stop\"}");

  g_thread_join (worker_thread);
  g_assert_cmpint (ctx.finished, ==, 1);
}

TESTCASE (message_can_be_received_with_data)
{
  const guint8 data_to_send[2] = { 0x13, 0x37 };
  GBytes * bytes;

  COMPILE_AND_LOAD_SCRIPT (
      "recv((message, data) => {"
      "  if (message.type === 'ping')"
      "    send('pong', data);"
      "});");
  EXPECT_NO_MESSAGES ();

  bytes = g_bytes_new_static (data_to_send, sizeof (data_to_send));
  gum_script_post (fixture->script, "{\"type\":\"ping\"}", bytes);
  g_bytes_unref (bytes);

  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"pong\"", "13 37");
}

TESTCASE (recv_may_specify_desired_message_type)
{
  COMPILE_AND_LOAD_SCRIPT (
      "recv('wobble', message => {"
      "  send('wibble');"
      "});"
      "recv('ping', message => {"
      "  send('pong');"
      "});");
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"ping\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"pong\"");
}

TESTCASE (recv_can_be_waited_for_from_an_application_thread)
{
  GThread * worker_thread;
  GumInvokeTargetContext ctx;

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    const op = recv('poke', pokeMessage => {"
      "      send('pokeBack');"
      "    });"
      "    op.wait();"
      "    send('pokeReceived');"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();

  ctx.script = fixture->script;
  ctx.repeat_duration = 0;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  while (ctx.started == 0)
    g_usleep (G_USEC_PER_SEC / 200);

  g_usleep (G_USEC_PER_SEC / 25);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (ctx.finished, ==, 0);

  POST_MESSAGE ("{\"type\":\"poke\"}");
  g_thread_join (worker_thread);
  g_assert_cmpint (ctx.finished, ==, 1);
  EXPECT_SEND_MESSAGE_WITH ("\"pokeBack\"");
  EXPECT_SEND_MESSAGE_WITH ("\"pokeReceived\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (recv_can_be_waited_for_from_two_application_threads)
{
  GThread * worker_thread1, * worker_thread2;
  GumInvokeTargetContext ctx;

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    const op = recv('poke', pokeMessage => {"
      "      send('pokeBack');"
      "    });"
      "    op.wait();"
      "    send('pokeReceived');"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();

  ctx.script = fixture->script;
  ctx.repeat_duration = 0;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread1 = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  g_usleep (G_USEC_PER_SEC / 25);
  worker_thread2 = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  while (ctx.started != 2)
    g_usleep (G_USEC_PER_SEC / 200);

  g_usleep (G_USEC_PER_SEC / 25);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (ctx.finished, ==, 0);

  POST_MESSAGE ("{\"type\":\"poke\"}");
  g_thread_join (worker_thread1);
  g_assert_cmpint (ctx.finished, ==, 1);
  EXPECT_SEND_MESSAGE_WITH ("\"pokeBack\"");
  EXPECT_SEND_MESSAGE_WITH ("\"pokeReceived\"");
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"poke\"}");
  g_thread_join (worker_thread2);
  g_assert_cmpint (ctx.finished, ==, 2);
  EXPECT_SEND_MESSAGE_WITH ("\"pokeBack\"");
  EXPECT_SEND_MESSAGE_WITH ("\"pokeReceived\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (recv_can_be_waited_for_from_our_js_thread)
{
  /*
   * We do the wait() in a setTimeout() as our test fixture loads the
   * script synchronously...
   */
  COMPILE_AND_LOAD_SCRIPT (
      "setTimeout(() => {"
      "  const op = recv('poke', pokeMessage => {"
      "    send('pokeBack');"
      "  });"
      "  op.wait();"
      "  send('pokeReceived');"
      "}, 0);");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"poke\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"pokeBack\"");
  EXPECT_SEND_MESSAGE_WITH ("\"pokeReceived\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (recv_wait_in_an_application_thread_should_throw_on_unload)
{
  GThread * worker_thread;
  GumInvokeTargetContext ctx;

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    const op = recv('poke', pokeMessage => {"
      "      send('pokeBack');"
      "    });"
      "    try {"
      "      op.wait();"
      "      send('pokeReceived');"
      "    } catch (e) {"
      "      send('oops: ' + e.message);"
      "    }"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();

  ctx.script = fixture->script;
  ctx.repeat_duration = 0;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);
  while (ctx.started == 0)
    g_usleep (G_USEC_PER_SEC / 200);

  g_usleep (G_USEC_PER_SEC / 25);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (ctx.finished, ==, 0);

  UNLOAD_SCRIPT ();
  g_thread_join (worker_thread);
  g_assert_cmpint (ctx.finished, ==, 1);
  EXPECT_SEND_MESSAGE_WITH ("\"oops: script is unloading\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (recv_wait_in_our_js_thread_should_throw_on_unload)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Script.pin();"
      "setTimeout(() => {"
      "  Script.unpin();"
      "  const op = recv('poke', pokeMessage => {"
      "    send('pokeBack');"
      "  });"
      "  try {"
      "    op.wait();"
      "    send('pokeReceived');"
      "  } catch (e) {"
      "    send('oops: ' + e.message);"
      "  }"
      "}, 0);");
  EXPECT_NO_MESSAGES ();

  UNLOAD_SCRIPT ();
  EXPECT_SEND_MESSAGE_WITH ("\"oops: script is unloading\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (recv_wait_should_not_leak)
{
  GThread * worker_thread;
  guint initial_heap_size;
  GumInvokeTargetContext ctx;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    const op = recv('input', onInput);"
      "    send('request-input');"
      "    op.wait();"
      "  }"
      "});"
      "function onInput() {"
      "}", target_function_int);
  EXPECT_NO_MESSAGES ();

  initial_heap_size = gum_peek_private_memory_usage ();

  ctx.script = fixture->script;
  ctx.repeat_duration = 3000;
  ctx.started = 0;
  ctx.finished = 0;
  worker_thread = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &ctx);

  do
  {
    TestScriptMessageItem * item;

    item = test_script_fixture_try_pop_message (fixture, 10);
    if (item != NULL)
    {
      const guint size = 1024 * 1024;
      gpointer dummy_data;
      GBytes * dummy_bytes;

      dummy_data = g_malloc (size);
      memset (dummy_data, g_random_int_range (0, G_MAXUINT8), size);
      dummy_bytes = g_bytes_new_take (dummy_data, size);

      gum_script_post (fixture->script, "{\"type\":\"input\"}", dummy_bytes);

      g_bytes_unref (dummy_bytes);

      test_script_message_item_free (item);
    }

    g_assert_cmpuint (gum_peek_private_memory_usage () / initial_heap_size,
        <, 1000);
  }
  while (!ctx.finished);

  g_thread_join (worker_thread);
}

static gpointer
invoke_target_function_int_worker (gpointer data)
{
  GumInvokeTargetContext * ctx = (GumInvokeTargetContext *) data;

  g_atomic_int_inc (&ctx->started);

  if (ctx->repeat_duration == 0)
  {
    target_function_int (42);
  }
  else
  {
    gdouble repeat_duration_in_seconds;
    GTimer * timer;

    repeat_duration_in_seconds = (gdouble) ctx->repeat_duration / 1000.0;
    timer = g_timer_new ();

    do
    {
      target_function_int (42);
    }
    while (g_timer_elapsed (timer, NULL) < repeat_duration_in_seconds);

    g_timer_destroy (timer);
  }

  g_atomic_int_inc (&ctx->finished);

  return NULL;
}

TESTCASE (message_can_be_logged)
{
  DISABLE_LOG_MESSAGE_HANDLING ();

  COMPILE_AND_LOAD_SCRIPT ("console.log('Hello', undefined, null, 1337, "
      "'world', true, { color: 'pink' });");
  EXPECT_LOG_MESSAGE_WITH ("info", "Hello undefined null 1337 world "
      "true [object Object]");

  COMPILE_AND_LOAD_SCRIPT ("console.warn('Trouble is coming');");
  EXPECT_LOG_MESSAGE_WITH ("warning", "Trouble is coming");

  COMPILE_AND_LOAD_SCRIPT ("console.error('Oh noes');");
  EXPECT_LOG_MESSAGE_WITH ("error", "Oh noes");
}

TESTCASE (thread_can_be_forced_to_sleep)
{
  GTimer * timer = g_timer_new ();
  COMPILE_AND_LOAD_SCRIPT ("Thread.sleep(0.25);");
  g_assert_cmpfloat (g_timer_elapsed (timer, NULL), >=, 0.2f);
  EXPECT_NO_MESSAGES ();
  g_timer_destroy (timer);
}

TESTCASE (thread_backtrace_can_be_captured_with_limit)
{
  COMPILE_AND_LOAD_SCRIPT (
      "send(Thread.backtrace(null, { limit: 2 }).length);");
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (hardware_breakpoint_can_be_set)
{
#if defined (HAVE_FREEBSD) || defined (HAVE_QNX)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "let threads = Process.enumerateThreads();\n"
      "Process.setExceptionHandler(e => {\n"
      "  if (!['breakpoint', 'single-step'].includes(e.type))\n"
      "    return false;\n"
      "  send('trapped');\n"
      "  threads.forEach(t => t.unsetHardwareBreakpoint(0));\n"
      "  return true;\n"
      "});\n"
      "threads = threads.filter(t => {\n"
      "  try {\n"
      "    t.setHardwareBreakpoint(0, " GUM_PTR_CONST ");\n"
      "    return true;\n"
      "  } catch (e) {\n"
      "    return false;\n"
      "  }\n"
      "});\n",
      target_function_int);
  EXPECT_NO_MESSAGES ();

  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("\"trapped\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (hardware_watchpoint_can_be_set)
{
  guint32 val = 42;

#if defined (HAVE_FREEBSD) || defined (HAVE_QNX)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  if (gum_is_running_under_rosetta ())
  {
    g_print ("<skipping, not supported under Rosetta> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "let threads = Process.enumerateThreads();\n"
      "Process.setExceptionHandler(e => {\n"
      "  if (!['breakpoint', 'single-step'].includes(e.type))\n"
      "    return false;\n"
      "  send('trapped');\n"
      "  threads.forEach(t => t.unsetHardwareWatchpoint(0));\n"
      "  return true;\n"
      "});\n"
      "threads = threads.filter(t => {\n"
      "  try {\n"
      "    t.setHardwareWatchpoint(0, " GUM_PTR_CONST ", 4, 'w');\n"
      "    return true;\n"
      "  } catch (e) {\n"
      "    return false;\n"
      "  }\n"
      "});\n",
      &val);
  EXPECT_NO_MESSAGES ();

  val = 1337;
  EXPECT_SEND_MESSAGE_WITH ("\"trapped\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (timeout_can_be_scheduled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "setTimeout(() => {"
      "  send(1337);"
      "}, 20);");
  EXPECT_NO_MESSAGES ();

  g_usleep (25000);
  EXPECT_SEND_MESSAGE_WITH ("1337");

  g_usleep (25000);
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "setTimeout(value => {"
      "  send(value);"
      "}, uint64(20), 1338);");
  EXPECT_NO_MESSAGES ();

  g_usleep (25000);
  EXPECT_SEND_MESSAGE_WITH ("1338");

  COMPILE_AND_LOAD_SCRIPT (
      "setTimeout(() => {"
      "  send(1227);"
      "});");
  g_usleep (10000);
  EXPECT_SEND_MESSAGE_WITH ("1227");
}

TESTCASE (timeout_can_be_cancelled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const timeout = setTimeout(() => {"
      "  send(1337);"
      "}, 20);"
      "clearTimeout(timeout);");
  g_usleep (25000);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (interval_can_be_scheduled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "setInterval(value => {"
      "  send(value);"
      "}, 20, 1337);");
  EXPECT_NO_MESSAGES ();

  g_usleep (25000);
  EXPECT_SEND_MESSAGE_WITH ("1337");

  g_usleep (25000);
  EXPECT_SEND_MESSAGE_WITH ("1337");
}

TESTCASE (interval_can_be_cancelled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "let count = 1;"
      "const interval = setInterval(() => {"
      "  send(count++);"
      "  if (count === 3)"
      "    clearInterval(interval);"
      "}, 20);");

  g_usleep (25000);
  EXPECT_SEND_MESSAGE_WITH ("1");

  g_usleep (25000);
  EXPECT_SEND_MESSAGE_WITH ("2");

  g_usleep (25000);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (callback_can_be_scheduled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "setImmediate(() => {"
      "  send(1337);"
      "});");
  EXPECT_SEND_MESSAGE_WITH ("1337");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (callback_can_be_scheduled_from_a_scheduled_callback)
{
  COMPILE_AND_LOAD_SCRIPT (
      "setImmediate(() => {"
      "  send(1337);"
      "  Script.nextTick(() => { send(1338); });"
      "  setImmediate(() => { send(1339); });"
      "});");
  EXPECT_SEND_MESSAGE_WITH ("1337");
  EXPECT_SEND_MESSAGE_WITH ("1338");
  EXPECT_SEND_MESSAGE_WITH ("1339");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (callback_can_be_cancelled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const id = setImmediate(() => {"
      "  send(1337);"
      "});"
      "clearImmediate(id);");
  g_usleep (25000);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (callback_can_be_scheduled_on_next_tick)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Script.nextTick(send, 1337, [0x13, 0x37, 0x0a]);");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("1337", "13 37 0a");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (timer_cancellation_apis_should_be_forgiving)
{
  COMPILE_AND_LOAD_SCRIPT (
      "clearTimeout(undefined);"
      "clearInterval(undefined);"
      "clearImmediate(undefined);");
  EXPECT_NO_MESSAGES ();
}

#ifndef HAVE_WINDOWS

TESTCASE (crash_on_thread_holding_js_lock_should_not_deadlock)
{
  struct sigaction sa;
  GThread * worker1, * worker2;
  GumInvokeTargetContext invoke_ctx;
  GumCrashExceptorContext crash_ctx;
  GumExceptor * exceptor;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  memset (&sa, 0, sizeof (sigaction));
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_NODEFER;
  sa.sa_sigaction = exit_on_sigsegv;
  sigaction (SIGSEGV, &sa, NULL);

  COMPILE_AND_LOAD_SCRIPT (
      "const strcmp = new NativeFunction("
      "    Module.getGlobalExportByName('strcmp'),"
      "    'int', ['pointer', 'pointer'],"
      "    {"
      "      scheduling: 'exclusive',"
      "      exceptions: 'propagate'"
      "    });"

      "Process.setExceptionHandler(() => {"
      "  console.log('never called');"
      "});"

      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    strcmp(ptr(341234213), ptr(3423423422));"
      "  }"
      "});",
      target_function_int);
  EXPECT_NO_MESSAGES ();

  invoke_ctx.script = fixture->script;
  invoke_ctx.repeat_duration = 1.0;
  invoke_ctx.started = 0;
  invoke_ctx.finished = 0;

  crash_ctx.called = FALSE;
  crash_ctx.backend = fixture->backend;

  exceptor = gum_exceptor_obtain ();
  gum_exceptor_add (exceptor, on_exceptor_called, &crash_ctx);

  worker1 = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &invoke_ctx);
  worker2 = g_thread_new ("script-test-worker-thread",
      invoke_target_function_int_worker, &invoke_ctx);

  while (invoke_ctx.started == 0)
    g_usleep (G_USEC_PER_SEC / 200);
  g_usleep (G_USEC_PER_SEC / 10);

  g_assert_true (crash_ctx.called);

  g_thread_join (worker1);
  g_thread_join (worker2);

  gum_exceptor_remove (exceptor, on_exceptor_called, &crash_ctx);
  g_object_unref (exceptor);
}

TESTCASE (recovered_crash_should_not_unbalance_interceptor)
{
  GumRecoverExceptorContext recover_ctx;
  GumExceptor * exceptor;
  GumInterceptor * interceptor;
  gpointer page;
  gsize page_size;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_mprotect (page, page_size, GUM_PAGE_NO_ACCESS);

  recover_ctx.called = FALSE;
  recover_ctx.page = page;

  COMPILE_AND_LOAD_SCRIPT (
      "const strcmp = new NativeFunction("
      "    Module.getGlobalExportByName('strcmp'),"
      "    'int', ['pointer', 'pointer'],"
      "    {"
      "      scheduling: 'exclusive',"
      "      exceptions: 'propagate'"
      "    });"

      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    strcmp(" GUM_PTR_CONST ", " GUM_PTR_CONST ");"
      "  }"
      "});",
      target_function_int, page, page);
  EXPECT_NO_MESSAGES ();

  exceptor = gum_exceptor_obtain ();
  gum_exceptor_add (exceptor, on_exceptor_recover, &recover_ctx);

  target_function_int (42);

  gum_exceptor_remove (exceptor, on_exceptor_recover, &recover_ctx);
  g_object_unref (exceptor);

  g_assert_true (recover_ctx.called);

  interceptor = gum_interceptor_obtain ();
  g_assert_true (gum_interceptor_flush (interceptor));
  g_object_unref (interceptor);

  gum_memory_free (page, page_size);
}

static void
exit_on_sigsegv (int sig,
                 siginfo_t * info,
                 void * context)
{
  exit (0);
}

static gboolean
on_exceptor_called (GumExceptionDetails * details,
                    gpointer user_data)
{
  GumCrashExceptorContext * ctx = user_data;

  ctx->called = TRUE;

#ifdef HAVE_DARWIN
  {
    GThread * worker = g_thread_new ("fake-crash-handler-thread",
        simulate_crash_handler, ctx);
    g_thread_join (worker);
  }
#endif

  return FALSE;
}

static gboolean
on_exceptor_recover (GumExceptionDetails * details,
                     gpointer user_data)
{
  GumRecoverExceptorContext * ctx = user_data;

  if (details->type != GUM_EXCEPTION_ACCESS_VIOLATION)
    return FALSE;
  if (details->memory.address != ctx->page)
    return FALSE;

  ctx->called = TRUE;

  gum_mprotect (ctx->page, gum_query_page_size (), GUM_PAGE_RW);

  return TRUE;
}

#ifdef HAVE_DARWIN

static gpointer
simulate_crash_handler (gpointer user_data)
{
  GumCrashExceptorContext * ctx = user_data;
  GumScriptBackend * backend = ctx->backend;

  gum_process_enumerate_threads (suspend_all_threads, backend,
      GUM_THREAD_FLAGS_NONE);
  gum_process_enumerate_threads (resume_all_threads, backend,
      GUM_THREAD_FLAGS_NONE);

  return NULL;
}

static gboolean
suspend_all_threads (const GumThreadDetails * details,
                     gpointer user_data)
{
#ifndef HAVE_WATCHOS
  GumScriptBackend * backend = user_data;

  if (details->id != gum_process_get_current_thread_id ())
  {
    gum_script_backend_with_lock_held (backend,
        (GumScriptBackendLockedFunc) thread_suspend,
        GSIZE_TO_POINTER (details->id));
  }
#endif

  return TRUE;
}

static gboolean
resume_all_threads (const GumThreadDetails * details,
                    gpointer user_data)
{
#ifndef HAVE_WATCHOS
  if (details->id != gum_process_get_current_thread_id ())
    thread_resume (details->id);
#endif

  return TRUE;
}

#endif /* HAVE_DARWIN */

#endif /* !HAVE_WINDOWS */

TESTCASE (argument_can_be_read)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();

  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");

  target_function_int (-42);
  EXPECT_SEND_MESSAGE_WITH ("-42");
}

TESTCASE (argument_can_be_replaced)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const replacementString = Memory.allocUtf8String('Hei');"
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    args[0] = replacementString;"
      "  }"
      "});", target_function_string);

  EXPECT_NO_MESSAGES ();
  g_assert_cmpstr (target_function_string ("Hello"), ==, "Hei");
  EXPECT_NO_MESSAGES ();
  g_assert_cmpstr (target_function_string ("Hello"), ==, "Hei");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (return_value_can_be_read)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onLeave(retval) {"
      "    send(retval.toInt32());"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_SEND_MESSAGE_WITH ("315");
}

TESTCASE (return_value_can_be_replaced)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onLeave(retval) {"
      "    retval.replace(1337);"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1337);
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onLeave(retval) {"
      "    retval.replace({ handle: ptr(1338) });"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1338);
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "let savedRetval = null;"
      "Interceptor.attach(" GUM_PTR_CONST  ", {"
      "  onLeave(retval) {"
      "    savedRetval = retval;"
      "  }"
      "});"
      "recv('try-replace', () => {"
      "  savedRetval.replace(1337);"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"try-replace\"}");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid operation");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (return_address_can_be_read)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter() {"
      "    send(this.returnAddress instanceof NativePointer);"
      "    this.onEnterReturnAddress = this.returnAddress;"
      "  },"
      "  onLeave() {"
      "    send(this.returnAddress.equals(this.onEnterReturnAddress));"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (general_purpose_register_can_be_read)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onLeave() {"
      "    send(this.context." GUM_RETURN_VALUE_REGISTER_NAME ".toInt32());"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("1890");
}

TESTCASE (general_purpose_register_can_be_written)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onLeave() {"
      "    this.context." GUM_RETURN_VALUE_REGISTER_NAME " = ptr(1337);"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (42), ==, 1337);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (vector_register_can_be_read)
{
#if (defined (HAVE_ARM) && defined (__ARM_PCS_VFP)) || \
    (defined (HAVE_ARM64) && G_BYTE_ORDER == G_LITTLE_ENDIAN)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter() {"
      "    const v = new Float64Array(this.context.q0);"
      "    send(v[0]);"
      "  }"
      "});", target_function_double);

  EXPECT_NO_MESSAGES ();
  target_function_double (42.0);
  EXPECT_SEND_MESSAGE_WITH ("42");
#else
  g_print ("<skipping, missing code for current architecture or ABI> ");
#endif
}

TESTCASE (double_register_can_be_read)
{
#if (defined (HAVE_ARM) && defined (__ARM_PCS_VFP)) || \
    (defined (HAVE_ARM64) && G_BYTE_ORDER == G_LITTLE_ENDIAN)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter() {"
      "    send(this.context.d0);"
      "  }"
      "});", target_function_double);

  EXPECT_NO_MESSAGES ();
  target_function_double (42.0);
  EXPECT_SEND_MESSAGE_WITH ("42");
#else
  g_print ("<skipping, missing code for current architecture or ABI> ");
#endif
}

TESTCASE (float_register_can_be_read)
{
#if (defined (HAVE_ARM) && defined (__ARM_PCS_VFP)) || \
    (defined (HAVE_ARM64) && G_BYTE_ORDER == G_LITTLE_ENDIAN)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter() {"
      "    send(this.context.s0);"
      "  }"
      "});", target_function_float);

  EXPECT_NO_MESSAGES ();
  target_function_float (42.0f);
  EXPECT_SEND_MESSAGE_WITH ("42");
#else
  g_print ("<skipping, missing code for current architecture or ABI> ");
#endif
}

TESTCASE (status_register_can_be_read)
{
#if defined (HAVE_ARM)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter() {"
      "    send(typeof this.context.cpsr);"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter() {"
      "    send(typeof this.context.nzcv);"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (system_error_can_be_read_from_interceptor_listener)
{
#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(retval) {"
      "    send(this.lastError);"
      "  }"
      "});", target_function_int);

  SetLastError (13);
  target_function_int (7);
  SetLastError (37);
  target_function_int (7);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(retval) {"
      "    send(this.errno);"
      "  }"
      "});", target_function_int);

  errno = 13;
  target_function_int (7);
  errno = 37;
  target_function_int (7);
#endif
  EXPECT_SEND_MESSAGE_WITH ("13");
  EXPECT_SEND_MESSAGE_WITH ("37");
}

TESTCASE (system_error_can_be_read_from_replacement_function)
{
  GumInterceptor * interceptor;

  interceptor = gum_interceptor_obtain ();

  /* Replacement should be used regardless: */
  gum_interceptor_ignore_current_thread (interceptor);

#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  send(this.lastError);"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

  SetLastError (13);
  target_function_int (7);
  SetLastError (37);
  target_function_int (7);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  send(this.errno);"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

  errno = 13;
  target_function_int (7);
  errno = 37;
  target_function_int (7);
#endif
  EXPECT_SEND_MESSAGE_WITH ("13");
  EXPECT_SEND_MESSAGE_WITH ("37");

  gum_interceptor_unignore_current_thread (interceptor);

  g_object_unref (interceptor);
}

TESTCASE (system_error_can_be_replaced_from_interceptor_listener)
{
#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(retval) {"
      "    this.lastError = 1337;"
      "  }"
      "});", target_function_int);

  SetLastError (42);
  target_function_int (7);
  g_assert_cmpint (GetLastError (), ==, 1337);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(retval) {"
      "    this.errno = 1337;"
      "  }"
      "});", target_function_int);

  errno = 42;
  target_function_int (7);
  g_assert_cmpint (errno, ==, 1337);
#endif
}

TESTCASE (system_error_can_be_replaced_from_replacement_function)
{
#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  this.lastError = 1337;"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

  SetLastError (42);
  target_function_int (7);
  g_assert_cmpint (GetLastError (), ==, 1337);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  this.errno = 1337;"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

  errno = 42;
  target_function_int (7);
  g_assert_cmpint (errno, ==, 1337);
#endif
}

TESTCASE (system_error_unaffected_by_replacement_if_set_to_original_value)
{
#ifdef HAVE_WINDOWS
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  this.lastError = 1337;"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

  SetLastError (1337);
  target_function_int (7);
  g_assert_cmpint (GetLastError (), ==, 1337);
#else
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  this.errno = 1337;"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

  errno = 1337;
  target_function_int (7);
  g_assert_cmpint (errno, ==, 1337);
#endif
}

TESTCASE (system_error_unaffected_by_replacement_if_untouched)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(function (arg) {"
      "  return 0;"
      "}, 'int', ['int']));", target_function_int);

#ifdef HAVE_WINDOWS
  SetLastError (1337);
  target_function_int (7);
  g_assert_cmpint (GetLastError (), ==, 1337);
#else
  errno = 1337;
  target_function_int (7);
  g_assert_cmpint (errno, ==, 1337);
#endif
}

TESTCASE (invocations_are_bound_on_tls_object)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(this.value || null);"
      "    this.value = args[0].toInt32();"
      "  },"
      "  onLeave(retval) {"
      "    send(this.value || null);"
      "  }"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("7");
  target_function_int (11);
  EXPECT_SEND_MESSAGE_WITH ("null");
  EXPECT_SEND_MESSAGE_WITH ("11");
}

TESTCASE (invocations_provide_thread_id)
{
  guint i;

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(this.threadId);"
      "  },"
      "  onLeave(retval) {"
      "    send(this.threadId);"
      "  }"
      "});",
      target_function_int);
  EXPECT_NO_MESSAGES ();

  target_function_int (7);
  for (i = 0; i != 2; i++)
  {
    TestScriptMessageItem * item;
    gint id;

    item = test_script_fixture_pop_message (fixture);
    id = 0;
    sscanf (item->message, "{\"type\":\"send\",\"payload\":%d}", &id);
    g_assert_cmpuint (id, !=, 0);
    test_script_message_item_free (item);
    g_assert_cmpint (id, ==, gum_process_get_current_thread_id ());
  }
}

TESTCASE (invocations_provide_call_depth)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send('>a' + this.depth);"
      "  },"
      "  onLeave(retval) {"
      "    send('<a' + this.depth);"
      "  }"
      "});"
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send('>b' + this.depth);"
      "  },"
      "  onLeave(retval) {"
      "    send('<b' + this.depth);"
      "  }"
      "});"
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send('>c' + this.depth);"
      "  },"
      "  onLeave(retval) {"
      "    send('<c' + this.depth);"
      "  }"
      "});",
      target_function_nested_a,
      target_function_nested_b,
      target_function_nested_c);

  EXPECT_NO_MESSAGES ();
  target_function_nested_a (7);
  EXPECT_SEND_MESSAGE_WITH ("\">a0\"");
  EXPECT_SEND_MESSAGE_WITH ("\">b1\"");
  EXPECT_SEND_MESSAGE_WITH ("\">c2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"<c2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"<b1\"");
  EXPECT_SEND_MESSAGE_WITH ("\"<a0\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (invocations_provide_context_for_backtrace)
{
  if (RUNNING_ON_VALGRIND)
  {
    g_print ("<skipping, not compatible with Valgrind> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "const mode = '%s';"
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(Thread.backtrace(this.context, Backtracer.ACCURATE)"
      "        .length > 0);"
      "  },"
      "  onLeave(retval) {"
      "    if (mode === 'slow')"
      "      send(Thread.backtrace(this.context, Backtracer.FUZZY).length > 0);"
      "  }"
      "});",
      g_test_slow () ? "slow" : "fast",
      target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_SEND_MESSAGE_WITH ("true");
  if (g_test_slow ())
    EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (invocations_provide_context_serializable_to_json)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(JSON.stringify(this.context) !== \"{}\");"
      "  },"
      "  onLeave(retval) {"
      "    send(JSON.stringify(this.context) !== \"{}\");"
      "  }"
      "});",
      target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (listener_can_be_detached)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const firstListener = Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(1);"
      "    firstListener.detach();"
      "  },"
      "  onLeave(retval) {"
      "    send(2);"
      "  }"
      "});"
      ""
      "const secondListener = Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(3);"
      "  },"
      "  onLeave(retval) {"
      "    send(4);"
      "    secondListener.detach();"
      "  }"
      "});",
      target_function_int, target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_SEND_MESSAGE_WITH ("4");
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (listener_can_be_detached_by_destruction_mid_call)
{
  const guint repeats = 10;
  guint i;
  TestTrigger trigger;

  g_mutex_init (&trigger.mutex);
  g_cond_init (&trigger.cond);

  for (i = 0; i != repeats; i++)
  {
    GThread * invoker_thread;

    g_mutex_lock (&trigger.mutex);
    trigger.ready = FALSE;
    trigger.fired = FALSE;
    g_mutex_unlock (&trigger.mutex);

    COMPILE_AND_LOAD_SCRIPT (
        "Interceptor.attach(" GUM_PTR_CONST ", {"
        "  onEnter(args) {"
        "  },"
        "  onLeave(retval) {"
        "  }"
        "});",
        target_function_trigger);

    invoker_thread = g_thread_new ("script-invoker-thread",
        invoke_target_function_trigger, &trigger);

    g_mutex_lock (&trigger.mutex);
    while (!trigger.ready)
      g_cond_wait (&trigger.cond, &trigger.mutex);
    g_mutex_unlock (&trigger.mutex);

    g_mutex_lock (&trigger.mutex);
    trigger.fired = TRUE;
    g_cond_signal (&trigger.cond);
    g_mutex_unlock (&trigger.mutex);

    UNLOAD_SCRIPT ();

    g_thread_join (invoker_thread);
  }

  g_cond_clear (&trigger.cond);
  g_mutex_clear (&trigger.mutex);
}

static gpointer
invoke_target_function_trigger (gpointer data)
{
  TestTrigger * trigger = (TestTrigger *) data;

  target_function_trigger (trigger);

  return NULL;
}

TESTCASE (all_listeners_can_be_detached)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});"
      "Interceptor.detachAll();",
      target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_replaced)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ","
      "    new NativeCallback(arg => {"
      "  send(arg);"
      "  return 1337;"
      "}, 'int', ['int']));",
      target_function_int);

  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1337);
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_NO_MESSAGES ();

  gum_script_unload_sync (fixture->script, NULL);
  target_function_int (1);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_replaced_and_called_immediately)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const address = " GUM_PTR_CONST ";"
      "Interceptor.replace(address,"
      "    new NativeCallback(arg => {"
      "  send(arg);"
      "  return 1337;"
      "}, 'int', ['int']));"
      "const f = new NativeFunction(address, 'int', ['int'],"
      "    { scheduling: 'exclusive' });"
      "f(7);"
      "Interceptor.flush();"
      "f(8);",
      target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("8");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_reverted)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ", new NativeCallback(arg => {"
      "  send(arg);"
      "  return 1337;"
      "}, 'int', ['int']));"
      "Interceptor.revert(" GUM_PTR_CONST ");",
      target_function_int, target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (replaced_function_should_have_invocation_context)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ", new NativeCallback(function () {"
      "  send(this.returnAddress instanceof NativePointer &&"
      "      !this.context.pc.isNull());"
      "  return 0;"
      "}, 'int', ['int']));",
      target_function_int);

  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (cfg_can_be_built)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const cfg = new ControlFlowGraph(" GUM_PTR_CONST ");"
      "const entry = cfg.entryBlock;"
      "send(entry !== null);"
      "send(cfg.blocks.length > 0);"
      "send(cfg.findBlockContaining(entry.start).start.equals(entry.start));"
      "send(cfg.dominates(entry.start, entry.start));"
      "const insns = entry.instructions;"
      "send(insns.length > 0);"
      "send(insns[0].address.equals(entry.start));"
      "send(Array.isArray(entry.successors));"
      "send(Array.isArray(entry.predecessors));"
      "send(Object.keys(JSON.parse(JSON.stringify(cfg))).length > 0);"
      "send(Object.keys(JSON.parse(JSON.stringify(entry))).length > 0);",
      gum_get_answer_to_life_universe_and_everything);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (cfg_can_be_serialized)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const cfg = new ControlFlowGraph(" GUM_PTR_CONST ");"
      "const json = JSON.parse(JSON.stringify(cfg));"
      "send(Array.isArray(json.blocks) && json.blocks.length > 0);"
      "const edges = json.blocks.flatMap(b =>"
      "    [...b.successors, ...b.predecessors]);"
      "send(edges.every(e => typeof e === 'string'));",
      gum_toupper);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (cfg_dominating_sites_can_be_enumerated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const cfg = new ControlFlowGraph(" GUM_PTR_CONST ");"
      "const entry = cfg.entryBlock;"
      "const sites = cfg.enumerateDominatingSites(entry.start);"
      "send(sites.length > 0);"
      "send(sites[0].address.equals(entry.start));"
      "send(typeof sites[0].capacity === 'number');",
      gum_get_answer_to_life_universe_and_everything);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
}

TESTCASE (instructions_can_be_probed)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", function () {"
      "  send(!!this.context);"
      "});", target_function_int);

  EXPECT_NO_MESSAGES ();

  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (interceptor_should_support_native_pointer_values)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const value = { handle: " GUM_PTR_CONST " };"
      "Interceptor.attach(value, {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");

  COMPILE_AND_LOAD_SCRIPT (
      "const value = { handle: " GUM_PTR_CONST " };"
      "Interceptor.replace(value,"
      "    new NativeCallback(arg => 1337, 'int', ['int']));",
      target_function_int);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1337);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (interceptor_should_support_instrumentation_options)
{
#if defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach({"
      "  target: " GUM_PTR_CONST ","
      "  scratchRegister: 'x15',"
      "  scenario: 'online',"
      "  relocation: 'checked'"
      "}, {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach("
      "    { target: " GUM_PTR_CONST ", scratchRegister: 'x99' },"
      "    () => {});",
      target_function_int);
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid arm64 register");

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach("
      "    { target: " GUM_PTR_CONST ", scenario: 'bogus' },"
      "    () => {});",
      target_function_int);
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: invalid value: bogus "
      "(expected one of: default, online, offline)");

  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (interceptor_should_support_custom_redirect)
{
#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach({"
      "  target: " GUM_PTR_CONST ","
      "  writeRedirect(d) {"
      "    send(d.capacity > 0);"
      "    d.writer.putJmpAddress(d.target);"
      "  }"
      "}, {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach({"
      "  target: " GUM_PTR_CONST ","
      "  writeRedirect(d) {"
      "    send(d.capacity > 0);"
      "    d.writer.putLdrRegAddress(d.scratchRegister, d.target);"
      "    d.writer.putBrReg(d.scratchRegister);"
      "  }"
      "}, {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (interceptor_should_support_default_options)
{
#if defined (HAVE_I386)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.defaults = {"
      "  writeRedirect(d) {"
      "    send(d.capacity > 0);"
      "    d.writer.putJmpAddress(d.target);"
      "  }"
      "};"
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
#elif defined (HAVE_ARM64)
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.defaults = {"
      "  writeRedirect(d) {"
      "    send(d.capacity > 0);"
      "    d.writer.putLdrRegAddress(d.scratchRegister, d.target);"
      "    d.writer.putBrReg(d.scratchRegister);"
      "  }"
      "};"
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "    send(args[0].toInt32());"
      "  }"
      "});", target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
  target_function_int (42);
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
#else
  g_print ("<skipping, missing code for current architecture> ");
#endif
}

TESTCASE (interceptor_should_handle_bad_pointers)
{
  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(ptr(0x42), {"
      "  onEnter(args) {"
      "  }"
      "});");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: access violation accessing 0x42");

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(ptr(0x42),"
      "    new NativeCallback(() => {}, 'void', []));");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: access violation accessing 0x42");
}

TESTCASE (interceptor_should_refuse_to_attach_without_any_callbacks)
{
  COMPILE_AND_LOAD_SCRIPT ("Interceptor.attach(" GUM_PTR_CONST ", {});",
      target_function_int);
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: expected at least one callback");
}

#ifdef HAVE_DARWIN

TESTCASE (interceptor_and_js_should_not_deadlock)
{
  GThread * worker_thread;
  int state = 0;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  worker_thread = g_thread_new ("script-test-worker-thread",
      interceptor_attacher_worker, &state);
  while (state == 0)
    g_usleep (G_USEC_PER_SEC / 200);

  COMPILE_AND_LOAD_SCRIPT (
      "const iterations = 100;"
      "send('Start loop');"
      "const threadSuspend = new NativeFunction("
      "  Module.getGlobalExportByName('thread_suspend'),"
      "  'int', ['int'], { scheduling: 'exclusive' }"
      ");"
      "Interceptor.replace(threadSuspend, new NativeCallback((threadId) => {"
      "  return threadSuspend(threadId);"
      "}, 'int', ['int']));"
      "Interceptor.flush();"
      "setTimeout(() => {"
      "  for (let i = 0; i !== iterations; i++)"
      "    Thread.sleep(0.1);"
      "  Interceptor.revert(threadSuspend);"
      "  send('The end');"
      "}, 0);");

  EXPECT_SEND_MESSAGE_WITH ("\"Start loop\"");

  g_usleep (G_USEC_PER_SEC / 25);
  g_thread_join (worker_thread);
  g_assert_cmpint (state, ==, 2);
  EXPECT_SEND_MESSAGE_WITH ("\"The end\"");
  EXPECT_NO_MESSAGES ();
}

static gpointer
interceptor_attacher_worker (gpointer data)
{
  int * state = data;
  guint i;
  GumInterceptor * interceptor;
  GumInvocationListener * listener;
  GumAttachReturn result;

  *state = 1;

  interceptor = gum_interceptor_obtain ();
  listener = gum_make_call_listener (empty_invocation_callback,
      empty_invocation_callback, NULL, NULL);

  for (i = 0; i != 300; i++)
  {
    result = gum_interceptor_attach (interceptor, target_function_int,
        GUM_INVOCATION_LISTENER (listener), NULL);
    if (result == GUM_ATTACH_OK)
    {
      g_usleep (G_USEC_PER_SEC / 25);
      gum_interceptor_detach (interceptor, GUM_INVOCATION_LISTENER (listener));
    }
  }

  g_object_unref (listener);
  g_object_unref (interceptor);

  *state = 2;

  return NULL;
}

static void
empty_invocation_callback (GumInvocationContext * context,
                           gpointer user_data)
{
}

#endif

TESTCASE (function_can_be_replaced_fast)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replaceFast(" GUM_PTR_CONST ", new NativeCallback(arg => {"
      "  send(arg);"
      "  return 1337;"
      "}, 'int', ['int']));",
      target_function_int);

  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1337);
  EXPECT_SEND_MESSAGE_WITH ("7");
  EXPECT_NO_MESSAGES ();

  gum_script_unload_sync (fixture->script, NULL);
  target_function_int (1);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_replaced_fast_and_called_immediately)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const address = " GUM_PTR_CONST ";"
      "Interceptor.replaceFast(address, new NativeCallback(arg => {"
      "  send(arg);"
      "  return 1337;"
      "}, 'int', ['int']));"
      "const f = new NativeFunction(address, 'int', ['int'],"
      "    { scheduling: 'exclusive' });"
      "f(7);"
      "Interceptor.flush();"
      "f(8);",
      target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("8");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_reverted_fast)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replaceFast(" GUM_PTR_CONST ", new NativeCallback(arg => {"
      "  send(arg);"
      "  return 1337;"
      "}, 'int', ['int']));"
      "Interceptor.revert(" GUM_PTR_CONST ");",
      target_function_int,
      target_function_int);
  EXPECT_NO_MESSAGES ();
  target_function_int (7);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (interceptor_should_support_native_pointer_values_fast)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const value = { handle: " GUM_PTR_CONST " };"
      "Interceptor.replaceFast(value,"
      "    new NativeCallback(arg => 1337, 'int', ['int']));",
      target_function_int);
  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1337);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (interceptor_should_handle_bad_pointers_fast)
{
  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replaceFast(ptr(0x42),"
      "    new NativeCallback(() => {}, 'void', []));");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: access violation accessing 0x42");
}

TESTCASE (function_can_be_replaced_and_call_original_fast)
{
  int ret = target_function_int (1);

  COMPILE_AND_LOAD_SCRIPT (
      "let func;"
      "const addr = Interceptor.replaceFast(" GUM_PTR_CONST ","
      "    new NativeCallback(arg => func(arg) + 1, 'int', ['int']));"
      "func = new NativeFunction(addr, 'int', ['int']);",
      target_function_int);
  EXPECT_NO_MESSAGES ();

  g_assert_cmpint (target_function_int (1), ==, ret + 1);
  EXPECT_NO_MESSAGES ();
}

TESTCASE (function_can_be_replaced_fast_performance)
{
  GTimer * timer;
  gdouble duration_default, duration_fast;
  guint i;

  target_function_original = NULL;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  timer = g_timer_new ();

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replace(" GUM_PTR_CONST ", " GUM_PTR_CONST ");",
      target_function_int,
      target_function_int_replacement);

  g_timer_reset (timer);
  for (i = 0; i != 1000000; i++)
    g_assert_cmpint (target_function_int (7), ==, 1337);
  duration_default = g_timer_elapsed (timer, NULL);

  gum_script_unload_sync (fixture->script, NULL);

  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.replaceFast(" GUM_PTR_CONST ", " GUM_PTR_CONST ");",
      target_function_int,
      target_function_int_replacement);

  g_timer_reset (timer);
  for (i = 0; i != 1000000; i++)
    g_assert_cmpint (target_function_int (7), ==, 1337);
  duration_fast = g_timer_elapsed (timer, NULL);

  g_timer_destroy (timer);

  g_print ("<duration_fast=%f duration_default=%f ratio=%f> ",
      duration_fast,
      duration_default,
      duration_fast / duration_default);
}

TESTCASE (function_can_be_replaced_and_call_original_fast_performance)
{
  GTimer * timer;
  gdouble duration_default, duration_fast;
  guint i;

  target_function_original = NULL;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  timer = g_timer_new ();

  COMPILE_AND_LOAD_SCRIPT (
      "const origPtr = ptr(" GUM_PTR_CONST ");"
      "const orig = ptr(" GUM_PTR_CONST ");"
      "Interceptor.replace(orig, " GUM_PTR_CONST ");"
      "origPtr.writePointer(orig);",
      &target_function_original,
      target_function_int,
      target_function_int_replacement);

  g_timer_reset (timer);
  for (i = 0; i != 1000000; i++)
    g_assert_cmpint (target_function_int (7), ==, 1652);
  duration_default = g_timer_elapsed (timer, NULL);

  gum_script_unload_sync (fixture->script, NULL);

  COMPILE_AND_LOAD_SCRIPT (
      "const origPtr = ptr(" GUM_PTR_CONST ");"
      "const orig = Interceptor.replaceFast(" GUM_PTR_CONST ", " GUM_PTR_CONST
        ");"
      "origPtr.writePointer(orig);",
      &target_function_original,
      target_function_int,
      target_function_int_replacement);

  g_timer_reset (timer);
  for (i = 0; i != 1000000; i++)
    g_assert_cmpint (target_function_int (7), ==, 1652);
  duration_fast = g_timer_elapsed (timer, NULL);

  g_timer_destroy (timer);

  g_print ("<duration_fast=%f duration_default=%f ratio=%f> ",
      duration_fast,
      duration_default,
      duration_fast / duration_default);
}

GUM_HOOK_TARGET static int
target_function_int_replacement (int arg)
{
  if (target_function_original == NULL)
    return 1337;
  else
    return 1337 + target_function_original (arg);
}

TESTCASE (interceptor_on_enter_performance)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "  }"
      "});", target_function_int);

#if 1
  measure_target_function_int_overhead ();
#else
  while (TRUE)
    target_function_int (7);
#endif
}

TESTCASE (interceptor_on_leave_performance)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onLeave(retval) {"
      "  }"
      "});", target_function_int);

#if 1
  measure_target_function_int_overhead ();
#else
  while (TRUE)
    target_function_int (7);
#endif
}

TESTCASE (interceptor_on_enter_and_leave_performance)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Interceptor.attach(" GUM_PTR_CONST ", {"
      "  onEnter(args) {"
      "  },"
      "  onLeave(retval) {"
      "  }"
      "});", target_function_int);

#if 1
  measure_target_function_int_overhead ();
#else
  while (TRUE)
    target_function_int (7);
#endif
}

static void
measure_target_function_int_overhead (void)
{
  GTimer * timer;
  guint i, n;
  gdouble measurement[1000], t_min, t_max, t_median;

  n = G_N_ELEMENTS (measurement);

  timer = g_timer_new ();

  for (i = 0; i != n; i++)
  {
    target_function_int (7);
  }

  for (i = 0; i != n; i++)
  {
    g_timer_reset (timer);
    target_function_int (7);
    measurement[i] = g_timer_elapsed (timer, NULL);
  }

  qsort (measurement, n, sizeof (gdouble), compare_measurements);

  t_min = measurement[0];
  t_max = measurement[n - 1];
  g_assert (n % 2 == 0);
  t_median = (measurement[n / 2] + measurement[(n / 2) - 1]) / 2.0;

  g_print ("<min: %.1f us, max: %.1f us, median: %.1f us> ",
      t_min * (gdouble) G_USEC_PER_SEC,
      t_max * (gdouble) G_USEC_PER_SEC,
      t_median * (gdouble) G_USEC_PER_SEC);

  g_timer_destroy (timer);
}

static int
compare_measurements (gconstpointer element_a,
                      gconstpointer element_b)
{
  const gdouble a = *(const gdouble *) element_a;
  const gdouble b = *(const gdouble *) element_b;

  if (a > b)
    return 1;

  if (a < b)
    return -1;

  return 0;
}

TESTCASE (memory_can_be_scanned_with_pattern_string)
{
  guint8 haystack1[] = { 0x01, 0x02, 0x13, 0x37, 0x03, 0x13, 0x37 };
  gchar haystack2[] = "Hello world, hello world, I said.";

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(" GUM_PTR_CONST ", 7, '13 37', {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack1, haystack1);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(" GUM_PTR_CONST ", uint64(7), '13 37', {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack1, haystack1);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");

  COMPILE_AND_LOAD_SCRIPT (
      "const regex = /[Hh]ello\\sworld/.toString();"
      "Memory.scan(" GUM_PTR_CONST ", 33, regex, {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack2, haystack2);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=0 size=11\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=13 size=11\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");
}

TESTCASE (memory_can_be_scanned_with_match_pattern_object)
{
  guint8 haystack1[] = { 0x01, 0x02, 0x13, 0x37, 0x03, 0x13, 0x37 };
  gchar haystack2[] = "Hello world, hello world, I said.";

  COMPILE_AND_LOAD_SCRIPT (
      "const pattern = new MatchPattern('13 37');"
      "Memory.scan(" GUM_PTR_CONST ", 7, pattern, {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack1, haystack1);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");

  COMPILE_AND_LOAD_SCRIPT (
      "const pattern = new MatchPattern('13 37');"
      "Memory.scan(" GUM_PTR_CONST ", uint64(7), pattern, {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack1, haystack1);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");

  COMPILE_AND_LOAD_SCRIPT (
      "const pattern = new MatchPattern(/[Hh]ello\\sworld/.toString());"
      "Memory.scan(" GUM_PTR_CONST ", 33, pattern, {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack2, haystack2);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=0 size=11\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=13 size=11\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");

  haystack2[7] = 0xd1;

  COMPILE_AND_LOAD_SCRIPT (
      "const pattern = new MatchPattern(/Hello/.toString());"
      "Memory.scan(" GUM_PTR_CONST ", 33, pattern, {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack2, haystack2);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=0 size=5\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");
}

TESTCASE (memory_can_be_scanned_synchronously)
{
  guint8 haystack[] = { 0x01, 0x02, 0x13, 0x37, 0x03, 0x13, 0x37 };

  COMPILE_AND_LOAD_SCRIPT (
      "for (const match of Memory.scanSync(" GUM_PTR_CONST ", 7, '13 37')) {"
      "  send(`match offset=${match.address.sub(" GUM_PTR_CONST ").toInt32()} "
          "size=${match.size}`);"
      "}"
      "send('done');",
      haystack, haystack);
  EXPECT_SEND_MESSAGE_WITH ("\"match offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"match offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");

  COMPILE_AND_LOAD_SCRIPT (
      "for (const match of Memory.scanSync(" GUM_PTR_CONST ", uint64(7), "
          "'13 37')) {"
      "  send(`match offset=${match.address.sub(" GUM_PTR_CONST ").toInt32()} "
          "size=${match.size}`);"
      "}"
      "send('done');",
      haystack, haystack);
  EXPECT_SEND_MESSAGE_WITH ("\"match offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"match offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
}

TESTCASE (memory_can_be_scanned_asynchronously)
{
  guint8 haystack[] = { 0x01, 0x02, 0x13, 0x37, 0x03, 0x13, 0x37 };

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(" GUM_PTR_CONST ", 7, '13 37', {"
      "  onMatch(address, size) {"
      "    send('onMatch offset=' + address.sub(" GUM_PTR_CONST ").toInt32()"
      "      + ' size=' + size);"
      "  }"
      "})"
      ".catch(e => console.error(e.message))"
      ".then(() => send('DONE'));", haystack, haystack);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=5 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"DONE\"");

  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "async function run() {"
      "  try {"
      "    await Memory.scan(ptr(0xdead), 7, '13 37', {"
      "      onMatch(address, size) {}"
      "    });"
      "  } catch (e) {"
      "    send(e.message);"
      "  }"
      "}"
      "run();"
  );
  EXPECT_SEND_MESSAGE_WITH ("\"access violation accessing 0xdead\"");
}

TESTCASE (memory_scan_should_be_interruptible)
{
  guint8 haystack[] = { 0x01, 0x02, 0x13, 0x37, 0x03, 0x13, 0x37 };
  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(" GUM_PTR_CONST ", 7, '13 37', {"
        "onMatch(address, size) {"
        "  send('onMatch offset=' + address.sub(" GUM_PTR_CONST
             ").toInt32() + ' size=' + size);"
        "  return 'stop';"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});", haystack, haystack);
  EXPECT_SEND_MESSAGE_WITH ("\"onMatch offset=2 size=2\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");
}

TESTCASE (memory_scan_handles_unreadable_memory)
{
  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(ptr(\"1328\"), 7, '13 37', {"
        "onMatch(address, size) {"
        "  send('onMatch');"
        "},"
        "onError(message) {"
        "  send('onError: ' + message);"
        "},"
        "onComplete() {"
        "  send('onComplete');"
        "}"
      "});");
  EXPECT_SEND_MESSAGE_WITH ("\"onError: access violation accessing 0x530\"");
  EXPECT_SEND_MESSAGE_WITH ("\"onComplete\"");

  COMPILE_AND_LOAD_SCRIPT (
      "try {"
        "Memory.scanSync(ptr(\"1328\"), 7, '13 37');"
      "} catch (e) {"
        "send(e.message);"
      "}");
  EXPECT_SEND_MESSAGE_WITH ("\"access violation accessing 0x530\"");
}

TESTCASE (memory_scan_handles_bad_arguments)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(0x1337, 7, '13 37', {"
      "  onMatch(address, size) {}, onComplete() {}"
      "});");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: expected a pointer");

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(ptr(0x1337), -7, '13 37', {"
      "  onMatch(address, size) {}, onComplete() {}"
      "});");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: expected an unsigned integer");

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(ptr(0x1337), 7, 0xbadcafe, {"
      "  onMatch(address, size) {},"
      "  onComplete() {}"
      "});");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: expected either a pattern string or a MatchPattern object");

  COMPILE_AND_LOAD_SCRIPT (
    "Memory.scan(ptr(0x1337), 7, 'bad pattern', {"
    "  onMatch(addres, size) {}"
    "});"
  );
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid match pattern");

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.scan(ptr(0x1337), 7, '13 37', { onComplete() {} });"
  );
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: expected a callback value");
}

TESTCASE (memory_pointers_can_be_found)
{
  gsize haystack[] = { 0x1111, 0xdeadbeef, 0x2222, 0xdeadbeef };

  COMPILE_AND_LOAD_SCRIPT (
      "const matches = Memory.findPointers("
      "    [{ base: " GUM_PTR_CONST ", size: %" G_GSIZE_FORMAT " }],"
      "    [ptr(0xdeadbeef)]);"
      "send(matches.length);"
      "for (const m of matches) {"
      "  const index = m.address.sub(" GUM_PTR_CONST ").toInt32() /"
      "      Process.pointerSize;"
      "  send(`${index} ${m.value}`);"
      "}"
      "send('done');",
      haystack, sizeof (haystack), haystack);
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("\"1 0xdeadbeef\"");
  EXPECT_SEND_MESSAGE_WITH ("\"3 0xdeadbeef\"");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");

  COMPILE_AND_LOAD_SCRIPT (
      "const matches = Memory.findPointers("
      "    { base: " GUM_PTR_CONST ", size: %" G_GSIZE_FORMAT " },"
      "    [ptr(0xbeef)],"
      "    { mask: ptr(0xffff) });"
      "send(matches.length);"
      "send('done');",
      haystack, sizeof (haystack));
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
}

TESTCASE (memory_access_can_be_monitored)
{
  volatile guint8 * a, * b;
  guint page_size;

  if (!check_exception_handling_testable ())
    return;

  page_size = gum_query_page_size ();
  a = gum_memory_allocate (NULL, 2 * page_size, page_size, GUM_PAGE_RW);
  b = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  COMPILE_AND_LOAD_SCRIPT (
      "MemoryAccessMonitor.enable([{ base: " GUM_PTR_CONST ", size: %u },"
        "{ base: " GUM_PTR_CONST ", size: %u }], {"
        "onAccess(details) {"
          "send([details.operation, !!details.from, details.address,"
            "details.rangeIndex, details.pageIndex, details.pagesCompleted,"
            "details.pagesTotal]);"
        "}"
      "});",
      a + page_size, page_size, b, page_size);
  EXPECT_NO_MESSAGES ();

  a[0] = 1;
  a[page_size - 1] = 2;
  EXPECT_NO_MESSAGES ();

  a[page_size] = 3;
  EXPECT_SEND_MESSAGE_WITH ("[\"write\",true,\"0x%" G_GSIZE_MODIFIER "x\","
      "0,0,1,2]", GPOINTER_TO_SIZE (a + page_size));

  a[0] = b[page_size - 1];
  EXPECT_SEND_MESSAGE_WITH ("[\"read\",true,\"0x%" G_GSIZE_MODIFIER "x\","
      "1,0,2,2]", GPOINTER_TO_SIZE (b + page_size - 1));

  gum_memory_free ((gpointer) b, page_size);
  gum_memory_free ((gpointer) a, 2 * page_size);
}

TESTCASE (memory_access_can_be_monitored_one_range)
{
  volatile guint8 * a;
  guint page_size;

  if (!check_exception_handling_testable ())
    return;

  page_size = gum_query_page_size ();
  a = gum_memory_allocate (NULL, 2 * page_size, page_size, GUM_PAGE_RW);

  COMPILE_AND_LOAD_SCRIPT (
      "MemoryAccessMonitor.enable({ base: " GUM_PTR_CONST ", size: %u }, {"
        "onAccess(details) {"
          "send([details.operation, !!details.from, details.address,"
            "details.rangeIndex, details.pageIndex, details.pagesCompleted,"
            "details.pagesTotal]);"
        "}"
      "});",
      a + page_size, page_size);
  EXPECT_NO_MESSAGES ();

  a[0] = 1;
  a[page_size - 1] = 2;
  EXPECT_NO_MESSAGES ();

  a[page_size] = 3;
  EXPECT_SEND_MESSAGE_WITH ("[\"write\",true,\"0x%" G_GSIZE_MODIFIER "x\","
      "0,0,1,1]", GPOINTER_TO_SIZE (a + page_size));

  gum_memory_free ((gpointer) a, 2 * page_size);
}

TESTCASE (memory_access_monitor_provides_thread_id)
{
  volatile guint8 * a;
  guint page_size;

  if (!check_exception_handling_testable ())
    return;

  page_size = gum_query_page_size ();
  a = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  COMPILE_AND_LOAD_SCRIPT (
      "MemoryAccessMonitor.enable({ base: " GUM_PTR_CONST ", size: %u }, {"
        "onAccess(details) {"
          "send([details.threadId]);"
        "}"
      "});",
      a, page_size);
  EXPECT_NO_MESSAGES ();

  a[0] = 1;
  EXPECT_SEND_MESSAGE_WITH ("[%" G_GSIZE_MODIFIER "u]",
      gum_process_get_current_thread_id ());

  gum_memory_free ((gpointer) a, page_size);
}

TESTCASE (memory_access_monitor_provides_cpu_context)
{
  volatile guint8 * a;
  guint page_size;

  if (!check_exception_handling_testable ())
    return;

  page_size = gum_query_page_size ();
  a = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  COMPILE_AND_LOAD_SCRIPT (
      "MemoryAccessMonitor.enable({ base: " GUM_PTR_CONST ", size: %u }, {"
        "onAccess(details) {"
          "send([details.context.pc.equals(details.from),"
            "details.context.sp.isNull()]);"
        "}"
      "});",
      a, page_size);
  EXPECT_NO_MESSAGES ();

  a[0] = 1;
  EXPECT_SEND_MESSAGE_WITH ("[true,false]");

  gum_memory_free ((gpointer) a, page_size);
}

TESTCASE (memory_access_monitor_cpu_context_can_be_modified)
{
  volatile guint8 * a;
  guint page_size;
  gsize (* func) (void);
  gchar * set_return_value_code;
  const gsize expected_ret = 0xdeadface;
  gsize ret;

  if (!check_exception_handling_testable ())
    return;

  page_size = gum_query_page_size ();
  a = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RX);

  gum_memory_patch_code ((gpointer) a, 4, put_return_instruction, NULL);
  func = (gsize (*) (void)) gum_sign_code_pointer ((gpointer) a);

  set_return_value_code = g_strdup_printf (
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
      "details.context.eax = " GUM_PTR_CONST ";"
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
      "details.context.rax = " GUM_PTR_CONST ";"
#elif defined (HAVE_ARM)
      "details.context.r0 = " GUM_PTR_CONST ";"
#elif defined (HAVE_ARM64)
      "details.context.x0 = " GUM_PTR_CONST ";"
#elif defined (HAVE_MIPS)
      "details.context.ra = " GUM_PTR_CONST ";"
#endif
      , expected_ret);

  COMPILE_AND_LOAD_SCRIPT (
      "MemoryAccessMonitor.enable({ base: " GUM_PTR_CONST ", size: %u }, {"
        "onAccess(details) {"
            "%s"
            "send(1337);"
        "}"
      "});",
      a, page_size, set_return_value_code);
  EXPECT_NO_MESSAGES ();

  ret = func ();
  EXPECT_SEND_MESSAGE_WITH ("1337");
  g_assert_cmphex (ret, ==, expected_ret);

  g_free (set_return_value_code);
  gum_memory_free ((gpointer) a, page_size);
}

static void
put_return_instruction (gpointer mem,
                        gpointer user_data)
{
#if defined (HAVE_I386)
  *((guint8 *) mem) = 0xc3;
#elif defined (HAVE_ARM)
  *((guint32 *) mem) = GUINT32_TO_LE (0xe1a0f00eU);
#elif defined (HAVE_ARM64)
  *((guint32 *) mem) = GUINT32_TO_LE (0xd65f03c0U);
#elif defined (HAVE_MIPS)
  guint32 * code = mem;
  code[0] = 0x03e00008U;
  code[1] = 0x00000000U;
#else
# error Unsupported architecture
#endif
}

TESTCASE (pointer_can_be_read)
{
  gpointer val = GSIZE_TO_POINTER (0x1337000);
  COMPILE_AND_LOAD_SCRIPT (
      "send(" GUM_PTR_CONST ".readPointer().toString());", &val);
  EXPECT_SEND_MESSAGE_WITH ("\"0x1337000\"");
}

TESTCASE (pointer_can_be_written)
{
  gpointer vals[2] = { NULL, NULL };
  COMPILE_AND_LOAD_SCRIPT (
      GUM_PTR_CONST ".writePointer(ptr(\"0x1337000\"))"
      ".add(Process.pointerSize).writePointer(ptr(\"0x1338000\"))",
      vals);
  g_assert_cmphex (GPOINTER_TO_SIZE (vals[0]), ==, 0x1337000);
  g_assert_cmphex (GPOINTER_TO_SIZE (vals[1]), ==, 0x1338000);
}

TESTCASE (memory_can_be_allocated_with_byte_granularity)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.alloc(8);"
      "p.writePointer(ptr('1337'));"
      "send(p.readPointer().toInt32() === 1337);");
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.alloc(uint64(8));"
      "p.writePointer(ptr('1337'));"
      "send(p.readPointer().toInt32() === 1337);");
  EXPECT_SEND_MESSAGE_WITH ("true");

  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.alloc(5);"
      "send('p', p.readByteArray(5));");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"p\"", "00 00 00 00 00");
}

TESTCASE (memory_can_be_allocated_with_page_granularity)
{
  gsize p;

  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.alloc(Process.pageSize);"
      "send(p);");
  p = GPOINTER_TO_SIZE (EXPECT_SEND_MESSAGE_WITH_POINTER ());
  g_assert_cmpuint (p, !=, 0);
  g_assert_cmpuint (p & (gum_query_page_size () - 1), ==, 0);

  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.alloc(5);"
      "send('p', p.readByteArray(5));");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"p\"", "00 00 00 00 00");
}

TESTCASE (memory_can_be_allocated_near_address)
{
  gsize p;

  COMPILE_AND_LOAD_SCRIPT (
      "const maxDistance = uint64(NULL.sub(1).toString());"
      "const a = Memory.alloc(Process.pageSize);"
      "const b = Memory.alloc(Process.pageSize, { near: a, maxDistance });"
      "send(b);");
  p = GPOINTER_TO_SIZE (EXPECT_SEND_MESSAGE_WITH_POINTER ());
  g_assert_cmpuint (p, !=, 0);
  g_assert_cmpuint (p & (gum_query_page_size () - 1), ==, 0);

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.alloc(Process.pageSize - 1, { "
          "near: ptr(Process.pageSize), "
          "maxDistance: 12345678 "
      "});");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: size must be a multiple of page size");

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.alloc(Process.pageSize, { near: ptr(Process.pageSize) });");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: missing maxDistance option");
}

TESTCASE (memory_can_be_copied)
{
  const gchar * from = "Hei";
  gchar to[5] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.copy(" GUM_PTR_CONST ", " GUM_PTR_CONST ", 3);", to, from);
  g_assert_cmphex (to[0], ==, 'H');
  g_assert_cmphex (to[1], ==, 'e');
  g_assert_cmphex (to[2], ==, 'i');
  g_assert_cmphex (to[3], ==, 0x04);
  g_assert_cmphex (to[4], ==, 0x05);

  COMPILE_AND_LOAD_SCRIPT (
      "Memory.copy(" GUM_PTR_CONST ".add(3), " GUM_PTR_CONST ", uint64(2));",
      to, from);
  g_assert_cmphex (to[0], ==, 'H');
  g_assert_cmphex (to[1], ==, 'e');
  g_assert_cmphex (to[2], ==, 'i');
  g_assert_cmphex (to[3], ==, 'H');
  g_assert_cmphex (to[4], ==, 'e');

  /* TODO: investigate */
#if !(defined (HAVE_LINUX) && defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4)
  if (!gum_process_is_debugger_attached () && !RUNNING_ON_VALGRIND)
  {
    COMPILE_AND_LOAD_SCRIPT (
        "Memory.copy(" GUM_PTR_CONST ", ptr(\"1337\"), 1);", to);
    EXPECT_ERROR_MESSAGE_WITH (1, "Error: access violation accessing 0x539");
  }
#endif
}

TESTCASE (memory_can_be_duped)
{
  guint8 buf[3] = { 0x13, 0x37, 0x42 };

  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.dup(" GUM_PTR_CONST ", 3);"
      "p.writeU8(0x12);"
      "send('p', p.readByteArray(3));"
      "send('buf', " GUM_PTR_CONST ".readByteArray(3));",
      buf, buf);
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"p\"", "12 37 42");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"buf\"", "13 37 42");

  COMPILE_AND_LOAD_SCRIPT (
      "const p = Memory.dup(" GUM_PTR_CONST ", uint64(2));"
      "p.writeU8(0x12);"
      "send('p', p.readByteArray(2));"
      "send('buf', " GUM_PTR_CONST ".readByteArray(2));",
      buf, buf);
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"p\"", "12 37");
  EXPECT_SEND_MESSAGE_WITH_PAYLOAD_AND_DATA ("\"buf\"", "13 37");
}

TESTCASE (memory_can_be_protected)
{
  gsize page_size;
  gpointer buf;
  gboolean exception_on_read, exception_on_write;

  page_size = gum_query_page_size ();
  buf = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  COMPILE_AND_LOAD_SCRIPT (
      "send(Memory.protect(" GUM_PTR_CONST ", 1, 'r--'));",
      buf, page_size);
  EXPECT_SEND_MESSAGE_WITH ("true");

  if (gum_process_is_debugger_attached ())
  {
    g_print ("<only partially tested, debugger is attached> ");

    gum_memory_free (buf, page_size);

    return;
  }

  /* avoid overlapping signal handlers */
  UNLOAD_SCRIPT ();

  gum_try_read_and_write_at (buf, 0, &exception_on_read, &exception_on_write);
  g_assert_false (exception_on_read);
  g_assert_true (exception_on_write);

  COMPILE_AND_LOAD_SCRIPT (
      "send(Memory.protect(" GUM_PTR_CONST ", uint64(1), '---'));",
      buf, page_size);
  EXPECT_SEND_MESSAGE_WITH ("true");

  /* avoid overlapping signal handlers */
  UNLOAD_SCRIPT ();

  gum_try_read_and_write_at (buf, 0, &exception_on_read, &exception_on_write);
  g_assert_true (exception_on_read);
  g_assert_true (exception_on_write);

  gum_memory_free (buf, page_size);
}

TESTCASE (memory_protection_can_be_queried)
{
  gsize page_size;
  gpointer buf;

  page_size = gum_query_page_size ();
  buf = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  COMPILE_AND_LOAD_SCRIPT (
      "const x = " GUM_PTR_CONST ";"
      "send(Memory.queryProtection(x) === 'rw-');"
      "Memory.protect(x, 1, 'r--');"
      "send(Memory.queryProtection(x) === 'r--');"
      "Memory.protect(x, 1, 'r-x');"
      "send(Memory.queryProtection(x) === 'r-x');"
      "Memory.protect(x, 1, 'rw-');"
      "send(Memory.queryProtection(x) === 'rw-');",
      buf);

  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");

  gum_memory_free (buf, page_size);
}

TESTCASE (code_can_be_patched)
{
  gsize page_size;
  guint8 * code;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  code[7] = 0xc3;
  gum_mprotect (code, page_size, GUM_PAGE_RX);

  COMPILE_AND_LOAD_SCRIPT ("Memory.patchCode(" GUM_PTR_CONST ", 1, ptr => {"
          "ptr.writeU8(0x90);"
      "});", code + 7);
  g_assert_cmphex (code[7], ==, 0x90);

  gum_memory_free (code, page_size);
}

TESTCASE (utf8_string_can_be_allocated)
{
  COMPILE_AND_LOAD_SCRIPT ("send("
      "Memory.allocUtf8String('Bjørheimsbygd').readUtf8String()"
      ");");
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");
}

TESTCASE (utf16_string_can_be_allocated)
{
  COMPILE_AND_LOAD_SCRIPT ("send("
      "Memory.allocUtf16String('Bjørheimsbygd').readUtf16String()"
      ");");
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");
}

#ifdef HAVE_WINDOWS

TESTCASE (ansi_string_can_be_allocated_in_code_page_936)
{
  CPINFOEX cpi;

  GetCPInfoEx (CP_THREAD_ACP, 0, &cpi);
  if (cpi.CodePage != 936)
  {
    g_print ("<skipping, only available on systems with ANSI code page 936> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT ("send("
      "Memory.allocAnsiString('test测试.').readAnsiString()"
      ");");
  EXPECT_SEND_MESSAGE_WITH ("\"test测试.\"");
}

TESTCASE (ansi_string_can_be_allocated_in_code_page_1252)
{
  CPINFOEX cpi;

  GetCPInfoEx (CP_THREAD_ACP, 0, &cpi);
  if (cpi.CodePage != 1252)
  {
    g_print ("<skipping, only available on systems with ANSI code page 1252> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT ("send("
      "Memory.allocAnsiString('Bjørheimsbygd').readAnsiString()"
      ");");
  EXPECT_SEND_MESSAGE_WITH ("\"Bjørheimsbygd\"");
}

#endif

#ifdef HAVE_TINYCC

TESTCASE (cmodule_can_be_defined)
{
  int (* add_impl) (int a, int b);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      ""
      "int\\n"
      "add (int a,\\n"
      "     int b)\\n"
      "{\\n"
      "  return a + b;\\n"
      "}"
      "');"
      "send(m.add);");

  add_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (add_impl);
  g_assert_cmpint (add_impl (3, 4), ==, 7);
}

TESTCASE (cmodule_can_be_defined_with_toolchain)
{
  const gchar * code =
      "int\\n"
      "answer (void)\\n"
      "{\\n"
      "  return 42;\\n"
      "}";
  int (* answer_impl) (void);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('%s', null, { toolchain: 'any' });"
      "send(m.answer);",
      code);
  answer_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_cmpint (answer_impl (), ==, 42);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('%s', null, { toolchain: 'internal' });"
      "send(m.answer);",
      code);
  answer_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_cmpint (answer_impl (), ==, 42);

#ifndef HAVE_MACOS
  if (g_test_slow ())
#endif
  {
    COMPILE_AND_LOAD_SCRIPT (
        "const m = new CModule('%s', null, { toolchain: 'external' });"
        "send(m.answer);",
        code);
    answer_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
    g_assert_cmpint (answer_impl (), ==, 42);
  }

  COMPILE_AND_LOAD_SCRIPT (
      "new CModule('%s', null, { toolchain: 'nope' });",
      code);
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER, "Error: invalid toolchain value");
}

TESTCASE (cmodule_can_be_created_from_prebuilt_binary)
{
#ifdef HAVE_DARWIN
  gchar * data_dir, * module_path;
  gpointer module_contents;
  gsize module_size;
  GBytes * module_bytes;
  int (* answer_impl) (void);

  data_dir = test_util_get_data_dir ();
  module_path = g_build_filename (data_dir, "prebuiltcmodule.dylib", NULL);
  g_assert_true (g_file_get_contents (module_path, (gchar **) &module_contents,
      &module_size, NULL));
  module_bytes =
      g_bytes_new_take (g_steal_pointer (&module_contents), module_size);

  COMPILE_AND_LOAD_SCRIPT (
      "let m = null;"
      "const notify = new NativeCallback(n => { send(n); }, 'void', ['int']);"
      "recv((message, data) => {"
      "  m = new CModule(data, { notify });"
      "  send(m.answer);"
      "});");
  EXPECT_NO_MESSAGES ();

  gum_script_post (fixture->script, "{}", module_bytes);
  answer_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_cmpint (answer_impl (), ==, 42);
  EXPECT_SEND_MESSAGE_WITH ("1337");
  EXPECT_NO_MESSAGES ();

  g_bytes_unref (module_bytes);
  g_free (module_path);
  g_free (data_dir);
#else
  g_test_skip ("Missing implementation or test on this OS");
#endif
}

TESTCASE (cmodule_symbols_can_be_provided)
{
  int a = 42;
  int b = 1337;
  int (* get_magic_impl) (void);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      ""
      "extern int a;\\n"
      "extern int b;\\n"
      "\\n"
      "int\\n"
      "get_magic (void)\\n"
      "{\\n"
      "  return a + b;\\n"
      "}"
      "', { a: " GUM_PTR_CONST ", b: " GUM_PTR_CONST " });"
      "send(m.get_magic);",
      &a, &b);

  get_magic_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (get_magic_impl);
  g_assert_cmpint (get_magic_impl (), ==, 1379);
}

TESTCASE (cmodule_should_report_parsing_errors)
{
  COMPILE_AND_LOAD_SCRIPT ("new CModule('void foo (int a');");
  EXPECT_ERROR_MESSAGE_MATCHING (ANY_LINE_NUMBER,
      "Error: compilation failed.+");
}

TESTCASE (cmodule_should_report_linking_errors)
{
  const gchar * expected_message =
      "(Error: linking failed: tcc: error: undefined symbol '"
#ifdef HAVE_DARWIN
      "_"
#endif
      "v'|undefined reference to `v')";

  COMPILE_AND_LOAD_SCRIPT ("new CModule('"
      "extern int v; int f (void) { return v; }');");
  EXPECT_ERROR_MESSAGE_MATCHING (ANY_LINE_NUMBER, expected_message);
}

TESTCASE (cmodule_should_provide_lifecycle_hooks)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      ""
      "extern void notify (int n);\\n"
      "\\n"
      "void\\n"
      "init (void)\\n"
      "{\\n"
      "  notify (1);\\n"
      "}\\n"
      "\\n"
      "void\\n"
      "finalize (void)\\n"
      "{\\n"
      "  notify (2);\\n"
      "}\\n"
      "', {"
      "  notify: new NativeCallback(n => { send(n); }, 'void', ['int'])"
      "});");
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_NO_MESSAGES ();

  UNLOAD_SCRIPT ();
  EXPECT_SEND_MESSAGE_WITH ("2");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (cmodule_can_be_used_with_interceptor_attach)
{
  int seen_argval = -1;
  int seen_retval = -1;
  gpointer seen_return_address = NULL;
  guint seen_thread_id = 0;
  guint seen_depth = G_MAXUINT;
  int seen_function_data = -1;
  int seen_thread_state_calls = -1;
  int seen_invocation_state_arg = -1;

  COMPILE_AND_LOAD_SCRIPT (
      "const cm = new CModule('"
      "  #include <gum/guminterceptor.h>\\n"
      "\\n"
      "  typedef struct _ThreadState ThreadState;\\n"
      "  typedef struct _InvState InvState;\\n"
      "\\n"
      "  struct _ThreadState\\n"
      "  {\\n"
      "    int calls;\\n"
      "  };\\n"
      "\\n"
      "  struct _InvState\\n"
      "  {\\n"
      "    int arg;\\n"
      "  };\\n"
      "\\n"
      "  extern int seenArgval;\\n"
      "  extern int seenRetval;\\n"
      "  extern gpointer seenReturnAddress;\\n"
      "  extern guint seenThreadId;\\n"
      "  extern guint seenDepth;\\n"
      "  extern int seenFunctionData;\\n"
      "  extern int seenThreadStateCalls;\\n"
      "  extern int seenInvocationStateArg;\\n"
      "\\n"
      "  void\\n"
      "  onEnter (GumInvocationContext * ic)\\n"
      "  {\\n"
      "    int arg = GPOINTER_TO_INT (\\n"
      "        gum_invocation_context_get_nth_argument (ic, 0));\\n"
      "\\n"
      "    seenArgval = arg;\\n"
      "    gum_invocation_context_replace_nth_argument (ic, 0,\\n"
      "        GINT_TO_POINTER (arg + 1));\\n"
      "\\n"
      "    seenReturnAddress =\\n"
      "        gum_invocation_context_get_return_address (ic);\\n"
      "    seenThreadId = gum_invocation_context_get_thread_id (ic);\\n"
      "    seenDepth = gum_invocation_context_get_depth (ic);\\n"
      "\\n"
      "    seenFunctionData = GUM_IC_GET_FUNC_DATA (ic, gsize);\\n"
      "\\n"
      "    ThreadState * ts = GUM_IC_GET_THREAD_DATA (ic, ThreadState);\\n"
      "    ts->calls++;\\n"
      "\\n"
      "    InvState * is = GUM_IC_GET_INVOCATION_DATA (ic, InvState);\\n"
      "    is->arg = seenArgval;\\n"
      "  }\\n"
      "\\n"
      "  void\\n"
      "  onLeave (GumInvocationContext * ic)\\n"
      "  {\\n"
      "    seenRetval = GPOINTER_TO_INT (\\n"
      "        gum_invocation_context_get_return_value (ic));\\n"
      "    gum_invocation_context_replace_return_value (ic,\\n"
      "        GINT_TO_POINTER (42));\\n"
      "\\n"
      "    ThreadState * ts = GUM_IC_GET_THREAD_DATA (ic, ThreadState);\\n"
      "    seenThreadStateCalls = ts->calls;\\n"
      "\\n"
      "    InvState * is = GUM_IC_GET_INVOCATION_DATA (ic, InvState);\\n"
      "    seenInvocationStateArg = is->arg;\\n"
      "  }\\n"
      "', {"
      "  seenArgval: " GUM_PTR_CONST ","
      "  seenRetval: " GUM_PTR_CONST ","
      "  seenReturnAddress: " GUM_PTR_CONST ","
      "  seenThreadId: " GUM_PTR_CONST ","
      "  seenDepth: " GUM_PTR_CONST ","
      "  seenFunctionData: " GUM_PTR_CONST ","
      "  seenThreadStateCalls: " GUM_PTR_CONST ","
      "  seenInvocationStateArg: " GUM_PTR_CONST
      "});"
      "Interceptor.attach(" GUM_PTR_CONST ", cm, ptr(1911));",
      &seen_argval,
      &seen_retval,
      &seen_return_address,
      &seen_thread_id,
      &seen_depth,
      &seen_function_data,
      &seen_thread_state_calls,
      &seen_invocation_state_arg,
      target_function_int);

  EXPECT_NO_MESSAGES ();

  g_assert_cmpint (target_function_int (1), ==, 42);
  g_assert_cmpint (seen_argval, ==, 1);
  g_assert_cmpint (seen_retval, ==, 90);
  g_assert_nonnull (seen_return_address);
  g_assert_cmpuint (seen_thread_id, ==, gum_process_get_current_thread_id ());
  g_assert_cmpuint (seen_depth, ==, 0);
  g_assert_cmpint (seen_function_data, ==, 1911);
  g_assert_cmpint (seen_thread_state_calls, ==, 1);
  g_assert_cmpint (seen_invocation_state_arg, ==, 1);

  target_function_int (12);
  g_assert_cmpint (seen_thread_state_calls, ==, 2);
  g_assert_cmpint (seen_invocation_state_arg, ==, 12);
}

TESTCASE (cmodule_can_be_used_with_interceptor_replace)
{
  int seen_replacement_data = -1;

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <gum/guminterceptor.h>\\n"
      "\\n"
      "extern int seenReplacementData;\\n"
      "\\n"
      "int\\n"
      "dummy (int arg)\\n"
      "{\\n"
      "  GumInvocationContext * ic =\\n"
      "      gum_interceptor_get_current_invocation ();\\n"
      "  seenReplacementData = GUM_IC_GET_REPLACEMENT_DATA (ic, gsize);\\n"
      "\\n"
      "  return 1337;\\n"
      "}\\n"
      "', { seenReplacementData: " GUM_PTR_CONST " });"
      "Interceptor.replace(" GUM_PTR_CONST ", m.dummy, ptr(1911));",
      &seen_replacement_data, target_function_int);

  EXPECT_NO_MESSAGES ();
  g_assert_cmpint (target_function_int (7), ==, 1337);
  g_assert_cmpint (seen_replacement_data, ==, 1911);

  gum_script_unload_sync (fixture->script, NULL);
  g_assert_cmpint (target_function_int (7), ==, 315);
}

TESTCASE (cmodule_can_be_used_with_stalker_events)
{
  GumThreadId test_thread_id;
  guint num_events = 0;
  gsize seen_user_data = 0;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <stdio.h>\\n"
      "#include <gum/gumspinlock.h>\\n"
      "#include <gum/gumstalker.h>\\n"
      "\\n"
      "extern GumSpinlock lock;\\n"
      "extern guint numEvents;\\n"
      "extern gpointer seenUserData;\\n"
      "\\n"
      "void\\n"
      "process (const GumEvent * event,\\n"
      "         GumCpuContext * cpu_context,\\n"
      "         gpointer user_data)\\n"
      "{\\n"
      "  switch (event->type)\\n"
      "  {\\n"
      "    case GUM_CALL:\\n"
      "      printf (\"[*] CALL\\\\n\");\\n"
      "      break;\\n"
      "    case GUM_RET:\\n"
      "      printf (\"[*] RET\\\\n\");\\n"
      "      break;\\n"
      "    case GUM_EXEC:\\n"
      "      printf (\"[*] EXEC\\\\n\");\\n"
      "      break;\\n"
      "    case GUM_BLOCK:\\n"
      "      printf (\"[*] BLOCK\\\\n\");\\n"
      "      break;\\n"
      "    case GUM_COMPILE:\\n"
      "      printf (\"[*] COMPILE\\\\n\");\\n"
      "      break;\\n"
      "    default:\\n"
      "      printf (\"[*] UNKNOWN\\\\n\");\\n"
      "      break;\\n"
      "  }\\n"
      "\\n"
      "  gum_spinlock_acquire (&lock);\\n"
      "  numEvents++;\\n"
      "  seenUserData = user_data;\\n"
      "  gum_spinlock_release (&lock);\\n"
      "}\\n"
      "', {"
      "  lock: Memory.alloc(Process.pointerSize),"
      "  numEvents: " GUM_PTR_CONST ","
      "  seenUserData: " GUM_PTR_CONST
      "});"
      "Stalker.follow(%" G_GSIZE_FORMAT ", {"
      "  events: { compile: true, call: true, ret: true },"
      "  onEvent: m.process,"
      "  data: ptr(42)"
      "});"
      "recv('stop', message => {"
      "  Stalker.unfollow(%" G_GSIZE_FORMAT ");"
      "  send('done');"
      "});",
      &num_events,
      &seen_user_data,
      test_thread_id,
      test_thread_id);
  g_usleep (1);
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
  EXPECT_NO_MESSAGES ();
  g_assert_true (num_events > 0);
  g_assert_cmphex (seen_user_data, ==, 42);
}

TESTCASE (cmodule_can_be_used_with_stalker_transform)
{
  GumThreadId test_thread_id;
  guint num_transforms = 0;
  gsize seen_user_data = 0;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <stdio.h>\\n"
      "#include <gum/gumstalker.h>\\n"
      "\\n"
      "static void on_ret (GumCpuContext * cpu_context, gpointer user_data);\\n"
      "\\n"
      "extern guint numTransforms;\\n"
      "extern gpointer seenUserData;\\n"
      "\\n"
      "void\\n"
      "transform (GumStalkerIterator * iterator,\\n"
      "           GumStalkerOutput * output,\\n"
      "           gpointer user_data)\\n"
      "{\\n"
      "  GumMemoryAccess access =\\n"
      "      gum_stalker_iterator_get_memory_access (iterator);\\n"
      "  printf (\"\\\\ntransform()\\\\n\");\\n"
      "  const cs_insn * insn = NULL;\\n"
      "  while (gum_stalker_iterator_next (iterator, &insn))\\n"
      "  {\\n"
      "    printf (\"\\\\t%%s %%s\\\\n\", insn->mnemonic, insn->op_str);\\n"
      "#if defined (HAVE_I386)\\n"
      "    if (insn->id == X86_INS_RET)\\n"
      "    {\\n"
      "      gum_x86_writer_put_nop (output->writer.x86);\\n"
      "      gum_stalker_iterator_put_callout (iterator, on_ret, NULL,\\n"
      "          NULL);\\n"
      "    }\\n"
      "#elif defined (HAVE_ARM)\\n"
      "    if (insn->id == ARM_INS_POP)\\n"
      "    {\\n"
      "      if (output->encoding == GUM_INSTRUCTION_DEFAULT)\\n"
      "        gum_arm_writer_put_nop (output->writer.arm);\\n"
      "      else\\n"
      "        gum_thumb_writer_put_nop (output->writer.thumb);\\n"
      "      if (access == GUM_MEMORY_ACCESS_OPEN)\\n"
      "      {\\n"
      "        gum_stalker_iterator_put_callout (iterator, on_ret, NULL,\\n"
      "            NULL);\\n"
      "      }\\n"
      "    }\\n"
      "#elif defined (HAVE_ARM64)\\n"
      "    if (insn->id == ARM64_INS_RET)\\n"
      "    {\\n"
      "      gum_arm64_writer_put_nop (output->writer.arm64);\\n"
      "      if (access == GUM_MEMORY_ACCESS_OPEN)\\n"
      "      {\\n"
      "        gum_stalker_iterator_put_callout (iterator, on_ret, NULL,\\n"
      "            NULL);\\n"
      "      }\\n"
      "    }\\n"
      "#endif\\n"
      "    gum_stalker_iterator_keep (iterator);\\n"
      "  }\\n"
      "  numTransforms++;\\n"
      "  seenUserData = user_data;\\n"
      "}\\n"
      "\\n"
      "static void\\n"
      "on_ret (GumCpuContext * cpu_context,"
      "        gpointer user_data)\\n"
      "{\\n"
      "  // printf (\"\\\\non_ret() cpu_context=%%p\\\\n\", cpu_context);\\n"
      "}\\n"
      "', {"
      "  numTransforms: " GUM_PTR_CONST ","
      "  seenUserData: " GUM_PTR_CONST
      "});"
      "Stalker.follow(%" G_GSIZE_FORMAT ", {"
      "  transform: m.transform,"
      "  data: ptr(3)"
      "});"
      "recv('stop', message => {"
      "  Stalker.unfollow(%" G_GSIZE_FORMAT ");"
      "  send('done');"
      "});",
      &num_transforms,
      &seen_user_data,
      test_thread_id,
      test_thread_id);
  g_usleep (1);
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"done\"");
  EXPECT_NO_MESSAGES ();
  g_assert_true (num_transforms > 0);
  g_assert_cmphex (seen_user_data, ==, 3);
}

TESTCASE (cmodule_can_be_used_with_stalker_callout)
{
  GumThreadId test_thread_id;
  guint num_callouts = 0;
  gsize seen_user_data = 0;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <stdio.h>\\n"
      "#include <gum/gumstalker.h>\\n"
      "\\n"
      "extern guint numCallouts;\\n"
      "extern gpointer seenUserData;\\n"
      "\\n"
      "void\\n"
      "onBeforeFirstInstruction (GumCpuContext * cpu_context,"
      "                          gpointer user_data)\\n"
      "{\\n"
      "  printf (\"cpu_context=%%p\\\\n\", cpu_context);\\n"
      "  numCallouts++;\\n"
      "  seenUserData = user_data;\\n"
      "}\\n"
      "', {"
      "  numCallouts: " GUM_PTR_CONST ","
      "  seenUserData: " GUM_PTR_CONST
      "});"
      "let instructionsSeen = 0;"
      "Stalker.follow(%" G_GSIZE_FORMAT ", {"
      "  transform(iterator) {"
      "    let instruction;"

      "    while ((instruction = iterator.next()) !== null) {"
      "      if (instructionsSeen === 0) {"
      "        iterator.putCallout(m.onBeforeFirstInstruction, ptr(7));"
      "      }"

      "      iterator.keep();"

      "      instructionsSeen++;"
      "    }"
      "  }"
      "});"
      "recv('stop', message => {"
      "  Stalker.unfollow(%" G_GSIZE_FORMAT ");"
      "  send(instructionsSeen > 0);"
      "});",
      &num_callouts,
      &seen_user_data,
      test_thread_id,
      test_thread_id);
  g_usleep (1);
  EXPECT_NO_MESSAGES ();
  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
  g_assert_true (num_callouts > 0);
  g_assert_cmphex (seen_user_data, ==, 7);
}

TESTCASE (cmodule_can_be_used_with_stalker_call_probe)
{
  GumThreadId test_thread_id;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  test_thread_id = gum_process_get_current_thread_id ();

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <stdio.h>\\n"
      "#include <gum/gumstalker.h>\\n"
      "\\n"
      "extern void send (gpointer v);\\n"
      ""
      "void\\n"
      "onCall (GumCallSite * site,"
      "        gpointer user_data)\\n"
      "{\\n"
      "  printf (\"block_address=%%p\\\\n\", site->block_address);\\n"
      "  send (user_data);\\n"
      "}\\n"
      "', {"
      "  send: new NativeCallback(v => { send(v.toUInt32()); }, 'void', "
          "['pointer'])"
      "});"
      "Stalker.addCallProbe(" GUM_PTR_CONST ", m.onCall, ptr(12));"
      "Stalker.follow(%" G_GSIZE_FORMAT ");"
      "recv('stop', message => {"
      "  Stalker.unfollow(%" G_GSIZE_FORMAT ");"
      "});"
      "send('ready');",
      target_function_int,
      test_thread_id,
      test_thread_id);
  EXPECT_SEND_MESSAGE_WITH ("\"ready\"");
  target_function_int (1337);
  EXPECT_SEND_MESSAGE_WITH ("12");
  POST_MESSAGE ("{\"type\":\"stop\"}");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (cmodule_can_be_used_with_module_map)
{
  TestScriptMessageItem * item;
  GumModuleMap * modules;
  gchar * expected_message;

  COMPILE_AND_LOAD_SCRIPT (
      "const modules = new ModuleMap();"
      "send(modules.handle);"
      ""
      "const cm = new CModule('"
      "#include <gum/gummodulemap.h>\\n"
      "\\n"
      "const gchar *\\n"
      "find (GumModuleMap * map,\\n"
      "      gconstpointer address)\\n"
      "{\\n"
      "  GumModule * m;\\n"
      "\\n"
      "  m = gum_module_map_find (map, GUM_ADDRESS (address));\\n"
      "  if (m == NULL)\\n"
      "    return NULL;\\n"
      "\\n"
      "  return gum_module_get_name (m);\\n"
      "}');"
      ""
      "const find = new NativeFunction(cm.find, 'pointer', "
          "['pointer', 'pointer']);"
      "send(find(modules, modules.values()[0].base).readUtf8String());"
      "send(find(modules, NULL).isNull());");

  item = test_script_fixture_pop_message (fixture);
  sscanf (item->message, "{\"type\":\"send\",\"payload\":\"0x%zx\"}",
      (size_t *) &modules);
  test_script_message_item_free (item);

  item = test_script_fixture_pop_message (fixture);
  expected_message = g_strdup_printf ("{\"type\":\"send\",\"payload\":\"%s\"}",
      gum_module_get_name (
        g_ptr_array_index (gum_module_map_get_values (modules), 0)));
  g_assert_cmpstr (item->message, ==, expected_message);
  g_free (expected_message);
  test_script_message_item_free (item);

  EXPECT_SEND_MESSAGE_WITH ("true");

  EXPECT_NO_MESSAGES ();
}

TESTCASE (cmodule_should_provide_some_builtin_string_functions)
{
  guint8 buf[2] = { 0, 0 };
  int (* score_impl) (const char * str);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <glib.h>\\n"
      "#include <string.h>\\n"
      "\\n"
      "extern guint8 buf[2];"
      ""
      "int\\n"
      "score (const char * str)\\n"
      "{\\n"
      "  if (strlen (str) == 1)\\n"
      "    return 1;\\n"
      "  if (strcmp (str, \"1234\") == 0)\\n"
      "    return 2;\\n"
      "  if (strstr (str, \"badger\") == str + 4)\\n"
      "    return 3;\\n"
      "  if (strchr (str, \\'!\\') == str + 3)\\n"
      "    return 4;\\n"
      "  if (strrchr (str, \\'/\\') == str + 8)\\n"
      "    return 5;\\n"
      "  if (strlen (str) == 2)\\n"
      "  {\\n"
      "    memcpy (buf, str, 2);\\n"
      "    return 6;\\n"
      "  }\\n"
      "  if (strlen (str) == 3)\\n"
      "  {\\n"
      "    memmove (buf, str + 1, 2);\\n"
      "    return 7;\\n"
      "  }\\n"
      "  if (strlen (str) == 4)\\n"
      "  {\\n"
      "    memset (buf, 88, 2);\\n"
      "    return 8;\\n"
      "  }\\n"
      "  if (strncmp (str, \"w00t\", 4) == 0)\\n"
      "    return 9;\\n"
      "  return -1;\\n"
      "}"
      "', { buf: " GUM_PTR_CONST " });"
      "send(m.score);",
      buf);

  score_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (score_impl);

  g_assert_cmpint (score_impl ("x"), ==, 1);
  g_assert_cmpint (score_impl ("1234"), ==, 2);
  g_assert_cmpint (score_impl ("Goodbadger"), ==, 3);
  g_assert_cmpint (score_impl ("Yay!"), ==, 4);
  g_assert_cmpint (score_impl ("/path/to/file"), ==, 5);

  g_assert_cmphex (buf[0], ==, 0);
  g_assert_cmphex (buf[1], ==, 0);
  g_assert_cmpint (score_impl ("xy"), ==, 6);
  g_assert_cmphex (buf[0], ==, 'x');
  g_assert_cmphex (buf[1], ==, 'y');

  memset (buf, 0, sizeof (buf));
  g_assert_cmpint (score_impl ("xyz"), ==, 7);
  g_assert_cmphex (buf[0], ==, 'y');
  g_assert_cmphex (buf[1], ==, 'z');

  memset (buf, 0, sizeof (buf));
  g_assert_cmpint (score_impl ("xyzx"), ==, 8);
  g_assert_cmphex (buf[0], ==, 'X');
  g_assert_cmphex (buf[1], ==, 'X');

  g_assert_cmpint (score_impl ("w00tage"), ==, 9);
}

TESTCASE (cmodule_should_provide_memory_access_apis)
{
  gboolean (* scan) (const guint8 * p, guint8 ** match);
  guint8 * match;
  const guint8 haystack[] = { 0x11, 0x22, 0x33, 0x13, 0x37, 0x44, 0x42, 0x55 };

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule(`"
      "#include <gum/gummemory.h>\\n"
      "\\n"
      "static gboolean store_match (GumAddress address, gsize size,\\n"
      "    gpointer user_data);\\n"
      "\\n"
      "gboolean\\n"
      "scan (const guint8 * p,\\n"
      "      guint8 ** match)\\n"
      "{\\n"
      "  guint8 * data;\\n"
      "  gsize n_bytes_read;\\n"
      "  GumMemoryRange range;\\n"
      "  GumMatchPattern * pattern;\\n"
      "\\n"
      "  *match = NULL;\\n"
      "\\n"
      "  data = gum_memory_read (p, 8, &n_bytes_read);\\n"
      "  if (data == NULL || n_bytes_read != 8)\\n"
      "    return FALSE;\\n"
      "\\n"
      "  range.base_address = GUM_ADDRESS (data);\\n"
      "  range.size = 8;\\n"
      "  pattern = gum_match_pattern_new_from_string (\"13 37 ?? 42\");\\n"
      "  gum_memory_scan (&range, pattern, store_match, match);\\n"
      "\\n"
      "  gum_match_pattern_unref (pattern);\\n"
      "  g_free (data);\\n"
      "\\n"
      "  return TRUE;\\n"
      "}\\n"
      "\\n"
      "static gboolean\\n"
      "store_match (GumAddress address,\\n"
      "             gsize size,\\n"
      "             gpointer user_data)\\n"
      "{\\n"
      "  guint8 ** match = user_data;\\n"
      "  *match = g_memdup (GSIZE_TO_POINTER (address), size);\\n"
      "  return FALSE;\\n"
      "}\\n"
      "`);"
      "send(m.scan);");

  scan = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (scan);

  g_assert_false (scan (GSIZE_TO_POINTER (42), &match));
  g_assert_null (match);

  g_assert_true (scan (haystack, &match));
  g_assert_nonnull (match);
  g_assert_cmphex (match[0], ==, 0x13);
  g_assert_cmphex (match[1], ==, 0x37);
  g_assert_cmphex (match[2], ==, 0x44);
  g_assert_cmphex (match[3], ==, 0x42);
  g_free (match);
}

TESTCASE (cmodule_should_support_memory_builtins)
{
  int (* f) (void);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule(`"
      "struct Pos1 { char x; char y; };\n"
      "struct Pos4 { int x; int y; };\n"
      "struct Pos8 { double x; double y; };\n"
      "\n"
      "int\n"
      "f (void)\n"
      "{\n"
      "  struct Pos1 a = { 0, }, b;\n"
      "  struct Pos4 c = { 0, }, d;\n"
      "  struct Pos8 e = { 0, }, f;\n"
      "  b = a;\n"
      "  d = c;\n"
      "  f = e;\n"
      "  return a.x + a.y + b.x + d.x + f.x;\n"
      "}\n"
      "`);"
      "send(m.f);");

  f = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (f);
  g_assert_cmpint (f (), ==, 0);
}

TESTCASE (cmodule_should_support_arithmetic_builtins)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule(`"
      "#include <stdint.h>\n"
      "\n"
      "int\n"
      "test_int_ops (int a,\n"
      "              int b)\n"
      "{\n"
      "  return (a / b) + (a %% b);\n"
      "}\n"
      "\n"
      "unsigned\n"
      "test_unsigned_ops (unsigned a,\n"
      "                   unsigned b)\n"
      "{\n"
      "  return (a / b) + (a %% b);\n"
      "}\n"
      "\n"
      "int64_t\n"
      "test_int64_ops (int64_t a,\n"
      "                int64_t b)\n"
      "{\n"
      "  return (a / b) + (a %% b);\n"
      "}\n"
      "`);"
      "send(m.test_int_ops);"
      "send(m.test_unsigned_ops);"
      "send(m.test_int64_ops);");

  {
    int (* test_int_ops) (int a, int b);

    test_int_ops = EXPECT_SEND_MESSAGE_WITH_POINTER ();
    g_assert_nonnull (test_int_ops);
    g_assert_cmpint (test_int_ops (16, 3), ==, 6);
  }

  {
    unsigned (* test_unsigned_ops) (unsigned a, unsigned b);

    test_unsigned_ops = EXPECT_SEND_MESSAGE_WITH_POINTER ();
    g_assert_nonnull (test_unsigned_ops);
    g_assert_cmpint (test_unsigned_ops (16, 3), ==, 6);
  }

  {
    gint64 (* test_int64_ops) (gint64 a, gint64 b);

    test_int64_ops = EXPECT_SEND_MESSAGE_WITH_POINTER ();
    g_assert_nonnull (test_int64_ops);
    g_assert_cmpint (test_int64_ops (16, 3), ==, 6);
  }
}

TESTCASE (cmodule_should_support_floating_point)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule([\n"
      "  '#include <glib.h>',\n"
      "  '',\n"
      "  'gdouble',\n"
      "  'measure (void)',\n"
      "  '{',\n"
      "  '  return 42.0;',\n"
      "  '}',\n"
      "].join('\\n'));\n"
      "\n"
      "const measure = new NativeFunction(m.measure, 'double', []);\n"
      "send(measure().toFixed(0));\n");
  EXPECT_SEND_MESSAGE_WITH ("\"42\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (cmodule_should_support_varargs)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule([\n"
      "  '#include <glib.h>',\n"
      "  '#include <stdio.h>',\n"
      "  '',\n"
      "  'typedef struct _MediumObj MediumObj;',\n"
      "  'typedef struct _LargeObj LargeObj;',\n"
      "  '',\n"
      "  'struct _MediumObj',\n"
      "  '{',\n"
      "  '  guint64 a;',\n"
      "  '  guint64 b;',\n"
      "  '};',\n"
      "  '',\n"
      "  'struct _LargeObj',\n"
      "  '{',\n"
      "  '  guint64 a;',\n"
      "  '  guint64 b;',\n"
      "  '  guint8 c;',\n"
      "  '};',\n"
      "  '',\n"
      "  'extern void deliver (const gchar * m1, const gchar * m2);',\n"
      "  '',\n"
      "  'static void log_generic (guint8 a1, guint16 a2, guint8 a3,',\n"
      "  '    guint8 a4, guint8 a5, guint8 a6, guint8 a7, guint8 a8,',\n"
      "  '    guint8 a9, guint8 a10, const gchar * format, ...);',\n"
      "  'static void log_special (const gchar * format, ...);',\n"
      "  '',\n"
      "  'void',\n"
      "  'sayHello (const gchar * name,',\n"
      "  '          guint8 x,',\n"
      "  '          guint8 y)',\n"
      "  '{',\n"
      "  '  // printf (\"Hello %%s, x=%%u, y=%%u\\\\n\", name, x, y);',\n"
      "  '  log_generic (201, 202, 203, 204, 205, 206, 207, 208, 209,',\n"
      "  '      210, \"Hello %%s, x=%%u, y=%%u\", name, x, y);',\n"
      "  '  {',\n"
      "  '    MediumObj m = { 100, 101 };',\n"
      "  '    LargeObj l = { 150, 151, 152 };',\n"
      "  '    log_special (\"slsm\", (guint8) 42, l, (guint8) 24, m);',\n"
      "  '  }',\n"
      "  '}',\n"
      "  '',\n"
      "  'static void',\n"
      "  'log_generic (guint8 a1,',\n"
      "  '             guint16 a2,',\n"
      "  '             guint8 a3,',\n"
      "  '             guint8 a4,',\n"
      "  '             guint8 a5,',\n"
      "  '             guint8 a6,',\n"
      "  '             guint8 a7,',\n"
      "  '             guint8 a8,',\n"
      "  '             guint8 a9,',\n"
      "  '             guint8 a10,',\n"
      "  '             const gchar * format,',\n"
      "  '             ...)',\n"
      "  '{',\n"
      "  '  gchar * m1, * m2;',\n"
      "  '  va_list args;',\n"
      "  '',\n"
      "  '  m1 = g_strdup_printf (\"%%u %%u %%u %%u %%u %%u %%u %%u %%u %%u\","
          "',\n"
      "  '      a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);',\n"
      "  '',\n"
      "  '  va_start (args, format);',\n"
      "  '  m2 = g_strdup_vprintf (format, args);',\n"
      "  '  va_end (args);',\n"
      "  '',\n"
      "  '  deliver (m1, m2);',\n"
      "  '',\n"
      "  '  g_free (m2);',\n"
      "  '  g_free (m1);',\n"
      "  '}',\n"
      "  '',\n"
      "  'static void',\n"
      "  'log_special (const gchar * format,',\n"
      "  '             ...)',\n"
      "  '{',\n"
      "  '  GString * message;',\n"
      "  '  va_list args;',\n"
      "  '  const gchar * p;',\n"
      "  '',\n"
      "  '  message = g_string_new (\"Yo\");',\n"
      "  '',\n"
      "  '  va_start (args, format);',\n"
      "  '',\n"
      "  '  p = format;',\n"
      "  '  while (*p != \\'\\\\0\\')',\n"
      "  '  {',\n"
      "  '    g_string_append_c (message, \\' \\');',\n"
      "  '',\n"
      "  '    switch (*p)',\n"
      "  '    {',\n"
      "  '      case \\'s\\':',\n"
      "  '      {',\n"
      "  '        unsigned int v = va_arg (args, unsigned int);',\n"
      "  '        g_string_append_printf (message, \"%%u\", v);',\n"
      "  '        break;',\n"
      "  '      }',\n"
      "  '      case \\'m\\':',\n"
      "  '      {',\n"
      "  '        MediumObj v = va_arg (args, MediumObj);',\n"
      "  '        g_string_append_printf (message, \"(%%\" G_GINT64_MODIFIER',"
      "  '            \"u, %%\" G_GINT64_MODIFIER \"u)\", v.a, v.b);',\n"
      "  '        break;',\n"
      "  '      }',\n"
      "  '      case \\'l\\':',\n"
      "  '      {',\n"
      "  '        LargeObj v = va_arg (args, LargeObj);',\n"
      "  '        g_string_append_printf (message, \"(%%\" G_GINT64_MODIFIER',"
      "  '            \"u, %%\" G_GINT64_MODIFIER \"u, %%u)\", v.a, v.b, v.c);"
          "',\n"
      "  '        break;',\n"
      "  '      }',\n"
      "  '      default:',\n"
      "  '        printf (\"Oops!\\\\n\");',\n"
      "  '        break;',\n"
      "  '    }',\n"
      "  '',\n"
      "  '    p++;',\n"
      "  '  }',\n"
      "  '',\n"
      "  '  va_end (args);',\n"
      "  '',\n"
      "  '  deliver (\"Also\", message->str);',\n"
      "  '',\n"
      "  '  g_string_free (message, TRUE);',\n"
      "  '}',\n"
      "].join('\\n'), {\n"
      "  deliver: new NativeCallback((m1, m2) => {\n"
      "    send([m1.readUtf8String(), m2.readUtf8String()]);\n"
      "  }, 'void', ['pointer', 'pointer'])\n"
      "});\n"
      "\n"
      "const sayHello = new NativeFunction(m.sayHello, 'void',\n"
      "    ['pointer', 'uint8', 'uint8']);\n"
      "sayHello(Memory.allocUtf8String('World'), 42, 24);\n");

  EXPECT_SEND_MESSAGE_WITH ("[\"201 202 203 204 205 206 207 208 209 210\","
      "\"Hello World, x=42, y=24\"]");
  EXPECT_SEND_MESSAGE_WITH ("[\"Also\",\"Yo 42 (150, 151, 152) 24 (100, 101)"
      "\"]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (cmodule_should_support_global_callbacks)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const cb = new NativeCallback(n => { send(n); }, 'void', ['int']);"
      "const cbPtr = Memory.alloc(Process.pointerSize);"
      "cbPtr.writePointer(cb);"
      ""
      "const m = new CModule('"
      "\\n"
      "extern void notify1 (int n);\\n"
      "extern void (* notify2) (int n);\\n"
      "extern void (* notify3) (int n);\\n"
      "\\n"
      "static void notify3_impl (int n);\\n"
      "\\n"
      "void\\n"
      "init (void)\\n"
      "{\\n"
      "  notify1 (42);\\n"
      "  notify2 (43);\\n"
      "  notify3 = notify3_impl;\\n"
      "  notify3 (44);\\n"
      "}\\n"
      "\\n"
      "static void\\n"
      "notify3_impl (int n)\\n"
      "{\\n"
      "  notify1 (n);\\n"
      "}\\n"
      "\\n"
      "', {"
      "  notify1: cb,"
      "  notify2: cbPtr,"
      "  notify3: Memory.alloc(Process.pointerSize)"
      "});");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_SEND_MESSAGE_WITH ("43");
  EXPECT_SEND_MESSAGE_WITH ("44");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (cmodule_should_provide_access_to_cpu_registers)
{
  int seen_value = -1;
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
# define GUM_IC_GET_FIRST_ARG(ic) *((int *) ((ic)->cpu_context->esp + 4))
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
# ifdef HAVE_WINDOWS
# define GUM_IC_GET_FIRST_ARG(ic) (ic)->cpu_context->rcx
# else
# define GUM_IC_GET_FIRST_ARG(ic) (ic)->cpu_context->rdi
# endif
#elif defined (HAVE_ARM)
# define GUM_IC_GET_FIRST_ARG(ic) (ic)->cpu_context->r[0]
#elif defined (HAVE_ARM64)
# define GUM_IC_GET_FIRST_ARG(ic) (ic)->cpu_context->x[0]
#elif defined (HAVE_MIPS)
# define GUM_IC_GET_FIRST_ARG(ic) (ic)->cpu_context->a0
#endif

  COMPILE_AND_LOAD_SCRIPT (
      "const cm = new CModule('"
      "  #include <gum/guminterceptor.h>\\n"
      "\\n"
      "  extern int seenValue;\\n"
      ""
      "  void\\n"
      "  onEnter (GumInvocationContext * ic)\\n"
      "  {\\n"
      "    seenValue = " G_STRINGIFY (GUM_IC_GET_FIRST_ARG (ic)) ";\\n"
      "  }\\n"
      "\\n"
      "', { seenValue: " GUM_PTR_CONST "});"
      "Interceptor.attach(" GUM_PTR_CONST ", cm);",
      &seen_value,
      target_function_int);

  EXPECT_NO_MESSAGES ();

  target_function_int (42);
  g_assert_cmpint (seen_value, ==, 42);
}

TESTCASE (cmodule_should_provide_access_to_system_error)
{
  void (* bump_impl) (void);

  COMPILE_AND_LOAD_SCRIPT (
      "const m = new CModule('"
      "#include <gum/gumprocess.h>\\n"
      ""
      "void\\n"
      "bump (void)\\n"
      "{\\n"
      "  gum_thread_set_system_error (gum_thread_get_system_error () + 1);\\n"
      "}"
      "');"
      "send(m.bump);");

  bump_impl = EXPECT_SEND_MESSAGE_WITH_POINTER ();
  g_assert_nonnull (bump_impl);

  gum_thread_set_system_error (1);
  bump_impl ();
  g_assert_cmpint (gum_thread_get_system_error (), ==, 2);
}

TESTCASE (system_error_unaffected_by_native_callback_from_cmodule)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const cm = new CModule(`\\n"
      "  #include <gum/guminterceptor.h>\\n"
      "\\n"
      "  extern void nativeCallback (void);\\n"
      "\\n"
      "  void\\n"
      "  replacement (GumInvocationContext * ic)\\n"
      "  {\\n"
      "    nativeCallback ();\\n"
      "  }\\n"
      "`, {\n"
      "  nativeCallback: new NativeCallback(() => {}, 'void', [])\n"
      "});\n"
      "Interceptor.replace(" GUM_PTR_CONST ", cm.replacement);",
      target_function_int);

#ifdef HAVE_WINDOWS
  SetLastError (1337);
  target_function_int (7);
  g_assert_cmpint (GetLastError (), ==, 1337);
#else
  errno = 1337;
  target_function_int (7);
  g_assert_cmpint (errno, ==, 1337);
#endif
}

#else /* !HAVE_TINYCC */

TESTCASE (cmodule_constructor_should_throw_not_available)
{
  COMPILE_AND_LOAD_SCRIPT ("new CModule('', {}, { toolchain: 'internal' });");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      "Error: internal toolchain is not available in this build configuration");
}

#endif

TESTCASE (cmodule_builtins_can_be_retrieved)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const { builtins } = CModule;"
      "send(typeof builtins);"
      "send(typeof builtins.defines);"
      "send(typeof builtins.headers);");
  EXPECT_SEND_MESSAGE_WITH ("\"object\"");
  EXPECT_SEND_MESSAGE_WITH ("\"object\"");
  EXPECT_SEND_MESSAGE_WITH ("\"object\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (script_can_be_compiled_to_bytecode)
{
  GError * error;
  GBytes * code;
  GumScript * script;

  error = NULL;
  code = gum_script_backend_compile_sync (fixture->backend, "testcase",
      "send(1337);\noops;", NULL, &error);
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_assert_nonnull (code);
    g_assert_no_error (error);

    g_assert_null (gum_script_backend_compile_sync (fixture->backend,
        "failcase1", "'", NULL, NULL));

    g_assert_null (gum_script_backend_compile_sync (fixture->backend,
        "failcase2", "'", NULL, &error));
    g_assert_nonnull (error);
    g_assert_true (g_str_has_prefix (error->message,
        "Script(line 1): SyntaxError: "));
    g_clear_error (&error);
  }
  else
  {
    g_assert_null (code);
    g_assert_nonnull (error);
    g_assert_cmpstr (error->message, ==,
        "compilation to bytecode is not supported by the V8 runtime");
    g_clear_error (&error);

    code = g_bytes_new (NULL, 0);
  }

  script = gum_script_backend_create_from_bytes_sync (fixture->backend, code,
      NULL, NULL, &error);
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    TestScriptMessageItem * item;

    g_assert_nonnull (script);
    g_assert_no_error (error);

    gum_script_set_message_handler (script, test_script_fixture_store_message,
        fixture, NULL);

    gum_script_load_sync (script, NULL);

    EXPECT_SEND_MESSAGE_WITH ("1337");

    item = test_script_fixture_pop_message (fixture);
    g_assert_nonnull (strstr (item->message, "ReferenceError"));
    g_assert_null (strstr (item->message, "agent.js"));
    g_assert_nonnull (strstr (item->message, "testcase.js"));
    test_script_message_item_free (item);

    EXPECT_NO_MESSAGES ();

    gum_script_unload_sync (script, NULL);

    g_object_unref (script);
  }
  else
  {
    g_assert_null (script);
    g_assert_nonnull (error);
    g_assert_cmpstr (error->message, ==,
        "script creation from bytecode is not supported by the V8 runtime");
    g_clear_error (&error);
  }

  g_bytes_unref (code);
}

TESTCASE (script_should_not_leak_if_destroyed_before_load)
{
  GumExceptor * held_instance;
  guint ref_count_before;
  GumScript * script;

  held_instance = gum_exceptor_obtain ();
  ref_count_before = G_OBJECT (held_instance)->ref_count;

  script = gum_script_backend_create_sync (fixture->backend, "testcase",
      "console.log('Hello World');", NULL, NULL, NULL);
  g_object_unref (script);

  g_assert_cmpuint (G_OBJECT (held_instance)->ref_count, ==, ref_count_before);
  g_object_unref (held_instance);
}

TESTCASE (script_memory_usage)
{
  GumScript * script;
  GTimer * timer;
  guint before, after;

  if (!GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_print ("<skipped due to runtime> ");
    return;
  }

  /* Warm up */
  script = gum_script_backend_create_sync (fixture->backend, "testcase",
      "const foo = 42;", NULL, NULL, NULL);
  gum_script_load_sync (script, NULL);
  gum_script_unload_sync (script, NULL);
  g_object_unref (script);

  timer = g_timer_new ();

  before = gum_peek_private_memory_usage ();

  g_timer_reset (timer);
  script = gum_script_backend_create_sync (fixture->backend, "testcase",
      "const foo = 42;", NULL, NULL, NULL);
  g_print ("created in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  g_timer_reset (timer);
  gum_script_load_sync (script, NULL);
  g_print ("loaded in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  after = gum_peek_private_memory_usage ();
  g_print ("memory usage: %u bytes\n", after - before);

  g_timer_reset (timer);
  gum_script_unload_sync (script, NULL);
  g_print ("unloaded in %u ms\n",
      (guint) (g_timer_elapsed (timer, NULL) * 1000.0));

  g_object_unref (script);
}

TESTCASE (esm_in_root_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "57 /main.js\n"
      "27 /dependency.js\n"
      "✄\n"
      "import { value } from './dependency.js';\n"
      "send({ value });\n"
      "✄\n"
      "export const value = 1337;\n");
  EXPECT_SEND_MESSAGE_WITH ("{\"value\":1337}");
}

TESTCASE (esm_in_subdir_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "57 /lib/main.js\n"
      "27 /lib/dependency.js\n"
      "✄\n"
      "import { value } from './dependency.js';\n"
      "send({ value });\n"
      "✄\n"
      "export const value = 1337;\n");
  EXPECT_SEND_MESSAGE_WITH ("{\"value\":1337}");
}

TESTCASE (esm_referencing_subdir_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "61 /main.js\n"
      "27 /lib/dependency.js\n"
      "✄\n"
      "import { value } from './lib/dependency.js';\n"
      "send({ value });\n"
      "✄\n"
      "export const value = 1337;\n");
  EXPECT_SEND_MESSAGE_WITH ("{\"value\":1337}");
}

TESTCASE (esm_referencing_parent_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "58 /lib/main.js\n"
      "27 /dependency.js\n"
      "✄\n"
      "import { value } from '../dependency.js';\n"
      "send({ value });\n"
      "✄\n"
      "export const value = 1337;\n");
  EXPECT_SEND_MESSAGE_WITH ("{\"value\":1337}");
}

TESTCASE (esm_throwing_on_load_should_emit_error)
{
  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "6 /main.js\n"
      "✄\n"
      "oops;\n");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "ReferenceError: 'oops' is not defined"
        : "ReferenceError: oops is not defined");
}

TESTCASE (esm_throwing_after_toplevel_await_should_emit_error)
{
  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "122 /main.js\n"
      "✄\n"
      "await sleep(10);\n"
      "oops;\n"
      "\n"
      "function sleep(duration) {\n"
      "  return new Promise(resolve => { setTimeout(resolve, duration); });\n"
      "}\n");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "ReferenceError: 'oops' is not defined"
        : "ReferenceError: oops is not defined");
}

TESTCASE (esm_referencing_missing_module_should_fail_to_load)
{
  const gchar * source =
      "📦\n"
      "41 /main.js\n"
      "✄\n"
      "import { value } from './dependency.js';\n";
  GError * error = NULL;

  g_assert_null (gum_script_backend_create_sync (fixture->backend,
      "testcase", source, NULL, NULL, &error));
  g_assert_nonnull (error);
  g_assert_cmpstr (error->message, ==,
      "Could not load module '/dependency.js'");
  g_error_free (error);
}

TESTCASE (dynamic_script_evaluation_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const result = Script.evaluate('/x.js', 'const x = 42; 1337;');"
      "send([result, x]);");
  EXPECT_SEND_MESSAGE_WITH ("[1337,42]");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (dynamic_script_evaluation_should_throw_on_syntax_error)
{
  COMPILE_AND_LOAD_SCRIPT ("Script.evaluate('/x.js', 'const x = \\'');");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "Error: could not parse '/x.js' line 1: unexpected end of string"
        : "Error: could not parse '/x.js' line 1: Invalid or unexpected token");
}

TESTCASE (dynamic_script_evaluation_should_throw_on_runtime_error)
{
  COMPILE_AND_LOAD_SCRIPT ("Script.evaluate('/x.js', 'x');");
  EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "ReferenceError: 'x' is not defined"
        : "ReferenceError: x is not defined");
}

TESTCASE (dynamic_script_loading_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "async function main() {"
        "const m = await Script.load('/x.js',"
            "'export const x = 42; send(\\'A\\');');"
        "send(typeof x);"
        "send(m.x);"
      "}"
      "main().catch(e => send(e.stack));");
  EXPECT_SEND_MESSAGE_WITH ("\"A\"");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (dynamic_script_loading_should_throw_on_syntax_error)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Script.load('/x.js', 'const x = \\'')"
          ".catch(e => { send(e.message); });");
  EXPECT_SEND_MESSAGE_WITH (
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "\"could not parse '/x.js' line 1: unexpected end of string\""
        : "\"could not parse '/x.js' line 1: Invalid or unexpected token\"");
}

TESTCASE (dynamic_script_loading_should_throw_on_runtime_error)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Script.load('/x.js', 'x')"
          ".catch(e => { send(e.message); });");
  EXPECT_SEND_MESSAGE_WITH (
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "\"'x' is not defined\""
        : "\"x is not defined\"");
}

TESTCASE (dynamic_script_loading_should_throw_on_error_with_toplevel_await)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Script.load('/x.js',"
          "`"
            "await sleep(10);\n"
            "x;\n"
            "\n"
            "function sleep(duration) {\n"
              "return new Promise(resolve => {\n"
                "setTimeout(resolve, duration);\n"
              "});\n"
            "}\n"
          "`)"
          ".catch(e => { send(e.message); });");
  EXPECT_SEND_MESSAGE_WITH (
      GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
        ? "\"'x' is not defined\""
        : "\"x is not defined\"");
}

TESTCASE (dynamic_script_loading_should_throw_on_dupe_load_attempt)
{
  COMPILE_AND_LOAD_SCRIPT (
      "async function main() {"
        "await Script.load('/x.js', 'true');"
        "Script.load('/x.js', 'true').catch(e => { send(e.message); });"
      "}"
      "main().catch(e => { Script.nextTick(() => { throw e; }); });");
  EXPECT_SEND_MESSAGE_WITH ("\"module '/x.js' already exists\"");
}

TESTCASE (dynamic_script_should_support_imports_from_parent)
{
  const gchar * source =
      "export const value = 1337;"

      "async function main() {"
        "await Script.load('/plugin.js', `"
          "import { value } from '/main.js';"
          "send(value);"
        "`);"
      "}"

      "main().catch(e => send(e.stack));";

  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "%u /main.js\n"
      "✄\n"
      "%s",
      (guint) strlen (source),
      source);
  EXPECT_SEND_MESSAGE_WITH ("1337");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (dynamic_script_should_support_imports_from_other_dynamic_scripts)
{
  COMPILE_AND_LOAD_SCRIPT (
      "async function main() {"
        "await Script.load('/dependency.js', 'export const value = 1337;');"
        "await Script.load('/main.js', `"
          "import { value } from './dependency.js';"
          "send(value);"
        "`);"
      "}"
      "main().catch(e => send(e.stack));");
  EXPECT_SEND_MESSAGE_WITH ("1337");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (dynamic_script_evaluated_should_support_inline_source_map)
{
  TestScriptMessageItem * item;

  /*
   * agent/index.ts
   * --------
   * 01 import * as math from "./math";
   * 02
   * 03 try {
   * 04     math.add(3, 4);
   * 05 } catch (e) {
   * 06     send((e as Error).stack);
   * 07 }
   *
   * agent/math.ts
   * -------
   * 01 export function add(a: number, b: number): number {
   * 02     throw new Error("not yet implemented");
   * 03 }
   */
  COMPILE_AND_LOAD_SCRIPT (
      "Script.evaluate('/user.js', `(function(){function r(e,n,t){function o(i,"
        "f){if(!n[i]){if(!e[i]){var c=\"function\"==typeof require&&require;if("
        "!f&&c)return c(i,!0);if(u)return u(i,!0);var a=new Error(\"Cannot find"
        " module '\"+i+\"'\");throw a.code=\"MODULE_NOT_FOUND\",a}var p=n[i]={e"
        "xports:{}};e[i][0].call(p.exports,function(r){var n=e[i][1][r];return "
        "o(n||r)},p,p.exports,r,e,n,t)}return n[i].exports}for(var u=\"function"
        "\"==typeof require&&require,i=0;i<t.length;i++)o(t[i]);return o}return"
        " r})()({1:[function(require,module,exports){\n"
        "\"use strict\";\n"
        "Object.defineProperty(exports, \"__esModule\", { value: true });\n"
        "const math = require(\"./math\");\n"
        "try {\n"
        "    math.add(3, 4);\n"
        "}\n"
        "catch (e) {\n"
        "    send(e.stack);\n"
        "}\n"
        "\n"
        "},{\"./math\":2}],2:[function(require,module,exports){\n"
        "\"use strict\";\n"
        "Object.defineProperty(exports, \"__esModule\", { value: true });\n"
        "exports.add = void 0;\n"
        "function add(a, b) {\n"
        "    throw new Error(\"not yet implemented\");\n"
        "}\n"
        "exports.add = add;\n"
        "\n"
        "},{}]},{},[1])\n"
        "//# sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2ZX"
        "JzaW9uIjozLCJzb3VyY2VzIjpbIm5vZGVfbW9kdWxlcy9icm93c2VyLXBhY2svX3ByZWx1"
        "ZGUuanMiLCJhZ2VudC9pbmRleC50cyIsImFnZW50L21hdGgudHMiXSwibmFtZXMiOltdLC"
        "JtYXBwaW5ncyI6IkFBQUE7OztBQ0FBLCtCQUErQjtBQUUvQixJQUFJO0lBQ0EsSUFBSSxD"
        "QUFDLEdBQUcsQ0FBQyxDQUFDLEVBQUUsQ0FBQyxDQUFDLENBQUM7Q0FDbEI7QUFBQyxPQU"
        "FPLENBQUMsRUFBRTtJQUNSLElBQUksQ0FBRSxDQUFXLENBQUMsS0FBSyxDQUFDLENBQUM7"
        "Q0FDNUI7Ozs7OztBQ05ELFNBQWdCLEdBQUcsQ0FBQyxDQUFTLEVBQUUsQ0FBUztJQUNwQy"
        "xNQUFNLElBQUksS0FBSyxDQUFDLHFCQUFxQixDQUFDLENBQUM7QUFDM0MsQ0FBQztBQUZE"
        "LGtCQUVDIiwiZmlsZSI6ImdlbmVyYXRlZC5qcyIsInNvdXJjZVJvb3QiOiIifQ==\n"
      "`);");

  item = test_script_fixture_pop_message (fixture);
  g_assert_nonnull (strstr (item->message, "\"type\":\"send\""));
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at add (agent/math.ts:2)\\n"
        "    at <anonymous> (agent/index.ts:4)\\n"
        "    at call (native)\\n"
        "    at o (node_modules/browser-pack/_prelude.js:1)\\n"
        "    at r (node_modules/browser-pack/_prelude.js:1)\\n"
        "    at <eval> (/user.js:21)"));
  }
  else
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at Object.add (agent/math.ts:2:11)\\n"
        "    at Object.1../math (agent/index.ts:4:10)\\n"
        "    at o (node_modules/browser-pack/_prelude.js:1:1)\\n"
        "    at r (node_modules/browser-pack/_prelude.js:1:1)\\n"
        "    at node_modules/browser-pack/_prelude.js:1:1"));
  }
  test_script_message_item_free (item);
}

TESTCASE (dynamic_script_loaded_should_support_inline_source_map)
{
  TestScriptMessageItem * item;

  /*
   * agent/index.ts
   * --------
   * 01 import * as math from "./math.js";
   * 02
   * 03 try {
   * 04     math.add(3, 4);
   * 05 } catch (e) {
   * 06     send((e as Error).stack);
   * 07 }
   *
   * agent/math.ts
   * -------
   * 01 export function add(a: number, b: number): number {
   * 02     throw new Error("not yet implemented");
   * 03 }
   */
  COMPILE_AND_LOAD_SCRIPT (
      "async function main() {"
        "await Script.load('/agent/math.js', `"
          "export function add(a, b) {\n"
          "    throw new Error(\"not yet implemented\");\n"
          "}\n"
          "//# sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2"
          "ZXJzaW9uIjozLCJmaWxlIjoibWF0aC5qcyIsInNvdXJjZVJvb3QiOiIvVXNlcnMvb2xl"
          "YXZyL3NyYy9mcmlkYS1hZ2VudC1leGFtcGxlLyIsInNvdXJjZXMiOlsiYWdlbnQvbWF0"
          "aC50cyJdLCJuYW1lcyI6W10sIm1hcHBpbmdzIjoiQUFBQSxNQUFNLFVBQVUsR0FBRyxD"
          "QUFDLENBQVMsRUFBRSxDQUFTO0lBQ3BDLE1BQU0sSUFBSSxLQUFLLENBQUMscUJBQXFC"
          "LENBQUMsQ0FBQztBQUMzQyxDQUFDIn0=\n"
        "`);"
        "await Script.load('/agent/index.js', `"
          "import * as math from \"./math.js\";\n"
          "try {\n"
          "    math.add(3, 4);\n"
          "}\n"
          "catch (e) {\n"
          "    send(e.stack);\n"
          "}\n"
          "//# sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2"
          "ZXJzaW9uIjozLCJmaWxlIjoiaW5kZXguanMiLCJzb3VyY2VSb290IjoiL1VzZXJzL29s"
          "ZWF2ci9zcmMvZnJpZGEtYWdlbnQtZXhhbXBsZS8iLCJzb3VyY2VzIjpbImFnZW50L2lu"
          "ZGV4LnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLE9BQU8sS0FBSyxJQUFJ"
          "LE1BQU0sV0FBVyxDQUFDO0FBRWxDLElBQUk7SUFDQSxJQUFJLENBQUMsR0FBRyxDQUFD"
          "LENBQUMsRUFBRSxDQUFDLENBQUMsQ0FBQztDQUNsQjtBQUFDLE9BQU8sQ0FBQyxFQUFF"
          "O0lBQ1IsSUFBSSxDQUFFLENBQVcsQ0FBQyxLQUFLLENBQUMsQ0FBQztDQUM1QiJ9\n"
        "`);"
      "}"
      "main().catch(e => send(e.stack));");

  item = test_script_fixture_pop_message (fixture);
  g_assert_nonnull (strstr (item->message, "\"type\":\"send\""));
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at add (agent/math.ts:2)\\n"
        "    at <anonymous> (agent/index.ts:4)"));
  }
  else
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at Module.add (agent/math.ts:2:11)\\n"
        "    at agent/index.ts:4:10"));
  }
  test_script_message_item_free (item);
}

TESTCASE (dynamic_script_loaded_should_support_separate_source_map)
{
  TestScriptMessageItem * item;

  /*
   * agent/index.ts
   * --------
   * 01 import * as math from "./math.js";
   * 02
   * 03 try {
   * 04     math.add(3, 4);
   * 05 } catch (e) {
   * 06     send((e as Error).stack);
   * 07 }
   *
   * agent/math.ts
   * -------
   * 01 export function add(a: number, b: number): number {
   * 02     throw new Error("not yet implemented");
   * 03 }
   */
  COMPILE_AND_LOAD_SCRIPT (
      "async function main() {"
        "Script.registerSourceMap('/agent/math.js', `{\"version\":3,\"file\":\""
          "math.js\",\"sourceRoot\":\"/Users/oleavr/src/frida-agent-example/\","
          "\"sources\":[\"agent/math.ts\"],\"names\":[],\"mappings\":\"AAAA,MAA"
          "M,UAAU,GAAG,CAAC,CAAS,EAAE,CAAS;IACpC,MAAM,IAAI,KAAK,CAAC,qBAAqB,CAA"
          "C,CAAC;AAC3C,CAAC\"}`);"
        "await Script.load('/agent/math.js', `"
          "export function add(a, b) {\n"
          "    throw new Error(\"not yet implemented\");\n"
          "}\n`);"
        "Script.registerSourceMap('/agent/index.js', `{\"version\":3,\"file\":"
          "\"index.js\",\"sourceRoot\":\"/Users/oleavr/src/frida-agent-example/"
          "\",\"sources\":[\"agent/index.ts\"],\"names\":[],\"mappings\":\"AAAA"
          ",OAAO,KAAK,IAAI,MAAM,WAAW,CAAC;AAElC,IAAI;IACA,IAAI,CAAC,GAAG,CAAC,C"
          "AAC,EAAE,CAAC,CAAC,CAAC;CAClB;AAAC,OAAO,CAAC,EAAE;IACR,IAAI,CAAE,CAA"
          "W,CAAC,KAAK,CAAC,CAAC;CAC5B\"}`);"
        "await Script.load('/agent/index.js', `"
          "import * as math from \"./math.js\";\n"
          "try {\n"
          "    math.add(3, 4);\n"
          "}\n"
          "catch (e) {\n"
          "    send(e.stack);\n"
          "}\n`);"
      "}"
      "main().catch(e => send(e.stack));");

  item = test_script_fixture_pop_message (fixture);
  g_assert_nonnull (strstr (item->message, "\"type\":\"send\""));
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at add (agent/math.ts:2)\\n"
        "    at <anonymous> (agent/index.ts:4)"));
  }
  else
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at Module.add (agent/math.ts:2:11)\\n"
        "    at agent/index.ts:4:10"));
  }
  test_script_message_item_free (item);
}

TESTCASE (worker_basics_should_be_supported)
{
  if (!GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_print ("<only available on QuickJS for now> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "202 /main.js\n"
      "126 /worker.js\n"
      "92 /wrangler.js\n"
      "✄\n"
      "import { url as workerUrl } from './worker.js';\n"
      "const w = new Worker(workerUrl, {\n"
      "    onMessage(message) {\n"
      "        send(`onMessage got: ${JSON.stringify(message)}`);\n"
      "    }\n"
      "});\n"
      "w.post({ type: 'ping' });\n"
      "\n"
      "✄\n"
      "import * as wrangler from './wrangler.js';\n"
      "export const url = import.meta.url;\n"
      "export function run() {\n"
      "    wrangler.init();\n"
      "}\n"
      "\n"
      "✄\n"
      "export function init() {\n"
      "    recv('ping', () => {\n"
      "        send({ type: 'pong' });\n"
      "    });\n"
      "}\n");
  EXPECT_SEND_MESSAGE_WITH ("\"onMessage got: {\\\"type\\\":\\\"pong\\\"}\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (worker_rpc_should_be_supported)
{
  if (!GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_print ("<only available on QuickJS for now> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "247 /main.js\n"
      "101 /worker.js\n"
      "✄\n"
      "import { url as workerUrl } from './worker.js';\n"
      "async function main() {\n"
      "    try {\n"
      "        const w = new Worker(workerUrl);\n"
      "        send(await w.exports.add(2, 3));\n"
      "    }\n"
      "    catch (e) {\n"
      "        Script.nextTick(() => { throw e; });\n"
      "    }\n"
      "}\n"
      "main();\n"
      "\n"
      "✄\n"
      "export const url = import.meta.url;\n"
      "export function run() {\n"
      "    rpc.exports.add = (a, b) => a + b;\n"
      "}\n");
  EXPECT_SEND_MESSAGE_WITH ("5");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (worker_termination_should_be_supported)
{
  if (!GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_print ("<only available on QuickJS for now> ");
    return;
  }

  COMPILE_AND_LOAD_SCRIPT (
      "📦\n"
      "290 /main.js\n"
      "221 /worker.js\n"
      "✄\n"
      "import { url as workerUrl } from './worker.js';\n"
      "async function main() {\n"
      "    try {\n"
      "        const w = new Worker(workerUrl);\n"
      "        setTimeout(() => { w.terminate(); }, 100);\n"
      "        send(await w.exports.simulateSlowRequest());\n"
      "    }\n"
      "    catch (e) {\n"
      "        send(e.message);\n"
      "    }\n"
      "}\n"
      "main();\n"
      "\n"
      "✄\n"
      "export const url = import.meta.url;\n"
      "export function run() {\n"
      "    rpc.exports.simulateSlowRequest = () => {\n"
      "        return new Promise(resolve => {\n"
      "            setTimeout(() => { resolve(42); }, 5000);\n"
      "        });\n"
      "    };\n"
      "}\n");
  EXPECT_SEND_MESSAGE_WITH ("\"worker terminated\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (source_maps_should_be_supported_for_our_runtime)
{
  TestScriptMessageItem * item;

  COMPILE_AND_LOAD_SCRIPT ("hexdump(null);");

  item = test_script_fixture_pop_message (fixture);
  g_assert_nonnull (strstr (item->message, " (/frida/runtime/hexdump.js:"));
  test_script_message_item_free (item);

  EXPECT_NO_MESSAGES ();
}

TESTCASE (source_maps_should_be_supported_for_user_scripts)
{
  TestScriptMessageItem * item;

  /*
   * index.js
   * --------
   * 01 'use strict';
   * 02
   * 03 var math = require('./math');
   * 04
   * 05 try {
   * 06   math.add(5, 2);
   * 07 } catch (e) {
   * 08   send(e.stack);
   * 09 }
   * 10
   * 11 setTimeout(function () {
   * 12   throw new Error('Oops!');
   * 13 }, 0);
   *
   * math.js
   * -------
   * 01 'use strict';
   * 02
   * 03 module.exports = {
   * 04   add: function (a, b) {
   * 05     throw new Error('not yet implemented');
   * 06   }
   * 07 };
   */

  COMPILE_AND_LOAD_SCRIPT (
      "(function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof requ"
      "ire==\"function\"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);v"
      "ar f=new Error(\"Cannot find module '\"+o+\"'\");throw f.code=\"MODULE_N"
      "OT_FOUND\",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){"
      "var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].expor"
      "ts}var i=typeof require==\"function\"&&require;for(var o=0;o<r.length;o+"
      "+)s(r[o]);return s})({1:[function(require,module,exports){"          "\n"
      "'use strict';"                                                       "\n"
      ""                                                                    "\n"
      "var math = require('./math');"                                       "\n"
      ""                                                                    "\n"
      "try {"                                                               "\n"
      /* testcase.js:7 => index.js:6 */
      "  math.add(5, 2);"                                                   "\n"
      "} catch (e) {"                                                       "\n"
      "  send(e.stack);"                                                    "\n"
      "}"                                                                   "\n"
      ""                                                                    "\n"
      "setTimeout(function () {"                                            "\n"
      /* testcase.js:13 => index.js:12 */
      "  throw new Error('Oops!');"                                         "\n"
      "}, 0);"                                                              "\n"
      ""                                                                    "\n"
      "},{\"./math\":2}],2:[function(require,module,exports){"              "\n"
      "'use strict';"                                                       "\n"
      ""                                                                    "\n"
      "module.exports = {"                                                  "\n"
      "  add: function (a, b) {"                                            "\n"
      /* testcase.js:21 => math.js:5 */
      "    throw new Error('not yet implemented');"                         "\n"
      "  }"                                                                 "\n"
      "};"                                                                  "\n"
      ""                                                                    "\n"
      "},{}]},{},[1])"                                                      "\n"
      "//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3"
      "VyY2VzIjpbIm5vZGVfbW9kdWxlcy9mcmlkYS9ub2RlX21vZHVsZXMvYnJvd3NlcmlmeS9ub2"
      "RlX21vZHVsZXMvYnJvd3Nlci1wYWNrL19wcmVsdWRlLmpzIiwiaW5kZXguanMiLCJtYXRoLm"
      "pzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBO0FDQUE7QUFDQTtBQUNBO0FBQ0E7QU"
      "FDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUNiQT"
      "tBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBIiwiZmlsZSI6ImdlbmVyYXRlZC"
      "5qcyIsInNvdXJjZVJvb3QiOiIifQ=="                                      "\n"
      "// And potentially some trailing code..."                            "\n"
  );

  item = test_script_fixture_pop_message (fixture);
  if (!GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
    g_assert_null (strstr (item->message, "testcase.js"));
  g_assert_nonnull (strstr (item->message, "\"type\":\"send\""));
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at add (math.js:5)\\n"
        "    at <anonymous> (index.js:6)\\n"
        "    at call (native)\\n"
        "    at s (node_modules/frida/node_modules/browserify/node_modules/"
            "browser-pack/_prelude.js:1)\\n"
        "    at e (node_modules/frida/node_modules/browserify/node_modules/"
            "browser-pack/_prelude.js:1)\\n"
        "    at <eval> (/testcase.js:25)"));
  }
  else
  {
    g_assert_nonnull (strstr (item->message,
        "\"payload\":\"Error: not yet implemented\\n"
        "    at Object.add (math.js:5:1)\\n"
        "    at Object.1../math (index.js:6:1)\\n"
        "    at s (node_modules/frida/node_modules/browserify/node_modules/"
            "browser-pack/_prelude.js:1:1)\\n"
        "    at e (node_modules/frida/node_modules/browserify/node_modules/"
            "browser-pack/_prelude.js:1:1)\\n"
        "    at node_modules/frida/node_modules/browserify/node_modules/"
            "browser-pack/_prelude.js:1:1\""));
  }
  test_script_message_item_free (item);

  item = test_script_fixture_pop_message (fixture);
  g_assert_null (strstr (item->message, "testcase.js"));
  g_assert_nonnull (strstr (item->message, "\"type\":\"error\""));
  g_assert_nonnull (strstr (item->message, "\"description\":\"Error: Oops!\""));
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_assert_nonnull (strstr (item->message, "\"stack\":\"Error: Oops!\\n"
        "    at <anonymous> (index.js:12)\\n"));
  }
  else
  {
    g_assert_nonnull (strstr (item->message, "\"stack\":\"Error: Oops!\\n"
        "    at index.js:12:1\\n"));
  }
  g_assert_nonnull (strstr (item->message, "\"fileName\":\"index.js\""));
  g_assert_nonnull (strstr (item->message, "\"lineNumber\":12"));
  g_assert_nonnull (strstr (item->message, "\"columnNumber\":1"));
  test_script_message_item_free (item);
}

TESTCASE (types_handle_invalid_construction)
{
  COMPILE_AND_LOAD_SCRIPT (
      "try {"
      "  NativePointer(\"0x1234\")"
      "} catch (e) {"
      "  send(e.message);"
      "}");
  EXPECT_SEND_MESSAGE_WITH (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
      ? "\"must be called with new\""
      : "\"use `new NativePointer()` to create a new instance, or use one of "
      "the two shorthands: `ptr()` and `NULL`\"");

  COMPILE_AND_LOAD_SCRIPT (
      "try {"
      "  NativeFunction(ptr(\"0x1234\"), 'void', []);"
      "} catch (e) {"
      "  send(e.message);"
      "}");
  EXPECT_SEND_MESSAGE_WITH (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
      ? "\"must be called with new\""
      : "\"use `new NativeFunction()` to create a new instance\"");

  COMPILE_AND_LOAD_SCRIPT (
      "try {"
      "  NativeCallback(() => {}, 'void', []);"
      "} catch (e) {"
      "  send(e.message);"
      "}");
  EXPECT_SEND_MESSAGE_WITH (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
      ? "\"must be called with new\""
      : "\"use `new NativeCallback()` to create a new instance\"");

  COMPILE_AND_LOAD_SCRIPT (
      "try {"
      "  File(\"/foo\", \"r\");"
      "} catch (e) {"
      "  send(e.message);"
      "}");
  EXPECT_SEND_MESSAGE_WITH (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend)
      ? "\"must be called with new\""
      : "\"use `new File()` to create a new instance\"");
}

TESTCASE (weak_ref_api_should_be_supported)
{
  COMPILE_AND_LOAD_SCRIPT (
      "let r = null;"
      "(() => {"
      "  const val = { name: 'Joe' };"
      "  r = new WeakRef(val);"
      "  send(r.deref() === val);"
      "})();"
      "setImmediate(() => {"
      "  gc();"
      "  send(typeof r.deref());"
      "});");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("\"undefined\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (weak_callback_is_triggered_on_gc)
{
  COMPILE_AND_LOAD_SCRIPT (
      "(() => {"
      "  const val = {};"
      "  Script.bindWeak(val, onWeakNotify);"
      "})();"
      "function onWeakNotify() {"
      "  send(\"weak notify\");"
      "}"
      "gc();");
  EXPECT_SEND_MESSAGE_WITH ("\"weak notify\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (weak_callback_is_triggered_on_unload)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const val = {};"
      "Script.bindWeak(val, () => {"
      "  send(\"weak notify\");"
      "});");
  EXPECT_NO_MESSAGES ();
  gum_script_unload_sync (fixture->script, NULL);
  EXPECT_SEND_MESSAGE_WITH ("\"weak notify\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (weak_callback_is_triggered_on_unbind)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const val = {};"
      "const id = Script.bindWeak(val, () => {"
      "  send(\"weak notify\");"
      "});"
      "Script.unbindWeak(id);");
  EXPECT_SEND_MESSAGE_WITH ("\"weak notify\"");
}

TESTCASE (weak_callback_should_not_be_exclusive)
{
  COMPILE_AND_LOAD_SCRIPT (
      "let val = {};"
      "const w1 = Script.bindWeak(val, onWeakNotify.bind(null, 'w1'));"
      "const w2 = Script.bindWeak(val, onWeakNotify.bind(null, 'w2'));"
      "recv(onMessage);"
      "function onMessage(message) {"
      "  switch (message.type) {"
      "    case 'unbind':"
      "      Script.unbindWeak(w1);"
      "      break;"
      "    case 'destroy':"
      "      val = null;"
      "      gc();"
      "  }"
      "  recv(onMessage);"
      "}"
      "function onWeakNotify(id) {"
      "  send(id);"
      "}");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"unbind\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"w1\"");
  EXPECT_NO_MESSAGES ();

  POST_MESSAGE ("{\"type\":\"destroy\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"w2\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (globals_can_be_dynamically_generated)
{
  COMPILE_AND_LOAD_SCRIPT (
      "Script.setGlobalAccessHandler({"
      "  get(property) {"
      "    if (property === 'badger')"
      "      return 1337 + mushroom;"
      "    else if (property === 'mushroom')"
      "      return 3;"
      "  },"
      "});"
      "send(badger);"
      "send(typeof badger);"
      "send(snake);");
  EXPECT_SEND_MESSAGE_WITH ("1340");
  EXPECT_SEND_MESSAGE_WITH ("\"number\"");
  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
        "ReferenceError: 'snake' is not defined");
  }
  else
  {
    EXPECT_ERROR_MESSAGE_WITH (ANY_LINE_NUMBER,
        "ReferenceError: snake is not defined");
  }
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "Script.setGlobalAccessHandler({"
      "  get(property) {"
      "  },"
      "});"
      "(1, eval)('mushroom = 42;');"
      "send(mushroom);");
  EXPECT_SEND_MESSAGE_WITH ("42");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (exceptions_can_be_handled)
{
  gsize page_size;
  gpointer page;
  gboolean exception_on_read, exception_on_write;

  if (!check_exception_handling_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "Process.setExceptionHandler(ex => {"
      "  send('w00t');"
      "});");

  EXPECT_NO_MESSAGES ();

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_mprotect (page, page_size, GUM_PAGE_NO_ACCESS);
  gum_try_read_and_write_at (page, 0, &exception_on_read, &exception_on_write);
  g_assert_true (exception_on_read);
  g_assert_true (exception_on_write);
  gum_memory_free (page, page_size);

  EXPECT_SEND_MESSAGE_WITH ("\"w00t\"");
  EXPECT_SEND_MESSAGE_WITH ("\"w00t\"");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (debugger_can_be_enabled)
{
  GumInspectorServer * server;
  GumScript * script;
  GError * error;

  if (GUM_QUICK_IS_SCRIPT_BACKEND (fixture->backend))
  {
    g_print ("<not available on QuickJS> ");
    return;
  }

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  server = gum_inspector_server_new ();
  g_signal_connect (server, "message", G_CALLBACK (on_incoming_debug_message),
      fixture);

  script = gum_script_backend_create_sync (fixture->backend, "script",
      "const scriptTimer = setInterval(() => {\n"
      "  send('hello');\n"
      "}, 1000);", NULL, NULL, NULL);
  fixture->script = script;
  gum_script_set_message_handler (script, on_script_message, "script", NULL);
  gum_script_set_debug_message_handler (script, on_outgoing_debug_message,
      server, NULL);
  gum_script_load_sync (script, NULL);

  error = NULL;
  if (gum_inspector_server_start (server, &error))
  {
    guint port;
    GMainLoop * loop;

    g_object_get (server, "port", &port, NULL);
    g_print ("Inspector server running on port %u.\n", port);

    loop = g_main_loop_new (g_main_context_get_thread_default (), FALSE);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
  }
  else
  {
    g_printerr ("Inspector server failed to start: %s\n", error->message);

    g_error_free (error);
  }

  g_object_unref (server);
}

TESTCASE (cloaked_items_can_be_queried_added_and_removed)
{
  void * buffer = malloc (64);

#ifdef HAVE_ANDROID
  COMPILE_AND_LOAD_SCRIPT (
      "const testRange = { base: " GUM_PTR_CONST ", size: 64 };"
      "send(Cloak.hasRangeContaining(testRange.base) === false);"
      "send(Process.findRangeByAddress(testRange.base) !== null);"
      "send(Cloak.clipRange(testRange) === null);"
      "Cloak.addRange(testRange);"
      "send(Cloak.hasRangeContaining(testRange.base));"
      "send(Process.findRangeByAddress(testRange.base) === null);"
      "send(Cloak.clipRange(testRange).length === 0);"
      "Cloak.removeRange(testRange);"

      "send(Cloak.hasCurrentThread() === false);"
      "const threadId = Process.getCurrentThreadId();"
      "send(Cloak.hasThread(threadId) === false);"
      "Cloak.addThread(threadId);"
      "send(!Process.enumerateThreads().map(x => x.id).includes(threadId));"
      "send(Cloak.hasCurrentThread() === true);"
      "Cloak.removeThread(threadId);"
      "send(true);"

      "const fd = 1;"
      "Cloak.addFileDescriptor(fd);"
      "send(Cloak.hasFileDescriptor(fd));"
      "Cloak.removeFileDescriptor(fd);"
      "send(!Cloak.hasFileDescriptor(fd));",
      buffer
  );
#else
  COMPILE_AND_LOAD_SCRIPT (
      "const testRange = { base: " GUM_PTR_CONST ", size: 64 };"
      "send(Cloak.hasRangeContaining(testRange.base) === false);"
      "send(Process.findRangeByAddress(testRange.base) !== null);"
      "send(Cloak.clipRange(testRange) === null);"
      "Cloak.addRange(testRange);"
      "send(Cloak.hasRangeContaining(testRange.base));"
      "send(Process.findRangeByAddress(testRange.base) === null);"
      "send(Cloak.clipRange(testRange).length === 0);"
      "Cloak.removeRange(testRange);"

      "send(Cloak.hasCurrentThread() === false);"
      "const threadId = Process.getCurrentThreadId();"
      "send(Cloak.hasThread(threadId) === false);"
      "Cloak.addThread(threadId);"
      "send(!Process.enumerateThreads().map(x => x.id).includes(threadId));"
      "send(Cloak.hasCurrentThread() === true);"
      "Cloak.removeThread(threadId);"
      "send(Process.enumerateThreads().map(x => x.id).includes(threadId));"

      "const fd = 1;"
      "Cloak.addFileDescriptor(fd);"
      "send(Cloak.hasFileDescriptor(fd));"
      "Cloak.removeFileDescriptor(fd);"
      "send(!Cloak.hasFileDescriptor(fd));",
      buffer
  );
#endif
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();

  free (buffer);
}

TESTCASE (code_can_be_profiled)
{
  TestScriptMessageItem * item;
  JsonNode * message;
  JsonReader * reader;
  const gchar * xml;

  COMPILE_AND_LOAD_SCRIPT (
      "const profiler = new Profiler();"
      "const sampler = new WallClockSampler();"
      "profiler.instrument(" GUM_PTR_CONST ", sampler, {"
      "  describe(args) {"
      "    return `n=${args[0].toUInt32()}`;"
      "  }"
      "});"
      "profiler.instrument(" GUM_PTR_CONST ", sampler);"
      "profiler.instrument(" GUM_PTR_CONST ", sampler);"
      "profiler.instrument(" GUM_PTR_CONST ", sampler);"
      "recv('stop', message => {"
      "  send(profiler.generateReport());"
      "});",
      target_function_variable_sleepyness,
      target_function_nested_a,
      target_function_nested_b,
      target_function_nested_c);
  EXPECT_NO_MESSAGES ();

  target_function_variable_sleepyness (7);
  target_function_variable_sleepyness (133700);
  target_function_variable_sleepyness (42);
  target_function_nested_a (42);

  POST_MESSAGE ("{\"type\":\"stop\"}");
  PUSH_TIMEOUT (2000);
  item = test_script_fixture_pop_message (fixture);
  POP_TIMEOUT ();
  EXPECT_NO_MESSAGES ();

  message = json_from_string (item->message, NULL);
  reader = json_reader_new (message);

  json_reader_read_member (reader, "type");
  g_assert_cmpstr (json_reader_get_string_value (reader), ==, "send");
  json_reader_end_member (reader);

  json_reader_read_member (reader, "payload");

  xml = json_reader_get_string_value (reader);

  if (g_test_verbose ())
    g_print ("%s\n", xml);

  g_assert_true (g_regex_match_simple (
        "    <node name=\"target_function_variable_sleepyness\" "
                 "total-calls=\"3\" total-duration=\"\\d+\">\n"
        "      <worst-case duration=\"\\d+\">n=133700<\\/worst-case>\n"
        "    <\\/node>", xml, G_REGEX_DEFAULT, G_REGEX_MATCH_DEFAULT));

  json_reader_end_member (reader);

  g_object_unref (reader);
  json_node_unref (message);
  test_script_message_item_free (item);
}

TESTCASE (cycles_can_be_sampled)
{
  if (!check_cycles_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "const sampler = new CycleSampler();"
      "const a = sampler.sample();"
      "const start = Date.now();"
      "while (Date.now() - start < 100)"
      "  ;"
      "const b = sampler.sample();"
      "send(b > a);");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (busy_cycles_can_be_sampled)
{
  if (!check_busy_cycles_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "const sampler = new BusyCycleSampler();"
      "const a = sampler.sample();"
      "const start = Date.now();"
      "while (Date.now() - start < 100)"
      "  ;"
      "const b = sampler.sample();"
      "send(b > a);");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (wall_clock_can_be_sampled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const sampler = new WallClockSampler();"
      "const a = sampler.sample();"
      "Thread.sleep(0.05);"
      "const b = sampler.sample();"
      "send(b > a);");
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (user_time_can_be_sampled)
{
  guint64 user_time_a = 0, user_time_b = 0;

  if (!check_user_time_testable ())
    return;

  COMPILE_AND_LOAD_SCRIPT (
      "const sampler = new UserTimeSampler();"
      "delay();"
      "const a = sampler.sample();"
      "delay();"
      "const b = sampler.sample();"
      GUM_PTR_CONST ".writeU64(a);"
      GUM_PTR_CONST ".writeU64(b);"
      "function delay() {"
      "  const start = Date.now();"
      "  while (Date.now() - start < 100)"
      "    for (let i = 0; i !== 1000000; i++);"
      "}",
      &user_time_a, &user_time_b);
  EXPECT_NO_MESSAGES ();

  g_assert_true (user_time_a != 0);
  g_assert_cmpuint (user_time_b, >, user_time_a);
}

TESTCASE (user_time_can_be_sampled_for_other_threads)
{
#if defined (HAVE_LINUX) && !defined (HAVE_PTHREAD_SETNAME_NP)
  g_print ("<skipping, libc is too old> ");
#else
  GumNamedSleeperContext ctx;
  GThread * thread;
  GumThreadId thread_id;
  guint64 user_time_a = 0, user_time_b = 0;

# ifdef HAVE_LINUX
  if (!check_exception_handling_testable ())
    return;
# endif

# ifdef HAVE_MIPS
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
# endif

  if (!check_user_time_testable ())
    return;

  ctx.controller_messages = g_async_queue_new ();
  ctx.sleeper_messages = g_async_queue_new ();

  thread = g_thread_new ("user-time", user_time_thread_proc, &ctx);
  g_assert_cmpstr (g_async_queue_pop (ctx.controller_messages), ==, "ready1");

  COMPILE_AND_LOAD_SCRIPT (
      "const threadId = Process.enumerateThreads()"
      "  .find(t => t.name === 'user-time')"
      "  .id;"
      GUM_PTR_CONST ".writeULong(threadId);",
      &thread_id);
  EXPECT_NO_MESSAGES ();

  COMPILE_AND_LOAD_SCRIPT (
      "const sampler = new UserTimeSampler(%" G_GSIZE_MODIFIER "u);"
      "const sample = sampler.sample();"
      GUM_PTR_CONST ".writeU64(sample);",
      thread_id, &user_time_a);
  EXPECT_NO_MESSAGES ();

  g_async_queue_push (ctx.sleeper_messages, "done1");
  g_assert_cmpstr (g_async_queue_pop (ctx.controller_messages), ==, "ready2");

  COMPILE_AND_LOAD_SCRIPT (
      "const sampler = new UserTimeSampler(%" G_GSIZE_MODIFIER "u);"
      "const sample = sampler.sample();"
      GUM_PTR_CONST ".writeU64(sample);",
      thread_id, &user_time_b);
  EXPECT_NO_MESSAGES ();

  g_async_queue_push (ctx.sleeper_messages, "done2");

  g_thread_join (thread);

  g_async_queue_unref (ctx.sleeper_messages);
  g_async_queue_unref (ctx.controller_messages);

  g_assert_true (user_time_a != 0);
  g_assert_cmpuint (user_time_b, >, user_time_a);
#endif
}

static gpointer
user_time_thread_proc (gpointer data)
{
  GumNamedSleeperContext * ctx = data;

  gum_ensure_current_thread_is_named ("user-time");

  do_work ();
  g_async_queue_push (ctx->controller_messages, "ready1");
  g_assert_cmpstr (g_async_queue_pop (ctx->sleeper_messages), ==, "done1");

  do_work ();
  g_async_queue_push (ctx->controller_messages, "ready2");
  g_assert_cmpstr (g_async_queue_pop (ctx->sleeper_messages), ==, "done2");

  return NULL;
}

TESTCASE (user_time_find_busy_threads)
{
#if defined (HAVE_LINUX) && !defined (HAVE_PTHREAD_SETNAME_NP)
  g_print ("<skipping, libc is too old> ");
#else
  guint rand, i;
  GumHotNamedSleeperContext ctx[NUM_THREADS];
  GThread * thread[NUM_THREADS];

# ifdef HAVE_LINUX
  if (!check_exception_handling_testable ())
    return;
# endif

# ifdef HAVE_MIPS
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
# endif

  if (!check_user_time_testable ())
    return;

  rand = g_random_int_range (0, NUM_THREADS);

  for (i = 0; i != NUM_THREADS; i++)
  {
    ctx[i].controller_messages = g_async_queue_new ();
    ctx[i].sleeper_messages = g_async_queue_new ();
    ctx[i].hot = (i == rand);

    thread[i] = g_thread_new ("user-time", user_time_find_busy_thread_proc,
        &ctx[i]);

    g_assert_cmpstr (g_async_queue_pop (ctx[i].controller_messages), ==,
        "ready1");
    g_async_queue_push (ctx[i].sleeper_messages, "done1");

    g_assert_cmpstr (g_async_queue_pop (ctx[i].controller_messages), ==,
        "ready2");
  }

  COMPILE_AND_LOAD_SCRIPT (
        "const times = Process.enumerateThreads()"
        "  .filter(t => t.name === 'user-time')"
        "  .map(t => {"
        "    const sampler = new UserTimeSampler(t.id);"
        "    const time = sampler.sample();"
        "    return { id: t.id, time };"
        "  });"
        "const result = times.sort((a, b) => Number(b.time - a.time))[0].id;"
        "send(result);");

  for (i = 0; i != NUM_THREADS; i++)
  {
    g_async_queue_push (ctx[i].sleeper_messages, "done2");
    g_thread_join (thread[i]);
    g_async_queue_unref (ctx[i].sleeper_messages);
    g_async_queue_unref (ctx[i].controller_messages);
  }

  EXPECT_SEND_MESSAGE_WITH ("%" G_GSIZE_MODIFIER "u", ctx[rand].id);
#endif
}

static gpointer
user_time_find_busy_thread_proc (gpointer data)
{
  GumHotNamedSleeperContext * ctx = data;

  gum_ensure_current_thread_is_named ("user-time");

  ctx->id = gum_process_get_current_thread_id ();
  g_async_queue_push (ctx->controller_messages, "ready1");
  g_assert_cmpstr (g_async_queue_pop (ctx->sleeper_messages), ==, "done1");

  if (ctx->hot)
    do_work ();
  g_async_queue_push (ctx->controller_messages, "ready2");
  g_assert_cmpstr (g_async_queue_pop (ctx->sleeper_messages), ==, "done2");

  return NULL;
}

static void
do_work (void)
{
  GTimer * timer;

  timer = g_timer_new ();

  while (g_timer_elapsed (timer, NULL) < 0.5)
  {
    guint i;

    for (i = 0; i != 1000000; i++)
      ;
  }

  g_timer_destroy (timer);
}

TESTCASE (malloc_count_can_be_sampled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const malloc = new NativeFunction(" GUM_PTR_CONST ", 'pointer', "
        "['size_t']);"
      "const free = new NativeFunction(" GUM_PTR_CONST ", 'void', "
        "['pointer']);"
      "const sampler = new MallocCountSampler();"
      "const a = sampler.sample();"
      "free(malloc(37));"
      "const b = sampler.sample();"
      "send(b > a);",
      malloc, free);
  EXPECT_SEND_MESSAGE_WITH ("true");
  EXPECT_NO_MESSAGES ();
}

TESTCASE (call_count_can_be_sampled)
{
  COMPILE_AND_LOAD_SCRIPT (
      "const f = new NativeFunction(" GUM_PTR_CONST ", 'int', ['int']);"
      "const sampler = new CallCountSampler([f]);"
      "f(42);"
      "send(Number(sampler.sample()));"
      "f(42);"
      "f(42);"
      "send(Number(sampler.sample()));",
      target_function_int);
  EXPECT_SEND_MESSAGE_WITH ("1");
  EXPECT_SEND_MESSAGE_WITH ("3");
  EXPECT_NO_MESSAGES ();
}

static gboolean
check_cycles_testable (void)
{
  gboolean available;
  GumSampler * sampler;

  sampler = gum_cycle_sampler_new ();
  available = gum_cycle_sampler_is_available (GUM_CYCLE_SAMPLER (sampler));
  g_object_unref (sampler);

  if (!available)
    g_print ("<skipping, unsupported OS> ");

  return available;
}

static gboolean
check_busy_cycles_testable (void)
{
  gboolean available;
  GumSampler * sampler;

  sampler = gum_busy_cycle_sampler_new ();
  available =
      gum_busy_cycle_sampler_is_available (GUM_BUSY_CYCLE_SAMPLER (sampler));
  g_object_unref (sampler);

  if (!available)
    g_print ("<skipping, unsupported OS> ");

  return available;
}

static gboolean
check_user_time_testable (void)
{
  gboolean available;
  GumSampler * sampler;

  sampler = gum_user_time_sampler_new ();
  available =
      gum_user_time_sampler_is_available (GUM_USER_TIME_SAMPLER (sampler));
  g_object_unref (sampler);

  if (!available)
    g_print ("<skipping, unsupported OS> ");

  return available;
}

static gboolean
check_exception_handling_testable (void)
{
  if (gum_process_is_debugger_attached ())
  {
    g_print ("<skipping, debugger is attached> ");
    return FALSE;
  }

  if (RUNNING_ON_VALGRIND)
  {
    g_print ("<skipping, not compatible with Valgrind> ");
    return FALSE;
  }

  return TRUE;
}

static void
on_script_message (const gchar * message,
                   GBytes * data,
                   gpointer user_data)
{
  gchar * sender = user_data;
  g_print ("Message from %s: %s\n", sender, message);
}

static void
on_incoming_debug_message (GumInspectorServer * server,
                           const gchar * message,
                           gpointer user_data)
{
  TestScriptFixture * fixture = user_data;

  gum_script_post_debug_message (fixture->script, message);
}

static void
on_outgoing_debug_message (const gchar * message,
                           gpointer user_data)
{
  GumInspectorServer * server = user_data;

  gum_inspector_server_post_message (server, message);
}

GUM_HOOK_TARGET static int
target_function_int (int arg)
{
  int result = 0;
  int i;

  /*
   * Pad the early part of the function so the loop doesn't branch back to the
   * first part, as we may need to overwrite quite a bit if we're unlucky.
   */
  gum_script_dummy_global_to_trick_optimizer += 1337;

  for (i = 0; i != 10; i++)
    result += i * arg;

  gum_script_dummy_global_to_trick_optimizer += result;

  /*
   * Throw in a dummy call to an external function so the platform's default ABI
   * is used at call-sites. Because this function is static there is otherwise
   * a chance that the compiler will invent its own calling convention, and any
   * JS-defined replacement function (NativeCallback) will be prone to clobber
   * registers used by the custom calling convention.
   */
  fflush (stdout);

  return result;
}

GUM_HOOK_TARGET static float
target_function_float (float arg)
{
  float result = 0;
  int i;

  for (i = 0; i != 10; i++)
    result += i * arg;

  gum_script_dummy_global_to_trick_optimizer += result;

  fflush (stdout);

  return result;
}

GUM_HOOK_TARGET static double
target_function_double (double arg)
{
  double result = 0;
  int i;

  for (i = 0; i != 10; i++)
    result += i * arg;

  gum_script_dummy_global_to_trick_optimizer += result;

  fflush (stdout);

  return result;
}

GUM_HOOK_TARGET static const guint8 *
target_function_base_plus_offset (const guint8 * base,
                                  int offset)
{
  gum_script_dummy_global_to_trick_optimizer += offset;

  fflush (stdout);

  return base + offset;
}

GUM_HOOK_TARGET static const gchar *
target_function_string (const gchar * arg)
{
  int i;

  for (i = 0; i != 10; i++)
    gum_script_dummy_global_to_trick_optimizer += i * arg[0];

  /* Prevent optimizer from assuming what the return value is. */
  if (gum_script_dummy_global_to_trick_optimizer == 0)
    return NULL;

  return arg;
}

GUM_HOOK_TARGET static void
target_function_callbacks (const gint value,
                           void (* first) (const gint * value),
                           void (* second) (const gint * value))
{
  int i;

  for (i = 0; i != 10; i++)
    gum_script_dummy_global_to_trick_optimizer += i * value;

  first (&value);

  second (&value);
}

GUM_HOOK_TARGET static void
target_function_trigger (TestTrigger * trigger)
{
  g_mutex_lock (&trigger->mutex);
  trigger->ready = TRUE;
  g_cond_signal (&trigger->cond);
  g_mutex_unlock (&trigger->mutex);

  g_mutex_lock (&trigger->mutex);
  while (!trigger->fired)
    g_cond_wait (&trigger->cond, &trigger->mutex);
  g_mutex_unlock (&trigger->mutex);
}

GUM_HOOK_TARGET static int
target_function_nested_a (int arg)
{
  int result = 0;
  int i;

  for (i = 0; i != 7; i++)
    result += i * arg;

  gum_script_dummy_global_to_trick_optimizer += result;

  return target_function_nested_b (result);
}

GUM_HOOK_TARGET static int
target_function_nested_b (int arg)
{
  int result = 0;
  int i;

  for (i = 0; i != 14; i++)
    result += i * arg;

  gum_script_dummy_global_to_trick_optimizer += result;

  return target_function_nested_c (result);
}

GUM_HOOK_TARGET static int
target_function_nested_c (int arg)
{
  int result = 0;
  int i;

  for (i = 0; i != 21; i++)
    result += i * arg;

  gum_script_dummy_global_to_trick_optimizer += result;

  return result;
}

GUM_HOOK_TARGET static void
target_function_variable_sleepyness (guint n)
{
  gum_script_dummy_global_to_trick_optimizer += n;

  g_usleep (n);
}

TESTCASE (script_can_be_interrupted)
{
  GumScript * script;
  GThread * worker;

  script = create_endless_script (fixture);

  worker = g_thread_new ("interrupt-test-loader", load_script_worker, script);

  g_usleep (G_USEC_PER_SEC / 10);

  gum_script_interrupt (script);

  g_thread_join (worker);

  gum_script_unload_sync (script, NULL);

  g_object_unref (script);
}

TESTCASE (script_interrupt_is_idempotent)
{
  GumScript * script;
  GThread * worker;

  script = create_endless_script (fixture);

  worker = g_thread_new ("interrupt-test-loader", load_script_worker, script);

  g_usleep (G_USEC_PER_SEC / 10);

  gum_script_interrupt (script);
  gum_script_interrupt (script);

  g_thread_join (worker);

  gum_script_unload_sync (script, NULL);

  g_object_unref (script);
}

TESTCASE (script_can_be_terminated)
{
  GumScript * script;
  GThread * worker;

  script = create_endless_script (fixture);

  worker = g_thread_new ("terminate-test-loader", load_script_worker, script);

  g_usleep (G_USEC_PER_SEC / 10);

  gum_script_terminate (script);

  g_thread_join (worker);

  g_object_unref (script);
}

TESTCASE (script_interrupt_while_idle_is_ignored)
{
  COMPILE_AND_LOAD_SCRIPT ("recv('poke', () => { send('pokeBack'); });");
  EXPECT_NO_MESSAGES ();

  gum_script_interrupt (fixture->script);

  POST_MESSAGE ("{\"type\":\"poke\"}");
  EXPECT_SEND_MESSAGE_WITH ("\"pokeBack\"");
  EXPECT_NO_MESSAGES ();
}

static gpointer
load_script_worker (gpointer data)
{
  GumScript * script = (GumScript *) data;

  gum_script_load_sync (script, NULL);

  return NULL;
}

static GumScript *
create_endless_script (TestScriptFixture * fixture)
{
  GumScript * script;
  GError * error = NULL;

  script = gum_script_backend_create_sync (fixture->backend, "testcase",
      "while (true) {}",
      NULL, NULL, &error);
  g_assert_nonnull (script);
  g_assert_null (error);

  gum_script_set_message_handler (script,
      test_script_fixture_store_message, fixture, NULL);

  return script;
}
