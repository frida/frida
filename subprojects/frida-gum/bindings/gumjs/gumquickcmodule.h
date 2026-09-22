/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CMODULE_H__
#define __GUM_QUICK_CMODULE_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickCModule GumQuickCModule;

struct _GumQuickCModule
{
  GumQuickCore * core;

  GHashTable * cmodules;

  JSClassID cmodule_class;
};

G_GNUC_INTERNAL void _gum_quick_cmodule_init (GumQuickCModule * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_cmodule_dispose (GumQuickCModule * self);
G_GNUC_INTERNAL void _gum_quick_cmodule_finalize (GumQuickCModule * self);

G_END_DECLS

#endif
