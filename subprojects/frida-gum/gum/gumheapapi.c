/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumheapapi.h"

#include "gumprocess.h"

#include <string.h>
#ifdef _MSC_VER
# include <malloc.h>
# include <stdlib.h>
# ifdef _DEBUG
#  include <crtdbg.h>
# endif
#endif

/**
 * GumHeapApiList: (skip)
 */

#ifdef HAVE_WINDOWS
static gboolean gum_collect_heap_api_if_crt_module (GumModule * module,
    gpointer user_data);
#endif
static void gum_collect_heap_api_from_libc_module (GumModule * libc,
    GumHeapApiList * list);

GumHeapApiList *
gum_process_find_heap_apis (void)
{
  GumHeapApiList * list;

  list = gum_heap_api_list_new ();

#ifdef _MSC_VER
  /* XXX: For now we assume that the static CRT is being used. */
  {
    GumHeapApi api = { 0, };

    api.malloc = (gpointer (*) (gsize)) malloc;
    api.calloc = (gpointer (*) (gsize, gsize)) calloc;
    api.realloc = (gpointer (*) (gpointer, gsize)) realloc;
    api.free = free;

# ifdef _DEBUG
    api._malloc_dbg = _malloc_dbg;
    api._calloc_dbg = _calloc_dbg;
    api._realloc_dbg = _realloc_dbg;
    api._free_dbg = _free_dbg;
    api._CrtReportBlockType = _CrtReportBlockType;
# endif

    gum_heap_api_list_add (list, &api);
  }
#endif

#ifdef HAVE_WINDOWS
  gum_process_enumerate_modules (gum_collect_heap_api_if_crt_module, list);
#else
  gum_collect_heap_api_from_libc_module (gum_process_get_libc_module (), list);
#endif

  return list;
}

#ifdef HAVE_WINDOWS

static gboolean
gum_collect_heap_api_if_crt_module (GumModule * module,
                                    gpointer user_data)
{
  GumHeapApiList * list = (GumHeapApiList *) user_data;
  const gchar * module_name;

  module_name = gum_module_get_name (module);

  if (g_ascii_strncasecmp (module_name, "msvcr", 5) == 0)
    gum_collect_heap_api_from_libc_module (module, list);

  return TRUE;
}

#endif

static void
gum_collect_heap_api_from_libc_module (GumModule * libc,
                                       GumHeapApiList * list)
{
  GumHeapApi api = { 0, };

#define GUM_ASSIGN(type, name) \
    api.name = GUM_POINTER_TO_FUNCPTR (type, gum_module_find_export_by_name ( \
        libc, G_STRINGIFY (name)))

  GUM_ASSIGN (GumMallocFunc, malloc);
  GUM_ASSIGN (GumCallocFunc, calloc);
  GUM_ASSIGN (GumReallocFunc, realloc);
  GUM_ASSIGN (GumFreeFunc, free);

#ifdef HAVE_WINDOWS
  if (g_str_has_suffix (gum_module_get_name (libc), "d.dll"))
  {
    GUM_ASSIGN (GumMallocDbgFunc, _malloc_dbg);
    GUM_ASSIGN (GumCallocDbgFunc, _calloc_dbg);
    GUM_ASSIGN (GumReallocDbgFunc, _realloc_dbg);
    GUM_ASSIGN (GumFreeDbgFunc, _free_dbg);
    GUM_ASSIGN (GumCrtReportBlockTypeFunc, _CrtReportBlockType);
  }
#endif

#undef GUM_ASSIGN

  gum_heap_api_list_add (list, &api);
}

GumHeapApiList *
gum_heap_api_list_new (void)
{
  return g_array_new (FALSE, FALSE, sizeof (GumHeapApi));
}

GumHeapApiList *
gum_heap_api_list_copy (const GumHeapApiList * list)
{
  GumHeapApiList * copy;

  copy = g_array_sized_new (FALSE, FALSE, sizeof (GumHeapApi), list->len);
  g_array_append_vals (copy, list->data, list->len);

  return copy;
}

void
gum_heap_api_list_free (GumHeapApiList * list)
{
  g_array_free (list, TRUE);
}

void
gum_heap_api_list_add (GumHeapApiList * self,
                       const GumHeapApi * api)
{
  GumHeapApi api_copy = *api;

  g_array_append_val (self, api_copy);
}

const GumHeapApi *
gum_heap_api_list_get_nth (const GumHeapApiList * self,
                           guint n)
{
  return &g_array_index (self, GumHeapApi, n);
}
