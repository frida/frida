/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023-2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#include "gum-init.h"
#include "gumcloak.h"
#include "gumleb.h"
#include "gummoduleregistry.h"

#ifdef HAVE_WINDOWS
# include <windows.h>
#endif

#ifndef HAVE_WINDOWS
# define GUM_OS_LACKS_MODULE_LOOKUP_APIS 1
#endif

#if defined (G_OS_NONE)
/* No unwinder to consult. */
#elif defined (HAVE_WINDOWS)
/* Resolved inline through RtlLookupFunctionEntry(). */
#elif defined (HAVE_DARWIN)
# define GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND 1
#elif defined (HAVE_ARM)
# define GUM_FUNCTION_RANGE_USES_EXIDX 1
#elif defined (HAVE_LINUX) || defined (HAVE_FREEBSD) || defined (HAVE_QNX)
# define GUM_FUNCTION_RANGE_USES_DWARF 1
#endif

#ifdef GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND
# define GUM_UNWIND_SECOND_LEVEL_REGULAR 2
# define GUM_UNWIND_SECOND_LEVEL_COMPRESSED 3
# define GUM_UNWIND_COMPRESSED_ENTRY_FUNC_OFFSET(entry) ((entry) & 0x00ffffff)
#endif

#ifdef GUM_FUNCTION_RANGE_USES_DWARF
# define GUM_DW_EH_PE_udata2 0x02
# define GUM_DW_EH_PE_udata4 0x03
# define GUM_DW_EH_PE_udata8 0x04
# define GUM_DW_EH_PE_sdata2 0x0a
# define GUM_DW_EH_PE_sdata4 0x0b
# define GUM_DW_EH_PE_sdata8 0x0c
#endif

#ifdef GUM_FUNCTION_RANGE_USES_EXIDX
typedef gsize _Unwind_Ptr;
#endif

typedef struct _GumEmitThreadsContext GumEmitThreadsContext;
#ifdef GUM_OS_LACKS_MODULE_LOOKUP_APIS
typedef struct _GumFindModuleByNameContext GumFindModuleByNameContext;
typedef struct _GumFindModuleByAddressContext GumFindModuleByAddressContext;
#endif
#ifdef GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND
typedef struct _GumDyldUnwindSections GumDyldUnwindSections;
typedef struct _GumUnwindInfoHeader GumUnwindInfoHeader;
typedef struct _GumUnwindInfoIndexEntry GumUnwindInfoIndexEntry;
typedef struct _GumUnwindInfoRegularPage GumUnwindInfoRegularPage;
typedef struct _GumUnwindInfoRegularEntry GumUnwindInfoRegularEntry;
typedef struct _GumUnwindInfoCompressedPage GumUnwindInfoCompressedPage;
typedef int (* GumDyldFindUnwindSectionsFunc) (void * addr,
    GumDyldUnwindSections * info);
#endif
#ifdef GUM_FUNCTION_RANGE_USES_DWARF
typedef struct _GumDwarfEhBases GumDwarfEhBases;
#endif
typedef struct _GumFindFunctionRangeContext GumFindFunctionRangeContext;
typedef struct _GumEmitRangesContext GumEmitRangesContext;

struct _GumEmitThreadsContext
{
  GumFoundThreadFunc func;
  gpointer user_data;
};

#ifdef GUM_OS_LACKS_MODULE_LOOKUP_APIS
struct _GumFindModuleByNameContext
{
  const gchar * name;
  GumModule * module;
};

struct _GumFindModuleByAddressContext
{
  GumAddress address;
  GumModule * module;
};
#endif

#ifdef GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND
struct _GumDyldUnwindSections
{
  const void * mh;
  const void * dwarf_section;
  uintptr_t dwarf_section_length;
  const void * compact_unwind_section;
  uintptr_t compact_unwind_section_length;
};

struct _GumUnwindInfoHeader
{
  guint32 version;
  guint32 common_encodings_array_section_offset;
  guint32 common_encodings_array_count;
  guint32 personality_array_section_offset;
  guint32 personality_array_count;
  guint32 index_section_offset;
  guint32 index_count;
};

