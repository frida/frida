/*
 * Copyright (C) 2017-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "testutil.h"

#define TESTCASE(NAME) \
    void test_cloak_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/Cloak", test_cloak, NAME)

TESTLIST_BEGIN (cloak)
  TESTENTRY (range_clip_should_not_include_uncloaked)
  TESTENTRY (range_clip_should_handle_full_clip)
  TESTENTRY (range_clip_should_handle_bottom_clip)
  TESTENTRY (range_clip_should_handle_middle_clip)
  TESTENTRY (range_clip_should_handle_top_clip)
  TESTENTRY (full_range_removal_should_impact_clip)
  TESTENTRY (partial_range_removal_should_impact_clip)
TESTLIST_END ()

TESTCASE (range_clip_should_not_include_uncloaked)
{
  gsize page_size;
  gpointer page;
  GumMemoryRange range;

  page_size = gum_query_page_size ();

  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  range.base_address = GUM_ADDRESS (page);
  range.size = page_size;
  g_assert_null (gum_cloak_clip_range (&range));

  gum_memory_free (page, page_size);
}

TESTCASE (range_clip_should_handle_full_clip)
{
  gsize page_size;
  gpointer page;
  GumMemoryRange range;
  GArray * clipped;

  page_size = gum_query_page_size ();

  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  range.base_address = GUM_ADDRESS (page);
  range.size = page_size;
  gum_cloak_add_range (&range);

  clipped = gum_cloak_clip_range (&range);
  g_assert_nonnull (clipped);
  g_assert_cmpuint (clipped->len, ==, 0);
  g_array_free (clipped, TRUE);

  gum_cloak_remove_range (&range);

  gum_memory_free (page, page_size);
}

TESTCASE (range_clip_should_handle_bottom_clip)
{
  gpointer pages;
  guint page_size;
  GumMemoryRange cloaked_range;
  GumMemoryRange full_range;
  GArray * clipped;
  GumMemoryRange * r;

  page_size = gum_query_page_size ();

  pages = gum_memory_allocate (NULL, 2 * page_size, page_size, GUM_PAGE_RW);

  cloaked_range.base_address = GUM_ADDRESS (pages);
  cloaked_range.size = page_size;
  gum_cloak_add_range (&cloaked_range);

  full_range.base_address = GUM_ADDRESS (pages);
  full_range.size = 2 * page_size;
  clipped = gum_cloak_clip_range (&full_range);
  g_assert_nonnull (clipped);
  g_assert_cmpuint (clipped->len, ==, 1);
  r = &g_array_index (clipped, GumMemoryRange, 0);
  g_assert_cmphex (r->base_address, ==, GUM_ADDRESS (pages) + page_size);
  g_assert_cmpuint (r->size, ==, page_size);
  g_array_free (clipped, TRUE);

  gum_cloak_remove_range (&cloaked_range);

  gum_memory_free (pages, 2 * page_size);
}

TESTCASE (range_clip_should_handle_middle_clip)
{
  gpointer pages;
  guint page_size;
  GumMemoryRange cloaked_range;
  GumMemoryRange full_range;
  GArray * clipped;
  GumMemoryRange * r;

  page_size = gum_query_page_size ();

  pages = gum_memory_allocate (NULL, 3 * page_size, page_size, GUM_PAGE_RW);

  cloaked_range.base_address = GUM_ADDRESS (pages) + page_size;
  cloaked_range.size = page_size;
  gum_cloak_add_range (&cloaked_range);

  full_range.base_address = GUM_ADDRESS (pages);
  full_range.size = 3 * page_size;
  clipped = gum_cloak_clip_range (&full_range);
  g_assert_nonnull (clipped);
  g_assert_cmpuint (clipped->len, ==, 2);
  r = &g_array_index (clipped, GumMemoryRange, 0);
  g_assert_cmphex (r->base_address, ==, GUM_ADDRESS (pages));
  g_assert_cmpuint (r->size, ==, page_size);
  r = &g_array_index (clipped, GumMemoryRange, 1);
  g_assert_cmphex (r->base_address, ==, GUM_ADDRESS (pages) + (2 * page_size));
  g_assert_cmpuint (r->size, ==, page_size);
  g_array_free (clipped, TRUE);

  gum_cloak_remove_range (&cloaked_range);

  gum_memory_free (pages, 3 * page_size);
}

TESTCASE (range_clip_should_handle_top_clip)
{
  gpointer pages;
  guint page_size;
  GumMemoryRange cloaked_range;
  GumMemoryRange full_range;
  GArray * clipped;
  GumMemoryRange * r;

  page_size = gum_query_page_size ();

  pages = gum_memory_allocate (NULL, 2 * page_size, page_size, GUM_PAGE_RW);

  cloaked_range.base_address = GUM_ADDRESS (pages) + page_size;
  cloaked_range.size = page_size;
  gum_cloak_add_range (&cloaked_range);

  full_range.base_address = GUM_ADDRESS (pages);
  full_range.size = 2 * page_size;
  clipped = gum_cloak_clip_range (&full_range);
  g_assert_nonnull (clipped);
  g_assert_cmpuint (clipped->len, ==, 1);
  r = &g_array_index (clipped, GumMemoryRange, 0);
  g_assert_cmphex (r->base_address, ==, GUM_ADDRESS (pages));
  g_assert_cmpuint (r->size, ==, page_size);
  g_array_free (clipped, TRUE);

  gum_cloak_remove_range (&cloaked_range);

  gum_memory_free (pages, 2 * page_size);
}

TESTCASE (full_range_removal_should_impact_clip)
{
  gsize page_size;
  gpointer page;
  GumMemoryRange range;

  page_size = gum_query_page_size ();

  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);

  range.base_address = GUM_ADDRESS (page);
  range.size = page_size;

  gum_cloak_add_range (&range);
  gum_cloak_remove_range (&range);

  g_assert_null (gum_cloak_clip_range (&range));

  gum_memory_free (page, page_size);
}

TESTCASE (partial_range_removal_should_impact_clip)
{
  gpointer pages;
  guint page_size;
  GumMemoryRange cloaked_range;
  GumMemoryRange full_range;
  GArray * clipped;
  GumMemoryRange * r;

  page_size = gum_query_page_size ();

  pages = gum_memory_allocate (NULL, 3 * page_size, page_size, GUM_PAGE_RW);

  cloaked_range.base_address = GUM_ADDRESS (pages);
  cloaked_range.size = 3 * page_size;
  gum_cloak_add_range (&cloaked_range);
  cloaked_range.base_address = GUM_ADDRESS (pages);
  cloaked_range.size = page_size;
  gum_cloak_remove_range (&cloaked_range);
  cloaked_range.base_address = GUM_ADDRESS (pages) + (2 * page_size);
  cloaked_range.size = page_size;
  gum_cloak_remove_range (&cloaked_range);

  full_range.base_address = GUM_ADDRESS (pages);
  full_range.size = 3 * page_size;
  clipped = gum_cloak_clip_range (&full_range);
  g_assert_nonnull (clipped);
  g_assert_cmpuint (clipped->len, ==, 2);
  r = &g_array_index (clipped, GumMemoryRange, 0);
  g_assert_cmphex (r->base_address, ==, GUM_ADDRESS (pages));
  g_assert_cmpuint (r->size, ==, page_size);
  r = &g_array_index (clipped, GumMemoryRange, 1);
  g_assert_cmphex (r->base_address, ==, GUM_ADDRESS (pages) + (2 * page_size));
  g_assert_cmpuint (r->size, ==, page_size);
  g_array_free (clipped, TRUE);

  cloaked_range.base_address = GUM_ADDRESS (pages) + page_size;
  cloaked_range.size = page_size;
  gum_cloak_remove_range (&cloaked_range);

  clipped = gum_cloak_clip_range (&full_range);
  g_assert_null (clipped);

  gum_memory_free (pages, 3 * page_size);
}
