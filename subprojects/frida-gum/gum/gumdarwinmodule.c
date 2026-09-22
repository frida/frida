/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2022 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2023 Fabian Freyer <fabian.freyer@physik.tu-berlin.de>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumdarwinmodule.h"

#include "gumdarwinmodule-priv.h"
#ifdef HAVE_DARWIN
# include "gum/gumdarwin.h"
#endif
#include "gumleb.h"
#include "gumkernel.h"

#define GUM_MAX_MACHO_METADATA_SIZE   (64 * 1024)

#define GUM_DARWIN_MODULE_HAS_HEADER_ONLY(self) \
    ((self->flags & GUM_DARWIN_MODULE_FLAGS_HEADER_ONLY) != 0)

typedef struct _GumResolveSymbolContext GumResolveSymbolContext;

typedef struct _GumEmitImportContext GumEmitImportContext;
typedef struct _GumEmitExportFromSymbolContext GumEmitExportFromSymbolContext;
typedef struct _GumQueryTlvParamsContext GumQueryTlvParamsContext;
typedef struct _GumEmitInitPointersContext GumEmitInitPointersContext;
typedef struct _GumEmitInitOffsetsContext GumEmitInitOffsetsContext;
typedef struct _GumEmitTermPointersContext GumEmitTermPointersContext;

typedef struct _GumExportsTrieForeachContext GumExportsTrieForeachContext;
typedef struct _GumPageAllocation GumPageAllocation;

enum
{
  PROP_0,
  PROP_NAME,
  PROP_UUID,
  PROP_SOURCE_VERSION,
  PROP_TASK,
  PROP_CPU_TYPE,
  PROP_PTRAUTH_SUPPORT,
  PROP_BASE_ADDRESS,
  PROP_SOURCE_PATH,
  PROP_SOURCE_BLOB,
  PROP_FLAGS,
};

struct _GumResolveSymbolContext
{
  const gchar * name;
  GumAddress result;
};

struct _GumEmitImportContext
{
  GumFoundImportFunc func;
  GumResolveExportFunc resolver;
  gpointer user_data;

  GumDarwinModule * module;
  GArray * threaded_binds;
  const guint8 * source_start;
  const guint8 * source_end;
  GMappedFile * source_file;
  gboolean carry_on;
};

struct _GumEmitExportFromSymbolContext
{
  GumFoundDarwinExportFunc func;
  gpointer user_data;
};

struct _GumQueryTlvParamsContext
{
  GumMachHeader32 * header;
  GumDarwinTlvParameters * params;
};

struct _GumEmitInitPointersContext
{
  GumFoundDarwinInitPointersFunc func;
  gpointer user_data;
  gsize pointer_size;
};

struct _GumEmitInitOffsetsContext
{
  GumFoundDarwinInitOffsetsFunc func;
  gpointer user_data;
};

struct _GumEmitTermPointersContext
{
  GumFoundDarwinTermPointersFunc func;
  gpointer user_data;
  gsize pointer_size;
};

struct _GumExportsTrieForeachContext
{
  GumFoundDarwinExportFunc func;
  gpointer user_data;

  GString * prefix;
  const guint8 * exports;
  const guint8 * exports_end;
};

struct _GumPageAllocation
{
  gpointer data;
  gsize size;
};

static void gum_darwin_module_constructed (GObject * object);
static void gum_darwin_module_finalize (GObject * object);
static void gum_darwin_module_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_darwin_module_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static gboolean gum_store_address_if_name_matches (
    const GumDarwinSymbolDetails * details, gpointer user_data);
static gboolean gum_emit_import (const GumDarwinBindDetails * details,
    gpointer user_data);
static gboolean gum_emit_export_from_symbol (
    const GumDarwinSymbolDetails * details, gpointer user_data);
static gboolean gum_collect_tlv_params (const GumDarwinSectionDetails * section,
    gpointer user_data);
static gboolean gum_emit_section_init_pointers (
    const GumDarwinSectionDetails * details, gpointer user_data);
static gboolean gum_emit_section_init_offsets (
    const GumDarwinSectionDetails * details, gpointer user_data);
static gboolean gum_emit_section_term_pointers (
    const GumDarwinSectionDetails * details, gpointer user_data);
static gboolean gum_darwin_module_load_image_from_filesystem (
    GumDarwinModule * self, const gchar * path, GError ** error);
static gboolean gum_darwin_module_load_image_header_from_filesystem (
    GumDarwinModule * self, const gchar * path, GError ** error);
static gboolean gum_darwin_module_load_image_from_blob (GumDarwinModule * self,
    GBytes * blob, GError ** error);
static gboolean gum_darwin_module_load_image_from_memory (
    GumDarwinModule * self, GError ** error);
static GBytes * gum_page_allocation_new_bytes (gpointer data,
    gsize content_size, gsize allocation_size);
static void gum_page_allocation_free (GumPageAllocation * self);
static gboolean gum_darwin_module_can_load (GumDarwinModule * self,
    GumDarwinCpuType cpu_type, GumDarwinCpuSubtype cpu_subtype);
static gboolean gum_darwin_module_take_image (GumDarwinModule * self,
    GumDarwinModuleImage * image, GError ** error);
static gboolean gum_darwin_module_get_header_offset_size (
    GumDarwinModule * self, gpointer data, gsize data_size, gsize * out_offset,
    gsize * out_size, GError ** error);
static void gum_darwin_module_read_and_assign (GumDarwinModule * self,
    GumAddress address, gsize size, const guint8 ** start, const guint8 ** end,
    gpointer * malloc_data);
static gboolean gum_find_linkedit (const guint8 * module, gsize module_size,
    GumAddress * linkedit);
static gboolean gum_add_text_range_if_text_section (
    const GumDarwinSectionDetails * details, gpointer user_data);
static gboolean gum_section_flags_indicate_text_section (guint32 flags);

static gboolean gum_exports_trie_find (const guint8 * exports,
    const guint8 * exports_end, const gchar * name,
    GumDarwinExportDetails * details);
static gboolean gum_exports_trie_foreach (const guint8 * exports,
    const guint8 * exports_end, GumFoundDarwinExportFunc func,
    gpointer user_data);
static gboolean gum_exports_trie_traverse (const guint8 * p,
    GumExportsTrieForeachContext * ctx);

static void gum_darwin_export_details_init_from_node (
    GumDarwinExportDetails * details, const gchar * name, const guint8 * node,
    const guint8 * exports_end);

static void gum_darwin_module_enumerate_chained_binds (GumDarwinModule * self,
    GumFoundDarwinBindFunc func, gpointer user_data);
static gboolean gum_emit_chained_imports (
    const GumDarwinChainedFixupsDetails * details, GumEmitImportContext * ctx);

static GumCpuType gum_cpu_type_from_darwin (GumDarwinCpuType cpu_type);
static GumPtrauthSupport gum_ptrauth_support_from_darwin (
    GumDarwinCpuType cpu_type, GumDarwinCpuSubtype cpu_subtype);
static guint gum_pointer_size_from_cpu_type (GumDarwinCpuType cpu_type);

G_DEFINE_TYPE (GumDarwinModule, gum_darwin_module, G_TYPE_OBJECT)

G_DEFINE_BOXED_TYPE (GumDarwinModuleImage, gum_darwin_module_image,
                     gum_darwin_module_image_dup, gum_darwin_module_image_free)

