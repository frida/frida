/*
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ARM_READER_H__
#define __GUM_ARM_READER_H__

#include "gumdefs.h"

#include <capstone.h>

G_BEGIN_DECLS

gpointer gum_arm_reader_try_get_relative_jump_target (gconstpointer address);
gpointer gum_arm_reader_try_get_indirect_jump_target (gconstpointer address);
cs_insn * gum_arm_reader_disassemble_instruction_at (gconstpointer address);

G_END_DECLS

#endif
