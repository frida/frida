/*
 * Copyright (C) 2009-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_EVENT_SINK_H__
#define __GUM_EVENT_SINK_H__

#include <gum/gumdefs.h>
#include <gum/gumevent.h>

G_BEGIN_DECLS

#define GUM_TYPE_EVENT_SINK (gum_event_sink_get_type ())
G_DECLARE_INTERFACE (GumEventSink, gum_event_sink, GUM, EVENT_SINK, GObject)

#define GUM_TYPE_DEFAULT_EVENT_SINK (gum_default_event_sink_get_type ())
G_DECLARE_FINAL_TYPE (GumDefaultEventSink, gum_default_event_sink, GUM,
                      DEFAULT_EVENT_SINK, GObject)

#define GUM_TYPE_CALLBACK_EVENT_SINK (gum_callback_event_sink_get_type ())
G_DECLARE_FINAL_TYPE (GumCallbackEventSink, gum_callback_event_sink, GUM,
                      CALLBACK_EVENT_SINK, GObject)

typedef void (* GumEventSinkCallback) (const GumEvent * event,
    GumCpuContext * cpu_context, gpointer user_data);

struct _GumEventSinkInterface
{
  GTypeInterface parent;

  GumEventType (* query_mask) (GumEventSink * self);
  void (* start) (GumEventSink * self);
  void (* process) (GumEventSink * self, const GumEvent * event,
      GumCpuContext * cpu_context);
  void (* flush) (GumEventSink * self);
  void (* stop) (GumEventSink * self);
};

GUM_API GumEventType gum_event_sink_query_mask (GumEventSink * self);
GUM_API void gum_event_sink_start (GumEventSink * self);
GUM_API void gum_event_sink_process (GumEventSink * self,
    const GumEvent * event, GumCpuContext * cpu_context);
GUM_API void gum_event_sink_flush (GumEventSink * self);
GUM_API void gum_event_sink_stop (GumEventSink * self);

GUM_API GumEventSink * gum_event_sink_make_default (void);
GUM_API GumEventSink * gum_event_sink_make_from_callback (GumEventType mask,
    GumEventSinkCallback callback, gpointer data, GDestroyNotify data_destroy);

G_END_DECLS

#endif
