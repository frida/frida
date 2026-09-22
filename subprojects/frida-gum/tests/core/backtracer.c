/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "backtracer-fixture.c"

#define PRINT_BACKTRACES        0
#define ENABLE_PERFORMANCE_TEST 0

TESTLIST_BEGIN (backtracer)
  TESTENTRY (basics)
  TESTENTRY (full_cycle_with_interceptor)
  TESTENTRY (full_cycle_with_allocation_tracker)
#if ENABLE_PERFORMANCE_TEST
  TESTENTRY (performance)
#endif
TESTLIST_END ()

#if PRINT_BACKTRACES
static void print_backtrace (GumReturnAddressArray * ret_addrs);
#endif

TESTCASE (basics)
{
  GumReturnAddressArray ret_addrs = { 0, };
  G_GNUC_UNUSED guint expected_line_number;
  GumReturnAddress first_address;
  GumReturnAddressDetails rad;

  expected_line_number = __LINE__ + 8;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  gum_backtracer_generate (fixture->backtracer, NULL, &ret_addrs);
  g_assert_cmpuint (ret_addrs.len, >=, 2);

#if PRINT_BACKTRACES
  print_backtrace (&ret_addrs);
#endif

  first_address = ret_addrs.items[0];
  g_assert_nonnull (first_address);

  g_assert_true (gum_return_address_details_from_address (first_address, &rad));
  g_assert_true (g_str_has_prefix (rad.module_name, "gum-tests") ||
      g_str_has_prefix (rad.module_name, "lt-gum-tests"));
  g_assert_cmpstr (rad.function_name, ==, __FUNCTION__);
#ifndef HAVE_DARWIN
  g_assert_true (g_str_has_suffix (rad.file_name, "backtracer.c"));
  g_assert_true (rad.line_number == expected_line_number ||
      rad.line_number == expected_line_number + 1);
# if !(defined (HAVE_WINDOWS) && defined (HAVE_ARM64))
  g_assert_cmpuint (rad.column, ==, 3);
# endif
#endif
}

TESTCASE (full_cycle_with_interceptor)
{
  GumInterceptor * interceptor;
  BacktraceCollector * collector;
  int (* open_impl) (const char * path, int oflag, ...);
  int (* close_impl) (int fd);
  int fd;
  GumReturnAddressDetails on_enter, on_leave;

  interceptor = gum_interceptor_obtain ();
  collector = backtrace_collector_new_with_backtracer (fixture->backtracer);

#ifdef HAVE_WINDOWS
  open_impl = _open;
  close_impl = _close;
#else
  open_impl = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (gum_process_get_libc_module (), "open"));
  close_impl = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (gum_process_get_libc_module (), "close"));
#endif

  gum_interceptor_attach (interceptor, open_impl,
      GUM_INVOCATION_LISTENER (collector), NULL);

  g_assert_cmpuint (collector->last_on_enter.len, ==, 0);
  g_assert_cmpuint (collector->last_on_leave.len, ==, 0);
  fd = open_impl ("badger.txt", O_RDONLY);
  g_assert_cmpuint (collector->last_on_enter.len, !=, 0);
  g_assert_cmpuint (collector->last_on_leave.len, !=, 0);

  gum_interceptor_detach (interceptor, GUM_INVOCATION_LISTENER (collector));

  if (fd != -1)
    close_impl (fd);

#if PRINT_BACKTRACES
  g_print ("\n\n*** on_enter:");
  print_backtrace (&collector->last_on_enter);

  g_print ("*** on_leave:");
  print_backtrace (&collector->last_on_leave);
#endif

  g_assert_true (gum_return_address_details_from_address (
      collector->last_on_enter.items[0], &on_enter));
  g_assert_true (g_str_has_prefix (on_enter.function_name, __FUNCTION__));

  g_assert_true (gum_return_address_details_from_address (
      collector->last_on_leave.items[0], &on_leave));
  g_assert_true (g_str_has_prefix (on_leave.function_name, __FUNCTION__));

  g_object_unref (collector);
  g_object_unref (interceptor);
}

