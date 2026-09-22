/*
 * Copyright (C) 2020-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_THREAD_H__
#define __GUM_QUICK_THREAD_H__

#include "gumquickcore.h"

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

typedef struct _GumQuickThread GumQuickThread;

struct _GumQuickThread
{
  GumQuickCore * core;

  JSClassID thread_class;

  GumBacktracer * accurate_backtracer;
  GumBacktracer * fuzzy_backtracer;
};

G_GNUC_INTERNAL void _gum_quick_thread_init (GumQuickThread * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_thread_dispose (GumQuickThread * self);
G_GNUC_INTERNAL void _gum_quick_thread_finalize (GumQuickThread * self);

G_GNUC_INTERNAL JSValue _gum_quick_thread_new (JSContext * ctx,
    const GumThreadDetails * details, GumQuickThread * parent);

G_END_DECLS

#endif
