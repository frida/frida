#ifndef __GUM_MODULE_MAP_H__
#define __GUM_MODULE_MAP_H__

#include "gummodule.h"

typedef struct _GumModuleMap GumModuleMap;

typedef gboolean (* GumModuleMapFilterFunc) (GumModule * module,
    gpointer user_data);

GumModuleMap * gum_module_map_new (void);
GumModuleMap * gum_module_map_new_filtered (GumModuleMapFilterFunc func,
    gpointer data, GDestroyNotify data_destroy);

GumModule * gum_module_map_find (GumModuleMap * self, GumAddress address);

void gum_module_map_update (GumModuleMap * self);

GPtrArray * gum_module_map_get_values (GumModuleMap * self);

#endif
