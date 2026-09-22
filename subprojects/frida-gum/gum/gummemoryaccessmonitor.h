/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MEMORY_ACCESS_MONITOR_H__
#define __GUM_MEMORY_ACCESS_MONITOR_H__

#include <gum/gumprocess.h>

G_BEGIN_DECLS

#define GUM_TYPE_MEMORY_ACCESS_MONITOR (gum_memory_access_monitor_get_type ())
G_DECLARE_FINAL_TYPE (GumMemoryAccessMonitor, gum_memory_access_monitor, GUM,
                      MEMORY_ACCESS_MONITOR, GObject)

typedef struct _GumMemoryAccessDetails GumMemoryAccessDetails;

typedef void (* GumMemoryAccessNotify) (GumMemoryAccessMonitor * monitor,
    const GumMemoryAccessDetails * details, gpointer user_data);

struct _GumMemoryAccessDetails
{
  GumThreadId thread_id;
  GumMemoryOperation operation;
  gpointer from;
  gpointer address;

  guint range_index;
  guint page_index;
  guint pages_completed;
  guint pages_total;

  GumCpuContext * context;
};

GUM_API GumMemoryAccessMonitor * gum_memory_access_monitor_new (
    const GumMemoryRange * ranges, guint num_ranges,
    GumPageProtection access_mask, gboolean auto_reset,
    GumMemoryAccessNotify func, gpointer data,
    GDestroyNotify data_destroy);

GUM_API gboolean gum_memory_access_monitor_enable (
    GumMemoryAccessMonitor * self, GError ** error);
GUM_API void gum_memory_access_monitor_disable (GumMemoryAccessMonitor * self);

G_END_DECLS

#endif
