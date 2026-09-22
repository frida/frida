/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CODE_RELOCATOR_H__
#define __GUM_QUICK_CODE_RELOCATOR_H__

#include "gumquickcodewriter.h"
#include "gumquickinstruction.h"

G_BEGIN_DECLS

typedef struct _GumQuickCodeRelocator GumQuickCodeRelocator;

struct _GumQuickCodeRelocator
{
  GumQuickCodeWriter * writer;
  GumQuickInstruction * instruction;
  GumQuickCore * core;

#include "gumquickcoderelocator-fields.inc"
};

G_GNUC_INTERNAL void _gum_quick_code_relocator_init (
    GumQuickCodeRelocator * self, JSValue ns, GumQuickCodeWriter * writer,
    GumQuickInstruction * instruction, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_code_relocator_dispose (
    GumQuickCodeRelocator * self);
G_GNUC_INTERNAL void _gum_quick_code_relocator_finalize (
    GumQuickCodeRelocator * self);

#include "gumquickcoderelocator-methods.inc"

G_END_DECLS

#endif
