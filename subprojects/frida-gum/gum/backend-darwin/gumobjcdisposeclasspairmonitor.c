/*
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumobjcdisposeclasspairmonitor.h"

#include <gum/guminvocationlistener.h>

static void gum_objc_dispose_class_pair_monitor_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_objc_dispose_class_pair_monitor_dispose (GObject * object);
static void gum_objc_dispose_class_pair_monitor_finalize (GObject * object);
static void gum_on_weak_notify (gpointer data, GObject * where_the_object_was);
static void gum_objc_dispose_class_pair_monitor_on_enter (
    GumInvocationListener * listener, GumInvocationContext * context);
static void gum_objc_dispose_class_pair_monitor_on_leave (
    GumInvocationListener * listener, GumInvocationContext * context);

G_DEFINE_TYPE_EXTENDED (GumObjcDisposeClassPairMonitor,
                        gum_objc_dispose_class_pair_monitor,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_objc_dispose_class_pair_monitor_iface_init))

static GMutex _gum_objc_dispose_class_pair_monitor_lock;
static GumObjcDisposeClassPairMonitor * _the_monitor = NULL;

static void
gum_objc_dispose_class_pair_monitor_class_init (
    GumObjcDisposeClassPairMonitorClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_objc_dispose_class_pair_monitor_dispose;
  object_class->finalize = gum_objc_dispose_class_pair_monitor_finalize;
}

static void
gum_objc_dispose_class_pair_monitor_iface_init (gpointer g_iface,
                                                gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_objc_dispose_class_pair_monitor_on_enter;
  iface->on_leave = gum_objc_dispose_class_pair_monitor_on_leave;
}

static void
gum_objc_dispose_class_pair_monitor_init (GumObjcDisposeClassPairMonitor * self)
{
  GumModule * libobjc;
  gpointer dispose_impl;
  GumAttachOptions options = { .ignorability = GUM_INVOCATION_UNIGNORABLE };

  g_rec_mutex_init (&self->mutex);

  libobjc = gum_process_find_module_by_name ("/usr/lib/libobjc.A.dylib");
  dispose_impl = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (libobjc, "objc_disposeClassPair"));
  g_assert (dispose_impl != NULL);
  g_object_unref (libobjc);

  self->interceptor = gum_interceptor_obtain ();
  gum_interceptor_attach (self->interceptor, dispose_impl,
      GUM_INVOCATION_LISTENER (self), &options);
}

static void
gum_objc_dispose_class_pair_monitor_dispose (GObject * object)
{
  GumObjcDisposeClassPairMonitor * self =
      GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR (object);

  if (self->interceptor != NULL)
  {
    g_rec_mutex_lock (&self->mutex);
    gum_interceptor_detach (self->interceptor, GUM_INVOCATION_LISTENER (self));
    g_rec_mutex_unlock (&self->mutex);

    g_object_unref (self->interceptor);
    self->interceptor = NULL;
  }

  G_OBJECT_CLASS (
      gum_objc_dispose_class_pair_monitor_parent_class)->dispose (object);
}

static void
gum_objc_dispose_class_pair_monitor_finalize (GObject * object)
{
  GumObjcDisposeClassPairMonitor * self =
      GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR (object);

  g_rec_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (
      gum_objc_dispose_class_pair_monitor_parent_class)->finalize (object);
}

GumObjcDisposeClassPairMonitor *
gum_objc_dispose_class_pair_monitor_obtain (void)
{
  GumObjcDisposeClassPairMonitor * monitor;

  g_mutex_lock (&_gum_objc_dispose_class_pair_monitor_lock);

  if (_the_monitor != NULL)
  {
    monitor = GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR (g_object_ref (_the_monitor));
  }
  else
  {
    _the_monitor = g_object_new (GUM_TYPE_OBJC_DISPOSE_CLASS_PAIR_MONITOR,
        NULL);
    g_object_weak_ref (G_OBJECT (_the_monitor), gum_on_weak_notify, NULL);

    monitor = _the_monitor;
  }

  g_mutex_unlock (&_gum_objc_dispose_class_pair_monitor_lock);

  return monitor;
}

static void
gum_on_weak_notify (gpointer data,
                    GObject * where_the_object_was)
{
  g_mutex_lock (&_gum_objc_dispose_class_pair_monitor_lock);

  g_assert (_the_monitor == (GumObjcDisposeClassPairMonitor *)
      where_the_object_was);
  _the_monitor = NULL;

  g_mutex_unlock (&_gum_objc_dispose_class_pair_monitor_lock);
}

static void
gum_objc_dispose_class_pair_monitor_on_enter (GumInvocationListener * listener,
                                              GumInvocationContext * context)
{
  GumObjcDisposeClassPairMonitor * self =
      GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR (listener);

  g_rec_mutex_lock (&self->mutex);
}

static void
gum_objc_dispose_class_pair_monitor_on_leave (GumInvocationListener * listener,
                                              GumInvocationContext * context)
{
  GumObjcDisposeClassPairMonitor * self =
      GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR (listener);

  g_rec_mutex_unlock (&self->mutex);
}
