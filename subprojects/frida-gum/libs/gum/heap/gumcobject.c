/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcobject.h"
#include "gummemory.h"
#include "gumreturnaddress.h"

#include <string.h>

GumCObject *
gum_cobject_new (gpointer address,
                 const gchar * type_name)
{
  GumCObject * cobject;

  cobject = g_slice_new (GumCObject);
  cobject->address = address;
  g_strlcpy (cobject->type_name, type_name, sizeof (cobject->type_name));
  cobject->return_addresses.len = 0;
  cobject->data = NULL;

  return cobject;
}

GumCObject *
gum_cobject_copy (const GumCObject * cobject)
{
  return g_slice_dup (GumCObject, cobject);
}

void
gum_cobject_free (GumCObject * cobject)
{
  g_slice_free (GumCObject, cobject);
}

void
gum_cobject_list_free (GList * cobject_list)
{
  GList * cur;

  for (cur = cobject_list; cur != NULL; cur = cur->next)
  {
    GumCObject * cobject = cur->data;
    gum_cobject_free (cobject);
  }

  g_list_free (cobject_list);
}
