/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "allocationtracker-fixture.c"

TESTLIST_BEGIN (allocation_tracker)
  TESTENTRY (begin)
  TESTENTRY (end)

  TESTENTRY (block_count)
  TESTENTRY (block_total_size)
  TESTENTRY (block_list_pointers)
  TESTENTRY (block_list_sizes)
  TESTENTRY (block_list_backtraces)
  TESTENTRY (block_groups)

  TESTENTRY (filter_function)

  TESTENTRY (realloc_new_block)
  TESTENTRY (realloc_unknown_block)
  TESTENTRY (realloc_zero_size)
  TESTENTRY (realloc_backtrace)

  TESTENTRY (memory_usage_without_backtracer_should_be_sensible)
  TESTENTRY (memory_usage_with_backtracer_should_be_sensible)

#ifdef HAVE_WINDOWS
  TESTENTRY (backtracer_gtype_interop)

  TESTENTRY (avoid_heap_priv)
  TESTENTRY (avoid_heap_public)
  TESTENTRY (hashtable_resize)
  TESTENTRY (hashtable_life)
#endif
TESTLIST_END ()

TESTCASE (begin)
{
  GumAllocationTracker * t = fixture->tracker;
  GList * blocks, * groups;

  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);
  g_assert_null (gum_allocation_tracker_peek_block_list (t));
  g_assert_null (gum_allocation_tracker_peek_block_groups (t));
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 123);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);
  g_assert_null (gum_allocation_tracker_peek_block_list (t));
  g_assert_null (gum_allocation_tracker_peek_block_groups (t));

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 321);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 321);

  blocks = gum_allocation_tracker_peek_block_list (t);
  g_assert_cmpuint (g_list_length (blocks), ==, 1);
  gum_allocation_block_list_free (blocks);

  groups = gum_allocation_tracker_peek_block_groups (t);
  g_assert_cmpuint (g_list_length (groups), ==, 1);
  gum_allocation_group_list_free (groups);
}

TESTCASE (end)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 321);
  gum_allocation_tracker_end (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 313);

  g_assert_null (gum_allocation_tracker_peek_block_list (t));
  g_assert_null (gum_allocation_tracker_peek_block_groups (t));

  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);
}

TESTCASE (block_count)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 42);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 1337);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_A, DUMMY_BLOCK_A, 84);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);

  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_A);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);

  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_B);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
}

TESTCASE (block_total_size)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);

  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 31);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 31);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 19);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 50);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_A, DUMMY_BLOCK_A, 81);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 100);

  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_A);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 19);

  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_B);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_total_size (t), ==, 0);
}

TESTCASE (block_list_pointers)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 42);
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 24);

  {
    GList * blocks, * cur;

    blocks = gum_allocation_tracker_peek_block_list (t);
    g_assert_cmpuint (g_list_length (blocks), ==, 2);

    for (cur = blocks; cur != NULL; cur = cur->next)
    {
      GumAllocationBlock * block = cur->data;
      g_assert_true (block->address == DUMMY_BLOCK_A ||
          block->address == DUMMY_BLOCK_B);
    }

    gum_allocation_block_list_free (blocks);
  }

  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_A);

  {
    GList * blocks;
    GumAllocationBlock * block;

    blocks = gum_allocation_tracker_peek_block_list (t);
    g_assert_cmpuint (g_list_length (blocks), ==, 1);

    block = blocks->data;
    g_assert_true (block->address == DUMMY_BLOCK_B);

    gum_allocation_block_list_free (blocks);
  }

  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_B);
}

