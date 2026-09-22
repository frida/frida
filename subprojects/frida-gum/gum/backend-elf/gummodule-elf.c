/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Daniel Baier <admin@remoteshell-security.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodule-elf.h"

#ifdef HAVE_ANDROID
# include "gum/gumandroid.h"
#endif

#include <dlfcn.h>

#define GUM_NATIVE_MODULE_LOCK(o) g_rec_mutex_lock (&(o)->mutex)
#define GUM_NATIVE_MODULE_UNLOCK(o) g_rec_mutex_unlock (&(o)->mutex)

typedef struct _GumEnumerateImportsContext GumEnumerateImportsContext;
typedef struct _GumEnumerateSymbolsContext GumEnumerateSymbolsContext;
typedef struct _GumEnumerateRangesContext GumEnumerateRangesContext;
typedef struct _GumEnumerateSectionsContext GumEnumerateSectionsContext;
#ifdef HAVE_ANDROID
typedef struct _GumFindExportByNameContext GumFindExportByNameContext;
#endif

struct _GumEnumerateImportsContext
{
  GumFoundImportFunc func;
  gpointer user_data;
};

struct _GumEnumerateSymbolsContext
{
  GumFoundSymbolFunc func;
  gpointer user_data;
};

struct _GumEnumerateRangesContext
{
  GumNativeModule * module;
  GumFoundRangeFunc func;
  gpointer user_data;
};

struct _GumEnumerateSectionsContext
{
  GumFoundSectionFunc func;
  gpointer user_data;
};

#ifdef HAVE_ANDROID
struct _GumFindExportByNameContext
{
  const gchar * name;
  GumAddress address;
};
#endif

static void gum_native_module_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_native_module_dispose (GObject * object);
static void gum_native_module_finalize (GObject * object);
static const gchar * gum_native_module_get_name (GumModule * module);
static const gchar * gum_native_module_get_path (GumModule * module);
static const GumMemoryRange * gum_native_module_get_range (GumModule * module);
static void gum_native_module_ensure_initialized (GumModule * module);
static void gum_native_module_enumerate_imports (GumModule * module,
    GumFoundImportFunc func, gpointer user_data);
static gboolean gum_emit_import (const GumImportDetails * details,
    gpointer user_data);
static void gum_native_module_enumerate_exports (GumModule * module,
    GumFoundExportFunc func, gpointer user_data);
static void gum_native_module_enumerate_symbols (GumModule * module,
    GumFoundSymbolFunc func, gpointer user_data);
static gboolean gum_emit_symbol (const GumElfSymbolDetails * details,
    gpointer user_data);
static void gum_native_module_enumerate_ranges (GumModule * module,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);
static gboolean gum_emit_range_if_module_name_matches (
    const GumRangeDetails * details, gpointer user_data);
static void gum_native_module_enumerate_sections (GumModule * module,
    GumFoundSectionFunc func, gpointer user_data);
static gboolean gum_emit_section (const GumElfSectionDetails * details,
    gpointer user_data);
static void gum_native_module_enumerate_dependencies (GumModule * module,
    GumFoundDependencyFunc func, gpointer user_data);
static GumAddress gum_native_module_find_export_by_name (GumModule * module,
    const gchar * symbol_name);
#ifdef HAVE_ANDROID
static gboolean gum_try_resolve_export_by_name (
    const GumExportDetails * details, gpointer user_data);
#endif

static GumAddress gum_dlsym (gpointer module_handle, const gchar * symbol_name);

#ifdef HAVE_ANDROID
static gboolean gum_path_indicates_apk_library (const gchar * path);
#endif

G_DEFINE_TYPE_EXTENDED (GumNativeModule,
                        gum_native_module,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_MODULE,
                            gum_native_module_iface_init))

static void
gum_native_module_class_init (GumNativeModuleClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_native_module_dispose;
  object_class->finalize = gum_native_module_finalize;
}

static void
gum_native_module_iface_init (gpointer g_iface,
                              gpointer iface_data)
{
  GumModuleInterface * iface = g_iface;

  iface->get_name = gum_native_module_get_name;
  iface->get_path = gum_native_module_get_path;
  iface->get_range = gum_native_module_get_range;
  iface->ensure_initialized = gum_native_module_ensure_initialized;
  iface->enumerate_imports = gum_native_module_enumerate_imports;
  iface->enumerate_exports = gum_native_module_enumerate_exports;
  iface->enumerate_symbols = gum_native_module_enumerate_symbols;
  iface->enumerate_ranges = gum_native_module_enumerate_ranges;
  iface->enumerate_sections = gum_native_module_enumerate_sections;
  iface->enumerate_dependencies = gum_native_module_enumerate_dependencies;
  iface->find_export_by_name = gum_native_module_find_export_by_name;
}