static void
gum_darwin_module_class_init (GumDarwinModuleClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gum_darwin_module_constructed;
  object_class->finalize = gum_darwin_module_finalize;
  object_class->get_property = gum_darwin_module_get_property;
  object_class->set_property = gum_darwin_module_set_property;

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Name", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_UUID,
      g_param_spec_string ("uuid", "UUID", "UUID", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_VERSION,
      g_param_spec_string ("source-version", "Source Version",
      "Version of the sources used to build the binary", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TASK,
      g_param_spec_uint ("task", "Task", "Mach task", 0, G_MAXUINT,
      GUM_DARWIN_PORT_NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CPU_TYPE,
      g_param_spec_uint ("cpu-type", "CpuType", "CPU type", 0, G_MAXUINT,
      GUM_CPU_INVALID, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PTRAUTH_SUPPORT,
      g_param_spec_uint ("ptrauth-support", "PtrauthSupport",
      "Pointer authentication support", 0, G_MAXUINT, GUM_PTRAUTH_INVALID,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BASE_ADDRESS,
      g_param_spec_uint64 ("base-address", "BaseAddress", "Base address", 0,
      G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_PATH,
      g_param_spec_string ("source-path", "SourcePath", "Source path", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_BLOB,
      g_param_spec_boxed ("source-blob", "SourceBlob", "Source blob",
      G_TYPE_BYTES,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Optional flags",
      GUM_TYPE_DARWIN_MODULE_FLAGS, GUM_DARWIN_MODULE_FLAGS_NONE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gum_darwin_module_init (GumDarwinModule * self)
{
  self->segments = g_array_new (FALSE, FALSE, sizeof (GumDarwinSegment));
  self->text_ranges = g_array_new (FALSE, FALSE, sizeof (GumMemoryRange));
  self->dependencies =
      g_array_new (FALSE, FALSE, sizeof (GumDependencyDetails));
  self->reexports = g_ptr_array_sized_new (5);
}

static void
gum_darwin_module_constructed (GObject * object)
{
  GumDarwinModule * self = GUM_DARWIN_MODULE (object);

#ifdef HAVE_DARWIN
  if (self->task != GUM_DARWIN_PORT_NULL)
  {
    self->is_local = self->task == mach_task_self ();
    self->is_kernel = self->task == gum_kernel_get_task ();
  }
#else
  self->is_local = self->source_path == NULL && self->source_blob == NULL;
#endif
}

static void
gum_darwin_module_finalize (GObject * object)
{
  GumDarwinModule * self = GUM_DARWIN_MODULE (object);

  g_array_unref (self->dependencies);
  g_ptr_array_unref (self->reexports);

  g_free (self->rebases_malloc_data);
  g_free (self->binds_malloc_data);
  g_free (self->lazy_binds_malloc_data);
  g_free (self->exports_malloc_data);

  g_array_unref (self->segments);
  g_array_unref (self->text_ranges);

  if (self->image != NULL)
    gum_darwin_module_image_free (self->image);

  g_free (self->source_path);
  g_bytes_unref (self->source_blob);

  g_free (self->name);
  g_free (self->uuid);
  g_free (self->source_version);

  G_OBJECT_CLASS (gum_darwin_module_parent_class)->finalize (object);
}

static void
gum_darwin_module_get_property (GObject * object,
                                guint property_id,
                                GValue * value,
                                GParamSpec * pspec)
{
  GumDarwinModule * self = GUM_DARWIN_MODULE (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_UUID:
      if (self->uuid == NULL)
        gum_darwin_module_ensure_image_loaded (self, NULL);
      g_value_set_string (value, self->uuid);
      break;
    case PROP_SOURCE_VERSION:
      if (self->source_version == NULL)
        gum_darwin_module_ensure_image_loaded (self, NULL);
      g_value_set_string (value, self->source_version);
      break;
    case PROP_TASK:
      g_value_set_uint (value, self->task);
      break;
    case PROP_CPU_TYPE:
      g_value_set_uint (value, self->cpu_type);
      break;
    case PROP_PTRAUTH_SUPPORT:
      g_value_set_uint (value, self->ptrauth_support);
      break;
    case PROP_BASE_ADDRESS:
      g_value_set_uint64 (value, self->base_address);
      break;
    case PROP_SOURCE_PATH:
      g_value_set_string (value, self->source_path);
      break;
    case PROP_SOURCE_BLOB:
      g_value_set_boxed (value, self->source_blob);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, self->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_darwin_module_set_property (GObject * object,
                                guint property_id,
                                const GValue * value,
                                GParamSpec * pspec)
{
  GumDarwinModule * self = GUM_DARWIN_MODULE (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;
    case PROP_TASK:
      self->task = g_value_get_uint (value);
      break;
    case PROP_CPU_TYPE:
      self->cpu_type = g_value_get_uint (value);
      break;
    case PROP_PTRAUTH_SUPPORT:
      self->ptrauth_support = g_value_get_uint (value);
      break;
    case PROP_BASE_ADDRESS:
      self->base_address = g_value_get_uint64 (value);
      break;
    case PROP_SOURCE_PATH:
      g_free (self->source_path);
      self->source_path = g_value_dup_string (value);
      break;
    case PROP_SOURCE_BLOB:
      g_clear_pointer (&self->source_blob, g_bytes_unref);
      self->source_blob = g_value_dup_boxed (value);
      break;
    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumDarwinModule *
gum_darwin_module_new_from_file (const gchar * path,
                                 GumCpuType cpu_type,
                                 GumPtrauthSupport ptrauth_support,
                                 GumDarwinModuleFlags flags,
                                 GError ** error)
{
  GumDarwinModule * module;

  module = g_object_new (GUM_TYPE_DARWIN_MODULE,
      "cpu-type", cpu_type,
      "ptrauth-support", ptrauth_support,
      "source-path", path,
      "flags", flags,
      NULL);
  if (!gum_darwin_module_load (module, error))
  {
    g_object_unref (module);
    module = NULL;
  }

  return module;
}

GumDarwinModule *
gum_darwin_module_new_from_blob (GBytes * blob,
                                 GumCpuType cpu_type,
                                 GumPtrauthSupport ptrauth_support,
                                 GumDarwinModuleFlags flags,
                                 GError ** error)
{
  GumDarwinModule * module;

  module = g_object_new (GUM_TYPE_DARWIN_MODULE,
      "cpu-type", cpu_type,
      "ptrauth-support", ptrauth_support,
      "source-blob", blob,
      "flags", flags,
      NULL);
  if (!gum_darwin_module_load (module, error))
  {
    g_object_unref (module);
    module = NULL;
  }

  return module;
}

GumDarwinModule *
gum_darwin_module_new_from_memory (const gchar * name,
                                   GumDarwinPort task,
                                   GumAddress base_address,
                                   GumDarwinModuleFlags flags,
                                   GError ** error)
{
  GumDarwinModule * module;

  module = g_object_new (GUM_TYPE_DARWIN_MODULE,
      "name", name,
      "task", task,
      "base-address", base_address,
      "flags", flags,
      NULL);
  if (!gum_darwin_module_load (module, error))
  {
    g_object_unref (module);
    module = NULL;
  }

  return module;
}

gboolean
gum_darwin_module_load (GumDarwinModule * self,
                        GError ** error)
{
  if (self->image != NULL)
    return TRUE;

  if (self->source_path != NULL)
  {
    if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self))
    {
      if (!gum_darwin_module_load_image_header_from_filesystem (self,
          self->source_path, error))
      {
        return FALSE;
      }
    }
    else
    {
      if (!gum_darwin_module_load_image_from_filesystem (self,
          self->source_path, error))
      {
        return FALSE;
      }
    }
  }
  else if (self->source_blob != NULL)
  {
    if (!gum_darwin_module_load_image_from_blob (self, self->source_blob,
        error))
    {
      return FALSE;
    }
  }

  if (self->name == NULL)
    return gum_darwin_module_ensure_image_loaded (self, error);

  return TRUE;
}

static guint8 *
gum_darwin_module_read_from_task (GumDarwinModule * self,
                                  GumAddress address,
                                  gsize len,
                                  gsize * n_bytes_read)
{
#ifdef HAVE_DARWIN
  return self->is_kernel
      ? gum_kernel_read (address, len, n_bytes_read)
      : gum_darwin_read (self->task, address, len, n_bytes_read);
#else
  return NULL;
#endif
}

gboolean
gum_darwin_module_resolve_export (GumDarwinModule * self,
                                  const gchar * name,
                                  GumDarwinExportDetails * details)
{
  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return FALSE;

  if (self->exports != NULL)
  {
    return gum_exports_trie_find (self->exports, self->exports_end, name,
        details);
  }
  else if (self->filetype == GUM_DARWIN_MODULE_FILETYPE_DYLINKER)
  {
    GumAddress address;

    address = gum_darwin_module_resolve_symbol_address (self, name);
    if (address == 0)
      return FALSE;

    details->name = name;
    details->flags = GUM_DARWIN_EXPORT_ABSOLUTE;
    details->offset = address;

    return TRUE;
  }

  return FALSE;
}

GumAddress
gum_darwin_module_resolve_symbol_address (GumDarwinModule * self,
                                          const gchar * name)
{
  GumResolveSymbolContext ctx;

  ctx.name = name;
  ctx.result = 0;

  gum_darwin_module_enumerate_symbols (self, gum_store_address_if_name_matches,
      &ctx);

  return ctx.result;
}

static gboolean
gum_store_address_if_name_matches (const GumDarwinSymbolDetails * details,
                                   gpointer user_data)
{
  GumResolveSymbolContext * ctx = user_data;
  gboolean carry_on = TRUE;

  if (strcmp (details->name, ctx->name) == 0)
  {
    ctx->result = details->address;
    carry_on = FALSE;
  }

  return carry_on;
}

gboolean
gum_darwin_module_get_lacks_exports_for_reexports (GumDarwinModule * self)
{
  guint32 flags;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return FALSE;

  /*
   * FIXME: There must be a better way to detect this behavioral change
   *        introduced in macOS 10.11 and iOS 9.0, but this will have to
   *        do for now.
   */
  flags = ((GumMachHeader32 *) self->image->data)->flags;

  return (flags & GUM_MH_PREBOUND) == 0;
}

/**
 * gum_darwin_module_enumerate_imports:
 * @self: module
 * @func: (scope call): function called with each import
 * @resolver: (scope call) (nullable): function to resolve exports
 * @user_data: data to pass to @func and @resolver
 *
 * Enumerates imports of the module.
 */
void
gum_darwin_module_enumerate_imports (GumDarwinModule * self,
                                     GumFoundImportFunc func,
                                     GumResolveExportFunc resolver,
                                     gpointer user_data)
{
  GumEmitImportContext ctx;

  ctx.func = func;
  ctx.resolver = resolver;
  ctx.user_data = user_data;

  ctx.module = self;
  ctx.threaded_binds = NULL;
  ctx.source_start = NULL;
  ctx.source_end = NULL;
  ctx.source_file = NULL;
  ctx.carry_on = TRUE;

  gum_darwin_module_enumerate_binds (self, gum_emit_import, &ctx);
  if (ctx.carry_on)
    gum_darwin_module_enumerate_lazy_binds (self, gum_emit_import, &ctx);
  if (ctx.carry_on)
    gum_darwin_module_enumerate_chained_binds (self, gum_emit_import, &ctx);

  g_clear_pointer (&ctx.source_file, g_mapped_file_unref);
  g_clear_pointer (&ctx.threaded_binds, g_array_unref);
}

static gboolean
gum_emit_import (const GumDarwinBindDetails * details,
                 gpointer user_data)
{
  GumEmitImportContext * ctx = user_data;
  GumDarwinModule * self = ctx->module;
  const GumDarwinSegment * segment = details->segment;
  GumAddress vm_base;

  vm_base = segment->vm_address + gum_darwin_module_get_slide (self);

  switch (details->type)
  {
    case GUM_DARWIN_BIND_POINTER:
    {
      GumImportDetails d;

      d.type = GUM_IMPORT_UNKNOWN;
      d.name = details->symbol_name;
      switch (details->library_ordinal)
      {
        case GUM_BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
        case GUM_BIND_SPECIAL_DYLIB_SELF:
          return TRUE;
        case GUM_BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
        {
          d.module = NULL;
          break;
        }
        default:
          d.module = gum_darwin_module_get_dependency_by_ordinal (self,
              details->library_ordinal);
          break;
      }
      d.address = 0;
      d.slot = vm_base + details->offset;

      if (ctx->threaded_binds != NULL)
        g_array_append_val (ctx->threaded_binds, d);
      else
        ctx->carry_on = ctx->func (&d, ctx->user_data);

      break;
    }
    case GUM_DARWIN_BIND_THREADED_TABLE:
    {
      g_clear_pointer (&ctx->threaded_binds, g_array_unref);
      ctx->threaded_binds = g_array_sized_new (FALSE, FALSE,
          sizeof (GumImportDetails), details->threaded_table_size);

      break;
    }
    case GUM_DARWIN_BIND_THREADED_ITEMS:
    {
      GArray * threaded_binds = ctx->threaded_binds;
      guint64 cursor;
      GumDarwinThreadedItem item;

      if (threaded_binds == NULL)
        return TRUE;

      if (ctx->source_start == NULL)
      {
        gchar * source_path = NULL;
        GMappedFile * file;

#ifdef HAVE_DARWIN
        if (self->task != GUM_DARWIN_PORT_NULL)
        {
          GumDarwinMappingDetails mapping;
          if (gum_darwin_query_mapped_address (self->task, vm_base, &mapping))
            source_path = g_strdup (mapping.path);
        }
#endif
        if (source_path == NULL)
        {
          source_path = g_strdup (self->name);
          if (source_path == NULL)
            return TRUE;
        }
        file = g_mapped_file_new (source_path, FALSE, NULL);
        g_free (source_path);
        if (file == NULL)
          return TRUE;

        ctx->source_start = (const guint8 *) g_mapped_file_get_contents (file);
        ctx->source_end = ctx->source_start + g_mapped_file_get_length (file);
        ctx->source_file = file;
      }

      cursor = details->offset;

      do
      {
        const guint8 * raw_slot;

        raw_slot = ctx->source_start + segment->file_offset + cursor;
        if (raw_slot < ctx->source_start ||
            raw_slot + sizeof (guint64) > ctx->source_end)
        {
          return FALSE;
        }

        gum_darwin_threaded_item_parse (*((const guint64 *) raw_slot), &item);

        if (item.type == GUM_DARWIN_THREADED_BIND)
        {
          guint ordinal = item.bind_ordinal;
          GumImportDetails * d;

          if (ordinal >= threaded_binds->len)
            return TRUE;
          d = &g_array_index (threaded_binds, GumImportDetails, ordinal);
          d->slot = vm_base + cursor;

          ctx->carry_on = ctx->func (d, ctx->user_data);
        }

        cursor += item.delta * sizeof (guint64);
      }
      while (item.delta != 0 && ctx->carry_on);

      break;
    }
    default:
      g_assert_not_reached ();
  }

  return ctx->carry_on;
}

/**
 * gum_darwin_module_enumerate_exports:
 * @self: module
 * @func: (scope call): function called with each export
 * @user_data: data to pass to @func
 *
 * Enumerates exports of the module.
 */
void
gum_darwin_module_enumerate_exports (GumDarwinModule * self,
                                     GumFoundDarwinExportFunc func,
                                     gpointer user_data)
{
  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return;

  if (self->exports != NULL)
  {
    gum_exports_trie_foreach (self->exports, self->exports_end, func,
        user_data);
  }
  else if (self->filetype == GUM_DARWIN_MODULE_FILETYPE_DYLINKER)
  {
    GumEmitExportFromSymbolContext ctx;

    ctx.func = func;
    ctx.user_data = user_data;

    gum_darwin_module_enumerate_symbols (self, gum_emit_export_from_symbol,
        &ctx);
  }
}

static gboolean
gum_emit_export_from_symbol (const GumDarwinSymbolDetails * details,
                             gpointer user_data)
{
  GumEmitExportFromSymbolContext * ctx = user_data;
  GumDarwinExportDetails d;

  if ((details->type & GUM_N_EXT) == 0)
    return TRUE;

  if ((details->type & GUM_N_TYPE) != GUM_N_SECT)
    return TRUE;

  d.name = details->name;
  d.flags = GUM_DARWIN_EXPORT_ABSOLUTE;
  d.offset = details->address;

  return ctx->func (&d, ctx->user_data);
}

/**
 * gum_darwin_module_enumerate_symbols:
 * @self: module
 * @func: (scope call): function called with each symbol
 * @user_data: data to pass to @func
 *
 * Enumerates symbols of the module.
 */
void
gum_darwin_module_enumerate_symbols (GumDarwinModule * self,
                                     GumFoundDarwinSymbolFunc func,
                                     gpointer user_data)
{
  GumDarwinModuleImage * image;
  const GumSymtabCommand * symtab;
  GumAddress slide;
  const guint8 * symbols, * strings;
  gpointer symbols_malloc_data = NULL;
  gpointer strings_malloc_data = NULL;
  gsize symbol_index;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    goto beach;
  }

  image = self->image;

  symtab = self->symtab;
  if (symtab == NULL)
    goto beach;

  slide = gum_darwin_module_get_slide (self);

  if (image->linkedit != NULL)
  {
    symbols = (guint8 *) image->linkedit + symtab->symoff;
    strings = (guint8 *) image->linkedit + symtab->stroff;
  }
  else
  {
    GumAddress linkedit;
    gsize symbol_size;

    if (!gum_find_linkedit (image->data, image->size, &linkedit))
      goto beach;
    linkedit += slide;

    symbol_size = (self->pointer_size == 8)
        ? sizeof (GumNList64)
        : sizeof (GumNList32);

    gum_darwin_module_read_and_assign (self, linkedit + symtab->symoff,
        symtab->nsyms * symbol_size, &symbols, NULL, &symbols_malloc_data);
    gum_darwin_module_read_and_assign (self, linkedit + symtab->stroff,
        symtab->strsize, &strings, NULL, &strings_malloc_data);
    if (symbols == NULL || strings == NULL)
      goto beach;
  }

  for (symbol_index = 0; symbol_index != symtab->nsyms; symbol_index++)
  {
    GumDarwinSymbolDetails details;
    gboolean carry_on;

    if (self->pointer_size == 8)
    {
      const GumNList64 * symbol;

      symbol = (GumNList64 *) (symbols + (symbol_index * sizeof (GumNList64)));

      details.name = (const gchar *) (strings + symbol->n_strx);
      details.address = (symbol->n_value != 0) ? symbol->n_value + slide : 0;

      details.type = symbol->n_type;
      details.section = symbol->n_sect;
      details.description = symbol->n_desc;
    }
    else
    {
      const GumNList32 * symbol;

      symbol = (GumNList32 *) (symbols + (symbol_index * sizeof (GumNList32)));

      details.name = (const gchar *) (strings + symbol->n_strx);
      details.address = (symbol->n_value != 0) ? symbol->n_value + slide : 0;

      details.type = symbol->n_type;
      details.section = symbol->n_sect;
      details.description = symbol->n_desc;
    }

    carry_on = func (&details, user_data);
    if (!carry_on)
      goto beach;
  }

beach:
  g_free (strings_malloc_data);
  g_free (symbols_malloc_data);
}

GumAddress
gum_darwin_module_get_slide (GumDarwinModule * self)
{
  return self->base_address - self->preferred_address;
}

const GumDarwinSegment *
gum_darwin_module_get_nth_segment (GumDarwinModule * self,
                                   gsize index)
{
  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return NULL;

  if (index >= self->segments->len)
    return NULL;

  return &g_array_index (self->segments, GumDarwinSegment, index);
}

/**
 * gum_darwin_module_enumerate_sections:
 * @self: module
 * @func: (scope call): function called with each section
 * @user_data: data to pass to @func
 *
 * Enumerates sections of the module.
 */
void
gum_darwin_module_enumerate_sections (GumDarwinModule * self,
                                      GumFoundDarwinSectionFunc func,
                                      gpointer user_data)
{
  const GumMachHeader32 * header;
  gconstpointer command;
  gsize command_index;
  GumAddress slide;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return;

  header = (GumMachHeader32 *) self->image->data;
  if (header->magic == GUM_MH_MAGIC_32)
    command = (GumMachHeader32 *) self->image->data + 1;
  else
    command = (GumMachHeader64 *) self->image->data + 1;
  slide = gum_darwin_module_get_slide (self);
  for (command_index = 0; command_index != header->ncmds; command_index++)
  {
    const GumLoadCommand * lc = command;

    if (lc->cmd == GUM_LC_SEGMENT_32 || lc->cmd == GUM_LC_SEGMENT_64)
    {
      GumDarwinSectionDetails details;
      const guint8 * sections;
      gsize section_count, section_index;

      if (lc->cmd == GUM_LC_SEGMENT_32)
      {
        const GumSegmentCommand32 * sc = command;

        details.protection = sc->initprot;

        sections = (const guint8 *) (sc + 1);
        section_count = sc->nsects;
      }
      else
      {
        const GumSegmentCommand64 * sc = command;

        details.protection = sc->initprot;

        sections = (const guint8 *) (sc + 1);
        section_count = sc->nsects;
      }

      for (section_index = 0; section_index != section_count; section_index++)
      {
        if (lc->cmd == GUM_LC_SEGMENT_32)
        {
          const GumSection32 * s =
              (const GumSection32 *) sections + section_index;

          g_strlcpy (details.segment_name, s->segname,
              sizeof (details.segment_name));
          g_strlcpy (details.section_name, s->sectname,
              sizeof (details.section_name));

          details.vm_address = s->addr + (guint32) slide;
          details.size = s->size;
          details.file_offset = s->offset;
          details.flags = s->flags;
        }
        else
        {
          const GumSection64 * s =
              (const GumSection64 *) sections + section_index;

          g_strlcpy (details.segment_name, s->segname,
              sizeof (details.segment_name));
          g_strlcpy (details.section_name, s->sectname,
              sizeof (details.section_name));

          details.vm_address = s->addr + (guint64) slide;
          details.size = s->size;
          details.file_offset = s->offset;
          details.flags = s->flags;
        }

        if (!func (&details, user_data))
          return;
      }
    }

    command = (const guint8 *) command + lc->cmdsize;
  }
}

gboolean
gum_darwin_module_is_address_in_text_section (GumDarwinModule * self,
                                              GumAddress address)
{
  gboolean metadata_is_offline;
  GumAddress normalized_address;
  guint i;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return FALSE;

  metadata_is_offline = self->source_path != NULL || self->source_blob != NULL;

  normalized_address = metadata_is_offline
      ? address - self->base_address
      : address;

  for (i = 0; i != self->text_ranges->len; i++)
  {
    GumMemoryRange * r = &g_array_index (self->text_ranges, GumMemoryRange, i);
    if (GUM_MEMORY_RANGE_INCLUDES (r, normalized_address))
      return TRUE;
  }

  return FALSE;
}

/**
 * gum_darwin_module_enumerate_chained_fixups:
 * @self: module
 * @func: (scope call): function called with each fixup
 * @user_data: data to pass to @func
 *
 * Enumerates chained fixups of the module.
 */
void
gum_darwin_module_enumerate_chained_fixups (
    GumDarwinModule * self,
    GumFoundDarwinChainedFixupsFunc func,
    gpointer user_data)
{
  GumDarwinModuleImage * image;
  const GumMachHeader32 * header;
  gconstpointer command;
  gsize command_index;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return;

  image = self->image;

  header = image->data;
  if (header->magic == GUM_MH_MAGIC_32)
    command = (GumMachHeader32 *) image->data + 1;
  else
    command = (GumMachHeader64 *) image->data + 1;
  for (command_index = 0; command_index != header->ncmds; command_index++)
  {
    const GumLoadCommand * lc = command;

    if (lc->cmd == GUM_LC_DYLD_CHAINED_FIXUPS)
    {
      const GumLinkeditDataCommand * fixups = command;
      GumAddress linkedit;
      GumDarwinChainedFixupsDetails details;

      if (!gum_find_linkedit (image->data, image->size, &linkedit))
        return;

      linkedit += gum_darwin_module_get_slide (self);

      details.vm_address = linkedit + fixups->dataoff;
      details.file_offset = fixups->dataoff;
      details.size = fixups->datasize;

      if (!func (&details, user_data))
        return;
    }

    command = (const guint8 *) command + lc->cmdsize;
  }
}

/**
 * gum_darwin_module_enumerate_rebases:
 * @self: module
 * @func: (scope call): function called with each rebase
 * @user_data: data to pass to @func
 *
 * Enumerates rebases of the module.
 */
void
gum_darwin_module_enumerate_rebases (GumDarwinModule * self,
                                     GumFoundDarwinRebaseFunc func,
                                     gpointer user_data)
{
  const guint8 * start, * end, * p;
  gboolean done;
  GumDarwinRebaseDetails details;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    return;
  }

  start = self->rebases;
  end = self->rebases_end;
  p = start;
  done = FALSE;

  details.segment = gum_darwin_module_get_nth_segment (self, 0);
  details.offset = 0;
  details.type = 0;
  details.slide = gum_darwin_module_get_slide (self);

  while (!done && p != end)
  {
    guint8 opcode = *p & GUM_REBASE_OPCODE_MASK;
    guint8 immediate = *p & GUM_REBASE_IMMEDIATE_MASK;

    p++;

    switch (opcode)
    {
      case GUM_REBASE_OPCODE_DONE:
        done = TRUE;
        break;
      case GUM_REBASE_OPCODE_SET_TYPE_IMM:
        details.type = immediate;
        break;
      case GUM_REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      {
        gint segment_index = immediate;
        details.segment =
            gum_darwin_module_get_nth_segment (self, segment_index);
        if (details.segment == NULL)
          return;
        details.offset = gum_read_uleb128 (&p, end);
        break;
      }
      case GUM_REBASE_OPCODE_ADD_ADDR_ULEB:
        details.offset += gum_read_uleb128 (&p, end);
        break;
      case GUM_REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
        details.offset += immediate * self->pointer_size;
        break;
      case GUM_REBASE_OPCODE_DO_REBASE_IMM_TIMES:
      {
        guint8 i;

        for (i = 0; i != immediate; i++)
        {
          if (!func (&details, user_data))
            return;
          details.offset += self->pointer_size;
        }

        break;
      }
      case GUM_REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
      {
        guint64 count, i;

        count = gum_read_uleb128 (&p, end);
        for (i = 0; i != count; i++)
        {
          if (!func (&details, user_data))
            return;
          details.offset += self->pointer_size;
        }

        break;
      }
      case GUM_REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
        if (!func (&details, user_data))
          return;
        details.offset += self->pointer_size + gum_read_uleb128 (&p, end);
        break;
      case GUM_REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
      {
        gsize count, skip, i;

        count = gum_read_uleb128 (&p, end);
        skip = gum_read_uleb128 (&p, end);
        for (i = 0; i != count; ++i)
        {
          if (!func (&details, user_data))
            return;
          details.offset += self->pointer_size + skip;
        }

        break;
      }
      default:
        return;
    }
  }
}

/**
 * gum_darwin_module_enumerate_binds:
 * @self: module
 * @func: (scope call): function called with each bind
 * @user_data: data to pass to @func
 *
 * Enumerates binds of the module.
 */
void
gum_darwin_module_enumerate_binds (GumDarwinModule * self,
                                   GumFoundDarwinBindFunc func,
                                   gpointer user_data)
{
  const guint8 * start, * end, * p;
  gboolean done;
  GumDarwinBindDetails details;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    return;
  }

  start = self->binds;
  end = self->binds_end;
  p = start;
  done = FALSE;

  details.segment = gum_darwin_module_get_nth_segment (self, 0);
  details.offset = 0;
  details.type = 0;
  details.library_ordinal = 0;
  details.symbol_name = NULL;
  details.symbol_flags = 0;
  details.addend = 0;
  details.threaded_table_size = 0;

  while (!done && p != end)
  {
    guint8 opcode = *p & GUM_BIND_OPCODE_MASK;
    guint8 immediate = *p & GUM_BIND_IMMEDIATE_MASK;

    p++;

    switch (opcode)
    {
      case GUM_BIND_OPCODE_DONE:
        done = TRUE;
        break;
      case GUM_BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
        details.library_ordinal = immediate;
        break;
      case GUM_BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
        details.library_ordinal = gum_read_uleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
        if (immediate == 0)
        {
          details.library_ordinal = 0;
        }
        else
        {
          gint8 value = GUM_BIND_OPCODE_MASK | immediate;
          details.library_ordinal = value;
        }
        break;
      case GUM_BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
        details.symbol_name = (gchar *) p;
        details.symbol_flags = immediate;
        while (*p != '\0')
          p++;
        p++;
        break;
      case GUM_BIND_OPCODE_SET_TYPE_IMM:
        details.type = immediate;
        break;
      case GUM_BIND_OPCODE_SET_ADDEND_SLEB:
        details.addend = gum_read_sleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      {
        gint segment_index = immediate;
        details.segment =
            gum_darwin_module_get_nth_segment (self, segment_index);
        if (details.segment == NULL)
          return;
        details.offset = gum_read_uleb128 (&p, end);
        break;
      }
      case GUM_BIND_OPCODE_ADD_ADDR_ULEB:
        details.offset += gum_read_uleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_DO_BIND:
        if (!func (&details, user_data))
          return;
        details.offset += self->pointer_size;
        break;
      case GUM_BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
        if (!func (&details, user_data))
          return;
        details.offset += self->pointer_size + gum_read_uleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
        if (!func (&details, user_data))
          return;
        details.offset += self->pointer_size + (immediate * self->pointer_size);
        break;
      case GUM_BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
      {
        guint64 count, skip, i;

        count = gum_read_uleb128 (&p, end);
        skip = gum_read_uleb128 (&p, end);
        for (i = 0; i != count; ++i)
        {
          if (!func (&details, user_data))
            return;
          details.offset += self->pointer_size + skip;
        }

        break;
      }
      case GUM_BIND_OPCODE_THREADED:
      {
        switch (immediate)
        {
          case GUM_BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB:
          {
            guint64 size;

            size = gum_read_uleb128 (&p, end);
            if (size > G_MAXUINT16)
              return;

            details.type = GUM_DARWIN_BIND_THREADED_TABLE;
            details.threaded_table_size = size;

            if (!func (&details, user_data))
              return;

            break;
          }
          case GUM_BIND_SUBOPCODE_THREADED_APPLY:
          {
            details.type = GUM_DARWIN_BIND_THREADED_ITEMS;

            if (!func (&details, user_data))
              return;

            break;
          }
          default:
            return;
        }

        break;
      }
      default:
        return;
    }
  }
}

/**
 * gum_darwin_module_enumerate_lazy_binds:
 * @self: module
 * @func: (scope call): function called with each lazy bind
 * @user_data: data to pass to @func
 *
 * Enumerates lazy binds of the module.
 */
void
gum_darwin_module_enumerate_lazy_binds (GumDarwinModule * self,
                                        GumFoundDarwinBindFunc func,
                                        gpointer user_data)
{
  const guint8 * start, * end, * p;
  GumDarwinBindDetails details;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    return;
  }

  start = self->lazy_binds;
  end = self->lazy_binds_end;
  p = start;

  details.segment = gum_darwin_module_get_nth_segment (self, 0);
  details.offset = 0;
  details.type = GUM_DARWIN_BIND_POINTER;
  details.library_ordinal = 0;
  details.symbol_name = NULL;
  details.symbol_flags = 0;
  details.addend = 0;

  while (p != end)
  {
    guint8 opcode = *p & GUM_BIND_OPCODE_MASK;
    guint8 immediate = *p & GUM_BIND_IMMEDIATE_MASK;

    p++;

    switch (opcode)
    {
      case GUM_BIND_OPCODE_DONE:
        break;
      case GUM_BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
        details.library_ordinal = immediate;
        break;
      case GUM_BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
        details.library_ordinal = gum_read_uleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
        if (immediate == 0)
        {
          details.library_ordinal = 0;
        }
        else
        {
          gint8 value = GUM_BIND_OPCODE_MASK | immediate;
          details.library_ordinal = value;
        }
        break;
      case GUM_BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
        details.symbol_name = (gchar *) p;
        details.symbol_flags = immediate;
        while (*p != '\0')
          p++;
        p++;
        break;
      case GUM_BIND_OPCODE_SET_TYPE_IMM:
        details.type = immediate;
        break;
      case GUM_BIND_OPCODE_SET_ADDEND_SLEB:
        details.addend = gum_read_sleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
      {
        gint segment_index = immediate;
        details.segment =
            gum_darwin_module_get_nth_segment (self, segment_index);
        if (details.segment == NULL)
          return;
        details.offset = gum_read_uleb128 (&p, end);
        break;
      }
      case GUM_BIND_OPCODE_ADD_ADDR_ULEB:
        details.offset += gum_read_uleb128 (&p, end);
        break;
      case GUM_BIND_OPCODE_DO_BIND:
        if (!func (&details, user_data))
          return;
        details.offset += self->pointer_size;
        break;
      case GUM_BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
      case GUM_BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
      case GUM_BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
      default:
        return;
    }
  }
}

static void
gum_darwin_module_enumerate_chained_binds (GumDarwinModule * self,
                                           GumFoundDarwinBindFunc func,
                                           gpointer user_data)
{
  GumEmitImportContext * ctx = user_data;

  g_assert (ctx->resolver != NULL);

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self))
    return;

  gum_darwin_module_enumerate_chained_fixups (self,
      (GumFoundDarwinChainedFixupsFunc) gum_emit_chained_imports,
      ctx);
}

static gboolean
gum_emit_chained_imports (const GumDarwinChainedFixupsDetails * details,
                          GumEmitImportContext * ctx)
{
  GumDarwinModule * self = ctx->module;
  const guint8 * fixups_start, * fixups_end;
  gpointer malloc_data;
  const GumChainedFixupsHeader * fixups_header;
  const gchar * symbols;
  GHashTable * targets;
  guint imp_index;
  const GumChainedStartsInImage * image_starts;
  gsize slide;
  guint seg_index;

  gum_darwin_module_read_and_assign (self, details->vm_address, details->size,
      &fixups_start, &fixups_end, &malloc_data);
  if (fixups_start == NULL)
    return ctx->carry_on;

  fixups_header = (const GumChainedFixupsHeader *) fixups_start;

  symbols = (const gchar *) fixups_start + fixups_header->symbols_offset;
  targets = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  for (imp_index = 0; imp_index != fixups_header->imports_count; imp_index++)
  {
    guint name_offset;
    gint8 lib_ordinal;
    GumImportDetails * d;
    gpointer key;

    switch (fixups_header->imports_format)
    {
      case GUM_CHAINED_IMPORT:
      {
        const GumChainedImport * imports = (const GumChainedImport *)
            (fixups_start + fixups_header->imports_offset);
        const GumChainedImport * import = &imports[imp_index];

        name_offset = import->name_offset;
        lib_ordinal = import->lib_ordinal;

        break;
      }
      case GUM_CHAINED_IMPORT_ADDEND:
      {
        const GumChainedImportAddend * imports =
            (const GumChainedImportAddend *) (fixups_start +
                fixups_header->imports_offset);
        const GumChainedImportAddend * import = &imports[imp_index];

        name_offset = import->name_offset;
        lib_ordinal = import->lib_ordinal;

        break;
      }
      case GUM_CHAINED_IMPORT_ADDEND64:
      {
        const GumChainedImportAddend64 * imports =
            (const GumChainedImportAddend64 *) (fixups_start +
                fixups_header->imports_offset);
        const GumChainedImportAddend64 * import = &imports[imp_index];

        name_offset = import->name_offset;
        lib_ordinal = import->lib_ordinal;

        break;
      }
      default:
        goto skip;
    }

    d = g_new (GumImportDetails, 1);
    d->type = GUM_IMPORT_UNKNOWN;
    d->name = symbols + name_offset;
    d->module = gum_darwin_module_get_dependency_by_ordinal (self, lib_ordinal);
    d->address = ctx->resolver (d->module, d->name, ctx->user_data);
    d->slot = 0;

    key = GSIZE_TO_POINTER (gum_strip_code_address (d->address));

    g_hash_table_replace (targets, key, d);
  }

  image_starts = (const GumChainedStartsInImage *) (fixups_start +
      fixups_header->starts_offset);

  slide = gum_darwin_module_get_slide (self);

  for (seg_index = 0; seg_index != image_starts->seg_count; seg_index++)
  {
    const guint seg_offset = image_starts->seg_info_offset[seg_index];
    const GumChainedStartsInSegment * seg_starts;
    const GumDarwinSegment * current_seg;
    guint16 page_index;

    if (seg_offset == 0)
      continue;

    seg_starts = (const GumChainedStartsInSegment *)
        ((const guint8 *) image_starts + seg_offset);

    current_seg = gum_darwin_module_get_nth_segment (self, seg_index);

    for (page_index = 0; page_index != seg_starts->page_count; page_index++)
    {
      guint16 start;
      GumAddress page_address;
      const guint8 * page_start, * page_end, * cursor;
      gpointer page_malloc_data;

      start = seg_starts->page_start[page_index];
      if (start == GUM_CHAINED_PTR_START_NONE)
        continue;

      page_address = current_seg->vm_address +
          (page_index * seg_starts->page_size) + start + slide;

      gum_darwin_module_read_and_assign (self, page_address,
          seg_starts->page_size - start, &page_start, &page_end,
          &page_malloc_data);
      if (page_start == NULL)
        continue;

      cursor = page_start;

      for (; cursor != page_end; cursor += GLIB_SIZEOF_VOID_P)
      {
        GumAddress candidate = *(guint64 *) cursor;
        gpointer key;
        GumImportDetails * d;

        if (candidate == 0)
          continue;

        key = GSIZE_TO_POINTER (gum_strip_code_address (candidate));

        d = g_hash_table_lookup (targets, key);
        if (d == NULL)
          continue;

        d->slot = page_address + (cursor - page_start);

        ctx->carry_on = ctx->func (d, ctx->user_data);
        if (!ctx->carry_on)
          break;
      }

      g_free (page_malloc_data);

      if (!ctx->carry_on)
        break;
    }

    if (!ctx->carry_on)
      break;
  }

skip:
  g_hash_table_unref (targets);
  g_free (malloc_data);

  return ctx->carry_on;
}

void
gum_darwin_module_query_tlv_parameters (GumDarwinModule * self,
                                        GumDarwinTlvParameters * params)
{
  GumMachHeader32 * header;
  guint32 flags;
  GumQueryTlvParamsContext ctx;

  params->num_descriptors = 0;
  params->descriptors_offset = 0;
  params->data_offset = 0;
  params->data_size = 0;
  params->bss_size = 0;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return;

  header = self->image->data;

  if (header->magic == GUM_MH_MAGIC_32)
    flags = header->flags;
  else
    flags = ((GumMachHeader64 *) header)->flags;
  if ((flags & GUM_MH_HAS_TLV_DESCRIPTORS) == 0)
    return;

  ctx.header = header;
  ctx.params = params;
  gum_darwin_module_enumerate_sections (self, gum_collect_tlv_params, &ctx);
}

static gboolean
gum_collect_tlv_params (const GumDarwinSectionDetails * section,
                        gpointer user_data)
{
  GumQueryTlvParamsContext * ctx = user_data;
  GumDarwinTlvParameters * params = ctx->params;

  switch (section->flags & GUM_SECTION_TYPE_MASK)
  {
    case GUM_S_THREAD_LOCAL_VARIABLES:
    {
      gsize descriptor_size = (ctx->header->magic == GUM_MH_MAGIC_64)
          ? sizeof (GumTlvThunk64)
          : sizeof (GumTlvThunk32);
      params->num_descriptors = section->size / descriptor_size;
      params->descriptors_offset = section->file_offset;
      break;
    }
    case GUM_S_THREAD_LOCAL_REGULAR:
      params->data_offset = section->file_offset;
      params->data_size = section->size;
      break;
    case GUM_S_THREAD_LOCAL_ZEROFILL:
      params->bss_size = section->size;
      break;
    default:
      break;
  }

  return TRUE;
}

/**
 * gum_darwin_module_enumerate_tlv_descriptors:
 * @self: module
 * @func: (scope call): function called with each TLV descriptor
 * @user_data: data to pass to @func
 *
 * Enumerates TLV descriptors of the module.
 */
void
gum_darwin_module_enumerate_tlv_descriptors (
    GumDarwinModule * self,
    GumFoundDarwinTlvDescriptorFunc func,
    gpointer user_data)
{
  GumDarwinTlvParameters tlv;
  gconstpointer descriptors;
  gsize i;
  guint32 format;

  gum_darwin_module_query_tlv_parameters (self, &tlv);
  if (tlv.num_descriptors == 0)
    return;

  descriptors =
      (const guint8 *) self->image->data + tlv.descriptors_offset;
  format = ((GumMachHeader32 *) self->image->data)->magic;

  for (i = 0; i != tlv.num_descriptors; i++)
  {
    GumDarwinTlvDescriptorDetails details;

    if (format == GUM_MH_MAGIC_32)
    {
      const GumTlvThunk32 * d = &((const GumTlvThunk32 *) descriptors)[i];
      details.file_offset =
          tlv.descriptors_offset + (i * sizeof (GumTlvThunk32));
      details.thunk = d->thunk;
      details.key = d->key;
      details.offset = d->offset;
    }
    else
    {
      const GumTlvThunk64 * d = &((const GumTlvThunk64 *) descriptors)[i];
      details.file_offset =
          tlv.descriptors_offset + (i * sizeof (GumTlvThunk64));
      details.thunk = d->thunk;
      details.key = d->key;
      details.offset = d->offset;
    }

    if (!func (&details, user_data))
      return;
  }
}

/**
 * gum_darwin_module_enumerate_init_pointers:
 * @self: module
 * @func: (scope call): function called with each init pointer
 * @user_data: data to pass to @func
 *
 * Enumerates initialization pointers of the module.
 */
void
gum_darwin_module_enumerate_init_pointers (GumDarwinModule * self,
                                           GumFoundDarwinInitPointersFunc func,
                                           gpointer user_data)
{
  GumEmitInitPointersContext ctx;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    return;
  }

  ctx.func = func;
  ctx.user_data = user_data;
  ctx.pointer_size = self->pointer_size;
  gum_darwin_module_enumerate_sections (self, gum_emit_section_init_pointers,
      &ctx);
}

/**
 * gum_darwin_module_enumerate_init_offsets:
 * @self: module
 * @func: (scope call): function called with each init offset
 * @user_data: data to pass to @func
 *
 * Enumerates initialization offsets of the module.
 */
void
gum_darwin_module_enumerate_init_offsets (GumDarwinModule * self,
                                          GumFoundDarwinInitOffsetsFunc func,
                                          gpointer user_data)
{
  GumEmitInitOffsetsContext ctx;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    return;
  }

  ctx.func = func;
  ctx.user_data = user_data;
  gum_darwin_module_enumerate_sections (self, gum_emit_section_init_offsets,
      &ctx);
}

/**
 * gum_darwin_module_enumerate_term_pointers:
 * @self: module
 * @func: (scope call): function called with each term pointer
 * @user_data: data to pass to @func
 *
 * Enumerates termination pointers of the module.
 */
void
gum_darwin_module_enumerate_term_pointers (GumDarwinModule * self,
                                           GumFoundDarwinTermPointersFunc func,
                                           gpointer user_data)
{
  GumEmitTermPointersContext ctx;

  if (GUM_DARWIN_MODULE_HAS_HEADER_ONLY (self) ||
      !gum_darwin_module_ensure_image_loaded (self, NULL))
  {
    return;
  }

  ctx.func = func;
  ctx.user_data = user_data;
  ctx.pointer_size = self->pointer_size;
  gum_darwin_module_enumerate_sections (self, gum_emit_section_term_pointers,
      &ctx);
}

static gboolean
gum_emit_section_init_pointers (const GumDarwinSectionDetails * details,
                                gpointer user_data)
{
  if ((details->flags & GUM_SECTION_TYPE_MASK) == GUM_S_MOD_INIT_FUNC_POINTERS)
  {
    GumEmitInitPointersContext * ctx = user_data;
    GumDarwinInitPointersDetails d;

    d.address = details->vm_address;
    d.count = details->size / ctx->pointer_size;

    return ctx->func (&d, ctx->user_data);
  }

  return TRUE;
}

static gboolean
gum_emit_section_init_offsets (const GumDarwinSectionDetails * details,
                               gpointer user_data)
{
  if ((details->flags & GUM_SECTION_TYPE_MASK) == GUM_S_INIT_FUNC_OFFSETS)
  {
    GumEmitInitOffsetsContext * ctx = user_data;
    GumDarwinInitOffsetsDetails d;

    d.address = details->vm_address;
    d.count = details->size / sizeof (guint32);

    return ctx->func (&d, ctx->user_data);
  }

  return TRUE;
}

static gboolean
gum_emit_section_term_pointers (const GumDarwinSectionDetails * details,
                                gpointer user_data)
{
  if ((details->flags & GUM_SECTION_TYPE_MASK) == GUM_S_MOD_TERM_FUNC_POINTERS)
  {
    GumEmitTermPointersContext * ctx = user_data;
    GumDarwinTermPointersDetails d;

    d.address = details->vm_address;
    d.count = details->size / ctx->pointer_size;

    return ctx->func (&d, ctx->user_data);
  }

  return TRUE;
}

/**
 * gum_darwin_module_enumerate_dependencies:
 * @self: module
 * @func: (scope call): function called with each dependency
 * @user_data: data to pass to @func
 *
 * Enumerates dependencies of the module.
 */
void
gum_darwin_module_enumerate_dependencies (GumDarwinModule * self,
                                          GumFoundDependencyFunc func,
                                          gpointer user_data)
{
  guint i;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return;

  for (i = 0; i != self->dependencies->len; i++)
  {
    const GumDependencyDetails * d =
        &g_array_index (self->dependencies, GumDependencyDetails, i);

    if (!func (d, user_data))
      return;
  }
}

/**
 * gum_darwin_module_enumerate_function_starts:
 * @self: module
 * @func: (scope call): function called with each function start
 * @user_data: data to pass to @func
 *
 * Enumerates function starts of the module.
 */
void
gum_darwin_module_enumerate_function_starts (
    GumDarwinModule * self,
    GumFoundDarwinFunctionStartsFunc func,
    gpointer user_data)
{
  GumDarwinModuleImage * image;
  const GumMachHeader32 * header;
  gconstpointer command;
  gsize command_index;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return;

  image = self->image;

  header = (GumMachHeader32 *) image->data;
  if (header->magic == GUM_MH_MAGIC_32)
    command = (GumMachHeader32 *) image->data + 1;
  else
    command = (GumMachHeader64 *) image->data + 1;
  for (command_index = 0; command_index != header->ncmds; command_index++)
  {
    const GumLoadCommand * lc = command;

    if (lc->cmd == GUM_LC_FUNCTION_STARTS)
    {
      const GumLinkeditDataCommand * starts = command;
      GumAddress linkedit;
      GumDarwinFunctionStartsDetails details;

      if (!gum_find_linkedit (image->data, image->size, &linkedit))
        return;

      linkedit += gum_darwin_module_get_slide (self);

      details.vm_address = linkedit + starts->dataoff;
      details.file_offset = starts->dataoff;
      details.size = starts->datasize;

      if (!func (&details, user_data))
        return;
    }

    command = (const guint8 *) command + lc->cmdsize;
  }
}

const gchar *
gum_darwin_module_get_dependency_by_ordinal (GumDarwinModule * self,
                                             gint ordinal)
{
  gint i;

  if (!gum_darwin_module_ensure_image_loaded (self, NULL))
    return NULL;

  switch (ordinal)
  {
    case GUM_BIND_SPECIAL_DYLIB_SELF:
      return self->name;
    case GUM_BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
    case GUM_BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
    case GUM_BIND_SPECIAL_DYLIB_WEAK_LOOKUP:
      return NULL;
  }

  i = ordinal - 1;

  if (i < 0 || i >= (gint) self->dependencies->len)
    return NULL;

  return g_array_index (self->dependencies, GumDependencyDetails, i).name;
}

gboolean
gum_darwin_module_ensure_image_loaded (GumDarwinModule * self,
                                       GError ** error)
{
  if (self->image != NULL)
    return TRUE;

  return gum_darwin_module_load_image_from_memory (self, error);
}

void
gum_darwin_threaded_item_parse (guint64 value,
                                GumDarwinThreadedItem * result)
{
  result->is_authenticated      = (value >> 63) & 1;
  result->type                  = (value >> 62) & 1;
  result->delta                 = (value >> 51) & GUM_INT11_MASK;
  result->key                   = (value >> 49) & GUM_INT2_MASK;
  result->has_address_diversity = (value >> 48) & 1;
  result->diversity             = (value >> 32) & GUM_INT16_MASK;

  if (result->type == GUM_DARWIN_THREADED_BIND)
  {
    result->bind_ordinal = value & GUM_INT16_MASK;
  }
  else if (result->type == GUM_DARWIN_THREADED_REBASE)
  {
    if (result->is_authenticated)
    {
      result->rebase_address = value & GUM_INT32_MASK;
    }
    else
    {
      guint64 top_8_bits, bottom_43_bits, sign_bits;
      gboolean sign_bit_set;

      top_8_bits = (value << 13) & G_GUINT64_CONSTANT (0xff00000000000000);
      bottom_43_bits = value     & G_GUINT64_CONSTANT (0x000007ffffffffff);

      sign_bit_set = (value >> 42) & 1;
      if (sign_bit_set)
        sign_bits = G_GUINT64_CONSTANT (0x00fff80000000000);
      else
        sign_bits = 0;

      result->rebase_address = top_8_bits | sign_bits | bottom_43_bits;
    }
  }
}

static gboolean
gum_darwin_module_load_image_from_filesystem (GumDarwinModule * self,
                                              const gchar * path,
                                              GError ** error)
{
  gboolean success;
  GMappedFile * file;
  gsize size, size_in_pages, page_size;
  gpointer data;
  GBytes * blob;

  file = g_mapped_file_new (path, FALSE, NULL);
  if (file == NULL)
    goto not_found;

  size = g_mapped_file_get_length (file);
  page_size = gum_query_page_size ();
  size_in_pages = size / page_size;
  if (size % page_size != 0)
    size_in_pages++;

  data = gum_memory_allocate (NULL, size_in_pages * page_size, page_size,
      GUM_PAGE_RW);
  memcpy (data, g_mapped_file_get_contents (file), size);

  g_clear_pointer (&file, g_mapped_file_unref);

  blob = gum_page_allocation_new_bytes (data, size, size_in_pages * page_size);

  success = gum_darwin_module_load_image_from_blob (self, blob, error);

  g_bytes_unref (blob);

  return success;

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
        "Module not found at \"%s\"", path);
    return FALSE;
  }
}

static gboolean
gum_darwin_module_load_image_header_from_filesystem (GumDarwinModule * self,
                                                     const gchar * path,
                                                     GError ** error)
{
  gboolean success;
  GMappedFile * file;
  gsize page_size, size, size_in_pages;
  gpointer data;
  gsize data_size;
  GBytes * blob;
  gsize header_size, cursor;
  gboolean is_fat;

  file = g_mapped_file_new (path, FALSE, NULL);
  if (file == NULL)
    goto not_found;

  page_size = gum_query_page_size ();
  data = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  data_size = page_size;
  size = page_size;

  header_size = 0;
  cursor = 0;
  do
  {
    gsize header_offset = 0;

    memcpy (data, g_mapped_file_get_contents (file) + cursor, size);
    if (!gum_darwin_module_get_header_offset_size (self, data, size,
        &header_offset, &header_size, error))
    {
      gum_memory_free (data, data_size);
      g_clear_pointer (&file, g_mapped_file_unref);
      return FALSE;
    }

    cursor += header_offset;
    is_fat = header_offset > 0;
  }
  while (is_fat);

  size_in_pages = header_size / page_size;
  if (header_size % page_size != 0)
    size_in_pages++;

  if (size_in_pages != 1)
  {
    gum_memory_free (data, data_size);
    data_size = size_in_pages * page_size;
    data = gum_memory_allocate (NULL, data_size, page_size, GUM_PAGE_RW);
  }

  memcpy (data, g_mapped_file_get_contents (file) + cursor, header_size);

  g_clear_pointer (&file, g_mapped_file_unref);

  blob = gum_page_allocation_new_bytes (data, header_size, data_size);

  success = gum_darwin_module_load_image_from_blob (self, blob, error);

  g_bytes_unref (blob);

  return success;

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
        "Module not found at \"%s\"", path);
    return FALSE;
  }
}