TESTCASE (block_list_sizes)
{
  GumAllocationTracker * t = fixture->tracker;
  GList * blocks, * cur;

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 42);
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 24);

  blocks = gum_allocation_tracker_peek_block_list (t);
  g_assert_cmpuint (g_list_length (blocks), ==, 2);

  for (cur = blocks; cur != NULL; cur = cur->next)
  {
    GumAllocationBlock * block = cur->data;

    if (block->address == DUMMY_BLOCK_A)
      g_assert_cmpuint (block->size, ==, 42);
    else if (block->address == DUMMY_BLOCK_B)
      g_assert_cmpuint (block->size, ==, 24);
    else
      g_assert_not_reached ();
  }

  gum_allocation_block_list_free (blocks);
}

TESTCASE (block_list_backtraces)
{
  GumBacktracer * backtracer;
  GumAllocationTracker * t;
  GList * blocks;
  GumAllocationBlock * block;

  backtracer = gum_fake_backtracer_new (dummy_return_addresses_a,
      G_N_ELEMENTS (dummy_return_addresses_a));
  t = gum_allocation_tracker_new_with_backtracer (backtracer);

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 42);

  blocks = gum_allocation_tracker_peek_block_list (t);
  g_assert_cmpuint (g_list_length (blocks), ==, 1);

  block = (GumAllocationBlock *) blocks->data;
  g_assert_true (block->address == DUMMY_BLOCK_A);

  g_assert_cmpuint (block->return_addresses.len, ==, 2);
  g_assert_true (block->return_addresses.items[0] ==
      dummy_return_addresses_a[0]);
  g_assert_true (block->return_addresses.items[1] ==
      dummy_return_addresses_a[1]);

  gum_allocation_block_list_free (blocks);

  g_object_unref (t);
  g_object_unref (backtracer);
}

TESTCASE (block_groups)
{
  GumAllocationTracker * t = fixture->tracker;
  GList * groups, * cur;

  gum_allocation_tracker_begin (t);

  groups = gum_allocation_tracker_peek_block_groups (t);
  g_assert_cmpuint (g_list_length (groups), ==, 0);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 42);
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 42);
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_C, 42);
  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_C);
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_C, 42);
  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_C);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_D, 1337);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_A, DUMMY_BLOCK_E, 1000);
  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_E);

  groups = gum_allocation_tracker_peek_block_groups (t);
  g_assert_cmpuint (g_list_length (groups), ==, 3);

  for (cur = groups; cur != NULL; cur = cur->next)
  {
    GumAllocationGroup * group = (GumAllocationGroup *) cur->data;

    if (group->size == 42)
    {
      g_assert_cmpuint (group->alive_now, ==, 1);
      g_assert_cmpuint (group->alive_peak, ==, 3);
      g_assert_cmpuint (group->total_peak, ==, 4);
    }
    else if (group->size == 1000)
    {
      g_assert_cmpuint (group->alive_now, ==, 0);
      g_assert_cmpuint (group->alive_peak, ==, 1);
      g_assert_cmpuint (group->total_peak, ==, 1);
    }
    else if (group->size == 1337)
    {
      g_assert_cmpuint (group->alive_now, ==, 1);
      g_assert_cmpuint (group->alive_peak, ==, 1);
      g_assert_cmpuint (group->total_peak, ==, 1);
    }
    else
      g_assert_not_reached ();
  }

  gum_allocation_group_list_free (groups);
}

TESTCASE (filter_function)
{
  GumBacktracer * backtracer;
  GumAllocationTracker * t;
  guint counter = 0;

  backtracer = gum_fake_backtracer_new (dummy_return_addresses_a,
      G_N_ELEMENTS (dummy_return_addresses_a));
  t = gum_allocation_tracker_new_with_backtracer (backtracer);

  gum_allocation_tracker_set_filter_function (t, filter_cb, &counter);

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 1337);
  g_assert_cmpuint (counter, ==, 1);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_B, 42);
  g_assert_cmpuint (counter, ==, 2);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_B, DUMMY_BLOCK_C, 84);
  g_assert_cmpuint (counter, ==, 2);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 2);

  g_object_unref (t);
  g_object_unref (backtracer);
}