static void
gum_native_module_init (GumNativeModule * self)
{
  g_rec_mutex_init (&self->mutex);
}

static void
gum_native_module_dispose (GObject * object)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (object);

  GUM_NATIVE_MODULE_LOCK (self);

  g_clear_object (&self->cached_elf_module);

  if (self->destroy_handle != NULL)
    g_clear_pointer (&self->cached_handle, self->destroy_handle);
  else
    self->cached_handle = NULL;

  GUM_NATIVE_MODULE_UNLOCK (self);

  G_OBJECT_CLASS (gum_native_module_parent_class)->dispose (object);
}

static void
gum_native_module_finalize (GObject * object)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (object);

  g_rec_mutex_clear (&self->mutex);

  g_free (self->path);

  G_OBJECT_CLASS (gum_native_module_parent_class)->finalize (object);
}

GumNativeModule *
_gum_native_module_make (const gchar * path,
                         const GumMemoryRange * range,
                         GumCreateModuleHandleFunc create_handle,
                         gpointer create_handle_data,
                         GDestroyNotify create_handle_data_destroy,
                         GDestroyNotify destroy_handle)
{
  GumNativeModule * module;
  gchar * name;

  module = g_object_new (GUM_TYPE_NATIVE_MODULE, NULL);

  module->path = g_strdup (path);
  module->range = *range;
  module->create_handle = create_handle;
  module->create_handle_data = create_handle_data;
  module->create_handle_data_destroy = create_handle_data_destroy;
  module->destroy_handle = destroy_handle;

  name = strrchr (module->path, '/');
  if (name != NULL)
    name++;
  else
    name = module->path;
  module->name = name;

  return module;
}

GumNativeModule *
_gum_native_module_make_handleless (const gchar * path,
                                    const GumMemoryRange * range)
{
  return _gum_native_module_make (path, range, NULL, NULL, NULL, NULL);
}

gpointer
_gum_native_module_get_handle (GumNativeModule * self)
{
  GUM_NATIVE_MODULE_LOCK (self);

  if (!self->attempted_handle_creation)
  {
    self->attempted_handle_creation = TRUE;

    if (self->create_handle != NULL)
    {
      self->cached_handle = self->create_handle (self,
          self->create_handle_data);
    }
  }

  GUM_NATIVE_MODULE_UNLOCK (self);

  return self->cached_handle;
}

GumElfModule *
_gum_native_module_get_elf_module (GumNativeModule * self)
{
  GUM_NATIVE_MODULE_LOCK (self);

  if (!self->attempted_elf_module_creation)
  {
    self->attempted_elf_module_creation = TRUE;

    self->cached_elf_module = gum_elf_module_new_from_memory (self->path,
        self->range.base_address, NULL);
  }

  GUM_NATIVE_MODULE_UNLOCK (self);

  return self->cached_elf_module;
}

static const gchar *
gum_native_module_get_name (GumModule * module)
{
  return GUM_NATIVE_MODULE (module)->name;
}

static const gchar *
gum_native_module_get_path (GumModule * module)
{
  return GUM_NATIVE_MODULE (module)->path;
}

static const GumMemoryRange *
gum_native_module_get_range (GumModule * module)
{
  return &GUM_NATIVE_MODULE (module)->range;
}

static void
gum_native_module_ensure_initialized (GumModule * module)
{
  _gum_native_module_get_handle (GUM_NATIVE_MODULE (module));
}

static void
gum_native_module_enumerate_imports (GumModule * module,
                                     GumFoundImportFunc func,
                                     gpointer user_data)
{
  GumElfModule * elf_module;
  GumEnumerateImportsContext ctx;

  elf_module = _gum_native_module_get_elf_module (GUM_NATIVE_MODULE (module));
  if (elf_module == NULL)
    return;

  ctx.func = func;
  ctx.user_data = user_data;

  gum_elf_module_enumerate_imports (elf_module, gum_emit_import, &ctx);
}

