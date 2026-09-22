/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickcodewriter.h"

#include "gumquickmacros.h"

static GumQuickCodeWriter * gumjs_get_parent_module (GumQuickCore * core);

#include "gumquickcodewriter.inc"

void
_gum_quick_code_writer_init (GumQuickCodeWriter * self,
                             JSValue ns,
                             GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "code-writer", self);

#include "gumquickcodewriter-init.inc"
}

void
_gum_quick_code_writer_dispose (GumQuickCodeWriter * self)
{
  JSContext * ctx = self->core->ctx;

#include "gumquickcodewriter-dispose.inc"
}

void
_gum_quick_code_writer_finalize (GumQuickCodeWriter * self)
{
}

static GumQuickCodeWriter *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "code-writer");
}
