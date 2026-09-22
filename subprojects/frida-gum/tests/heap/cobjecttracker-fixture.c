/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcobjecttracker.h"

#ifdef HAVE_WINDOWS

#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void test_cobject_tracker_ ## NAME (TestCObjectTrackerFixture * fixture, \
        gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Heap/CObjectTracker", test_cobject_tracker, \
        NAME, TestCObjectTrackerFixture)

typedef struct _MyObject MyObject;

GUM_HOOK_TARGET static MyObject *
my_object_new (void)
{
  return (MyObject *) malloc (1);
}

GUM_HOOK_TARGET static void
my_object_free (MyObject * obj)
{
  free (obj);
}

typedef struct _TestCObjectTrackerFixture
{
  GumCObjectTracker * tracker;
  GHashTable * ht1;
  GHashTable * ht2;
  MyObject * mo;
} TestCObjectTrackerFixture;

static void
test_cobject_tracker_fixture_create_tracker (
    TestCObjectTrackerFixture * fixture,
    GumBacktracer * backtracer)
{
  if (backtracer != NULL)
    fixture->tracker = gum_cobject_tracker_new_with_backtracer (backtracer);
  else
    fixture->tracker = gum_cobject_tracker_new ();

  gum_cobject_tracker_track (fixture->tracker,
      "GHashTable", g_hash_table_new_full);
  gum_cobject_tracker_track (fixture->tracker,
      "MyObject", my_object_new);

  gum_cobject_tracker_begin (fixture->tracker);
}

static void
test_cobject_tracker_fixture_enable_backtracer (
    TestCObjectTrackerFixture * fixture)
{
  GumBacktracer * backtracer;

  g_object_unref (fixture->tracker);

  backtracer = gum_backtracer_make_accurate ();
  test_cobject_tracker_fixture_create_tracker (fixture, backtracer);
  g_object_unref (backtracer);
}

static void
test_cobject_tracker_fixture_setup (TestCObjectTrackerFixture * fixture,
                                    gconstpointer data)
{
  test_cobject_tracker_fixture_create_tracker (fixture, NULL);
}

static void
test_cobject_tracker_fixture_teardown (TestCObjectTrackerFixture * fixture,
                                       gconstpointer data)
{
  if (fixture->ht1 != NULL)
    g_hash_table_unref (fixture->ht1);
  if (fixture->ht2 != NULL)
    g_hash_table_unref (fixture->ht2);
  if (fixture->mo != NULL)
    my_object_free (fixture->mo);
  g_object_unref (fixture->tracker);
}

#endif /* HAVE_WINDOWS */
