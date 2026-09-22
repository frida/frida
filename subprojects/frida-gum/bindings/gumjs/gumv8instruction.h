/*
 * Copyright (C) 2014-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_INSTRUCTION_H__
#define __GUM_V8_INSTRUCTION_H__

#include "gumv8core.h"

#include <capstone.h>

struct GumV8Instruction
{
  GumV8Core * core;

  csh capstone;
  GHashTable * instructions;

  v8::Global<v8::FunctionTemplate> * klass;
  v8::Global<v8::Object> * template_object;
};

struct GumV8InstructionValue
{
  v8::Global<v8::Object> * object;
  const cs_insn * insn;
  gboolean owns_memory;
  gconstpointer target;

  GumV8Instruction * module;
};

G_GNUC_INTERNAL void _gum_v8_instruction_init (GumV8Instruction * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_instruction_realize (GumV8Instruction * self);
G_GNUC_INTERNAL void _gum_v8_instruction_dispose (GumV8Instruction * self);
G_GNUC_INTERNAL void _gum_v8_instruction_finalize (GumV8Instruction * self);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_instruction_new (
    csh capstone, const cs_insn * insn, gboolean is_owned, gconstpointer target,
    GumV8Instruction * module);

G_GNUC_INTERNAL GumV8InstructionValue * _gum_v8_instruction_new_persistent (
    GumV8Instruction * module);
G_GNUC_INTERNAL void _gum_v8_instruction_release_persistent (
    GumV8InstructionValue * value);

#endif
