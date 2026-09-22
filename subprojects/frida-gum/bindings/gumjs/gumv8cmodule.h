/*
 * Copyright (C) 2019-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CMODULE_H__
#define __GUM_V8_CMODULE_H__

#include "gumv8core.h"

struct GumV8CModule
{
  GumV8Core * core;

  GHashTable * cmodules;

  v8::Global<v8::String> * toolchain_key;
};

G_GNUC_INTERNAL void _gum_v8_cmodule_init (GumV8CModule * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_cmodule_realize (GumV8CModule * self);
G_GNUC_INTERNAL void _gum_v8_cmodule_dispose (GumV8CModule * self);
G_GNUC_INTERNAL void _gum_v8_cmodule_finalize (GumV8CModule * self);

#endif
