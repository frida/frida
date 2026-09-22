/*
 * Copyright (C) 2016-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2020 Grant Douglas <grant@reconditorium.uk>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/**
 * GumModuleApiResolver:
 *
 * Resolves APIs by searching exports, imports, and sections of currently loaded
 * modules.
 *
 * See [iface@Gum.ApiResolver] for more information.
 */

#include "gummoduleapiresolver.h"

#include "gummodulemap.h"
#include "gumprocess.h"

#include <string.h>

typedef struct _GumModuleMetadata GumModuleMetadata;
typedef struct _GumFunctionMetadata GumFunctionMetadata;

struct _GumModuleApiResolver
{
  GObject parent;

  GRegex * query_pattern;

  GumModuleMap * all_modules;
  GHashTable * module_by_name;
};

struct _GumModuleMetadata
{
  gint ref_count;

  GumModule * module;

  GHashTable * import_by_name;
  GHashTable * export_by_name;
  GArray * sections;
};

struct _GumFunctionMetadata
{
  gchar * name;
  GumAddress address;
  gchar * module;
};

static void gum_module_api_resolver_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_module_api_resolver_finalize (GObject * object);
static void gum_module_api_resolver_enumerate_matches (
    GumApiResolver * resolver, const gchar * query, GumFoundApiFunc func,
    gpointer user_data, GError ** error);

static void gum_module_metadata_unref (GumModuleMetadata * module);
static GHashTable * gum_module_metadata_get_imports (GumModuleMetadata * self);
static GHashTable * gum_module_metadata_get_exports (GumModuleMetadata * self);
static GArray * gum_module_metadata_get_sections (GumModuleMetadata * self);
static gboolean gum_module_metadata_collect_import (
    const GumImportDetails * details, gpointer user_data);
static gboolean gum_module_metadata_collect_export (
    const GumExportDetails * details, gpointer user_data);
static gboolean gum_module_metadata_collect_section (
    const GumSectionDetails * details, gpointer user_data);

static GumFunctionMetadata * gum_function_metadata_new (const gchar * name,
    GumAddress address, const gchar * module);
static void gum_function_metadata_free (GumFunctionMetadata * function);

static void gum_section_details_free (GumSectionDetails * self);

G_DEFINE_TYPE_EXTENDED (GumModuleApiResolver,
                        gum_module_api_resolver,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_API_RESOLVER,
                            gum_module_api_resolver_iface_init))

static void
gum_module_api_resolver_class_init (GumModuleApiResolverClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_module_api_resolver_finalize;
}

static void
gum_module_api_resolver_iface_init (gpointer g_iface,
                                    gpointer iface_data)
{
  GumApiResolverInterface * iface = g_iface;

  iface->enumerate_matches = gum_module_api_resolver_enumerate_matches;
}

static void
gum_module_api_resolver_init (GumModuleApiResolver * self)
{
  GPtrArray * entries;
  guint i;

  self->query_pattern =
      g_regex_new ("(imports|exports|sections):(.+)!([^\\n\\r\\/]+)(\\/i)?",
          0, 0, NULL);

  self->all_modules = gum_module_map_new ();
  self->module_by_name = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) gum_module_metadata_unref);
  entries = gum_module_map_get_values (self->all_modules);
  for (i = 0; i != entries->len; i++)
  {
    GumModule * module;
    GumModuleMetadata * meta;

    module = g_ptr_array_index (entries, i);

    meta = g_slice_new (GumModuleMetadata);
    meta->ref_count = 2;
    meta->module = g_object_ref (module);
    meta->import_by_name = NULL;
    meta->export_by_name = NULL;
    meta->sections = NULL;

    g_hash_table_insert (self->module_by_name,
        (gpointer) gum_module_get_name (module), meta);
    g_hash_table_insert (self->module_by_name,
        (gpointer) gum_module_get_path (module), meta);
  }
}

static void
gum_module_api_resolver_finalize (GObject * object)
{
  GumModuleApiResolver * self = GUM_MODULE_API_RESOLVER (object);

  g_hash_table_unref (self->module_by_name);
  g_object_unref (self->all_modules);

  g_regex_unref (self->query_pattern);

  G_OBJECT_CLASS (gum_module_api_resolver_parent_class)->finalize (object);
}

/**
 * gum_module_api_resolver_new:
 *
 * Creates a new resolver that searches exports and imports of currently loaded
 * modules.
 *
 * Returns: (transfer full): the newly created resolver instance
 */
GumApiResolver *
gum_module_api_resolver_new (void)
{
  return g_object_new (GUM_TYPE_MODULE_API_RESOLVER, NULL);
}

