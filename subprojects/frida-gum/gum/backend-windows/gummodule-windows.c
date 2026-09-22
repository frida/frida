/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2025 William Tan <1284324+Ninja3047@users.noreply.github.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodule-windows.h"

#include <psapi.h>

typedef struct _GumCollectCodePagesContext GumCollectCodePagesContext;
typedef struct _GumEnumerateSymbolsContext GumEnumerateSymbolsContext;
typedef struct _GumFindExportContext GumFindExportContext;

struct _GumNativeModule
{
  GObject parent;

  HMODULE handle;
  gchar * name;
  gchar * path;
  GumMemoryRange range;
};

struct _GumCollectCodePagesContext
{
  GumAddress base;
  gsize page_size;
  gsize num_pages;
  guint8 * bitmap;
};

struct _GumEnumerateSymbolsContext
{
  GumFoundSymbolFunc func;
  gpointer user_data;
};

struct _GumFindExportContext
{
  const gchar * symbol_name;
  GumAddress result;
};

static void gum_native_module_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_native_module_finalize (GObject * object);
static const gchar * gum_native_module_get_name (GumModule * module);
static const gchar * gum_native_module_get_path (GumModule * module);
static const GumMemoryRange * gum_native_module_get_range (GumModule * module);
static void gum_native_module_ensure_initialized (GumModule * module);
static void gum_native_module_enumerate_imports (GumModule * module,
    GumFoundImportFunc func, gpointer user_data);
static void gum_native_module_enumerate_exports (GumModule * module,
    GumFoundExportFunc func, gpointer user_data);
static gboolean gum_collect_code_pages (const GumRangeDetails * details,
    gpointer user_data);
static void gum_native_module_enumerate_symbols (GumModule * module,
    GumFoundSymbolFunc func, gpointer user_data);
static BOOL CALLBACK gum_emit_symbol (PSYMBOL_INFO info, ULONG symbol_size,
    PVOID user_context);
static void gum_native_module_enumerate_ranges (GumModule * module,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);
static void gum_native_module_enumerate_sections (GumModule * module,
    GumFoundSectionFunc func, gpointer user_data);
static void gum_native_module_enumerate_dependencies (GumModule * module,
    GumFoundDependencyFunc func, gpointer user_data);
static GumAddress gum_native_module_find_export_by_name (GumModule * module,
    const gchar * symbol_name);
static gboolean gum_store_address_if_module_has_export (GumModule * module,
    gpointer user_data);

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
}

static void
gum_native_module_finalize (GObject * object)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (object);

  g_free (self->path);

  G_OBJECT_CLASS (gum_native_module_parent_class)->finalize (object);
}

GumModule *
gum_module_load (const gchar * module_name,
                 GError ** error)
{
  gunichar2 * wide_name;
  HMODULE handle;

  wide_name = g_utf8_to_utf16 (module_name, -1, NULL, NULL, NULL);
  handle = LoadLibraryW ((LPCWSTR) wide_name);
  g_free (wide_name);

  if (handle == NULL)
    goto not_found;

  return GUM_MODULE (_gum_native_module_make (handle));

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
        "LoadLibrary failed: 0x%08lx", GetLastError ());
    return NULL;
  }
}

GumNativeModule *
_gum_native_module_make (HMODULE handle)
{
  GumNativeModule * module;
  WCHAR path_utf16[MAX_PATH];
  MODULEINFO mi;

  module = g_object_new (GUM_TYPE_NATIVE_MODULE, NULL);
  module->handle = handle;

  GetModuleFileNameW (handle, path_utf16, MAX_PATH);
  module->path = g_utf16_to_utf8 ((const gunichar2 *) path_utf16, -1, NULL,
      NULL, NULL);
  module->name = strrchr (module->path, '\\') + 1;

  GetModuleInformation (GetCurrentProcess (), handle, &mi, sizeof (mi));
  module->range.base_address = GUM_ADDRESS (mi.lpBaseOfDll);
  module->range.size = mi.SizeOfImage;

  return module;
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
}

