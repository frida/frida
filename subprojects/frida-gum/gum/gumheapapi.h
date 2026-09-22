/*
 * Copyright (C) 2010-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_HEAP_API_H__
#define __GUM_HEAP_API_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

typedef struct _GumHeapApi GumHeapApi;
typedef GArray GumHeapApiList;

typedef gpointer (* GumMallocFunc) (gsize size);
typedef gpointer (* GumCallocFunc) (gsize num, gsize size);
typedef gpointer (* GumReallocFunc) (gpointer old_address, gsize new_size);
typedef void (* GumFreeFunc) (gpointer address);
typedef gpointer (* GumMallocDbgFunc) (gsize size, gint block_type,
    const gchar * filename, gint linenumber);
typedef gpointer (* GumCallocDbgFunc) (gsize num, gsize size, gint block_type,
    const gchar * filename, gint linenumber);
typedef gpointer (* GumReallocDbgFunc) (gpointer old_address, gsize new_size,
    gint block_type, const gchar * filename, gint linenumber);
typedef void (* GumFreeDbgFunc) (gpointer address, gint block_type);
typedef gint (* GumCrtReportBlockTypeFunc) (gpointer block);

struct _GumHeapApi
{
  GumMallocFunc malloc;
  GumCallocFunc calloc;
  GumReallocFunc realloc;
  GumFreeFunc free;

  /* For Microsoft's Debug CRT: */
  GumMallocDbgFunc _malloc_dbg;
  GumCallocDbgFunc _calloc_dbg;
  GumReallocDbgFunc _realloc_dbg;
  GumFreeDbgFunc _free_dbg;
  GumCrtReportBlockTypeFunc _CrtReportBlockType;
};

GUM_API GumHeapApiList * gum_process_find_heap_apis (void);

GUM_API GumHeapApiList * gum_heap_api_list_new (void);
GUM_API GumHeapApiList * gum_heap_api_list_copy (const GumHeapApiList * list);
GUM_API void gum_heap_api_list_free (GumHeapApiList * list);

GUM_API void gum_heap_api_list_add (GumHeapApiList * self,
    const GumHeapApi * api);
GUM_API const GumHeapApi * gum_heap_api_list_get_nth (
    const GumHeapApiList * self, guint n);

G_END_DECLS

#endif
