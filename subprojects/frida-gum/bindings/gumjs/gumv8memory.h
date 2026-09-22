/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_MEMORY_H__
#define __GUM_V8_MEMORY_H__

#include "gumv8core.h"

#include <gum/gummemoryaccessmonitor.h>

struct GumV8Memory
{
  GumV8Core * core;

  GumMemoryAccessMonitor * monitor;
  v8::Global<v8::Function> * on_access;
};

G_GNUC_INTERNAL void _gum_v8_memory_init (GumV8Memory * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_memory_realize (GumV8Memory * self);
G_GNUC_INTERNAL void _gum_v8_memory_dispose (GumV8Memory * self);
G_GNUC_INTERNAL void _gum_v8_memory_finalize (GumV8Memory * self);

#endif