static void
gum_native_module_enumerate_imports (GumModule * module,
                                     GumFoundImportFunc func,
                                     gpointer user_data)
{
  GumNativeModule * self;
  const guint8 * mod_base;
  const IMAGE_DOS_HEADER * dos_hdr;
  const IMAGE_NT_HEADERS * nt_hdrs;
  const IMAGE_DATA_DIRECTORY * entry;
  const IMAGE_IMPORT_DESCRIPTOR * desc;

  self = GUM_NATIVE_MODULE (module);

  mod_base = (const guint8 *) self->handle;
  dos_hdr = (const IMAGE_DOS_HEADER *) self->handle;
  nt_hdrs = (const IMAGE_NT_HEADERS *) &mod_base[dos_hdr->e_lfanew];
  entry = &nt_hdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  desc = (const IMAGE_IMPORT_DESCRIPTOR *) (mod_base + entry->VirtualAddress);

  for (; desc->Characteristics != 0; desc++)
  {
    GumImportDetails details;
    const IMAGE_THUNK_DATA * thunk_data;

    if (desc->OriginalFirstThunk == 0)
      continue;

    details.type = GUM_IMPORT_FUNCTION; /* FIXME: how can we tell? */
    details.name = NULL;
    details.module = (const gchar *) (mod_base + desc->Name);
    details.address = 0;
    details.slot = GUM_ADDRESS (mod_base + desc->FirstThunk);

    thunk_data = (const IMAGE_THUNK_DATA *)
        (mod_base + desc->OriginalFirstThunk);
    for (; thunk_data->u1.AddressOfData != 0; thunk_data++)
    {
      if ((thunk_data->u1.AddressOfData & IMAGE_ORDINAL_FLAG) != 0)
        continue; /* FIXME: we ignore imports by ordinal */

      details.name = (const gchar *)
          (mod_base + thunk_data->u1.AddressOfData + 2);
      details.address = gum_module_find_export_by_name (module, details.name);

      if (!func (&details, user_data))
        return;
    }
  }
}

static void
gum_native_module_enumerate_exports (GumModule * module,
                                     GumFoundExportFunc func,
                                     gpointer user_data)
{
  GumNativeModule * self;
  const guint8 * mod_base;
  const IMAGE_DOS_HEADER * dos_hdr;
  const IMAGE_NT_HEADERS * nt_hdrs;
  const IMAGE_DATA_DIRECTORY * entry;
  const IMAGE_EXPORT_DIRECTORY * exp;
  const guint8 * exp_start, * exp_end;
  GumCollectCodePagesContext ctx;
  const DWORD * name_rvas, * func_rvas;
  const WORD * ord_rvas;
  DWORD i;

  self = GUM_NATIVE_MODULE (module);

  mod_base = (const guint8 *) self->handle;
  dos_hdr = (const IMAGE_DOS_HEADER *) self->handle;
  nt_hdrs = (const IMAGE_NT_HEADERS *) &mod_base[dos_hdr->e_lfanew];
  entry = &nt_hdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  exp = (const IMAGE_EXPORT_DIRECTORY *)(mod_base + entry->VirtualAddress);
  exp_start = mod_base + entry->VirtualAddress;
  exp_end = exp_start + entry->Size - 1;

  if (exp->AddressOfNames == 0)
    return;

  ctx.base = GUM_ADDRESS (mod_base);
  ctx.page_size = gum_query_page_size ();
  ctx.num_pages = GUM_ALIGN_SIZE (
      nt_hdrs->OptionalHeader.SizeOfImage, ctx.page_size) / ctx.page_size;
  ctx.bitmap = g_malloc0 (GUM_ALIGN_SIZE (ctx.num_pages, 8) / 8);

  gum_module_enumerate_ranges (module, GUM_PAGE_EXECUTE, gum_collect_code_pages,
      &ctx);

  name_rvas = (const DWORD *) &mod_base[exp->AddressOfNames];
  ord_rvas = (const WORD *) &mod_base[exp->AddressOfNameOrdinals];
  func_rvas = (const DWORD *) &mod_base[exp->AddressOfFunctions];

  for (i = 0; i != exp->NumberOfNames; i++)
  {
    DWORD func_rva;
    const guint8 * func_address;
    GumExportDetails details;
    gsize page_index;

    func_rva = func_rvas[ord_rvas[i]];
    func_address = &mod_base[func_rva];
    if (func_address >= exp_start && func_address <= exp_end)
      continue;

    page_index = func_rva / ctx.page_size;

    details.type = (page_index < ctx.num_pages &&
        (ctx.bitmap[page_index / 8] & (1 << (page_index % 8))) != 0)
        ? GUM_EXPORT_FUNCTION
        : GUM_EXPORT_VARIABLE;
    details.name = (const gchar *) &mod_base[name_rvas[i]];
    details.address = GUM_ADDRESS (func_address);
    details.size = -1;

    if (!func (&details, user_data))
      goto beach;
  }

beach:
  g_free (ctx.bitmap);
}

static gboolean
gum_collect_code_pages (const GumRangeDetails * details,
                        gpointer user_data)
{
  GumCollectCodePagesContext * ctx = user_data;
  gsize start_page, end_page, page;

  start_page = (details->range->base_address - ctx->base) / ctx->page_size;
  end_page = start_page + (details->range->size / ctx->page_size);

  for (page = start_page; page != end_page; page++)
    ctx->bitmap[page / 8] |= 1 << (page % 8);

  return TRUE;
}

static void
gum_native_module_enumerate_symbols (GumModule * module,
                                     GumFoundSymbolFunc func,
                                     gpointer user_data)
{
  GumNativeModule * self;
  GumDbghelpImpl * dbghelp;
  GumEnumerateSymbolsContext ctx;

  self = GUM_NATIVE_MODULE (module);

  dbghelp = gum_dbghelp_impl_try_obtain ();
  if (dbghelp == NULL)
    return;

  ctx.func = func;
  ctx.user_data = user_data;
  dbghelp->SymEnumSymbols (GetCurrentProcess (),
      GPOINTER_TO_SIZE (self->handle), NULL, gum_emit_symbol, &ctx);
}

