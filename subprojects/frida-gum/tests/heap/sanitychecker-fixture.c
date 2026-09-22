/*
 * Copyright (C) 2010-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsanitychecker.h"

#ifdef HAVE_WINDOWS

#include "dummyclasses.h"
#include "testutil.h"

#include <stdlib.h>
#include <string.h>

#define TESTCASE(NAME) \
    void test_sanity_checker_ ## NAME ( \
        TestSanityCheckerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Heap/SanityChecker", \
        test_sanity_checker, NAME, TestSanityCheckerFixture)

typedef struct _TestSanityCheckerFixture
{
  GumSanityChecker * checker;
  GumInterceptor * interceptor;
  GString * output;

  guint simulation_call_count;
  gboolean run_returned_true;
  guint output_call_count;

  MyPony * first_pony;
  MyPony * second_pony;
  ZooZebra * first_zebra;
  ZooZebra * second_zebra;

  gpointer first_block;
  gpointer second_block;
  gpointer third_block;

  GParamSpec * pspec;

  guint leak_flags;
} TestSanityCheckerFixture;

typedef enum _LeakFlags
{
  LEAK_FIRST_PONY     = (1 << 0),
  LEAK_SECOND_PONY    = (1 << 1),
  LEAK_FIRST_ZEBRA    = (1 << 2),
  LEAK_SECOND_ZEBRA   = (1 << 3),

  LEAK_FIRST_BLOCK    = (1 << 4),
  LEAK_SECOND_BLOCK   = (1 << 5),
  LEAK_THIRD_BLOCK    = (1 << 6),

  LEAK_GPARAM_ONCE    = (1 << 7),
} LeakFlags;

static void simulation (gpointer user_data);
static void test_sanity_checker_fixture_do_cleanup (
    TestSanityCheckerFixture * fixture);
static void test_sanity_checker_fixture_do_output (const gchar * text,
    gpointer user_data);

static void
test_sanity_checker_fixture_setup (TestSanityCheckerFixture * fixture,
                                   gconstpointer data)
{
  fixture->output = g_string_new ("");

  fixture->interceptor = gum_interceptor_obtain ();
  gum_interceptor_ignore_other_threads (fixture->interceptor);

  fixture->checker = gum_sanity_checker_new_with_heap_apis (
      test_util_heap_apis (), test_sanity_checker_fixture_do_output, fixture);
}

static void
test_sanity_checker_fixture_teardown (TestSanityCheckerFixture * fixture,
                                      gconstpointer data)
{
  test_sanity_checker_fixture_do_cleanup (fixture);

  gum_sanity_checker_destroy (fixture->checker);

  gum_interceptor_unignore_other_threads (fixture->interceptor);
  g_object_unref (fixture->interceptor);

  g_string_free (fixture->output, TRUE);
}

static void
run_simulation (TestSanityCheckerFixture * fixture,
                guint leak_flags)
{
  fixture->leak_flags = leak_flags;
  fixture->simulation_call_count = 0;
  fixture->run_returned_true =
      gum_sanity_checker_run (fixture->checker, simulation, fixture);
}

static void
assert_same_output (TestSanityCheckerFixture * fixture,
                    const gchar * expected_output_format,
                    ...)
{
  gboolean is_exact_match;
  va_list args;
  gchar * expected_output;

  va_start (args, expected_output_format);
  expected_output = g_strdup_vprintf (expected_output_format, args);
  va_end (args);

  is_exact_match = strcmp (fixture->output->str, expected_output) == 0;
  if (!is_exact_match)
  {
    GString * message;
    gchar * diff;

    message = g_string_new ("Generated output not like expected:\n\n");

    diff = test_util_diff_text (expected_output, fixture->output->str);
    g_string_append (message, diff);
    g_free (diff);

    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,
        message->str);

    g_string_free (message, TRUE);
  }

  g_free (expected_output);
}

static void
simulation (gpointer user_data)
{
  TestSanityCheckerFixture * fixture = (TestSanityCheckerFixture *) user_data;

  fixture->simulation_call_count++;

  test_sanity_checker_fixture_do_cleanup (fixture);

  if ((fixture->leak_flags & (LEAK_FIRST_PONY | LEAK_SECOND_PONY |
      LEAK_FIRST_ZEBRA | LEAK_SECOND_ZEBRA)) != 0)
  {
    fixture->first_pony = g_object_new (MY_TYPE_PONY, NULL);
    fixture->second_pony = g_object_new (MY_TYPE_PONY, NULL);
    g_object_ref (fixture->second_pony);
    fixture->first_zebra = g_object_new (ZOO_TYPE_ZEBRA, NULL);
    fixture->second_zebra = g_object_new (ZOO_TYPE_ZEBRA, NULL);

    if ((fixture->leak_flags & LEAK_FIRST_PONY) == 0)
      g_clear_object (&fixture->first_pony);
    if ((fixture->leak_flags & LEAK_SECOND_PONY) == 0)
    {
      g_object_unref (fixture->second_pony);
      g_clear_object (&fixture->second_pony);
    }

    if ((fixture->leak_flags & LEAK_FIRST_ZEBRA) == 0)
      g_clear_object (&fixture->first_zebra);
    if ((fixture->leak_flags & LEAK_SECOND_ZEBRA) == 0)
      g_clear_object (&fixture->second_zebra);
  }

  if ((fixture->leak_flags & (LEAK_FIRST_BLOCK | LEAK_SECOND_BLOCK |
      LEAK_THIRD_BLOCK)) != 0)
  {
    fixture->first_block = malloc (5);
    fixture->second_block = malloc (10);
    fixture->third_block = malloc (15);

    /* just to get a group of size 42 with 0 objects alive: */
    free (malloc (42));

    if ((fixture->leak_flags & LEAK_FIRST_BLOCK) == 0)
      g_clear_pointer (&fixture->first_block, free);
    if ((fixture->leak_flags & LEAK_SECOND_BLOCK) == 0)
      g_clear_pointer (&fixture->second_block, free);
    if ((fixture->leak_flags & LEAK_THIRD_BLOCK) == 0)
      g_clear_pointer (&fixture->third_block, free);
  }

  if ((fixture->leak_flags & LEAK_GPARAM_ONCE) != 0 &&
      fixture->simulation_call_count > 1)
  {
    fixture->pspec = g_param_spec_int ("badger", "Badger", "Badger", 1, 10, 7,
        (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    fixture->leak_flags &= ~LEAK_GPARAM_ONCE;
  }
}

static void
test_sanity_checker_fixture_do_cleanup (TestSanityCheckerFixture * fixture)
{
  if (fixture->pspec != NULL)
  {
    g_param_spec_unref (fixture->pspec);
    fixture->pspec = NULL;
  }

  g_clear_pointer (&fixture->first_block, free);
  g_clear_pointer (&fixture->second_block, free);
  g_clear_pointer (&fixture->third_block, free);

  g_clear_object (&fixture->first_pony);
  g_clear_object (&fixture->second_pony);
  g_clear_object (&fixture->first_zebra);
  g_clear_object (&fixture->second_zebra);
}

static void
test_sanity_checker_fixture_do_output (const gchar * text,
                                       gpointer user_data)
{
  TestSanityCheckerFixture * fixture = (TestSanityCheckerFixture *) user_data;

  fixture->output_call_count++;
  g_string_append (fixture->output, text);
}

#endif /* HAVE_WINDOWS */
