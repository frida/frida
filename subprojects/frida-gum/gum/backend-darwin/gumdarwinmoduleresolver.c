/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum/gumdarwinmoduleresolver.h"

#include "gumdarwin-priv.h"
#include "gummodule-darwin.h"
#include "gum/gumdarwin.h"

#include <stdlib.h>
#include <mach-o/loader.h>

#define GUM_DARWIN_MODULE_RESOLVER_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_DARWIN_MODULE_RESOLVER_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

enum
{
  PROP_0,
  PROP_TASK
};

static void gum_darwin_module_resolver_dispose (GObject * object);
static void gum_darwin_module_resolver_finalize (GObject * object);
static void gum_darwin_module_resolver_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_darwin_module_resolver_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static GPtrArray * gum_darwin_module_resolver_do_load (
    GumDarwinModuleResolver * self, GError ** error);
static void gum_darwin_module_resolver_rebuild_indexes (
    GumDarwinModuleResolver * self, GPtrArray * latest_modules);

static gint gum_darwin_module_compare_base (GumDarwinModule ** lhs_module,
    GumDarwinModule ** rhs_module);
static gint gum_darwin_module_compare_to_key (const GumAddress * key_ptr,
    GumDarwinModule ** member);

G_DEFINE_TYPE (GumDarwinModuleResolver,
               gum_darwin_module_resolver,
               G_TYPE_OBJECT)