static gboolean
gum_emit_import (const GumImportDetails * details,
                 gpointer user_data)
{
  GumEnumerateImportsContext * ctx = user_data;
  gboolean carry_on;
  GumImportDetails d;

  d.type = details->type;
  d.name = details->name;
  d.slot = details->slot;

  d.address = (d.slot != 0)
      ? GUM_ADDRESS (*((gpointer *) GSIZE_TO_POINTER (d.slot)))
      : 0;
  d.module = (d.address != 0)
      ? _gum_native_module_find_path_by_address (d.address)
      : NULL;

  carry_on = ctx->func (&d, ctx->user_data);

  g_free ((gpointer) d.module);

  return carry_on;
}

static void
gum_native_module_enumerate_exports (GumModule * module,
                                     GumFoundExportFunc func,
                                     gpointer user_data)
{
  GumNativeModule * self;
  GumElfModule * elf_module;

  self = GUM_NATIVE_MODULE (module);

#ifdef HAVE_ANDROID
  if (gum_android_is_linker_module_name (self->path))
  {
    const gchar ** magic_exports;
    guint i;

    magic_exports = gum_android_get_magic_linker_export_names ();

    for (i = 0; magic_exports[i] != NULL; i++)
    {
      const gchar * name = magic_exports[i];
      GumExportDetails d;

      d.type = GUM_EXPORT_FUNCTION;
      d.name = name;
      d.address = gum_module_find_export_by_name (module, name);
      g_assert (d.address != 0);
      d.size = -1;

      if (!func (&d, user_data))
        return;
    }
  }
#endif

  elf_module = _gum_native_module_get_elf_module (self);
  if (elf_module == NULL)
    return;

  gum_elf_module_enumerate_exports (elf_module, func, user_data);
}

static void
gum_native_module_enumerate_symbols (GumModule * module,
                                     GumFoundSymbolFunc func,
                                     gpointer user_data)
{
  GumElfModule * elf_module;
  GumEnumerateSymbolsContext ctx;

  elf_module = _gum_native_module_get_elf_module (GUM_NATIVE_MODULE (module));
  if (elf_module == NULL)
    return;

  ctx.func = func;
  ctx.user_data = user_data;

  gum_elf_module_enumerate_symbols (elf_module, gum_emit_symbol, &ctx);
}

static gboolean
gum_emit_symbol (const GumElfSymbolDetails * details,
                 gpointer user_data)
{
  GumEnumerateSymbolsContext * ctx = user_data;
  GumSymbolDetails symbol;
  const GumElfSectionDetails * section;
  GumSymbolSection symsect;

  symbol.is_global = details->bind == GUM_ELF_BIND_GLOBAL ||
      details->bind == GUM_ELF_BIND_WEAK;

  switch (details->type)
  {
    case GUM_ELF_SYMBOL_OBJECT:  symbol.type = GUM_SYMBOL_OBJECT;   break;
    case GUM_ELF_SYMBOL_FUNC:    symbol.type = GUM_SYMBOL_FUNCTION; break;
    case GUM_ELF_SYMBOL_SECTION: symbol.type = GUM_SYMBOL_SECTION;  break;
    case GUM_ELF_SYMBOL_FILE:    symbol.type = GUM_SYMBOL_FILE;     break;
    case GUM_ELF_SYMBOL_COMMON:  symbol.type = GUM_SYMBOL_COMMON;   break;
    case GUM_ELF_SYMBOL_TLS:     symbol.type = GUM_SYMBOL_TLS;      break;
    default:                     symbol.type = GUM_SYMBOL_UNKNOWN;  break;
  }

  section = details->section;
  if (section != NULL)
  {
    symsect.id = section->id;
    symsect.protection = section->protection;
    symbol.section = &symsect;
  }
  else
  {
    symbol.section = NULL;
  }

  symbol.name = details->name;
  symbol.address = details->address;
  symbol.size = details->size;

  return ctx->func (&symbol, ctx->user_data);
}

static void
gum_native_module_enumerate_ranges (GumModule * module,
                                    GumPageProtection prot,
                                    GumFoundRangeFunc func,
                                    gpointer user_data)
{
  GumEnumerateRangesContext ctx;

  ctx.module = GUM_NATIVE_MODULE (module);
  ctx.func = func;
  ctx.user_data = user_data;

  gum_process_enumerate_ranges (prot, gum_emit_range_if_module_name_matches,
      &ctx);
}

