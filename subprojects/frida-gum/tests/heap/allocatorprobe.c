/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "allocatorprobe-fixture.c"

#ifdef HAVE_WINDOWS

TESTLIST_BEGIN (allocator_probe)
  TESTENTRY (basics)
  TESTENTRY (ignore_gquark)
#ifdef _DEBUG
  TESTENTRY (nonstandard_basics)
  TESTENTRY (nonstandard_ignored)
#endif
  TESTENTRY (full_cycle)
  TESTENTRY (gtype_interop)
TESTLIST_END ()

TESTCASE (basics)
{
  guint malloc_count, realloc_count, free_count;
  gpointer a, b;

  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  a = malloc (314);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);
  free (a);

  ATTACH_PROBE ();

  a = malloc (42);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 1);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  b = calloc (1, 48);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 2);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  a = realloc (a, 84);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 2);
  g_assert_cmpuint (realloc_count, ==, 1);
  g_assert_cmpuint (free_count, ==, 0);

  free (b);
  free (a);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 2);
  g_assert_cmpuint (realloc_count, ==, 1);
  g_assert_cmpuint (free_count, ==, 2);

  DETACH_PROBE ();

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);
}

TESTCASE (ignore_gquark)
{
  guint malloc_count, realloc_count, free_count;

  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);
  ATTACH_PROBE ();

  g_quark_from_static_string ("gumtestquark1");
  g_quark_from_string ("gumtestquark2");

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  DETACH_PROBE ();
}

#ifdef _DEBUG

#include <crtdbg.h>

TESTCASE (nonstandard_basics)
{
  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);

  ATTACH_PROBE ();
  do_nonstandard_heap_calls (fixture, _NORMAL_BLOCK, 1);
  DETACH_PROBE ();
}

TESTCASE (nonstandard_ignored)
{
  g_object_set (fixture->ap, "enable-counters", TRUE, NULL);
  ATTACH_PROBE ();

  do_nonstandard_heap_calls (fixture, _CRT_BLOCK, 0);

  DETACH_PROBE ();
}

#endif

TESTCASE (full_cycle)
{
  GumAllocationTracker * t;
  gpointer a, b;

  t = gum_allocation_tracker_new ();
  gum_allocation_tracker_begin (t);

  g_object_set (fixture->ap, "allocation-tracker", t, NULL);

  ATTACH_PROBE ();

  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);

  a = malloc (24);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 24);

  b = calloc (2, 42);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 108);

  a = realloc (a, 40);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 124);

  free (a);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 84);

  free (b);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);

  g_object_unref (t);
}

/*
 * Turns out that doing any GType lookups from within the context where
 * malloc() or similar is being called can be dangerous, as the caller
 * might be from within GType itself. The caller could hold a lock that
 * we try to reacquire by re-entering into GType, which is bad.
 *
 * We circumvent such issues by storing away as much as possible, which
 * also improves performance.
 *
 * FIXME: This test covers both AllocatorProbe and Interceptor, so the
 *        latter should obviously also have a test covering its own layer.
 */
TESTCASE (gtype_interop)
{
  MyPony * pony;

  ATTACH_PROBE ();

  pony = g_object_new (MY_TYPE_PONY, NULL);
  g_object_unref (pony);
}

#ifdef _DEBUG

static void
do_nonstandard_heap_calls (TestAllocatorProbeFixture * fixture,
                           gint block_type,
                           gint factor)
{
  guint malloc_count, realloc_count, free_count;
  gpointer a, b;

  a = _malloc_dbg (42, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 1 * factor);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count,    ==, 0);

  b = _calloc_dbg (1, 48, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count,    ==, 0);

  a = _realloc_dbg (a, 84, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 1 * factor);
  g_assert_cmpuint (free_count,    ==, 0);

  b = _recalloc_dbg (b, 2, 48, block_type, __FILE__, __LINE__);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 2 * factor);
  g_assert_cmpuint (free_count,    ==, 0);

  _free_dbg (b, block_type);
  _free_dbg (a, block_type);
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count,  ==, 2 * factor);
  g_assert_cmpuint (realloc_count, ==, 2 * factor);
  g_assert_cmpuint (free_count,    ==, 2 * factor);
}

#endif /* _DEBUG */

#endif /* HAVE_WINDOWS */
