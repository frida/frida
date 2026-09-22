/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ALLOCATION_BLOCK_H__
#define __GUM_ALLOCATION_BLOCK_H__

#include <gum/gumdefs.h>
#include <gum/gumreturnaddress.h>

typedef struct _GumAllocationBlock GumAllocationBlock;

struct _GumAllocationBlock
{
  gpointer address;
  guint size;
  GumReturnAddressArray return_addresses;
};

#define GUM_ALLOCATION_BLOCK(b) ((GumAllocationBlock *) (b))

G_BEGIN_DECLS

GUM_API GumAllocationBlock * gum_allocation_block_new (gpointer address,
    guint size);
GUM_API GumAllocationBlock * gum_allocation_block_copy (
    const GumAllocationBlock * block);
GUM_API void gum_allocation_block_free (GumAllocationBlock * block);

GUM_API void gum_allocation_block_list_free (GList * block_list);

G_END_DECLS

#endif
