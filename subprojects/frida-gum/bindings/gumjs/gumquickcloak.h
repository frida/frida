/*
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CLOAK_H__
#define __GUM_QUICK_CLOAK_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickCloak GumQuickCloak;

struct _GumQuickCloak
{
  GumQuickCore * core;
};

G_GNUC_INTERNAL void _gum_quick_cloak_init (GumQuickCloak * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_cloak_dispose (GumQuickCloak * self);
G_GNUC_INTERNAL void _gum_quick_cloak_finalize (GumQuickCloak * self);

G_END_DECLS

#endif
