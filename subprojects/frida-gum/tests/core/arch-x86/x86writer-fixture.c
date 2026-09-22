/*
 * Copyright (C) 2009-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumx86writer.h"

#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void test_code_writer_ ## NAME ( \
        TestCodeWriterFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/X86Writer", test_code_writer, NAME, \
        TestCodeWriterFixture)

typedef struct _TestCodeWriterFixture
{
  guint8 output[32];
  GumX86Writer cw;
} TestCodeWriterFixture;

static void
test_code_writer_fixture_setup (TestCodeWriterFixture * fixture,
                                gconstpointer data)
{
  gum_x86_writer_init (&fixture->cw, fixture->output);

  gum_x86_writer_set_target_cpu (&fixture->cw, GUM_CPU_AMD64);
  gum_x86_writer_set_target_abi (&fixture->cw, GUM_ABI_WINDOWS);
}

static void
test_code_writer_fixture_teardown (TestCodeWriterFixture * fixture,
                                   gconstpointer data)
{
  gum_x86_writer_clear (&fixture->cw);
}

static void
test_code_writer_fixture_assert_output_equals (TestCodeWriterFixture * fixture,
                                               const guint8 * expected_code,
                                               guint expected_length)
{
  guint actual_length;
  gboolean same_length, same_content;

  gum_x86_writer_flush (&fixture->cw);

  actual_length = gum_x86_writer_offset (&fixture->cw);
  same_length = (actual_length == expected_length);
  if (same_length)
  {
    same_content =
        memcmp (fixture->output, expected_code, expected_length) == 0;
  }
  else
  {
    same_content = FALSE;
  }

  if (!same_length || !same_content)
  {
    gchar * diff;

    if (actual_length != 0)
    {
      diff = test_util_diff_binary (expected_code, expected_length,
          fixture->output, actual_length);
      g_print ("\n\nGenerated code is not equal to expected code:\n\n%s\n",
          diff);
      g_free (diff);
    }
    else
    {
      g_print ("\n\nNo code was generated!\n\n");
    }
  }

  g_assert_true (same_length);
  g_assert_true (same_content);
}

#ifdef HAVE_I386
static void gum_test_native_function (const gchar * arg1, const gchar * arg2,
    const gchar * arg3, const gchar * arg4);
#endif

#define assert_output_equals(e) \
    test_code_writer_fixture_assert_output_equals (fixture, e, sizeof (e))
