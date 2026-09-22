/*
 * Copyright (C) 2015 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_THUMB_READER_H__
#define __GUM_THUMB_READER_H__

#include "gumdefs.h"

#include <capstone.h>

G_BEGIN_DECLS

gpointer gum_thumb_reader_try_get_relative_jump_target (gconstpointer address);
cs_insn * gum_thumb_reader_disassemble_instruction_at (gconstpointer address);

G_END_DECLS

#endif
