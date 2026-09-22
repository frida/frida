/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_API_H__
#define __GUM_QUICK_API_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickApi GumQuickApi;

struct _GumQuickApi
{
  GumQuickCore * core;

  GPtrArray * apis;
  GPtrArray * entries;
};

G_GNUC_INTERNAL void _gum_quick_api_init (GumQuickApi * self, JSValue ns,
    GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_api_finalize (GumQuickApi * self);

G_END_DECLS

#endif
