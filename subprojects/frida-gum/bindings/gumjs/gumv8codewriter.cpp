/*
 * Copyright (C) 2017-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8codewriter.h"

#include "gumv8macros.h"

#define GUMJS_MODULE_NAME CodeWriter

using namespace v8;

#include "gumv8codewriter.inc"

void
_gum_v8_code_writer_init (GumV8CodeWriter * self,
                          GumV8Core * core,
                          Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

#include "gumv8codewriter-init.inc"
}

void
_gum_v8_code_writer_realize (GumV8CodeWriter * self)
{
}

void
_gum_v8_code_writer_dispose (GumV8CodeWriter * self)
{
#include "gumv8codewriter-dispose.inc"
}

void
_gum_v8_code_writer_finalize (GumV8CodeWriter * self)
{
}