static gboolean
gum_darwin_module_get_header_offset_size (GumDarwinModule * self,
                                          gpointer data,
                                          gsize data_size,
                                          gsize * out_offset,
                                          gsize * out_size,
                                          GError ** error)
{
  GumFatHeader * fat_header;
  gpointer data_end;
  gboolean found;

  fat_header = data;
  data_end = (guint8 *) data + data_size;

  found = FALSE;
  switch (fat_header->magic)
  {
    case GUM_FAT_CIGAM_32:
    {
      guint32 count, i;

      count = GUINT32_FROM_BE (fat_header->nfat_arch);
      for (i = 0; i != count && !found; i++)
      {
        GumFatArch32 * fat_arch;
        guint32 offset;
        GumDarwinCpuType cpu_type;
        GumDarwinCpuSubtype cpu_subtype;

        fat_arch = ((GumFatArch32 *) (fat_header + 1)) + i;
        if ((gpointer) (fat_arch + 1) > data_end)
          goto invalid_blob;

        offset = GUINT32_FROM_BE (fat_arch->offset);
        cpu_type = GUINT32_FROM_BE (fat_arch->cputype);
        cpu_subtype = GUINT32_FROM_BE (fat_arch->cpusubtype);

        found = gum_darwin_module_can_load (self, cpu_type, cpu_subtype);
        if (found)
        {
          *out_offset = offset;
          *out_size = (gum_pointer_size_from_cpu_type (cpu_type) == 8)
              ? sizeof (GumMachHeader64)
              : sizeof (GumMachHeader32);
        }
      }

      break;
    }
    case GUM_MH_MAGIC_32:
    {
      GumMachHeader32 * header = data;

      if ((gpointer) (header + 1) > data_end)
        goto invalid_blob;

      found = gum_darwin_module_can_load (self, header->cputype,
          header->cpusubtype);
      if (found)
      {
        *out_offset = 0;
        *out_size = sizeof (GumMachHeader32) + header->sizeofcmds;
      }

      break;
    }
    case GUM_MH_MAGIC_64:
    {
      GumMachHeader64 * header = data;

      if ((gpointer) (header + 1) > data_end)
        goto invalid_blob;

      found = gum_darwin_module_can_load (self, header->cputype,
          header->cpusubtype);
      if (found)
      {
        *out_offset = 0;
        *out_size = sizeof (GumMachHeader64) + header->sizeofcmds;
      }

      break;
    }
    default:
      goto invalid_blob;
  }

  if (!found)
    goto incompatible_image;

  return TRUE;

invalid_blob:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Invalid Mach-O image");
    return FALSE;
  }
