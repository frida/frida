/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_H__
#define __GUM_MODULE_H__

#include <gum/gummemory.h>

G_BEGIN_DECLS

#define GUM_TYPE_MODULE (gum_module_get_type ())
G_DECLARE_INTERFACE (GumModule, gum_module, GUM, MODULE, GObject)

typedef struct _GumImportDetails GumImportDetails;
typedef struct _GumExportDetails GumExportDetails;
typedef struct _GumSymbolDetails GumSymbolDetails;
typedef struct _GumSymbolSection GumSymbolSection;
typedef struct _GumSectionDetails GumSectionDetails;
typedef struct _GumDependencyDetails GumDependencyDetails;

typedef gboolean (* GumFoundImportFunc) (const GumImportDetails * details,
    gpointer user_data);
typedef gboolean (* GumFoundExportFunc) (const GumExportDetails * details,
    gpointer user_data);
typedef gboolean (* GumFoundSymbolFunc) (const GumSymbolDetails * details,
    gpointer user_data);
typedef gboolean (* GumFoundSectionFunc) (const GumSectionDetails * details,
    gpointer user_data);
typedef gboolean (* GumFoundDependencyFunc) (
    const GumDependencyDetails * details, gpointer user_data);
typedef GumAddress (* GumResolveExportFunc) (const char * module_name,
    const char * symbol_name, gpointer user_data);

typedef enum {
  GUM_IMPORT_UNKNOWN,
  GUM_IMPORT_FUNCTION,
  GUM_IMPORT_VARIABLE
} GumImportType;

typedef enum {
  GUM_EXPORT_FUNCTION = 1,
  GUM_EXPORT_VARIABLE
} GumExportType;

typedef enum {
  /* Common */
  GUM_SYMBOL_UNKNOWN,
  GUM_SYMBOL_SECTION,

  /* Mach-O */
  GUM_SYMBOL_UNDEFINED,
  GUM_SYMBOL_ABSOLUTE,
  GUM_SYMBOL_PREBOUND_UNDEFINED,
  GUM_SYMBOL_INDIRECT,

  /* ELF */
  GUM_SYMBOL_OBJECT,
  GUM_SYMBOL_FUNCTION,
  GUM_SYMBOL_FILE,
  GUM_SYMBOL_COMMON,
  GUM_SYMBOL_TLS,
} GumSymbolType;

struct _GumModuleInterface
{
  GTypeInterface parent;

  const gchar * (* get_name) (GumModule * self);
  const gchar * (* get_version) (GumModule * self);
  const gchar * (* get_path) (GumModule * self);
  const GumMemoryRange * (* get_range) (GumModule * self);
  void (* ensure_initialized) (GumModule * self);
  void (* enumerate_imports) (GumModule * self, GumFoundImportFunc func,
      gpointer user_data);
  void (* enumerate_exports) (GumModule * self, GumFoundExportFunc func,
      gpointer user_data);
  void (* enumerate_symbols) (GumModule * self, GumFoundSymbolFunc func,
      gpointer user_data);
  void (* enumerate_ranges) (GumModule * self, GumPageProtection prot,
      GumFoundRangeFunc func, gpointer user_data);
  void (* enumerate_sections) (GumModule * self, GumFoundSectionFunc func,
      gpointer user_data);
  void (* enumerate_dependencies) (GumModule * self,
      GumFoundDependencyFunc func, gpointer user_data);
  GumAddress (* find_export_by_name) (GumModule * self,
      const gchar * symbol_name);
  GumAddress (* find_symbol_by_name) (GumModule * self,
      const gchar * symbol_name);
};

struct _GumImportDetails
{
  GumImportType type;
  const gchar * name;
  const gchar * module;
  GumAddress address;
  GumAddress slot;
};

struct _GumExportDetails
{
  GumExportType type;
  const gchar * name;
  GumAddress address;
  gssize size;
};

struct _GumSymbolDetails
{
  gboolean is_global;
  GumSymbolType type;
  const GumSymbolSection * section;
  const gchar * name;
  GumAddress address;
  gssize size;
};

struct _GumSymbolSection
{
  const gchar * id;
  GumPageProtection protection;
};

struct _GumSectionDetails
{
  const gchar * id;
  const gchar * name;
  GumAddress address;
  gsize size;
};

typedef enum {
  GUM_DEPENDENCY_REGULAR,
  GUM_DEPENDENCY_WEAK,
  GUM_DEPENDENCY_REEXPORT,
  GUM_DEPENDENCY_UPWARD,
} GumDependencyType;

struct _GumDependencyDetails
{
  const gchar * name;
  GumDependencyType type;
};

GUM_API GumModule * gum_module_load (const gchar * module_name,
    GError ** error);

GUM_API const gchar * gum_module_get_name (GumModule * self);
GUM_API const gchar * gum_module_get_version (GumModule * self);
GUM_API const gchar * gum_module_get_path (GumModule * self);
GUM_API const GumMemoryRange * gum_module_get_range (GumModule * self);

GUM_API void gum_module_ensure_initialized (GumModule * self);
GUM_API void gum_module_enumerate_imports (GumModule * self,
    GumFoundImportFunc func, gpointer user_data);
GUM_API void gum_module_enumerate_exports (GumModule * self,
    GumFoundExportFunc func, gpointer user_data);
GUM_API void gum_module_enumerate_symbols (GumModule * self,
    GumFoundSymbolFunc func, gpointer user_data);
GUM_API void gum_module_enumerate_ranges (GumModule * self,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);
GUM_API void gum_module_enumerate_sections (GumModule * self,
    GumFoundSectionFunc func, gpointer user_data);
GUM_API void gum_module_enumerate_dependencies (GumModule * self,
    GumFoundDependencyFunc func, gpointer user_data);
GUM_API GumAddress gum_module_find_export_by_name (GumModule * self,
    const gchar * symbol_name);
GUM_API GumAddress gum_module_find_global_export_by_name (
    const gchar * symbol_name);
GUM_API GumAddress gum_module_find_symbol_by_name (GumModule * self,
    const gchar * symbol_name);

GUM_API const gchar * gum_symbol_type_to_string (GumSymbolType type);

G_END_DECLS

#endif