struct _GumUnwindInfoIndexEntry
{
  guint32 function_offset;
  guint32 second_level_pages_section_offset;
  guint32 lsda_index_array_section_offset;
};

struct _GumUnwindInfoRegularPage
{
  guint32 kind;
  guint16 entry_page_offset;
  guint16 entry_count;
};

struct _GumUnwindInfoRegularEntry
{
  guint32 function_offset;
  guint32 encoding;
};

struct _GumUnwindInfoCompressedPage
{
  guint32 kind;
  guint16 entry_page_offset;
  guint16 entry_count;
  guint16 encodings_page_offset;
  guint16 encodings_count;
};
#endif

#ifdef GUM_FUNCTION_RANGE_USES_DWARF
struct _GumDwarfEhBases
{
  gpointer tbase;
  gpointer dbase;
  gpointer func;
};
#endif

struct _GumFindFunctionRangeContext
{
  GumAddress address;
  GumMemoryRange * range;
  gboolean found;
};

struct _GumEmitRangesContext
{
  GumFoundRangeFunc func;
  gpointer user_data;
};

static gboolean gum_emit_thread_if_not_cloaked (
    const GumThreadDetails * details, gpointer user_data);
static void gum_deinit_main_module (void);
#ifdef GUM_OS_LACKS_MODULE_LOOKUP_APIS
static gboolean gum_try_resolve_module_by_name (GumModule * module,
    gpointer user_data);
static gboolean gum_try_resolve_module_by_path (GumModule * module,
    gpointer user_data);
static gboolean gum_try_resolve_module_by_address (GumModule * module,
    gpointer user_data);
#endif
static gboolean gum_find_function_range_from_unwind_info (
    gconstpointer address, GumMemoryRange * range);
static gboolean gum_find_function_range_from_symbol (gconstpointer address,
    GumMemoryRange * range);
static gboolean gum_store_symbol_range_if_containing (
    const GumSymbolDetails * details, gpointer user_data);
#ifdef GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND
static GumDyldFindUnwindSectionsFunc gum_get_dyld_find_unwind_sections (void);
#endif
#ifdef GUM_FUNCTION_RANGE_USES_DWARF
static gsize gum_read_fde_function_size (const guint8 * fde);
static guint8 gum_query_cie_fde_encoding (const guint8 * cie,
    const guint8 * end);
static guint gum_dwarf_encoded_size (guint8 encoding);
static gsize gum_read_dwarf_value (const guint8 * value, guint8 encoding);
#endif
#ifdef GUM_FUNCTION_RANGE_USES_EXIDX
static GumAddress gum_decode_prel31 (const guint32 * slot);
#endif
static gboolean gum_emit_range_if_not_cloaked (const GumRangeDetails * details,
    gpointer user_data);

static GumTeardownRequirement gum_teardown_requirement =
    GUM_TEARDOWN_REQUIREMENT_FULL;
static GumCodeSigningPolicy gum_code_signing_policy = GUM_CODE_SIGNING_OPTIONAL;

G_DEFINE_BOXED_TYPE (GumThreadDetails, gum_thread_details,
                     gum_thread_details_copy, gum_thread_details_free)

#ifdef GUM_FUNCTION_RANGE_USES_DWARF
extern const void * _Unwind_Find_FDE (const void * pc, GumDwarfEhBases * bases);
#endif
#ifdef GUM_FUNCTION_RANGE_USES_EXIDX
extern _Unwind_Ptr __gnu_Unwind_Find_exidx (_Unwind_Ptr pc, int * nrec);
#endif

GumOS
gum_process_get_native_os (void)
{
#if defined (G_OS_NONE)
  return GUM_OS_NONE;
#elif defined (HAVE_WINDOWS)
  return GUM_OS_WINDOWS;
#elif defined (HAVE_MACOS)
  return GUM_OS_MACOS;
#elif defined (HAVE_LINUX) && !defined (HAVE_ANDROID)
  return GUM_OS_LINUX;
#elif defined (HAVE_IOS)
  return GUM_OS_IOS;
#elif defined (HAVE_WATCHOS)
  return GUM_OS_WATCHOS;
#elif defined (HAVE_TVOS)
  return GUM_OS_TVOS;
#elif defined (HAVE_XROS)
  return GUM_OS_XROS;
#elif defined (HAVE_ANDROID)
  return GUM_OS_ANDROID;
#elif defined (HAVE_FREEBSD)
  return GUM_OS_FREEBSD;
#elif defined (HAVE_QNX)
  return GUM_OS_QNX;
#else
# error Unknown OS
#endif
}