static void
gum_darwin_module_resolver_class_init (GumDarwinModuleResolverClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_darwin_module_resolver_dispose;
  object_class->finalize = gum_darwin_module_resolver_finalize;
  object_class->get_property = gum_darwin_module_resolver_get_property;
  object_class->set_property = gum_darwin_module_resolver_set_property;

  g_object_class_install_property (object_class, PROP_TASK,
      g_param_spec_uint ("task", "task", "Mach task", 0, G_MAXUINT,
      MACH_PORT_NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
}

static void
gum_darwin_module_resolver_init (GumDarwinModuleResolver * self)
{
  g_mutex_init (&self->mutex);

  self->state = GUM_DARWIN_MODULE_RESOLVER_CREATED;
}

static void
gum_darwin_module_resolver_dispose (GObject * object)
{
  GumDarwinModuleResolver * self = GUM_DARWIN_MODULE_RESOLVER (object);

  gum_darwin_module_resolver_set_dynamic_lookup_handler (self, NULL, NULL,
      NULL);

  if (self->load_data_destroy != NULL)
    self->load_data_destroy (self->load_data);
  self->load_func = NULL;
  self->load_data = NULL;
  self->load_data_destroy = NULL;

  g_clear_pointer (&self->module_by_name, g_hash_table_unref);
  g_clear_pointer (&self->sorted_modules, g_ptr_array_unref);
  g_clear_pointer (&self->last_modules, g_ptr_array_unref);

  G_OBJECT_CLASS (gum_darwin_module_resolver_parent_class)->dispose (object);
}

static void
gum_darwin_module_resolver_finalize (GObject * object)
{
  GumDarwinModuleResolver * self = GUM_DARWIN_MODULE_RESOLVER (object);

  g_free (self->sysroot);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_darwin_module_resolver_parent_class)->finalize (object);
}

static void
gum_darwin_module_resolver_get_property (GObject * object,
                                         guint property_id,
                                         GValue * value,
                                         GParamSpec * pspec)
{
  GumDarwinModuleResolver * self = GUM_DARWIN_MODULE_RESOLVER (object);

  switch (property_id)
  {
    case PROP_TASK:
      g_value_set_uint (value, self->task);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_darwin_module_resolver_set_property (GObject * object,
                                         guint property_id,
                                         const GValue * value,
                                         GParamSpec * pspec)
{
  GumDarwinModuleResolver * self = GUM_DARWIN_MODULE_RESOLVER (object);

  switch (property_id)
  {
    case PROP_TASK:
      self->task = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumDarwinModuleResolver *
gum_darwin_module_resolver_new (mach_port_t task,
                                GError ** error)
{
  GumDarwinModuleResolver * resolver;

  resolver = g_object_new (GUM_DARWIN_TYPE_MODULE_RESOLVER,
      "task", task,
      NULL);

  if (!gum_darwin_module_resolver_load (resolver, error))
  {
    g_object_unref (resolver);
    resolver = NULL;
  }

  return resolver;
}

GumDarwinModuleResolver *
gum_darwin_module_resolver_new_with_loader (
    mach_port_t task,
    GumDarwinModuleResolverLoadFunc func,
    gpointer data,
    GDestroyNotify data_destroy,
    GError ** error)
{
  GumDarwinModuleResolver * resolver;

  resolver = g_object_new (GUM_DARWIN_TYPE_MODULE_RESOLVER,
      "task", task,
      NULL);
  resolver->load_func = func;
  resolver->load_data = data;
  resolver->load_data_destroy = data_destroy;

  if (!gum_darwin_module_resolver_load (resolver, error))
  {
    g_object_unref (resolver);
    resolver = NULL;
  }

  return resolver;
}

gboolean
gum_darwin_module_resolver_load (GumDarwinModuleResolver * self,
                                 GError ** error)
{
  gboolean success = FALSE;
  int pid;

  GUM_DARWIN_MODULE_RESOLVER_LOCK (self);

  if (self->state != GUM_DARWIN_MODULE_RESOLVER_CREATED)
    goto beach;

  if (!gum_darwin_query_ptrauth_support (self->task, &self->ptrauth_support))
    goto invalid_task;

  if (!gum_darwin_query_page_size (self->task, &self->page_size))
    goto invalid_task;

  if (self->task == mach_task_self ())
  {
    self->cpu_type = GUM_NATIVE_CPU;
  }
  else
  {
    if (pid_for_task (self->task, &pid) != KERN_SUCCESS)
      goto invalid_task;

    if (!gum_darwin_cpu_type_from_pid (pid, &self->cpu_type))
      goto invalid_task;
  }

  if (self->load_func == NULL)
  {
    GPtrArray * modules = gum_darwin_module_resolver_do_load (self, error);
    if (modules == NULL)
      goto beach;

    self->load_func = (GumDarwinModuleResolverLoadFunc) g_ptr_array_ref;
    self->load_data = modules;
    self->load_data_destroy = (GDestroyNotify) g_ptr_array_unref;
  }

  self->state = GUM_DARWIN_MODULE_RESOLVER_LOADED;

  success = TRUE;
  goto beach;

invalid_task:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Process is dead");
    goto beach;
  }
beach:
  {
    GUM_DARWIN_MODULE_RESOLVER_UNLOCK (self);

    return success;
  }
}

static GPtrArray *
gum_darwin_module_resolver_do_load (GumDarwinModuleResolver * self,
                                    GError ** error)
{
  GPtrArray * modules;
  GumDarwinImageSnapshot * snapshot;
  GumDarwinImageIter iter;
  const GumDarwinImage * image;

  snapshot = gum_darwin_snapshot_images (self->task, error);
  if (snapshot == NULL)
    return NULL;

  self->sysroot = gum_darwin_image_snapshot_infer_sysroot (snapshot);

  modules = g_ptr_array_new_full (64, g_object_unref);

  gum_darwin_image_iter_init (&iter, snapshot);
  while (gum_darwin_image_iter_next (&iter, &image))
  {
    g_ptr_array_add (modules,
        _gum_native_module_make (image->path, &image->range, self));
  }

  gum_darwin_image_snapshot_unref (snapshot);

  return modules;
}

void
gum_darwin_module_resolver_set_dynamic_lookup_handler (
    GumDarwinModuleResolver * self,
    GumDarwinModuleResolverLookupFunc func,
    gpointer data,
    GDestroyNotify data_destroy)
{
  if (self->lookup_dynamic_data_destroy != NULL)
    self->lookup_dynamic_data_destroy (self->lookup_dynamic_data);

  self->lookup_dynamic_func = func;
  self->lookup_dynamic_data = data;
  self->lookup_dynamic_data_destroy = data_destroy;
}

void
gum_darwin_module_resolver_fetch_modules (GumDarwinModuleResolver * self,
                                          GPtrArray ** sorted_modules,
                                          GHashTable ** module_by_name)
{
  GPtrArray * latest;

  latest = self->load_func (self->load_data);

  GUM_DARWIN_MODULE_RESOLVER_LOCK (self);

  if (latest == self->last_modules)
  {
    g_ptr_array_unref (latest);
  }
  else
  {
    gum_darwin_module_resolver_rebuild_indexes (self, latest);
    g_clear_pointer (&self->last_modules, g_ptr_array_unref);
    self->last_modules = latest;
  }

  if (sorted_modules != NULL)
    *sorted_modules = g_ptr_array_ref (self->sorted_modules);

  if (module_by_name != NULL)
    *module_by_name = g_hash_table_ref (self->module_by_name);

  GUM_DARWIN_MODULE_RESOLVER_UNLOCK (self);
}

static void
gum_darwin_module_resolver_rebuild_indexes (GumDarwinModuleResolver * self,
                                            GPtrArray * latest_modules)
{
  GPtrArray * sorted;
  GHashTable * by_name;
  gsize sysroot_length;
  guint i;

  sorted = g_ptr_array_new_full (latest_modules->len, g_object_unref);
  by_name = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  sysroot_length = (self->sysroot != NULL) ? strlen (self->sysroot) : 0;

  for (i = 0; i != latest_modules->len; i++)
  {
    GumModule * mod;
    const gchar * path;

    mod = g_ptr_array_index (latest_modules, i);
    path = gum_module_get_path (mod);

    g_ptr_array_add (sorted, g_object_ref (mod));

    g_hash_table_insert (by_name, g_strdup (gum_module_get_name (mod)), mod);
    g_hash_table_insert (by_name, g_strdup (path), mod);
  }

  if (sysroot_length != 0)
  {
    for (i = 0; i != latest_modules->len; i++)
    {
      GumModule * mod;
      const gchar * path;

      mod = g_ptr_array_index (latest_modules, i);
      path = gum_module_get_path (mod);

      if (g_str_has_prefix (path, self->sysroot))
      {
        g_hash_table_insert (by_name,
            g_strdup (gum_module_get_name (mod)), mod);
        g_hash_table_insert (by_name, g_strdup (path + sysroot_length), mod);
      }
    }
  }

  g_ptr_array_sort (sorted, (GCompareFunc) gum_darwin_module_compare_base);

  g_clear_pointer (&self->sorted_modules, g_ptr_array_unref);
  self->sorted_modules = sorted;

  g_clear_pointer (&self->module_by_name, g_hash_table_unref);
  self->module_by_name = by_name;
}

GumDarwinModule *
gum_darwin_module_resolver_find_module_by_name (GumDarwinModuleResolver * self,
                                                const gchar * name)
{
  GumDarwinModule * result;
  GHashTable * module_by_name;
  GumNativeModule * module;

  gum_darwin_module_resolver_fetch_modules (self, NULL, &module_by_name);

  module = g_hash_table_lookup (module_by_name, name);
  if (module == NULL && g_str_has_prefix (name, "/usr/lib/system/"))
  {
    gchar * alias =
        g_strconcat ("/usr/lib/system/introspection/", name + 16, NULL);

    module = g_hash_table_lookup (module_by_name, alias);

    g_free (alias);
  }

  result = (module != NULL)
      ? g_object_ref (_gum_native_module_get_darwin_module (module))
      : NULL;

  g_hash_table_unref (module_by_name);

  return result;
}

GumDarwinModule *
gum_darwin_module_resolver_find_module_by_address (
    GumDarwinModuleResolver * self,
    GumAddress address)
{
  GumDarwinModule * result;
  GPtrArray * modules;
  GumAddress bare_address;
  GumNativeModule ** entry;

  gum_darwin_module_resolver_fetch_modules (self, &modules, NULL);

  bare_address = gum_strip_code_address (address);

  entry = bsearch (&bare_address, modules->pdata, modules->len,
      sizeof (GumModule *), (GCompareFunc) gum_darwin_module_compare_to_key);
  result = (entry != NULL)
      ? g_object_ref (_gum_native_module_get_darwin_module (*entry))
      : NULL;

  g_ptr_array_unref (modules);

  return result;
}

gboolean
gum_darwin_module_resolver_find_export (GumDarwinModuleResolver * self,
                                        GumDarwinModule * module,
                                        const gchar * symbol,
                                        GumExportDetails * details)
{
  gchar * mangled_symbol;
  gboolean success;

  mangled_symbol = g_strconcat ("_", symbol, NULL);
  success = gum_darwin_module_resolver_find_export_by_mangled_name (self,
      module, mangled_symbol, details);
  g_free (mangled_symbol);

  return success;
}

GumAddress
gum_darwin_module_resolver_find_export_address (GumDarwinModuleResolver * self,
                                                GumDarwinModule * module,
                                                const gchar * symbol)
{
  GumExportDetails details;

  if (!gum_darwin_module_resolver_find_export (self, module, symbol, &details))
    return 0;

  return details.address;
}

gboolean
gum_darwin_module_resolver_find_export_by_mangled_name (
    GumDarwinModuleResolver * self,
    GumDarwinModule * module,
    const gchar * symbol,
    GumExportDetails * details)
{
  GumDarwinModule * m;
  GumDarwinExportDetails d;
  gboolean found;

  found = gum_darwin_module_resolve_export (module, symbol, &d);
  if (found)
  {
    m = module;
  }
  else if (gum_darwin_module_get_lacks_exports_for_reexports (module))
  {
    GPtrArray * reexports = module->reexports;
    guint i;

    for (i = 0; !found && i != reexports->len; i++)
    {
      GumDarwinModule * reexport;

      reexport = gum_darwin_module_resolver_find_module_by_name (self,
          g_ptr_array_index (reexports, i));
      if (reexport != NULL)
      {
        found = gum_darwin_module_resolve_export (reexport, symbol, &d);
        if (found)
          m = reexport;

        g_object_unref (reexport);
      }
    }

    if (!found)
      return FALSE;
  }
  else
  {
    return FALSE;
  }

  return gum_darwin_module_resolver_resolve_export (self, m, &d, details);
}

gboolean
gum_darwin_module_resolver_resolve_export (
    GumDarwinModuleResolver * self,
    GumDarwinModule * module,
    const GumDarwinExportDetails * export,
    GumExportDetails * result)
{
  if ((export->flags & EXPORT_SYMBOL_FLAGS_REEXPORT) != 0)
  {
    const gchar * target_module_name;
    GumDarwinModule * target_module;
    gboolean is_reexporting_itself, success;

    target_module_name = gum_darwin_module_get_dependency_by_ordinal (module,
        export->reexport_library_ordinal);
    target_module = gum_darwin_module_resolver_find_module_by_name (self,
        target_module_name);
    if (target_module == NULL)
      return FALSE;

    is_reexporting_itself = (target_module == module &&
        strcmp (export->reexport_symbol, export->name) == 0);
    if (is_reexporting_itself)
    {
      /*
       * Happens with a few of the Security.framework exports on High Sierra
       * beta 4, and seems like a bug given that dlsym() crashes with a
       * stack-overflow when asked to resolve these.
       */
      success = FALSE;
    }
    else
    {
      success = gum_darwin_module_resolver_find_export_by_mangled_name (self,
          target_module, export->reexport_symbol, result);
    }

    g_object_unref (target_module);

    return success;
  }

  result->name = gum_symbol_name_from_darwin (export->name);

  switch (export->flags & GUM_DARWIN_EXPORT_KIND_MASK)
  {
    case GUM_DARWIN_EXPORT_REGULAR:
      if ((export->flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER) != 0)
      {
        /* XXX: we ignore resolver and interposing */
        result->address = module->base_address + export->stub;
      }
      else
      {
        result->address = module->base_address + export->offset;
      }
      break;
    case GUM_DARWIN_EXPORT_THREAD_LOCAL:
      result->address = module->base_address + export->offset;
      break;
    case GUM_DARWIN_EXPORT_ABSOLUTE:
      result->address = export->offset;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  result->type =
      gum_darwin_module_is_address_in_text_section (module, result->address)
      ? GUM_EXPORT_FUNCTION
      : GUM_EXPORT_VARIABLE;

  if (result->type == GUM_EXPORT_FUNCTION &&
      self->ptrauth_support == GUM_PTRAUTH_SUPPORTED)
  {
    result->address = gum_sign_code_address (result->address);
  }

  result->size = -1;

  return TRUE;
}

GumAddress
gum_darwin_module_resolver_find_dynamic_address (GumDarwinModuleResolver * self,
                                                 const gchar * symbol)
{
  if (self->lookup_dynamic_func != NULL)
    return self->lookup_dynamic_func (symbol, self->lookup_dynamic_data);

  return 0;
}

static gint
gum_darwin_module_compare_base (GumDarwinModule ** lhs_module,
                                GumDarwinModule ** rhs_module)
{
  GumAddress lhs;
  GumAddress rhs;

  lhs = (*lhs_module)->base_address;
  rhs = (*rhs_module)->base_address;

  if (lhs < rhs)
    return -1;

  if (lhs > rhs)
    return 1;

  return 0;
}

static gint
gum_darwin_module_compare_to_key (const GumAddress * key_ptr,
                                  GumDarwinModule ** member)
{
  GumAddress key = *key_ptr;
  GumDarwinModule * module = *member;

  if (key < module->base_address)
    return -1;

  if (key >= module->base_address + module->text_size)
    return 1;

  return 0;
}
