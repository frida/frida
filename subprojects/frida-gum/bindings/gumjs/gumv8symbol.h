/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_SYMBOL_H__
#define __GUM_V8_SYMBOL_H__

#include "gumv8core.h"

struct GumV8Symbol
{
  GumV8Core * core;

  GHashTable * symbols;

  v8::Global<v8::FunctionTemplate> * klass;
  v8::Global<v8::Object> * template_object;
};

G_GNUC_INTERNAL void _gum_v8_symbol_init (GumV8Symbol * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_symbol_realize (GumV8Symbol * self);
G_GNUC_INTERNAL void _gum_v8_symbol_dispose (GumV8Symbol * self);
G_GNUC_INTERNAL void _gum_v8_symbol_finalize (GumV8Symbol * self);

#endif
