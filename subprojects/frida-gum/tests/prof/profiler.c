/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "profiler-fixture.c"

TESTLIST_BEGIN (profiler)
#ifdef HAVE_I386
  TESTENTRY (i_can_has_instrumentability)
#endif
  TESTENTRY (already_instrumented)

  TESTENTRY (flat_function)
  TESTENTRY (two_calls)
  TESTENTRY (profile_matching_functions)
  TESTENTRY (recursion)
  TESTENTRY (deep_recursion)
  TESTENTRY (worst_case_duration)
  TESTENTRY (worst_case_info)
  TESTENTRY (worst_case_info_on_recursion)

  REPORT_TESTENTRY (bottleneck)
  REPORT_TESTENTRY (bottlenecks)
  REPORT_TESTENTRY (child_depth)
  REPORT_TESTENTRY (cyclic_recursion)
  REPORT_TESTENTRY (xml_basic)
  REPORT_TESTENTRY (xml_loop)
  REPORT_TESTENTRY (xml_loop_implicit)
  REPORT_TESTENTRY (xml_multiple_threads)
  REPORT_TESTENTRY (xml_worst_case_info)
  REPORT_TESTENTRY (xml_thread_ordering)
TESTLIST_END ()

#ifdef HAVE_I386

TESTCASE (i_can_has_instrumentability)
{
  UnsupportedFunction * unsupported_functions;
  guint count;

  unsupported_functions = unsupported_function_list_new (&count);

  g_assert_cmpint (gum_profiler_instrument_function (fixture->profiler,
      unsupported_functions[0].code, fixture->sampler), ==,
      GUM_INSTRUMENT_WRONG_SIGNATURE);

  unsupported_function_list_free (unsupported_functions);
}

#endif

TESTCASE (already_instrumented)
{
  g_assert_cmpint (gum_profiler_instrument_function (fixture->profiler,
      &sleepy_function, fixture->sampler), ==, GUM_INSTRUMENT_OK);
  g_assert_cmpint (gum_profiler_instrument_function (fixture->profiler,
      &sleepy_function, fixture->sampler), ==,
      GUM_INSTRUMENT_WAS_INSTRUMENTED);
}

TESTCASE (flat_function)
{
  GumProfiler * prof = fixture->profiler;

  g_assert_cmpint (gum_profiler_instrument_function (prof,
      &sleepy_function, fixture->sampler), ==, GUM_INSTRUMENT_OK);

  g_assert_cmpuint (gum_profiler_get_number_of_threads (prof), ==, 0);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (prof, 0,
      &sleepy_function), ==, 0);

  sleepy_function (fixture->fake_sampler);

  g_assert_cmpuint (gum_profiler_get_number_of_threads (prof), ==, 1);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (prof, 0,
      &sleepy_function), ==, 1000);
}

TESTCASE (two_calls)
{
  GumProfiler * prof = fixture->profiler;

  gum_profiler_instrument_function (prof, &sleepy_function, fixture->sampler);

  g_assert_cmpuint (gum_profiler_get_number_of_threads (prof), ==, 0);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (prof, 0,
      &sleepy_function), ==, 0);

  sleepy_function (fixture->fake_sampler);
  sleepy_function (fixture->fake_sampler);

  g_assert_cmpuint (gum_profiler_get_number_of_threads (prof), ==, 1);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (prof, 0,
      &sleepy_function), ==, 2 * 1000);
}

REPORT_TESTCASE (bottleneck)
{
  instrument_example_functions (fixture);

  example_a (fixture->fake_sampler);

  assert_n_top_nodes (fixture, 1, "example_a", "example_c");
}

REPORT_TESTCASE (bottlenecks)
{
  instrument_example_functions (fixture);

  example_a (fixture->fake_sampler);
  example_d (fixture->fake_sampler);

  assert_n_top_nodes (fixture, 2,
      "example_d", "example_c",
      "example_a", "example_c");
}

REPORT_TESTCASE (child_depth)
{
  instrument_example_functions (fixture);

  example_e (fixture->fake_sampler);

  assert_n_top_nodes (fixture, 1, "example_e", "example_f");
  assert_depth_from_root_node (fixture, 0, "example_e", "example_f",
      "example_g", NULL);
}

REPORT_TESTCASE (cyclic_recursion)
{
  instrument_example_functions (fixture);

  example_cyclic_a (fixture->fake_sampler, 1);

  assert_n_top_nodes (fixture, 1, "example_cyclic_a", "example_cyclic_b");
  assert_depth_from_root_node (fixture, 0, "example_cyclic_a",
      "example_cyclic_b", NULL);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (fixture->profiler, 0,
      &example_cyclic_a), ==, 4);
}

