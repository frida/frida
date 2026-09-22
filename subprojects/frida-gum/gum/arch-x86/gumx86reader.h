/*
 * Copyright (C) 2009 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_X86_READER_H__
#define __GUM_X86_READER_H__

#include "gumdefs.h"

#include <capstone.h>

G_BEGIN_DECLS

GUM_API guint gum_x86_reader_insn_length (guint8 * code);
GUM_API gboolean gum_x86_reader_insn_is_jcc (const cs_insn * insn);

GUM_API gpointer gum_x86_reader_find_next_call_target (gconstpointer address);
GUM_API gpointer gum_x86_reader_try_get_relative_call_target (
    gconstpointer address);
GUM_API gpointer gum_x86_reader_try_get_relative_jump_target (
    gconstpointer address);
GUM_API gpointer gum_x86_reader_try_get_indirect_jump_target (
    gconstpointer address);
GUM_API cs_insn * gum_x86_reader_disassemble_instruction_at (
    gconstpointer address);

G_END_DECLS

#endif
