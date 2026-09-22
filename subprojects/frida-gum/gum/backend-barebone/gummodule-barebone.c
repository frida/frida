/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodule.h"

G_GNUC_WEAK GumModule *
gum_module_load (const gchar * module_name,
                 GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Not supported by the Barebone backend");
  return NULL;
}

G_GNUC_WEAK GumAddress
gum_module_find_global_export_by_name (const gchar * symbol_name)
{
  return 0;
}
