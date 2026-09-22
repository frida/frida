/*
 * Copyright (C) 2020-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Alex Soler <asoler@nowsecure.com>
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickeventsink.h"

#include "gumquickvalue.h"

#include <glib/gprintf.h>
#include <gum/gumspinlock.h>
#include <string.h>

struct _GumQuickJSEventSink
{
  GObject parent;

  GumSpinlock lock;
  GArray * queue;
  guint queue_capacity;
  guint queue_drain_interval;

  GumQuickCore * core;
  GMainContext * main_context;
  GumEventType event_mask;
  JSValue on_receive;
  JSValue on_call_summary;
  GSource * source;
};

struct _GumQuickNativeEventSink
{
  GObject parent;

  GumEventType event_mask;
  GumQuickOnEvent on_event;
  gpointer user_data;
};

static void gum_quick_js_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_quick_js_event_sink_dispose (GObject * obj);
static void gum_quick_js_event_sink_finalize (GObject * obj);
static GumEventType gum_quick_js_event_sink_query_mask (GumEventSink * sink);
static void gum_quick_js_event_sink_start (GumEventSink * sink);
static void gum_quick_js_event_sink_process (GumEventSink * sink,
    const GumEvent * event, GumCpuContext * cpu_context);
static void gum_quick_js_event_sink_flush (GumEventSink * sink);
static void gum_quick_js_event_sink_stop (GumEventSink * sink);
static gboolean gum_quick_js_event_sink_stop_when_idle (
    GumQuickJSEventSink * self);
static gboolean gum_quick_js_event_sink_drain (GumQuickJSEventSink * self);

static void gum_quick_native_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumEventType gum_quick_native_event_sink_query_mask (
    GumEventSink * sink);
static void gum_quick_native_event_sink_process (GumEventSink * sink,
    const GumEvent * event, GumCpuContext * cpu_context);

G_DEFINE_TYPE_EXTENDED (GumQuickJSEventSink,
                        gum_quick_js_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                            gum_quick_js_event_sink_iface_init))

G_DEFINE_TYPE_EXTENDED (GumQuickNativeEventSink,
                        gum_quick_native_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                            gum_quick_native_event_sink_iface_init))

GumEventSink *
gum_quick_event_sink_new (JSContext * ctx,
                          const GumQuickEventSinkOptions * options)
{
  if (options->on_event != NULL)
  {
    GumQuickNativeEventSink * sink;

    sink = g_object_new (GUM_QUICK_TYPE_NATIVE_EVENT_SINK, NULL);

    sink->event_mask = options->event_mask;
    sink->on_event = options->on_event;
    sink->user_data = options->user_data;

    return GUM_EVENT_SINK (sink);
  }
  else
  {
    GumQuickJSEventSink * sink;

    sink = g_object_new (GUM_QUICK_TYPE_JS_EVENT_SINK, NULL);

    sink->queue = g_array_sized_new (FALSE, FALSE, sizeof (GumEvent),
        options->queue_capacity);
    sink->queue_capacity = options->queue_capacity;
    sink->queue_drain_interval = options->queue_drain_interval;

    g_object_ref (options->core->script);
    sink->core = options->core;
    sink->main_context = options->main_context;
    sink->event_mask = options->event_mask;

    sink->on_receive = JS_DupValue (ctx, options->on_receive);
    sink->on_call_summary = JS_DupValue (ctx, options->on_call_summary);

    return GUM_EVENT_SINK (sink);
  }
}

static void
gum_quick_js_event_sink_class_init (GumQuickJSEventSinkClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_quick_js_event_sink_dispose;
  object_class->finalize = gum_quick_js_event_sink_finalize;
}

static void
gum_quick_js_event_sink_iface_init (gpointer g_iface,
                                    gpointer iface_data)
{
  GumEventSinkInterface * iface = g_iface;

  iface->query_mask = gum_quick_js_event_sink_query_mask;
  iface->start = gum_quick_js_event_sink_start;
  iface->process = gum_quick_js_event_sink_process;
  iface->flush = gum_quick_js_event_sink_flush;
  iface->stop = gum_quick_js_event_sink_stop;
}

static void
gum_quick_js_event_sink_init (GumQuickJSEventSink * self)
{
  gum_spinlock_init (&self->lock);
}

static void
gum_quick_js_event_sink_release_core (GumQuickJSEventSink * self)
{
  GumQuickCore * core;

  core = g_steal_pointer (&self->core);
  if (core == NULL)
    return;

  {
    JSContext * ctx = core->ctx;
    GumQuickScope scope;

    _gum_quick_scope_enter (&scope, core);

    JS_FreeValue (ctx, self->on_receive);
    JS_FreeValue (ctx, self->on_call_summary);
    self->on_receive = JS_NULL;
    self->on_call_summary = JS_NULL;

    _gum_quick_scope_leave (&scope);
  }

  g_object_unref (core->script);
}

static void
gum_quick_js_event_sink_dispose (GObject * obj)
{
  gum_quick_js_event_sink_release_core (GUM_QUICK_JS_EVENT_SINK (obj));

  G_OBJECT_CLASS (gum_quick_js_event_sink_parent_class)->dispose (obj);
}

static void
gum_quick_js_event_sink_finalize (GObject * obj)
{
  GumQuickJSEventSink * self = GUM_QUICK_JS_EVENT_SINK (obj);

  g_assert (self->source == NULL);

  g_array_free (self->queue, TRUE);

  G_OBJECT_CLASS (gum_quick_js_event_sink_parent_class)->finalize (obj);
}

static GumEventType
gum_quick_js_event_sink_query_mask (GumEventSink * sink)
{
  return GUM_QUICK_JS_EVENT_SINK (sink)->event_mask;
}

static void
gum_quick_js_event_sink_start (GumEventSink * sink)
{
  GumQuickJSEventSink * self = GUM_QUICK_JS_EVENT_SINK (sink);

  if (self->queue_drain_interval != 0)
  {
    self->source = g_timeout_source_new (self->queue_drain_interval);
    g_source_set_callback (self->source,
        (GSourceFunc) gum_quick_js_event_sink_drain, g_object_ref (self),
        g_object_unref);
    g_source_attach (self->source, self->main_context);
  }
}

static void
gum_quick_js_event_sink_process (GumEventSink * sink,
                                 const GumEvent * event,
                                 GumCpuContext * cpu_context)
{
  GumQuickJSEventSink * self = GUM_QUICK_JS_EVENT_SINK_CAST (sink);

  gum_spinlock_acquire (&self->lock);
  if (self->queue->len != self->queue_capacity)
    g_array_append_val (self->queue, *event);
  gum_spinlock_release (&self->lock);
}

static void
gum_quick_js_event_sink_flush (GumEventSink * sink)
{
  GumQuickJSEventSink * self = GUM_QUICK_JS_EVENT_SINK (sink);

  if (self->core == NULL)
    return;

  gum_quick_js_event_sink_drain (self);
}

static void
gum_quick_js_event_sink_stop (GumEventSink * sink)
{
  GumQuickJSEventSink * self = GUM_QUICK_JS_EVENT_SINK (sink);

  if (g_main_context_is_owner (self->main_context))
  {
    gum_quick_js_event_sink_stop_when_idle (self);
  }
  else
  {
    GSource * source;

    source = g_idle_source_new ();
    g_source_set_callback (source,
        (GSourceFunc) gum_quick_js_event_sink_stop_when_idle,
        g_object_ref (self), g_object_unref);
    g_source_attach (source, self->main_context);
    g_source_unref (source);
  }
}

static gboolean
gum_quick_js_event_sink_stop_when_idle (GumQuickJSEventSink * self)
{
  gum_quick_js_event_sink_drain (self);

  g_object_ref (self);

  if (self->source != NULL)
  {
    g_source_destroy (self->source);
    g_source_unref (self->source);
    self->source = NULL;
  }

  gum_quick_js_event_sink_release_core (self);

  g_object_unref (self);

  return FALSE;
}

static gboolean
gum_quick_js_event_sink_drain (GumQuickJSEventSink * self)
{
  GumQuickCore * core = self->core;
  JSContext * ctx = core->ctx;
  gpointer buffer_data;
  JSValue buffer_val;
  guint len, size;
  GumQuickScope scope;

  if (core == NULL)
    return FALSE;

  len = self->queue->len;
  if (len == 0)
    return TRUE;
  size = len * sizeof (GumEvent);

  buffer_data = g_memdup2 (self->queue->data, size);

  gum_spinlock_acquire (&self->lock);
  g_array_remove_range (self->queue, 0, len);
  gum_spinlock_release (&self->lock);

  _gum_quick_scope_enter (&scope, core);

  buffer_val = JS_NewArrayBuffer (ctx, buffer_data, size,
      _gum_quick_array_buffer_free, buffer_data, FALSE);

  if (!JS_IsNull (self->on_call_summary))
  {
    JSValue callback;
    JSValue summary;
    GHashTable * frequencies;
    GumCallEvent * ev;
    guint i;
    GHashTableIter iter;
    gpointer target, count;
    gchar target_str[32];

    callback = JS_DupValue (ctx, self->on_call_summary);

    summary = JS_NewObject (ctx);

    frequencies = g_hash_table_new (NULL, NULL);

    ev = buffer_data;
    for (i = 0; i != len; i++)
    {
      if (ev->type == GUM_CALL)
      {
        gsize n;

        n = GPOINTER_TO_SIZE (g_hash_table_lookup (frequencies, ev->target));
        n++;
        g_hash_table_insert (frequencies, ev->target, GSIZE_TO_POINTER (n));
      }

      ev++;
    }

    g_hash_table_iter_init (&iter, frequencies);
    while (g_hash_table_iter_next (&iter, &target, &count))
    {
      g_sprintf (target_str, "0x%" G_GSIZE_MODIFIER "x",
          GPOINTER_TO_SIZE (target));
      JS_DefinePropertyValueStr (ctx, summary,
          target_str,
          JS_NewInt32 (ctx, GPOINTER_TO_SIZE (count)),
          JS_PROP_C_W_E);
    }

    g_hash_table_unref (frequencies);

    _gum_quick_scope_call_void (&scope, callback, JS_UNDEFINED, 1, &summary);

    JS_FreeValue (ctx, summary);
    JS_FreeValue (ctx, callback);
  }

  if (!JS_IsNull (self->on_receive))
  {
    JSValue callback = JS_DupValue (ctx, self->on_receive);

    _gum_quick_scope_call_void (&scope, callback, JS_UNDEFINED, 1, &buffer_val);

    JS_FreeValue (ctx, callback);
  }

  JS_FreeValue (ctx, buffer_val);

  _gum_quick_scope_leave (&scope);

  return TRUE;
}

static void
gum_quick_native_event_sink_class_init (GumQuickNativeEventSinkClass * klass)
{
}

static void
gum_quick_native_event_sink_iface_init (gpointer g_iface,
                                        gpointer iface_data)
{
  GumEventSinkInterface * iface = g_iface;

  iface->query_mask = gum_quick_native_event_sink_query_mask;
  iface->process = gum_quick_native_event_sink_process;
}

static void
gum_quick_native_event_sink_init (GumQuickNativeEventSink * self)
{
}

static GumEventType
gum_quick_native_event_sink_query_mask (GumEventSink * sink)
{
  return GUM_QUICK_NATIVE_EVENT_SINK (sink)->event_mask;
}

static void
gum_quick_native_event_sink_process (GumEventSink * sink,
                                     const GumEvent * event,
                                     GumCpuContext * cpu_context)
{
  GumQuickNativeEventSink * self = GUM_QUICK_NATIVE_EVENT_SINK_CAST (sink);

  self->on_event (event, cpu_context, self->user_data);
}
