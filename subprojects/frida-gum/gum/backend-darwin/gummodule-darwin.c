/*
 * Copyright (C) 2010-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2022-2023 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodule-darwin.h"

#include "gumdarwin-priv.h"
#include "gum/gumdarwin.h"

#include <dlfcn.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>

#define GUM_NATIVE_MODULE_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_NATIVE_MODULE_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

typedef struct _GumEnumerateImportsContext GumEnumerateImportsContext;
typedef struct _GumEnumerateExportsContext GumEnumerateExportsContext;
typedef struct _GumEnumerateSymbolsContext GumEnumerateSymbolsContext;
typedef struct _GumEnumerateSectionsContext GumEnumerateSectionsContext;

struct _GumNativeModule
{
  GObject parent;

  gchar * name;
  gchar * path;
  GumMemoryRange range;
  GumDarwinModuleResolver * resolver;

  GMutex mutex;

  gpointer cached_handle;
  gboolean attempted_handle_creation;

  GumDarwinModule * cached_darwin_module;
  gboolean attempted_darwin_module_creation;
};

struct _GumEnumerateImportsContext
{
  GumFoundImportFunc func;
  gpointer user_data;

  GumDarwinModuleResolver * resolver;
};

struct _GumEnumerateExportsContext
{
  GumFoundExportFunc func;
  gpointer user_data;

  GumDarwinModuleResolver * resolver;
  GumDarwinModule * module;
  gboolean carry_on;
};

struct _GumEnumerateSymbolsContext
{
  GumFoundSymbolFunc func;
  gpointer user_data;

  GArray * sections;
};

struct _GumEnumerateSectionsContext
{
  GumFoundSectionFunc func;
  gpointer user_data;

  guint next_section_id;
};

static void gum_native_module_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_native_module_dispose (GObject * object);
static void gum_native_module_finalize (GObject * object);
static const gchar * gum_native_module_get_name (GumModule * module);
static const gchar * gum_native_module_get_version (GumModule * module);
static const gchar * gum_native_module_get_path (GumModule * module);
static const GumMemoryRange * gum_native_module_get_range (GumModule * module);
static void gum_native_module_ensure_initialized (GumModule * module);
static void gum_native_module_enumerate_imports (GumModule * module,
    GumFoundImportFunc func, gpointer user_data);
static gboolean gum_emit_import (const GumImportDetails * details,
    gpointer user_data);
static GumAddress gum_resolve_export (const char * module_name,
    const char * symbol_name, gpointer user_data);
static void gum_native_module_enumerate_exports (GumModule * module,
    GumFoundExportFunc func, gpointer user_data);
static gboolean gum_emit_export (const GumDarwinExportDetails * details,
    gpointer user_data);
static void gum_native_module_enumerate_symbols (GumModule * module,
    GumFoundSymbolFunc func, gpointer user_data);
static gboolean gum_emit_symbol (const GumDarwinSymbolDetails * details,
    gpointer user_data);
static gboolean gum_append_symbol_section (
    const GumDarwinSectionDetails * details, gpointer user_data);
static void gum_symbol_section_destroy (GumSymbolSection * self);
static void gum_native_module_enumerate_ranges (GumModule * module,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);
static void gum_native_module_enumerate_sections (GumModule * module,
    GumFoundSectionFunc func, gpointer user_data);
static gboolean gum_emit_section (const GumDarwinSectionDetails * details,
    gpointer user_data);
static void gum_native_module_enumerate_dependencies (GumModule * module,
    GumFoundDependencyFunc func, gpointer user_data);
static GumAddress gum_native_module_find_export_by_name (GumModule * module,
    const gchar * symbol_name);

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
  iface->get_version = gum_native_module_get_version;
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
  g_mutex_init (&self->mutex);
}

static void
gum_native_module_dispose (GObject * object)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (object);

  GUM_NATIVE_MODULE_LOCK (self);

  g_clear_object (&self->cached_darwin_module);
  g_clear_pointer (&self->cached_handle, dlclose);

  GUM_NATIVE_MODULE_UNLOCK (self);

  G_OBJECT_CLASS (gum_native_module_parent_class)->dispose (object);
}

static void
gum_native_module_finalize (GObject * object)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (object);

  g_mutex_clear (&self->mutex);

  g_free (self->path);

  G_OBJECT_CLASS (gum_native_module_parent_class)->finalize (object);
}

GumModule *
gum_module_load (const gchar * module_name,
                 GError ** error)
{
  GumModule * module;
  gpointer handle;
  static gsize initialized = FALSE;
  static const struct mach_header * (* get_dlopen_image_header) (void * handle);

  handle = dlopen (module_name, RTLD_LAZY);
  if (handle == NULL)
    goto not_found;

  if (g_once_init_enter (&initialized))
  {
    get_dlopen_image_header =
        dlsym (RTLD_DEFAULT, "_dyld_get_dlopen_image_header");

    g_once_init_leave (&initialized, TRUE);
  }

  if (get_dlopen_image_header != NULL)
  {
    module = gum_process_find_module_by_address (
        GUM_ADDRESS (get_dlopen_image_header (handle)));
  }
  else
  {
    module = gum_process_find_module_by_name (module_name);
  }
  g_assert (module != NULL);

  return module;

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND, "%s", dlerror ());
    return NULL;
  }
}

GumNativeModule *
_gum_native_module_make (const gchar * path,
                         const GumMemoryRange * range,
                         GumDarwinModuleResolver * resolver)
{
  GumNativeModule * module;
  gchar * name;

  module = g_object_new (GUM_TYPE_NATIVE_MODULE, NULL);

  module->path = g_strdup (path);
  module->range = *range;
  module->resolver = resolver;

  name = strrchr (module->path, '/');
  if (name != NULL)
    name++;
  else
    name = module->path;
  module->name = name;

  return module;
}

void
_gum_native_module_detach_resolver (GumNativeModule * self)
{
  self->resolver = NULL;
}

gpointer
_gum_native_module_get_handle (GumNativeModule * self)
{
  GUM_NATIVE_MODULE_LOCK (self);

  if (!self->attempted_handle_creation)
  {
    self->attempted_handle_creation = TRUE;

    if (self->resolver->task == mach_task_self ())
      self->cached_handle = dlopen (self->path, RTLD_LAZY);
  }

  GUM_NATIVE_MODULE_UNLOCK (self);

  return self->cached_handle;
}

GumDarwinModule *
_gum_native_module_get_darwin_module (GumNativeModule * self)
{
  GUM_NATIVE_MODULE_LOCK (self);

  if (!self->attempted_darwin_module_creation)
  {
    self->attempted_darwin_module_creation = TRUE;

    self->cached_darwin_module = gum_darwin_module_new_from_memory (self->path,
        self->resolver->task, self->range.base_address,
        GUM_DARWIN_MODULE_FLAGS_NONE, NULL);
    gum_darwin_module_ensure_image_loaded (self->cached_darwin_module, NULL);
  }

  GUM_NATIVE_MODULE_UNLOCK (self);

  return self->cached_darwin_module;
}

static const gchar *
gum_native_module_get_name (GumModule * module)
{
  return GUM_NATIVE_MODULE (module)->name;
}

static const gchar *
gum_native_module_get_version (GumModule * module)
{
  GumDarwinModule * dm =
      _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module));
  return dm->source_version;
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
  GumNativeModule * self;
  GumEnumerateImportsContext ctx;

  self = GUM_NATIVE_MODULE (module);

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.resolver = self->resolver;

  gum_darwin_module_enumerate_imports (
      _gum_native_module_get_darwin_module (self), gum_emit_import,
      gum_resolve_export, &ctx);
}

static gboolean
gum_emit_import (const GumImportDetails * details,
                 gpointer user_data)
{
  GumEnumerateImportsContext * ctx = user_data;
  gboolean carry_on;
  GumImportDetails d;
  GumDarwinModule * module = NULL;

  d.type = GUM_IMPORT_UNKNOWN;
  d.name = gum_symbol_name_from_darwin (details->name);
  d.module = details->module;
  d.address = details->address;
  d.slot = details->slot;

  if (d.module != NULL)
  {
    module = gum_darwin_module_resolver_find_module_by_name (ctx->resolver,
        d.module);
  }
  else
  {
    const gboolean inprocess = ctx->resolver->task == mach_task_self ();

    if (d.address == 0 && inprocess)
      d.address = GUM_ADDRESS (dlsym (RTLD_DEFAULT, d.name));

    if (d.address != 0)
    {
      Dl_info info;

      module = gum_darwin_module_resolver_find_module_by_address (ctx->resolver,
          d.address);
      if (module != NULL)
        d.module = module->name;
      else if (inprocess && dladdr (GSIZE_TO_POINTER (d.address), &info))
        d.module = info.dli_fname;
    }
  }

  if (module != NULL)
  {
    GumExportDetails exp;

    if (gum_darwin_module_resolver_find_export_by_mangled_name (ctx->resolver,
        module, details->name, &exp))
    {
      switch (exp.type)
      {
        case GUM_EXPORT_FUNCTION:
          d.type = GUM_IMPORT_FUNCTION;
          break;
        case GUM_EXPORT_VARIABLE:
          d.type = GUM_IMPORT_VARIABLE;
          break;
        default:
          g_assert_not_reached ();
      }

      d.address = exp.address;
    }
  }

  carry_on = ctx->func (&d, ctx->user_data);

  g_clear_object (&module);

  return carry_on;
}

static GumAddress
gum_resolve_export (const char * module_name,
                    const char * symbol_name,
                    gpointer user_data)
{
  GumEnumerateImportsContext * ctx = user_data;
  GumDarwinModule * module;

  if (module_name == NULL)
  {
    const char * name = gum_symbol_name_from_darwin (symbol_name);
    return GUM_ADDRESS (dlsym (RTLD_DEFAULT, name));
  }

  module = gum_darwin_module_resolver_find_module_by_name (ctx->resolver,
      module_name);
  if (module != NULL)
  {
    gboolean found;
    GumExportDetails exp;

    found = gum_darwin_module_resolver_find_export_by_mangled_name (
        ctx->resolver, module, symbol_name, &exp);

    g_object_unref (module);

    if (found)
      return exp.address;
  }

  return 0;
}

static void
gum_native_module_enumerate_exports (GumModule * module,
                                     GumFoundExportFunc func,
                                     gpointer user_data)
{
  GumNativeModule * self;
  GumDarwinModule * darwin_module;
  GumEnumerateExportsContext ctx;

  self = GUM_NATIVE_MODULE (module);

  darwin_module = _gum_native_module_get_darwin_module (self);

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.resolver = self->resolver;
  ctx.module = darwin_module;
  ctx.carry_on = TRUE;
  if (ctx.module != NULL)
  {
    gum_darwin_module_enumerate_exports (ctx.module, gum_emit_export, &ctx);

    if (gum_darwin_module_get_lacks_exports_for_reexports (ctx.module))
    {
      GPtrArray * reexports = ctx.module->reexports;
      guint i;

      for (i = 0; ctx.carry_on && i != reexports->len; i++)
      {
        GumDarwinModule * reexport;

        reexport = gum_darwin_module_resolver_find_module_by_name (ctx.resolver,
            g_ptr_array_index (reexports, i));
        if (reexport != NULL)
        {
          ctx.module = reexport;
          gum_darwin_module_enumerate_exports (reexport, gum_emit_export, &ctx);

          g_object_unref (reexport);
        }
      }
    }
  }
}

static gboolean
gum_emit_export (const GumDarwinExportDetails * details,
                 gpointer user_data)
{
  GumEnumerateExportsContext * ctx = user_data;
  GumExportDetails export;

  if (!gum_darwin_module_resolver_resolve_export (ctx->resolver, ctx->module,
      details, &export))
  {
    return TRUE;
  }

  ctx->carry_on = ctx->func (&export, ctx->user_data);

  return ctx->carry_on;
}

static void
gum_native_module_enumerate_symbols (GumModule * module,
                                     GumFoundSymbolFunc func,
                                     gpointer user_data)
{
  GumDarwinModule * dm;
  GumEnumerateSymbolsContext ctx;

  dm = _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module));

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.sections = g_array_new (FALSE, FALSE, sizeof (GumSymbolSection));
  g_array_set_clear_func (ctx.sections,
      (GDestroyNotify) gum_symbol_section_destroy);

  gum_darwin_module_enumerate_sections (dm, gum_append_symbol_section,
      ctx.sections);

  gum_darwin_module_enumerate_symbols (dm, gum_emit_symbol, &ctx);

  g_array_free (ctx.sections, TRUE);
}

static gboolean
gum_emit_symbol (const GumDarwinSymbolDetails * details,
                 gpointer user_data)
{
  GumEnumerateSymbolsContext * ctx = user_data;
  GumSymbolDetails symbol;

  symbol.is_global = (details->type & N_EXT) != 0;

  switch (details->type & N_TYPE)
  {
    case N_UNDF: symbol.type = GUM_SYMBOL_UNDEFINED;          break;
    case N_ABS:  symbol.type = GUM_SYMBOL_ABSOLUTE;           break;
    case N_SECT: symbol.type = GUM_SYMBOL_SECTION;            break;
    case N_PBUD: symbol.type = GUM_SYMBOL_PREBOUND_UNDEFINED; break;
    case N_INDR: symbol.type = GUM_SYMBOL_INDIRECT;           break;
    default:     symbol.type = GUM_SYMBOL_UNKNOWN;            break;
  }

  if (details->section != NO_SECT && details->section <= ctx->sections->len)
  {
    symbol.section = &g_array_index (ctx->sections, GumSymbolSection,
        details->section - 1);
  }
  else
  {
    symbol.section = NULL;
  }

  symbol.name = gum_symbol_name_from_darwin (details->name);
  symbol.address = details->address;
  symbol.size = -1;

  return ctx->func (&symbol, ctx->user_data);
}

static gboolean
gum_append_symbol_section (const GumDarwinSectionDetails * details,
                           gpointer user_data)
{
  GArray * sections = user_data;
  GumSymbolSection section;

  section.id = g_strdup_printf ("%u.%s.%s", sections->len,
      details->segment_name, details->section_name);
  section.protection = gum_page_protection_from_mach (details->protection);

  g_array_append_val (sections, section);

  return TRUE;
}

static void
gum_symbol_section_destroy (GumSymbolSection * self)
{
  g_free ((gpointer) self->id);
}

static void
gum_native_module_enumerate_ranges (GumModule * module,
                                    GumPageProtection prot,
                                    GumFoundRangeFunc func,
                                    gpointer user_data)
{
  GumDarwinModule * darwin_module;
  GumAddress slide;
  gint pid;
  gum_mach_header_t * header;
  guint8 * p;
  guint cmd_index;

  darwin_module =
      _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module));
  slide = gum_darwin_module_get_slide (darwin_module);

  pid = getpid ();

  header = GSIZE_TO_POINTER (darwin_module->base_address);
  p = (guint8 *) (header + 1);
  for (cmd_index = 0; cmd_index != header->ncmds; cmd_index++)
  {
    struct load_command * lc = (struct load_command *) p;

    if (lc->cmd == GUM_LC_SEGMENT)
    {
      gum_segment_command_t * segcmd = (gum_segment_command_t *) lc;
      gboolean is_page_zero;
      GumPageProtection cur_prot;

      is_page_zero = segcmd->vmaddr == 0 &&
          segcmd->filesize == 0 &&
          segcmd->vmsize != 0 &&
          (segcmd->initprot & VM_PROT_ALL) == VM_PROT_NONE &&
          (segcmd->maxprot & VM_PROT_ALL) == VM_PROT_NONE;
      if (is_page_zero)
      {
        p += lc->cmdsize;
        continue;
      }

      cur_prot = gum_page_protection_from_mach (segcmd->initprot);

      if ((cur_prot & prot) == prot)
      {
        GumMemoryRange range;
        GumRangeDetails details;
        GumFileMapping file;
        struct proc_regionwithpathinfo region;

        range.base_address = GUM_ADDRESS (
            GSIZE_TO_POINTER (segcmd->vmaddr) + slide);
        range.size = segcmd->vmsize;

        details.range = &range;
        details.protection = cur_prot;
        details.file = NULL;

        if (pid != 0 && _gum_darwin_fill_file_mapping (pid, range.base_address,
              &file, &region))
        {
          details.file = &file;
          _gum_darwin_clamp_range_size (&range, &file);
        }

        if (!func (&details, user_data))
          return;
      }
    }

    p += lc->cmdsize;
  }
}

static void
gum_native_module_enumerate_sections (GumModule * module,
                                      GumFoundSectionFunc func,
                                      gpointer user_data)
{
  GumDarwinModule * dm;
  GumEnumerateSectionsContext ctx;

  dm = _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module));

  ctx.func = func;
  ctx.user_data = user_data;
  ctx.next_section_id = 0;

  gum_darwin_module_enumerate_sections (dm, gum_emit_section, &ctx);
}

static gboolean
gum_emit_section (const GumDarwinSectionDetails * details,
                  gpointer user_data)
{
  GumEnumerateSectionsContext * ctx = user_data;
  gboolean carry_on;
  GumSectionDetails section;

  section.id = g_strdup_printf ("%u.%s.%s", ctx->next_section_id,
      details->segment_name, details->section_name);
  section.name = details->section_name;
  section.address = details->vm_address;
  section.size = details->size;

  carry_on = ctx->func (&section, ctx->user_data);

  g_free ((gpointer) section.id);

  ctx->next_section_id++;

  return carry_on;
}

static void
gum_native_module_enumerate_dependencies (GumModule * module,
                                          GumFoundDependencyFunc func,
                                          gpointer user_data)
{
  gum_darwin_module_enumerate_dependencies (
      _gum_native_module_get_darwin_module (GUM_NATIVE_MODULE (module)), func,
      user_data);
}

static GumAddress
gum_native_module_find_export_by_name (GumModule * module,
                                       const gchar * symbol_name)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (module);

  return gum_darwin_module_resolver_find_export_address (self->resolver,
      _gum_native_module_get_darwin_module (self), symbol_name);
}

GumAddress
gum_module_find_global_export_by_name (const gchar * symbol_name)
{
  return GUM_ADDRESS (dlsym (RTLD_DEFAULT, symbol_name));
}