incompatible_image:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Incompatible Mach-O image");
    return FALSE;
  }
}

static gboolean
gum_darwin_module_load_image_from_blob (GumDarwinModule * self,
                                        GBytes * blob,
                                        GError ** error)
{
  GumDarwinModuleImage * image;
  guint8 * blob_start, * blob_end;
  gsize blob_size;
  gsize page_size;
  gboolean is_page_aligned;
  gpointer data;
  gsize size;
  guint32 magic;

  image = gum_darwin_module_image_new ();
  image->bytes = g_bytes_ref (blob);

  blob_start = (guint8 *) g_bytes_get_data (blob, &blob_size);
  blob_end = blob_start + blob_size;

  page_size = gum_query_page_size ();
  is_page_aligned = (GPOINTER_TO_SIZE (blob_start) % page_size) == 0;
  if (!is_page_aligned)
  {
    gsize size_in_pages;
    gpointer copy;

    size_in_pages = blob_size / page_size;
    if (blob_size % page_size != 0)
      size_in_pages++;

    copy = gum_memory_allocate (NULL, size_in_pages * page_size, page_size,
        GUM_PAGE_RW);
    memcpy (copy, blob_start, blob_size);

    blob = gum_page_allocation_new_bytes (copy, blob_size,
        size_in_pages * page_size);
    blob_start = copy;
    blob_end = blob_start + blob_size;

    g_bytes_unref (image->bytes);
    image->bytes = blob;
  }

  data = blob_start;
  size = blob_size;

  if (blob_size < 4)
    goto invalid_blob;
  magic = *((guint32 *) data);

  if (magic == GUM_FAT_CIGAM_32)
  {
    GumFatHeader * fat_header;
    guint32 count, i;
    gboolean found;

    fat_header = (GumFatHeader *) blob_start;

    count = GUINT32_FROM_BE (fat_header->nfat_arch);
    found = FALSE;
    for (i = 0; i != count && !found; i++)
    {
      GumFatArch32 * fat_arch;
      GumDarwinCpuType cpu_type;
      GumDarwinCpuSubtype cpu_subtype;

      fat_arch = ((GumFatArch32 *) (fat_header + 1)) + i;
      if ((guint8 *) (fat_arch + 1) > blob_end)
        goto invalid_blob;

      cpu_type = GUINT32_FROM_BE (fat_arch->cputype);
      cpu_subtype = GUINT32_FROM_BE (fat_arch->cpusubtype);

      found = gum_darwin_module_can_load (self, cpu_type, cpu_subtype);
      if (found)
      {
        data = blob_start + GUINT32_FROM_BE (fat_arch->offset);
        size = GUINT32_FROM_BE (fat_arch->size);
      }
    }

    if (!found)
      goto incompatible_image;

    if ((guint8 *) data + 4 > blob_end)
      goto invalid_blob;
    magic = *((guint32 *) data);
  }

  switch (magic)
  {
    case GUM_MH_MAGIC_32:
    {
      GumMachHeader32 * header = data;

      if ((guint8 *) (header + 1) > blob_end)
        goto invalid_blob;

      if (!gum_darwin_module_can_load (self, header->cputype,
          header->cpusubtype))
      {
        goto incompatible_image;
      }

      break;
    }
    case GUM_MH_MAGIC_64:
    {
      GumMachHeader64 * header = data;

      if ((guint8 *) (header + 1) > blob_end)
        goto invalid_blob;

      if (!gum_darwin_module_can_load (self, header->cputype,
          header->cpusubtype))
      {
        goto incompatible_image;
      }

      break;
    }
    default:
      goto invalid_blob;
  }

  image->data = data;
  image->size = size;
  image->linkedit = data;

  return gum_darwin_module_take_image (self, image, error);

invalid_blob:
  {
    gum_darwin_module_image_free (image);

    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Invalid Mach-O image");
    return FALSE;
  }
incompatible_image:
  {
    gum_darwin_module_image_free (image);

    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Incompatible Mach-O image");
    return FALSE;
  }
}

