/*
 * Copyright (C) 2017-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8coderelocator.h"

#include "gumv8macros.h"

#define GUMJS_MODULE_NAME CodeRelocator

using namespace v8;

#include "gumv8coderelocator.inc"

void
_gum_v8_code_relocator_init (GumV8CodeRelocator * self,
                             GumV8CodeWriter * writer,
                             GumV8Instruction * instruction,
                             GumV8Core * core,
                             Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->writer = writer;
  self->instruction = instruction;
  self->core = core;

  auto module = External::New (isolate, self);

#include "gumv8coderelocator-init.inc"
}

void
_gum_v8_code_relocator_realize (GumV8CodeRelocator * self)
{
}

void
_gum_v8_code_relocator_dispose (GumV8CodeRelocator * self)
{
#include "gumv8coderelocator-dispose.inc"
}

void
_gum_v8_code_relocator_finalize (GumV8CodeRelocator * self)
{
}
