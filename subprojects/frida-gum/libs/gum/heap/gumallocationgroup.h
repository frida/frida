/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ALLOCATION_GROUP_H__
#define __GUM_ALLOCATION_GROUP_H__

#include <gum/gumdefs.h>

typedef struct _GumAllocationGroup GumAllocationGroup;

struct _GumAllocationGroup
{
  guint size;
  guint alive_now;
  guint alive_peak;
  guint total_peak;
};

G_BEGIN_DECLS

GUM_API GumAllocationGroup * gum_allocation_group_new (guint size);
GUM_API GumAllocationGroup * gum_allocation_group_copy (
    const GumAllocationGroup * group);
GUM_API void gum_allocation_group_free (GumAllocationGroup * group);

GUM_API void gum_allocation_group_list_free (GList * groups);

G_END_DECLS

#endif
