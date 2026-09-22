/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarm64relocator.h"

#include "gummemory.h"
#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void test_arm64_relocator_ ## NAME ( \
        TestArm64RelocatorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Arm64Relocator", test_arm64_relocator, \
        NAME, TestArm64RelocatorFixture)

#define TEST_OUTBUF_SIZE 32

typedef struct _TestArm64RelocatorFixture
{
  guint8 * output;
  gsize output_size;
  GumArm64Writer aw;
  GumArm64Relocator rl;
  gboolean rl_initialized;
} TestArm64RelocatorFixture;

static void
test_arm64_relocator_fixture_setup (TestArm64RelocatorFixture * fixture,
                                    gconstpointer data)
{
  GumArm64Writer * aw = &fixture->aw;
  gsize page_size;

  page_size = gum_query_page_size ();
  fixture->output_size = page_size;
  fixture->output = (guint8 *) gum_memory_allocate (NULL, fixture->output_size,
      page_size, GUM_PAGE_RW);

  gum_arm64_writer_init (aw, fixture->output);
  aw->target_os = GUM_OS_LINUX;
  aw->ptrauth_support = GUM_PTRAUTH_UNSUPPORTED;
  aw->pc = 1024;
}

static void
test_arm64_relocator_fixture_teardown (TestArm64RelocatorFixture * fixture,
                                       gconstpointer data)
{
  if (fixture->rl_initialized)
    gum_arm64_relocator_clear (&fixture->rl);

  gum_arm64_writer_clear (&fixture->aw);

  gum_memory_free (fixture->output, fixture->output_size);
}

static const guint8 cleared_outbuf[TEST_OUTBUF_SIZE] = { 0, };

#define SETUP_RELOCATOR_WITH(CODE) \
    gum_arm64_relocator_init (&fixture->rl, CODE, &fixture->aw); \
    fixture->rl.input_pc = 2048; \
    fixture->rl_initialized = TRUE

#define assert_outbuf_still_zeroed_from_offset(OFF) \
    g_assert_cmpint (memcmp (fixture->output + OFF, cleared_outbuf + OFF, \
        sizeof (cleared_outbuf) - OFF), ==, 0)
