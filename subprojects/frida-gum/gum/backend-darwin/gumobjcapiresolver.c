/*
 * Copyright (C) 2016-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2020 Grant Douglas <grant@reconditorium.uk>
 * Copyright (C)      2021 Abdelrahman Eid <hot3eed@gmail.com>
 * Copyright (C) 2021-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumobjcapiresolver.h"

#include "gumdarwinmodule.h"
#include "guminterceptor.h"
#include "gummetalarray.h"
#include "gummodule-darwin.h"
#include "gummoduleregistry.h"
#include "gumobjcapiresolver-priv.h"
#include "gumobjcdisposeclasspairmonitor.h"
#include "gumprocess.h"

#include <dlfcn.h>
#include <objc/runtime.h>
#include <stdlib.h>

typedef struct _GumObjcClassMetadata GumObjcClassMetadata;
typedef struct _GumObjcSectionContext GumObjcSectionContext;
typedef struct _GumCategoryHeader GumCategoryHeader;
typedef void (* GumLibcFreeFunc) (gpointer mem);

struct _GumObjcApiResolver
{
  GObject parent;

  GRegex * query_pattern;

  gboolean available;
  GHashTable * class_by_handle;
  GHashTable * classes_by_module;
  GRecMutex class_cache_mutex;
  GumObjcDisposeClassPairMonitor * monitor;
  GumModuleRegistry * registry;
  gulong on_removed_handler;

  gint (* objc_getClassList) (Class * buffer, gint class_count);
  Class (* objc_lookUpClass) (const gchar * name);
  Class (* class_getSuperclass) (Class klass);
  const gchar * (* class_getName) (Class klass);
  Method * (* class_copyMethodList) (Class klass, guint * method_count);
  Class (* object_getClass) (gpointer object);
  SEL (* method_getName) (Method method);
  IMP (* method_getImplementation) (Method method);
  const gchar * (* sel_getName) (SEL selector);
};

struct _GumObjcClassMetadata
{
  Class handle;
  const gchar * name;
  const gchar * category_name;

  Method * class_methods;
  guint class_method_count;

  Method * instance_methods;
  guint instance_method_count;

  GSList * subclasses;

  GumObjcApiResolver * resolver;
};

struct _GumObjcSectionContext
{
  const gchar * name;
  GumAddress address;
  gsize size;
};

struct _GumCategoryHeader
{
  const gchar * name;
  Class target_handle;
};

static void gum_objc_api_resolver_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_objc_api_resolver_dispose (GObject * object);
static void gum_objc_api_resolver_finalize (GObject * object);
static void gum_objc_api_resolver_enumerate_matches (GumApiResolver * resolver,
    const gchar * query, GumFoundApiFunc func, gpointer user_data,
    GError ** error);
static gboolean gum_objc_api_resolver_enumerate_matches_for_class (
    GumObjcApiResolver * self, GumObjcClassMetadata * klass, gchar method_type,
    GPatternSpec * method_spec, GHashTable * visited_classes,
    GHashTable * class_by_handle, gboolean ignore_case, GumFoundApiFunc func,
    gpointer user_data);

static GumMetalArray * gum_objc_api_resolver_get_classes_by_module (
    GumObjcApiResolver * self, GumModule * module);
static void gum_objc_api_resolver_collect_class_list (GumObjcApiResolver * self,
    GumModule * module, GumMetalArray * classes);
static void gum_objc_api_resolver_collect_category_list (
    GumObjcApiResolver * self, GumModule * module, GumMetalArray * classes);
static GumObjcClassMetadata * gum_objc_api_resolver_append_class (
    GumObjcApiResolver * self, GumMetalArray * classes, Class handle);
static gboolean gum_find_objc_section (const GumSectionDetails * details,
    GumObjcSectionContext * ctx);
static void gum_objc_classes_array_free (GumMetalArray * classes_array);
static void gum_schedule_updates_on_module_removed (GumObjcApiResolver * self);
static void gum_clear_classes_by_module_on_module_removed (
    GumModuleRegistry * registry, GumModule * module, gpointer user_data);

static gchar gum_method_type_from_match_info (GMatchInfo * match_info,
    gint match_num);
static GPatternSpec * gum_pattern_spec_from_match_info (GMatchInfo * match_info,
    gint match_num, gboolean ignore_case);

static GHashTable * gum_objc_api_resolver_create_snapshot (
    GumObjcApiResolver * resolver);

static void gum_objc_class_metadata_free (GumObjcClassMetadata * klass);
static void gum_objc_class_metadata_free_data (GumObjcClassMetadata * klass);
static const Method * gum_objc_class_metadata_get_methods (
    GumObjcClassMetadata * self, gchar type, guint * count);
static gboolean gum_objc_class_metadata_is_disposed (
    GumObjcClassMetadata * self);

G_DEFINE_TYPE_EXTENDED (GumObjcApiResolver,
                        gum_objc_api_resolver,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_API_RESOLVER,
                            gum_objc_api_resolver_iface_init))

static GumLibcFreeFunc gum_libc_free;

static void
gum_objc_api_resolver_class_init (GumObjcApiResolverClass * klass)
{
  GObjectClass * object_class;
  GumModule * libsystem_malloc;

  object_class = G_OBJECT_CLASS (klass);

  libsystem_malloc = gum_process_find_module_by_name (
      "/usr/lib/system/libsystem_malloc.dylib");
  gum_libc_free = (GumLibcFreeFunc) gum_module_find_export_by_name (
      libsystem_malloc,
      "free");
  g_object_unref (libsystem_malloc);

  object_class->dispose = gum_objc_api_resolver_dispose;
  object_class->finalize = gum_objc_api_resolver_finalize;
}

static void
gum_objc_api_resolver_iface_init (gpointer g_iface,
                                  gpointer iface_data)
{
  GumApiResolverInterface * iface = g_iface;

  iface->enumerate_matches = gum_objc_api_resolver_enumerate_matches;
}

static void
gum_objc_api_resolver_init (GumObjcApiResolver * self)
{
  gpointer objc;

  self->query_pattern = g_regex_new ("([+*-])\\[(\\S+)\\s+(\\S+)\\](\\/i)?", 0,
      0, NULL);

  objc = dlopen ("/usr/lib/libobjc.A.dylib",
      RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD);
  if (objc == NULL)
    goto beach;

#define GUM_TRY_ASSIGN_OBJC_FUNC(N) \
    self->N = dlsym (objc, G_STRINGIFY (N)); \
    if (self->N == NULL) \
      goto beach

  GUM_TRY_ASSIGN_OBJC_FUNC (objc_getClassList);
  GUM_TRY_ASSIGN_OBJC_FUNC (objc_lookUpClass);
  GUM_TRY_ASSIGN_OBJC_FUNC (class_getSuperclass);
  GUM_TRY_ASSIGN_OBJC_FUNC (class_getName);
  GUM_TRY_ASSIGN_OBJC_FUNC (class_copyMethodList);
  GUM_TRY_ASSIGN_OBJC_FUNC (object_getClass);
  GUM_TRY_ASSIGN_OBJC_FUNC (method_getName);
  GUM_TRY_ASSIGN_OBJC_FUNC (method_getImplementation);
  GUM_TRY_ASSIGN_OBJC_FUNC (sel_getName);

  self->available = TRUE;
  self->monitor = gum_objc_dispose_class_pair_monitor_obtain ();

  g_rec_mutex_init (&self->class_cache_mutex);

beach:
  if (objc != NULL)
    dlclose (objc);
}

static void
gum_objc_api_resolver_dispose (GObject * object)
{
  GumObjcApiResolver * self = GUM_OBJC_API_RESOLVER (object);

  if (self->on_removed_handler != 0)
  {
    g_signal_handler_disconnect (self->registry, self->on_removed_handler);
    self->registry = NULL;
    self->on_removed_handler = 0;
  }

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gum_objc_api_resolver_parent_class)->dispose (object);
}

static void
gum_objc_api_resolver_finalize (GObject * object)
{
  GumObjcApiResolver * self = GUM_OBJC_API_RESOLVER (object);

  g_clear_pointer (&self->class_by_handle, g_hash_table_unref);
  g_clear_pointer (&self->classes_by_module, g_hash_table_unref);
  g_rec_mutex_clear (&self->class_cache_mutex);

  g_regex_unref (self->query_pattern);

  G_OBJECT_CLASS (gum_objc_api_resolver_parent_class)->finalize (object);
}

GumApiResolver *
gum_objc_api_resolver_new (void)
{
  GumObjcApiResolver * resolver;

  resolver = g_object_new (GUM_TYPE_OBJC_API_RESOLVER, NULL);
  if (!resolver->available)
  {
    g_object_unref (resolver);
    return NULL;
  }

  return GUM_API_RESOLVER (resolver);
}

static void
gum_objc_api_resolver_ensure_class_by_handle (GumObjcApiResolver * self)
{
  g_rec_mutex_lock (&self->monitor->mutex);

  if (self->class_by_handle == NULL)
    self->class_by_handle = gum_objc_api_resolver_create_snapshot (self);

  g_rec_mutex_unlock (&self->monitor->mutex);
}

static void
gum_objc_api_resolver_enumerate_matches (GumApiResolver * resolver,
                                         const gchar * query,
                                         GumFoundApiFunc func,
                                         gpointer user_data,
                                         GError ** error)
{
  GumObjcApiResolver * self = GUM_OBJC_API_RESOLVER (resolver);
  GMatchInfo * query_info;
  gboolean ignore_case;
  gchar method_type;
  GPatternSpec * class_spec, * method_spec;
  GHashTable * class_by_handle;
  GHashTableIter iter;
  gboolean carry_on;
  GHashTable * visited_classes;
  GumObjcClassMetadata * klass;

  if (self->monitor == NULL)
    return;

  g_regex_match (self->query_pattern, query, 0, &query_info);
  if (!g_match_info_matches (query_info))
    goto invalid_query;

  ignore_case = g_match_info_get_match_count (query_info) >= 5;

  method_type = gum_method_type_from_match_info (query_info, 1);
  class_spec = gum_pattern_spec_from_match_info (query_info, 2, ignore_case);
  method_spec = gum_pattern_spec_from_match_info (query_info, 3, ignore_case);

  g_match_info_free (query_info);

  g_rec_mutex_lock (&self->class_cache_mutex);

  gum_objc_api_resolver_ensure_class_by_handle (self);

  class_by_handle = g_hash_table_ref (self->class_by_handle);

  g_rec_mutex_unlock (&self->class_cache_mutex);

  g_hash_table_iter_init (&iter, class_by_handle);
  carry_on = TRUE;
  visited_classes = g_hash_table_new (NULL, NULL);
  while (carry_on && g_hash_table_iter_next (&iter, NULL, (gpointer *) &klass))
  {
    const gchar * class_name = klass->name;
    gchar * class_name_copy = NULL;

    if (gum_objc_class_metadata_is_disposed (klass))
    {
      g_hash_table_iter_remove (&iter);
      continue;
    }

    if (ignore_case)
    {
      class_name_copy = g_utf8_strdown (class_name, -1);
      class_name = class_name_copy;
    }

    if (g_pattern_match_string (class_spec, class_name))
    {
      carry_on = gum_objc_api_resolver_enumerate_matches_for_class (self, klass,
          method_type, method_spec, visited_classes, class_by_handle,
          ignore_case, func, user_data);
    }

    g_free (class_name_copy);
  }
  g_hash_table_unref (visited_classes);
  g_hash_table_unref (class_by_handle);

  g_pattern_spec_free (method_spec);
  g_pattern_spec_free (class_spec);

  return;

invalid_query:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "invalid query; format is: "
        "-[NS*Number foo:bar:], +[Foo foo*] or *[Bar baz]");
  }
}

static gboolean
gum_objc_api_resolver_enumerate_matches_for_class (GumObjcApiResolver * self,
                                                   GumObjcClassMetadata * klass,
                                                   gchar method_type,
                                                   GPatternSpec * method_spec,
                                                   GHashTable * visited_classes,
                                                   GHashTable * class_by_handle,
                                                   gboolean ignore_case,
                                                   GumFoundApiFunc func,
                                                   gpointer user_data)
{
  const gchar all_method_types[3] = { '+', '-', '\0' };
  const gchar one_method_type[2] = { method_type, '\0' };
  const gchar * method_types, * t;
  gboolean carry_on;
  GSList * cur;

  if (g_hash_table_lookup (visited_classes, klass) != NULL)
    return TRUE;
  g_hash_table_add (visited_classes, klass);

  method_types = (method_type == '*') ? all_method_types : one_method_type;

  for (t = method_types; *t != '\0'; t++)
  {
    const Method * method_handles;
    guint method_count, method_index;
    const gchar prefix[3] = { *t, '[', '\0' };
    const gchar suffix[2] = { ']', '\0' };

    method_handles =
        gum_objc_class_metadata_get_methods (klass, *t, &method_count);
    for (method_index = 0; method_index != method_count; method_index++)
    {
      Method method_handle = method_handles[method_index];
      const gchar * method_name, * canonical_method_name;
      gchar * method_name_copy = NULL;

      method_name = self->sel_getName (self->method_getName (method_handle));
      canonical_method_name = method_name;

      if (ignore_case)
      {
        method_name_copy = g_utf8_strdown (method_name, -1);
        method_name = method_name_copy;
      }

      if (g_pattern_match_string (method_spec, method_name))
      {
        GumApiDetails details;

        details.name = g_strconcat (prefix, klass->name, " ",
            canonical_method_name, suffix, NULL);
        details.address = GUM_ADDRESS (
            self->method_getImplementation (method_handle));
        details.size = GUM_API_SIZE_NONE;

        carry_on = func (&details, user_data);

        g_free ((gpointer) details.name);

        if (!carry_on)
        {
          g_free (method_name_copy);
          return FALSE;
        }
      }

      g_free (method_name_copy);
    }
  }

  for (cur = klass->subclasses; cur != NULL; cur = cur->next)
  {
    Class subclass_handle = cur->data;
    GumObjcClassMetadata * subclass;

    subclass = g_hash_table_lookup (class_by_handle, subclass_handle);
    if (subclass == NULL)
      continue;

    if (gum_objc_class_metadata_is_disposed (subclass))
      continue;

    carry_on = gum_objc_api_resolver_enumerate_matches_for_class (self,
        subclass, method_type, method_spec, visited_classes, class_by_handle,
        ignore_case, func, user_data);
    if (!carry_on)
      return FALSE;
  }

  return TRUE;
}

gchar *
_gum_objc_api_resolver_find_method_by_address (GumApiResolver * resolver,
                                               GumAddress address,
                                               GumModule * address_module)
{
  GumObjcApiResolver * self = GUM_OBJC_API_RESOLVER (resolver);
  gchar * result = NULL;
  GumAddress bare_address;
  GumMetalArray * classes;
  guint i;

  if (self->monitor == NULL)
    return NULL;

  bare_address = gum_strip_code_address (address);

  g_rec_mutex_lock (&self->class_cache_mutex);

  classes = gum_objc_api_resolver_get_classes_by_module (self, address_module);

  for (i = 0; i != classes->length; i++)
  {
    GumObjcClassMetadata * klass;
    const gchar * t;
    const gchar all_method_types[] = { '+', '-', '\0' };

    klass = gum_metal_array_element_at (classes, i);

    for (t = all_method_types; *t != '\0' && result == NULL; t++)
    {
      const Method * method_handles;
      guint count, i;

      method_handles = gum_objc_class_metadata_get_methods (klass, *t, &count);

      for (i = 0; i != count; i++)
      {
        Method handle = method_handles[i];
        GumAddress imp;

        imp = GUM_ADDRESS (self->method_getImplementation (handle));

        if (gum_strip_code_address (imp) == bare_address)
        {
          const gchar * name;
          const gchar prefix[2] = { *t, '\0' };

          name = self->sel_getName (self->method_getName (handle));

          if (klass->category_name == NULL)
          {
            result = g_strdup_printf ("%s[%s %s]",
                prefix, klass->name, name);
          }
          else
          {
            result = g_strdup_printf ("%s[%s(%s) %s]",
                prefix, klass->name, klass->category_name, name);
          }

          break;
        }
      }
    }
  }

  g_rec_mutex_unlock (&self->class_cache_mutex);

  return result;
}

static GumMetalArray *
gum_objc_api_resolver_get_classes_by_module (GumObjcApiResolver * self,
                                             GumModule * module)
{
  GumMetalArray * classes = NULL;
  GumAddress module_base;

  module_base = gum_module_get_range (module)->base_address;

  g_rec_mutex_lock (&self->class_cache_mutex);

  if (self->classes_by_module == NULL)
  {
    self->classes_by_module = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_objc_classes_array_free);
    gum_schedule_updates_on_module_removed (self);
  }
  else
  {
    classes = g_hash_table_lookup (self->classes_by_module,
        GSIZE_TO_POINTER (module_base));
  }

  if (classes == NULL)
  {
    classes = g_slice_new (GumMetalArray);
    g_hash_table_insert (self->classes_by_module,
        GSIZE_TO_POINTER (module_base), classes);

    gum_metal_array_init (classes, sizeof (GumObjcClassMetadata));

    gum_objc_api_resolver_collect_class_list (self, module, classes);
    gum_objc_api_resolver_collect_category_list (self, module, classes);
  }

  g_rec_mutex_unlock (&self->class_cache_mutex);

  return classes;
}

static void
gum_objc_api_resolver_collect_class_list (GumObjcApiResolver * self,
                                          GumModule * module,
                                          GumMetalArray * classes)
{
  GumObjcSectionContext ctx;
  Class * class_list;
  guint class_count, i;
  GumDarwinModule * darwin_module;

  ctx.name = "__objc_classlist";
  ctx.address = 0;
  ctx.size = 0;

  gum_module_enumerate_sections (module,
      (GumFoundSectionFunc) gum_find_objc_section, &ctx);

  if (ctx.address == 0 || ctx.size == 0)
    return;

  darwin_module =
      _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module));

  class_list = GSIZE_TO_POINTER (ctx.address);
  class_count = ctx.size / darwin_module->pointer_size;

  for (i = 0; i != class_count; i++)
  {
    Class handle = class_list[i];
    gum_objc_api_resolver_append_class (self, classes, handle);
  }
}

static void
gum_objc_api_resolver_collect_category_list (GumObjcApiResolver * self,
                                             GumModule * module,
                                             GumMetalArray * classes)
{
  GumObjcSectionContext ctx;
  GumCategoryHeader ** cat_list;
  GumDarwinModule * darwin_module;
  gsize pointer_size;
  guint cat_count, i;

  ctx.name = "__objc_catlist";
  ctx.address = 0;
  ctx.size = 0;

  gum_module_enumerate_sections (module,
    (GumFoundSectionFunc) gum_find_objc_section, &ctx);

  if (ctx.address == 0 || ctx.size == 0)
    return;

  darwin_module =
      _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module));

  pointer_size = darwin_module->pointer_size;

  cat_list = GSIZE_TO_POINTER (ctx.address);
  cat_count = ctx.size / pointer_size;

  for (i = 0; i != cat_count; i++)
  {
    GumCategoryHeader * cat_data = cat_list[i];
    GumObjcClassMetadata * klass;

    if (cat_data->target_handle == NULL)
      continue;

    klass = gum_objc_api_resolver_append_class (self, classes,
        cat_data->target_handle);

    klass->category_name = cat_data->name;
  }
}

static GumObjcClassMetadata *
gum_objc_api_resolver_append_class (GumObjcApiResolver * self,
                                    GumMetalArray * classes,
                                    Class handle)
{
  GumObjcClassMetadata * klass;

  klass = gum_metal_array_append (classes);
  klass->handle = handle;
  klass->name = self->class_getName (handle);
  klass->category_name = NULL;
  klass->class_methods = NULL;
  klass->instance_methods = NULL;
  klass->subclasses = NULL;

  klass->resolver = self;

  self->objc_lookUpClass (klass->name);

  return klass;
}

static gboolean
gum_find_objc_section (const GumSectionDetails * details,
                       GumObjcSectionContext * ctx)
{
  if (strcmp (details->name, ctx->name) == 0)
  {
    ctx->address = details->address;
    ctx->size = details->size;

    return FALSE;
  }

  return TRUE;
}

static void
gum_objc_classes_array_free (GumMetalArray * classes_array)
{
  guint i;

  for (i = 0; i != classes_array->length; i++)
  {
    GumObjcClassMetadata * klass = gum_metal_array_element_at (
        classes_array, i);
    gum_objc_class_metadata_free_data (klass);
  }

  gum_metal_array_free (classes_array);

  g_slice_free (GumMetalArray, classes_array);
}

static void
gum_schedule_updates_on_module_removed (GumObjcApiResolver * self)
{
  if (self->registry != NULL)
    return;

  g_assert (self->on_removed_handler == 0);

  self->registry = gum_module_registry_obtain ();
  gum_module_registry_lock (self->registry);

  self->on_removed_handler = g_signal_connect (self->registry, "module-removed",
      G_CALLBACK (gum_clear_classes_by_module_on_module_removed),
      self);

  gum_module_registry_unlock (self->registry);
}

static void
gum_clear_classes_by_module_on_module_removed (GumModuleRegistry * registry,
                                               GumModule * module,
                                               gpointer user_data)
{
  GumObjcApiResolver * self = user_data;

  if (self->monitor == NULL)
    return;

  g_rec_mutex_lock (&self->class_cache_mutex);

  if (self->classes_by_module != NULL)
  {
    g_hash_table_remove (self->classes_by_module,
        GSIZE_TO_POINTER (gum_module_get_range (module)->base_address));
  }

  g_rec_mutex_unlock (&self->class_cache_mutex);
}

static gchar
gum_method_type_from_match_info (GMatchInfo * match_info,
                                 gint match_num)
{
  gchar * type_str, type;

  type_str = g_match_info_fetch (match_info, match_num);
  type = type_str[0];
  g_free (type_str);

  return type;
}

static GPatternSpec *
gum_pattern_spec_from_match_info (GMatchInfo * match_info,
                                  gint match_num,
                                  gboolean ignore_case)
{
  GPatternSpec * spec;
  gchar * pattern;

  pattern = g_match_info_fetch (match_info, match_num);
  if (ignore_case)
  {
    gchar * str = g_utf8_strdown (pattern, -1);
    g_free (pattern);
    pattern = str;
  }

  spec = g_pattern_spec_new (pattern);

  g_free (pattern);

  return spec;
}

static GHashTable *
gum_objc_api_resolver_create_snapshot (GumObjcApiResolver * self)
{
  GHashTable * class_by_handle;
  gint class_count, class_index;
  Class * classes;

  class_by_handle = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_objc_class_metadata_free);

  class_count = self->objc_getClassList (NULL, 0);
  classes = g_malloc (class_count * sizeof (Class));
  self->objc_getClassList (classes, class_count);

  for (class_index = 0; class_index != class_count; class_index++)
  {
    Class handle = classes[class_index];
    GumObjcClassMetadata * klass;

    klass = g_slice_new (GumObjcClassMetadata);
    klass->handle = handle;
    klass->name = self->class_getName (handle);
    klass->class_methods = NULL;
    klass->instance_methods = NULL;
    klass->subclasses = NULL;

    klass->resolver = self;

    g_hash_table_insert (class_by_handle, handle, klass);
  }

  for (class_index = 0; class_index != class_count; class_index++)
  {
    Class handle = classes[class_index];
    Class super_handle;

    super_handle = self->class_getSuperclass (handle);
    if (super_handle != NULL)
    {
      GumObjcClassMetadata * klass;

      klass = g_hash_table_lookup (class_by_handle, super_handle);
      if (klass != NULL)
        klass->subclasses = g_slist_prepend (klass->subclasses, handle);
    }
  }

  g_free (classes);

  return class_by_handle;
}

static void
gum_objc_class_metadata_free (GumObjcClassMetadata * klass)
{
  gum_objc_class_metadata_free_data (klass);

  g_slice_free (GumObjcClassMetadata, klass);
}

static void
gum_objc_class_metadata_free_data (GumObjcClassMetadata * klass)
{
  g_slist_free (klass->subclasses);

  if (klass->instance_methods != NULL)
    gum_libc_free (klass->instance_methods);

  if (klass->class_methods != NULL)
    gum_libc_free (klass->class_methods);
}

static const Method *
gum_objc_class_metadata_get_methods (GumObjcClassMetadata * self,
                                     gchar type,
                                     guint * count)
{
  Method ** cached_methods;
  guint * cached_method_count;

  if (type == '+')
  {
    cached_methods = &self->class_methods;
    cached_method_count = &self->class_method_count;
  }
  else
  {
    cached_methods = &self->instance_methods;
    cached_method_count = &self->instance_method_count;
  }

  if (*cached_methods == NULL)
  {
    GumObjcApiResolver * resolver = self->resolver;

    *cached_methods = resolver->class_copyMethodList (
        (type == '+') ? resolver->object_getClass (self->handle) : self->handle,
        cached_method_count);
  }

  *count = *cached_method_count;

  return *cached_methods;
}

static gboolean
gum_objc_class_metadata_is_disposed (GumObjcClassMetadata * self)
{
  GumObjcApiResolver * resolver = self->resolver;

  return resolver->objc_lookUpClass (self->name) != self->handle;
}
