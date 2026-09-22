/*
 * Copyright (C) 2020-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_MODULE_H__
#define __GUM_QUICK_MODULE_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickModule GumQuickModule;

struct _GumQuickModule
{
  GumQuickCore * core;

  JSClassID module_class;
  JSClassID module_map_class;
};

G_GNUC_INTERNAL void _gum_quick_module_init (GumQuickModule * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_module_dispose (GumQuickModule * self);
G_GNUC_INTERNAL void _gum_quick_module_finalize (GumQuickModule * self);

G_GNUC_INTERNAL JSValue _gum_quick_module_new_from_handle (JSContext * ctx,
    GumModule * handle, GumQuickModule * parent);
G_GNUC_INTERNAL JSValue _gum_quick_module_new_take_handle (JSContext * ctx,
    GumModule * handle, GumQuickModule * parent);

G_END_DECLS

#endif
