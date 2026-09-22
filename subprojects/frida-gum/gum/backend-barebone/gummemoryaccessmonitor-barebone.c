/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemoryaccessmonitor.h"

struct _GumMemoryAccessMonitor
{
  GObject parent;
};

G_DEFINE_TYPE (GumMemoryAccessMonitor, gum_memory_access_monitor, G_TYPE_OBJECT)

static void
gum_memory_access_monitor_class_init (GumMemoryAccessMonitorClass * klass)
{
}

static void
gum_memory_access_monitor_init (GumMemoryAccessMonitor * self)
{
}

GumMemoryAccessMonitor *
gum_memory_access_monitor_new (const GumMemoryRange * ranges,
                               guint num_ranges,
                               GumPageProtection access_mask,
                               gboolean auto_reset,
                               GumMemoryAccessNotify func,
                               gpointer data,
                               GDestroyNotify data_destroy)
{
  if (data_destroy != NULL)
    data_destroy (data);

  return g_object_new (GUM_TYPE_MEMORY_ACCESS_MONITOR, NULL);
}

gboolean
gum_memory_access_monitor_enable (GumMemoryAccessMonitor * self,
                                  GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Not supported by the Barebone backend");
  return FALSE;
}

void
gum_memory_access_monitor_disable (GumMemoryAccessMonitor * self)
{
}
