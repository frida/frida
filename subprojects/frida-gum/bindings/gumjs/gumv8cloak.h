/*
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CLOAK_H__
#define __GUM_V8_CLOAK_H__

#include "gumv8core.h"

struct GumV8Cloak
{
  GumV8Core * core;
};

G_GNUC_INTERNAL void _gum_v8_cloak_init (GumV8Cloak * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_cloak_realize (GumV8Cloak * self);
G_GNUC_INTERNAL void _gum_v8_cloak_dispose (GumV8Cloak * self);
G_GNUC_INTERNAL void _gum_v8_cloak_finalize (GumV8Cloak * self);

#endif
