/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumx86relocator.h"

#include "gummemory.h"
#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void test_relocator_ ## NAME ( \
        TestRelocatorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/X86Relocator", test_relocator, NAME, \
        TestRelocatorFixture)

#define TEST_OUTBUF_SIZE 32

typedef struct _TestRelocatorFixture
{
  guint8 * output;
  gsize output_size;
  GumX86Writer cw;
  GumX86Relocator rl;
} TestRelocatorFixture;

static void
test_relocator_fixture_setup (TestRelocatorFixture * fixture,
                              gconstpointer data)
{
  guint page_size;
  guint8 stack_data[1] = { 42 };
  GumAddressSpec as;

  page_size = gum_query_page_size ();

  as.near_address = (gpointer) stack_data;
  as.max_distance = G_MAXINT32 - page_size;

  fixture->output_size = page_size;
  fixture->output = (guint8 *) gum_memory_allocate_near (&as,
      fixture->output_size, page_size, GUM_PAGE_RW);
  memset (fixture->output, 0, page_size);

  gum_x86_writer_init (&fixture->cw, fixture->output);
}

static void
test_relocator_fixture_teardown (TestRelocatorFixture * fixture,
                                 gconstpointer data)
{
  gum_x86_relocator_clear (&fixture->rl);
  gum_x86_writer_clear (&fixture->cw);
  gum_memory_free (fixture->output, fixture->output_size);
}

static void
test_relocator_fixture_assert_output_equals (TestRelocatorFixture * fixture,
                                             const guint8 * expected_code,
                                             guint expected_length)
{
  guint actual_length;
  gboolean same_length, same_content;

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
      g_print ("\n\nRelocated code is not equal to expected code:\n\n%s\n",
          diff);
      g_free (diff);
    }
    else
    {
      g_print ("\n\nNo code was relocated!\n\n");
    }
  }

  g_assert_true (same_length);
  g_assert_true (same_content);
}

static const guint8 cleared_outbuf[TEST_OUTBUF_SIZE] = { 0, };

#define SETUP_RELOCATOR_WITH(CODE) \
    gum_x86_relocator_init (&fixture->rl, CODE, &fixture->cw)

#define assert_outbuf_still_zeroed_from_offset(OFF) \
    g_assert_cmpint (memcmp (fixture->output + OFF, cleared_outbuf + OFF, \
        sizeof (cleared_outbuf) - OFF), ==, 0)
#define assert_output_equals(e) test_relocator_fixture_assert_output_equals \
    (fixture, e, sizeof (e))
