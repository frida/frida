/*
 * Copyright (C) 2015 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_SYMBOL_H__
#define __GUM_QUICK_SYMBOL_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickSymbol GumQuickSymbol;

struct _GumQuickSymbol
{
  GumQuickCore * core;

  JSClassID symbol_class;
};

G_GNUC_INTERNAL void _gum_quick_symbol_init (GumQuickSymbol * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_symbol_dispose (GumQuickSymbol * self);
G_GNUC_INTERNAL void _gum_quick_symbol_finalize (GumQuickSymbol * self);

G_END_DECLS

#endif
