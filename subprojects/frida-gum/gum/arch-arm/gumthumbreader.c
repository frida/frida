/*
 * Copyright (C) 2015-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthumbreader.h"

#include <capstone.h>

gpointer
gum_thumb_reader_try_get_relative_jump_target (gconstpointer address)
{
  gpointer result = NULL;
  cs_insn * insn;
  cs_arm_op * op;

  insn = gum_thumb_reader_disassemble_instruction_at (address);
  if (insn == NULL)
    return NULL;

  op = &insn->detail->arm.operands[0];
  if (insn->id == ARM_INS_B && op->type == ARM_OP_IMM)
    result = GSIZE_TO_POINTER (op->imm | 1);
  else if (insn->id == ARM_INS_BX && op->type == ARM_OP_IMM)
    result = GSIZE_TO_POINTER (op->imm);

  cs_free (insn, 1);

  return result;
}

cs_insn *
gum_thumb_reader_disassemble_instruction_at (gconstpointer address)
{
  gconstpointer code = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (address) & ~1);
  csh capstone;
  cs_insn * insn = NULL;

  cs_arch_register_arm ();
  cs_open (CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_V8, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  cs_disasm (capstone, code, 16, GPOINTER_TO_SIZE (code), 1, &insn);

  cs_close (&capstone);

  return insn;
}