static BOOL CALLBACK
gum_emit_symbol (PSYMBOL_INFO info,
                 ULONG symbol_size,
                 PVOID user_context)
{
  GumEnumerateSymbolsContext * ctx = user_context;
  GumSymbolDetails details;

  details.is_global = info->Tag == SymTagPublicSymbol ||
      (info->Flags & SYMFLAG_EXPORT) != 0;

  if (info->Tag == SymTagPublicSymbol || info->Tag == SymTagFunction)
  {
    details.type = GUM_SYMBOL_FUNCTION;
  }
  else if (info->Tag == SymTagData)
  {
    details.type = ((info->Flags & SYMFLAG_TLSREL) != 0)
        ? GUM_SYMBOL_TLS
        : GUM_SYMBOL_OBJECT;
  }
  else
  {
    return TRUE;
  }

  details.section = NULL;
  details.name = info->Name;
  details.address = info->Address;
  details.size = symbol_size;

  return ctx->func (&details, ctx->user_data);
}

static void
gum_native_module_enumerate_ranges (GumModule * module,
                                    GumPageProtection prot,
                                    GumFoundRangeFunc func,
                                    gpointer user_data)
{
  GumNativeModule * self;
  guint8 * cur_base_address, * end_address;

  self = GUM_NATIVE_MODULE (module);

  cur_base_address = GSIZE_TO_POINTER (self->range.base_address);
  end_address = cur_base_address + self->range.size;

  do
  {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T ret G_GNUC_UNUSED;

    ret = VirtualQuery (cur_base_address, &mbi, sizeof (mbi));
    g_assert (ret != 0);

    if (mbi.Protect != 0)
    {
      GumPageProtection cur_prot;

      cur_prot = gum_page_protection_from_windows (mbi.Protect);

      if ((cur_prot & prot) == prot)
      {
        GumMemoryRange range;
        GumRangeDetails details;

        range.base_address = GUM_ADDRESS (cur_base_address);
        range.size = mbi.RegionSize;

        details.range = &range;
        details.protection = cur_prot;
        details.file = NULL; /* TODO */

        if (!func (&details, user_data))
          return;
      }
    }

    cur_base_address += mbi.RegionSize;
  }
  while (cur_base_address < end_address);
}

static void
gum_native_module_enumerate_sections (GumModule * module,
                                      GumFoundSectionFunc func,
                                      gpointer user_data)
{
  GumNativeModule * self;
  const guint8 * mod_base;
  const IMAGE_DOS_HEADER * dos_hdr;
  const IMAGE_NT_HEADERS * nt_hdrs;
  const IMAGE_SECTION_HEADER * sec_hdrs;
  WORD i;

  self = GUM_NATIVE_MODULE (module);

  mod_base = (const guint8 *) self->handle;
  dos_hdr = (const IMAGE_DOS_HEADER *) self->handle;
  nt_hdrs = (const IMAGE_NT_HEADERS *) &mod_base[dos_hdr->e_lfanew];
  sec_hdrs = IMAGE_FIRST_SECTION (nt_hdrs);

  for (i = 0; i != nt_hdrs->FileHeader.NumberOfSections; i++)
  {
    const IMAGE_SECTION_HEADER * section = &sec_hdrs[i];
    GumSectionDetails details;
    gboolean carry_on;

    details.id = g_strdup_printf ("%d.%s", i, section->Name);
    details.name = section->Name;
    details.address = GUM_ADDRESS (mod_base + section->VirtualAddress);
    details.size = section->SizeOfRawData;

    carry_on = func (&details, user_data);

    g_free ((gpointer) details.id);

    if (!carry_on)
      break;
  }
}

static void
gum_native_module_enumerate_dependencies (GumModule * module,
                                          GumFoundDependencyFunc func,
                                          gpointer user_data)
{
}

static GumAddress
gum_native_module_find_export_by_name (GumModule * module,
                                       const gchar * symbol_name)
{
  GumNativeModule * self = GUM_NATIVE_MODULE (module);

  return GUM_ADDRESS (GetProcAddress (self->handle, symbol_name));
}

GumAddress
gum_module_find_global_export_by_name (const gchar * symbol_name)
{
  GumFindExportContext ctx;

  ctx.symbol_name = symbol_name;
  ctx.result = 0;

  gum_process_enumerate_modules (gum_store_address_if_module_has_export, &ctx);

  return ctx.result;
}

static gboolean
gum_store_address_if_module_has_export (GumModule * module,
                                        gpointer user_data)
{
  GumFindExportContext * ctx = user_data;

  ctx->result = gum_module_find_export_by_name (module, ctx->symbol_name);

  return ctx->result == 0;
}
