/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodulefacade.h"

struct _GumModuleFacade
{
  GObject parent;

  GumModule * module;
  GObject * resolver;
};

static void gum_module_facade_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_module_facade_dispose (GObject * object);
static const gchar * gum_module_facade_get_name (GumModule * module);
static const gchar * gum_module_facade_get_path (GumModule * module);
static const GumMemoryRange * gum_module_facade_get_range (GumModule * module);
static void gum_module_facade_ensure_initialized (GumModule * module);
static void gum_module_facade_enumerate_imports (GumModule * module,
    GumFoundImportFunc func, gpointer user_data);
static void gum_module_facade_enumerate_exports (GumModule * module,
    GumFoundExportFunc func, gpointer user_data);
static void gum_module_facade_enumerate_symbols (GumModule * module,
    GumFoundSymbolFunc func, gpointer user_data);
static void gum_module_facade_enumerate_ranges (GumModule * module,
    GumPageProtection prot, GumFoundRangeFunc func, gpointer user_data);
static void gum_module_facade_enumerate_sections (GumModule * module,
    GumFoundSectionFunc func, gpointer user_data);
static void gum_module_facade_enumerate_dependencies (GumModule * module,
    GumFoundDependencyFunc func, gpointer user_data);
static GumAddress gum_module_facade_find_export_by_name (GumModule * module,
    const gchar * symbol_name);
static GumAddress gum_module_facade_find_symbol_by_name (GumModule * module,
    const gchar * symbol_name);

G_DEFINE_TYPE_EXTENDED (GumModuleFacade,
                        gum_module_facade,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_MODULE,
                            gum_module_facade_iface_init))

static void
gum_module_facade_class_init (GumModuleFacadeClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_module_facade_dispose;
}

static void
gum_module_facade_iface_init (gpointer g_iface,
                              gpointer iface_data)
{
  GumModuleInterface * iface = g_iface;

  iface->get_name = gum_module_facade_get_name;
  iface->get_path = gum_module_facade_get_path;
  iface->get_range = gum_module_facade_get_range;
  iface->ensure_initialized = gum_module_facade_ensure_initialized;
  iface->enumerate_imports = gum_module_facade_enumerate_imports;
  iface->enumerate_exports = gum_module_facade_enumerate_exports;
  iface->enumerate_symbols = gum_module_facade_enumerate_symbols;
  iface->enumerate_ranges = gum_module_facade_enumerate_ranges;
  iface->enumerate_sections = gum_module_facade_enumerate_sections;
  iface->enumerate_dependencies = gum_module_facade_enumerate_dependencies;
  iface->find_export_by_name = gum_module_facade_find_export_by_name;
  iface->find_symbol_by_name = gum_module_facade_find_symbol_by_name;
}

static void
gum_module_facade_init (GumModuleFacade * self)
{
}

static void
gum_module_facade_dispose (GObject * object)
{
  GumModuleFacade * self = GUM_MODULE_FACADE (object);

  g_clear_object (&self->module);
  g_clear_object (&self->resolver);

  G_OBJECT_CLASS (gum_module_facade_parent_class)->dispose (object);
}

GumModuleFacade *
_gum_module_facade_new (GumModule * module,
                        GObject * resolver)
{
  GumModuleFacade * facade;

  facade = g_object_new (GUM_TYPE_MODULE_FACADE, NULL);
  facade->module = g_object_ref (module);
  facade->resolver = g_object_ref (resolver);

  return facade;
}

GumModule *
_gum_module_facade_get_module (GumModuleFacade * self)
{
  return self->module;
}

static const gchar *
gum_module_facade_get_name (GumModule * module)
{
  return gum_module_get_name (GUM_MODULE_FACADE (module)->module);
}

static const gchar *
gum_module_facade_get_path (GumModule * module)
{
  return gum_module_get_path (GUM_MODULE_FACADE (module)->module);
}

static const GumMemoryRange *
gum_module_facade_get_range (GumModule * module)
{
  return gum_module_get_range (GUM_MODULE_FACADE (module)->module);
}

static void
gum_module_facade_ensure_initialized (GumModule * module)
{
  gum_module_ensure_initialized (GUM_MODULE_FACADE (module)->module);
}

static void
gum_module_facade_enumerate_imports (GumModule * module,
                                     GumFoundImportFunc func,
                                     gpointer user_data)
{
  gum_module_enumerate_imports (GUM_MODULE_FACADE (module)->module, func,
      user_data);
}

static void
gum_module_facade_enumerate_exports (GumModule * module,
                                     GumFoundExportFunc func,
                                     gpointer user_data)
{
  gum_module_enumerate_exports (GUM_MODULE_FACADE (module)->module, func,
      user_data);
}

static void
gum_module_facade_enumerate_symbols (GumModule * module,
                                     GumFoundSymbolFunc func,
                                     gpointer user_data)
{
  gum_module_enumerate_symbols (GUM_MODULE_FACADE (module)->module, func,
      user_data);
}

static void
gum_module_facade_enumerate_ranges (GumModule * module,
                                    GumPageProtection prot,
                                    GumFoundRangeFunc func,
                                    gpointer user_data)
{
  gum_module_enumerate_ranges (GUM_MODULE_FACADE (module)->module, prot, func,
      user_data);
}

static void
gum_module_facade_enumerate_sections (GumModule * module,
                                      GumFoundSectionFunc func,
                                      gpointer user_data)
{
  gum_module_enumerate_sections (GUM_MODULE_FACADE (module)->module, func,
      user_data);
}

static void
gum_module_facade_enumerate_dependencies (GumModule * module,
                                          GumFoundDependencyFunc func,
                                          gpointer user_data)
{
  gum_module_enumerate_dependencies (GUM_MODULE_FACADE (module)->module, func,
      user_data);
}

static GumAddress
gum_module_facade_find_export_by_name (GumModule * module,
                                       const gchar * symbol_name)
{
  return gum_module_find_export_by_name (GUM_MODULE_FACADE (module)->module,
      symbol_name);
}

static GumAddress
gum_module_facade_find_symbol_by_name (GumModule * module,
                                       const gchar * symbol_name)
{
  return gum_module_find_symbol_by_name (GUM_MODULE_FACADE (module)->module,
      symbol_name);
}
