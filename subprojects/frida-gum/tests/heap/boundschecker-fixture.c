/*
 * Copyright (C) 2008-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumboundschecker.h"

#include "fakebacktracer.h"
#include "gummemory.h"
#include "testutil.h"

#include <stdlib.h>
#include <string.h>

#define TESTCASE(NAME) \
    void test_bounds_checker_ ## NAME ( \
        TestBoundsCheckerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Heap/BoundsChecker", \
        test_bounds_checker, NAME, TestBoundsCheckerFixture)

typedef struct _TestBoundsCheckerFixture
{
  GumBoundsChecker * checker;
  GumFakeBacktracer * backtracer;
  GString * output;
  guint output_call_count;
} TestBoundsCheckerFixture;

static void test_bounds_checker_fixture_do_output (const gchar * text,
    gpointer user_data);

static void
test_bounds_checker_fixture_setup (TestBoundsCheckerFixture * fixture,
                                   gconstpointer data)
{
  GumBacktracer * backtracer;

  backtracer = gum_fake_backtracer_new (NULL, 0);

  fixture->backtracer = GUM_FAKE_BACKTRACER (backtracer);
  fixture->output = g_string_new ("");

  fixture->checker = gum_bounds_checker_new (backtracer,
      test_bounds_checker_fixture_do_output, fixture);
}

static void
test_bounds_checker_fixture_teardown (TestBoundsCheckerFixture * fixture,
                                      gconstpointer data)
{
  g_object_unref (fixture->checker);

  g_string_free (fixture->output, TRUE);
  g_object_unref (fixture->backtracer);
}

static void
assert_same_output (TestBoundsCheckerFixture * fixture,
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
test_bounds_checker_fixture_do_output (const gchar * text,
                                       gpointer user_data)
{
  TestBoundsCheckerFixture * fixture = (TestBoundsCheckerFixture *) user_data;

  fixture->output_call_count++;
  g_string_append (fixture->output, text);
}

#define ATTACH_CHECKER() \
    gum_bounds_checker_attach_to_apis (fixture->checker, \
        test_util_heap_apis ())
#define DETACH_CHECKER() \
    gum_bounds_checker_detach (fixture->checker)

#define USE_BACKTRACE(bt) \
    fixture->backtracer->ret_addrs = bt; \
    fixture->backtracer->num_ret_addrs = G_N_ELEMENTS (bt);

static const GumReturnAddress malloc_backtrace[] =
{
  GUINT_TO_POINTER (0xbbbb1111),
  GUINT_TO_POINTER (0xbbbb2222)
};

static const GumReturnAddress free_backtrace[] =
{
  GUINT_TO_POINTER (0xcccc1111),
  GUINT_TO_POINTER (0xcccc2222)
};

static const GumReturnAddress violation_backtrace[] =
{
  GUINT_TO_POINTER (0xaaaa1111),
  GUINT_TO_POINTER (0xaaaa2222)
};

#if defined (__GNUC__) && __GNUC__ >= 12
# pragma GCC diagnostic ignored "-Wuse-after-free"
#endif
