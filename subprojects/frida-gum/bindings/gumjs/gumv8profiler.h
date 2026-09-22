/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_PROFILER_H__
#define __GUM_V8_PROFILER_H__

#include "gumv8interceptor.h"
#include "gumv8object.h"
#include "gumv8sampler.h"

struct GumV8Profiler
{
  GumV8Sampler * sampler;
  GumV8Interceptor * interceptor;
  GumV8Core * core;

  GumV8ObjectManager objects;
};

G_GNUC_INTERNAL void _gum_v8_profiler_init (GumV8Profiler * self,
    GumV8Sampler * sampler, GumV8Interceptor * interceptor, GumV8Core * core,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_profiler_realize (GumV8Profiler * self);
G_GNUC_INTERNAL void _gum_v8_profiler_flush (GumV8Profiler * self);
G_GNUC_INTERNAL void _gum_v8_profiler_dispose (GumV8Profiler * self);
G_GNUC_INTERNAL void _gum_v8_profiler_finalize (GumV8Profiler * self);

#endif