static gboolean
gum_darwin_module_load_image_from_memory (GumDarwinModule * self,
                                          GError ** error)
{
  guint8 * start, * end;
  gpointer malloc_data;
  GumDarwinModuleImage * image;

  g_assert (self->base_address != 0);

  gum_darwin_module_read_and_assign (self, self->base_address,
      GUM_MAX_MACHO_METADATA_SIZE, (const guint8 **) &start,
      (const guint8 **) &end, &malloc_data);
  if (start == NULL)
    goto invalid_task;

  image = gum_darwin_module_image_new ();

  image->data = start;
  image->size = end - start;

  image->malloc_data = malloc_data;

  return gum_darwin_module_take_image (self, image, error);

invalid_task:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Process is dead");
    return FALSE;
  }
}

static GBytes *
gum_page_allocation_new_bytes (gpointer data,
                               gsize content_size,
                               gsize allocation_size)
{
  GumPageAllocation * allocation;

  allocation = g_slice_new (GumPageAllocation);
  allocation->data = data;
  allocation->size = allocation_size;

  return g_bytes_new_with_free_func (data, content_size,
      (GDestroyNotify) gum_page_allocation_free, allocation);
}

static void
gum_page_allocation_free (GumPageAllocation * self)
{
  gum_memory_free (self->data, self->size);

  g_slice_free (GumPageAllocation, self);
}

