/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsymbolutil.h"

static GArray * gum_pointer_array_new_empty (void);

G_GNUC_WEAK gboolean
gum_symbol_details_from_address (gpointer address,
                                 GumDebugSymbolDetails * details)
{
  return FALSE;
}

G_GNUC_WEAK gchar *
gum_symbol_name_from_address (gpointer address)
{
  return NULL;
}

G_GNUC_WEAK gpointer
gum_find_function (const gchar * name)
{
  return NULL;
}

G_GNUC_WEAK GArray *
gum_find_functions_named (const gchar * name)
{
  return gum_pointer_array_new_empty ();
}

G_GNUC_WEAK GArray *
gum_find_functions_matching (const gchar * str)
{
  return gum_pointer_array_new_empty ();
}

G_GNUC_WEAK gboolean
gum_load_symbols (const gchar * path)
{
  return FALSE;
}

static GArray *
gum_pointer_array_new_empty (void)
{
  return g_array_new (FALSE, FALSE, sizeof (gpointer));
}
