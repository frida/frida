/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickcoderelocator.h"

#include "gumquickmacros.h"

static GumQuickCodeRelocator * gumjs_get_parent_module (GumQuickCore * core);

#include "gumquickcoderelocator.inc"

void
_gum_quick_code_relocator_init (GumQuickCodeRelocator * self,
                                JSValue ns,
                                GumQuickCodeWriter * writer,
                                GumQuickInstruction * instruction,
                                GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->writer = writer;
  self->instruction = instruction;
  self->core = core;

  _gum_quick_core_store_module_data (core, "code-relocator", self);

#include "gumquickcoderelocator-init.inc"
}

void
_gum_quick_code_relocator_dispose (GumQuickCodeRelocator * self)
{
  JSContext * ctx = self->core->ctx;

#include "gumquickcoderelocator-dispose.inc"
}

void
_gum_quick_code_relocator_finalize (GumQuickCodeRelocator * self)
{
}

static GumQuickCodeRelocator *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "code-relocator");
}
