/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_COBJECT_H__
#define __GUM_COBJECT_H__

#include <gum/gumdefs.h>
#include <gum/gumreturnaddress.h>

typedef struct _GumCObject GumCObject;

struct _GumCObject
{
  gpointer address;
  gchar type_name[GUM_MAX_TYPE_NAME + 1];
  GumReturnAddressArray return_addresses;

  /*< private */
  gpointer data;
};

#define GUM_COBJECT(b) ((GumCObject *) (b))

G_BEGIN_DECLS

GUM_API GumCObject * gum_cobject_new (gpointer address,
    const gchar * type_name);
GUM_API GumCObject * gum_cobject_copy (
    const GumCObject * cobject);
GUM_API void gum_cobject_free (GumCObject * cobject);

GUM_API void gum_cobject_list_free (GList * cobject_list);

G_END_DECLS

#endif
