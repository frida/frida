/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SCRIPT_API_PRIV_H__
#define __GUM_SCRIPT_API_PRIV_H__

#include "gumscriptapi.h"

G_BEGIN_DECLS

typedef struct _GumScriptApiFunction GumScriptApiFunction;

struct _GumScriptApiFunction
{
  gchar * name;
  GumScriptApiType * arg_types;
  guint arity;
  GumScriptApiType retval_type;
  GumScriptApiFunc func;
  gpointer user_data;
};

G_GNUC_INTERNAL GPtrArray * _gum_script_api_registry_snapshot (
    GumScriptApiRegistry * self);

G_GNUC_INTERNAL const gchar * _gum_script_api_get_name (GumScriptApi * self);
G_GNUC_INTERNAL GArray * _gum_script_api_peek_functions (GumScriptApi * self);
G_GNUC_INTERNAL const gchar * _gum_script_api_get_prelude (GumScriptApi * self);

G_END_DECLS

#endif
