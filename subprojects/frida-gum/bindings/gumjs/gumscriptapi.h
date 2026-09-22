/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SCRIPT_API_H__
#define __GUM_SCRIPT_API_H__

#include <gum/gum.h>

G_BEGIN_DECLS

#define GUM_TYPE_SCRIPT_API_REGISTRY (gum_script_api_registry_get_type ())
G_DECLARE_FINAL_TYPE (GumScriptApiRegistry, gum_script_api_registry, GUM,
    SCRIPT_API_REGISTRY, GObject)

typedef struct _GumScriptApi GumScriptApi;
typedef struct _GumScriptApiValue GumScriptApiValue;
typedef guint GumScriptApiType;

typedef gboolean (* GumScriptApiFunc) (const GumScriptApiValue * args,
    GumScriptApiValue * retval, gpointer user_data, GError ** error);

enum _GumScriptApiType
{
  GUM_SCRIPT_API_VOID,
  GUM_SCRIPT_API_BOOLEAN,
  GUM_SCRIPT_API_INT,
  GUM_SCRIPT_API_UINT,
  GUM_SCRIPT_API_INT64,
  GUM_SCRIPT_API_UINT64,
  GUM_SCRIPT_API_NUMBER,
  GUM_SCRIPT_API_STRING,
  GUM_SCRIPT_API_POINTER,
};

struct _GumScriptApiValue
{
  GumScriptApiType type;

  union
  {
    gboolean b;
    gint i;
    guint u;
    gint64 i64;
    guint64 u64;
    gdouble n;
    const gchar * s;
    gpointer p;
  };
};

GUM_API GumScriptApiRegistry * gum_script_api_registry_obtain (void);
GUM_API void gum_script_api_registry_add (GumScriptApiRegistry * self,
    GumScriptApi * api);
GUM_API void gum_script_api_registry_remove (GumScriptApiRegistry * self,
    GumScriptApi * api);

GUM_API GumScriptApi * gum_script_api_new (const gchar * name);
GUM_API GumScriptApi * gum_script_api_ref (GumScriptApi * api);
GUM_API void gum_script_api_unref (GumScriptApi * api);
GUM_API void gum_script_api_add_function (GumScriptApi * self,
    const gchar * name, const gchar * arg_types, GumScriptApiType retval_type,
    GumScriptApiFunc func, gpointer user_data);
GUM_API void gum_script_api_set_prelude (GumScriptApi * self,
    const gchar * source);

G_END_DECLS

#endif
