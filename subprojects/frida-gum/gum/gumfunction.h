/*
 * Copyright (C) 2009 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_FUNCTION_H__
#define __GUM_FUNCTION_H__

G_BEGIN_DECLS

typedef struct _GumFunctionDetails  GumFunctionDetails;

struct _GumFunctionDetails
{
  const gchar * name;
  gpointer address;
  gint num_arguments;
};

G_END_DECLS

#endif