REPORT_TESTCASE (xml_basic)
{
  instrument_example_functions (fixture);

  example_a (fixture->fake_sampler);

  assert_same_xml (fixture,
      "<profile-report>\n"
      "  <thread>\n"
      "    <node name=\"example_a\" total-calls=\"1\" total-duration=\"9\">\n"
      "      <worst-case duration=\"9\" />\n"
      "      <node name=\"example_c\" total-calls=\"1\" total-duration=\"4\">\n"
      "        <worst-case duration=\"4\" />\n"
      "      </node>\n"
      "    </node>\n"
      "  </thread>\n"
      "</profile-report>");
}

REPORT_TESTCASE (xml_loop)
{
  instrument_example_functions (fixture);

  example_cyclic_a (fixture->fake_sampler, 1);

  assert_same_xml (fixture,
      "<profile-report>\n"
      "  <thread>\n"
      "    <node name=\"example_cyclic_a\" total-calls=\"2\" "
                 "total-duration=\"4\">\n"
      "      <worst-case duration=\"4\" />\n"
      "      <node name=\"example_cyclic_b\" total-calls=\"1\" "
                 "total-duration=\"3\">\n"
      "        <worst-case duration=\"3\" />\n"
      "      </node>\n"
      "    </node>\n"
      "  </thread>\n"
      "</profile-report>");
}

REPORT_TESTCASE (xml_loop_implicit)
{
  instrument_example_functions (fixture);

  example_cyclic_a (fixture->fake_sampler, 1);
  example_cyclic_b (fixture->fake_sampler, 0);

  assert_same_xml (fixture,
      "<profile-report>\n"
      "  <thread>\n"
      "    <node name=\"example_cyclic_b\" total-calls=\"2\" "
              "total-duration=\"6\">\n"
      "      <worst-case duration=\"3\" />\n"
      "      <node name=\"example_cyclic_a\" total-calls=\"3\" "
                "total-duration=\"5\">\n"
      "        <worst-case duration=\"4\" />\n"
      "      </node>\n"
      "    </node>\n"
      "    <node name=\"example_cyclic_a\" total-calls=\"3\" "
              "total-duration=\"5\">\n"
      "      <worst-case duration=\"4\" />\n"
      "      <node name=\"example_cyclic_b\" total-calls=\"2\" "
                "total-duration=\"6\">\n"
      "        <worst-case duration=\"3\" />\n"
      "      </node>\n"
      "    </node>\n"
      "  </thread>\n"
      "</profile-report>");
}

REPORT_TESTCASE (xml_multiple_threads)
{
  instrument_example_functions (fixture);

  example_a (fixture->fake_sampler);
  g_thread_join (g_thread_new ("profiler-test-multiple-threads",
      (GThreadFunc) example_d, fixture->fake_sampler));

  assert_same_xml (fixture,
      "<profile-report>\n"
      "  <thread>\n"
      "    <node name=\"example_d\" total-calls=\"1\" total-duration=\"11\">\n"
      "      <worst-case duration=\"11\" />\n"
      "      <node name=\"example_c\" total-calls=\"1\" total-duration=\"4\">\n"
      "        <worst-case duration=\"4\" />\n"
      "      </node>\n"
      "    </node>\n"
      "  </thread>\n"
      "  <thread>\n"
      "    <node name=\"example_a\" total-calls=\"1\" total-duration=\"9\">\n"
      "      <worst-case duration=\"9\" />\n"
      "      <node name=\"example_c\" total-calls=\"1\" total-duration=\"4\">\n"
      "        <worst-case duration=\"4\" />\n"
      "      </node>\n"
      "    </node>\n"
      "  </thread>\n"
      "</profile-report>");
}

REPORT_TESTCASE (xml_worst_case_info)
{
  gum_profiler_instrument_function_with_inspector (fixture->profiler,
      &example_worst_case_info, fixture->sampler, inspect_worst_case_info,
      NULL, NULL);

  example_worst_case_info (fixture->fake_sampler, "early", 1);
  example_worst_case_info (fixture->fake_sampler, "mid", 3);
  example_worst_case_info (fixture->fake_sampler, "late", 2);

  assert_same_xml (fixture,
      "<profile-report>\n"
      "  <thread>\n"
      "    <node name=\"example_worst_case_info\" total-calls=\"3\" "
               "total-duration=\"6\">\n"
      "      <worst-case duration=\"3\">mid</worst-case>\n"
      "    </node>\n"
      "  </thread>\n"
      "</profile-report>");
}

