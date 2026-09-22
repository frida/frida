/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "boundschecker-fixture.c"

TESTLIST_BEGIN (boundschecker)
  TESTENTRY (tail_checking_malloc)
  TESTENTRY (tail_checking_calloc)
  TESTENTRY (tail_checking_realloc)
  TESTENTRY (realloc_shrink)
  TESTENTRY (tail_checking_realloc_null)
  TESTENTRY (realloc_migration_pool_to_pool)
  TESTENTRY (realloc_migration_pool_to_heap)
  TESTENTRY (protected_after_free)
  TESTENTRY (calloc_initializes_to_zero)
  TESTENTRY (custom_front_alignment)
#ifndef HAVE_QNX
  TESTENTRY (output_report_on_access_beyond_end)
  TESTENTRY (output_report_on_access_after_free)
#endif
TESTLIST_END ()

TESTCASE (output_report_on_access_beyond_end)
{
  guint8 * p;

  ATTACH_CHECKER ();
  USE_BACKTRACE (malloc_backtrace);
  p = (guint8 *) malloc (16);
  USE_BACKTRACE (violation_backtrace);
  gum_try_read_and_write_at (p, 16, NULL, NULL);
  USE_BACKTRACE (free_backtrace);
  free (p);
  DETACH_CHECKER ();

  assert_same_output (fixture,
      "Oops! Heap block %p of 16 bytes was accessed at offset 16 from:\n"
      "\t%p\n"
      "\t%p\n"
      "Allocated at:\n"
      "\t%p\n"
      "\t%p\n",
      p, violation_backtrace[0], violation_backtrace[1],
      malloc_backtrace[0], malloc_backtrace[1]);
}

TESTCASE (output_report_on_access_after_free)
{
  guint8 * p;

  ATTACH_CHECKER ();
  USE_BACKTRACE (malloc_backtrace);
  p = (guint8 *) malloc (10);
  USE_BACKTRACE (free_backtrace);
  free (p);
  USE_BACKTRACE (violation_backtrace);
  gum_try_read_and_write_at (p, 7, NULL, NULL);
  DETACH_CHECKER ();

  assert_same_output (fixture,
      "Oops! Freed block %p of 10 bytes was accessed at offset 7 from:\n"
      "\t%p\n"
      "\t%p\n"
      "Allocated at:\n"
      "\t%p\n"
      "\t%p\n"
      "Freed at:\n"
      "\t%p\n"
      "\t%p\n",
      p, violation_backtrace[0], violation_backtrace[1],
      malloc_backtrace[0], malloc_backtrace[1],
      free_backtrace[0], free_backtrace[1]);
}

TESTCASE (tail_checking_malloc)
{
  guint8 * a;
  gboolean exception_on_read, exception_on_write;

  ATTACH_CHECKER ();
  a = (guint8 *) malloc (1);
  a[0] = 1;
  gum_try_read_and_write_at (a, 16, &exception_on_read, &exception_on_write);
  free (a);
  DETACH_CHECKER ();

  g_assert_true (exception_on_read && exception_on_write);
}

TESTCASE (tail_checking_calloc)
{
  guint8 * a;
  gboolean exception_on_read, exception_on_write;

  ATTACH_CHECKER ();
  a = calloc (1, 1);
  a[0] = 1;
  gum_try_read_and_write_at (a, 16, &exception_on_read, &exception_on_write);
  free (a);
  DETACH_CHECKER ();

  g_assert_true (exception_on_read && exception_on_write);
}

TESTCASE (tail_checking_realloc)
{
  guint8 * a;
  gboolean exception_on_read, exception_on_write;

  ATTACH_CHECKER ();
  a = (guint8 *) malloc (1);
  a = (guint8 *) realloc (a, 2);
  a[0] = 1;
  gum_try_read_and_write_at (a, 16, &exception_on_read, &exception_on_write);
  free (a);
  DETACH_CHECKER ();

  g_assert_true (exception_on_read && exception_on_write);
}

TESTCASE (realloc_shrink)
{
  guint8 * a;

  ATTACH_CHECKER ();
  a = (guint8 *) malloc (4096);
  a = (guint8 *) realloc (a, 1);
  free (a);
  DETACH_CHECKER ();
}

TESTCASE (tail_checking_realloc_null)
{
  guint8 * a;
  gboolean exception_on_read, exception_on_write;

  ATTACH_CHECKER ();
  a = (guint8 *) realloc (NULL, 1);
  a[0] = 1;
  gum_try_read_and_write_at (a, 16, &exception_on_read, &exception_on_write);
  free (a);
  DETACH_CHECKER ();

  g_assert_true (exception_on_read && exception_on_write);
}

TESTCASE (realloc_migration_pool_to_pool)
{
  guint32 * p;
  guint32 value_after_migration;

  ATTACH_CHECKER ();
  p = (guint32 *) malloc (4);
  *p = 0x1234face;
  p = (guint32 *) realloc (p, 8);
  value_after_migration = *p;
  free (p);
  DETACH_CHECKER ();

  g_assert_cmphex (value_after_migration, ==, 0x1234face);
}

TESTCASE (realloc_migration_pool_to_heap)
{
  guint32 * a;
  guint32 value_after_migration;

  g_object_set (fixture->checker, "pool-size", 2, NULL);

  ATTACH_CHECKER ();
  a = (guint32 *) malloc (4);
  *a = 0x1234face;
  a = (guint32 *) realloc (a, gum_query_page_size () + 1);
  value_after_migration = *a;
  free (a);
  DETACH_CHECKER ();

  g_assert_cmphex (value_after_migration, ==, 0x1234face);
}

TESTCASE (protected_after_free)
{
  guint8 * a;
  gboolean exception_on_read, exception_on_write;

  ATTACH_CHECKER ();
  a = (guint8 *) malloc (1);
  a[0] = 1;
  free (a);
  gum_try_read_and_write_at (a, 0, &exception_on_read, &exception_on_write);
  DETACH_CHECKER ();

  g_assert_true (exception_on_read && exception_on_write);
}

TESTCASE (calloc_initializes_to_zero)
{
  guint8 * p;
  guint8 expected[1024] = { 0, };

  g_object_set (fixture->checker, "pool-size", 2, NULL);

  ATTACH_CHECKER ();
  p = (guint8 *) calloc (1, sizeof (expected));
  memset (p, 0xcc, sizeof (expected));
  free (p);
  p = (guint8 *) calloc (1, sizeof (expected));
  g_assert_cmpint (memcmp (p, expected, sizeof (expected)), ==, 0);
  free (p);
  DETACH_CHECKER ();
}

TESTCASE (custom_front_alignment)
{
  guint8 * a;
  gboolean exception_on_read, exception_on_write;

  g_object_set (fixture->checker, "front-alignment", 1, NULL);
  ATTACH_CHECKER ();
  a = (guint8 *) malloc (1);
  a[0] = 1;
  gum_try_read_and_write_at (a, 1, &exception_on_read, &exception_on_write);
  free (a);
  DETACH_CHECKER ();

  g_assert_true (exception_on_read && exception_on_write);
}
