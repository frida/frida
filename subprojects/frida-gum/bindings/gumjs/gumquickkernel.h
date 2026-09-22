/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_KERNEL_H__
#define __GUM_QUICK_KERNEL_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickKernel GumQuickKernel;

struct _GumQuickKernel
{
  GumQuickCore * core;
};

G_GNUC_INTERNAL void _gum_quick_kernel_init (GumQuickKernel * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_kernel_dispose (GumQuickKernel * self);
G_GNUC_INTERNAL void _gum_quick_kernel_finalize (GumQuickKernel * self);

G_END_DECLS

#endif
