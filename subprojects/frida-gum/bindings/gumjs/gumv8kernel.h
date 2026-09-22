/*
 * Copyright (C) 2015-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_KERNEL_H__
#define __GUM_V8_KERNEL_H__

#include "gumv8core.h"

struct GumV8Kernel
{
  GumV8Core * core;
};

G_GNUC_INTERNAL void _gum_v8_kernel_init (GumV8Kernel * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_kernel_realize (GumV8Kernel * self);
G_GNUC_INTERNAL void _gum_v8_kernel_dispose (GumV8Kernel * self);
G_GNUC_INTERNAL void _gum_v8_kernel_finalize (GumV8Kernel * self);

#endif
