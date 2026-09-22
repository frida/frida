/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "pagepool-fixture.c"

TESTLIST_BEGIN (pagepool)
  TESTENTRY (alloc_sizes)
  TESTENTRY (alloc_alignment)
  TESTENTRY (alloc_protection)
  TESTENTRY (free)
  TESTENTRY (free_protection)
  TESTENTRY (query_block_details)
  TESTENTRY (peek_used)
  TESTENTRY (alloc_and_fill_full_cycle)
TESTLIST_END ()

TESTCASE (alloc_sizes)
{
  GumPagePool * pool;
  guint page_size;
  gpointer p1, p2;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 4);
  g_object_get (pool, "page-size", &page_size, NULL);

  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 4);

  p1 = gum_page_pool_try_alloc (pool, 1);
  g_assert_nonnull (p1);
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 2);

  p2 = gum_page_pool_try_alloc (pool, page_size + 1);
  g_assert_null (p2);
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 2);
  p2 = gum_page_pool_try_alloc (pool, page_size);
  g_assert_nonnull (p2);
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 0);
}

TESTCASE (alloc_alignment)
{
  GumPagePool * pool;
  guint page_size;
  guint8 * start, * end;
  guint8 * p1, * p2;
  guint8 * expected_p1, * expected_p2;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 4);
  g_object_get (pool, "page-size", &page_size, NULL);
  gum_page_pool_get_bounds (pool, &start, &end);

  p1 = gum_page_pool_try_alloc (pool, 1);
  p2 = gum_page_pool_try_alloc (pool, 17);
  g_assert_cmphex (GPOINTER_TO_SIZE (p1), !=, GPOINTER_TO_SIZE (p2));

  expected_p1 = start + page_size - 16;
  g_assert_cmphex (GPOINTER_TO_SIZE (p1), ==, GPOINTER_TO_SIZE (expected_p1));

  expected_p2 = start + (2 * page_size) + page_size - 32;
  g_assert_cmphex (GPOINTER_TO_SIZE (p2), ==, GPOINTER_TO_SIZE (expected_p2));
}

TESTCASE (alloc_protection)
{
  GumPagePool * pool;
  guint8 * p1, * p2;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 4);

  p1 = gum_page_pool_try_alloc (pool, 1);
  g_assert_true (gum_memory_is_readable (p1, 16));
  g_assert_false (gum_memory_is_readable (p1 + 16, 1));

  p2 = gum_page_pool_try_alloc (pool, 17);
  g_assert_true (gum_memory_is_readable (p2, 32));
  g_assert_false (gum_memory_is_readable (p2 + 32, 1));
}

TESTCASE (free)
{
  GumPagePool * pool;
  guint page_size;
  guint8 * p1, * p2;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 5);
  g_object_get (pool, "page-size", &page_size, NULL);

  g_assert_false (gum_page_pool_try_free (pool, GSIZE_TO_POINTER (1)));

  p1 = gum_page_pool_try_alloc (pool, page_size + 1);
  p2 = gum_page_pool_try_alloc (pool, 1);
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 0);

  g_assert_true (gum_page_pool_try_free (pool, p1));
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 3);
  p1 = gum_page_pool_try_alloc (pool, page_size + 1);
  g_assert_nonnull (p1);
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 0);

  g_assert_true (gum_page_pool_try_free (pool, p2));
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 2);
  p2 = gum_page_pool_try_alloc (pool, 1);
  g_assert_nonnull (p2);
  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 0);
}

TESTCASE (free_protection)
{
  GumPagePool * pool;
  guint8 * p;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 4);

  p = gum_page_pool_try_alloc (pool, 1);
  g_assert_true (gum_page_pool_try_free (pool, p));
  g_assert_false (gum_memory_is_readable (p, 16));
  g_assert_false (gum_memory_is_readable (p + 16, 1));
}

TESTCASE (query_block_details)
{
  GumPagePool * pool;
  guint page_size, size;
  GumBlockDetails details;
  guint8 * p;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 3);

  g_object_get (pool, "page-size", &page_size, NULL);

  g_assert_false (gum_page_pool_query_block_details (pool, GSIZE_TO_POINTER (1),
      &details));
  size = page_size + 1;
  p = (guint8 *) gum_page_pool_try_alloc (pool, size);

  g_assert_true (gum_page_pool_query_block_details (pool, p, &details));
  g_assert_cmphex (GPOINTER_TO_SIZE (details.address),
      ==, GPOINTER_TO_SIZE (p));
  g_assert_cmpuint (details.size, ==, size);
  g_assert_true (details.allocated);

  g_assert_true (gum_page_pool_query_block_details (pool, p + 1, &details));
  g_assert_cmphex (GPOINTER_TO_SIZE (details.address),
      ==, GPOINTER_TO_SIZE (p));
  g_assert_cmpuint (details.size, ==, size);
  g_assert_true (details.allocated);

  g_assert_true (gum_page_pool_query_block_details (pool, p + size - 1,
      &details));
  g_assert_cmphex (GPOINTER_TO_SIZE (details.address),
      ==, GPOINTER_TO_SIZE (p));
  g_assert_cmpuint (details.size, ==, size);
  g_assert_true (details.allocated);

  gum_page_pool_try_free (pool, p);

  g_assert_true (gum_page_pool_query_block_details (pool, p, &details));
  g_assert_cmphex (GPOINTER_TO_SIZE (details.address),
      ==, GPOINTER_TO_SIZE (p));
  g_assert_cmpuint (details.size, ==, size);
  g_assert_false (details.allocated);
}

TESTCASE (peek_used)
{
  GumPagePool * pool;
  guint8 * p;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, 2);

  g_assert_cmpuint (gum_page_pool_peek_used (pool), ==, 0);
  p = (guint8 *) gum_page_pool_try_alloc (pool, 1337);
  g_assert_cmpuint (gum_page_pool_peek_used (pool), ==, 2);
  gum_page_pool_try_free (pool, p);
  g_assert_cmpuint (gum_page_pool_peek_used (pool), ==, 0);
}

TESTCASE (alloc_and_fill_full_cycle)
{
  guint page_size, pool_size;
  GumPagePool * pool;
  guint8 * start, * end;
  guint8 * p;
  guint i;
  guint buffer_size;

  page_size = gum_query_page_size ();
  buffer_size = (3 * page_size) + 1;
  pool_size = (buffer_size / page_size) + 1;
  if (buffer_size % page_size != 0)
    pool_size++;

  SETUP_POOL (&pool, GUM_PROTECT_MODE_ABOVE, pool_size);
  gum_page_pool_get_bounds (pool, &start, &end);

  p = (guint8 *) gum_page_pool_try_alloc (pool, buffer_size);
  g_assert_nonnull (p);

  g_assert_cmpuint (gum_page_pool_peek_available (pool), ==, 0);

  g_assert_cmpuint (p - start, ==, page_size - 16);
  g_assert_cmpuint ((end - (p + buffer_size)) - page_size, ==, 15);

  for (i = 0; i < pool_size - 1; i++)
  {
    g_assert_true (gum_memory_is_readable (start + (i * page_size), page_size));
  }
  g_assert_false (gum_memory_is_readable (end - page_size, page_size));

  memset (p, 0, buffer_size);
}
