/*
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarmwriter.h"

#include "gumarmreg.h"
#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void test_arm_writer_ ## NAME ( \
        TestArmWriterFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/ArmWriter", test_arm_writer, NAME, \
        TestArmWriterFixture)

typedef struct _TestArmWriterFixture
{
  guint32 output[16];
  GumArmWriter aw;
} TestArmWriterFixture;

static void
test_arm_writer_fixture_setup (TestArmWriterFixture * fixture,
                               gconstpointer data)
{
  gum_arm_writer_init (&fixture->aw, fixture->output);
}

static void
test_arm_writer_fixture_teardown (TestArmWriterFixture * fixture,
                                  gconstpointer data)
{
  gum_arm_writer_clear (&fixture->aw);
}

#define assert_output_n_equals(n, v) \
    g_assert_cmphex (GUINT32_FROM_LE (fixture->output[n]), ==, v)
#define assert_output_equals(v) \
    assert_output_n_equals (0, v)
