/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "sanitychecker-fixture.c"

#ifdef HAVE_WINDOWS

TESTLIST_BEGIN (sanitychecker)
  TESTENTRY (no_leaks)
  TESTENTRY (three_leaked_instances)
  TESTENTRY (three_leaked_blocks)
  TESTENTRY (ignore_gparam_instances)
  TESTENTRY (array_access_out_of_bounds_causes_exception)
  TESTENTRY (multiple_checks_at_once_should_not_collide)
  TESTENTRY (checker_itself_does_not_leak)
TESTLIST_END ()

TESTCASE (no_leaks)
{
  run_simulation (fixture, 0);
  g_assert_true (fixture->run_returned_true);
  g_assert_cmpuint (fixture->simulation_call_count, ==, 4);
  g_assert_cmpuint (fixture->output_call_count, ==, 0);
}

TESTCASE (three_leaked_instances)
{
  run_simulation (fixture,
      LEAK_FIRST_PONY | LEAK_SECOND_PONY | LEAK_FIRST_ZEBRA);
  g_assert_false (fixture->run_returned_true);
  g_assert_cmpuint (fixture->simulation_call_count, ==, 2);
  g_assert_cmpuint (fixture->output_call_count, >, 0);
  assert_same_output (fixture,
      "Instance leaks detected:\n"
      "\n"
      "\tCount\tGType\n"
      "\t-----\t-----\n"
      "\t2\tMyPony\n"
      "\t1\tZooZebra\n"
      "\n"
      "\tAddress\t\tRefCount\tGType\n"
      "\t--------\t--------\t-----\n"
      "\t%p\t2\t\tMyPony\n"
      "\t%p\t1\t\tMyPony\n"
      "\t%p\t1\t\tZooZebra\n",
      fixture->second_pony, fixture->first_pony, fixture->first_zebra);
}

TESTCASE (three_leaked_blocks)
{
  run_simulation (fixture,
      LEAK_FIRST_BLOCK | LEAK_SECOND_BLOCK | LEAK_THIRD_BLOCK);
  g_assert_false (fixture->run_returned_true);
  g_assert_cmpuint (fixture->simulation_call_count, ==, 3);
  g_assert_cmpuint (fixture->output_call_count, >, 0);
  assert_same_output (fixture,
      "Block leaks detected:\n"
      "\n"
      "\tCount\tSize\n"
      "\t-----\t----\n"
      "\t1\t15\n"
      "\t1\t10\n"
      "\t1\t5\n"
      "\n"
      "\tAddress\t\tSize\n"
      "\t--------\t----\n"
      "\t%p\t15\n"
      "\t%p\t10\n"
      "\t%p\t5\n",
      fixture->third_block,
      fixture->second_block,
      fixture->first_block);
}

TESTCASE (ignore_gparam_instances)
{
  run_simulation (fixture, LEAK_GPARAM_ONCE);
  g_assert_true (fixture->run_returned_true);
  g_assert_cmpuint (fixture->simulation_call_count, ==, 4);
  g_assert_cmpuint (fixture->output_call_count, ==, 0);
}

TESTCASE (array_access_out_of_bounds_causes_exception)
{
  guint8 * bytes;
  gboolean exception_on_read = FALSE, exception_on_write = FALSE;

#ifndef HAVE_WINDOWS
  if (gum_is_debugger_present ())
  {
    g_print ("<skipping, test must be run without debugger attached> ");
    return;
  }
#endif

  gum_sanity_checker_begin (fixture->checker, GUM_CHECK_BOUNDS);
  bytes = (guint8 *) malloc (1);
  bytes[0] = 42;
  gum_try_read_and_write_at (bytes, 1, &exception_on_read, &exception_on_write);
  free (bytes);
  gum_sanity_checker_end (fixture->checker);

  g_assert_true (exception_on_read);
  g_assert_true (exception_on_write);
}

TESTCASE (multiple_checks_at_once_should_not_collide)
{
  gboolean all_checks_pass;

  gum_sanity_checker_begin (fixture->checker,
      GUM_CHECK_BLOCK_LEAKS | GUM_CHECK_INSTANCE_LEAKS | GUM_CHECK_BOUNDS);
  all_checks_pass = gum_sanity_checker_end (fixture->checker);
  g_assert_true (all_checks_pass);
  g_assert_cmpuint (fixture->output->len, ==, 0);
}

TESTCASE (checker_itself_does_not_leak)
{
  GumSanityChecker * checker;

  checker = gum_sanity_checker_new (test_sanity_checker_fixture_do_output,
      fixture);
  gum_sanity_checker_begin (fixture->checker,
      GUM_CHECK_BLOCK_LEAKS | GUM_CHECK_INSTANCE_LEAKS | GUM_CHECK_BOUNDS);
  gum_sanity_checker_destroy (checker);
}

#endif /* HAVE_WINDOWS */
