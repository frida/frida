/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumbacktracer.h"

#include "testutil.h"
#include "valgrind.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_WINDOWS
# include <io.h>
#endif
#ifdef G_OS_UNIX
# include <unistd.h>
#endif

#define TESTCASE(NAME) \
    void test_backtracer_ ## NAME ( \
        TestBacktracerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Backtracer", test_backtracer, NAME, \
        TestBacktracerFixture)

#define GUM_TEST_TYPE_BACKTRACE_COLLECTOR (backtrace_collector_get_type ())
G_DECLARE_FINAL_TYPE (BacktraceCollector, backtrace_collector, GUM_TEST,
    BACKTRACE_COLLECTOR, GObject)

typedef struct _TestBacktracerFixture TestBacktracerFixture;

struct _TestBacktracerFixture
{
  GumBacktracer * backtracer;
};

struct _BacktraceCollector
{
  GObject parent;

  GumBacktracer * backtracer;

  GumReturnAddressArray last_on_enter;
  GumReturnAddressArray last_on_leave;
};

static void backtrace_collector_iface_init (gpointer g_iface,
    gpointer iface_data);
static void backtrace_collector_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void backtrace_collector_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);

G_DEFINE_TYPE_EXTENDED (BacktraceCollector,
                        backtrace_collector,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            backtrace_collector_iface_init))

static void
test_backtracer_fixture_setup (TestBacktracerFixture * fixture,
                               gconstpointer data)
{
  fixture->backtracer = gum_backtracer_make_accurate ();
}

static void
test_backtracer_fixture_teardown (TestBacktracerFixture * fixture,
                                  gconstpointer data)
{
  if (fixture->backtracer != NULL)
    g_object_unref (fixture->backtracer);
}

static BacktraceCollector *
backtrace_collector_new_with_backtracer (GumBacktracer * backtracer)
{
  BacktraceCollector * collector;

  collector = g_object_new (GUM_TEST_TYPE_BACKTRACE_COLLECTOR, NULL);
  collector->backtracer = backtracer;

  return collector;
}

static void
backtrace_collector_class_init (BacktraceCollectorClass * klass)
{
}

static void
backtrace_collector_iface_init (gpointer g_iface,
                                gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = backtrace_collector_on_enter;
  iface->on_leave = backtrace_collector_on_leave;
}

static void
backtrace_collector_init (BacktraceCollector * self)
{
}

static void
backtrace_collector_on_enter (GumInvocationListener * listener,
                              GumInvocationContext * context)
{
  BacktraceCollector * self = (BacktraceCollector *) listener;

  gum_backtracer_generate (self->backtracer, context->cpu_context,
      &self->last_on_enter);
}

static void
backtrace_collector_on_leave (GumInvocationListener * listener,
                              GumInvocationContext * context)
{
  BacktraceCollector * self = (BacktraceCollector *) listener;

  gum_backtracer_generate (self->backtracer, context->cpu_context,
      &self->last_on_leave);
}
