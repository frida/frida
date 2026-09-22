/*
 * Copyright (C) 2020-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_INSTRUCTION_H__
#define __GUM_QUICK_INSTRUCTION_H__

#include "gumquickcore.h"

#include <capstone.h>

G_BEGIN_DECLS

typedef struct _GumQuickInstruction GumQuickInstruction;
typedef struct _GumQuickInstructionValue GumQuickInstructionValue;

struct _GumQuickInstruction
{
  GumQuickCore * core;

  csh capstone;

  JSClassID instruction_class;
};

struct _GumQuickInstructionValue
{
  JSValue wrapper;
  const cs_insn * insn;
  gboolean owns_memory;
  gconstpointer target;
};

G_GNUC_INTERNAL void _gum_quick_instruction_init (GumQuickInstruction * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_instruction_dispose (
    GumQuickInstruction * self);
G_GNUC_INTERNAL void _gum_quick_instruction_finalize (
    GumQuickInstruction * self);

G_GNUC_INTERNAL JSValue _gum_quick_instruction_new (JSContext * ctx,
    const cs_insn * insn, gboolean is_owned, gconstpointer target, csh capstone,
    GumQuickInstruction * parent, GumQuickInstructionValue ** instruction);
G_GNUC_INTERNAL gboolean _gum_quick_instruction_get (JSContext * ctx,
    JSValue val, GumQuickInstruction * parent,
    GumQuickInstructionValue ** instruction);

G_END_DECLS

#endif
