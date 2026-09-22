/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumallocationtracker.h"

#include "dummyclasses.h"
#include "fakebacktracer.h"
#include "testutil.h"

#define TESTCASE(NAME) \
    void test_allocation_tracker_ ## NAME ( \
        TestAllocationTrackerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Heap/AllocationTracker", \
        test_allocation_tracker, NAME, TestAllocationTrackerFixture)

typedef struct _TestAllocationTrackerFixture
{
  GumAllocationTracker * tracker;
} TestAllocationTrackerFixture;

static void
test_allocation_tracker_fixture_setup (
    TestAllocationTrackerFixture * fixture,
    gconstpointer data)
{
  fixture->tracker = gum_allocation_tracker_new ();
}

static void
test_allocation_tracker_fixture_teardown (
    TestAllocationTrackerFixture * fixture,
    gconstpointer data)
{
  g_object_unref (fixture->tracker);
}

#define DUMMY_BLOCK_A (GUINT_TO_POINTER (0xDEADBEEF))
#define DUMMY_BLOCK_B (GUINT_TO_POINTER (0xB00BFACE))
#define DUMMY_BLOCK_C (GUINT_TO_POINTER (0xBEEFFACE))
#define DUMMY_BLOCK_D (GUINT_TO_POINTER (0xBEEFB00B))
#define DUMMY_BLOCK_E (GUINT_TO_POINTER (0xBEB00BEF))

static const GumReturnAddress dummy_return_addresses_a[] =
{
  GUINT_TO_POINTER (0x1234),
  GUINT_TO_POINTER (0x4321)
};

static const GumReturnAddress dummy_return_addresses_b[] =
{
  GUINT_TO_POINTER (0x1250),
  GUINT_TO_POINTER (0x4321),
};

static gboolean filter_cb (GumAllocationTracker * tracker, gpointer address,
    guint size, gpointer user_data);
