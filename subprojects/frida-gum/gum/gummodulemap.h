/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_MAP_H__
#define __GUM_MODULE_MAP_H__

#include <gum/gumprocess.h>

G_BEGIN_DECLS

#define GUM_TYPE_MODULE_MAP (gum_module_map_get_type ())
G_DECLARE_FINAL_TYPE (GumModuleMap, gum_module_map, GUM, MODULE_MAP, GObject)

typedef gboolean (* GumModuleMapFilterFunc) (GumModule * module,
    gpointer user_data);

GUM_API GumModuleMap * gum_module_map_new (void);
GUM_API GumModuleMap * gum_module_map_new_filtered (GumModuleMapFilterFunc func,
    gpointer data, GDestroyNotify data_destroy);

GUM_API GumModule * gum_module_map_find (GumModuleMap * self,
    GumAddress address);

GUM_API void gum_module_map_update (GumModuleMap * self);

GUM_API GPtrArray * gum_module_map_get_values (GumModuleMap * self);

G_END_DECLS

#endif
