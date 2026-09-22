/*
 * Copyright (C) 2016-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_API_RESOLVER_H__
#define __GUM_API_RESOLVER_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

#define GUM_API_SIZE_NONE -1

#define GUM_TYPE_API_RESOLVER (gum_api_resolver_get_type ())
G_DECLARE_INTERFACE (GumApiResolver, gum_api_resolver, GUM, API_RESOLVER,
                     GObject)

typedef struct _GumApiDetails GumApiDetails;

typedef gboolean (* GumFoundApiFunc) (const GumApiDetails * details,
    gpointer user_data);

struct _GumApiResolverInterface
{
  GTypeInterface parent;

  void (* enumerate_matches) (GumApiResolver * self, const gchar * query,
      GumFoundApiFunc func, gpointer user_data, GError ** error);
};

struct _GumApiDetails
{
  const gchar * name;
  GumAddress address;
  gssize size;
};

GUM_API GumApiResolver * gum_api_resolver_make (const gchar * type);

GUM_API void gum_api_resolver_enumerate_matches (GumApiResolver * self,
    const gchar * query, GumFoundApiFunc func, gpointer user_data,
    GError ** error);

G_END_DECLS

#endif
