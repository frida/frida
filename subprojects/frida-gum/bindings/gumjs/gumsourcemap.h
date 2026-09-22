/*
 * Copyright (C) 2016-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SOURCE_MAP_H__
#define __GUM_SOURCE_MAP_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GUM_TYPE_SOURCE_MAP (gum_source_map_get_type ())
G_DECLARE_FINAL_TYPE (GumSourceMap, gum_source_map, GUM, SOURCE_MAP, GObject)

GumSourceMap * gum_source_map_new (const gchar * json);

gboolean gum_source_map_resolve (GumSourceMap * self, guint * line,
    guint * column, const gchar ** source, const gchar ** name);

gchar * gum_source_map_try_extract_inline (const gchar * source);

G_END_DECLS

#endif
