/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "allocatorprobe-fixture.c"

#ifdef HAVE_WINDOWS

G_BEGIN_DECLS

TESTLIST_BEGIN (allocator_probe_cxx)
  TESTENTRY (new_delete)
  TESTENTRY (concurrency)
TESTLIST_END ()

static gpointer concurrency_torture_helper (gpointer data);

TESTCASE (new_delete)
{
  guint malloc_count, realloc_count, free_count;
  int * a;

  g_object_set (fixture->ap, "enable-counters", TRUE, (void *) NULL);

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  a = new int;
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  delete a;

  ATTACH_PROBE ();

  a = new int;
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 1);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);

  delete a;
  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 1);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 1);

  DETACH_PROBE ();

  READ_PROBE_COUNTERS ();
  g_assert_cmpuint (malloc_count, ==, 0);
  g_assert_cmpuint (realloc_count, ==, 0);
  g_assert_cmpuint (free_count, ==, 0);
}

TESTCASE (concurrency)
{
  gum_interceptor_unignore_other_threads (fixture->interceptor);

  gum_allocator_probe_attach (fixture->ap);

  GThread * thread = g_thread_new ("allocatorprobex-test-concurrency",
      concurrency_torture_helper, NULL);

  g_thread_yield ();

  for (int i = 0; i < 2000; i++)
  {
    int * a = new int;
    delete a;
  }

  g_thread_join (thread);

  gum_interceptor_ignore_other_threads (fixture->interceptor);
}

static gpointer
concurrency_torture_helper (gpointer data)
{
  for (int i = 0; i < 2000; i++)
  {
    void * b = malloc (1);
    free (b);
  }

  return NULL;
}

G_END_DECLS

#endif /* HAVE_WINDOWS */
