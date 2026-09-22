/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_PROFILER_H__
#define __GUM_QUICK_PROFILER_H__

#include "gumquickinterceptor.h"
#include "gumquicksampler.h"

G_BEGIN_DECLS

typedef struct _GumQuickProfiler GumQuickProfiler;

struct _GumQuickProfiler
{
  GumQuickSampler * sampler;
  GumQuickInterceptor * interceptor;
  GumQuickCore * core;

  JSClassID profiler_class;
};

G_GNUC_INTERNAL void _gum_quick_profiler_init (GumQuickProfiler * self,
    JSValue ns, GumQuickSampler * sampler, GumQuickInterceptor * interceptor,
    GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_profiler_dispose (GumQuickProfiler * self);
G_GNUC_INTERNAL void _gum_quick_profiler_finalize (GumQuickProfiler * self);

G_END_DECLS

#endif
