/*
 * Copyright (C) 2017-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CODE_WRITER_H__
#define __GUM_V8_CODE_WRITER_H__

#include "gumv8core.h"

struct GumV8CodeWriter
{
  GumV8Core * core;

#include "gumv8codewriter-fields.inc"
};

G_GNUC_INTERNAL void _gum_v8_code_writer_init (GumV8CodeWriter * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_code_writer_realize (GumV8CodeWriter * self);
G_GNUC_INTERNAL void _gum_v8_code_writer_dispose (GumV8CodeWriter * self);
G_GNUC_INTERNAL void _gum_v8_code_writer_finalize (GumV8CodeWriter * self);

#include "gumv8codewriter-methods.inc"

#endif
