/*
 * Copyright (C) 2021-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_GRAFTER_H__
#define __GUM_DARWIN_GRAFTER_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

typedef enum {
  GUM_DARWIN_GRAFTER_FLAGS_NONE                   = 0,
  GUM_DARWIN_GRAFTER_FLAGS_INGEST_FUNCTION_STARTS = (1 << 0),
  GUM_DARWIN_GRAFTER_FLAGS_INGEST_IMPORTS         = (1 << 1),
  GUM_DARWIN_GRAFTER_FLAGS_TRANSFORM_LAZY_BINDS   = (1 << 2),
} GumDarwinGrafterFlags;

#define GUM_TYPE_DARWIN_GRAFTER (gum_darwin_grafter_get_type ())
G_DECLARE_FINAL_TYPE (GumDarwinGrafter, gum_darwin_grafter, GUM, DARWIN_GRAFTER,
                      GObject)

GUM_API GumDarwinGrafter * gum_darwin_grafter_new_from_file (
    const gchar * path, GumDarwinGrafterFlags flags);

GUM_API void gum_darwin_grafter_add (GumDarwinGrafter * self,
    guint32 code_offset);

GUM_API gboolean gum_darwin_grafter_graft (GumDarwinGrafter * self,
    GError ** error);

G_END_DECLS

#endif