static void
gum_module_api_resolver_enumerate_matches (GumApiResolver * resolver,
                                           const gchar * query,
                                           GumFoundApiFunc func,
                                           gpointer user_data,
                                           GError ** error)
{
  GumModuleApiResolver * self = GUM_MODULE_API_RESOLVER (resolver);
  GMatchInfo * query_info;
  gboolean ignore_case;
  gchar * collection, * module_query, * item_query;
  gboolean no_patterns_in_item_query;
  GPatternSpec * module_spec, * item_spec;
  GHashTableIter module_iter;
  GHashTable * seen_modules;
  gboolean carry_on;
  GumModuleMetadata * module;

  g_regex_match (self->query_pattern, query, 0, &query_info);
  if (!g_match_info_matches (query_info))
    goto invalid_query;

  ignore_case = g_match_info_get_match_count (query_info) >= 5;

  collection = g_match_info_fetch (query_info, 1);
  module_query = g_match_info_fetch (query_info, 2);
  item_query = g_match_info_fetch (query_info, 3);

  g_match_info_free (query_info);

  if (ignore_case)
  {
    gchar * str;

    str = g_utf8_strdown (module_query, -1);
    g_free (module_query);
    module_query = str;

    str = g_utf8_strdown (item_query, -1);
    g_free (item_query);
    item_query = str;
  }

  no_patterns_in_item_query =
      !ignore_case &&
      strchr (item_query, '*') == NULL &&
      strchr (item_query, '?') == NULL;

  module_spec = g_pattern_spec_new (module_query);
  item_spec = g_pattern_spec_new (item_query);

  g_hash_table_iter_init (&module_iter, self->module_by_name);
  seen_modules = g_hash_table_new (NULL, NULL);
  carry_on = TRUE;

  while (carry_on &&
      g_hash_table_iter_next (&module_iter, NULL, (gpointer *) &module))
  {
    const gchar * module_name, * module_path;
    const gchar * normalized_module_name, * normalized_module_path;
    gchar * module_name_copy = NULL;
    gchar * module_path_copy = NULL;

    if (g_hash_table_contains (seen_modules, module))
      continue;
    g_hash_table_add (seen_modules, module);

    module_name = gum_module_get_name (module->module);
    module_path = gum_module_get_path (module->module);

    if (ignore_case)
    {
      module_name_copy = g_utf8_strdown (module_name, -1);
      normalized_module_name = module_name_copy;

      module_path_copy = g_utf8_strdown (module_path, -1);
      normalized_module_path = module_path_copy;
    }
    else
    {
      normalized_module_name = module_name;
      normalized_module_path = module_path;
    }

    if (g_pattern_spec_match_string (module_spec, normalized_module_name) ||
        g_pattern_spec_match_string (module_spec, normalized_module_path))
    {
      GHashTable * functions;
      GHashTableIter function_iter;
      GumFunctionMetadata * function;

      if (collection[0] == 's')
      {
        GArray * sections;
        guint i;

        sections = gum_module_metadata_get_sections (module);
        for (i = 0; i != sections->len; i++)
        {
          const GumSectionDetails * section = &g_array_index (sections,
              GumSectionDetails, i);

          if (g_pattern_spec_match_string (item_spec, section->name))
          {
            GumApiDetails details;

            details.name = g_strconcat (
                module_path,
                "!",
                section->id,
                NULL);
            details.address = section->address;
            details.size = section->size;

            carry_on = func (&details, user_data);

            g_free ((gpointer) details.name);
          }
        }

        continue;
      }

      if (collection[0] == 'e' && no_patterns_in_item_query)
      {
        GumApiDetails details;

        details.address =
            gum_module_find_export_by_name (module->module, item_query);
        details.size = GUM_API_SIZE_NONE;

#ifndef HAVE_WINDOWS
        if (details.address != 0)
        {
          if (gum_module_map_find (self->all_modules, details.address) !=
              module->module)
            details.address = 0;
        }
#endif

        if (details.address != 0)
        {
          details.name = g_strconcat (module_path, "!", item_query, NULL);

          carry_on = func (&details, user_data);

          g_free ((gpointer) details.name);
        }

        g_assert (module_name_copy == NULL && module_path_copy == NULL);

        continue;
      }

      functions = (collection[0] == 'i')
          ? gum_module_metadata_get_imports (module)
          : gum_module_metadata_get_exports (module);

      g_hash_table_iter_init (&function_iter, functions);
      while (carry_on &&
          g_hash_table_iter_next (&function_iter, NULL, (gpointer *) &function))
      {
        const gchar * function_name = function->name;
        gchar * function_name_copy = NULL;

        if (ignore_case)
        {
          function_name_copy = g_utf8_strdown (function_name, -1);
          function_name = function_name_copy;
        }

        if (g_pattern_spec_match_string (item_spec, function_name))
        {
          GumApiDetails details;

          details.name = g_strconcat (
              (function->module != NULL) ? function->module : module_path,
              "!",
              function->name,
              NULL);
          details.address = function->address;
          details.size = GUM_API_SIZE_NONE;

          carry_on = func (&details, user_data);

          g_free ((gpointer) details.name);
        }

        g_free (function_name_copy);
      }
    }

    g_free (module_path_copy);
    g_free (module_name_copy);
  }

  g_hash_table_unref (seen_modules);

  g_pattern_spec_free (item_spec);
  g_pattern_spec_free (module_spec);

  g_free (item_query);
  g_free (module_query);
  g_free (collection);

  return;

invalid_query:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "invalid query; format is: "
        "exports:*!open*, exports:libc.so!*, imports:notepad.exe!*, "
        "or sections:libc.so!*data*");
  }
}

