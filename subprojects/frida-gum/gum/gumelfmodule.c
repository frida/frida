/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2019 Jon Wilson <jonwilson@zepler.net>
 * Copyright (C)      2021 Paul Schmidt <p.schmidt@tu-bs.de>
 * Copyright (C)      2026 Daniel Baier <admin@remoteshell-security.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumelfmodule.h"

#include "gumelfmodule-priv.h"
#ifdef HAVE_LZMA
# include <lzma.h>
#endif
#if defined (HAVE_ANDROID) && defined (HAVE_MINIZIP)
# include <minizip/mz.h>
# include <minizip/mz_strm.h>
# include <minizip/mz_strm_os.h>
# include <minizip/mz_zip.h>
# include <minizip/mz_zip_rw.h>
#endif

#include <string.h>

#define GUM_ELF_DEFAULT_MAPPED_SIZE (64 * 1024)
#define GUM_ELF_PAGE_START(value, page_size) \
    (GUM_ADDRESS (value) & ~GUM_ADDRESS (page_size - 1))

#define GUM_ELF_MODULE_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_ELF_MODULE_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

#define GUM_CHECK_BOUNDS(l, r, name) \
    G_STMT_START \
    { \
      if (!gum_elf_module_check_bounds (self, l, r, data, size, name, error)) \
        goto propagate_error; \
    } \
    G_STMT_END
#define GUM_CHECK_STR_BOUNDS(s, name) \
    G_STMT_START \
    { \
      if (!gum_elf_module_check_str_bounds (self, s, data, size, name, error)) \
        goto propagate_error; \
    } \
    G_STMT_END
#define GUM_READ(dst, src, type) \
    dst = G_PASTE (gum_elf_module_read_, type) (self, &src);

typedef guint GumElfDynamicAddressState;
typedef struct _GumElfRelocationGroup GumElfRelocationGroup;
typedef struct _GumElfEnumerateImportsContext GumElfEnumerateImportsContext;
typedef struct _GumElfEnumerateExportsContext GumElfEnumerateExportsContext;
typedef struct _GumElfStoreSymtabParamsContext GumElfStoreSymtabParamsContext;
typedef struct _GumElfEnumerateSymbolsContext GumElfEnumerateSymbolsContext;
typedef struct _GumElfEnumerateDepsContext GumElfEnumerateDepsContext;

enum
{
  PROP_0,
  PROP_ETYPE,
  PROP_POINTER_SIZE,
  PROP_BYTE_ORDER,
  PROP_OS_ABI,
  PROP_OS_ABI_VERSION,
  PROP_MACHINE,
  PROP_BASE_ADDRESS,
  PROP_PREFERRED_ADDRESS,
  PROP_MAPPED_SIZE,
  PROP_ENTRYPOINT,
  PROP_INTERPRETER,
  PROP_SOURCE_PATH,
  PROP_SOURCE_BLOB,
  PROP_SOURCE_MODE,
};

struct _GumElfModule
{
  GObject parent;

  gchar * source_path;
  GBytes * source_blob;
  GumElfSourceMode source_mode;

  GBytes * file_bytes;
  gconstpointer file_data;
  gsize file_size;
  GumMemoryRange file_mapped_range;

  GumElfEhdr ehdr;
  GArray * phdrs;
  GArray * shdrs;
  GArray * dyns;

  GPtrArray * sections;

  GumAddress base_address;
  GumAddress preferred_address;
  guint64 mapped_size;
  GumElfDynamicAddressState dynamic_address_state;
  const gchar * dynamic_strings;

  GMutex mutex;

  GumElfModule * fallback_elf_module;
  gboolean attempted_fallback_load;
};

enum _GumElfDynamicAddressState
{
  GUM_ELF_DYNAMIC_ADDRESS_PRISTINE,
  GUM_ELF_DYNAMIC_ADDRESS_ADJUSTED,
};

struct _GumElfRelocationGroup
{
  guint64 offset;
  guint64 size;
  guint64 entsize;
  gboolean relocs_have_addend;

  guint64 symtab_offset;
  guint64 symtab_entsize;

  const gchar * strings;
  const gchar * strings_base;
  gsize strings_size;

  const GumElfSectionDetails * parent;
};

struct _GumElfEnumerateImportsContext
{
  GumFoundImportFunc func;
  gpointer user_data;

  GHashTable * slots;
  guint32 jump_slot_type;
};

struct _GumElfEnumerateExportsContext
{
  GumFoundExportFunc func;
  gpointer user_data;
};

struct _GumElfStoreSymtabParamsContext
{
  guint pending;
  gboolean found_hash;

  gpointer entries;
  gsize entry_size;
  gsize entry_count;

  GumElfModule * module;
};

struct _GumElfEnumerateSymbolsContext
{
  GumFoundElfSymbolFunc func;
  gpointer user_data;
  guint count;
};

struct _GumElfEnumerateDepsContext
{
  GumFoundDependencyFunc func;
  gpointer user_data;

  GumElfModule * module;
};

static void gum_elf_module_dispose (GObject * object);
static void gum_elf_module_finalize (GObject * object);
static void gum_elf_module_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_elf_module_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static gboolean gum_elf_module_load_elf_header (GumElfModule * self,
    GError ** error);
static gboolean gum_elf_module_load_program_headers (GumElfModule * self,
    GError ** error);
static gboolean gum_elf_module_load_section_headers (GumElfModule * self,
    GError ** error);
static gboolean gum_elf_module_load_section_details (GumElfModule * self,
    GError ** error);
static gboolean gum_elf_module_load_dynamic_entries (GumElfModule * self,
    GError ** error);
static gconstpointer gum_elf_module_get_live_data (GumElfModule * self,
    gsize * size);
static void gum_elf_module_unload (GumElfModule * self);
static GumElfModule * gum_elf_module_try_get_fallback_elf_module (
    GumElfModule * self);
static gboolean gum_try_load_debugdata_as_fallback_module (
    const GumElfSectionDetails * details, gpointer user_data);
static gboolean gum_elf_module_emit_relocations (GumElfModule * self,
    const GumElfRelocationGroup * g, GumFoundElfRelocationFunc func,
    gpointer user_data);
static gboolean gum_emit_elf_import (const GumElfSymbolDetails * details,
    gpointer user_data);
static gboolean gum_try_get_jump_slot_relocation_type_for_machine (
    GumElfMachine machine, guint32 * type);
static gboolean gum_maybe_collect_import_slot_from_relocation (
    const GumElfRelocationDetails * details, gpointer user_data);
static gboolean gum_emit_elf_export (const GumElfSymbolDetails * details,
    gpointer user_data);
static void gum_elf_module_parse_symbol (GumElfModule * self,
    const GumElfSym * sym, const gchar * strings, GumElfSymbolDetails * d);
static void gum_elf_module_read_symbol (GumElfModule * self,
    gconstpointer raw_sym, GumElfSym * sym);
static gboolean gum_store_symtab_params (
    const GumElfDynamicEntryDetails * details, gpointer user_data);
static gboolean gum_adjust_symtab_params (const GumElfSectionDetails * details,
    gpointer user_data);
static gboolean gum_count_and_emit_symbol (const GumElfSymbolDetails * details,
    gpointer user_data);
static void gum_elf_module_enumerate_symbols_in_section (GumElfModule * self,
    GumElfSectionType section, GumFoundElfSymbolFunc func, gpointer user_data);
static gboolean gum_emit_each_needed (const GumElfDynamicEntryDetails * details,
    gpointer user_data);
static gboolean gum_elf_module_find_address_protection (GumElfModule * self,
    GumAddress address, GumPageProtection * prot);
static GumPageProtection gum_parse_phdr_protection (const GumElfPhdr * phdr);
static const GumElfPhdr * gum_elf_module_find_phdr_by_type (GumElfModule * self,
    guint32 type);
static const GumElfPhdr * gum_elf_module_find_load_phdr_by_address (
    GumElfModule * self, GumAddress address);
static const GumElfShdr * gum_elf_module_find_section_header_by_index (
    GumElfModule * self, guint i);
static const GumElfShdr * gum_elf_module_find_section_header_by_type (
    GumElfModule * self, GumElfSectionType type);
static GumElfSectionDetails * gum_elf_module_find_section_details_by_index (
    GumElfModule * self, guint i);
static GumAddress gum_elf_module_compute_preferred_address (
    GumElfModule * self);
static guint64 gum_elf_module_compute_mapped_size (GumElfModule * self);
static GumElfDynamicAddressState gum_elf_module_detect_dynamic_address_state (
    GumElfModule * self);
static gpointer gum_elf_module_resolve_dynamic_virtual_location (
    GumElfModule * self, GumAddress address);
static gboolean gum_store_dynamic_string_table (
    const GumElfDynamicEntryDetails * details, gpointer user_data);

static gboolean gum_elf_module_check_bounds (GumElfModule * self,
    gconstpointer left, gconstpointer right, gconstpointer base, gsize size,
    const gchar * name, GError ** error);
static gboolean gum_elf_module_check_str_bounds (GumElfModule * self,
    const gchar * str, gconstpointer base, gsize size, const gchar * name,
    GError ** error);

static guint8 gum_elf_module_read_uint8 (GumElfModule * self, const guint8 * v);
static guint16 gum_elf_module_read_uint16 (GumElfModule * self,
    const guint16 * v);
static gint32 gum_elf_module_read_int32 (GumElfModule * self, const gint32 * v);
static guint32 gum_elf_module_read_uint32 (GumElfModule * self,
    const guint32 * v);
static gint64 gum_elf_module_read_int64 (GumElfModule * self, const gint64 * v);
static guint64 gum_elf_module_read_uint64 (GumElfModule * self,
    const guint64 * v);

static GBytes * gum_decompress_xz (gconstpointer data, gsize size);

static gboolean gum_maybe_extract_from_apk (const gchar * path,
    GBytes ** file_bytes);

G_DEFINE_TYPE (GumElfModule, gum_elf_module, G_TYPE_OBJECT)

G_DEFINE_BOXED_TYPE (GumElfSectionDetails, gum_elf_section_details,
                     gum_elf_section_details_ref, gum_elf_section_details_unref)
G_DEFINE_BOXED_TYPE (GumElfSymbolDetails, gum_elf_symbol_details,
                     gum_elf_symbol_details_copy, gum_elf_symbol_details_free)

