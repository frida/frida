/*
 * Copyright (C) 2010-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemoryaccessmonitor.h"

#include "testutil.h"

#define TESTCASE(NAME) \
    void test_memory_access_monitor_ ## NAME (TestMAMonitorFixture * fixture, \
        gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/MemoryAccessMonitor", \
        test_memory_access_monitor, NAME, TestMAMonitorFixture)

typedef struct _TestMAMonitorFixture
{
  GumMemoryAccessMonitor * monitor;

  GumMemoryRange range;
  guint offset_in_first_page;
  guint offset_in_second_page;
  GCallback nop_function_in_third_page;

  volatile guint number_of_notifies;
  volatile GumMemoryAccessDetails last_details;
} TestMAMonitorFixture;

static void put_return_instruction (gpointer mem, gpointer user_data);

static void
test_memory_access_monitor_fixture_setup (TestMAMonitorFixture * fixture,
                                          gconstpointer data)
{
  guint page_size, slab_size;
  gpointer slab, nop_func;

  page_size = gum_query_page_size ();

  slab_size = 3 * page_size;
  slab = gum_memory_allocate (NULL, slab_size, page_size, GUM_PAGE_RW);

  fixture->range.base_address = GUM_ADDRESS (slab);
  fixture->range.size = slab_size;

  fixture->offset_in_first_page = page_size / 2;
  fixture->offset_in_second_page = fixture->offset_in_first_page + page_size;

  nop_func = (guint8 *) slab + (2 * page_size);
  gum_memory_patch_code (nop_func, 4, put_return_instruction, NULL);
  fixture->nop_function_in_third_page = GUM_POINTER_TO_FUNCPTR (GCallback,
      gum_sign_code_pointer (nop_func));

  fixture->number_of_notifies = 0;

  fixture->monitor = NULL;
}

static void
test_memory_access_monitor_fixture_teardown (TestMAMonitorFixture * fixture,
                                             gconstpointer data)
{
  if (fixture->monitor != NULL)
    g_object_unref (fixture->monitor);

  g_free (fixture->last_details.context);

  gum_memory_free (GSIZE_TO_POINTER (fixture->range.base_address),
      fixture->range.size);
}

static void
put_return_instruction (gpointer mem,
                        gpointer user_data)
{
#if defined (HAVE_I386)
  *((guint8 *) mem) = 0xc3;
#elif defined (HAVE_ARM)
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* mov pc, lr */
  *((guint32 *) mem) = 0xe1a0f00e;
#else
  *((guint32 *) mem) = 0x0ef0a0e1;
#endif
#elif defined (HAVE_ARM64)
  *((guint32 *) mem) = GUINT32_TO_LE (0xd65f03c0);
#endif
}

static void
memory_access_notify_cb (GumMemoryAccessMonitor * monitor,
                         const GumMemoryAccessDetails * details,
                         gpointer user_data)
{
  TestMAMonitorFixture * fixture = (TestMAMonitorFixture *) user_data;

  fixture->number_of_notifies++;
  g_free (fixture->last_details.context);
  fixture->last_details = *details;
  fixture->last_details.context =
      g_memdup2 (details->context, sizeof (GumCpuContext));
}

#define ENABLE_MONITOR() \
    g_assert_null (fixture->monitor); \
    fixture->monitor = gum_memory_access_monitor_new (&fixture->range, 1, \
        GUM_PAGE_RWX, TRUE, memory_access_notify_cb, fixture, NULL); \
    g_assert_nonnull (fixture->monitor); \
    g_assert_true (gum_memory_access_monitor_enable (fixture->monitor, NULL)); \
    g_assert_cmpuint (fixture->number_of_notifies, ==, 0)
#define DISABLE_MONITOR() \
    gum_memory_access_monitor_disable (fixture->monitor)