static void
gum_module_metadata_unref (GumModuleMetadata * meta)
{
  meta->ref_count--;
  if (meta->ref_count == 0)
  {
    if (meta->sections != NULL)
      g_array_unref (meta->sections);

    if (meta->export_by_name != NULL)
      g_hash_table_unref (meta->export_by_name);

    if (meta->import_by_name != NULL)
      g_hash_table_unref (meta->import_by_name);

    g_object_unref (meta->module);

    g_slice_free (GumModuleMetadata, meta);
  }
}

static GHashTable *
gum_module_metadata_get_imports (GumModuleMetadata * self)
{
  if (self->import_by_name == NULL)
  {
    self->import_by_name = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, (GDestroyNotify) gum_function_metadata_free);
    gum_module_enumerate_imports (self->module,
        gum_module_metadata_collect_import, self->import_by_name);
  }

  return self->import_by_name;
}

static GHashTable *
gum_module_metadata_get_exports (GumModuleMetadata * self)
{
  if (self->export_by_name == NULL)
  {
    self->export_by_name = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, (GDestroyNotify) gum_function_metadata_free);
    gum_module_enumerate_exports (self->module,
        gum_module_metadata_collect_export, self->export_by_name);
  }

  return self->export_by_name;
}

static GArray *
gum_module_metadata_get_sections (GumModuleMetadata * self)
{
  if (self->sections == NULL)
  {
    self->sections = g_array_new (FALSE, FALSE, sizeof (GumSectionDetails));
    g_array_set_clear_func (self->sections,
        (GDestroyNotify) gum_section_details_free);
    gum_module_enumerate_sections (self->module,
        gum_module_metadata_collect_section, self->sections);
  }

  return self->sections;
}

static gboolean
gum_module_metadata_collect_import (const GumImportDetails * details,
                                    gpointer user_data)
{
  GHashTable * import_by_name = user_data;

  if (details->type == GUM_IMPORT_FUNCTION && details->address != 0)
  {
    GumFunctionMetadata * function;

    function = gum_function_metadata_new (details->name, details->address,
        details->module);
    g_hash_table_insert (import_by_name, g_strdup (function->name), function);
  }

  return TRUE;
}

static gboolean
gum_module_metadata_collect_export (const GumExportDetails * details,
                                    gpointer user_data)
{
  GHashTable * export_by_name = user_data;

  if (details->type == GUM_EXPORT_FUNCTION)
  {
    GumFunctionMetadata * function;

    function = gum_function_metadata_new (details->name, details->address,
        NULL);
    g_hash_table_insert (export_by_name, g_strdup (function->name), function);
  }

  return TRUE;
}

static gboolean
gum_module_metadata_collect_section (const GumSectionDetails * details,
                                     gpointer user_data)
{
  GArray * sections = user_data;
  GumSectionDetails d;

  d = *details;
  d.id = g_strdup (d.id);
  d.name = g_strdup (d.name);
  g_array_append_val (sections, d);

  return TRUE;
}

static GumFunctionMetadata *
gum_function_metadata_new (const gchar * name,
                           GumAddress address,
                           const gchar * module)
{
  GumFunctionMetadata * function;

  function = g_slice_new (GumFunctionMetadata);
  function->name = g_strdup (name);
  function->address = address;
  function->module = g_strdup (module);

  return function;
}

static void
gum_function_metadata_free (GumFunctionMetadata * meta)
{
  g_free (meta->module);
  g_free (meta->name);

  g_slice_free (GumFunctionMetadata, meta);
}

static void
gum_section_details_free (GumSectionDetails * section)
{
  g_free ((gpointer) section->id);
  g_free ((gpointer) section->name);
}
