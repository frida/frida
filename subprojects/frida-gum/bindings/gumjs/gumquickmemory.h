/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_MEMORY_H__
#define __GUM_QUICK_MEMORY_H__

#include "gumquickcore.h"

#include <gum/gummemoryaccessmonitor.h>

G_BEGIN_DECLS

typedef struct _GumQuickMemory GumQuickMemory;

struct _GumQuickMemory
{
  GumQuickCore * core;

  GumMemoryAccessMonitor * monitor;
  JSValue on_access;

  JSClassID memory_access_details_class;
};

G_GNUC_INTERNAL void _gum_quick_memory_init (GumQuickMemory * self, JSValue ns,
    GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_memory_dispose (GumQuickMemory * self);
G_GNUC_INTERNAL void _gum_quick_memory_finalize (GumQuickMemory * self);

G_END_DECLS

#endif
