/*
 * Copyright (C) 2008-2010 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_RETURN_ADDRESS_H__
#define __GUM_RETURN_ADDRESS_H__

#include <gum/gumdefs.h>

typedef struct _GumReturnAddressDetails GumReturnAddressDetails;
typedef gpointer GumReturnAddress;
typedef struct _GumReturnAddressArray GumReturnAddressArray;

struct _GumReturnAddressDetails
{
  GumReturnAddress address;
  gchar module_name[GUM_MAX_PATH + 1];
  gchar function_name[GUM_MAX_SYMBOL_NAME + 1];
  gchar file_name[GUM_MAX_PATH + 1];
  guint line_number;
  guint column;
};

struct _GumReturnAddressArray
{
  guint len;
  GumReturnAddress items[GUM_MAX_BACKTRACE_DEPTH];
};

G_BEGIN_DECLS

GUM_API gboolean gum_return_address_details_from_address (
    GumReturnAddress address, GumReturnAddressDetails * details);

GUM_API gboolean gum_return_address_array_is_equal (
    const GumReturnAddressArray * array1,
    const GumReturnAddressArray * array2);

G_END_DECLS

#endif
