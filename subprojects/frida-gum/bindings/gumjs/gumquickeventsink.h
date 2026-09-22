/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_EVENT_SINK_H__
#define __GUM_QUICK_EVENT_SINK_H__

#include "gumquickcore.h"

#include <gum/gumeventsink.h>

G_BEGIN_DECLS

#define GUM_QUICK_TYPE_JS_EVENT_SINK (gum_quick_js_event_sink_get_type ())
#define GUM_QUICK_JS_EVENT_SINK_CAST(obj) ((GumQuickJSEventSink *) (obj))
G_DECLARE_FINAL_TYPE (GumQuickJSEventSink, gum_quick_js_event_sink, GUM_QUICK,
    JS_EVENT_SINK, GObject)

#define GUM_QUICK_TYPE_NATIVE_EVENT_SINK \
    (gum_quick_native_event_sink_get_type ())
#define GUM_QUICK_NATIVE_EVENT_SINK_CAST(obj) \
    ((GumQuickNativeEventSink *) (obj))
G_DECLARE_FINAL_TYPE (GumQuickNativeEventSink, gum_quick_native_event_sink,
    GUM_QUICK, NATIVE_EVENT_SINK, GObject)

typedef struct _GumQuickEventSinkOptions GumQuickEventSinkOptions;

typedef void (* GumQuickOnEvent) (const GumEvent * event,
    GumCpuContext * cpu_context, gpointer user_data);

struct _GumQuickEventSinkOptions
{
  GumQuickCore * core;
  GMainContext * main_context;

  GumEventType event_mask;

  guint queue_capacity;
  guint queue_drain_interval;
  JSValue on_receive;
  JSValue on_call_summary;

  GumQuickOnEvent on_event;
  gpointer user_data;
};

G_GNUC_INTERNAL GumEventSink * gum_quick_event_sink_new (JSContext * ctx,
    const GumQuickEventSinkOptions * options);

G_END_DECLS

#endif
