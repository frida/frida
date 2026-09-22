/*
 * Copyright (C) 2012-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_EVENT_SINK_H__
#define __GUM_V8_EVENT_SINK_H__

#include "gumv8core.h"

#include <gum/gumeventsink.h>

#define GUM_V8_TYPE_JS_EVENT_SINK (gum_v8_js_event_sink_get_type ())
#define GUM_V8_JS_EVENT_SINK_CAST(obj) ((GumV8JSEventSink *) (obj))
G_DECLARE_FINAL_TYPE (GumV8JSEventSink, gum_v8_js_event_sink, GUM_V8,
    JS_EVENT_SINK, GObject)

#define GUM_V8_TYPE_NATIVE_EVENT_SINK (gum_v8_native_event_sink_get_type ())
#define GUM_V8_NATIVE_EVENT_SINK_CAST(obj) ((GumV8NativeEventSink *) (obj))
G_DECLARE_FINAL_TYPE (GumV8NativeEventSink, gum_v8_native_event_sink, GUM_V8,
    NATIVE_EVENT_SINK, GObject)

typedef void (* GumV8OnEvent) (const GumEvent * event,
    GumCpuContext * cpu_context, gpointer user_data);

struct GumV8EventSinkOptions
{
  GumV8Core * core;
  GMainContext * main_context;

  GumEventType event_mask;

  guint queue_capacity;
  guint queue_drain_interval;
  v8::Local<v8::Function> on_receive;
  v8::Local<v8::Function> on_call_summary;

  GumV8OnEvent on_event;
  gpointer user_data;
};

G_GNUC_INTERNAL GumEventSink * gum_v8_event_sink_new (
    const GumV8EventSinkOptions * options);

#endif
