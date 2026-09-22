/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PAGE_POOL_H__
#define __GUM_PAGE_POOL_H__

#include <glib-object.h>
#include <gum/gumdefs.h>

G_BEGIN_DECLS

#define GUM_TYPE_PAGE_POOL (gum_page_pool_get_type ())
G_DECLARE_FINAL_TYPE (GumPagePool, gum_page_pool, GUM, PAGE_POOL, GObject)

typedef enum _GumProtectMode GumProtectMode;
typedef struct _GumBlockDetails GumBlockDetails;

enum _GumProtectMode
{
  GUM_PROTECT_MODE_ABOVE = 1
};

struct _GumBlockDetails
{
  gpointer address;
  gsize size;
  gpointer guard;
  gsize guard_size;
  gboolean allocated;
};

GUM_API GumPagePool * gum_page_pool_new (GumProtectMode protect_mode,
    guint n_pages);

GUM_API gpointer gum_page_pool_try_alloc (GumPagePool * self, guint size);
GUM_API gboolean gum_page_pool_try_free (GumPagePool * self, gpointer mem);

GUM_API guint gum_page_pool_peek_available (GumPagePool * self);
GUM_API guint gum_page_pool_peek_used (GumPagePool * self);
GUM_API void gum_page_pool_get_bounds (GumPagePool * self, guint8 ** lower,
    guint8 ** upper);
GUM_API gboolean gum_page_pool_query_block_details (GumPagePool * self,
    gconstpointer mem, GumBlockDetails * details);

G_END_DECLS

#endif