TESTCASE (full_cycle_with_allocation_tracker)
{
  const GumHeapApiList * heap_apis;
  const GumHeapApi * api;
  GumAllocatorProbe * probe;
  GumAllocationTracker * tracker;
  GumInterceptor * interceptor;
  guint expected_line_number, alternate_line_number;
  volatile gpointer a;
  GList * blocks;
  GumAllocationBlock * block;
  GumReturnAddress first_address;

  if (RUNNING_ON_VALGRIND)
  {
    g_print ("<skipping, not compatible with Valgrind> ");
    return;
  }

  heap_apis = test_util_heap_apis ();
  api = gum_heap_api_list_get_nth (heap_apis, 0);

  tracker = gum_allocation_tracker_new_with_backtracer (fixture->backtracer);
  gum_allocation_tracker_begin (tracker);

  probe = gum_allocator_probe_new ();
  g_object_set (probe, "allocation-tracker", tracker, NULL);
  interceptor = gum_interceptor_obtain ();
  gum_interceptor_ignore_other_threads (interceptor);
  gum_allocator_probe_attach_to_apis (probe, heap_apis);

  expected_line_number = __LINE__ + 1;
  a = api->malloc (1337);

  /* TODO: Remove this once reentrancy protection has been implemented to also
   *       cover AllocationTracker's methods */
  alternate_line_number = __LINE__ + 1;
  gum_allocator_probe_detach (probe);

  blocks = gum_allocation_tracker_peek_block_list (tracker);
  g_assert_cmpuint (g_list_length (blocks), ==, 1);

  block = (GumAllocationBlock *) blocks->data;

#if PRINT_BACKTRACES
  print_backtrace (&block->return_addresses);
#endif

  g_assert_cmpuint (block->return_addresses.len, >=, 1);

  first_address = block->return_addresses.items[0];
  g_assert_nonnull (first_address);

  {
#ifdef HAVE_WINDOWS
    GumReturnAddressDetails rad;

    g_assert_true (gum_return_address_details_from_address (first_address,
        &rad));
    g_assert_true (g_str_has_prefix (rad.module_name, "gum-tests"));
    g_assert_cmpstr (rad.function_name, ==, __FUNCTION__);
    g_assert_true (g_str_has_suffix (rad.file_name, "backtracer.c"));
    if (rad.line_number != alternate_line_number)
      g_assert_cmpuint (rad.line_number, ==, expected_line_number);
#else
    g_assert_nonnull (first_address);
    (void) expected_line_number;
    (void) alternate_line_number;
#endif
  }

  gum_allocation_block_list_free (blocks);

  api->free (a);

  gum_interceptor_unignore_other_threads (interceptor);
  g_object_unref (interceptor);
  g_object_unref (probe);
  g_object_unref (tracker);
}

#if ENABLE_PERFORMANCE_TEST

TESTCASE (performance)
{
  GumReturnAddressArray ret_addrs = { 0, };
  GTimer * timer;
  guint count = 0;

  timer = g_timer_new ();

  do
  {
    guint i;

    for (i = 0; i < 100; i++)
    {
      gum_backtracer_generate (fixture->backtracer, NULL, &ret_addrs);
      ret_addrs.len = 0;
    }

    count += 100;
  }
  while (g_timer_elapsed (timer, NULL) < 1.0);

  g_print ("(%d backtraces per second) ", count);

  g_timer_destroy (timer);
}

#endif

#if PRINT_BACKTRACES

static void
print_backtrace (GumReturnAddressArray * ret_addrs)
{
  guint i;

  g_print ("\n\nBacktrace (%d return addresses):\n", ret_addrs->len);

  for (i = 0; i != ret_addrs->len; i++)
  {
    GumReturnAddress * ra = ret_addrs->items[i];
    GumReturnAddressDetails rad;

    if (gum_return_address_details_from_address (ra, &rad))
    {
      g_print ("  %p %s!%s %s:%d:%d\n", rad.address, rad.module_name,
          rad.function_name, rad.file_name, rad.line_number, rad.column);
    }
    else
    {
      g_print ("  %p <unknown>\n", ra);
    }
  }

  g_print ("\n\n");
}

#endif
