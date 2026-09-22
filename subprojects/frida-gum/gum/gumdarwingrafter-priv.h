/*
 * Copyright (C) 2021-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_GRAFTER_PRIV_H__
#define __GUM_DARWIN_GRAFTER_PRIV_H__

#include "gumdarwingrafter.h"

#define GUM_DARWIN_GRAFTER_ABI_VERSION 1

#define GUM_GRAFTED_HOOK_ON_ENTER_OFFSET(h) (((h)->flags >> 17) & 0x7f)
#define GUM_GRAFTED_HOOK_ON_LEAVE_OFFSET(h) (((h)->flags >> 10) & 0x7f)
#define GUM_GRAFTED_HOOK_ON_INVOKE_OFFSET(h) (((h)->flags >> 3) & 0x7f)

#define GUM_GRAFTED_IMPORT_ON_ENTER_OFFSET(i) (((i)->flags >> 17) & 0x7f)
#define GUM_GRAFTED_IMPORT_ON_LEAVE_OFFSET(i) (((i)->flags >> 10) & 0x7f)

G_BEGIN_DECLS

typedef struct _GumGraftedHeader GumGraftedHeader;
typedef struct _GumGraftedHook GumGraftedHook;
typedef struct _GumGraftedImport GumGraftedImport;

/* FIXME: Make this portable. */
#ifdef HAVE_PACK_PRAGMA
# pragma pack (push, 1)
#endif

struct _GumGraftedHeader
{
  guint32 abi_version;
  guint32 num_hooks;
  guint32 num_imports;
  guint32 padding;
  guint64 begin_invocation;
  guint64 end_invocation;
};

struct _GumGraftedHook
{
  guint32 code_offset;
  guint32 trampoline_offset;
  guint32 flags;
  guint64 user_data;
};

struct _GumGraftedImport
{
  guint32 slot_offset;
  guint32 trampoline_offset;
  guint32 flags;
  guint64 user_data;
};

#ifdef HAVE_PACK_PRAGMA
# pragma pack (pop)
#endif

G_GNUC_INTERNAL void _gum_grafted_hook_activate (GumGraftedHook * self);
G_GNUC_INTERNAL void _gum_grafted_hook_deactivate (GumGraftedHook * self);

G_END_DECLS

#endif