static gboolean
filter_cb (GumAllocationTracker * tracker,
           gpointer address,
           guint size,
           gpointer user_data)
{
  guint * counter = (guint *) user_data;

  (*counter)++;

  return (size == 1337);
}

TESTCASE (realloc_new_block)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_realloc (t, NULL, DUMMY_BLOCK_A, 1337);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);
}

TESTCASE (realloc_unknown_block)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_A, DUMMY_BLOCK_A, 1337);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
}

TESTCASE (realloc_zero_size)
{
  GumAllocationTracker * t = fixture->tracker;

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_realloc (t, NULL, DUMMY_BLOCK_A, 1337);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 1);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_A, NULL, 0);
  g_assert_cmpuint (gum_allocation_tracker_peek_block_count (t), ==, 0);
}

TESTCASE (realloc_backtrace)
{
  GumBacktracer * backtracer;
  GumAllocationTracker * t;
  GList * blocks_before, * blocks_after;
  GumReturnAddressArray * addrs_before, * addrs_after;

  backtracer = gum_fake_backtracer_new (dummy_return_addresses_a,
      G_N_ELEMENTS (dummy_return_addresses_a));
  t = gum_allocation_tracker_new_with_backtracer (backtracer);

  gum_allocation_tracker_begin (t);

  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 42);

  blocks_before = gum_allocation_tracker_peek_block_list (t);
  addrs_before = &GUM_ALLOCATION_BLOCK (blocks_before->data)->return_addresses;

  GUM_FAKE_BACKTRACER (backtracer)->ret_addrs = dummy_return_addresses_b;
  GUM_FAKE_BACKTRACER (backtracer)->num_ret_addrs =
      G_N_ELEMENTS (dummy_return_addresses_b);

  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_A, DUMMY_BLOCK_A, 84);

  blocks_after = gum_allocation_tracker_peek_block_list (t);
  addrs_after = &GUM_ALLOCATION_BLOCK (blocks_after->data)->return_addresses;

  g_assert_true (gum_return_address_array_is_equal (addrs_before, addrs_after));

  gum_allocation_block_list_free (blocks_before);
  gum_allocation_block_list_free (blocks_after);

  g_object_unref (t);
  g_object_unref (backtracer);
}

TESTCASE (memory_usage_without_backtracer_should_be_sensible)
{
  GumAllocationTracker * t = fixture->tracker;
  const guint num_allocations = 10000;
  guint bytes_before, bytes_after, i, bytes_per_allocation;

  t = gum_allocation_tracker_new ();
  gum_allocation_tracker_begin (t);

  bytes_before = gum_peek_private_memory_usage ();
  for (i = 0; i != num_allocations; i++)
    gum_allocation_tracker_on_malloc (t, GUINT_TO_POINTER (0x50000 + (i * 64)),
        64);
  bytes_after = gum_peek_private_memory_usage ();

  bytes_per_allocation = (bytes_after - bytes_before) / num_allocations;
  g_assert_cmpuint (bytes_per_allocation, <=, 50);

  g_object_unref (t);
}

TESTCASE (memory_usage_with_backtracer_should_be_sensible)
{
  GumBacktracer * backtracer;
  GumAllocationTracker * t;
  const guint num_allocations = 10;
  guint bytes_before, bytes_after, i, bytes_per_allocation;

  backtracer = gum_fake_backtracer_new (dummy_return_addresses_a,
      G_N_ELEMENTS (dummy_return_addresses_a));
  t = gum_allocation_tracker_new_with_backtracer (backtracer);
  gum_allocation_tracker_begin (t);

  bytes_before = gum_peek_private_memory_usage ();
  for (i = 0; i != num_allocations; i++)
    gum_allocation_tracker_on_malloc (t, GUINT_TO_POINTER (0x50000 + (i * 64)),
        64);
  bytes_after = gum_peek_private_memory_usage ();

  bytes_per_allocation = (bytes_after - bytes_before) / num_allocations;
  g_assert_cmpuint (bytes_per_allocation, <=, 128);

  g_object_unref (backtracer);
  g_object_unref (t);
}

