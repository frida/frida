/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummodule-elf.h"

#include <dlfcn.h>

GumModule *
gum_module_load (const gchar * module_name,
                 GError ** error)
{
  GumModule * module;
  gpointer handle;

  handle = dlopen (module_name, RTLD_LAZY);
  if (handle == NULL)
    goto not_found;

  module = gum_process_find_module_by_name (module_name);
  g_assert (module != NULL);

  return module;

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND, "%s", dlerror ());
    return NULL;
  }
}

gchar *
_gum_native_module_find_path_by_address (GumAddress address)
{
  Dl_info info;

  if (dladdr (GSIZE_TO_POINTER (address), &info) == 0)
    return NULL;

  return g_strdup (info.dli_fname);
}
