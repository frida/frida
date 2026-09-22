/*
 * Copyright (C) 2017-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CODE_RELOCATOR_H__
#define __GUM_V8_CODE_RELOCATOR_H__

#include "gumv8codewriter.h"
#include "gumv8instruction.h"

struct GumV8CodeRelocator
{
  GumV8CodeWriter * writer;
  GumV8Instruction * instruction;
  GumV8Core * core;

#include "gumv8coderelocator-fields.inc"
};

G_GNUC_INTERNAL void _gum_v8_code_relocator_init (GumV8CodeRelocator * self,
    GumV8CodeWriter * writer, GumV8Instruction * instruction, GumV8Core * core,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_code_relocator_realize (GumV8CodeRelocator * self);
G_GNUC_INTERNAL void _gum_v8_code_relocator_dispose (GumV8CodeRelocator * self);
G_GNUC_INTERNAL void _gum_v8_code_relocator_finalize (
    GumV8CodeRelocator * self);

#include "gumv8coderelocator-methods.inc"

#endif
