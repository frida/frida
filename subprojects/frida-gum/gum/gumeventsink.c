/*
 * Copyright (C) 2009-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumeventsink.h"

struct _GumDefaultEventSink
{
  GObject parent;
};

struct _GumCallbackEventSink
{
  GObject parent;

  GumEventType mask;
  GumEventSinkCallback callback;
  gpointer data;
  GDestroyNotify data_destroy;
};

static void gum_default_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumEventType gum_default_event_sink_query_mask (GumEventSink * sink);
static void gum_default_event_sink_process (GumEventSink * sink,
    const GumEvent * event, GumCpuContext * cpu_context);

static void gum_callback_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_callback_event_sink_finalize (GObject * object);
static GumEventType gum_callback_event_sink_query_mask (GumEventSink * sink);
static void gum_callback_event_sink_process (GumEventSink * sink,
    const GumEvent * event, GumCpuContext * cpu_context);

G_DEFINE_INTERFACE (GumEventSink, gum_event_sink, G_TYPE_OBJECT)

G_DEFINE_TYPE_EXTENDED (GumDefaultEventSink,
                        gum_default_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                            gum_default_event_sink_iface_init))

G_DEFINE_TYPE_EXTENDED (GumCallbackEventSink,
                        gum_callback_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                            gum_callback_event_sink_iface_init))

static void
gum_event_sink_default_init (GumEventSinkInterface * iface)
{
}

GumEventType
gum_event_sink_query_mask (GumEventSink * self)
{
  GumEventSinkInterface * iface = GUM_EVENT_SINK_GET_IFACE (self);

  g_assert (iface->query_mask != NULL);

  return iface->query_mask (self);
}

void
gum_event_sink_start (GumEventSink * self)
{
  GumEventSinkInterface * iface = GUM_EVENT_SINK_GET_IFACE (self);

  if (iface->start != NULL)
    iface->start (self);
}

void
gum_event_sink_process (GumEventSink * self,
                        const GumEvent * event,
                        GumCpuContext * cpu_context)
{
  GumEventSinkInterface * iface = GUM_EVENT_SINK_GET_IFACE (self);

  g_assert (iface->process != NULL);

  iface->process (self, event, cpu_context);
}

void
gum_event_sink_flush (GumEventSink * self)
{
  GumEventSinkInterface * iface = GUM_EVENT_SINK_GET_IFACE (self);

  if (iface->flush != NULL)
    iface->flush (self);
}

void
gum_event_sink_stop (GumEventSink * self)
{
  GumEventSinkInterface * iface = GUM_EVENT_SINK_GET_IFACE (self);

  if (iface->stop != NULL)
    iface->stop (self);
}

/**
 * gum_event_sink_make_default:
 *
 * Creates a default #GumEventSink that throws away any events directed at it.
 *
 * Returns: (transfer full): a newly created #GumEventSink
 */
GumEventSink *
gum_event_sink_make_default (void)
{
  return g_object_new (GUM_TYPE_DEFAULT_EVENT_SINK, NULL);
}

/**
 * gum_event_sink_make_from_callback:
 * @mask: bitfield specifying event types that are of interest
 * @callback: (not nullable): function called with each event
 * @data: data to pass to @callback
 * @data_destroy: (destroy data): function to destroy @data
 *
 * Creates a #GumEventSink that delivers events to @callback.
 *
 * Returns: (transfer full): a newly created #GumEventSink
 */
GumEventSink *
gum_event_sink_make_from_callback (GumEventType mask,
                                   GumEventSinkCallback callback,
                                   gpointer data,
                                   GDestroyNotify data_destroy)
{
  GumCallbackEventSink * sink;

  sink = g_object_new (GUM_TYPE_CALLBACK_EVENT_SINK, NULL);
  sink->mask = mask;
  sink->callback = callback;
  sink->data = data;
  sink->data_destroy = data_destroy;

  return GUM_EVENT_SINK (sink);
}

static void
gum_default_event_sink_class_init (GumDefaultEventSinkClass * klass)
{
}

static void
gum_default_event_sink_iface_init (gpointer g_iface,
                                   gpointer iface_data)
{
  GumEventSinkInterface * iface = g_iface;

  iface->query_mask = gum_default_event_sink_query_mask;
  iface->process = gum_default_event_sink_process;
}

static void
gum_default_event_sink_init (GumDefaultEventSink * self)
{
}

static GumEventType
gum_default_event_sink_query_mask (GumEventSink * sink)
{
  return GUM_NOTHING;
}

static void
gum_default_event_sink_process (GumEventSink * sink,
                                const GumEvent * event,
                                GumCpuContext * cpu_context)
{
}

static void
gum_callback_event_sink_class_init (GumCallbackEventSinkClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_callback_event_sink_finalize;
}

static void
gum_callback_event_sink_iface_init (gpointer g_iface,
                                    gpointer iface_data)
{
  GumEventSinkInterface * iface = g_iface;

  iface->query_mask = gum_callback_event_sink_query_mask;
  iface->process = gum_callback_event_sink_process;
}

static void
gum_callback_event_sink_init (GumCallbackEventSink * self)
{
}

static void
gum_callback_event_sink_finalize (GObject * object)
{
  GumCallbackEventSink * self = GUM_CALLBACK_EVENT_SINK (object);

  if (self->data_destroy != NULL)
    self->data_destroy (self->data);

  G_OBJECT_CLASS (gum_callback_event_sink_parent_class)->finalize (object);
}

static GumEventType
gum_callback_event_sink_query_mask (GumEventSink * sink)
{
  return GUM_CALLBACK_EVENT_SINK (sink)->mask;
}

static void
gum_callback_event_sink_process (GumEventSink * sink,
                                 const GumEvent * event,
                                 GumCpuContext * cpu_context)
{
  GumCallbackEventSink * self = (GumCallbackEventSink *) sink;

  self->callback (event, cpu_context, self->data);
}
