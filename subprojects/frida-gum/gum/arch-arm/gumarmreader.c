/*
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarmreader.h"

static guint gum_rotate_right_32bit (guint val, guint rotation);

gpointer
gum_arm_reader_try_get_relative_jump_target (gconstpointer address)
{
  gpointer result = NULL;
  cs_insn * insn;
  cs_arm_op * op;

  insn = gum_arm_reader_disassemble_instruction_at (address);
  if (insn == NULL)
    return NULL;

  op = &insn->detail->arm.operands[0];
  if (insn->id == ARM_INS_B && op->type == ARM_OP_IMM)
    result = GSIZE_TO_POINTER (op->imm);

  cs_free (insn, 1);

  return result;
}

gpointer
gum_arm_reader_try_get_indirect_jump_target (gconstpointer address)
{
  gpointer result = NULL;
  cs_insn * insn;
  cs_arm_op * op0;
  cs_arm_op * op1;
  cs_arm_op * op2;
  cs_arm_op * op3;

  /*
   * First instruction: add r12, pc, 0
   */
  insn = gum_arm_reader_disassemble_instruction_at (address);
  if (insn == NULL)
    return NULL;
  op0 = &insn->detail->arm.operands[0];
  op1 = &insn->detail->arm.operands[1];
  op2 = &insn->detail->arm.operands[2];
  op3 = &insn->detail->arm.operands[3];
  if (insn->id == ARM_INS_ADD &&
      op0->type == ARM_OP_REG && op0->reg == ARM_REG_R12 &&
      op1->type == ARM_OP_REG && op1->reg == ARM_REG_PC &&
      op2->type == ARM_OP_IMM)
  {
    result = (gpointer) address + 8 +
        gum_rotate_right_32bit (op2->imm, op3->imm);
  }
  else
    goto beach;

  /*
   * Second instruction: add r12, r12, 96, 20
   */
  insn = gum_arm_reader_disassemble_instruction_at (address + 4);
  op0 = &insn->detail->arm.operands[0];
  op1 = &insn->detail->arm.operands[1];
  op2 = &insn->detail->arm.operands[2];
  op3 = &insn->detail->arm.operands[3];
  if (insn->id == ARM_INS_ADD &&
      op0->type == ARM_OP_REG && op0->reg == ARM_REG_R12 &&
      op1->type == ARM_OP_REG && op1->reg == ARM_REG_R12 &&
      op2->type == ARM_OP_IMM)
  {
    if (insn->detail->arm.op_count == 4)
    {
      /*
       * I couldn't really find the documentation of WHY this
       * should be shifted by 12, but it seems to be how both
       * objdump and IDA decode.
       */
      result += (op2->imm << 12);
    }
    else
      result += op2->imm;
  }
  else
  {
    result = NULL;
    goto beach;
  }

  /*
   * Third instruction: ldr pc, [r12, x]
   */
  insn = gum_arm_reader_disassemble_instruction_at (address + 8);
  op0 = &insn->detail->arm.operands[0];
  op1 = &insn->detail->arm.operands[1];
  if (insn->id == ARM_INS_LDR &&
      op0->type == ARM_OP_REG && op0->reg == ARM_REG_PC &&
      op1->type == ARM_OP_MEM && op1->mem.base == ARM_REG_R12)
  {
    result = *((gpointer *) (result + op1->mem.disp));
  }
  else
  {
    result = NULL;
  }

beach:
  cs_free (insn, 1);

  return result;
}

cs_insn *
gum_arm_reader_disassemble_instruction_at (gconstpointer address)
{
  csh capstone;
  cs_insn * insn = NULL;

  cs_arch_register_arm ();
  cs_open (CS_ARCH_ARM, CS_MODE_ARM | CS_MODE_V8, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  cs_disasm (capstone, address, 4, GPOINTER_TO_SIZE (address), 1, &insn);

  cs_close (&capstone);

  return insn;
}

static guint
gum_rotate_right_32bit (guint val,
                        guint rotation)
{
  if (rotation == 0x0)
    return val;
  return ((val >> rotation) & (-1 << (32 - rotation))) |
      (val << (32 - rotation));
}