static gboolean
gum_darwin_module_can_load (GumDarwinModule * self,
                            GumDarwinCpuType cpu_type,
                            GumDarwinCpuSubtype cpu_subtype)
{
  GumCpuType canonical_cpu_type;
  gboolean allow_any_cpu, allow_any_ptrauth;

  canonical_cpu_type = gum_cpu_type_from_darwin (cpu_type);

  allow_any_cpu = self->cpu_type == GUM_CPU_INVALID;
  if (allow_any_cpu)
  {
    gboolean is_supported = canonical_cpu_type != GUM_CPU_INVALID;
    if (!is_supported)
      return FALSE;
  }
  else
  {
    gboolean matches_selected_cpu = canonical_cpu_type == self->cpu_type;
    if (!matches_selected_cpu)
      return FALSE;
  }

  allow_any_ptrauth = self->ptrauth_support == GUM_PTRAUTH_INVALID;
  if (!allow_any_ptrauth)
  {
    gboolean matches_selected_ptrauth =
        gum_ptrauth_support_from_darwin (cpu_type, cpu_subtype)
        == self->ptrauth_support;
    if (!matches_selected_ptrauth)
      return FALSE;
  }

  return TRUE;
}

static gboolean
gum_darwin_module_take_image (GumDarwinModule * self,
                              GumDarwinModuleImage * image,
                              GError ** error)
{
  gboolean success = FALSE;
  const GumMachHeader32 * header;
  gconstpointer command;
  gsize command_index;
  const GumLinkeditDataCommand * exports_trie = NULL;

  g_assert (self->image == NULL);
  self->image = image;

  header = (GumMachHeader32 *) image->data;

  self->filetype = header->filetype;

  if (self->cpu_type == GUM_CPU_INVALID)
    self->cpu_type = gum_cpu_type_from_darwin (header->cputype);

  if (self->ptrauth_support == GUM_PTRAUTH_INVALID)
  {
    self->ptrauth_support =
        gum_ptrauth_support_from_darwin (header->cputype, header->cpusubtype);
  }

  self->pointer_size = gum_pointer_size_from_cpu_type (header->cputype);

  if (header->magic == GUM_MH_MAGIC_32)
    command = (GumMachHeader32 *) image->data + 1;
  else
    command = (GumMachHeader64 *) image->data + 1;
  for (command_index = 0; command_index != header->ncmds; command_index++)
  {
    const GumLoadCommand * lc = (GumLoadCommand *) command;

    switch (lc->cmd)
    {
      case GUM_LC_ID_DYLIB:
      {
        if (self->name == NULL)
        {
          const GumDylib * dl = &((GumDylibCommand *) lc)->dylib;
          const gchar * raw_path;
          guint raw_path_len;

          raw_path = (const gchar *) command + dl->name.offset;
          raw_path_len = lc->cmdsize - sizeof (GumDylibCommand);

          self->name = g_strndup (raw_path, raw_path_len);
        }

        break;
      }
      case GUM_LC_ID_DYLINKER:
      {
        if (self->name == NULL)
        {
          const GumDylinkerCommand * dl = (const GumDylinkerCommand *) lc;
          const gchar * raw_path;
          guint raw_path_len;

          raw_path = (const gchar *) command + dl->name.offset;
          raw_path_len = lc->cmdsize - sizeof (GumDylinkerCommand);

          self->name = g_strndup (raw_path, raw_path_len);
        }

        break;
      }
      case GUM_LC_UUID:
      {
        if (self->uuid == NULL)
        {
          const GumUUIDCommand * uc = command;
          const uint8_t * u = uc->uuid;

          self->uuid = g_strdup_printf ("%02X%02X%02X%02X-%02X%02X-%02X%02X-"
              "%02X%02X-%02X%02X%02X%02X%02X%02X", u[0], u[1], u[2], u[3],
              u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13],
              u[14], u[15]);
        }

        break;
      }
      case GUM_LC_SOURCE_VERSION:
      {
        if (self->source_version == NULL)
        {
          const GumSourceVersionCommand * vc = command;
          const guint64 version = vc->version;
          guint a, b, c, d, e;
          GString * s;

          a = (version >> 40) & GUM_INT24_MASK;
          b = (version >> 30) & GUM_INT10_MASK;
          c = (version >> 20) & GUM_INT10_MASK;
          d = (version >> 10) & GUM_INT10_MASK;
          e = (version >>  0) & GUM_INT10_MASK;

          s = g_string_sized_new (28);

          g_string_append_printf (s, "%u.%u", a, b);

          if (c != 0 || d != 0 || e != 0)
              g_string_append_printf (s, ".%u", c);

          if (d != 0 || e != 0)
              g_string_append_printf (s, ".%u", d);

          if (e != 0)
              g_string_append_printf (s, ".%u", e);

          self->source_version = g_string_free (s, FALSE);
        }

        break;
      }
      case GUM_LC_SEGMENT_32:
      case GUM_LC_SEGMENT_64:
      {
        GumDarwinSegment segment;

        if (lc->cmd == GUM_LC_SEGMENT_32)
        {
          const GumSegmentCommand32 * sc = command;

          g_strlcpy (segment.name, sc->segname, sizeof (segment.name));
          segment.vm_address = sc->vmaddr;
          segment.vm_size = sc->vmsize;
          segment.file_offset = sc->fileoff;
          segment.file_size = sc->filesize;
          segment.protection = sc->initprot;
        }
        else
        {
          const GumSegmentCommand64 * sc = command;

          g_strlcpy (segment.name, sc->segname, sizeof (segment.name));
          segment.vm_address = sc->vmaddr;
          segment.vm_size = sc->vmsize;
          segment.file_offset = sc->fileoff;
          segment.file_size = sc->filesize;
          segment.protection = sc->initprot;
        }

        g_array_append_val (self->segments, segment);

        if (strcmp (segment.name, "__TEXT") == 0)
        {
          self->preferred_address = segment.vm_address;
          self->text_size = segment.vm_size;
        }

        break;
      }
      case GUM_LC_LOAD_DYLIB:
      case GUM_LC_LOAD_WEAK_DYLIB:
      case GUM_LC_REEXPORT_DYLIB:
      case GUM_LC_LOAD_UPWARD_DYLIB:
      {
        const GumDylibCommand * dc = command;
        GumDependencyDetails dep;

        dep.name = (const gchar *) command + dc->dylib.name.offset;
        switch (lc->cmd)
        {
          case GUM_LC_LOAD_DYLIB:
            dep.type = GUM_DEPENDENCY_REGULAR;
            break;
          case GUM_LC_LOAD_WEAK_DYLIB:
            dep.type = GUM_DEPENDENCY_WEAK;
            break;
          case GUM_LC_REEXPORT_DYLIB:
            dep.type = GUM_DEPENDENCY_REEXPORT;
            break;
          case GUM_LC_LOAD_UPWARD_DYLIB:
            dep.type = GUM_DEPENDENCY_UPWARD;
            break;
          default:
            g_assert_not_reached ();
        }
        g_array_append_val (self->dependencies, dep);

        if (lc->cmd == GUM_LC_REEXPORT_DYLIB)
          g_ptr_array_add (self->reexports, (gpointer) dep.name);

        break;
      }
      case GUM_LC_DYLD_INFO_ONLY:
        self->info = command;
        break;
      case GUM_LC_DYLD_EXPORTS_TRIE:
        exports_trie = command;
        break;
      case GUM_LC_SYMTAB:
        self->symtab = command;
        break;
      case GUM_LC_DYSYMTAB:
        self->dysymtab = command;
        break;
      default:
        break;
    }

    command = (const guint8 *) command + lc->cmdsize;
  }

  gum_darwin_module_enumerate_sections (self,
      gum_add_text_range_if_text_section, self->text_ranges);

  if (self->info == NULL)
  {
    if (exports_trie != NULL)
    {
      if (image->linkedit != NULL)
      {
        self->exports =
            (const guint8 *) image->linkedit + exports_trie->dataoff;
        self->exports_end = self->exports + exports_trie->datasize;
        self->exports_malloc_data = NULL;
      }
      else
      {
        GumAddress linkedit;

        if (!gum_find_linkedit (image->data, image->size, &linkedit))
          goto beach;
        linkedit += gum_darwin_module_get_slide (self);

        gum_darwin_module_read_and_assign (self,
            linkedit + exports_trie->dataoff,
            exports_trie->datasize,
            &self->exports,
            &self->exports_end,
            &self->exports_malloc_data);
      }
    }
  }
  else if (image->linkedit != NULL)
  {
    self->rebases = (const guint8 *) image->linkedit + self->info->rebase_off;
    self->rebases_end = self->rebases + self->info->rebase_size;
    self->rebases_malloc_data = NULL;

    self->binds = (const guint8 *) image->linkedit + self->info->bind_off;
    self->binds_end = self->binds + self->info->bind_size;
    self->binds_malloc_data = NULL;

    self->lazy_binds =
        (const guint8 *) image->linkedit + self->info->lazy_bind_off;
    self->lazy_binds_end = self->lazy_binds + self->info->lazy_bind_size;
    self->lazy_binds_malloc_data = NULL;

    self->exports = (const guint8 *) image->linkedit + self->info->export_off;
    self->exports_end = self->exports + self->info->export_size;
    self->exports_malloc_data = NULL;
  }
  else
  {
    GumAddress linkedit;

    if (!gum_find_linkedit (image->data, image->size, &linkedit))
      goto beach;
    linkedit += gum_darwin_module_get_slide (self);

    gum_darwin_module_read_and_assign (self,
        linkedit + self->info->rebase_off,
        self->info->rebase_size,
        &self->rebases,
        &self->rebases_end,
        &self->rebases_malloc_data);

    gum_darwin_module_read_and_assign (self,
        linkedit + self->info->bind_off,
        self->info->bind_size,
        &self->binds,
        &self->binds_end,
        &self->binds_malloc_data);

    gum_darwin_module_read_and_assign (self,
        linkedit + self->info->lazy_bind_off,
        self->info->lazy_bind_size,
        &self->lazy_binds,
        &self->lazy_binds_end,
        &self->lazy_binds_malloc_data);

    gum_darwin_module_read_and_assign (self,
        linkedit + self->info->export_off,
        self->info->export_size,
        &self->exports,
        &self->exports_end,
        &self->exports_malloc_data);
  }

  success = self->segments->len != 0;

