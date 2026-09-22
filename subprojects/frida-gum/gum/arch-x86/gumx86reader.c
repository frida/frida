/*
 * Copyright (C) 2009-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumx86reader.h"

static gpointer try_get_relative_call_or_jump_target (gconstpointer address,
    guint call_or_jump);

guint
gum_x86_reader_insn_length (guint8 * code)
{
  guint result;
  cs_insn * insn;

  insn = gum_x86_reader_disassemble_instruction_at (code);
  if (insn == NULL)
    return 0;
  result = insn->size;
  cs_free (insn, 1);

  return result;
}

gboolean
gum_x86_reader_insn_is_jcc (const cs_insn * insn)
{
  switch (insn->id)
  {
    case X86_INS_JA:
    case X86_INS_JAE:
    case X86_INS_JB:
    case X86_INS_JBE:
    case X86_INS_JE:
    case X86_INS_JG:
    case X86_INS_JGE:
    case X86_INS_JL:
    case X86_INS_JLE:
    case X86_INS_JNE:
    case X86_INS_JNO:
    case X86_INS_JNP:
    case X86_INS_JNS:
    case X86_INS_JO:
    case X86_INS_JP:
    case X86_INS_JS:
      return TRUE;

    default:
      break;
  }

  return FALSE;
}

gpointer
gum_x86_reader_find_next_call_target (gconstpointer address)
{
  gpointer result = NULL;
  csh capstone;
  const uint8_t * code;
  size_t size;
  cs_insn * insn;
  uint64_t addr;

  cs_arch_register_x86 ();
  cs_open (CS_ARCH_X86, GUM_CPU_MODE, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  code = address;
  size = 1024;
  addr = GPOINTER_TO_SIZE (address);

  insn = cs_malloc (capstone);

  while (cs_disasm_iter (capstone, &code, &size, &addr, insn))
  {
    if (insn->id == X86_INS_CALL)
    {
      result = GSIZE_TO_POINTER (insn->detail->x86.operands[0].imm);
      break;
    }
  }

  cs_free (insn, 1);

  cs_close (&capstone);

  return result;
}

gpointer
gum_x86_reader_try_get_relative_call_target (gconstpointer address)
{
  return try_get_relative_call_or_jump_target (address, X86_INS_CALL);
}

gpointer
gum_x86_reader_try_get_relative_jump_target (gconstpointer address)
{
  return try_get_relative_call_or_jump_target (address, X86_INS_JMP);
}

gpointer
gum_x86_reader_try_get_indirect_jump_target (gconstpointer address)
{
  gpointer result = NULL;
  cs_insn * insn;
  cs_x86_op * op;

  insn = gum_x86_reader_disassemble_instruction_at (address);
  if (insn == NULL)
    return NULL;

  op = &insn->detail->x86.operands[0];
  if (insn->id == X86_INS_JMP && op->type == X86_OP_MEM)
  {
    if (op->mem.base == X86_REG_RIP && op->mem.index == X86_REG_INVALID)
    {
      result = *((gpointer *) ((guint8 *) address + insn->size + op->mem.disp));
    }
    else if (op->mem.base == X86_REG_INVALID &&
        op->mem.index == X86_REG_INVALID)
    {
      result = *((gpointer *) GSIZE_TO_POINTER (op->mem.disp));
    }
  }

  cs_free (insn, 1);

  return result;
}

static gpointer
try_get_relative_call_or_jump_target (gconstpointer address,
                                      guint call_or_jump)
{
  gpointer result = NULL;
  cs_insn * insn;
  cs_x86_op * op;

  insn = gum_x86_reader_disassemble_instruction_at (address);
  if (insn == NULL)
    return NULL;

  op = &insn->detail->x86.operands[0];
  if (insn->id == call_or_jump && op->type == X86_OP_IMM)
    result = GSIZE_TO_POINTER (op->imm);

  cs_free (insn, 1);

  return result;
}

cs_insn *
gum_x86_reader_disassemble_instruction_at (gconstpointer address)
{
  csh capstone;
  cs_insn * insn = NULL;

  cs_arch_register_x86 ();
  cs_open (CS_ARCH_X86, GUM_CPU_MODE, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  cs_disasm (capstone, address, 16, GPOINTER_TO_SIZE (address), 1, &insn);

  cs_close (&capstone);

  return insn;
}