static void
gum_elf_module_class_init (GumElfModuleClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_elf_module_dispose;
  object_class->finalize = gum_elf_module_finalize;
  object_class->get_property = gum_elf_module_get_property;
  object_class->set_property = gum_elf_module_set_property;

  g_object_class_install_property (object_class, PROP_ETYPE,
      g_param_spec_enum ("etype", "Type", "ELF Type",
      GUM_TYPE_ELF_TYPE, GUM_ELF_NONE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_POINTER_SIZE,
      g_param_spec_uint ("pointer-size", "Pointer Size",
      "Pointer size in bytes", 4, 8, 8,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BYTE_ORDER,
      g_param_spec_int ("byte-order", "Byte Order",
      "Byte order/endian", G_LITTLE_ENDIAN, G_BIG_ENDIAN, G_BYTE_ORDER,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_OS_ABI,
      g_param_spec_enum ("os-abi", "OS ABI", "Operating system ABI",
      GUM_TYPE_ELF_OSABI, GUM_ELF_OS_SYSV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_OS_ABI_VERSION,
      g_param_spec_uint ("os-abi-version", "OS ABI Version",
      "Operating system ABI version", 0, G_MAXUINT8, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MACHINE,
      g_param_spec_enum ("machine", "Machine", "Machine",
      GUM_TYPE_ELF_MACHINE, GUM_ELF_MACHINE_NONE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BASE_ADDRESS,
      g_param_spec_uint64 ("base-address", "Base Address",
      "Base virtual address, or zero when operating offline", 0,
      G_MAXUINT64, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PREFERRED_ADDRESS,
      g_param_spec_uint64 ("preferred-address", "Preferred Address",
      "Preferred virtual address", 0, G_MAXUINT64, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MAPPED_SIZE,
      g_param_spec_uint64 ("mapped-size", "Mapped Size",
      "Mapped size", 0, G_MAXUINT64, GUM_ELF_DEFAULT_MAPPED_SIZE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ENTRYPOINT,
      g_param_spec_uint64 ("entrypoint", "Entrypoint",
      "Entrypoint virtual address", 0, G_MAXUINT64, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INTERPRETER,
      g_param_spec_string ("interpreter", "Interpreter", "Interpreter", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_PATH,
      g_param_spec_string ("source-path", "SourcePath", "Source path", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_BLOB,
      g_param_spec_boxed ("source-blob", "SourceBlob", "Source blob",
      G_TYPE_BYTES,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_MODE,
      g_param_spec_enum ("source-mode", "SourceMode", "Source mode",
      GUM_TYPE_ELF_SOURCE_MODE, GUM_ELF_SOURCE_MODE_OFFLINE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gum_elf_module_init (GumElfModule * self)
{
  self->source_mode = GUM_ELF_SOURCE_MODE_OFFLINE;

  self->phdrs = g_array_new (FALSE, FALSE, sizeof (GumElfPhdr));
  self->shdrs = g_array_new (FALSE, FALSE, sizeof (GumElfShdr));
  self->dyns = g_array_new (FALSE, FALSE, sizeof (GumElfDyn));

  self->sections = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gum_elf_section_details_unref);

  self->mapped_size = GUM_ELF_DEFAULT_MAPPED_SIZE;
  self->dynamic_address_state = GUM_ELF_DYNAMIC_ADDRESS_PRISTINE;

  g_mutex_init (&self->mutex);
}

static void
gum_elf_module_dispose (GObject * object)
{
  GumElfModule * self = GUM_ELF_MODULE (object);

  g_clear_object (&self->fallback_elf_module);

  gum_elf_module_unload (self);

  G_OBJECT_CLASS (gum_elf_module_parent_class)->dispose (object);
}

static void
gum_elf_module_finalize (GObject * object)
{
  GumElfModule * self = GUM_ELF_MODULE (object);

  g_mutex_clear (&self->mutex);

  g_ptr_array_unref (self->sections);

  g_array_unref (self->dyns);
  g_array_unref (self->shdrs);
  g_array_unref (self->phdrs);

  g_bytes_unref (self->source_blob);
  g_free (self->source_path);

  G_OBJECT_CLASS (gum_elf_module_parent_class)->finalize (object);
}

static void
gum_elf_module_get_property (GObject * object,
                             guint property_id,
                             GValue * value,
                             GParamSpec * pspec)
{
  GumElfModule * self = GUM_ELF_MODULE (object);

  switch (property_id)
  {
    case PROP_ETYPE:
      g_value_set_enum (value, gum_elf_module_get_etype (self));
      break;
    case PROP_POINTER_SIZE:
      g_value_set_uint (value, gum_elf_module_get_pointer_size (self));
      break;
    case PROP_BYTE_ORDER:
      g_value_set_int (value, gum_elf_module_get_byte_order (self));
      break;
    case PROP_OS_ABI:
      g_value_set_enum (value, gum_elf_module_get_os_abi (self));
      break;
    case PROP_OS_ABI_VERSION:
      g_value_set_uint (value, gum_elf_module_get_os_abi_version (self));
      break;
    case PROP_MACHINE:
      g_value_set_enum (value, gum_elf_module_get_machine (self));
      break;
    case PROP_BASE_ADDRESS:
      g_value_set_uint64 (value, gum_elf_module_get_base_address (self));
      break;
    case PROP_PREFERRED_ADDRESS:
      g_value_set_uint64 (value, gum_elf_module_get_preferred_address (self));
      break;
    case PROP_MAPPED_SIZE:
      g_value_set_uint64 (value, gum_elf_module_get_mapped_size (self));
      break;
    case PROP_ENTRYPOINT:
      g_value_set_uint64 (value, gum_elf_module_get_entrypoint (self));
      break;
    case PROP_INTERPRETER:
      g_value_set_string (value, gum_elf_module_get_interpreter (self));
      break;
    case PROP_SOURCE_PATH:
      g_value_set_string (value, gum_elf_module_get_source_path (self));
      break;
    case PROP_SOURCE_BLOB:
      g_value_set_boxed (value, gum_elf_module_get_source_blob (self));
      break;
    case PROP_SOURCE_MODE:
      g_value_set_enum (value, gum_elf_module_get_source_mode (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_elf_module_set_property (GObject * object,
                             guint property_id,
                             const GValue * value,
                             GParamSpec * pspec)
{
  GumElfModule * self = GUM_ELF_MODULE (object);

  switch (property_id)
  {
    case PROP_BASE_ADDRESS:
      self->base_address = g_value_get_uint64 (value);
      break;
    case PROP_SOURCE_PATH:
      g_free (self->source_path);
      self->source_path = g_value_dup_string (value);
      break;
    case PROP_SOURCE_BLOB:
      g_bytes_unref (self->source_blob);
      self->source_blob = g_value_dup_boxed (value);
      break;
    case PROP_SOURCE_MODE:
      self->source_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumElfModule *
gum_elf_module_new_from_file (const gchar * path,
                              GError ** error)
{
  GumElfModule * module;

  module = g_object_new (GUM_ELF_TYPE_MODULE,
      "source-path", path,
      "source-mode", GUM_ELF_SOURCE_MODE_OFFLINE,
      NULL);
  if (!gum_elf_module_load (module, error))
  {
    g_object_unref (module);
    return NULL;
  }

  return module;
}

GumElfModule *
gum_elf_module_new_from_blob (GBytes * blob,
                              GError ** error)
{
  GumElfModule * module;

  module = g_object_new (GUM_ELF_TYPE_MODULE,
      "source-blob", blob,
      "source-mode", GUM_ELF_SOURCE_MODE_OFFLINE,
      NULL);
  if (!gum_elf_module_load (module, error))
  {
    g_object_unref (module);
    return NULL;
  }

  return module;
}

GumElfModule *
gum_elf_module_new_from_memory (const gchar * path,
                                GumAddress base_address,
                                GError ** error)
{
  GumElfModule * module;

  module = g_object_new (GUM_ELF_TYPE_MODULE,
      "base-address", base_address,
      "source-path", path,
      "source-mode", GUM_ELF_SOURCE_MODE_ONLINE,
      NULL);
  if (!gum_elf_module_load (module, error))
  {
    g_object_unref (module);
    return NULL;
  }

  return module;
}

gboolean
gum_elf_module_load (GumElfModule * self,
                     GError ** error)
{
  GError * local_error = NULL;

  if (self->file_bytes != NULL)
    return TRUE;

  if (self->source_blob != NULL)
  {
    self->file_bytes = g_bytes_ref (self->source_blob);
  }
  else
  {
#ifdef HAVE_LINUX
    if (self->source_mode == GUM_ELF_SOURCE_MODE_ONLINE &&
        strcmp (self->source_path, "linux-vdso.so.1") == 0)
    {
      self->file_bytes = g_bytes_new_static (
          GSIZE_TO_POINTER (self->base_address), gum_query_page_size ());
    }
    else
#endif
    if (!gum_maybe_extract_from_apk (self->source_path, &self->file_bytes))
    {
      GMappedFile * file;
      gconstpointer data;
      gsize size;
      GumMemoryRange r;

      file = g_mapped_file_new (self->source_path, FALSE, &local_error);
      if (file != NULL)
      {
        self->file_bytes = g_mapped_file_get_bytes (file);
        g_mapped_file_unref (file);

        data = g_bytes_get_data (self->file_bytes, &size);
        r.base_address = GUM_ADDRESS (data);
        r.size = GUM_ALIGN_SIZE (size, gum_query_page_size ());
        gum_cloak_add_range (&r);
        self->file_mapped_range = r;
      }
      else
      {
        if (self->source_mode == GUM_ELF_SOURCE_MODE_OFFLINE)
          goto unable_to_open;

        self->file_bytes = g_bytes_new_static (
            GSIZE_TO_POINTER (self->base_address),
            G_MAXSIZE - self->base_address);
      }
    }
  }

  self->file_data = g_bytes_get_data (self->file_bytes, &self->file_size);

  if (!gum_elf_module_load_elf_header (self, error))
    goto propagate_error;

  if (!gum_elf_module_load_program_headers (self, error))
    goto propagate_error;

  self->mapped_size = gum_elf_module_compute_mapped_size (self);
  self->preferred_address = gum_elf_module_compute_preferred_address (self);

  if (self->file_data == GSIZE_TO_POINTER (self->base_address))
    self->file_size = self->mapped_size;

  if (!gum_elf_module_load_section_headers (self, error))
  {
    if (self->source_mode != GUM_ELF_SOURCE_MODE_ONLINE)
      goto propagate_error;
    g_clear_error (error);
  }

  if (!gum_elf_module_load_dynamic_entries (self, error))
    goto propagate_error;

  self->dynamic_address_state =
      gum_elf_module_detect_dynamic_address_state (self);

  gum_elf_module_enumerate_dynamic_entries (self,
      gum_store_dynamic_string_table, self);

  if (!gum_elf_module_load_section_details (self, error))
  {
    if (self->source_mode != GUM_ELF_SOURCE_MODE_ONLINE)
      goto propagate_error;
    g_clear_error (error);
  }

  return TRUE;

unable_to_open:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "%s", local_error->message);
    goto propagate_error;
  }
propagate_error:
  {
    g_clear_error (&local_error);

    gum_elf_module_unload (self);

    return FALSE;
  }
}

static gboolean
gum_elf_module_load_elf_header (GumElfModule * self,
                                GError ** error)
{
  gconstpointer data;
  gsize size;
  const GumElfIdentity * identity;

  data = gum_elf_module_get_live_data (self, &size);

  identity = data;
  GUM_CHECK_BOUNDS (identity, identity + 1, "ELF header");
  self->ehdr.identity = *identity;

#define GUM_READ_EHDR_FIELD(name, type) \
    GUM_READ (self->ehdr.name, src->name, type)
#define GUM_READ_EHDR() \
    G_STMT_START \
    { \
      GUM_READ_EHDR_FIELD (type,      uint16); \
      GUM_READ_EHDR_FIELD (machine,   uint16); \
      GUM_READ_EHDR_FIELD (version,   uint32); \
      GUM_READ_EHDR_FIELD (entry,     uint64); \
      GUM_READ_EHDR_FIELD (phoff,     uint64); \
      GUM_READ_EHDR_FIELD (shoff,     uint64); \
      GUM_READ_EHDR_FIELD (flags,     uint32); \
      GUM_READ_EHDR_FIELD (ehsize,    uint16); \
      GUM_READ_EHDR_FIELD (phentsize, uint16); \
      GUM_READ_EHDR_FIELD (phnum,     uint16); \
      GUM_READ_EHDR_FIELD (shentsize, uint16); \
      GUM_READ_EHDR_FIELD (shnum,     uint16); \
      GUM_READ_EHDR_FIELD (shstrndx,  uint16); \
    } \
    G_STMT_END
#define GUM_READ_EHDR32() \
    G_STMT_START \
    { \
      GUM_READ_EHDR_FIELD (type,      uint16); \
      GUM_READ_EHDR_FIELD (machine,   uint16); \
      GUM_READ_EHDR_FIELD (version,   uint32); \
      GUM_READ_EHDR_FIELD (entry,     uint32); \
      GUM_READ_EHDR_FIELD (phoff,     uint32); \
      GUM_READ_EHDR_FIELD (shoff,     uint32); \
      GUM_READ_EHDR_FIELD (flags,     uint32); \
      GUM_READ_EHDR_FIELD (ehsize,    uint16); \
      GUM_READ_EHDR_FIELD (phentsize, uint16); \
      GUM_READ_EHDR_FIELD (phnum,     uint16); \
      GUM_READ_EHDR_FIELD (shentsize, uint16); \
      GUM_READ_EHDR_FIELD (shnum,     uint16); \
      GUM_READ_EHDR_FIELD (shstrndx,  uint16); \
    } \
    G_STMT_END

  switch (identity->klass)
  {
    case GUM_ELF_CLASS_64:
    {
      const GumElfEhdr * src = data;

      GUM_CHECK_BOUNDS (src, src + 1, "ELF header");
      GUM_READ_EHDR ();

      break;
    }
    case GUM_ELF_CLASS_32:
    {
      const GumElfEhdr32 * src = data;

      GUM_CHECK_BOUNDS (src, src + 1, "ELF header");
      GUM_READ_EHDR32 ();

      break;
    }
    default:
      goto invalid_value;
  }

#undef GUM_READ_EHDR_FIELD
#undef GUM_READ_EHDR
#undef GUM_READ_EHDR32

  return TRUE;

invalid_value:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Invalid ELF header");
    goto propagate_error;
  }
propagate_error:
  {
    return FALSE;
  }
}

static gboolean
gum_elf_module_load_program_headers (GumElfModule * self,
                                     GError ** error)
{
  gconstpointer data;
  gsize size;
  guint16 n;
  gconstpointer start, end, cursor;
  guint16 i;

  data = gum_elf_module_get_file_data (self, &size);
  if (data == GSIZE_TO_POINTER (self->base_address))
    size = gum_query_page_size ();

  n = self->ehdr.phnum;

  start = (const guint8 *) data + self->ehdr.phoff;
  end = (const guint8 *) start + (n * self->ehdr.phentsize);
  GUM_CHECK_BOUNDS (start, end, "program headers");

  g_array_set_size (self->phdrs, n);

  cursor = start;
  for (i = 0; i != n; i++)
  {
    GumElfPhdr * dst = &g_array_index (self->phdrs, GumElfPhdr, i);

#define GUM_READ_PHDR_FIELD(name, type) \
    GUM_READ (dst->name, src->name, type)
#define GUM_READ_PHDR() \
    G_STMT_START \
    { \
      GUM_READ_PHDR_FIELD (type,   uint32); \
      GUM_READ_PHDR_FIELD (flags,  uint32); \
      GUM_READ_PHDR_FIELD (offset, uint64); \
      GUM_READ_PHDR_FIELD (vaddr,  uint64); \
      GUM_READ_PHDR_FIELD (paddr,  uint64); \
      GUM_READ_PHDR_FIELD (filesz, uint64); \
      GUM_READ_PHDR_FIELD (memsz,  uint64); \
      GUM_READ_PHDR_FIELD (align,  uint64); \
    } \
    G_STMT_END
#define GUM_READ_PHDR32() \
    G_STMT_START \
    { \
      GUM_READ_PHDR_FIELD (type,   uint32); \
      GUM_READ_PHDR_FIELD (offset, uint32); \
      GUM_READ_PHDR_FIELD (vaddr,  uint32); \
      GUM_READ_PHDR_FIELD (paddr,  uint32); \
      GUM_READ_PHDR_FIELD (filesz, uint32); \
      GUM_READ_PHDR_FIELD (memsz,  uint32); \
      GUM_READ_PHDR_FIELD (flags,  uint32); \
      GUM_READ_PHDR_FIELD (align,  uint32); \
    } \
    G_STMT_END

    switch (self->ehdr.identity.klass)
    {
      case GUM_ELF_CLASS_64:
      {
        const GumElfPhdr * src = cursor;
        GUM_READ_PHDR ();
        break;
      }
      case GUM_ELF_CLASS_32:
      {
        const GumElfPhdr32 * src = cursor;
        GUM_READ_PHDR32 ();
        break;
      }
      default:
        g_assert_not_reached ();
    }

#undef GUM_READ_PHDR_FIELD
#undef GUM_READ_PHDR
#undef GUM_READ_PHDR32

    cursor = (const guint8 *) cursor + self->ehdr.phentsize;
  }

  return TRUE;

propagate_error:
  {
    return FALSE;
  }
}

static gboolean
gum_elf_module_load_section_headers (GumElfModule * self,
                                     GError ** error)
{
  gconstpointer data;
  gsize size;
  guint16 n;
  gconstpointer start, end, cursor;
  guint16 i;

  data = gum_elf_module_get_file_data (self, &size);

  n = self->ehdr.shnum;

  start = (const guint8 *) data + self->ehdr.shoff;
  end = (const guint8 *) start + (n * self->ehdr.shentsize);
  if (end == start)
    return TRUE;
  GUM_CHECK_BOUNDS (start, end, "section headers");

  g_array_set_size (self->shdrs, n);

  cursor = start;
  for (i = 0; i != n; i++)
  {
    GumElfShdr * dst = &g_array_index (self->shdrs, GumElfShdr, i);

#define GUM_READ_SHDR_FIELD(name, type) \
    GUM_READ (dst->name, src->name, type)
#define GUM_READ_SHDR() \
    G_STMT_START \
    { \
      GUM_READ_SHDR_FIELD (name,      uint32); \
      GUM_READ_SHDR_FIELD (type,      uint32); \
      GUM_READ_SHDR_FIELD (flags,     uint64); \
      GUM_READ_SHDR_FIELD (addr,      uint64); \
      GUM_READ_SHDR_FIELD (offset,    uint64); \
      GUM_READ_SHDR_FIELD (size,      uint64); \
      GUM_READ_SHDR_FIELD (link,      uint32); \
      GUM_READ_SHDR_FIELD (info,      uint32); \
      GUM_READ_SHDR_FIELD (addralign, uint64); \
      GUM_READ_SHDR_FIELD (entsize,   uint64); \
    } \
    G_STMT_END
#define GUM_READ_SHDR32() \
    G_STMT_START \
    { \
      GUM_READ_SHDR_FIELD (name,      uint32); \
      GUM_READ_SHDR_FIELD (type,      uint32); \
      GUM_READ_SHDR_FIELD (flags,     uint32); \
      GUM_READ_SHDR_FIELD (addr,      uint32); \
      GUM_READ_SHDR_FIELD (offset,    uint32); \
      GUM_READ_SHDR_FIELD (size,      uint32); \
      GUM_READ_SHDR_FIELD (link,      uint32); \
      GUM_READ_SHDR_FIELD (info,      uint32); \
      GUM_READ_SHDR_FIELD (addralign, uint32); \
      GUM_READ_SHDR_FIELD (entsize,   uint32); \
    } \
    G_STMT_END

    switch (self->ehdr.identity.klass)
    {
      case GUM_ELF_CLASS_64:
      {
        const GumElfShdr * src = cursor;
        GUM_READ_SHDR ();
        break;
      }
      case GUM_ELF_CLASS_32:
      {
        const GumElfShdr32 * src = cursor;
        GUM_READ_SHDR32 ();
        break;
      }
      default:
        g_assert_not_reached ();
    }

#undef GUM_READ_SHDR_FIELD
#undef GUM_READ_SHDR
#undef GUM_READ_SHDR32

    cursor = (const guint8 *) cursor + self->ehdr.shentsize;
  }

  return TRUE;

propagate_error:
  {
    return FALSE;
  }
}

static gboolean
gum_elf_module_load_section_details (GumElfModule * self,
                                     GError ** error)
{
  const GumElfShdr * strings_shdr;
  gconstpointer data;
  gsize size;
  const gchar * strings;
  guint n, i;

  strings_shdr =
      gum_elf_module_find_section_header_by_index (self, self->ehdr.shstrndx);
  if (strings_shdr == NULL)
    return TRUE;

  data = gum_elf_module_get_file_data (self, &size);

  strings = (const gchar *) data + strings_shdr->offset;

  n = self->shdrs->len;

  for (i = 0; i != n; i++)
  {
    const GumElfShdr * shdr =
        &g_array_index (self->shdrs, GumElfShdr, i);
    const gchar * name = strings + shdr->name;
    GumElfSectionDetails * d;

    GUM_CHECK_STR_BOUNDS (name, "section name");

    d = g_slice_new (GumElfSectionDetails);

    if (name[0] != '\0')
    {
      d->id = g_strdup_printf ("%u%s%s",
          i,
          (name[0] != '.') ? "." : "",
          name);
    }
    else
    {
      d->id = g_strdup_printf ("%u", i);
    }
    d->name = g_strdup (name);
    d->type = shdr->type;
    d->flags = shdr->flags;
    d->address = ((shdr->flags & GUM_ELF_SECTION_FLAG_ALLOC) != 0)
        ? gum_elf_module_translate_to_online (self, shdr->addr)
        : shdr->addr;
    d->offset = shdr->offset;
    d->size = shdr->size;
    d->link = shdr->link;
    d->info = shdr->info;
    d->alignment = shdr->addralign;
    d->entry_size = shdr->entsize;
    if (!gum_elf_module_find_address_protection (self, shdr->addr,
        &d->protection))
    {
      d->protection = GUM_PAGE_NO_ACCESS;
    }

    d->ref_count = 1;

    g_ptr_array_add (self->sections, d);
  }

  return TRUE;

propagate_error:
  {
    g_ptr_array_set_size (self->sections, 0);

    return FALSE;
  }
}

static gboolean
gum_elf_module_load_dynamic_entries (GumElfModule * self,
                                     GError ** error)
{
  const GumElfPhdr * phdr;
  gconstpointer data;
  gsize size, entry_size, n;
  gconstpointer start, end, cursor;
  gsize i;

  phdr = gum_elf_module_find_phdr_by_type (self, GUM_ELF_PHDR_DYNAMIC);
  if (phdr == NULL)
    return TRUE;

  data = gum_elf_module_get_live_data (self, &size);

  entry_size = (self->ehdr.identity.klass == GUM_ELF_CLASS_64)
      ? sizeof (GumElfDyn)
      : sizeof (GumElfDyn32);
  n = phdr->filesz / entry_size;

  start = (self->source_mode == GUM_ELF_SOURCE_MODE_ONLINE)
      ? GSIZE_TO_POINTER (
          gum_elf_module_translate_to_online (self, phdr->vaddr))
      : (const guint8 *) data + phdr->offset;
  end = (const guint8 *) start + (n * entry_size);
  GUM_CHECK_BOUNDS (start, end, "dynamic entries");

  g_array_set_size (self->dyns, n);

  cursor = start;
  for (i = 0; i != n; i++)
  {
    GumElfDyn * dst = &g_array_index (self->dyns, GumElfDyn, i);

#define GUM_READ_DYN_FIELD(name, type) \
    GUM_READ (dst->name, src->name, type)
#define GUM_READ_DYN() \
    G_STMT_START \
    { \
      GUM_READ_DYN_FIELD (tag, int64); \
      GUM_READ_DYN_FIELD (val, uint64); \
    } \
    G_STMT_END
#define GUM_READ_DYN32() \
    G_STMT_START \
    { \
      GUM_READ_DYN_FIELD (tag, int32); \
      GUM_READ_DYN_FIELD (val, uint32); \
    } \
    G_STMT_END

    switch (self->ehdr.identity.klass)
    {
      case GUM_ELF_CLASS_64:
      {
        const GumElfDyn * src = cursor;
        GUM_READ_DYN ();
        break;
      }
      case GUM_ELF_CLASS_32:
      {
        const GumElfDyn32 * src = cursor;
        GUM_READ_DYN32 ();
        break;
      }
      default:
        g_assert_not_reached ();
    }

#undef GUM_READ_DYN_FIELD
#undef GUM_READ_DYN
#undef GUM_READ_DYN32

    cursor = (const guint8 *) cursor + entry_size;
  }

  return TRUE;

propagate_error:
  {
    return FALSE;
  }
}

static gconstpointer
gum_elf_module_get_live_data (GumElfModule * self,
                              gsize * size)
{
  if (self->source_mode == GUM_ELF_SOURCE_MODE_ONLINE)
  {
    *size = self->mapped_size;
    return GSIZE_TO_POINTER (self->base_address);
  }
  else
  {
    *size = self->file_size;
    return self->file_data;
  }
}

static void
gum_elf_module_unload (GumElfModule * self)
{
  self->dynamic_strings = NULL;
  self->dynamic_address_state = GUM_ELF_DYNAMIC_ADDRESS_PRISTINE;
  self->mapped_size = GUM_ELF_DEFAULT_MAPPED_SIZE;
  self->preferred_address = 0;

  g_ptr_array_set_size (self->sections, 0);

  g_array_set_size (self->dyns, 0);
  g_array_set_size (self->shdrs, 0);
  g_array_set_size (self->phdrs, 0);
  memset (&self->ehdr, 0, sizeof (self->ehdr));

  if (self->file_mapped_range.base_address != 0)
  {
    gum_cloak_remove_range (&self->file_mapped_range);
    self->file_mapped_range.base_address = 0;
    self->file_mapped_range.size = 0;
  }
  g_bytes_unref (self->file_bytes);
  self->file_bytes = NULL;
  self->file_data = NULL;
  self->file_size = 0;
}

static GumElfModule *
gum_elf_module_try_get_fallback_elf_module (GumElfModule * self)
{
  GUM_ELF_MODULE_LOCK (self);

  if (!self->attempted_fallback_load)
  {
    self->attempted_fallback_load = TRUE;

    if (self->source_mode == GUM_ELF_SOURCE_MODE_ONLINE)
    {
      self->fallback_elf_module =
          gum_elf_module_new_from_file (self->source_path, NULL);
    }
    else
    {
      gum_elf_module_enumerate_sections (self,
          gum_try_load_debugdata_as_fallback_module, self);
    }

    if (self->fallback_elf_module != NULL)
      self->fallback_elf_module->base_address = self->base_address;
  }

  GUM_ELF_MODULE_UNLOCK (self);

  return self->fallback_elf_module;
}

static gboolean
gum_try_load_debugdata_as_fallback_module (const GumElfSectionDetails * details,
                                           gpointer user_data)
{
  GumElfModule * self = user_data;
  GBytes * debugdata;

  if (strcmp (details->name, ".gnu_debugdata") != 0)
    return TRUE;

  debugdata = gum_decompress_xz (
      (const guint8 *) gum_elf_module_get_file_data (self, NULL) +
        details->offset,
      details->size);
  if (debugdata != NULL)
  {
    self->fallback_elf_module = gum_elf_module_new_from_blob (debugdata, NULL);
    g_bytes_unref (debugdata);
  }

  return FALSE;
}

GumElfType
gum_elf_module_get_etype (GumElfModule * self)
{
  return self->ehdr.type;
}

guint
gum_elf_module_get_pointer_size (GumElfModule * self)
{
  return (self->ehdr.identity.klass == GUM_ELF_CLASS_64) ? 8 : 4;
}

gint
gum_elf_module_get_byte_order (GumElfModule * self)
{
  return (self->ehdr.identity.data_encoding == GUM_ELF_DATA_ENCODING_LSB)
      ? G_LITTLE_ENDIAN
      : G_BIG_ENDIAN;
}

GumElfOSABI
gum_elf_module_get_os_abi (GumElfModule * self)
{
  return self->ehdr.identity.os_abi;
}

guint8
gum_elf_module_get_os_abi_version (GumElfModule * self)
{
  return self->ehdr.identity.os_abi_version;
}

GumElfMachine
gum_elf_module_get_machine (GumElfModule * self)
{
  return self->ehdr.machine;
}

GumAddress
gum_elf_module_get_base_address (GumElfModule * self)
{
  return self->base_address;
}

GumAddress
gum_elf_module_get_preferred_address (GumElfModule * self)
{
  return self->preferred_address;
}

guint64
gum_elf_module_get_mapped_size (GumElfModule * self)
{
  return self->mapped_size;
}

GumAddress
gum_elf_module_get_entrypoint (GumElfModule * self)
{
  GumAddress entrypoint = self->ehdr.entry;

  if (self->ehdr.type == GUM_ELF_DYN)
    entrypoint += self->base_address;

  return gum_elf_module_translate_to_online (self, entrypoint);
}

const gchar *
gum_elf_module_get_interpreter (GumElfModule * self)
{
  guint i;

  for (i = 0; i != self->phdrs->len; i++)
  {
    const GumElfPhdr * phdr = &g_array_index (self->phdrs, GumElfPhdr, i);

    if (phdr->type == GUM_ELF_PHDR_INTERP)
    {
      gconstpointer data;
      gsize size;
      const gchar * interp;

      data = gum_elf_module_get_file_data (self, &size);

      interp = (const gchar *) data + phdr->offset;
      if (!gum_elf_module_check_str_bounds (self, interp, data, size, "interp",
            NULL))
      {
        return NULL;
      }

      return interp;
    }
  }

  return NULL;
}

const gchar *
gum_elf_module_get_source_path (GumElfModule * self)
{
  return self->source_path;
}

GBytes *
gum_elf_module_get_source_blob (GumElfModule * self)
{
  return self->source_blob;
}

GumElfSourceMode
gum_elf_module_get_source_mode (GumElfModule * self)
{
  return self->source_mode;
}

gconstpointer
gum_elf_module_get_file_data (GumElfModule * self,
                              gsize * size)
{
  if (size != NULL)
    *size = self->file_size;

  return self->file_data;
}

/**
 * gum_elf_module_enumerate_segments:
 * @self: module
 * @func: (scope call): function called with each segment
 * @user_data: data to pass to @func
 *
 * Enumerates segments of the module.
 */
void
gum_elf_module_enumerate_segments (GumElfModule * self,
                                   GumFoundElfSegmentFunc func,
                                   gpointer user_data)
{
  guint i;

  for (i = 0; i != self->phdrs->len; i++)
  {
    const GumElfPhdr * h = &g_array_index (self->phdrs, GumElfPhdr, i);
    GumElfSegmentDetails d;

    if (h->type != GUM_ELF_PHDR_LOAD)
      continue;

    d.vm_address = h->vaddr;
    d.vm_size = h->memsz;
    d.file_offset = h->offset;
    d.file_size = h->filesz;
    d.protection = gum_parse_phdr_protection (h);

    if (!func (&d, user_data))
      return;
  }
}

/**
 * gum_elf_module_enumerate_sections:
 * @self: module
 * @func: (scope call): function called with each section
 * @user_data: data to pass to @func
 *
 * Enumerates sections of the module.
 */
void
gum_elf_module_enumerate_sections (GumElfModule * self,
                                   GumFoundElfSectionFunc func,
                                   gpointer user_data)
{
  guint i;

  for (i = 0; i != self->shdrs->len; i++)
  {
    const GumElfSectionDetails * d = g_ptr_array_index (self->sections, i);

    if (!func (d, user_data))
      return;
  }
}

/**
 * gum_elf_module_enumerate_relocations:
 * @self: module
 * @func: (scope call): function called with each relocation
 * @user_data: data to pass to @func
 *
 * Enumerates relocations of the module.
 */
void
gum_elf_module_enumerate_relocations (GumElfModule * self,
                                      GumFoundElfRelocationFunc func,
                                      gpointer user_data)
{
  gconstpointer data;
  gsize size;
  guint i;
  GumElfRelocationGroup g = { 0, };

  data = gum_elf_module_get_file_data (self, &size);

  for (i = 0; i != self->shdrs->len; i++)
  {
    const GumElfShdr * shdr = &g_array_index (self->shdrs, GumElfShdr, i);

    switch (shdr->type)
    {
      case GUM_ELF_SECTION_REL:
      case GUM_ELF_SECTION_RELA:
      {
        const GumElfShdr * symtab_shdr;

        memset (&g, 0, sizeof (g));

        g.offset = shdr->offset;
        g.size = shdr->size;
        g.entsize = shdr->entsize;
        g.relocs_have_addend = shdr->type == GUM_ELF_SECTION_RELA;

        symtab_shdr =
            gum_elf_module_find_section_header_by_index (self, shdr->link);
        if (symtab_shdr != NULL)
        {
          const GumElfShdr * strings_shdr;

          g.symtab_offset = symtab_shdr->offset;
          g.symtab_entsize = symtab_shdr->entsize;

          strings_shdr = gum_elf_module_find_section_header_by_index (self,
              symtab_shdr->link);
          if (strings_shdr != NULL)
          {
            g.strings = (const gchar *) data + strings_shdr->offset;
            g.strings_base = data;
            g.strings_size = size;
          }
        }

        g.parent = g_ptr_array_index (self->sections, i);

        if (!gum_elf_module_emit_relocations (self, &g, func, user_data))
          return;

        break;
      }
      default:
        break;
    }
  }

  if (g.offset != 0)
    return;

  for (i = 0; i != self->dyns->len; i++)
  {
    const GumElfDyn * dyn = &g_array_index (self->dyns, GumElfDyn, i);

    switch (dyn->tag)
    {
      case GUM_ELF_DYNAMIC_REL:
      case GUM_ELF_DYNAMIC_RELA:
        g.offset = dyn->val;
        g.relocs_have_addend = dyn->tag == GUM_ELF_DYNAMIC_RELA;
        break;
      case GUM_ELF_DYNAMIC_RELSZ:
      case GUM_ELF_DYNAMIC_RELASZ:
        g.size = dyn->val;
        break;
      case GUM_ELF_DYNAMIC_RELENT:
      case GUM_ELF_DYNAMIC_RELAENT:
        g.entsize = dyn->val;
        break;
      case GUM_ELF_DYNAMIC_SYMTAB:
        g.symtab_offset = dyn->val;
        break;
      case GUM_ELF_DYNAMIC_SYMENT:
        g.symtab_entsize = dyn->val;
        break;
      default:
        break;
    }
  }

  g.strings = self->dynamic_strings;
  g.strings_base = gum_elf_module_get_live_data (self, &g.strings_size);

  gum_elf_module_emit_relocations (self, &g, func, user_data);
}

static gboolean
gum_elf_module_emit_relocations (GumElfModule * self,
                                 const GumElfRelocationGroup * g,
                                 GumFoundElfRelocationFunc func,
                                 gpointer user_data)
{
  guint64 minimum_entsize;
  gconstpointer data;
  gsize size;
  guint n, i;
  gconstpointer start, end, cursor;
  GError ** error = NULL;

  if (g->offset == 0 || g->size == 0 || g->entsize == 0)
    goto invalid_group;
  if (g->symtab_offset == 0 || g->symtab_entsize == 0)
    goto invalid_group;
  if (g->strings == NULL)
    goto invalid_group;

  switch (self->ehdr.identity.klass)
  {
    case GUM_ELF_CLASS_64:
      minimum_entsize = g->relocs_have_addend ? 24 : 16;
      break;
    case GUM_ELF_CLASS_32:
      minimum_entsize = g->relocs_have_addend ? 12 : 8;
      break;
    default:
      g_assert_not_reached ();
  }
  if (g->entsize < minimum_entsize)
    goto invalid_group;

  data = gum_elf_module_get_file_data (self, &size);

  n = g->size / g->entsize;

  start = (const guint8 *) data + g->offset;
  end = (const guint8 *) start + g->size;
  GUM_CHECK_BOUNDS (start, end, "relocations");

  cursor = start;
  for (i = 0; i != n; i++)
  {
    GumElfRelocationDetails d;
    guint32 sym_index;
    GumElfSymbolDetails sym_details;

    d.addend = 0;

    switch (self->ehdr.identity.klass)
    {
      case GUM_ELF_CLASS_64:
      {
        guint64 info;

        d.address = gum_elf_module_read_uint64 (self, cursor);

        info = gum_elf_module_read_uint64 (self,
            (const guint64 *) ((const guint8 *) cursor + 8));
        d.type = info & GUM_INT32_MASK;
        sym_index = info >> 32;

        if (g->relocs_have_addend)
        {
          d.addend = gum_elf_module_read_int64 (self,
              (const gint64 *) ((const guint8 *) cursor + 16));
        }

        break;
      }
      case GUM_ELF_CLASS_32:
      {
        guint32 info;

        d.address = gum_elf_module_read_uint32 (self, cursor);

        info = gum_elf_module_read_uint32 (self,
            (const guint32 *) ((const guint8 *) cursor + 4));
        d.type = info & GUM_INT8_MASK;
        sym_index = info >> 8;

        if (g->relocs_have_addend)
        {
          d.addend = gum_elf_module_read_int32 (self,
              (const gint32 *) ((const guint8 *) cursor + 8));
        }

        break;
      }
      default:
        g_assert_not_reached ();
    }

    d.address = gum_elf_module_translate_to_online (self, d.address);

    if (sym_index != GUM_STN_UNDEF)
    {
      gconstpointer sym_start, sym_end;
      GumElfSym sym_val;

      sym_start = (const guint8 *) data + g->symtab_offset +
          (sym_index * g->symtab_entsize);
      sym_end = (const guint8 *) sym_start + g->symtab_entsize;
      GUM_CHECK_BOUNDS (sym_start, sym_end, "relocation symbol");

      gum_elf_module_read_symbol (self, sym_start, &sym_val);

      gum_elf_module_parse_symbol (self, &sym_val, g->strings, &sym_details);
      if (sym_details.name != NULL)
      {
        if (sym_details.type != GUM_ELF_SYMBOL_SECTION &&
            !gum_elf_module_check_str_bounds (self, sym_details.name,
              g->strings_base, g->strings_size, "relocation symbol name", NULL))
        {
          goto invalid_group;
        }
      }
      else
      {
        sym_details.name = "";
      }

      d.symbol = &sym_details;
    }
    else
    {
      d.symbol = NULL;
    }

    d.parent = g->parent;

    if (!func (&d, user_data))
      return FALSE;

    cursor = (const guint8 *) cursor + g->entsize;
  }

  return TRUE;

invalid_group:
propagate_error:
  return TRUE;
}

/**
 * gum_elf_module_enumerate_dynamic_entries:
 * @self: module
 * @func: (scope call): function called with each dynamic entry
 * @user_data: data to pass to @func
 *
 * Enumerates dynamic entries of the module.
 */
void
gum_elf_module_enumerate_dynamic_entries (GumElfModule * self,
                                          GumFoundElfDynamicEntryFunc func,
                                          gpointer user_data)
{
  guint i;

  for (i = 0; i != self->dyns->len; i++)
  {
    const GumElfDyn * dyn = &g_array_index (self->dyns, GumElfDyn, i);
    GumElfDynamicEntryDetails d;

    d.tag = dyn->tag;
    d.val = dyn->val;

    if (!func (&d, user_data))
      return;
  }
}

/**
 * gum_elf_module_enumerate_imports:
 * @self: module
 * @func: (scope call): function called with each import
 * @user_data: data to pass to @func
 *
 * Enumerates imports of the module.
 */
void
gum_elf_module_enumerate_imports (GumElfModule * self,
                                  GumFoundImportFunc func,
                                  gpointer user_data)
{
  GumElfEnumerateImportsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.slots = g_hash_table_new (g_str_hash, g_str_equal);
  if (gum_try_get_jump_slot_relocation_type_for_machine (self->ehdr.machine,
        &ctx.jump_slot_type))
  {
    gum_elf_module_enumerate_relocations (self,
        gum_maybe_collect_import_slot_from_relocation, &ctx);
  }

  gum_elf_module_enumerate_dynamic_symbols (self, gum_emit_elf_import, &ctx);

  g_hash_table_unref (ctx.slots);
}

static gboolean
gum_emit_elf_import (const GumElfSymbolDetails * details,
                     gpointer user_data)
{
  GumElfEnumerateImportsContext * ctx = user_data;

  if (details->shdr_index == GUM_ELF_SHDR_INDEX_UNDEF &&
      (details->type == GUM_ELF_SYMBOL_FUNC ||
       details->type == GUM_ELF_SYMBOL_OBJECT))
  {
    GumImportDetails d;

    d.type = (details->type == GUM_ELF_SYMBOL_FUNC)
        ? GUM_IMPORT_FUNCTION
        : GUM_IMPORT_VARIABLE;
    d.name = details->name;
    d.module = NULL;
    d.address = 0;
    d.slot = GUM_ADDRESS (g_hash_table_lookup (ctx->slots, details->name));

    if (!ctx->func (&d, ctx->user_data))
      return FALSE;
  }

  return TRUE;
}

static gboolean
gum_try_get_jump_slot_relocation_type_for_machine (GumElfMachine machine,
                                                   guint32 * type)
{
  switch (machine)
  {
    case GUM_ELF_MACHINE_386:
      *type = GUM_ELF_IA32_JMP_SLOT;
      break;
    case GUM_ELF_MACHINE_X86_64:
      *type = GUM_ELF_X64_JUMP_SLOT;
      break;
    case GUM_ELF_MACHINE_ARM:
      *type = GUM_ELF_ARM_JUMP_SLOT;
      break;
    case GUM_ELF_MACHINE_AARCH64:
      *type = GUM_ELF_ARM64_JUMP_SLOT;
      break;
    case GUM_ELF_MACHINE_MIPS:
    case GUM_ELF_MACHINE_MIPS_RS3_LE:
    case GUM_ELF_MACHINE_MIPS_X:
      *type = GUM_ELF_MIPS_JUMP_SLOT;
      break;
    default:
      *type = G_MAXUINT32;
      return FALSE;
  }

  return TRUE;
}

static gboolean
gum_maybe_collect_import_slot_from_relocation (
    const GumElfRelocationDetails * details,
    gpointer user_data)
{
  GumElfEnumerateImportsContext * ctx = user_data;

  if (details->type == ctx->jump_slot_type && details->symbol != NULL)
  {
    g_hash_table_insert (ctx->slots, (gpointer) details->symbol->name,
        GSIZE_TO_POINTER (details->address));
  }

  return TRUE;
}

/**
 * gum_elf_module_enumerate_exports:
 * @self: module
 * @func: (scope call): function called with each export
 * @user_data: data to pass to @func
 *
 * Enumerates exports of the module.
 */
void
gum_elf_module_enumerate_exports (GumElfModule * self,
                                  GumFoundExportFunc func,
                                  gpointer user_data)
{
  GumElfEnumerateExportsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  gum_elf_module_enumerate_dynamic_symbols (self, gum_emit_elf_export, &ctx);
}

static gboolean
gum_emit_elf_export (const GumElfSymbolDetails * details,
                     gpointer user_data)
{
  GumElfEnumerateExportsContext * ctx = user_data;

  if (details->shdr_index != GUM_ELF_SHDR_INDEX_UNDEF &&
      (details->type == GUM_ELF_SYMBOL_FUNC ||
       details->type == GUM_ELF_SYMBOL_OBJECT) &&
      (details->bind == GUM_ELF_BIND_GLOBAL ||
       details->bind == GUM_ELF_BIND_WEAK))
  {
    GumExportDetails d;

    d.type = (details->type == GUM_ELF_SYMBOL_FUNC)
        ? GUM_EXPORT_FUNCTION
        : GUM_EXPORT_VARIABLE;
    d.name = details->name;
    d.address = details->address;
    d.size = details->size;

    if (!ctx->func (&d, ctx->user_data))
      return FALSE;
  }

  return TRUE;
}

/**
 * gum_elf_module_enumerate_dynamic_symbols:
 * @self: module
 * @func: (scope call): function called with each dynamic symbol
 * @user_data: data to pass to @func
 *
 * Enumerates dynamic symbols of the module.
 */
void
gum_elf_module_enumerate_dynamic_symbols (GumElfModule * self,
                                          GumFoundElfSymbolFunc func,
                                          gpointer user_data)
{
  GumElfStoreSymtabParamsContext ctx;
  gsize i;
  gconstpointer data;
  gsize size;
  GError ** error = NULL;

  ctx.pending = 3;
  ctx.found_hash = FALSE;

  ctx.entries = NULL;
  ctx.entry_size = 0;
  ctx.entry_count = 0;

  ctx.module = self;

  gum_elf_module_enumerate_dynamic_entries (self, gum_store_symtab_params,
      &ctx);
  if (ctx.pending != 0 || ctx.entry_count == 0)
    return;

  gum_elf_module_enumerate_sections (self, gum_adjust_symtab_params, &ctx);

  data = gum_elf_module_get_live_data (self, &size);

  for (i = 1; i != ctx.entry_count; i++)
  {
    gconstpointer entry = (const guint8 *) ctx.entries + (i * ctx.entry_size);
    GumElfSym sym;
    GumElfSymbolDetails details;

    gum_elf_module_read_symbol (self, entry, &sym);

    gum_elf_module_parse_symbol (self, &sym, self->dynamic_strings, &details);
    if (details.name != NULL)
    {
      if (details.type != GUM_ELF_SYMBOL_SECTION)
        GUM_CHECK_STR_BOUNDS (details.name, "symbol name");
    }
    else
      details.name = "";

    if (!func (&details, user_data))
      return;
  }

propagate_error:
  return;
}

static void
gum_elf_module_parse_symbol (GumElfModule * self,
                             const GumElfSym * sym,
                             const gchar * strings,
                             GumElfSymbolDetails * d)
{
  GumElfSymbolType type = GUM_ELF_ST_TYPE (sym->info);
  GumElfSectionDetails * section;

  section = gum_elf_module_find_section_details_by_index (self, sym->shndx);

  if (type == GUM_ELF_SYMBOL_SECTION)
  {
    d->name = (section != NULL) ? section->name : NULL;
    d->address = self->base_address + sym->value;
  }
  else
  {
    d->name = strings + sym->name;
    d->address = (sym->value != 0)
        ? gum_elf_module_translate_to_online (self, sym->value)
        : 0;
  }

  d->size = sym->size;
  d->type = type;
  d->bind = GUM_ELF_ST_BIND (sym->info);
  d->shdr_index = sym->shndx;
  d->section = section;
}

static void
gum_elf_module_read_symbol (GumElfModule * self,
                            gconstpointer raw_sym,
                            GumElfSym * sym)
{
#define GUM_READ_SYM_FIELD(name, type) \
    GUM_READ (sym->name, src->name, type)
#define GUM_READ_SYM() \
    G_STMT_START \
    { \
      GUM_READ_SYM_FIELD (name,  uint32); \
      GUM_READ_SYM_FIELD (info,  uint8); \
      GUM_READ_SYM_FIELD (other, uint8); \
      GUM_READ_SYM_FIELD (shndx, uint16); \
      GUM_READ_SYM_FIELD (value, uint64); \
      GUM_READ_SYM_FIELD (size,  uint64); \
    } \
    G_STMT_END
#define GUM_READ_SYM32() \
    G_STMT_START \
    { \
      GUM_READ_SYM_FIELD (name,  uint32); \
      GUM_READ_SYM_FIELD (value, uint32); \
      GUM_READ_SYM_FIELD (size,  uint32); \
      GUM_READ_SYM_FIELD (info,  uint8); \
      GUM_READ_SYM_FIELD (other, uint8); \
      GUM_READ_SYM_FIELD (shndx, uint16); \
    } \
    G_STMT_END

  switch (self->ehdr.identity.klass)
  {
    case GUM_ELF_CLASS_64:
    {
      const GumElfSym * src = raw_sym;
      GUM_READ_SYM ();
      break;
    }
    case GUM_ELF_CLASS_32:
    {
      const GumElfSym32 * src = raw_sym;
      GUM_READ_SYM32 ();
      break;
    }
    default:
      g_assert_not_reached ();
  }

#undef GUM_READ_SYM_FIELD
#undef GUM_READ_SYM
#undef GUM_READ_SYM32
}

static gboolean
gum_store_symtab_params (const GumElfDynamicEntryDetails * details,
                         gpointer user_data)
{
  GumElfStoreSymtabParamsContext * ctx = user_data;
  GumElfModule * self = ctx->module;
  gconstpointer data;
  gsize size;
  GError ** error = NULL;

  data = gum_elf_module_get_live_data (self, &size);

  switch (details->tag)
  {
    case GUM_ELF_DYNAMIC_SYMTAB:
      ctx->entries = gum_elf_module_resolve_dynamic_virtual_location (
          ctx->module, details->val);
      ctx->pending--;
      break;
    case GUM_ELF_DYNAMIC_SYMENT:
      ctx->entry_size = details->val;
      ctx->pending--;
      break;
    case GUM_ELF_DYNAMIC_HASH:
    {
      const guint32 * hash_params;
      guint32 nchain;

      if (ctx->found_hash)
        break;

      hash_params = gum_elf_module_resolve_dynamic_virtual_location (
          ctx->module, details->val);
      if (hash_params == NULL)
        break;

      GUM_CHECK_BOUNDS (hash_params,
          (const guint8 *) hash_params + (2 * sizeof (guint32)),
          "SYSV hash header");

      nchain = hash_params[1];

      ctx->found_hash = TRUE;
      ctx->entry_count = nchain;
      ctx->pending--;

      break;
    }
    case GUM_ELF_DYNAMIC_GNU_HASH:
    {
      const guint32 * hash_params;
      guint32 nbuckets;
      guint32 symoffset;
      guint32 bloom_size;
      guint word_size;
      const guint8 * bloom;
      guint remaining;
      const guint32 * buckets;
      const guint32 * chain;
      guint32 highest_index, bucket_index;
      gsize bloom_bytes, buckets_bytes;
      gsize max_chain_entries;

      if (ctx->found_hash)
        break;

      hash_params = gum_elf_module_resolve_dynamic_virtual_location (
          ctx->module, details->val);
      if (hash_params == NULL)
        break;

      GUM_CHECK_BOUNDS (hash_params,
          (const guint8 *) hash_params + (4 * sizeof (guint32)),
          "GNU hash header");

      nbuckets = hash_params[0];
      symoffset = hash_params[1];
      bloom_size = hash_params[2];

      word_size = gum_elf_module_get_pointer_size (self);

      bloom = (const guint8 *) (hash_params + 4);

      remaining = (const guint8 *) data + size - bloom;
      if ((gsize) bloom_size > (gsize) remaining / word_size)
        goto propagate_error;
      bloom_bytes = (gsize) bloom_size * word_size;
      GUM_CHECK_BOUNDS (bloom, bloom + bloom_bytes, "GNU hash bloom");

      buckets = (const guint32 *) (bloom + bloom_bytes);

      remaining = (const guint8 *) data + size - (const guint8 *) buckets;
      if ((gsize) nbuckets > (gsize) remaining / sizeof (guint32))
        goto propagate_error;
      buckets_bytes = (gsize) nbuckets * sizeof (guint32);
      GUM_CHECK_BOUNDS (buckets,
          (const guint8 *) buckets + buckets_bytes,
          "GNU hash buckets");

      chain = buckets + nbuckets;
      GUM_CHECK_BOUNDS (chain, (const guint8 *) chain + 1, "GNU hash chain");

      ctx->found_hash = TRUE;

      highest_index = 0;
      for (bucket_index = 0; bucket_index != nbuckets; bucket_index++)
        highest_index = MAX (buckets[bucket_index], highest_index);

      if (highest_index >= symoffset)
      {
        max_chain_entries = ((const guint8 *) data + size -
            (const guint8 *) chain) / sizeof (guint32);

        while (TRUE)
        {
          guint32 chain_index = highest_index - symoffset;
          guint32 hash;

          if ((gsize) chain_index >= max_chain_entries)
            goto propagate_error;

          hash = chain[chain_index];

          if ((hash & 1) != 0)
            break;

          highest_index++;
        }
      }

      ctx->entry_count = highest_index + 1;
      ctx->pending--;

      break;
    }
    default:
      break;
  }

  return ctx->pending != 0;

propagate_error:
  {
    ctx->found_hash = FALSE;
    return TRUE;
  }
}

static gboolean
gum_adjust_symtab_params (const GumElfSectionDetails * details,
                          gpointer user_data)
{
  GumElfStoreSymtabParamsContext * ctx = user_data;

  if (details->address == GUM_ADDRESS (ctx->entries))
  {
    ctx->entry_count = details->size / ctx->entry_size;
    return FALSE;
  }

  return TRUE;
}

/**
 * gum_elf_module_enumerate_symbols:
 * @self: module
 * @func: (scope call): function called with each symbol
 * @user_data: data to pass to @func
 *
 * Enumerates symbols of the module.
 */
void
gum_elf_module_enumerate_symbols (GumElfModule * self,
                                  GumFoundElfSymbolFunc func,
                                  gpointer user_data)
{
  GumElfEnumerateSymbolsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;
  ctx.count = 0;

  gum_elf_module_enumerate_symbols_in_section (self, GUM_ELF_SECTION_SYMTAB,
      gum_count_and_emit_symbol, &ctx);

  if (ctx.count == 0)
    gum_elf_module_enumerate_dynamic_symbols (self, func, user_data);
}

static gboolean
gum_count_and_emit_symbol (const GumElfSymbolDetails * details,
                           gpointer user_data)
{
  GumElfEnumerateSymbolsContext * ctx = user_data;

  ctx->count++;

  return ctx->func (details, ctx->user_data);
}

static void
gum_elf_module_enumerate_symbols_in_section (GumElfModule * self,
                                             GumElfSectionType section,
                                             GumFoundElfSymbolFunc func,
                                             gpointer user_data)
{
  const GumElfShdr * shdr, * strings_shdr;
  gconstpointer data;
  gsize size;
  guint64 n, i;
  gconstpointer start, end;
  const gchar * strings;
  gconstpointer cursor;
  GError ** error = NULL;

  shdr = gum_elf_module_find_section_header_by_type (self, section);
  if (shdr == NULL)
    goto consider_fallback;

  strings_shdr =
      gum_elf_module_find_section_header_by_index (self, shdr->link);
  if (strings_shdr == NULL)
    return;

  data = gum_elf_module_get_file_data (self, &size);

  n = shdr->size / shdr->entsize;

  start = (const guint8 *) data + shdr->offset;
  end = (const guint8 *) start + (n * shdr->entsize);
  GUM_CHECK_BOUNDS (start, end, "symbols");

  strings = (const gchar *) data + strings_shdr->offset;

  cursor = start;
  for (i = 0; i != n; i++)
  {
    GumElfSym sym;
    GumElfSymbolDetails details;

    gum_elf_module_read_symbol (self, cursor, &sym);

    gum_elf_module_parse_symbol (self, &sym, strings, &details);
    if (details.name != NULL)
    {
      if (details.type != GUM_ELF_SYMBOL_SECTION)
        GUM_CHECK_STR_BOUNDS (details.name, "symbol name");
    }
    else
    {
      details.name = "";
    }

    if (!func (&details, user_data))
      return;

    cursor = (const guint8 *) cursor + shdr->entsize;
  }

propagate_error:
  return;

consider_fallback:
  {
    GumElfModule * fallback = gum_elf_module_try_get_fallback_elf_module (self);
    if (fallback != NULL)
    {
      gum_elf_module_enumerate_symbols_in_section (fallback, section, func,
          user_data);
    }
  }
}

/**
 * gum_elf_module_enumerate_dependencies:
 * @self: module
 * @func: (scope call): function called with each dependency
 * @user_data: data to pass to @func
 *
 * Enumerates dependencies of the module.
 */
void
gum_elf_module_enumerate_dependencies (GumElfModule * self,
                                       GumFoundDependencyFunc func,
                                       gpointer user_data)
{
  GumElfEnumerateDepsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.module = self;

  gum_elf_module_enumerate_dynamic_entries (self, gum_emit_each_needed, &ctx);
}

static gboolean
gum_emit_each_needed (const GumElfDynamicEntryDetails * details,
                      gpointer user_data)
{
  GumElfEnumerateDepsContext * ctx = user_data;
  gconstpointer data;
  gsize size;
  GumDependencyDetails d;

  if (details->tag != GUM_ELF_DYNAMIC_NEEDED)
    return TRUE;

  data = gum_elf_module_get_live_data (ctx->module, &size);

  d.name = ctx->module->dynamic_strings + details->val;
  if (!gum_elf_module_check_str_bounds (ctx->module, d.name, data, size,
        "dependencies", NULL))
  {
    return TRUE;
  }
  d.type = GUM_DEPENDENCY_REGULAR;

  return ctx->func (&d, ctx->user_data);
}

static gboolean
gum_elf_module_find_address_file_offset (GumElfModule * self,
                                         GumAddress address,
                                         guint64 * offset)
{
  const GumElfPhdr * phdr;
  gsize delta;

  phdr = gum_elf_module_find_load_phdr_by_address (self, address);
  if (phdr == NULL)
    return FALSE;

  delta = address - phdr->vaddr;
  if (delta >= phdr->filesz)
    return FALSE;

  *offset = phdr->offset + delta;

  if (*offset >= self->file_size)
    return FALSE;

  return TRUE;
}

static gboolean
gum_elf_module_find_address_protection (GumElfModule * self,
                                        GumAddress address,
                                        GumPageProtection * prot)
{
  const GumElfPhdr * phdr;

  phdr = gum_elf_module_find_load_phdr_by_address (self, address);
  if (phdr == NULL)
    return FALSE;

  *prot = gum_parse_phdr_protection (phdr);

  return TRUE;
}

static GumPageProtection
gum_parse_phdr_protection (const GumElfPhdr * phdr)
{
  GumPageProtection p;

  p = GUM_PAGE_NO_ACCESS;
  if ((phdr->flags & GUM_ELF_PHDR_R) != 0)
    p |= GUM_PAGE_READ;
  if ((phdr->flags & GUM_ELF_PHDR_W) != 0)
    p |= GUM_PAGE_WRITE;
  if ((phdr->flags & GUM_ELF_PHDR_X) != 0)
    p |= GUM_PAGE_EXECUTE;

  return p;
}

static const GumElfPhdr *
gum_elf_module_find_phdr_by_type (GumElfModule * self,
                                  guint32 type)
{
  guint i;

  for (i = 0; i != self->phdrs->len; i++)
  {
    const GumElfPhdr * h = &g_array_index (self->phdrs, GumElfPhdr, i);

    if (h->type == type)
      return h;
  }

  return NULL;
}

static const GumElfPhdr *
gum_elf_module_find_load_phdr_by_address (GumElfModule * self,
                                          GumAddress address)
{
  guint i;

  for (i = 0; i != self->phdrs->len; i++)
  {
    const GumElfPhdr * h = &g_array_index (self->phdrs, GumElfPhdr, i);

    if (h->type == GUM_ELF_PHDR_LOAD &&
        address >= h->vaddr &&
        address < h->vaddr + h->memsz)
    {
      return h;
    }
  }

  return NULL;
}

static const GumElfShdr *
gum_elf_module_find_section_header_by_index (GumElfModule * self,
                                             guint i)
{
  if (i == GUM_ELF_SHDR_INDEX_UNDEF)
    return NULL;

  if (i >= self->shdrs->len)
    return NULL;

  return &g_array_index (self->shdrs, GumElfShdr, i);
}

static const GumElfShdr *
gum_elf_module_find_section_header_by_type (GumElfModule * self,
                                            GumElfSectionType type)
{
  guint i;

  for (i = 0; i != self->shdrs->len; i++)
  {
    const GumElfShdr * shdr = &g_array_index (self->shdrs, GumElfShdr, i);

    if ((GumElfSectionType) shdr->type == type)
      return shdr;
  }

  return NULL;
}

static GumElfSectionDetails *
gum_elf_module_find_section_details_by_index (GumElfModule * self,
                                              guint i)
{
  if (i == GUM_ELF_SHDR_INDEX_UNDEF)
    return NULL;

  if (i >= self->sections->len)
    return NULL;

  return g_ptr_array_index (self->sections, i);
}

static GumAddress
gum_elf_module_compute_preferred_address (GumElfModule * self)
{
  guint i;

  for (i = 0; i != self->phdrs->len; i++)
  {
    const GumElfPhdr * phdr = &g_array_index (self->phdrs, GumElfPhdr, i);

    if (phdr->type == GUM_ELF_PHDR_LOAD && phdr->offset == 0)
      return phdr->vaddr;
  }

  return 0;
}

static guint64
gum_elf_module_compute_mapped_size (GumElfModule * self)
{
  guint64 lowest, highest, page_size;
  guint i;

  lowest = ~G_GUINT64_CONSTANT (0);
  highest = 0;

  page_size = gum_query_page_size ();

  for (i = 0; i != self->phdrs->len; i++)
  {
    const GumElfPhdr * phdr = &g_array_index (self->phdrs, GumElfPhdr, i);

    if (phdr->type == GUM_ELF_PHDR_LOAD)
    {
      lowest = MIN (GUM_ELF_PAGE_START (phdr->vaddr, page_size), lowest);
      highest = MAX (phdr->vaddr + phdr->memsz, highest);
    }
  }

  return highest - lowest;
}

static GumElfDynamicAddressState
gum_elf_module_detect_dynamic_address_state (GumElfModule * self)
{
  guint i;

  if (self->source_mode == GUM_ELF_SOURCE_MODE_OFFLINE)
    return GUM_ELF_DYNAMIC_ADDRESS_PRISTINE;

  for (i = 0; i != self->dyns->len; i++)
  {
    const GumElfDyn * dyn = &g_array_index (self->dyns, GumElfDyn, i);

    switch (dyn->tag)
    {
      case GUM_ELF_DYNAMIC_SYMTAB:
      case GUM_ELF_DYNAMIC_STRTAB:
        if (dyn->val > self->base_address)
          return GUM_ELF_DYNAMIC_ADDRESS_ADJUSTED;
        break;
    }
  }

  return GUM_ELF_DYNAMIC_ADDRESS_PRISTINE;
}

GumAddress
gum_elf_module_translate_to_offline (GumElfModule * self,
                                     GumAddress online_address)
{
  return self->preferred_address + (online_address - self->base_address);
}

GumAddress
gum_elf_module_translate_to_online (GumElfModule * self,
                                    GumAddress offline_address)
{
  return self->base_address + (offline_address - self->preferred_address);
}

static gpointer
gum_elf_module_resolve_dynamic_virtual_location (GumElfModule * self,
                                                 GumAddress address)
{
  if (self->source_mode == GUM_ELF_SOURCE_MODE_ONLINE)
  {
    switch (self->dynamic_address_state)
    {
      case GUM_ELF_DYNAMIC_ADDRESS_PRISTINE:
        return GSIZE_TO_POINTER (
            gum_elf_module_translate_to_online (self, address));
      case GUM_ELF_DYNAMIC_ADDRESS_ADJUSTED:
        return GSIZE_TO_POINTER (address);
      default:
        g_assert_not_reached ();
    }

    return NULL;
  }
  else
  {
    guint64 offset;

    if (!gum_elf_module_find_address_file_offset (self, address, &offset))
      return NULL;

    return (guint8 *) self->file_data + offset;
  }
}

static gboolean
gum_store_dynamic_string_table (const GumElfDynamicEntryDetails * details,
                                gpointer user_data)
{
  GumElfModule * self = user_data;

  if (details->tag != GUM_ELF_DYNAMIC_STRTAB)
    return TRUE;

  self->dynamic_strings = gum_elf_module_resolve_dynamic_virtual_location (self,
      details->val);
  return FALSE;
}

static gboolean
gum_elf_module_check_bounds (GumElfModule * self,
                             gconstpointer left,
                             gconstpointer right,
                             gconstpointer base,
                             gsize size,
                             const gchar * name,
                             GError ** error)
{
  const guint8 * l = left;
  const guint8 * r = right;

  if (r < l)
    goto oob;

  if (l < (const guint8 *) base)
    goto oob;

  if (r > (const guint8 *) base + size)
    goto oob;

  return TRUE;

oob:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Missing data while reading %s", name);
    return FALSE;
  }
}

static gboolean
gum_elf_module_check_str_bounds (GumElfModule * self,
                                 const gchar * str,
                                 gconstpointer base,
                                 gsize size,
                                 const gchar * name,
                                 GError ** error)
{
  const gchar * end, * cursor;

  if (str < (const gchar *) base)
    goto consider_file_data;

  end = (const gchar *) base + size;
  if (str >= end)
    goto consider_file_data;

  cursor = str;
  do
  {
    if (cursor >= end)
      goto oob;
  }
  while (*cursor++ != '\0');

  return TRUE;

consider_file_data:
  {
    if (self->source_mode == GUM_ELF_SOURCE_MODE_ONLINE &&
        GUM_ADDRESS (base) == self->base_address &&
        base != self->file_data)
    {
      return gum_elf_module_check_str_bounds (self, str, self->file_data,
          self->file_size, name, error);
    }

    goto oob;
  }
oob:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Missing data while reading %s", name);
    return FALSE;
  }
}

static guint8
gum_elf_module_read_uint8 (GumElfModule * self,
                           const guint8 * v)
{
  return *v;
}

static guint16
gum_elf_module_read_uint16 (GumElfModule * self,
                            const guint16 * v)
{
  guint16 aligned_val;

  memcpy (&aligned_val, v, sizeof (aligned_val));

  return (self->ehdr.identity.data_encoding == GUM_ELF_DATA_ENCODING_LSB)
      ? GUINT16_FROM_LE (aligned_val)
      : GUINT16_FROM_BE (aligned_val);
}

static gint32
gum_elf_module_read_int32 (GumElfModule * self,
                           const gint32 * v)
{
  gint32 aligned_val;

  memcpy (&aligned_val, v, sizeof (aligned_val));

  return (self->ehdr.identity.data_encoding == GUM_ELF_DATA_ENCODING_LSB)
      ? GINT32_FROM_LE (aligned_val)
      : GINT32_FROM_BE (aligned_val);
}

static guint32
gum_elf_module_read_uint32 (GumElfModule * self,
                            const guint32 * v)
{
  guint32 aligned_val;

  memcpy (&aligned_val, v, sizeof (aligned_val));

  return (self->ehdr.identity.data_encoding == GUM_ELF_DATA_ENCODING_LSB)
      ? GUINT32_FROM_LE (aligned_val)
      : GUINT32_FROM_BE (aligned_val);
}

static gint64
gum_elf_module_read_int64 (GumElfModule * self,
                           const gint64 * v)
{
  gint64 aligned_val;

  memcpy (&aligned_val, v, sizeof (aligned_val));

  return (self->ehdr.identity.data_encoding == GUM_ELF_DATA_ENCODING_LSB)
      ? GINT64_FROM_LE (aligned_val)
      : GINT64_FROM_BE (aligned_val);
}

static guint64
gum_elf_module_read_uint64 (GumElfModule * self,
                            const guint64 * v)
{
  guint64 aligned_val;

  memcpy (&aligned_val, v, sizeof (aligned_val));

  return (self->ehdr.identity.data_encoding == GUM_ELF_DATA_ENCODING_LSB)
      ? GUINT64_FROM_LE (aligned_val)
      : GUINT64_FROM_BE (aligned_val);
}

static GBytes *
gum_decompress_xz (gconstpointer data,
                   gsize size)
{
#ifdef HAVE_LZMA
  gpointer out_data;
  gsize out_capacity;
  size_t in_pos, out_pos;

  out_capacity = 4 * size;
  out_data = g_malloc (out_capacity);

  in_pos = 0;
  out_pos = 0;

  while (TRUE)
  {
    uint64_t memlimit = UINT64_MAX;
    uint32_t flags = 0;

    switch (lzma_stream_buffer_decode (&memlimit, flags, NULL, data, &in_pos,
          size, out_data, &out_pos, out_capacity))
    {
      case LZMA_OK:
        out_data = g_realloc (out_data, out_pos);
        return g_bytes_new_take (out_data, out_pos);
      case LZMA_BUF_ERROR:
        out_capacity *= 2;
        out_data = g_realloc (out_data, out_capacity);
        break;
      default:
        g_free (out_data);
        return NULL;
    }
  }
#else
  return NULL;
#endif
}

static gboolean
gum_maybe_extract_from_apk (const gchar * path,
                            GBytes ** file_bytes)
{
#if defined (HAVE_ANDROID) && defined (HAVE_MINIZIP)
  gboolean success = FALSE;
  gchar ** tokens;
  const gchar * apk_path, * file_path, * bare_file_path;
  void * zip_stream = NULL;
  void * zip_reader = NULL;
  gsize size;
  gpointer buffer = NULL;

  tokens = g_strsplit (path, "!", 2);
  if (g_strv_length (tokens) != 2 || !g_str_has_suffix (tokens[0], ".apk"))
    goto beach;
  apk_path = tokens[0];
  file_path = tokens[1];
  bare_file_path = file_path + 1;

  mz_stream_os_create (&zip_stream);
  if (mz_stream_os_open (zip_stream, apk_path, MZ_OPEN_MODE_READ) != MZ_OK)
    goto beach;

  mz_zip_reader_create (&zip_reader);
  if (mz_zip_reader_open (zip_reader, zip_stream) != MZ_OK)
    goto beach;

  if (mz_zip_reader_locate_entry (zip_reader, bare_file_path, TRUE) != MZ_OK)
    goto beach;

  size = mz_zip_reader_entry_save_buffer_length (zip_reader);
  buffer = g_malloc (size);
  if (mz_zip_reader_entry_save_buffer (zip_reader, buffer, size) != MZ_OK)
    goto beach;

  success = TRUE;

  *file_bytes = g_bytes_new_take (g_steal_pointer (&buffer), size);

beach:
  g_free (buffer);
  mz_zip_reader_delete (&zip_reader);
  mz_stream_os_delete (&zip_stream);
  g_strfreev (tokens);

  return success;
#else
  return FALSE;
#endif
}

GumElfSectionDetails *
gum_elf_section_details_ref (GumElfSectionDetails * details)
{
  g_atomic_int_inc (&details->ref_count);

  return details;
}

void
gum_elf_section_details_unref (GumElfSectionDetails * details)
{
  if (!g_atomic_int_dec_and_test (&details->ref_count))
    return;

  g_free ((gpointer) details->name);
  g_free ((gpointer) details->id);

  g_slice_free (GumElfSectionDetails, details);
}

GumElfSymbolDetails *
gum_elf_symbol_details_copy (const GumElfSymbolDetails * details)
{
  GumElfSymbolDetails * d;

  d = g_slice_dup (GumElfSymbolDetails, details);
  d->name = g_strdup (details->name);
  d->section = (details->section != NULL)
      ? gum_elf_section_details_ref (details->section)
      : NULL;

  return d;
}

void
gum_elf_symbol_details_free (GumElfSymbolDetails * details)
{
  if (details == NULL)
    return;

  if (details->section != NULL)
    gum_elf_section_details_unref (details->section);
  g_free ((gpointer) details->name);

  g_slice_free (GumElfSymbolDetails, details);
}
