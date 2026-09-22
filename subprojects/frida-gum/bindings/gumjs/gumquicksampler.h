/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_SAMPLER_H__
#define __GUM_QUICK_SAMPLER_H__

#include "gumquickcore.h"

#include <gum/gum-prof.h>

G_BEGIN_DECLS

typedef struct _GumQuickSampler GumQuickSampler;

struct _GumQuickSampler
{
  GumQuickCore * core;

  JSClassID sampler_class;
  JSClassID cycle_sampler_class;
  JSClassID busy_cycle_sampler_class;
  JSClassID wall_clock_sampler_class;
  JSClassID user_time_sampler_class;
  JSClassID malloc_count_sampler_class;
  JSClassID call_count_sampler_class;
};

G_GNUC_INTERNAL void _gum_quick_sampler_init (GumQuickSampler * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_sampler_dispose (GumQuickSampler * self);
G_GNUC_INTERNAL void _gum_quick_sampler_finalize (GumQuickSampler * self);

G_GNUC_INTERNAL gboolean _gum_quick_sampler_get (JSContext * ctx, JSValue val,
    GumQuickSampler * parent, GumSampler ** sampler);

G_END_DECLS

#endif
