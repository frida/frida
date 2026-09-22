/*
 * Copyright (C) 2013-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MEMORY_MAP_H__
#define __GUM_MEMORY_MAP_H__

#include <gum/gummemory.h>

G_BEGIN_DECLS

#define GUM_TYPE_MEMORY_MAP (gum_memory_map_get_type ())
G_DECLARE_FINAL_TYPE (GumMemoryMap, gum_memory_map, GUM, MEMORY_MAP, GObject)

GUM_API GumMemoryMap * gum_memory_map_new (GumPageProtection prot);

GUM_API gboolean gum_memory_map_contains (GumMemoryMap * self,
    const GumMemoryRange * range);

GUM_API void gum_memory_map_update (GumMemoryMap * self);

G_END_DECLS

#endif