GumTeardownRequirement
gum_process_get_teardown_requirement (void)
{
  return gum_teardown_requirement;
}

void
gum_process_set_teardown_requirement (GumTeardownRequirement requirement)
{
  gum_teardown_requirement = requirement;
}

GumCodeSigningPolicy
gum_process_get_code_signing_policy (void)
{
  return gum_code_signing_policy;
}

void
gum_process_set_code_signing_policy (GumCodeSigningPolicy policy)
{
  gum_code_signing_policy = policy;
}

/**
 * gum_process_modify_thread:
 * @thread_id: ID of thread to modify
 * @func: (scope call): function to apply the modifications
 * @user_data: data to pass to @func
 * @flags: flags to customize behavior
 *
 * Modifies a given thread by first pausing it, reading its state, and then
 * passing that to @func, followed by writing back the new state and then
 * resuming the thread. May also be used to inspect the current state without
 * modifying it.
 *
 * Returns: whether the modifications were successfully applied
 */

/**
 * gum_process_find_thread_by_id:
 * @thread_id: ID of the thread to find
 * @flags: flags specifying the desired level of detail
 *
 * Looks up a single thread by its ID. Unlike
 * [func@Gum.process_enumerate_threads], cloaked threads are not hidden: an
 * explicit lookup by ID always returns the thread if it exists.
 *
 * Returns: (nullable) (transfer full): the thread's #GumThreadDetails, or %NULL
 * if no such thread exists
 */

/**
 * gum_process_enumerate_threads:
 * @func: (scope call): function called with #GumThreadDetails
 * @user_data: data to pass to @func
 * @flags: flags specifying the desired level of detail
 *
 * Enumerates all threads, calling @func with #GumThreadDetails about each
 * thread found.
 */
void
gum_process_enumerate_threads (GumFoundThreadFunc func,
                               gpointer user_data,
                               GumThreadFlags flags)
{
  GumEmitThreadsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;
  _gum_process_enumerate_threads (gum_emit_thread_if_not_cloaked, &ctx, flags);
}

static gboolean
gum_emit_thread_if_not_cloaked (const GumThreadDetails * details,
                                gpointer user_data)
{
  GumEmitThreadsContext * ctx = user_data;

  if (gum_cloak_has_thread (details->id))
    return TRUE;

  return ctx->func (details, ctx->user_data);
}

/**
 * gum_process_get_main_module:
 *
 * Returns the module representing the main executable of the process.
 *
 * Returns: (transfer none): the main module
 */
GumModule *
gum_process_get_main_module (void)
{
  static gsize cached_result = 0;

  if (g_once_init_enter (&cached_result))
  {
    GumModule * result;

    gum_process_enumerate_modules (_gum_process_collect_main_module, &result);

    _gum_register_destructor (gum_deinit_main_module);

    g_once_init_leave (&cached_result, GPOINTER_TO_SIZE (result) + 1);
  }

  return GSIZE_TO_POINTER (cached_result - 1);
}

static void
gum_deinit_main_module (void)
{
  g_object_unref (gum_process_get_main_module ());
}

/**
 * gum_process_get_libc_module:
 *
 * Returns the module representing the C runtime library.
 *
 * Returns: (transfer none): the libc module
 */

/**
 * gum_process_find_module_by_name:
 * @name: name of a currently loaded module
 *
 * Finds a currently loaded module by name or filesystem path.
 *
 * Returns: (transfer full) (nullable): module matching @name, or %NULL if none
 *   was found
 */
