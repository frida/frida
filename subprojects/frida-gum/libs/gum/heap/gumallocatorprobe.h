/*
 * Copyright (C) 2008-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ALLOCATOR_PROBE_H__
#define __GUM_ALLOCATOR_PROBE_H__

#include "gumallocationtracker.h"

#include <gum/gumheapapi.h>

G_BEGIN_DECLS

#define GUM_TYPE_ALLOCATOR_PROBE (gum_allocator_probe_get_type ())
G_DECLARE_FINAL_TYPE (GumAllocatorProbe, gum_allocator_probe, GUM,
    ALLOCATOR_PROBE, GObject)

GUM_API GumAllocatorProbe * gum_allocator_probe_new (void);

GUM_API void gum_allocator_probe_attach (GumAllocatorProbe * self);
GUM_API void gum_allocator_probe_attach_to_apis (GumAllocatorProbe * self,
    const GumHeapApiList * apis);
GUM_API void gum_allocator_probe_detach (GumAllocatorProbe * self);

GUM_API void gum_allocator_probe_suppress (GumAllocatorProbe * self,
    gpointer function_address);

G_END_DECLS

#endif
