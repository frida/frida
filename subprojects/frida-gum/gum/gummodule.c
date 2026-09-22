/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodule.h"

#include <string.h>

typedef struct _GumSymbolEntry GumSymbolEntry;

struct _GumSymbolEntry
{
  const gchar * name;
  GumAddress address;
};

static gboolean gum_store_symbol (const GumSymbolDetails * details,
    gpointer user_data);
static gint gum_symbol_entry_compare (const GumSymbolEntry * lhs,
    const GumSymbolEntry * rhs);

/**
 * GumModule:
 *
 * Represents a loaded shared library, exposing its metadata and symbols.
 *
 * A module provides its name, version, path and address range, and lets you
 * enumerate its imports, exports, symbols, sections, ranges and dependencies,
 * as well as resolve individual exports and symbols by name. Obtain modules
 * through the Process API — for example `gum_process_find_module_by_name()` or
 * `gum_process_enumerate_modules()` — or load one explicitly with
 * [func@Gum.Module.load].
 */

/**
 * gum_module_load:
 * @module_name: name or path of the module to load
 * @error: (nullable): return location for a #GError
 *
 * Loads the specified module.
 *
 * Returns: (transfer full) (nullable): the loaded module, or %NULL on
 *   error
 */

/**
 * GumImportDetails:
 * @type: the kind of import
 * @name: name of the imported symbol
 * @module: (nullable): name of the module it is imported from, if known
 * @address: resolved address of the import, or 0 if not resolved
 * @slot: address of the slot holding the import, or 0 if not applicable
 *
 * Details about an imported symbol, as passed to a #GumFoundImportFunc.
 */

/**
 * GumImportType:
 * @GUM_IMPORT_UNKNOWN: the kind of import could not be determined
 * @GUM_IMPORT_FUNCTION: an imported function
 * @GUM_IMPORT_VARIABLE: an imported variable
 *
 * The type of an imported symbol.
 */

/**
 * GumExportDetails:
 * @type: the kind of export
 * @name: name of the exported symbol
 * @address: address of the export
 * @size: size of the export in bytes, or -1 if unknown
 *
 * Details about an exported symbol, as passed to a #GumFoundExportFunc.
 */

/**
 * GumExportType:
 * @GUM_EXPORT_FUNCTION: an exported function
 * @GUM_EXPORT_VARIABLE: an exported variable
 *
 * The type of an exported symbol.
 */

/**
 * GumSymbolDetails:
 * @is_global: whether the symbol is global rather than local
 * @type: the kind of symbol
 * @section: (nullable): the section the symbol belongs to, if any
 * @name: name of the symbol
 * @address: address of the symbol
 * @size: size of the symbol in bytes, or -1 if unknown
 *
 * Details about a symbol, as passed to a #GumFoundSymbolFunc.
 */

/**
 * GumSymbolType:
 * @GUM_SYMBOL_UNKNOWN: unknown symbol type
 * @GUM_SYMBOL_SECTION: a section
 * @GUM_SYMBOL_UNDEFINED: Mach-O: an undefined symbol
 * @GUM_SYMBOL_ABSOLUTE: Mach-O: an absolute symbol
 * @GUM_SYMBOL_PREBOUND_UNDEFINED: Mach-O: a prebound undefined symbol
 * @GUM_SYMBOL_INDIRECT: Mach-O: an indirect symbol
 * @GUM_SYMBOL_OBJECT: ELF: a data object
 * @GUM_SYMBOL_FUNCTION: ELF: a function
 * @GUM_SYMBOL_FILE: ELF: a source file name
 * @GUM_SYMBOL_COMMON: ELF: a common block
 * @GUM_SYMBOL_TLS: ELF: a thread-local storage entry
 *
 * The type of a symbol, spanning both Mach-O and ELF classifications.
 */

/**
 * GumSymbolSection:
 * @id: stable identifier of the section, e.g. `0.__TEXT.__text`
 * @protection: the section's memory protection
 *
 * The section a symbol belongs to.
 */

/**
 * GumSectionDetails:
 * @id: stable identifier of the section, e.g. `0.__TEXT.__text`
 * @name: name of the section
 * @address: address of the section in memory
 * @size: size of the section in bytes
 *
 * Details about a section, as passed to a #GumFoundSectionFunc.
 */

/**
 * GumDependencyDetails:
 * @name: name of the dependency
 * @type: the kind of dependency
 *
 * Details about a module dependency, passed to a #GumFoundDependencyFunc.
 */

/**
 * GumDependencyType:
 * @GUM_DEPENDENCY_REGULAR: a regular dependency
 * @GUM_DEPENDENCY_WEAK: a weak dependency
 * @GUM_DEPENDENCY_REEXPORT: a re-exported dependency
 * @GUM_DEPENDENCY_UPWARD: an upward dependency
 *
 * The type of a module dependency.
 */