REPORT_TESTCASE (xml_thread_ordering)
{
  GThread * t1, * t2, * td;
  instrument_simple_functions (fixture);

  simple_1 (fixture->fake_sampler);

  /*
   * These threads must run in series since our fake sampler isn't re-entrant.
   */
  t1 = g_thread_new ("profiler-test-helper-a", (GThreadFunc) simple_2,
      fixture->fake_sampler);
  g_thread_join (t1);

  /*
   * Some OSes (QNX) aggressively recycle thread IDs. Avoid t1 and t2 having
   * the same thread ID by creating a new thread in the interim. We don't join
   * this thread until later so that it can't be reaped.
   */
  td = g_thread_new ("dummy", (GThreadFunc) dummy, fixture->fake_sampler);

  t2 = g_thread_new ("profiler-test-helper-b", (GThreadFunc) simple_3,
      fixture->fake_sampler);
  g_thread_join (t2);

  assert_same_xml (fixture,
      "<profile-report>\n"
      "  <thread>\n"
      "    <node name=\"simple_3\" total-calls=\"1\" total-duration=\"3\">\n"
      "      <worst-case duration=\"3\" />\n"
      "    </node>\n"
      "  </thread>\n"
      "  <thread>\n"
      "    <node name=\"simple_2\" total-calls=\"1\" total-duration=\"2\">\n"
      "      <worst-case duration=\"2\" />\n"
      "    </node>\n"
      "  </thread>\n"
      "  <thread>\n"
      "    <node name=\"simple_1\" total-calls=\"1\" total-duration=\"1\">\n"
      "      <worst-case duration=\"1\" />\n"
      "    </node>\n"
      "  </thread>\n"
      "</profile-report>");

  g_thread_join (td);
}

TESTCASE (profile_matching_functions)
{
#ifdef HAVE_DARWIN
  if (!g_test_slow ())
  {
    g_print ("<skipping due to CoreSymbolication issue, run in slow mode> ");
    return;
  }
#endif

  gum_profiler_instrument_functions_matching (fixture->profiler, "simple_*",
      fixture->sampler, exclude_simple_stdcall_50, NULL);

  simple_cdecl_42 (fixture->fake_sampler);
  simple_stdcall_48 (fixture->fake_sampler);
  simple_stdcall_50 (fixture->fake_sampler);

  g_assert_cmpuint (gum_profiler_get_total_duration_of (fixture->profiler, 0,
      &simple_cdecl_42), ==, 42);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (fixture->profiler, 0,
      &simple_stdcall_48), ==, 48);
  g_assert_cmpuint (gum_profiler_get_total_duration_of (fixture->profiler, 0,
      &simple_stdcall_50), ==, 0);
}

TESTCASE (recursion)
{
  gum_profiler_instrument_function (fixture->profiler, &recursive_function,
      fixture->sampler);
  recursive_function (2);
}

TESTCASE (deep_recursion)
{
  gum_profiler_instrument_function (fixture->profiler,
      &deep_recursive_function, fixture->sampler);
  gum_profiler_instrument_function (fixture->profiler,
      &deep_recursive_caller, fixture->sampler);
  deep_recursive_function (3);
}

TESTCASE (worst_case_duration)
{
  gum_profiler_instrument_function (fixture->profiler,
      &example_a_calls_b_thrice, fixture->sampler);
  gum_profiler_instrument_function (fixture->profiler,
      &example_b_dynamic, fixture->sampler);

  g_assert_cmpuint (gum_profiler_get_worst_case_duration_of (fixture->profiler,
      0, &example_b_dynamic), ==, 0);

  example_a_calls_b_thrice (fixture->fake_sampler);

  g_assert_cmpuint (gum_profiler_get_worst_case_duration_of (fixture->profiler,
      0, &example_b_dynamic), ==, 3);
}

TESTCASE (worst_case_info)
{
  gum_profiler_instrument_function_with_inspector (fixture->profiler,
      &example_worst_case_info, fixture->sampler, inspect_worst_case_info,
      NULL, NULL);

  g_assert_cmpstr (gum_profiler_get_worst_case_info_of (fixture->profiler, 0,
      &example_worst_case_info), ==, "");

  example_worst_case_info (fixture->fake_sampler, "early", 1);
  example_worst_case_info (fixture->fake_sampler, "mid", 3);
  example_worst_case_info (fixture->fake_sampler, "late", 2);

  g_assert_cmpstr (gum_profiler_get_worst_case_info_of (fixture->profiler, 0,
      &example_worst_case_info), ==, "mid");
}

TESTCASE (worst_case_info_on_recursion)
{
  gum_profiler_instrument_function_with_inspector (fixture->profiler,
      &example_worst_case_recursive, fixture->sampler,
      inspect_recursive_worst_case_info, NULL, NULL);

  example_worst_case_recursive (2, fixture->fake_sampler);

  g_assert_cmpstr (gum_profiler_get_worst_case_info_of (fixture->profiler, 0,
      &example_worst_case_recursive), ==, "2");
}
