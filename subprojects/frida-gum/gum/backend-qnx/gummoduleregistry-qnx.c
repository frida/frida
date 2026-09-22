/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry-elf.h"

#include "gummodule-elf.h"
#include "gum/gumqnx.h"

#include <dlfcn.h>
#include <sys/link.h>

static int gum_module_registry_on_ldd_event (Ldd_Eh_Data_t * ehd,
    void * eh_d_handle, unsigned flags);

static gpointer gum_create_module_handle (GumNativeModule * module,
    gpointer user_data);
static gchar * gum_resolve_path (const gchar * path);

static void * gum_ldd_handler;

void
_gum_module_registry_activate (GumModuleRegistry * self)
{
  gum_ldd_handler = __ldd_register_eh (gum_module_registry_on_ldd_event, self,
      LDD_EH_DLL_REPLAY | LDD_EH_DLL_LOAD | LDD_EH_DLL_UNLOAD);
}

void
_gum_module_registry_deactivate (GumModuleRegistry * self)
{
  __ldd_deregister_eh (gum_ldd_handler);
}

static int
gum_module_registry_on_ldd_event (Ldd_Eh_Data_t * ehd,
                                  void * eh_d_handle,
                                  unsigned flags)
{
  GumModuleRegistry * self = eh_d_handle;
  const Link_map * map = ehd->l_map;

  if ((flags & LDD_EH_DLL_LOAD) != 0)
  {
    const Elf32_Ehdr * ehdr = GSIZE_TO_POINTER (map->l_addr);
    gchar * resolved_path;
    const gchar * path;
    GumMemoryRange range;
    const Elf32_Phdr * phdr;
    GumAddress lowest, highest;
    gsize page_size_mask;
    guint i;
    GumNativeModule * module;

    if (ehdr->e_type == ET_EXEC)
    {
      resolved_path = gum_qnx_query_program_path_for_self (NULL);
      g_assert (resolved_path != NULL);

      path = resolved_path;
    }
    else
    {
      resolved_path = gum_resolve_path (map->l_path);

      path = resolved_path;
    }

    lowest = ~0;
    highest = 0;
    page_size_mask = ~((gsize) gum_query_page_size () - 1);
    phdr = (gconstpointer) ehdr + ehdr->e_phoff;
    for (i = 0; i != ehdr->e_phnum; i++)
    {
      const Elf32_Phdr * h = &phdr[i];
      if (h->p_type == PT_LOAD)
      {
        lowest = MIN (h->p_vaddr & page_size_mask, lowest);
        highest = MAX (h->p_vaddr + h->p_memsz, highest);
      }
    }
    range.base_address = map->l_addr + lowest;
    range.size = highest - lowest;

    module = _gum_native_module_make (path, &range, gum_create_module_handle,
        NULL, NULL, (GDestroyNotify) dlclose);

    _gum_module_registry_register (self, GUM_MODULE (module));

    g_object_unref (module);

    g_free (resolved_path);
  }
  else
  {
    _gum_module_registry_unregister (self, map->l_addr);
  }

  return 0;
}

static gpointer
gum_create_module_handle (GumNativeModule * module,
                          gpointer user_data)
{
  return dlopen (module->path, RTLD_LAZY | RTLD_NOLOAD);
}

static gchar *
gum_resolve_path (const gchar * path)
{
  gchar * target, * parent_dir, * canonical_path;

  target = g_file_read_link (path, NULL);
  if (target == NULL)
    return g_strdup (path);

  parent_dir = g_path_get_dirname (path);

  canonical_path = g_canonicalize_filename (target, parent_dir);

  g_free (parent_dir);
  g_free (target);

  return canonical_path;
}
