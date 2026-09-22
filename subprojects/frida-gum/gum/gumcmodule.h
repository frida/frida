/*
 * Copyright (C) 2019-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_CMODULE_H__
#define __GUM_CMODULE_H__

#include <gum/gummemory.h>

G_BEGIN_DECLS

#define GUM_TYPE_CMODULE (gum_cmodule_get_type ())
G_DECLARE_DERIVABLE_TYPE (GumCModule, gum_cmodule, GUM, CMODULE, GObject)

typedef struct _GumCModuleOptions GumCModuleOptions;
typedef guint GumCModuleToolchain;
typedef struct _GumCDefineDetails GumCDefineDetails;
typedef struct _GumCHeaderDetails GumCHeaderDetails;
typedef guint GumCHeaderKind;
typedef struct _GumCSymbolDetails GumCSymbolDetails;

typedef void (* GumFoundCDefineFunc) (const GumCDefineDetails * details,
    gpointer user_data);
typedef void (* GumFoundCHeaderFunc) (const GumCHeaderDetails * details,
    gpointer user_data);
typedef void (* GumFoundCSymbolFunc) (const GumCSymbolDetails * details,
    gpointer user_data);

struct _GumCModuleClass
{
  GObjectClass parent_class;

  void (* add_define) (GumCModule * cm, const gchar * name,
      const gchar * value);
  void (* add_symbol) (GumCModule * cm, const gchar * name,
      gconstpointer value);
  gboolean (* link_pre) (GumCModule * cm, gsize * size,
      GString ** error_messages);
  gboolean (* link_at) (GumCModule * cm, gpointer base,
      GString ** error_messages);
  void (* link_post) (GumCModule * cm);
  void (* enumerate_symbols) (GumCModule * cm, GumFoundCSymbolFunc func,
      gpointer user_data);
  gpointer (* find_symbol_by_name) (GumCModule * cm, const gchar * name);
  void (* drop_metadata) (GumCModule * cm);
};

struct _GumCModuleOptions
{
  GumCModuleToolchain toolchain;
};

enum _GumCModuleToolchain
{
  GUM_CMODULE_TOOLCHAIN_ANY,
  GUM_CMODULE_TOOLCHAIN_INTERNAL,
  GUM_CMODULE_TOOLCHAIN_EXTERNAL
};

struct _GumCDefineDetails
{
  const gchar * name;
  const gchar * value;
};

struct _GumCHeaderDetails
{
  const gchar * name;
  const gchar * data;
  guint size;
  GumCHeaderKind kind;
};

enum _GumCHeaderKind
{
  GUM_CHEADER_FRIDA,
  GUM_CHEADER_TCC
};

struct _GumCSymbolDetails
{
  const gchar * name;
  gpointer address;
};

GUM_API GumCModule * gum_cmodule_new (const gchar * source, GBytes * binary,
    const GumCModuleOptions * options, GError ** error);

GUM_API const GumMemoryRange * gum_cmodule_get_range (GumCModule * self);

GUM_API void gum_cmodule_add_symbol (GumCModule * self, const gchar * name,
    gconstpointer value);

GUM_API gboolean gum_cmodule_link (GumCModule * self, GError ** error);

GUM_API void gum_cmodule_enumerate_builtin_defines (GumFoundCDefineFunc func,
    gpointer user_data);
GUM_API void gum_cmodule_enumerate_builtin_headers (GumFoundCHeaderFunc func,
    gpointer user_data);
GUM_API void gum_cmodule_enumerate_symbols (GumCModule * self,
    GumFoundCSymbolFunc func, gpointer user_data);
GUM_API gpointer gum_cmodule_find_symbol_by_name (GumCModule * self,
    const gchar * name);

GUM_API void gum_cmodule_drop_metadata (GumCModule * self);

G_END_DECLS

#endif
