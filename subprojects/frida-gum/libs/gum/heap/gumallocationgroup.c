/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumallocationgroup.h"
#include "gummemory.h"

GumAllocationGroup *
gum_allocation_group_new (guint size)
{
  GumAllocationGroup * group;

  group = g_slice_new0 (GumAllocationGroup);
  group->size = size;

  return group;
}

GumAllocationGroup *
gum_allocation_group_copy (const GumAllocationGroup * group)
{
  return g_slice_dup (GumAllocationGroup, group);
}

void
gum_allocation_group_free (GumAllocationGroup * group)
{
  g_slice_free (GumAllocationGroup, group);
}

void
gum_allocation_group_list_free (GList * groups)
{
  GList * cur;

  for (cur = groups; cur != NULL; cur = cur->next)
    gum_allocation_group_free (cur->data);

  g_list_free (groups);
}