beach:
  if (!success)
  {
    self->image = NULL;
    gum_darwin_module_image_free (image);

    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Invalid Mach-O image");
  }

  return success;
}

static void
gum_darwin_module_read_and_assign (GumDarwinModule * self,
                                   GumAddress address,
                                   gsize size,
                                   const guint8 ** start,
                                   const guint8 ** end,
                                   gpointer * malloc_data)
{
  guint8 * data;

  if (size == 0)
    goto empty_read;

  if (self->is_local)
  {
    *start = GSIZE_TO_POINTER (address);
    if (end != NULL)
      *end = GSIZE_TO_POINTER (address + size);

    *malloc_data = NULL;
  }
  else
  {
    gsize n_bytes_read;

    n_bytes_read = 0;
    data = gum_darwin_module_read_from_task (self, address, size,
        &n_bytes_read);

    *start = data;
    if (end != NULL)
      *end = (data != NULL) ? data + n_bytes_read : NULL;
    else if (n_bytes_read != size)
      goto short_read;

    *malloc_data = data;
  }

  return;

empty_read:
  {
    *start = NULL;
    if (end != NULL)
      *end = NULL;

    *malloc_data = NULL;

    return;
  }
short_read:
  {
    g_free (data);
    *start = NULL;

    *malloc_data = NULL;

    return;
  }
}

