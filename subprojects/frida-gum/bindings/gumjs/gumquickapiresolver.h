/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_API_RESOLVER_H__
#define __GUM_QUICK_API_RESOLVER_H__

#include "gumquickobject.h"

G_BEGIN_DECLS

typedef struct _GumQuickApiResolver GumQuickApiResolver;

struct _GumQuickApiResolver
{
  GumQuickCore * core;

  GumQuickObjectManager objects;

  JSClassID api_resolver_class;
};

G_GNUC_INTERNAL void _gum_quick_api_resolver_init (GumQuickApiResolver * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_api_resolver_dispose (
    GumQuickApiResolver * self);
G_GNUC_INTERNAL void _gum_quick_api_resolver_finalize (
    GumQuickApiResolver * self);

G_END_DECLS

#endif