#ifdef HAVE_WINDOWS

TESTCASE (backtracer_gtype_interop)
{
  GumBacktracer * backtracer;
  GumAllocationTracker * tracker;
  GumAllocatorProbe * probe;
  ZooZebra * zebra;

  backtracer = gum_backtracer_make_accurate ();
  tracker = gum_allocation_tracker_new_with_backtracer (backtracer);
  gum_allocation_tracker_begin (tracker);

  probe = gum_allocator_probe_new ();
  g_object_set (probe, "allocation-tracker", tracker, NULL);
  gum_allocator_probe_attach (probe);

  zebra = g_object_new (ZOO_TYPE_ZEBRA, NULL);
  g_object_unref (zebra);

  g_object_unref (probe);
  g_object_unref (tracker);
  g_object_unref (backtracer);
}

TESTCASE (avoid_heap_priv)
{
  GumAllocationTracker * t = fixture->tracker;
  GumSampler * heap_access_counter;

  gum_allocation_tracker_begin (t);

  heap_access_counter = heap_access_counter_new ();
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 321);
  gum_allocation_tracker_on_free (t, DUMMY_BLOCK_A);
  gum_allocation_tracker_on_realloc (t, NULL, DUMMY_BLOCK_B, 10);
  gum_allocation_tracker_on_realloc (t, DUMMY_BLOCK_B, DUMMY_BLOCK_C, 20);
  g_assert_cmpint (gum_sampler_sample (heap_access_counter), ==, 0);
  g_object_unref (heap_access_counter);

  gum_allocation_tracker_end (t);
}

TESTCASE (avoid_heap_public)
{
  GumAllocationTracker * t = fixture->tracker;
  GumSampler * heap_access_counter;
  GList * blocks, * groups;

  gum_allocation_tracker_begin (t);

  heap_access_counter = heap_access_counter_new ();
  gum_allocation_tracker_on_malloc (t, DUMMY_BLOCK_A, 321);
  gum_allocation_tracker_on_realloc (t, NULL, DUMMY_BLOCK_B, 10);
  blocks = gum_allocation_tracker_peek_block_list (t);
  gum_allocation_block_list_free (blocks);
  groups = gum_allocation_tracker_peek_block_groups (t);
  gum_allocation_group_list_free (groups);
  g_assert_cmpint (gum_sampler_sample (heap_access_counter), ==, 0);
  g_object_unref (heap_access_counter);

  gum_allocation_tracker_end (t);
}

TESTCASE (hashtable_resize)
{
  GumAllocationTracker * t = fixture->tracker;
  GumSampler * heap_access_counter;
  guint i;

  gum_allocation_tracker_begin (t);

  heap_access_counter = heap_access_counter_new ();
  for (i = 0; i < 100; i++)
  {
    gum_allocation_tracker_on_malloc (t, GUINT_TO_POINTER (0xf00d + i), i + 1);
    g_assert_cmpint (gum_sampler_sample (heap_access_counter), ==, 0);
  }
  g_object_unref (heap_access_counter);

  gum_allocation_tracker_end (t);
}

TESTCASE (hashtable_life)
{
  GumSampler * heap_access_counter;
  GHashTable * hashtable;
  guint i;

  heap_access_counter = heap_access_counter_new ();
  hashtable = g_hash_table_new (NULL, NULL);
  for (i = 0; i < 10000; i++)
  {
    g_hash_table_insert (hashtable, GUINT_TO_POINTER (i + 1),
        GUINT_TO_POINTER (2 * i));
  }
  g_hash_table_unref (hashtable);
  g_assert_cmpint (gum_sampler_sample (heap_access_counter), ==, 0);

  g_object_unref (heap_access_counter);
}

#endif /* HAVE_WINDOWS */
