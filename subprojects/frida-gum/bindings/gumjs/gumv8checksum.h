/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CHECKSUM_H__
#define __GUM_V8_CHECKSUM_H__

#include "gumv8core.h"

struct GumV8Checksum
{
  GumV8Core * core;

  GHashTable * checksums;
};

G_GNUC_INTERNAL void _gum_v8_checksum_init (GumV8Checksum * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_checksum_realize (GumV8Checksum * self);
G_GNUC_INTERNAL void _gum_v8_checksum_dispose (GumV8Checksum * self);
G_GNUC_INTERNAL void _gum_v8_checksum_finalize (GumV8Checksum * self);

#endif
