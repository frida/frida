/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_API_H__
#define __GUM_V8_API_H__

#include "gumv8core.h"

struct GumV8Api
{
  GumV8Core * core;

  GPtrArray * apis;
  GPtrArray * entries;
};

G_GNUC_INTERNAL void _gum_v8_api_init (GumV8Api * self, GumV8Core * core,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_api_realize (GumV8Api * self);
G_GNUC_INTERNAL void _gum_v8_api_finalize (GumV8Api * self);

#endif
