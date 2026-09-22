/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarm64reader.h"

#include <capstone.h>

static gboolean gum_is_bl_imm (guint32 insn);

gpointer
gum_arm64_reader_find_next_bl_target (gconstpointer address)
{
  const guint32 * cursor = address;

  do
  {
    guint32 insn = *cursor;

    if (gum_is_bl_imm (insn))
    {
      union
      {
        gint32 i;
        guint32 u;
      } distance;

      distance.u = insn & GUM_INT26_MASK;
      if ((distance.u & (1 << (26 - 1))) != 0)
        distance.u |= 0xfc000000;

      return (gpointer) (cursor + distance.i);
    }

    cursor++;
  }
  while (TRUE);
}

gpointer
gum_arm64_reader_try_get_relative_jump_target (gconstpointer address)
{
  gpointer result = NULL;
  csh capstone;
  cs_insn * insn;
  const uint8_t * code;
  size_t size;
  uint64_t pc;
  const cs_arm64_op * ops;

  cs_arch_register_arm64 ();
  cs_open (CS_ARCH_ARM64, GUM_DEFAULT_CS_ENDIAN, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  insn = cs_malloc (capstone);

  code = address;
  size = 16;
  pc = GPOINTER_TO_SIZE (address);

#define GUM_DISASM_NEXT() \
    if (!cs_disasm_iter (capstone, &code, &size, &pc, insn)) \
      goto beach; \
    ops = insn->detail->arm64.operands
#define GUM_CHECK_ID(i) \
    if (insn->id != G_PASTE (ARM64_INS_, i)) \
      goto beach
#define GUM_CHECK_OP_TYPE(n, t) \
    if (ops[n].type != G_PASTE (ARM64_OP_, t)) \
      goto beach
#define GUM_CHECK_OP_REG(n, r) \
    if (ops[n].reg != G_PASTE (ARM64_REG_, r)) \
      goto beach
#define GUM_CHECK_OP_MEM(n, b, i, d) \
    if (ops[n].mem.base != G_PASTE (ARM64_REG_, b)) \
      goto beach; \
    if (ops[n].mem.index != G_PASTE (ARM64_REG_, i)) \
      goto beach; \
    if (ops[n].mem.disp != d) \
      goto beach

  GUM_DISASM_NEXT ();

  switch (insn->id)
  {
    case ARM64_INS_B:
      result = GSIZE_TO_POINTER (ops[0].imm);
      break;
#ifdef HAVE_DARWIN
    case ARM64_INS_ADRP:
    {
      GumAddress target;

      GUM_CHECK_OP_REG (0, X17);
      target = ops[1].imm;

      GUM_DISASM_NEXT ();
      GUM_CHECK_ID (ADD);
      GUM_CHECK_OP_REG (0, X17);
      GUM_CHECK_OP_REG (1, X17);
      GUM_CHECK_OP_TYPE (2, IMM);
      target += ops[2].imm;

      GUM_DISASM_NEXT ();
      GUM_CHECK_ID (LDR);
      GUM_CHECK_OP_REG (0, X16);
      GUM_CHECK_OP_TYPE (1, MEM);
      GUM_CHECK_OP_MEM (1, X17, INVALID, 0);

      GUM_DISASM_NEXT ();
      GUM_CHECK_ID (BRAA);
      GUM_CHECK_OP_REG (0, X16);
      GUM_CHECK_OP_REG (1, X17);

      result = *((gpointer *) GSIZE_TO_POINTER (target));

      break;
    }
#endif
    default:
      break;
  }

beach:
  cs_free (insn, 1);

  cs_close (&capstone);

  return result;
}

cs_insn *
gum_arm64_reader_disassemble_instruction_at (gconstpointer address)
{
  csh capstone;
  cs_insn * insn = NULL;

  cs_arch_register_arm64 ();
  cs_open (CS_ARCH_ARM64, GUM_DEFAULT_CS_ENDIAN, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  cs_disasm (capstone, address, 16, GPOINTER_TO_SIZE (address), 1, &insn);

  cs_close (&capstone);

  return insn;
}

static gboolean
gum_is_bl_imm (guint32 insn)
{
  return (insn & ~GUM_INT26_MASK) == 0x94000000;
}
