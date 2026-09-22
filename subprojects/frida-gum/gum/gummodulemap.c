/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodulemap.h"

#include <stdlib.h>

struct _GumModuleMap
{
  GObject parent;

  GPtrArray * modules;

  GumModuleMapFilterFunc filter_func;
  gpointer filter_data;
  GDestroyNotify filter_data_destroy;
};

static void gum_module_map_dispose (GObject * object);

static gboolean gum_add_module (GumModule * module, gpointer user_data);
static gint gum_module_compare_base (GumModule ** lhs_module,
    GumModule ** rhs_module);
static gint gum_module_compare_to_key (const GumAddress * key_ptr,
    GumModule ** member);

/**
 * GumModuleMap:
 *
 * A snapshot of the process's loaded modules, ordered for fast lookup of the
 * module containing a given address.
 *
 * The map is populated when created and can be refreshed with
 * [method@Gum.ModuleMap.update] after modules are loaded or unloaded. A
 * filtered map retains only the modules accepted by a
 * [callback@Gum.ModuleMapFilterFunc], handy for narrowing lookups to a subset
 * such as your own modules.
 */

/**
 * GumModuleMapFilterFunc:
 * @module: the module being considered
 * @user_data: data passed to [ctor@Gum.ModuleMap.new_filtered]
 *
 * Decides whether @module should be included in a filtered module map.
 *
 * Returns: %TRUE to include @module, %FALSE to skip it
 */

G_DEFINE_TYPE (GumModuleMap, gum_module_map, G_TYPE_OBJECT)

static void
gum_module_map_class_init (GumModuleMapClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_module_map_dispose;
}

static void
gum_module_map_init (GumModuleMap * self)
{
  self->modules = g_ptr_array_new_full (0, g_object_unref);
}

static void
gum_module_map_dispose (GObject * object)
{
  GumModuleMap * self = GUM_MODULE_MAP (object);

  g_clear_pointer (&self->modules, g_ptr_array_unref);

  if (self->filter_data_destroy != NULL)
    self->filter_data_destroy (self->filter_data);

  self->filter_func = NULL;
  self->filter_data = NULL;
  self->filter_data_destroy = NULL;

  G_OBJECT_CLASS (gum_module_map_parent_class)->dispose (object);
}

/**
 * gum_module_map_new:
 *
 * Creates a module map covering all of the process's currently loaded modules.
 *
 * Returns: (transfer full): a new #GumModuleMap
 */
GumModuleMap *
gum_module_map_new (void)
{
  GumModuleMap * map;

  map = g_object_new (GUM_TYPE_MODULE_MAP, NULL);

  gum_module_map_update (map);

  return map;
}

/**
 * gum_module_map_new_filtered:
 * @func: (scope notified): filter deciding which modules to include
 * @data: data to pass to @func
 * @data_destroy: (nullable): destroy notify for @data
 *
 * Creates a module map containing only the modules for which @func returns
 * %TRUE.
 *
 * Returns: (transfer full): a new #GumModuleMap
 */
GumModuleMap *
gum_module_map_new_filtered (GumModuleMapFilterFunc func,
                             gpointer data,
                             GDestroyNotify data_destroy)
{
  GumModuleMap * map;

  map = g_object_new (GUM_TYPE_MODULE_MAP, NULL);
  map->filter_func = func;
  map->filter_data = data;
  map->filter_data_destroy = data_destroy;

  gum_module_map_update (map);

  return map;
}

/**
 * gum_module_map_find:
 * @self: module map
 * @address: address to look up
 *
 * Finds the module containing the given address.
 *
 * Returns: (transfer none) (nullable): the module, or %NULL
 */
GumModule *
gum_module_map_find (GumModuleMap * self,
                     GumAddress address)
{
  GumModule ** entry;
  GumAddress bare_address;

  bare_address = gum_strip_code_address (address);

  entry = bsearch (&bare_address, self->modules->pdata, self->modules->len,
      sizeof (GumModule *), (GCompareFunc) gum_module_compare_to_key);
  if (entry == NULL)
    return NULL;

  return *entry;
}

/**
 * gum_module_map_update:
 * @self: module map
 *
 * Refreshes the map from the process's current set of loaded modules. Call this
 * after modules may have been loaded or unloaded, since lookups through a stale
 * map can otherwise return wrong results.
 */
void
gum_module_map_update (GumModuleMap * self)
{
  g_ptr_array_set_size (self->modules, 0);
  gum_process_enumerate_modules (gum_add_module, self);
  g_ptr_array_sort (self->modules, (GCompareFunc) gum_module_compare_base);
}

/**
 * gum_module_map_get_values:
 * @self: module map
 *
 * Returns the modules in the map.
 *
 * Returns: (transfer none) (element-type GumModule): the modules
 */
GPtrArray *
gum_module_map_get_values (GumModuleMap * self)
{
  return self->modules;
}

static gboolean
gum_add_module (GumModule * module,
                gpointer user_data)
{
  GumModuleMap * self = user_data;

  if (self->filter_func != NULL)
  {
    if (!self->filter_func (module, self->filter_data))
      return TRUE;
  }

  g_ptr_array_add (self->modules, g_object_ref (module));

  return TRUE;
}

static gint
gum_module_compare_base (GumModule ** lhs_module,
                         GumModule ** rhs_module)
{
  GumAddress lhs;
  GumAddress rhs;

  lhs = gum_module_get_range (*lhs_module)->base_address;
  rhs = gum_module_get_range (*rhs_module)->base_address;

  if (lhs < rhs)
    return -1;

  if (lhs > rhs)
    return 1;

  return 0;
}

static gint
gum_module_compare_to_key (const GumAddress * key_ptr,
                           GumModule ** member)
{
  GumAddress key = *key_ptr;
  const GumMemoryRange * r;

  r = gum_module_get_range (*member);

  if (key < r->base_address)
    return -1;

  if (key >= r->base_address + r->size)
    return 1;

  return 0;
}