#ifdef GUM_OS_LACKS_MODULE_LOOKUP_APIS
GumModule *
gum_process_find_module_by_name (const gchar * name)
{
  GumFindModuleByNameContext ctx = {
    .name = name,
    .module = NULL
  };

  if (g_path_is_absolute (name))
    gum_process_enumerate_modules (gum_try_resolve_module_by_path, &ctx);
  else
    gum_process_enumerate_modules (gum_try_resolve_module_by_name, &ctx);

  return ctx.module;
}

static gboolean
gum_try_resolve_module_by_name (GumModule * module,
                                gpointer user_data)
{
  GumFindModuleByNameContext * ctx = user_data;

  if (strcmp (gum_module_get_name (module), ctx->name) == 0)
  {
    ctx->module = g_object_ref (module);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gum_try_resolve_module_by_path (GumModule * module,
                                gpointer user_data)
{
  GumFindModuleByNameContext * ctx = user_data;

  if (strcmp (gum_module_get_path (module), ctx->name) == 0)
  {
    ctx->module = g_object_ref (module);
    return FALSE;
  }

  return TRUE;
}
#endif

/**
 * gum_process_find_module_by_address:
 * @address: memory address potentially belonging to a module
 *
 * Determines which module @address belongs to, if any. Note that #ModuleMap is
 * more efficient for repeated lookups.
 *
 * Returns: (transfer full) (nullable): module containing @address, or %NULL if
 *   none was found
 */
#ifdef GUM_OS_LACKS_MODULE_LOOKUP_APIS
GumModule *
gum_process_find_module_by_address (GumAddress address)
{
  GumFindModuleByAddressContext ctx = {
    .address = address,
    .module = NULL
  };

  gum_process_enumerate_modules (gum_try_resolve_module_by_address, &ctx);

  return ctx.module;
}

static gboolean
gum_try_resolve_module_by_address (GumModule * module,
                                   gpointer user_data)
{
  GumFindModuleByAddressContext * ctx = user_data;
  const GumMemoryRange * range;

  range = gum_module_get_range (module);

  if (GUM_MEMORY_RANGE_INCLUDES (range, ctx->address))
  {
    ctx->module = g_object_ref (module);
    return FALSE;
  }

  return TRUE;
}
#endif

/**
 * gum_process_find_function_range:
 * @address: an address belonging to the function
 * @range: (out): the function's contiguous code range covering @address
 *
 * Resolves the code range that @address belongs to, derived from the platform's
 * unwind tables. A function whose body is split across several ranges (e.g. a
 * cold .text.unlikely fragment) is represented by one range per fragment; this
 * returns the one covering @address. Where no unwind information is available
 * (e.g. a leaf function, or a target lacking unwind tables altogether), the
 * containing symbol's bounds are used as a best-effort fallback.
 *
 * Returns: %TRUE if a range was found
 */
gboolean
gum_process_find_function_range (gconstpointer address,
                                 GumMemoryRange * range)
{
  if (gum_find_function_range_from_unwind_info (address, range))
    return TRUE;

  return gum_find_function_range_from_symbol (address, range);
}

static gboolean
gum_find_function_range_from_unwind_info (gconstpointer address,
                                          GumMemoryRange * range)
{
#if defined (HAVE_WINDOWS) && GLIB_SIZEOF_VOID_P == 8
  PRUNTIME_FUNCTION function;
  DWORD64 image_base;

  function = RtlLookupFunctionEntry (GPOINTER_TO_SIZE (address), &image_base,
      NULL);
  if (function == NULL)
    return FALSE;

  range->base_address = image_base + function->BeginAddress;
# ifdef HAVE_ARM64
  {
    /*
     * On ARM64 the entry has no EndAddress; the function length (in 32-bit
     * words) lives either packed into UnwindData when its low two bits are
     * set, or in the .xdata record it points to otherwise.
     */
    DWORD unwind = function->UnwindData;
    guint32 length;

    if ((unwind & 0x3) != 0)
    {
      length = (unwind >> 2) & 0x7ff;
    }
    else
    {
      const DWORD * xdata = GSIZE_TO_POINTER (image_base + unwind);
      length = *xdata & 0x3ffff;
    }

    range->size = (gsize) length * sizeof (guint32);
  }
# else
  range->size = function->EndAddress - function->BeginAddress;
# endif

  return TRUE;
#elif defined (GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND)
  GumDyldFindUnwindSectionsFunc find_unwind_sections;
  GumDyldUnwindSections info;
  const guint8 * base;
  const GumUnwindInfoHeader * header;
  const GumUnwindInfoIndexEntry * index;
  guint32 target_offset, first_func, next_first_func, kind;
  guint i;
  const guint8 * page;
  gboolean found = FALSE;
  guint32 start = 0, end = 0;

  find_unwind_sections = gum_get_dyld_find_unwind_sections ();
  if (find_unwind_sections == NULL)
    return FALSE;

  if (!find_unwind_sections ((void *) address, &info))
    return FALSE;
  if (info.compact_unwind_section == NULL)
    return FALSE;

  base = info.compact_unwind_section;
  header = (const GumUnwindInfoHeader *) base;
  if (header->version != 1 || header->index_count < 2)
    return FALSE;

  index = (const GumUnwindInfoIndexEntry *)
      (base + header->index_section_offset);
  target_offset = (guint32) (GUM_ADDRESS (address) - GUM_ADDRESS (info.mh));

  for (i = 0; i + 1 != header->index_count; i++)
  {
    if (target_offset >= index[i].function_offset &&
        target_offset < index[i + 1].function_offset)
      break;
  }
  if (i + 1 == header->index_count ||
      index[i].second_level_pages_section_offset == 0)
    return FALSE;

  first_func = index[i].function_offset;
  next_first_func = index[i + 1].function_offset;
  page = base + index[i].second_level_pages_section_offset;
  kind = *(const guint32 *) page;

  if (kind == GUM_UNWIND_SECOND_LEVEL_COMPRESSED)
  {
    const GumUnwindInfoCompressedPage * ph =
        (const GumUnwindInfoCompressedPage *) page;
    const guint32 * entries = (const guint32 *) (page + ph->entry_page_offset);
    guint e;

    for (e = 0; e != ph->entry_count; e++)
    {
      guint32 func = first_func +
          GUM_UNWIND_COMPRESSED_ENTRY_FUNC_OFFSET (entries[e]);

      if (func > target_offset)
        break;

      start = func;
      end = next_first_func;
      if (e + 1 != ph->entry_count)
      {
        end = first_func +
            GUM_UNWIND_COMPRESSED_ENTRY_FUNC_OFFSET (entries[e + 1]);
      }
      found = TRUE;
    }
  }
  else if (kind == GUM_UNWIND_SECOND_LEVEL_REGULAR)
  {
    const GumUnwindInfoRegularPage * ph =
        (const GumUnwindInfoRegularPage *) page;
    const GumUnwindInfoRegularEntry * entries =
        (const GumUnwindInfoRegularEntry *) (page + ph->entry_page_offset);
    guint e;

    for (e = 0; e != ph->entry_count; e++)
    {
      if (entries[e].function_offset > target_offset)
        break;

      start = entries[e].function_offset;
      end = (e + 1 != ph->entry_count)
          ? entries[e + 1].function_offset
          : next_first_func;
      found = TRUE;
    }
  }

  if (!found)
    return FALSE;

  range->base_address = GUM_ADDRESS (info.mh) + start;
  range->size = end - start;

  return TRUE;
#elif defined (GUM_FUNCTION_RANGE_USES_DWARF)
  const guint8 * fde;
  GumDwarfEhBases bases;
  gsize size;

  fde = _Unwind_Find_FDE (address, &bases);
  if (fde == NULL)
    return FALSE;

  size = gum_read_fde_function_size (fde);
  if (size == 0)
    return FALSE;

  range->base_address = GUM_ADDRESS (bases.func);
  range->size = size;

  return TRUE;
#elif defined (GUM_FUNCTION_RANGE_USES_EXIDX)
  GumAddress pc = GUM_ADDRESS (address);
  const guint32 * entries;
  int count, i, match;

  entries = (const guint32 *) __gnu_Unwind_Find_exidx (
      (_Unwind_Ptr) GPOINTER_TO_SIZE (address), &count);
  if (entries == NULL || count <= 0)
    return FALSE;

  match = -1;
  for (i = 0; i != count; i++)
  {
    if (gum_decode_prel31 (&entries[2 * i]) <= pc)
      match = i;
    else
      break;
  }
  if (match == -1 || match == count - 1)
    return FALSE;

  range->base_address = gum_decode_prel31 (&entries[2 * match]);
  range->size = gum_decode_prel31 (&entries[2 * (match + 1)]) -
      range->base_address;

  return TRUE;
#else
  return FALSE;
#endif
}

static gboolean
gum_find_function_range_from_symbol (gconstpointer address,
                                     GumMemoryRange * range)
{
  GumFindFunctionRangeContext ctx;
  GumModule * module;

  module = gum_process_find_module_by_address (GUM_ADDRESS (address));
  if (module == NULL)
    return FALSE;

  ctx.address = GUM_ADDRESS (address);
  ctx.range = range;
  ctx.found = FALSE;

  gum_module_enumerate_symbols (module, gum_store_symbol_range_if_containing,
      &ctx);

  g_object_unref (module);

  return ctx.found;
}

static gboolean
gum_store_symbol_range_if_containing (const GumSymbolDetails * details,
                                      gpointer user_data)
{
  GumFindFunctionRangeContext * ctx = user_data;

  if (details->size <= 0)
    return TRUE;

  if (ctx->address < details->address ||
      ctx->address >= details->address + details->size)
    return TRUE;

  ctx->range->base_address = details->address;
  ctx->range->size = details->size;
  ctx->found = TRUE;

  return FALSE;
}

#ifdef GUM_FUNCTION_RANGE_USES_COMPACT_UNWIND

static GumDyldFindUnwindSectionsFunc
gum_get_dyld_find_unwind_sections (void)
{
  static gsize cached_func = 0;

  if (g_once_init_enter (&cached_func))
  {
    GumAddress func = gum_module_find_global_export_by_name (
        "_dyld_find_unwind_sections");

    g_once_init_leave (&cached_func, (func != 0) ? func : 1);
  }

  if (cached_func == 1)
    return NULL;

  return GUM_POINTER_TO_FUNCPTR (GumDyldFindUnwindSectionsFunc,
      GSIZE_TO_POINTER (cached_func));
}

#endif

#ifdef GUM_FUNCTION_RANGE_USES_DWARF

static gsize
gum_read_fde_function_size (const guint8 * fde)
{
  const guint8 * cursor = fde;
  guint32 length;
  const guint8 * cie;
  guint8 encoding;

  length = *(const guint32 *) cursor;
  cursor += sizeof (guint32);
  if (length == G_MAXUINT32)
    return 0;

  cie = cursor - *(const guint32 *) cursor;
  cursor += sizeof (guint32);

  encoding = gum_query_cie_fde_encoding (cie, fde);

  cursor += gum_dwarf_encoded_size (encoding);

  return gum_read_dwarf_value (cursor, encoding);
}

static guint8
gum_query_cie_fde_encoding (const guint8 * cie,
                            const guint8 * end)
{
  const guint8 * cursor = cie + sizeof (guint32) + sizeof (guint32);
  guint8 version = *cursor++;
  const gchar * augmentation = (const gchar *) cursor;

  cursor += strlen (augmentation) + 1;
  gum_skip_leb128 (&cursor, end);
  gum_skip_leb128 (&cursor, end);
  if (version == 1)
    cursor++;
  else
    gum_skip_leb128 (&cursor, end);

  if (augmentation[0] != 'z')
    return 0;

  gum_skip_leb128 (&cursor, end);

  for (augmentation++; *augmentation != '\0'; augmentation++)
  {
    switch (*augmentation)
    {
      case 'L':
        cursor++;
        break;
      case 'P':
        cursor += 1 + gum_dwarf_encoded_size (*cursor);
        break;
      case 'R':
        return *cursor;
    }
  }

  return 0;
}

static guint
gum_dwarf_encoded_size (guint8 encoding)
{
  switch (encoding & 0x0f)
  {
    case GUM_DW_EH_PE_udata2:
    case GUM_DW_EH_PE_sdata2:
      return 2;
    case GUM_DW_EH_PE_udata4:
    case GUM_DW_EH_PE_sdata4:
      return 4;
    case GUM_DW_EH_PE_udata8:
    case GUM_DW_EH_PE_sdata8:
      return 8;
    default:
      return sizeof (gpointer);
  }
}

static gsize
gum_read_dwarf_value (const guint8 * value,
                      guint8 encoding)
{
  switch (encoding & 0x0f)
  {
    case GUM_DW_EH_PE_udata2:
    case GUM_DW_EH_PE_sdata2:
      return *(const guint16 *) value;
    case GUM_DW_EH_PE_udata4:
    case GUM_DW_EH_PE_sdata4:
      return *(const guint32 *) value;
    case GUM_DW_EH_PE_udata8:
    case GUM_DW_EH_PE_sdata8:
      return *(const guint64 *) value;
    default:
      return *(const gsize *) value;
  }
}

#endif

#ifdef GUM_FUNCTION_RANGE_USES_EXIDX

static GumAddress
gum_decode_prel31 (const guint32 * slot)
{
  gint32 offset = (gint32) (*slot << 1) >> 1;

  return GUM_ADDRESS (slot) + offset;
}

#endif

/**
 * gum_process_enumerate_modules:
 * @func: (scope call): function called with #GumModule
 * @user_data: data to pass to @func
 *
 * Enumerates modules loaded right now, calling @func with each #GumModule
 * found.
 */
void
gum_process_enumerate_modules (GumFoundModuleFunc func,
                               gpointer user_data)
{
  gum_module_registry_enumerate_modules (gum_module_registry_obtain (), func,
      user_data);
}

/**
 * gum_process_enumerate_ranges:
 * @prot: bitfield specifying the minimum protection
 * @func: (scope call): function called with #GumRangeDetails
 * @user_data: data to pass to @func
 *
 * Enumerates memory ranges satisfying @prot, calling @func with
 * #GumRangeDetails about each such range found.
 */
void
gum_process_enumerate_ranges (GumPageProtection prot,
                              GumFoundRangeFunc func,
                              gpointer user_data)
{
  GumEmitRangesContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;
  _gum_process_enumerate_ranges (prot, gum_emit_range_if_not_cloaked, &ctx);
}

static gboolean
gum_emit_range_if_not_cloaked (const GumRangeDetails * details,
                               gpointer user_data)
{
  GumEmitRangesContext * ctx = user_data;
  GArray * sub_ranges;

  sub_ranges = gum_cloak_clip_range (details->range);
  if (sub_ranges != NULL)
  {
    gboolean carry_on = TRUE;
    GumRangeDetails sub_details;
    guint i;

    sub_details.protection = details->protection;
    sub_details.file = details->file;

    for (i = 0; i != sub_ranges->len && carry_on; i++)
    {
      sub_details.range = &g_array_index (sub_ranges, GumMemoryRange, i);

      carry_on = ctx->func (&sub_details, ctx->user_data);
    }

    g_array_free (sub_ranges, TRUE);

    return carry_on;
  }

  return ctx->func (details, ctx->user_data);
}

/**
 * gum_process_enumerate_malloc_ranges:
 * @func: (scope call): function called with #GumMallocRangeDetails
 * @user_data: data to pass to @func
 *
 * Enumerates individual memory allocations known to the system heap, calling
 * @func with #GumMallocRangeDetails about each range found.
 */

const gchar *
gum_code_signing_policy_to_string (GumCodeSigningPolicy policy)
{
  switch (policy)
  {
    case GUM_CODE_SIGNING_OPTIONAL: return "optional";
    case GUM_CODE_SIGNING_REQUIRED: return "required";
  }

  g_assert_not_reached ();
  return NULL;
}

GumThreadDetails *
gum_thread_details_copy (const GumThreadDetails * details)
{
  GumThreadDetails * d;

  d = g_slice_dup (GumThreadDetails, details);
  d->name = g_strdup (details->name);

  return d;
}

void
gum_thread_details_free (GumThreadDetails * details)
{
  if (details == NULL)
    return;

  g_free ((gpointer) details->name);
  g_slice_free (GumThreadDetails, details);
}
