/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __FAKE_EVENT_SINK_H__
#define __FAKE_EVENT_SINK_H__

#include <glib-object.h>
#include <gum/gum.h>
#include <gum/gummetalarray.h>

G_BEGIN_DECLS

#define GUM_TYPE_FAKE_EVENT_SINK (gum_fake_event_sink_get_type ())
G_DECLARE_FINAL_TYPE (GumFakeEventSink, gum_fake_event_sink, GUM,
    FAKE_EVENT_SINK, GObject)

struct _GumFakeEventSink
{
  GObject parent;

  GumEventType mask;
  /*
   * Page-backed (GumMetalArray uses gum_memory_allocate()), so appending an
   * event never re-enters the heap allocator. The sink's process() runs on
   * the stalked thread, which may be executing instrumented allocator code
   * while holding the heap lock -- a g_array_append() there would deadlock.
   */
  GumMetalArray * events;
};

GumEventSink * gum_fake_event_sink_new (void);

void gum_fake_event_sink_reset (GumFakeEventSink * self);

const GumCallEvent * gum_fake_event_sink_get_nth_event_as_call (
    GumFakeEventSink * self, guint n);
const GumRetEvent * gum_fake_event_sink_get_nth_event_as_ret (
    GumFakeEventSink * self, guint n);
const GumExecEvent * gum_fake_event_sink_get_nth_event_as_exec (
    GumFakeEventSink * self, guint n);

void gum_fake_event_sink_dump (GumFakeEventSink * self);

G_END_DECLS

#endif
