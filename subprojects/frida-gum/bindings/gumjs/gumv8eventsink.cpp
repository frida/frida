/*
 * Copyright (C) 2012-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Alex Soler <asoler@nowsecure.com>
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8eventsink.h"

#include "gumv8scope.h"
#include "gumv8value.h"

#include <glib/gprintf.h>
#include <gum/gumspinlock.h>
#include <string.h>

using namespace v8;

struct _GumV8JSEventSink
{
  GObject parent;

  GumSpinlock lock;
  GArray * queue;
  guint queue_capacity;
  guint queue_drain_interval;

  GumV8Core * core;
  GMainContext * main_context;
  GumEventType event_mask;
  Global<Function> * on_receive;
  Global<Function> * on_call_summary;
  GSource * source;
};

struct _GumV8NativeEventSink
{
  GObject parent;

  GumEventType event_mask;
  GumV8OnEvent on_event;
  gpointer user_data;
};

static void gum_v8_js_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_v8_js_event_sink_dispose (GObject * obj);
static void gum_v8_js_event_sink_finalize (GObject * obj);
static GumEventType gum_v8_js_event_sink_query_mask (GumEventSink * sink);
static void gum_v8_js_event_sink_start (GumEventSink * sink);
static void gum_v8_js_event_sink_process (GumEventSink * sink,
    const GumEvent * event, GumCpuContext * cpu_context);
static void gum_v8_js_event_sink_flush (GumEventSink * sink);
static void gum_v8_js_event_sink_stop (GumEventSink * sink);
static gboolean gum_v8_js_event_sink_stop_when_idle (GumV8JSEventSink * self);
static gboolean gum_v8_js_event_sink_drain (GumV8JSEventSink * self);

static void gum_v8_native_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumEventType gum_v8_native_event_sink_query_mask (GumEventSink * sink);
static void gum_v8_native_event_sink_process (GumEventSink * sink,
    const GumEvent * event, GumCpuContext * cpu_context);

G_DEFINE_TYPE_EXTENDED (GumV8JSEventSink,
                        gum_v8_js_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                            gum_v8_js_event_sink_iface_init))

G_DEFINE_TYPE_EXTENDED (GumV8NativeEventSink,
                        gum_v8_native_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                            gum_v8_native_event_sink_iface_init))

GumEventSink *
gum_v8_event_sink_new (const GumV8EventSinkOptions * options)
{
  if (options->on_event != NULL)
  {
    auto sink = GUM_V8_NATIVE_EVENT_SINK (
        g_object_new (GUM_V8_TYPE_NATIVE_EVENT_SINK, NULL));

    sink->event_mask = options->event_mask;
    sink->on_event = options->on_event;
    sink->user_data = options->user_data;

    return GUM_EVENT_SINK (sink);
  }
  else
  {
    auto isolate = options->core->isolate;

    auto sink = GUM_V8_JS_EVENT_SINK (
        g_object_new (GUM_V8_TYPE_JS_EVENT_SINK, NULL));

    sink->queue = g_array_sized_new (FALSE, FALSE, sizeof (GumEvent),
        options->queue_capacity);
    sink->queue_capacity = options->queue_capacity;
    sink->queue_drain_interval = options->queue_drain_interval;

    g_object_ref (options->core->script);
    sink->core = options->core;
    sink->main_context = options->main_context;
    sink->event_mask = options->event_mask;
    if (!options->on_receive.IsEmpty ())
    {
      sink->on_receive =
          new Global<Function> (isolate, options->on_receive);
    }
    if (!options->on_call_summary.IsEmpty ())
    {
      sink->on_call_summary =
          new Global<Function> (isolate, options->on_call_summary);
    }

    return GUM_EVENT_SINK (sink);
  }
}

static void
gum_v8_js_event_sink_class_init (GumV8JSEventSinkClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_v8_js_event_sink_dispose;
  object_class->finalize = gum_v8_js_event_sink_finalize;
}

static void
gum_v8_js_event_sink_iface_init (gpointer g_iface,
                                 gpointer iface_data)
{
  auto iface = (GumEventSinkInterface *) g_iface;

  iface->query_mask = gum_v8_js_event_sink_query_mask;
  iface->start = gum_v8_js_event_sink_start;
  iface->process = gum_v8_js_event_sink_process;
  iface->flush = gum_v8_js_event_sink_flush;
  iface->stop = gum_v8_js_event_sink_stop;
}

static void
gum_v8_js_event_sink_init (GumV8JSEventSink * self)
{
  gum_spinlock_init (&self->lock);
}

static void
gum_v8_js_event_sink_release_core (GumV8JSEventSink * self)
{
  GumV8Core * core = (GumV8Core *) g_steal_pointer (&self->core);
  if (core == NULL)
    return;

  auto script = core->script;

  {
    ScriptScope scope (script);

    delete self->on_receive;
    self->on_receive = nullptr;

    delete self->on_call_summary;
    self->on_call_summary = nullptr;
  }

  g_object_unref (script);
}

static void
gum_v8_js_event_sink_dispose (GObject * obj)
{
  gum_v8_js_event_sink_release_core (GUM_V8_JS_EVENT_SINK (obj));

  G_OBJECT_CLASS (gum_v8_js_event_sink_parent_class)->dispose (obj);
}

static void
gum_v8_js_event_sink_finalize (GObject * obj)
{
  auto self = GUM_V8_JS_EVENT_SINK (obj);

  g_assert (self->source == NULL);

  g_array_free (self->queue, TRUE);

  G_OBJECT_CLASS (gum_v8_js_event_sink_parent_class)->finalize (obj);
}

static GumEventType
gum_v8_js_event_sink_query_mask (GumEventSink * sink)
{
  return GUM_V8_JS_EVENT_SINK (sink)->event_mask;
}

static void
gum_v8_js_event_sink_start (GumEventSink * sink)
{
  auto self = GUM_V8_JS_EVENT_SINK (sink);

  if (self->queue_drain_interval != 0)
  {
    self->source = g_timeout_source_new (self->queue_drain_interval);
    g_source_set_callback (self->source,
        (GSourceFunc) gum_v8_js_event_sink_drain, g_object_ref (self),
        g_object_unref);
    g_source_attach (self->source, self->main_context);
  }
}

static void
gum_v8_js_event_sink_process (GumEventSink * sink,
                              const GumEvent * event,
                              GumCpuContext * cpu_context)
{
  auto self = GUM_V8_JS_EVENT_SINK_CAST (sink);

  gum_spinlock_acquire (&self->lock);
  if (self->queue->len != self->queue_capacity)
    g_array_append_val (self->queue, *event);
  gum_spinlock_release (&self->lock);
}

static void
gum_v8_js_event_sink_flush (GumEventSink * sink)
{
  auto self = GUM_V8_JS_EVENT_SINK (sink);

  if (self->core == NULL)
    return;

  gum_v8_js_event_sink_drain (self);
}

static void
gum_v8_js_event_sink_stop (GumEventSink * sink)
{
  auto self = GUM_V8_JS_EVENT_SINK (sink);

  if (g_main_context_is_owner (self->main_context))
  {
    gum_v8_js_event_sink_stop_when_idle (self);
  }
  else
  {
    auto source = g_idle_source_new ();
    g_source_set_callback (source,
        (GSourceFunc) gum_v8_js_event_sink_stop_when_idle, g_object_ref (self),
        g_object_unref);
    g_source_attach (source, self->main_context);
    g_source_unref (source);
  }
}

static gboolean
gum_v8_js_event_sink_stop_when_idle (GumV8JSEventSink * self)
{
  gum_v8_js_event_sink_drain (self);

  g_object_ref (self);

  if (self->source != NULL)
  {
    g_source_destroy (self->source);
    g_source_unref (self->source);
    self->source = NULL;
  }

  gum_v8_js_event_sink_release_core (self);

  g_object_unref (self);

  return FALSE;
}

static gboolean
gum_v8_js_event_sink_drain (GumV8JSEventSink * self)
{
  gpointer buffer = NULL;
  guint len, size;

  auto core = self->core;
  if (core == NULL)
    return FALSE;

  len = self->queue->len;
  size = len * sizeof (GumEvent);
  if (len != 0)
  {
    buffer = g_memdup2 (self->queue->data, size);

    gum_spinlock_acquire (&self->lock);
    g_array_remove_range (self->queue, 0, len);
    gum_spinlock_release (&self->lock);
  }

  if (buffer != NULL)
  {
    GHashTable * frequencies = NULL;

    if (self->on_call_summary != nullptr)
    {
      frequencies = g_hash_table_new (NULL, NULL);

      auto ev = (GumCallEvent *) buffer;
      for (guint i = 0; i != len; i++)
      {
        if (ev->type == GUM_CALL)
        {
          auto count = GPOINTER_TO_SIZE (
              g_hash_table_lookup (frequencies, ev->target));
          count++;
          g_hash_table_insert (frequencies, ev->target,
              GSIZE_TO_POINTER (count));
        }

        ev++;
      }
    }

    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();
    auto recv = Undefined (isolate);

    if (frequencies != NULL)
    {
      auto summary = Object::New (isolate);

      GHashTableIter iter;
      g_hash_table_iter_init (&iter, frequencies);
      gpointer target, count;
      gchar target_str[32];
      while (g_hash_table_iter_next (&iter, &target, &count))
      {
        g_sprintf (target_str, "0x%" G_GSIZE_MODIFIER "x",
            GPOINTER_TO_SIZE (target));
        _gum_v8_object_set (summary, target_str,
            Number::New (isolate, GPOINTER_TO_SIZE (count)), core);
      }

      g_hash_table_unref (frequencies);

      Local<Value> argv[] = { summary };
      auto on_call_summary =
          Local<Function>::New (isolate, *self->on_call_summary);
      auto result =
          on_call_summary->Call (context, recv, G_N_ELEMENTS (argv), argv);
      if (result.IsEmpty ())
        scope.ProcessAnyPendingException ();
    }

    if (self->on_receive != nullptr)
    {
      auto on_receive = Local<Function>::New (isolate, *self->on_receive);
      Local<Value> argv[] = {
        _gum_v8_array_buffer_new_take (isolate, g_steal_pointer (&buffer),
            size),
      };
      auto result = on_receive->Call (context, recv, G_N_ELEMENTS (argv), argv);
      if (result.IsEmpty ())
        scope.ProcessAnyPendingException ();
    }

    g_free (buffer);
  }

  return TRUE;
}

static void
gum_v8_native_event_sink_class_init (GumV8NativeEventSinkClass * klass)
{
}

static void
gum_v8_native_event_sink_iface_init (gpointer g_iface,
                                     gpointer iface_data)
{
  auto iface = (GumEventSinkInterface *) g_iface;

  iface->query_mask = gum_v8_native_event_sink_query_mask;
  iface->process = gum_v8_native_event_sink_process;
}

static void
gum_v8_native_event_sink_init (GumV8NativeEventSink * self)
{
}

static GumEventType
gum_v8_native_event_sink_query_mask (GumEventSink * sink)
{
  return GUM_V8_NATIVE_EVENT_SINK (sink)->event_mask;
}

static void
gum_v8_native_event_sink_process (GumEventSink * sink,
                                  const GumEvent * event,
                                  GumCpuContext * cpu_context)
{
  auto self = GUM_V8_NATIVE_EVENT_SINK_CAST (sink);

  self->on_event (event, cpu_context, self->user_data);
}
