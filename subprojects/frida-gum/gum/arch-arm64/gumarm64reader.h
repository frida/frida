/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ARM64_READER_H__
#define __GUM_ARM64_READER_H__

#include "gumdefs.h"

#include <capstone.h>

G_BEGIN_DECLS

GUM_API gpointer gum_arm64_reader_find_next_bl_target (gconstpointer address);
GUM_API gpointer gum_arm64_reader_try_get_relative_jump_target (
    gconstpointer address);
GUM_API cs_insn * gum_arm64_reader_disassemble_instruction_at (
    gconstpointer address);

G_END_DECLS

#endif
