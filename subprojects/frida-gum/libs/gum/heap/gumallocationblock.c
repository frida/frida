/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumallocationblock.h"

#include "gummemory.h"
#include "gumreturnaddress.h"

#include <string.h>

GumAllocationBlock *
gum_allocation_block_new (gpointer address,
                          guint size)
{
  GumAllocationBlock * block;

  block = g_slice_new (GumAllocationBlock);
  block->address = address;
  block->size = size;
  block->return_addresses.len = 0;

  return block;
}

GumAllocationBlock *
gum_allocation_block_copy (const GumAllocationBlock * block)
{
  return g_slice_dup (GumAllocationBlock, block);
}

void
gum_allocation_block_free (GumAllocationBlock * block)
{
  g_slice_free (GumAllocationBlock, block);
}

void
gum_allocation_block_list_free (GList * block_list)
{
  GList * cur;

  for (cur = block_list; cur != NULL; cur = cur->next)
  {
    GumAllocationBlock * block = cur->data;
    gum_allocation_block_free (block);
  }

  g_list_free (block_list);
}
