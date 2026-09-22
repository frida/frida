/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarmrelocator.h"

#include "gummemory.h"

#define GUM_MAX_INPUT_INSN_COUNT (100)

typedef struct _GumCodeGenCtx GumCodeGenCtx;

struct _GumCodeGenCtx
{
  const cs_insn * insn;
  cs_arm * detail;
  GumAddress pc;

  GumArmWriter * output;
};

static gboolean gum_arm_branch_is_unconditional (const cs_insn * insn);
static gboolean gum_reg_dest_is_pc (const cs_insn * insn);
static gboolean gum_reg_list_contains_pc (const cs_insn * insn,
    guint8 start_index);

static gboolean gum_arm_relocator_rewrite_ldr (GumArmRelocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm_relocator_rewrite_mov (GumArmRelocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm_relocator_rewrite_add (GumArmRelocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm_relocator_rewrite_sub (GumArmRelocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm_relocator_rewrite_b (GumArmRelocator * self,
    cs_mode target_mode, GumCodeGenCtx * ctx);
static gboolean gum_arm_relocator_rewrite_b_cond (GumArmRelocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm_relocator_rewrite_bl (GumArmRelocator * self,
    cs_mode target_mode, GumCodeGenCtx * ctx);

GumArmRelocator *
gum_arm_relocator_new (gconstpointer input_code,
                       GumArmWriter * output)
{
  GumArmRelocator * relocator;

  relocator = g_slice_new (GumArmRelocator);

  gum_arm_relocator_init (relocator, input_code, output);

  return relocator;
}

GumArmRelocator *
gum_arm_relocator_ref (GumArmRelocator * relocator)
{
  g_atomic_int_inc (&relocator->ref_count);

  return relocator;
}

void
gum_arm_relocator_unref (GumArmRelocator * relocator)
{
  if (g_atomic_int_dec_and_test (&relocator->ref_count))
  {
    gum_arm_relocator_clear (relocator);

    g_slice_free (GumArmRelocator, relocator);
  }
}

void
gum_arm_relocator_init (GumArmRelocator * relocator,
                        gconstpointer input_code,
                        GumArmWriter * output)
{
  relocator->ref_count = 1;

  cs_arch_register_arm ();
  cs_open (CS_ARCH_ARM, CS_MODE_ARM | CS_MODE_V8, &relocator->capstone);
  cs_option (relocator->capstone, CS_OPT_DETAIL, CS_OPT_ON);
  relocator->input_insns = g_new0 (cs_insn *, GUM_MAX_INPUT_INSN_COUNT);

  relocator->output = NULL;

  gum_arm_relocator_reset (relocator, input_code, output);
}

void
gum_arm_relocator_clear (GumArmRelocator * relocator)
{
  guint i;

  gum_arm_relocator_reset (relocator, NULL, NULL);

  for (i = 0; i != GUM_MAX_INPUT_INSN_COUNT; i++)
  {
    cs_insn * insn = relocator->input_insns[i];
    if (insn != NULL)
    {
      cs_free (insn, 1);
      relocator->input_insns[i] = NULL;
    }
  }
  g_free (relocator->input_insns);

  cs_close (&relocator->capstone);
}

void
gum_arm_relocator_reset (GumArmRelocator * relocator,
                         gconstpointer input_code,
                         GumArmWriter * output)
{
  relocator->input_start = input_code;
  relocator->input_cur = input_code;
  relocator->input_pc = GUM_ADDRESS (input_code);

  if (output != NULL)
    gum_arm_writer_ref (output);
  if (relocator->output != NULL)
    gum_arm_writer_unref (relocator->output);
  relocator->output = output;

  relocator->inpos = 0;
  relocator->outpos = 0;

  relocator->eob = FALSE;
  relocator->eoi = FALSE;
}

static guint
gum_arm_relocator_inpos (GumArmRelocator * self)
{
  return self->inpos % GUM_MAX_INPUT_INSN_COUNT;
}

static guint
gum_arm_relocator_outpos (GumArmRelocator * self)
{
  return self->outpos % GUM_MAX_INPUT_INSN_COUNT;
}

static void
gum_arm_relocator_increment_inpos (GumArmRelocator * self)
{
  self->inpos++;
  g_assert (self->inpos > self->outpos);
}

static void
gum_arm_relocator_increment_outpos (GumArmRelocator * self)
{
  self->outpos++;
  g_assert (self->outpos <= self->inpos);
}

guint
gum_arm_relocator_read_one (GumArmRelocator * self,
                            const cs_insn ** instruction)
{
  cs_insn ** insn_ptr, * insn;
  const uint8_t * code;
  size_t size;
  uint64_t address;

  if (self->eoi)
    return 0;

  insn_ptr = &self->input_insns[gum_arm_relocator_inpos (self)];

  if (*insn_ptr == NULL)
    *insn_ptr = cs_malloc (self->capstone);

  code = self->input_cur;
  size = 4;
  address = self->input_pc;
  insn = *insn_ptr;

  if (!cs_disasm_iter (self->capstone, &code, &size, &address, insn))
    return 0;

  switch (insn->id)
  {
    case ARM_INS_B:
    case ARM_INS_BX:
      self->eob = TRUE;
      self->eoi = gum_arm_branch_is_unconditional (insn);
      break;
    case ARM_INS_BL:
    case ARM_INS_BLX:
      self->eob = TRUE;
      self->eoi = FALSE;
      break;
    case ARM_INS_LDR:
    case ARM_INS_MOV:
    case ARM_INS_ADD:
    case ARM_INS_SUB:
      self->eob = self->eoi = gum_reg_dest_is_pc (insn);
      break;
    case ARM_INS_POP:
      self->eob = self->eoi = gum_reg_list_contains_pc (insn, 0);
      break;
    case ARM_INS_LDM:
      self->eob = self->eoi = gum_reg_list_contains_pc (insn, 1);
      break;
    default:
      self->eob = FALSE;
      break;
  }

  gum_arm_relocator_increment_inpos (self);

  if (instruction != NULL)
    *instruction = insn;

  self->input_cur = code;
  self->input_pc = address;

  return self->input_cur - self->input_start;
}

cs_insn *
gum_arm_relocator_peek_next_write_insn (GumArmRelocator * self)
{
  if (self->outpos == self->inpos)
    return NULL;

  return self->input_insns[gum_arm_relocator_outpos (self)];
}

gpointer
gum_arm_relocator_peek_next_write_source (GumArmRelocator * self)
{
  cs_insn * next;

  next = gum_arm_relocator_peek_next_write_insn (self);
  if (next == NULL)
    return NULL;

  return GSIZE_TO_POINTER (next->address);
}

void
gum_arm_relocator_skip_one (GumArmRelocator * self)
{
  gum_arm_relocator_increment_outpos (self);
}

gboolean
gum_arm_relocator_write_one (GumArmRelocator * self)
{
  const cs_insn * insn;
  GumCodeGenCtx ctx;
  gboolean rewritten;

  if ((insn = gum_arm_relocator_peek_next_write_insn (self)) == NULL)
    return FALSE;
  gum_arm_relocator_increment_outpos (self);
  ctx.insn = insn;
  ctx.detail = &ctx.insn->detail->arm;
  ctx.pc = insn->address + 8;
  ctx.output = self->output;

  switch (insn->id)
  {
    case ARM_INS_LDR:
      rewritten = gum_arm_relocator_rewrite_ldr (self, &ctx);
      break;
    case ARM_INS_MOV:
      rewritten = gum_arm_relocator_rewrite_mov (self, &ctx);
      break;
    case ARM_INS_ADD:
      rewritten = gum_arm_relocator_rewrite_add (self, &ctx);
      break;
    case ARM_INS_SUB:
      rewritten = gum_arm_relocator_rewrite_sub (self, &ctx);
      break;
    case ARM_INS_B:
      if (gum_arm_branch_is_unconditional (insn))
        rewritten = gum_arm_relocator_rewrite_b (self, CS_MODE_ARM, &ctx);
      else
        rewritten = gum_arm_relocator_rewrite_b_cond (self, &ctx);
      break;
    case ARM_INS_BX:
      rewritten = gum_arm_relocator_rewrite_b (self, CS_MODE_THUMB, &ctx);
      break;
    case ARM_INS_BL:
      rewritten = gum_arm_relocator_rewrite_bl (self, CS_MODE_ARM, &ctx);
      break;
    case ARM_INS_BLX:
      rewritten = gum_arm_relocator_rewrite_bl (self, CS_MODE_THUMB, &ctx);
      break;
    default:
      rewritten = FALSE;
      break;
  }

  if (!rewritten)
    gum_arm_writer_put_bytes (ctx.output, insn->bytes, insn->size);

  return TRUE;
}

void
gum_arm_relocator_write_all (GumArmRelocator * self)
{
  G_GNUC_UNUSED guint count = 0;

  while (gum_arm_relocator_write_one (self))
    count++;

  g_assert (count > 0);
}

gboolean
gum_arm_relocator_eob (GumArmRelocator * self)
{
  return self->eob;
}

gboolean
gum_arm_relocator_eoi (GumArmRelocator * self)
{
  return self->eoi;
}

gboolean
gum_arm_relocator_can_relocate (gpointer address,
                                guint min_bytes,
                                guint * maximum)
{
  guint n = 0;
  guint8 * buf;
  GumArmWriter cw;
  GumArmRelocator rl;
  guint reloc_bytes;

  buf = g_alloca (3 * min_bytes);
  gum_arm_writer_init (&cw, buf);
  cw.cpu_features = gum_query_cpu_features ();

  gum_arm_relocator_init (&rl, address, &cw);

  do
  {
    reloc_bytes = gum_arm_relocator_read_one (&rl, NULL);
    if (reloc_bytes == 0)
      break;

    n = reloc_bytes;
  }
  while (reloc_bytes < min_bytes);

  gum_arm_relocator_clear (&rl);

  gum_arm_writer_clear (&cw);

  if (maximum != NULL)
    *maximum = n;

  return n >= min_bytes;
}

guint
gum_arm_relocator_relocate (gpointer from,
                            guint min_bytes,
                            gpointer to)
{
  GumArmWriter cw;
  GumArmRelocator rl;
  guint reloc_bytes;

  gum_arm_writer_init (&cw, to);
  cw.cpu_features = gum_query_cpu_features ();

  gum_arm_relocator_init (&rl, from, &cw);

  do
  {
    reloc_bytes = gum_arm_relocator_read_one (&rl, NULL);
    g_assert (reloc_bytes != 0);
  }
  while (reloc_bytes < min_bytes);

  gum_arm_relocator_write_all (&rl);

  gum_arm_relocator_clear (&rl);
  gum_arm_writer_clear (&cw);

  return reloc_bytes;
}

static gboolean
gum_arm_branch_is_unconditional (const cs_insn * insn)
{
  switch (insn->detail->arm.cc)
  {
    case ARM_CC_INVALID:
    case ARM_CC_AL:
      return TRUE;
    default:
      return FALSE;
  }
}

static gboolean
gum_reg_dest_is_pc (const cs_insn * insn)
{
  return insn->detail->arm.operands[0].reg == ARM_REG_PC;
}

static gboolean
gum_reg_list_contains_pc (const cs_insn * insn,
                          guint8 start_index)
{
  guint8 i;

  for (i = start_index; i < insn->detail->arm.op_count; i++)
  {
    if (insn->detail->arm.operands[i].reg == ARM_REG_PC)
      return TRUE;
  }

  return FALSE;
}

static gboolean
gum_arm_relocator_rewrite_ldr (GumArmRelocator * self,
                               GumCodeGenCtx * ctx)
{
  const cs_arm_op * dst = &ctx->detail->operands[0];
  const cs_arm_op * src = &ctx->detail->operands[1];
  arm_reg target;

  if (src->type != ARM_OP_MEM || src->mem.base != ARM_REG_PC)
    return FALSE;

  if (ctx->detail->writeback)
  {
    /* FIXME: LDR with writeback not yet supported. */
    g_assert_not_reached ();
    return FALSE;
  }

  if (dst->reg == ARM_REG_PC)
  {
    /*
     * When choosing a scratch register, we favor Rm since it is often this
     * value we wish to use to start our calculation and this avoids a register
     * move.
     *
     * If however Rm is an immediate, we choose an arbitrary register.
     */
    target = (src->mem.index != ARM_REG_INVALID) ? src->mem.index : ARM_REG_R0;

    gum_arm_writer_put_push_regs (ctx->output, 2, target, ARM_REG_PC);
  }
  else
  {
    target = dst->reg;
  }

  /* Handle 'LDR Rt, [Rn, #x]' or 'LDR Rt, [Rn, #-x]' */
  if (src->mem.index == ARM_REG_INVALID)
  {
    gum_arm_writer_put_ldr_reg_address (ctx->output, target,
        ctx->pc + src->mem.disp);
  }
  else
  {
    if (src->subtracted)
    {
      /* FIXME: 'LDR Rt, [Rn, -Rm, #x]' not yet supported. */
      gum_arm_writer_put_breakpoint (ctx->output);
      return TRUE;
    }

    /* Handle 'LDR Rt, [Rn, Rm, lsl #x]' */
    gum_arm_writer_put_mov_reg_reg_shift (ctx->output, target, src->mem.index,
        src->shift.type, src->shift.value);

    gum_arm_writer_put_add_reg_u32 (ctx->output, target, ctx->pc);
  }

  gum_arm_writer_put_ldr_reg_reg_offset (ctx->output, target, target, 0);

  if (dst->reg == ARM_REG_PC)
  {
    gum_arm_writer_put_str_reg_reg_offset (ctx->output, target, ARM_REG_SP, 4);
    gum_arm_writer_put_pop_regs (ctx->output, 2, target, ARM_REG_PC);
  }

  return TRUE;
}

static gboolean
gum_arm_relocator_rewrite_mov (GumArmRelocator * self,
                               GumCodeGenCtx * ctx)
{
  const cs_arm_op * dst = &ctx->detail->operands[0];
  const cs_arm_op * src = &ctx->detail->operands[1];

  if (src->type != ARM_OP_REG || src->reg != ARM_REG_PC)
    return FALSE;

  gum_arm_writer_put_ldr_reg_address (ctx->output, dst->reg, ctx->pc);
  return TRUE;
}

static gboolean
gum_arm_relocator_rewrite_add (GumArmRelocator * self,
                               GumCodeGenCtx * ctx)
{
  const cs_arm_op * operands = ctx->detail->operands;
  const cs_arm_op * dst = &operands[0];
  const cs_arm_op * left = &operands[1];
  const cs_arm_op * right = &operands[2];
  arm_reg target;

  if (right->type == ARM_OP_REG && right->reg == ARM_REG_PC)
  {
    const cs_arm_op * l = left;
    left = right;
    right = l;
  }

  if (left->reg != ARM_REG_PC)
    return FALSE;

  if (dst->reg == ARM_REG_PC)
  {
    /*
     * When choosing a scratch register, we favor Rm since it is often this
     * value we wish to use to start our calculation and this avoids a register
     * move.
     *
     * If however Rm is an immediate, we choose an arbitrary register.
     */
    target = (right->type == ARM_OP_REG) ? right->reg : ARM_REG_R0;

    gum_arm_writer_put_push_regs (ctx->output, 2, target, ARM_REG_PC);
  }
  else
  {
    target = dst->reg;
  }

  if (right->shift.value == 0 && ctx->detail->op_count < 4)
  {
    /*
     * We have no shift to apply, so we start our calculation with the value of
     * PC since we can store this as a literal in the code stream and reduce the
     * number of instructions we need to generate.
     */
    if (right->type == ARM_OP_IMM)
    {
      /* Handle 'ADD Rd, Rn, #x' */
      gum_arm_writer_put_ldr_reg_address (ctx->output, target, ctx->pc);
      gum_arm_writer_put_add_reg_u32 (ctx->output, target, right->imm);
    }
    else if (right->reg == dst->reg)
    {
      /*
       * Handle 'ADD Rd, Rn, Rd'. This is a special case since we cannot load PC
       * from a literal into Rd since in doing so, we lose the value of Rm which
       * we want to add on. This calculation can be simplified to just adding
       * the PC to Rd.
       */
      gum_arm_writer_put_add_reg_u32 (ctx->output, target, ctx->pc);
    }
    else
    {
      /* Handle 'ADD Rd, Rn, Rm' */
      gum_arm_writer_put_ldr_reg_address (ctx->output, target, ctx->pc);
      gum_arm_writer_put_add_reg_reg_reg (ctx->output, target, target,
          right->reg);
    }
  }
  else
  {
    /*
     * As we have a shift operation to apply, we must start by calculating this
     * value and adding on PC, as we would otherwise need a second scratch
     * register to calculate this. Note that in this case, we don't have to
     * worry if Rd == Rm since although we may be using Rd to hold the
     * intermediate result, we perform all necessary calculations on Rm before
     * we update Rd.
     */

    if (right->type == ARM_OP_IMM)
    {
      /* Handle 'ADD Rd, Rn, #x, lsl #n' */
      gum_arm_writer_put_ldr_reg_u32 (ctx->output, target, right->imm);
    }
    else
    {
      /* Handle 'ADD Rd, Rn, Rm, lsl #n' */
      gum_arm_writer_put_mov_reg_reg (ctx->output, target, right->reg);
    }

    if (ctx->detail->op_count < 4)
    {
      gum_arm_writer_put_mov_reg_reg_shift (ctx->output, target, target,
          right->shift.type, right->shift.value);
    }
    else
    {
      gum_arm_writer_put_mov_reg_reg_shift (ctx->output, target, target,
          ARM_SFT_ROR, operands[3].imm);
    }

    /*
     * Now the shifted second operand has been calculated, we can simply add the
     * PC value.
     */
    gum_arm_writer_put_add_reg_u32 (ctx->output, target, ctx->pc);
  }

  if (dst->reg == ARM_REG_PC)
  {
    gum_arm_writer_put_str_reg_reg_offset (ctx->output, target, ARM_REG_SP, 4);
    gum_arm_writer_put_pop_regs (ctx->output, 2, target, ARM_REG_PC);
  }

  return TRUE;
}

static gboolean
gum_arm_relocator_rewrite_sub (GumArmRelocator * self,
                               GumCodeGenCtx * ctx)
{
  const cs_arm_op * operands = ctx->detail->operands;
  const cs_arm_op * dst = &operands[0];
  const cs_arm_op * left = &operands[1];
  const cs_arm_op * right = &operands[2];
  gboolean pc_is_involved;
  arm_reg target;

  pc_is_involved = (left->type == ARM_OP_REG && left->reg == ARM_REG_PC) ||
      (right->type == ARM_OP_REG && right->reg == ARM_REG_PC);
  if (!pc_is_involved)
    return FALSE;

  if (dst->reg == ARM_REG_PC)
  {
    /*
     * When choosing a scratch register, we favor Rm since it is often this
     * value we wish to use to start our calculation and this avoids a register
     * move.
     *
     * If however Rm is an immediate, we choose an arbitrary register.
     */
    target = (right->type == ARM_OP_REG && right->reg != ARM_REG_PC)
        ? right->reg
        : ARM_REG_R0;

    gum_arm_writer_put_push_regs (ctx->output, 2, target, ARM_REG_PC);
  }
  else
  {
    target = dst->reg;
  }

  if (right->shift.value == 0)
  {
    /*
     * We have no shift to apply, so we start our calculation with the value of
     * PC since we can store this as a literal in the code stream and reduce the
     * number of instructions we need to generate.
     */
    if (right->type == ARM_OP_IMM)
    {
      /* Handle 'SUB Rd, PC, #x'. */
      gum_arm_writer_put_ldr_reg_address (ctx->output, target, ctx->pc);
      gum_arm_writer_put_sub_reg_u32 (ctx->output, target, right->imm);
    }
    else if (dst->reg == left->reg && left->reg == right->reg)
    {
      /* Handle 'SUB, PC, PC, PC'. */
      gum_arm_writer_put_sub_reg_reg_reg (ctx->output, target, target, target);
    }
    else if (left->reg == dst->reg)
    {
      if (left->reg == ARM_REG_PC)
      {
        /* Handle 'SUB PC, PC, Rm'. */
        gum_arm_writer_put_rsb_reg_reg_imm (ctx->output, target, target, 0);
        gum_arm_writer_put_add_reg_u32 (ctx->output, target, ctx->pc);
      }
      else
      {
        /* Handle 'SUB Rd, Rd, PC'. */
        gum_arm_writer_put_sub_reg_u32 (ctx->output, target, ctx->pc);
      }
    }
    else if (right->reg == dst->reg)
    {
      if (right->reg == ARM_REG_PC)
      {
        /* Handle 'SUB PC, Rn, PC'. */
        gum_arm_writer_put_mov_reg_reg (ctx->output, target, left->reg);
        gum_arm_writer_put_sub_reg_u32 (ctx->output, target, ctx->pc);
      }
      else
      {
        /* Handle 'SUB Rd, PC, Rd'. */
        gum_arm_writer_put_rsb_reg_reg_imm (ctx->output, target, target, 0);
        gum_arm_writer_put_add_reg_u32 (ctx->output, target, ctx->pc);
      }
    }
    else if (left->reg == ARM_REG_PC)
    {
      /* Handle 'SUB Rd, PC, Rm'. */
      gum_arm_writer_put_ldr_reg_address (ctx->output, target, ctx->pc);
      gum_arm_writer_put_sub_reg_reg_imm (ctx->output, target, right->reg, 0);
    }
    else if (right->reg == ARM_REG_PC)
    {
      /* Handle 'SUB Rd, Rn, PC'. */
      gum_arm_writer_put_ldr_reg_address (ctx->output, target, ctx->pc);
      gum_arm_writer_put_rsb_reg_reg_imm (ctx->output, target, target, 0);
      gum_arm_writer_put_add_reg_reg_imm (ctx->output, target, left->reg, 0);
    }
  }
  else
  {
    /*
     * As we have a shift operation to apply, we must start by calculating this
     * value and subtracting from PC, as we would otherwise need a second
     * scratch register to calculate this. Note that in this case, we don't have
     * to worry if Rd == Rm since although we may be using Rd to hold the
     * intermediate result, we perform all necessary calculations on Rm before
     * we update Rd.
     */
    if (right->type == ARM_OP_IMM)
    {
      /* Handle 'SUB Rd, PC, #x, lsl #n'. */
      gum_arm_writer_put_ldr_reg_u32 (ctx->output, target, right->imm);
    }
    else
    {
      /*
      * Whilst technically possible, it seems quite unlikely that anyone would
      * want to perform any shifting operations on the PC itself.
      */
      g_assert (right->reg != ARM_REG_PC);

      /* Handle 'SUB Rd, PC, Rm, lsl #n'. */
      gum_arm_writer_put_mov_reg_reg (ctx->output, target, right->reg);
    }

    gum_arm_writer_put_mov_reg_reg_shift (ctx->output, target, target,
        right->shift.type, right->shift.value);

    /*
     * Now the shifted second operand has been calculated, we can negate it and
     * add the PC value.
     */
    gum_arm_writer_put_rsb_reg_reg_imm (ctx->output, target, target, 0);
    gum_arm_writer_put_add_reg_u32 (ctx->output, target, ctx->pc);
  }

  if (dst->reg == ARM_REG_PC)
  {
    gum_arm_writer_put_str_reg_reg_offset (ctx->output, target, ARM_REG_SP, 4);
    gum_arm_writer_put_pop_regs (ctx->output, 2, target, ARM_REG_PC);
  }

  return TRUE;
}

static gboolean
gum_arm_relocator_rewrite_b (GumArmRelocator * self,
                             cs_mode target_mode,
                             GumCodeGenCtx * ctx)
{
  const cs_arm_op * target = &ctx->detail->operands[0];

  if (target->type != ARM_OP_IMM)
    return FALSE;

  gum_arm_writer_put_ldr_reg_address (ctx->output, ARM_REG_PC,
      (target_mode == CS_MODE_THUMB) ? target->imm | 1 : target->imm);
  return TRUE;
}

static gboolean
gum_arm_relocator_rewrite_b_cond (GumArmRelocator * self,
                                  GumCodeGenCtx * ctx)
{
  const cs_arm_op * target = &ctx->detail->operands[0];
  gsize unique_id = GPOINTER_TO_SIZE (ctx->output->code) << 1;
  gconstpointer is_true = GSIZE_TO_POINTER (unique_id | 1);
  gconstpointer is_false = GSIZE_TO_POINTER (unique_id | 0);

  if (target->type != ARM_OP_IMM)
    return FALSE;

  gum_arm_writer_put_b_cond_label (ctx->output, ctx->detail->cc, is_true);
  gum_arm_writer_put_b_label (ctx->output, is_false);

  gum_arm_writer_put_label (ctx->output, is_true);
  gum_arm_writer_put_push_regs (ctx->output, 1, ARM_REG_R0);
  gum_arm_writer_put_push_regs (ctx->output, 1, ARM_REG_R0);
  gum_arm_writer_put_ldr_reg_address (ctx->output, ARM_REG_R0,
      target->imm);
  gum_arm_writer_put_str_reg_reg_offset (ctx->output, ARM_REG_R0,
      ARM_REG_SP, 4);
  gum_arm_writer_put_pop_regs (ctx->output, 2, ARM_REG_R0, ARM_REG_PC);

  gum_arm_writer_put_label (ctx->output, is_false);

  return TRUE;
}

static gboolean
gum_arm_relocator_rewrite_bl (GumArmRelocator * self,
                              cs_mode target_mode,
                              GumCodeGenCtx * ctx)
{
  const cs_arm_op * target = &ctx->detail->operands[0];

  if (target->type != ARM_OP_IMM)
    return FALSE;

  gum_arm_writer_put_ldr_reg_address (ctx->output, ARM_REG_LR,
      ctx->output->pc + (2 * 4));
  gum_arm_writer_put_ldr_reg_address (ctx->output, ARM_REG_PC,
      (target_mode == CS_MODE_THUMB) ? target->imm | 1 : target->imm);
  return TRUE;
}