static gboolean
gum_emit_range_if_module_name_matches (const GumRangeDetails * details,
                                       gpointer user_data)
{
  GumEnumerateRangesContext * ctx = user_data;
  const gchar * range_path, * module_path;

  if (details->file == NULL)
    return TRUE;

  range_path = details->file->path;
  module_path = ctx->module->path;

  if (strcmp (range_path, module_path) == 0)
    return ctx->func (details, ctx->user_data);

#ifdef HAVE_ANDROID
  /*
   * Android APK-embedded library handling:
   * When android:extractNativeLibs="false", libraries are loaded directly
   * from the APK. /proc/maps shows paths like:
   *   /data/app/.../base.apk!/lib/arm64-v8a/libfoo.so
   *
   * The module's stored path (from soinfo::get_realpath()) may differ.
   * Use address-based matching as a fallback.
   */
  if (gum_path_indicates_apk_library (range_path) ||
      gum_path_indicates_apk_library (module_path))
  {
    if (GUM_MEMORY_RANGE_INCLUDES (&ctx->module->range,
          details->range->base_address))
    {
      return ctx->func (details, ctx->user_data);
    }
  }
#endif

  return TRUE;
}

static void
gum_native_module_enumerate_sections (GumModule * module,
                                      GumFoundSectionFunc func,
                                      gpointer user_data)
{
  GumElfModule * elf_module;
  GumEnumerateSectionsContext ctx;

  elf_module = _gum_native_module_get_elf_module (GUM_NATIVE_MODULE (module));
  if (elf_module == NULL)
    return;

  ctx.func = func;
  ctx.user_data = user_data;

  gum_elf_module_enumerate_sections (elf_module, gum_emit_section, &ctx);
}

static gboolean
gum_emit_section (const GumElfSectionDetails * details,
                  gpointer user_data)
{
  GumEnumerateSectionsContext * ctx = user_data;
  GumSectionDetails section;

  section.id = details->id;
  section.name = details->name;
  section.address = details->address;
  section.size = details->size;

  return ctx->func (&section, ctx->user_data);
}

static void
gum_native_module_enumerate_dependencies (GumModule * module,
                                          GumFoundDependencyFunc func,
                                          gpointer user_data)
{
  GumElfModule * elf_module;

  elf_module = _gum_native_module_get_elf_module (GUM_NATIVE_MODULE (module));
  if (elf_module == NULL)
    return;

  gum_elf_module_enumerate_dependencies (elf_module, func, user_data);
}

static GumAddress
gum_native_module_find_export_by_name (GumModule * module,
                                       const gchar * symbol_name)
{
  GumNativeModule * self;
  gpointer handle;

  self = GUM_NATIVE_MODULE (module);

#ifdef HAVE_ANDROID
  {
    GumAddress addr;

    if (gum_android_get_linker_flavor () == GUM_ANDROID_LINKER_NATIVE &&
        gum_android_try_resolve_magic_export (self->path, symbol_name, &addr))
      return addr;

    if (module == gum_android_get_linker_module ())
    {
      GumFindExportByNameContext ctx;

      ctx.name = symbol_name;
      ctx.address = 0;

      gum_module_enumerate_exports (module, gum_try_resolve_export_by_name,
          &ctx);

      return ctx.address;
    }
  }
#endif

  handle = _gum_native_module_get_handle (self);
  if (handle == NULL)
    return 0;

  return gum_dlsym (handle, symbol_name);
}

#ifdef HAVE_ANDROID

static gboolean
gum_try_resolve_export_by_name (const GumExportDetails * details,
                                gpointer user_data)
{
  GumFindExportByNameContext * ctx = user_data;

  if (strcmp (details->name, ctx->name) == 0)
  {
    ctx->address = details->address;
    return FALSE;
  }

  return TRUE;
}

#endif

GumAddress
gum_module_find_global_export_by_name (const gchar * symbol_name)
{
  return gum_dlsym (RTLD_DEFAULT, symbol_name);
}

static GumAddress
gum_dlsym (gpointer module_handle,
           const gchar * symbol_name)
{
#ifdef HAVE_ANDROID
  GumGenericDlsymImpl dlsym_impl = dlsym;

  if (gum_android_get_linker_flavor () == GUM_ANDROID_LINKER_NATIVE)
    gum_android_find_unrestricted_dlsym (&dlsym_impl);

  return GUM_ADDRESS (dlsym_impl (module_handle, symbol_name));
#else
  return GUM_ADDRESS (dlsym (module_handle, symbol_name));
#endif
}

#ifdef HAVE_ANDROID

static gboolean
gum_path_indicates_apk_library (const gchar * path)
{
  return strstr (path, ".apk!") != NULL;
}

#endif
