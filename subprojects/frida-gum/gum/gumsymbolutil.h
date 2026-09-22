/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 * Copyright (C) 2020 Matt Oh <oh.jeongwook@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SYMBOL_UTIL_H__
#define __GUM_SYMBOL_UTIL_H__

#include <gum/gummemory.h>

typedef struct _GumDebugSymbolDetails GumDebugSymbolDetails;

struct _GumDebugSymbolDetails
{
  GumAddress address;
  gchar module_name[GUM_MAX_PATH + 1];
  gchar symbol_name[GUM_MAX_SYMBOL_NAME + 1];
  gchar file_name[GUM_MAX_PATH + 1];
  guint line_number;
  guint column;
};

G_BEGIN_DECLS

GUM_API gboolean gum_symbol_details_from_address (gpointer address,
    GumDebugSymbolDetails * details);
GUM_API gchar * gum_symbol_name_from_address (gpointer address);

GUM_API gpointer gum_find_function (const gchar * name);
GUM_API GArray * gum_find_functions_named (const gchar * name);
GUM_API GArray * gum_find_functions_matching (const gchar * str);
GUM_API gboolean gum_load_symbols (const gchar * path);

G_END_DECLS

#endif
