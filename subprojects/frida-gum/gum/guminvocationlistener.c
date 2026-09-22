/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminvocationlistener.h"

#define GUM_TYPE_CALL_LISTENER (gum_call_listener_get_type ())
G_DECLARE_FINAL_TYPE (GumCallListener, gum_call_listener, GUM, CALL_LISTENER,
                      GObject)

#define GUM_TYPE_PROBE_LISTENER (gum_probe_listener_get_type ())
G_DECLARE_FINAL_TYPE (GumProbeListener, gum_probe_listener, GUM, PROBE_LISTENER,
                      GObject)

struct _GumCallListener
{
  GObject parent;

  GumInvocationCallback on_enter;
  GumInvocationCallback on_leave;

  gpointer data;
  GDestroyNotify data_destroy;
};

struct _GumProbeListener
{
  GObject parent;

  GumInvocationCallback on_hit;

  gpointer data;
  GDestroyNotify data_destroy;
};

G_DEFINE_INTERFACE (GumInvocationListener, gum_invocation_listener,
                    G_TYPE_OBJECT)

static void gum_call_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_call_listener_finalize (GObject * object);
static void gum_call_listener_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void gum_call_listener_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);
G_DEFINE_TYPE_EXTENDED (GumCallListener,
                        gum_call_listener,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_call_listener_iface_init))

static void gum_probe_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_probe_listener_finalize (GObject * object);
static void gum_probe_listener_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
G_DEFINE_TYPE_EXTENDED (GumProbeListener,
                        gum_probe_listener,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_probe_listener_iface_init))

static void
gum_invocation_listener_default_init (GumInvocationListenerInterface * iface)
{
}

/**
 * gum_make_call_listener:
 * @on_enter: (scope forever) (nullable): callback for function entry
 * @on_leave: (scope forever) (nullable): callback for function exit
 * @data: data to pass to the callbacks
 * @data_destroy: function to destroy @data
 *
 * Creates a new call listener.
 *
 * Returns: (transfer full): the newly created listener
 */
GumInvocationListener *
gum_make_call_listener (GumInvocationCallback on_enter,
                        GumInvocationCallback on_leave,
                        gpointer data,
                        GDestroyNotify data_destroy)
{
  GumCallListener * listener;

  listener = g_object_new (GUM_TYPE_CALL_LISTENER, NULL);
  listener->on_enter = on_enter;
  listener->on_leave = on_leave;
  listener->data = data;
  listener->data_destroy = data_destroy;

  return GUM_INVOCATION_LISTENER (listener);
}

/**
 * gum_make_probe_listener:
 * @on_hit: (scope forever): callback for each invocation
 * @data: data to pass to @on_hit
 * @data_destroy: function to destroy @data
 *
 * Creates a new probe listener.
 *
 * Returns: (transfer full): the newly created listener
 */
GumInvocationListener *
gum_make_probe_listener (GumInvocationCallback on_hit,
                         gpointer data,
                         GDestroyNotify data_destroy)
{
  GumProbeListener * listener;

  listener = g_object_new (GUM_TYPE_PROBE_LISTENER, NULL);
  listener->on_hit = on_hit;
  listener->data = data;
  listener->data_destroy = data_destroy;

  return GUM_INVOCATION_LISTENER (listener);
}

void
gum_invocation_listener_on_enter (GumInvocationListener * self,
                                  GumInvocationContext * context)
{
  GumInvocationListenerInterface * iface =
      GUM_INVOCATION_LISTENER_GET_IFACE (self);

  if (iface->on_enter != NULL)
    iface->on_enter (self, context);
}

void
gum_invocation_listener_on_leave (GumInvocationListener * self,
                                  GumInvocationContext * context)
{
  GumInvocationListenerInterface * iface =
      GUM_INVOCATION_LISTENER_GET_IFACE (self);

  if (iface->on_leave != NULL)
    iface->on_leave (self, context);
}

static void
gum_call_listener_class_init (GumCallListenerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_call_listener_finalize;
}

static void
gum_call_listener_iface_init (gpointer g_iface,
                              gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_call_listener_on_enter;
  iface->on_leave = gum_call_listener_on_leave;
}

static void
gum_call_listener_init (GumCallListener * self)
{
}

static void
gum_call_listener_finalize (GObject * object)
{
  GumCallListener * self = GUM_CALL_LISTENER (object);

  if (self->data_destroy != NULL)
    self->data_destroy (self->data);

  G_OBJECT_CLASS (gum_call_listener_parent_class)->finalize (object);
}

static void
gum_call_listener_on_enter (GumInvocationListener * listener,
                            GumInvocationContext * context)
{
  GumCallListener * self = GUM_CALL_LISTENER (listener);

  if (self->on_enter != NULL)
    self->on_enter (context, self->data);
}

static void
gum_call_listener_on_leave (GumInvocationListener * listener,
                            GumInvocationContext * context)
{
  GumCallListener * self = GUM_CALL_LISTENER (listener);

  if (self->on_leave != NULL)
    self->on_leave (context, self->data);
}

static void
gum_probe_listener_class_init (GumProbeListenerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_probe_listener_finalize;
}

static void
gum_probe_listener_iface_init (gpointer g_iface,
                               gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_probe_listener_on_enter;
}

static void
gum_probe_listener_init (GumProbeListener * self)
{
}

static void
gum_probe_listener_finalize (GObject * object)
{
  GumProbeListener * self = GUM_PROBE_LISTENER (object);

  if (self->data_destroy != NULL)
    self->data_destroy (self->data);

  G_OBJECT_CLASS (gum_probe_listener_parent_class)->finalize (object);
}

static void
gum_probe_listener_on_enter (GumInvocationListener * listener,
                             GumInvocationContext * context)
{
  GumProbeListener * self = GUM_PROBE_LISTENER (listener);

  self->on_hit (context, self->data);
}
