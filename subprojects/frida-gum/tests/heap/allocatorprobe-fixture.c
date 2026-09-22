/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumallocatorprobe.h"

#ifdef HAVE_WINDOWS

#include "dummyclasses.h"
#include "testutil.h"

#include <stdlib.h>

#define TESTCASE(NAME) \
    void test_allocator_probe_ ## NAME (TestAllocatorProbeFixture * fixture, \
        gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Heap/AllocatorProbe", test_allocator_probe, \
        NAME, TestAllocatorProbeFixture)

typedef struct _TestAllocatorProbeFixture
{
  GumAllocatorProbe * ap;
  GumInterceptor * interceptor;
} TestAllocatorProbeFixture;

static void
test_allocator_probe_fixture_setup (TestAllocatorProbeFixture * fixture,
                                    gconstpointer data)
{
  fixture->ap = gum_allocator_probe_new ();

  fixture->interceptor = gum_interceptor_obtain ();
  gum_interceptor_ignore_other_threads (fixture->interceptor);
}

static void
test_allocator_probe_fixture_teardown (TestAllocatorProbeFixture * fixture,
                                       gconstpointer data)
{
  gum_interceptor_unignore_other_threads (fixture->interceptor);
  g_object_unref (fixture->interceptor);

  g_object_unref (fixture->ap);
}

#define ATTACH_PROBE() \
    gum_allocator_probe_attach_to_apis (fixture->ap, test_util_heap_apis ())
#define DETACH_PROBE() \
    gum_allocator_probe_detach (fixture->ap)
#define READ_PROBE_COUNTERS()            \
    g_object_get (fixture->ap,           \
        "malloc-count", &malloc_count,   \
        "realloc-count", &realloc_count, \
        "free-count", &free_count,       \
        NULL);

G_BEGIN_DECLS

#if defined (HAVE_WINDOWS) && defined (_DEBUG)
static void do_nonstandard_heap_calls (TestAllocatorProbeFixture * fixture,
    gint block_type, gint factor);
#endif

G_END_DECLS

#endif /* HAVE_WINDOWS */