G_DEFINE_INTERFACE (GumModule, gum_module, G_TYPE_OBJECT)

G_LOCK_DEFINE_STATIC (gum_module_symbol_cache);

static void
gum_module_default_init (GumModuleInterface * iface)
{
}

/**
 * gum_module_get_name:
 * @self: module
 *
 * Gets the module's short name, e.g. `libc.so.6`.
 *
 * Returns: the name
 */
const gchar *
gum_module_get_name (GumModule * self)
{
  return GUM_MODULE_GET_IFACE (self)->get_name (self);
}

/**
 * gum_module_get_version:
 * @self: module
 *
 * Gets the module's version string, where the platform provides one.
 *
 * Returns: (nullable): the version, or %NULL if unavailable
 */
const gchar *
gum_module_get_version (GumModule * self)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->get_version != NULL)
    return iface->get_version (self);

  return NULL;
}

/**
 * gum_module_get_path:
 * @self: module
 *
 * Gets the module's full filesystem path.
 *
 * Returns: the path
 */
const gchar *
gum_module_get_path (GumModule * self)
{
  return GUM_MODULE_GET_IFACE (self)->get_path (self);
}

/**
 * gum_module_get_range:
 * @self: module
 *
 * Gets the module's base address and size in memory.
 *
 * Returns: (transfer none): the memory range
 */
const GumMemoryRange *
gum_module_get_range (GumModule * self)
{
  return GUM_MODULE_GET_IFACE (self)->get_range (self);
}

/**
 * gum_module_ensure_initialized:
 * @self: module
 *
 * Ensures the module's initializers have run, loading it fully if it was only
 * partially loaded, so that calling into its APIs is safe.
 */
void
gum_module_ensure_initialized (GumModule * self)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->ensure_initialized != NULL)
    iface->ensure_initialized (self);
}

/**
 * gum_module_enumerate_imports:
 * @self: module
 * @func: (scope call): function called with #GumImportDetails
 * @user_data: data to pass to @func
 *
 * Enumerates the module's imports, calling @func for each one. Enumeration
 * stops if @func returns %FALSE.
 */
void
gum_module_enumerate_imports (GumModule * self,
                              GumFoundImportFunc func,
                              gpointer user_data)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->enumerate_imports != NULL)
    iface->enumerate_imports (self, func, user_data);
}

/**
 * gum_module_enumerate_exports:
 * @self: module
 * @func: (scope call): function called with #GumExportDetails
 * @user_data: data to pass to @func
 *
 * Enumerates the module's exports, calling @func for each one. Enumeration
 * stops if @func returns %FALSE.
 */
void
gum_module_enumerate_exports (GumModule * self,
                              GumFoundExportFunc func,
                              gpointer user_data)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->enumerate_exports != NULL)
    iface->enumerate_exports (self, func, user_data);
}

/**
 * gum_module_enumerate_symbols:
 * @self: module
 * @func: (scope call): function called with #GumSymbolDetails
 * @user_data: data to pass to @func
 *
 * Enumerates the module's symbols, calling @func for each one. Enumeration
 * stops if @func returns %FALSE. Unlike exports, this includes local and debug
 * symbols where the platform exposes them.
 */
void
gum_module_enumerate_symbols (GumModule * self,
                              GumFoundSymbolFunc func,
                              gpointer user_data)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->enumerate_symbols != NULL)
    iface->enumerate_symbols (self, func, user_data);
}

/**
 * gum_module_enumerate_ranges:
 * @self: module
 * @prot: minimum protection of the ranges to include
 * @func: (scope call): function called with #GumRangeDetails
 * @user_data: data to pass to @func
 *
 * Enumerates the module's memory ranges whose protection includes @prot,
 * calling @func for each one. Enumeration stops if @func returns %FALSE.
 */
void
gum_module_enumerate_ranges (GumModule * self,
                             GumPageProtection prot,
                             GumFoundRangeFunc func,
                             gpointer user_data)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->enumerate_ranges != NULL)
    iface->enumerate_ranges (self, prot, func, user_data);
}

/**
 * gum_module_enumerate_sections:
 * @self: module
 * @func: (scope call): function called with #GumSectionDetails
 * @user_data: data to pass to @func
 *
 * Enumerates sections of the specified module.
 */
void
gum_module_enumerate_sections (GumModule * self,
                               GumFoundSectionFunc func,
                               gpointer user_data)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->enumerate_sections != NULL)
    iface->enumerate_sections (self, func, user_data);
}