static gboolean
gum_find_linkedit (const guint8 * module,
                   gsize module_size,
                   GumAddress * linkedit)
{
  GumMachHeader32 * header;
  const guint8 * p;
  guint cmd_index;

  header = (GumMachHeader32 *) module;
  if (header->magic == GUM_MH_MAGIC_32)
    p = module + sizeof (GumMachHeader32);
  else
    p = module + sizeof (GumMachHeader64);
  for (cmd_index = 0; cmd_index != header->ncmds; cmd_index++)
  {
    GumLoadCommand * lc = (GumLoadCommand *) p;

    if (lc->cmd == GUM_LC_SEGMENT_32 || lc->cmd == GUM_LC_SEGMENT_64)
    {
      GumSegmentCommand32 * sc32 = (GumSegmentCommand32 *) lc;
      GumSegmentCommand64 * sc64 = (GumSegmentCommand64 *) lc;
      if (strncmp (sc32->segname, "__LINKEDIT", 10) == 0)
      {
        if (header->magic == GUM_MH_MAGIC_32)
          *linkedit = sc32->vmaddr - sc32->fileoff;
        else
          *linkedit = sc64->vmaddr - sc64->fileoff;
        return TRUE;
      }
    }

    p += lc->cmdsize;
  }

  return FALSE;
}

static gboolean
gum_add_text_range_if_text_section (const GumDarwinSectionDetails * details,
                                    gpointer user_data)
{
  GArray * ranges = user_data;

  if (gum_section_flags_indicate_text_section (details->flags))
  {
    GumMemoryRange r;
    r.base_address = details->vm_address;
    r.size = details->size;
    g_array_append_val (ranges, r);
  }

  return TRUE;
}

static gboolean
gum_section_flags_indicate_text_section (guint32 flags)
{
  return (flags & (GUM_S_ATTR_PURE_INSTRUCTIONS | GUM_S_ATTR_SOME_INSTRUCTIONS))
      != 0;
}

GumDarwinModuleImage *
gum_darwin_module_image_new (void)
{
  GumDarwinModuleImage * image;

  image = g_slice_new0 (GumDarwinModuleImage);
  image->shared_segments = g_array_new (FALSE, FALSE,
      sizeof (GumDarwinModuleImageSegment));

  return image;
}

GumDarwinModuleImage *
gum_darwin_module_image_dup (const GumDarwinModuleImage * other)
{
  GumDarwinModuleImage * image;

  image = g_slice_new0 (GumDarwinModuleImage);

  image->size = other->size;

  image->source_offset = other->source_offset;
  image->source_size = other->source_size;
  image->shared_offset = other->shared_offset;
  image->shared_size = other->shared_size;
  image->shared_segments = g_array_ref (other->shared_segments);

  if (other->bytes != NULL)
    image->bytes = g_bytes_ref (other->bytes);

  if (other->shared_segments->len > 0)
  {
    guint i;

    image->malloc_data = g_malloc (other->size);
    image->data = image->malloc_data;

    g_assert (other->source_size != 0);
    memcpy (image->data, other->data, other->source_size);

    for (i = 0; i != other->shared_segments->len; i++)
    {
      GumDarwinModuleImageSegment * s = &g_array_index (other->shared_segments,
          GumDarwinModuleImageSegment, i);
      memcpy ((guint8 *) image->data + s->offset,
          (const guint8 *) other->data + s->offset, s->size);
    }
  }
  else
  {
    image->malloc_data = g_memdup2 (other->data, other->size);
    image->data = image->malloc_data;
  }

  if (other->bytes != NULL)
  {
    gconstpointer data;
    gsize size;

    data = g_bytes_get_data (other->bytes, &size);
    if (other->linkedit >= data &&
        other->linkedit < (gconstpointer) ((const guint8 *) data + size))
    {
      image->linkedit = other->linkedit;
    }
  }

  if (image->linkedit == NULL && other->linkedit != NULL)
  {
    g_assert (other->linkedit >= other->data && other->linkedit <
        (gconstpointer) ((guint8 *) other->data + other->size));
    image->linkedit = (guint8 *) image->data +
        ((guint8 *) other->linkedit - (guint8 *) other->data);
  }

  return image;
}

void
gum_darwin_module_image_free (GumDarwinModuleImage * image)
{
  if (image == NULL)
    return;

  g_free (image->malloc_data);
  g_bytes_unref (image->bytes);

  g_array_unref (image->shared_segments);

  g_slice_free (GumDarwinModuleImage, image);
}

static gboolean
gum_exports_trie_find (const guint8 * exports,
                       const guint8 * exports_end,
                       const gchar * name,
                       GumDarwinExportDetails * details)
{
  const gchar * s;
  const guint8 * p;

  if (exports == exports_end)
    return FALSE;

  s = name;
  p = exports;
  while (p != NULL)
  {
    gint64 terminal_size;
    const guint8 * children;
    guint8 child_count, i;
    guint64 node_offset;

    terminal_size = gum_read_uleb128 (&p, exports_end);

    if (*s == '\0' && terminal_size != 0)
    {
      gum_darwin_export_details_init_from_node (details, name, p, exports_end);
      return TRUE;
    }

    children = p + terminal_size;
    child_count = *children++;
    p = children;
    node_offset = 0;
    for (i = 0; i != child_count; i++)
    {
      const gchar * symbol_cur;
      gboolean matching_edge;

      symbol_cur = s;
      matching_edge = TRUE;
      while (*p != '\0')
      {
        if (matching_edge)
        {
          if (*p != *symbol_cur)
            matching_edge = FALSE;
          symbol_cur++;
        }
        p++;
      }
      p++;

      if (matching_edge)
      {
        node_offset = gum_read_uleb128 (&p, exports_end);
        s = symbol_cur;
        break;
      }
      else
      {
        gum_skip_leb128 (&p, exports_end);
      }
    }

    if (node_offset != 0)
      p = exports + node_offset;
    else
      p = NULL;
  }

  return FALSE;
}

static gboolean
gum_exports_trie_foreach (const guint8 * exports,
                          const guint8 * exports_end,
                          GumFoundDarwinExportFunc func,
                          gpointer user_data)
{
  GumExportsTrieForeachContext ctx;
  gboolean carry_on;

  if (exports == exports_end)
    return TRUE;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.prefix = g_string_sized_new (1024);
  ctx.exports = exports;
  ctx.exports_end = exports_end;

  carry_on = gum_exports_trie_traverse (exports, &ctx);

  g_string_free (ctx.prefix, TRUE);

  return carry_on;
}

static gboolean
gum_exports_trie_traverse (const guint8 * p,
                           GumExportsTrieForeachContext * ctx)
{
  GString * prefix = ctx->prefix;
  const guint8 * exports = ctx->exports;
  const guint8 * exports_end = ctx->exports_end;
  gboolean carry_on;
  guint64 terminal_size;
  guint8 child_count, i;

  terminal_size = gum_read_uleb128 (&p, exports_end);
  if (terminal_size != 0)
  {
    GumDarwinExportDetails details;

    gum_darwin_export_details_init_from_node (&details, prefix->str, p,
        exports_end);

    carry_on = ctx->func (&details, ctx->user_data);
    if (!carry_on)
      return FALSE;
  }

  p += terminal_size;
  child_count = *p++;
  for (i = 0; i != child_count; i++)
  {
    gsize length = 0;

    while (*p != '\0')
    {
      g_string_append_c (prefix, *p++);
      length++;
    }
    p++;

    carry_on = gum_exports_trie_traverse (
        exports + gum_read_uleb128 (&p, exports_end),
        ctx);
    if (!carry_on)
      return FALSE;

    g_string_truncate (prefix, prefix->len - length);
  }

  return TRUE;
}

static void
gum_darwin_export_details_init_from_node (GumDarwinExportDetails * details,
                                          const gchar * name,
                                          const guint8 * node,
                                          const guint8 * exports_end)
{
  const guint8 * p = node;

  details->name = name;
  details->flags = gum_read_uleb128 (&p, exports_end);
  if ((details->flags & GUM_EXPORT_SYMBOL_FLAGS_REEXPORT) != 0)
  {
    details->reexport_library_ordinal = gum_read_uleb128 (&p, exports_end);
    details->reexport_symbol = (*p != '\0') ? (gchar *) p : name;
  }
  else if ((details->flags & GUM_EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER) != 0)
  {
    details->stub = gum_read_uleb128 (&p, exports_end);
    details->resolver = gum_read_uleb128 (&p, exports_end);
  }
  else
  {
    details->offset = gum_read_uleb128 (&p, exports_end);
  }
}

static GumCpuType
gum_cpu_type_from_darwin (GumDarwinCpuType cpu_type)
{
  switch (cpu_type)
  {
    case GUM_DARWIN_CPU_X86:
      return GUM_CPU_IA32;
    case GUM_DARWIN_CPU_X86_64:
      return GUM_CPU_AMD64;
    case GUM_DARWIN_CPU_ARM:
      return GUM_CPU_ARM;
    case GUM_DARWIN_CPU_ARM64:
      return GUM_CPU_ARM64;
    default:
      return GUM_CPU_INVALID;
  }
}

static GumPtrauthSupport
gum_ptrauth_support_from_darwin (GumDarwinCpuType cpu_type,
                                 GumDarwinCpuSubtype cpu_subtype)
{
  if (cpu_type == GUM_DARWIN_CPU_ARM64)
  {
    return ((cpu_subtype & GUM_DARWIN_CPU_SUBTYPE_MASK) ==
            GUM_DARWIN_CPU_SUBTYPE_ARM64E)
        ? GUM_PTRAUTH_SUPPORTED
        : GUM_PTRAUTH_UNSUPPORTED;
  }

  return GUM_PTRAUTH_UNSUPPORTED;
}

static guint
gum_pointer_size_from_cpu_type (GumDarwinCpuType cpu_type)
{
  switch (cpu_type)
  {
    case GUM_DARWIN_CPU_X86:
    case GUM_DARWIN_CPU_ARM:
      return 4;
    case GUM_DARWIN_CPU_X86_64:
    case GUM_DARWIN_CPU_ARM64:
      return 8;
    default:
      return 0;
  }
}
