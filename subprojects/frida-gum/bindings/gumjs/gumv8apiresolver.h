/*
 * Copyright (C) 2016-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_API_RESOLVER_H__
#define __GUM_V8_API_RESOLVER_H__

#include "gumv8object.h"

#include <gum/gumapiresolver.h>

struct GumV8ApiResolver
{
  GumV8Core * core;

  GumV8ObjectManager objects;
};

typedef GumV8Object<GumApiResolver, GumV8ApiResolver> GumV8ApiResolverObject;

G_GNUC_INTERNAL void _gum_v8_api_resolver_init (GumV8ApiResolver * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_api_resolver_realize (GumV8ApiResolver * self);
G_GNUC_INTERNAL void _gum_v8_api_resolver_dispose (GumV8ApiResolver * self);
G_GNUC_INTERNAL void _gum_v8_api_resolver_finalize (GumV8ApiResolver * self);

#endif