/**
 * gum_module_enumerate_dependencies:
 * @self: module
 * @func: (scope call): function called with #GumDependencyDetails
 * @user_data: data to pass to @func
 *
 * Enumerates dependencies of the specified module.
 */
void
gum_module_enumerate_dependencies (GumModule * self,
                                   GumFoundDependencyFunc func,
                                   gpointer user_data)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->enumerate_dependencies != NULL)
    iface->enumerate_dependencies (self, func, user_data);
}

/**
 * gum_module_find_export_by_name:
 * @self: module
 * @symbol_name: name of the export to resolve
 *
 * Resolves an exported symbol in this module.
 *
 * Returns: the export's address, or 0 if not found
 */
GumAddress
gum_module_find_export_by_name (GumModule * self,
                                const gchar * symbol_name)
{
  GumModuleInterface * iface = GUM_MODULE_GET_IFACE (self);

  if (iface->find_export_by_name != NULL)
    return iface->find_export_by_name (self, symbol_name);

  return 0;
}

/**
 * gum_module_find_symbol_by_name:
 * @self: module
 * @symbol_name: name of the symbol to resolve
 *
 * Resolves a symbol in this module by name. Where the backend offers no direct
 * lookup, the module's symbols are enumerated once and cached for subsequent
 * queries.
 *
 * Returns: the symbol's address, or 0 if not found
 */
GumAddress
gum_module_find_symbol_by_name (GumModule * self,
                                const gchar * symbol_name)
{
  GumModuleInterface * iface;
  GArray * cache;
  GumSymbolEntry needle;
  guint matched_index;

  iface = GUM_MODULE_GET_IFACE (self);

  if (iface->find_symbol_by_name != NULL)
    return iface->find_symbol_by_name (self, symbol_name);

  G_LOCK (gum_module_symbol_cache);

  cache = g_object_get_data (G_OBJECT (self), "symbol-cache");
  if (cache == NULL)
  {
    cache = g_array_new (FALSE, FALSE, sizeof (GumSymbolEntry));
    gum_module_enumerate_symbols (self, gum_store_symbol, cache);
    g_array_sort (cache, (GCompareFunc) gum_symbol_entry_compare);
    g_object_set_data_full (G_OBJECT (self), "symbol-cache", cache,
        (GDestroyNotify) g_array_unref);
  }

  G_UNLOCK (gum_module_symbol_cache);

  needle.name = symbol_name;
  needle.address = 0;

  if (!g_array_binary_search (cache, &needle,
        (GCompareFunc) gum_symbol_entry_compare, &matched_index))
  {
    return 0;
  }

  return g_array_index (cache, GumSymbolEntry, matched_index).address;
}

static gboolean
gum_store_symbol (const GumSymbolDetails * details,
                  gpointer user_data)
{
  GArray * cache = user_data;
  GumSymbolEntry entry;

  /*
   * Implementations guarantee that the lifetime of this string is at least that
   * of the module.
   */
  entry.name = details->name;
  entry.address = details->address;
  g_array_append_val (cache, entry);

  return TRUE;
}

static gint
gum_symbol_entry_compare (const GumSymbolEntry * lhs,
                          const GumSymbolEntry * rhs)
{
  return strcmp (lhs->name, rhs->name);
}

/**
 * gum_module_find_global_export_by_name:
 * @symbol_name: name of the export to resolve
 *
 * Resolves an exported symbol across all loaded modules, following the
 * platform's global symbol resolution order. Convenient when you do not already
 * have the owning module in hand.
 *
 * Returns: the export's address, or 0 if not found
 */

/**
 * gum_symbol_type_to_string:
 * @type: a #GumSymbolType
 *
 * Converts @type to a human-readable string.
 *
 * Returns: a string describing @type
 */
const gchar *
gum_symbol_type_to_string (GumSymbolType type)
{
  switch (type)
  {
    /* Common */
    case GUM_SYMBOL_UNKNOWN:            return "unknown";
    case GUM_SYMBOL_SECTION:            return "section";

    /* Mach-O */
    case GUM_SYMBOL_UNDEFINED:          return "undefined";
    case GUM_SYMBOL_ABSOLUTE:           return "absolute";
    case GUM_SYMBOL_PREBOUND_UNDEFINED: return "prebound-undefined";
    case GUM_SYMBOL_INDIRECT:           return "indirect";

    /* ELF */
    case GUM_SYMBOL_OBJECT:             return "object";
    case GUM_SYMBOL_FUNCTION:           return "function";
    case GUM_SYMBOL_FILE:               return "file";
    case GUM_SYMBOL_COMMON:             return "common";
    case GUM_SYMBOL_TLS:                return "tls";
  }

  g_assert_not_reached ();
  return NULL;
}
