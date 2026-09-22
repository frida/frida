/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Haiwei Wang <haiwei.wang1109@gmail.com>
 * Copyright (C) 2026 inforcqb <fanjiawei080615@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/* Useful reference: C4.1 A64 instruction index by encoding */

#include "gumarm64relocator.h"

#include "gummemory.h"

#define GUM_MAX_INPUT_INSN_COUNT (100)

typedef struct _GumCodeGenCtx GumCodeGenCtx;

struct _GumCodeGenCtx
{
  const cs_insn * insn;
  cs_arm64 * detail;

  GumArm64Writer * output;
};

static gboolean gum_arm64_relocator_register_is_free (
    const GumArm64Relocator * self, guint n, arm64_reg reg);
static gpointer gum_arm64_relocator_extract_branch_target (
    const cs_insn * insn);

static gboolean gum_arm64_branch_is_unconditional (const cs_insn * insn);

static gboolean gum_arm64_relocator_rewrite_ldr (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm64_relocator_rewrite_adr (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm64_relocator_rewrite_b (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm64_relocator_rewrite_b_cond (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm64_relocator_rewrite_bl (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm64_relocator_rewrite_cbz (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);
static gboolean gum_arm64_relocator_rewrite_tbz (GumArm64Relocator * self,
    GumCodeGenCtx * ctx);

GumArm64Relocator *
gum_arm64_relocator_new (gconstpointer input_code,
                         GumArm64Writer * output)
{
  GumArm64Relocator * relocator;

  relocator = g_slice_new (GumArm64Relocator);

  gum_arm64_relocator_init (relocator, input_code, output);

  return relocator;
}

GumArm64Relocator *
gum_arm64_relocator_ref (GumArm64Relocator * relocator)
{
  g_atomic_int_inc (&relocator->ref_count);

  return relocator;
}

void
gum_arm64_relocator_unref (GumArm64Relocator * relocator)
{
  if (g_atomic_int_dec_and_test (&relocator->ref_count))
  {
    gum_arm64_relocator_clear (relocator);

    g_slice_free (GumArm64Relocator, relocator);
  }
}

void
gum_arm64_relocator_init (GumArm64Relocator * relocator,
                          gconstpointer input_code,
                          GumArm64Writer * output)
{
  relocator->ref_count = 1;

  cs_arch_register_arm64 ();
  cs_open (CS_ARCH_ARM64, GUM_DEFAULT_CS_ENDIAN, &relocator->capstone);
  cs_option (relocator->capstone, CS_OPT_DETAIL, CS_OPT_ON);
  relocator->input_insns = g_new0 (cs_insn *, GUM_MAX_INPUT_INSN_COUNT);

  relocator->output = NULL;

  gum_arm64_relocator_reset (relocator, input_code, output);
}

void
gum_arm64_relocator_clear (GumArm64Relocator * relocator)
{
  guint i;

  gum_arm64_relocator_reset (relocator, NULL, NULL);

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
gum_arm64_relocator_reset (GumArm64Relocator * relocator,
                           gconstpointer input_code,
                           GumArm64Writer * output)
{
  relocator->input_start = input_code;
  relocator->input_cur = input_code;
  relocator->input_pc = GUM_ADDRESS (input_code);

  if (output != NULL)
    gum_arm64_writer_ref (output);
  if (relocator->output != NULL)
    gum_arm64_writer_unref (relocator->output);
  relocator->output = output;

  relocator->inpos = 0;
  relocator->outpos = 0;

  relocator->eob = FALSE;
  relocator->eoi = FALSE;
}

static guint
gum_arm64_relocator_inpos (GumArm64Relocator * self)
{
  return self->inpos % GUM_MAX_INPUT_INSN_COUNT;
}

static guint
gum_arm64_relocator_outpos (GumArm64Relocator * self)
{
  return self->outpos % GUM_MAX_INPUT_INSN_COUNT;
}

static void
gum_arm64_relocator_increment_inpos (GumArm64Relocator * self)
{
  self->inpos++;
  g_assert (self->inpos > self->outpos);
}

static void
gum_arm64_relocator_increment_outpos (GumArm64Relocator * self)
{
  self->outpos++;
  g_assert (self->outpos <= self->inpos);
}

guint
gum_arm64_relocator_read_one (GumArm64Relocator * self,
                              const cs_insn ** instruction)
{
  cs_insn ** insn_ptr, * insn;
  const uint8_t * code;
  size_t size;
  uint64_t address;

  if (self->eoi)
    return 0;

  insn_ptr = &self->input_insns[gum_arm64_relocator_inpos (self)];

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
    case ARM64_INS_B:
      self->eob = TRUE;
      self->eoi = gum_arm64_branch_is_unconditional (insn);
      break;
    case ARM64_INS_BR:
    case ARM64_INS_BRAA:
    case ARM64_INS_BRAAZ:
    case ARM64_INS_BRAB:
    case ARM64_INS_BRABZ:
    case ARM64_INS_RET:
    case ARM64_INS_RETAA:
    case ARM64_INS_RETAB:
      self->eob = TRUE;
      self->eoi = TRUE;
      break;
    case ARM64_INS_BL:
    case ARM64_INS_BLR:
    case ARM64_INS_BLRAA:
    case ARM64_INS_BLRAAZ:
    case ARM64_INS_BLRAB:
    case ARM64_INS_BLRABZ:
      self->eob = TRUE;
      self->eoi = FALSE;
      break;
    case ARM64_INS_CBZ:
    case ARM64_INS_CBNZ:
    case ARM64_INS_TBZ:
    case ARM64_INS_TBNZ:
      self->eob = TRUE;
      self->eoi = FALSE;
      break;
    default:
      self->eob = FALSE;
      break;
  }

  gum_arm64_relocator_increment_inpos (self);

  if (instruction != NULL)
    *instruction = insn;

  self->input_cur = code;
  self->input_pc = address;

  return self->input_cur - self->input_start;
}

cs_insn *
gum_arm64_relocator_peek_next_write_insn (GumArm64Relocator * self)
{
  if (self->outpos == self->inpos)
    return NULL;

  return self->input_insns[gum_arm64_relocator_outpos (self)];
}

gpointer
gum_arm64_relocator_peek_next_write_source (GumArm64Relocator * self)
{
  cs_insn * next;

  next = gum_arm64_relocator_peek_next_write_insn (self);
  if (next == NULL)
    return NULL;

  return GSIZE_TO_POINTER (next->address);
}

void
gum_arm64_relocator_skip_one (GumArm64Relocator * self)
{
  gum_arm64_relocator_increment_outpos (self);
}

gboolean
gum_arm64_relocator_write_one (GumArm64Relocator * self)
{
  const cs_insn * insn;
  GumCodeGenCtx ctx;
  gboolean rewritten;

  if ((insn = gum_arm64_relocator_peek_next_write_insn (self)) == NULL)
    return FALSE;
  gum_arm64_relocator_increment_outpos (self);
  ctx.insn = insn;
  ctx.detail = &ctx.insn->detail->arm64;
  ctx.output = self->output;

  switch (insn->id)
  {
    case ARM64_INS_LDR:
    case ARM64_INS_LDRSW:
      rewritten = gum_arm64_relocator_rewrite_ldr (self, &ctx);
      break;
    case ARM64_INS_ADR:
    case ARM64_INS_ADRP:
      rewritten = gum_arm64_relocator_rewrite_adr (self, &ctx);
      break;
    case ARM64_INS_B:
      if (gum_arm64_branch_is_unconditional (ctx.insn))
        rewritten = gum_arm64_relocator_rewrite_b (self, &ctx);
      else
        rewritten = gum_arm64_relocator_rewrite_b_cond (self, &ctx);
      break;
    case ARM64_INS_BL:
      rewritten = gum_arm64_relocator_rewrite_bl (self, &ctx);
      break;
    case ARM64_INS_CBZ:
    case ARM64_INS_CBNZ:
      rewritten = gum_arm64_relocator_rewrite_cbz (self, &ctx);
      break;
    case ARM64_INS_TBZ:
    case ARM64_INS_TBNZ:
      rewritten = gum_arm64_relocator_rewrite_tbz (self, &ctx);
      break;
    default:
      rewritten = FALSE;
      break;
  }

  if (!rewritten)
    gum_arm64_writer_put_bytes (ctx.output, insn->bytes, insn->size);

  return TRUE;
}

void
gum_arm64_relocator_write_all (GumArm64Relocator * self)
{
  G_GNUC_UNUSED guint count = 0;

  while (gum_arm64_relocator_write_one (self))
    count++;

  g_assert (count > 0);
}

gboolean
gum_arm64_relocator_eob (GumArm64Relocator * self)
{
  return self->eob;
}

gboolean
gum_arm64_relocator_eoi (GumArm64Relocator * self)
{
  return self->eoi;
}

gboolean
gum_arm64_relocator_can_relocate (gpointer address,
                                  guint min_bytes,
                                  GumRelocationScenario scenario,
                                  GumRelocationPolicy policy,
                                  guint * maximum,
                                  arm64_reg * available_scratch_reg)
{
  guint n = 0;
  guint8 * buf;
  GumArm64Writer cw;
  GumArm64Relocator rl;
  guint reloc_bytes;

  buf = g_alloca (3 * min_bytes);
  gum_arm64_writer_init (&cw, buf);

  gum_arm64_relocator_init (&rl, address, &cw);

  do
  {
    const cs_insn * insn;
    gboolean safe_to_relocate_further;

    reloc_bytes = gum_arm64_relocator_read_one (&rl, &insn);
    if (reloc_bytes == 0)
      break;

    n = reloc_bytes;

    if (scenario == GUM_SCENARIO_ONLINE)
    {
      switch (insn->id)
      {
        case ARM64_INS_BL:
        case ARM64_INS_BLR:
        case ARM64_INS_SVC:
          safe_to_relocate_further = FALSE;
          break;
        default:
          safe_to_relocate_further = TRUE;
          break;
      }
    }
    else
    {
      safe_to_relocate_further = TRUE;
    }

    if (!safe_to_relocate_further)
      break;
  }
  while (reloc_bytes < min_bytes);

  if (policy == GUM_RELOCATION_CHECKED && !rl.eoi)
  {
    GHashTable * checked_targets, * targets_to_check;
    csh capstone;
    cs_insn * insn;
    const guint8 * current_code;
    uint64_t current_address;
    size_t current_code_size;
    gpointer target;
    GHashTableIter iter;
    guint insn_index;
    guint num_insns;

    checked_targets = g_hash_table_new (NULL, NULL);
    targets_to_check = g_hash_table_new (NULL, NULL);

    /*
     * Relocated conditional/unconditional branches are rewritten as absolute
     * jumps back to the original target address. If that target lies inside the
     * range we are about to overwrite, the rewritten branch would land in the
     * middle of the redirect patch. Shrink the range so every such target falls
     * outside it. Also keep out-of-range targets queued for reachability checks
     * below, since those paths must not jump back into the patch either.
     */
    num_insns = n / 4;
    for (insn_index = 0; insn_index != num_insns; insn_index++)
    {
      const cs_insn * input_insn = rl.input_insns[insn_index];
      gssize offset;

      if (input_insn == NULL)
        break;

      /* Instruction was cut out by an earlier shrink. */
      if (insn_index * 4 >= n)
        break;

      target = gum_arm64_relocator_extract_branch_target (input_insn);
      if (target == NULL)
        continue;

      offset = (gssize) target - (gssize) address;
      if (offset > 0 && offset < (gssize) n)
        n = (guint) offset;
      else if (offset >= (gssize) n)
        g_hash_table_add (targets_to_check, target);
    }

    /*
     * If an internal branch forced us to shrink n, rewind so the reachability
     * scan below starts at the new relocatable boundary rather than wherever
     * read_one() stopped when trying to satisfy min_bytes.
     */
    rl.input_cur = (const guint8 *) address + n;
    rl.input_pc = GUM_ADDRESS (address) + n;

    cs_open (CS_ARCH_ARM64, GUM_DEFAULT_CS_ENDIAN, &capstone);
    cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

    insn = cs_malloc (capstone);
    current_code = rl.input_cur;
    current_address = rl.input_pc;
    current_code_size = 1024;

    do
    {
      gboolean carry_on = TRUE;

      g_hash_table_add (checked_targets, (gpointer) current_code);

      gum_ensure_code_readable (current_code, current_code_size);

      while (carry_on && cs_disasm_iter (capstone, &current_code,
          &current_code_size, &current_address, insn))
      {
        cs_arm64 * d = &insn->detail->arm64;

        switch (insn->id)
        {
          case ARM64_INS_B:
          {
            target = gum_arm64_relocator_extract_branch_target (insn);
            g_assert (target != NULL);
            if (!g_hash_table_contains (checked_targets, target))
              g_hash_table_add (targets_to_check, target);

            carry_on = d->cc != ARM64_CC_INVALID && d->cc != ARM64_CC_AL &&
                d->cc != ARM64_CC_NV;

            break;
          }
          case ARM64_INS_CBZ:
          case ARM64_INS_CBNZ:
          case ARM64_INS_TBZ:
          case ARM64_INS_TBNZ:
          {
            target = gum_arm64_relocator_extract_branch_target (insn);
            g_assert (target != NULL);
            if (!g_hash_table_contains (checked_targets, target))
              g_hash_table_add (targets_to_check, target);

            break;
          }
          case ARM64_INS_RET:
          case ARM64_INS_RETAA:
          case ARM64_INS_RETAB:
          {
            carry_on = FALSE;
            break;
          }
          case ARM64_INS_BR:
          case ARM64_INS_BRAA:
          case ARM64_INS_BRAAZ:
          case ARM64_INS_BRAB:
          case ARM64_INS_BRABZ:
          {
            carry_on = FALSE;
            break;
          }
          default:
            break;
        }
      }

      g_hash_table_iter_init (&iter, targets_to_check);
      if (g_hash_table_iter_next (&iter, &target, NULL))
      {
        current_code = target;
        if (current_code > rl.input_cur)
          current_address = (current_code - rl.input_cur) + rl.input_pc;
        else
          current_address = rl.input_pc - (rl.input_cur - current_code);
        g_hash_table_iter_remove (&iter);
      }
      else
      {
        current_code = NULL;
      }
    }
    while (current_code != NULL);

    g_hash_table_iter_init (&iter, checked_targets);
    while (g_hash_table_iter_next (&iter, &target, NULL))
    {
      gssize offset = (gssize) target - (gssize) address;
      if (offset > 0 && offset < (gssize) n)
      {
        n = offset;
        if (n == 4)
          break;
      }
    }

    cs_free (insn, 1);

    cs_close (&capstone);

    g_hash_table_unref (targets_to_check);
    g_hash_table_unref (checked_targets);
  }

  if (available_scratch_reg != NULL)
  {
    arm64_reg requested_scratch_reg = *available_scratch_reg;

    if (requested_scratch_reg != ARM64_REG_INVALID)
    {
      if (!gum_arm64_relocator_register_is_free (&rl, n, requested_scratch_reg))
        *available_scratch_reg = ARM64_REG_INVALID;
    }
    else if (gum_arm64_relocator_register_is_free (&rl, n, ARM64_REG_X16))
    {
      *available_scratch_reg = ARM64_REG_X16;
    }
    else if (gum_arm64_relocator_register_is_free (&rl, n, ARM64_REG_X17))
    {
      *available_scratch_reg = ARM64_REG_X17;
    }
    else
    {
      *available_scratch_reg = ARM64_REG_INVALID;
    }
  }

  gum_arm64_relocator_clear (&rl);

  gum_arm64_writer_clear (&cw);

  if (maximum != NULL)
    *maximum = n;

  return n >= min_bytes;
}

static gboolean
gum_arm64_relocator_register_is_free (const GumArm64Relocator * self,
                                      guint n,
                                      arm64_reg reg)
{
  guint insn_index;

  for (insn_index = 0; insn_index != n / 4; insn_index++)
  {
    const cs_insn * insn = self->input_insns[insn_index];
    const cs_arm64 * info = &insn->detail->arm64;
    uint8_t op_index;

    for (op_index = 0; op_index != info->op_count; op_index++)
    {
      const cs_arm64_op * op = &info->operands[op_index];

      if (op->type == ARM64_OP_REG && op->reg == reg)
        return FALSE;
    }
  }

  return TRUE;
}

static gpointer
gum_arm64_relocator_extract_branch_target (const cs_insn * insn)
{
  const cs_arm64 * d = &insn->detail->arm64;
  const cs_arm64_op * op;

  switch (insn->id)
  {
    case ARM64_INS_B:
      op = &d->operands[0];
      break;
    case ARM64_INS_CBZ:
    case ARM64_INS_CBNZ:
      op = &d->operands[1];
      break;
    case ARM64_INS_TBZ:
    case ARM64_INS_TBNZ:
      op = &d->operands[2];
      break;
    default:
      return NULL;
  }

  if (op->type != ARM64_OP_IMM)
    return NULL;

  return GSIZE_TO_POINTER (op->imm);
}

guint
gum_arm64_relocator_relocate (gpointer from,
                              guint min_bytes,
                              gpointer to)
{
  GumArm64Writer cw;
  GumArm64Relocator rl;
  guint reloc_bytes;

  gum_arm64_writer_init (&cw, to);

  gum_arm64_relocator_init (&rl, from, &cw);

  do
  {
    reloc_bytes = gum_arm64_relocator_read_one (&rl, NULL);
    g_assert (reloc_bytes != 0);
  }
  while (reloc_bytes < min_bytes);

  gum_arm64_relocator_write_all (&rl);

  gum_arm64_relocator_clear (&rl);
  gum_arm64_writer_clear (&cw);

  return reloc_bytes;
}

static gboolean
gum_arm64_branch_is_unconditional (const cs_insn * insn)
{
  switch (insn->detail->arm64.cc)
  {
    case ARM64_CC_INVALID:
    case ARM64_CC_AL:
    case ARM64_CC_NV:
      return TRUE;
    default:
      return FALSE;
  }
}

static gboolean
gum_arm64_relocator_rewrite_ldr (GumArm64Relocator * self,
                                 GumCodeGenCtx * ctx)
{
  arm64_insn insn_id = ctx->insn->id;
  const cs_arm64_op * dst = &ctx->detail->operands[0];
  const cs_arm64_op * src = &ctx->detail->operands[1];
  gboolean dst_reg_is_fp_or_simd;
  arm64_reg tmp_reg;

  if (src->type != ARM64_OP_IMM)
    return FALSE;

  dst_reg_is_fp_or_simd =
      (dst->reg >= ARM64_REG_S0 && dst->reg <= ARM64_REG_S31) ||
      (dst->reg >= ARM64_REG_D0 && dst->reg <= ARM64_REG_D31) ||
      (dst->reg >= ARM64_REG_Q0 && dst->reg <= ARM64_REG_Q31);
  if (dst_reg_is_fp_or_simd)
  {
    tmp_reg = ARM64_REG_X0;

    gum_arm64_writer_put_push_reg_reg (ctx->output, tmp_reg, ARM64_REG_X1);

    gum_arm64_writer_put_ldr_reg_address (ctx->output, tmp_reg, src->imm);
    g_assert (insn_id == ARM64_INS_LDR);
    gum_arm64_writer_put_ldr_reg_reg_offset (ctx->output, dst->reg, tmp_reg, 0);

    gum_arm64_writer_put_pop_reg_reg (ctx->output, tmp_reg, ARM64_REG_X1);
  }
  else
  {
    if (dst->reg >= ARM64_REG_W0 && dst->reg <= ARM64_REG_W28)
      tmp_reg = ARM64_REG_X0 + (dst->reg - ARM64_REG_W0);
    else if (dst->reg >= ARM64_REG_W29 && dst->reg <= ARM64_REG_W30)
      tmp_reg = ARM64_REG_X29 + (dst->reg - ARM64_REG_W29);
    else
      tmp_reg = dst->reg;

    gum_arm64_writer_put_ldr_reg_address (ctx->output, tmp_reg, src->imm);
    if (insn_id == ARM64_INS_LDR)
    {
      gum_arm64_writer_put_ldr_reg_reg_offset (ctx->output, dst->reg, tmp_reg,
          0);
    }
    else
    {
      gum_arm64_writer_put_ldrsw_reg_reg_offset (ctx->output, dst->reg, tmp_reg,
          0);
    }
  }

  return TRUE;
}

static gboolean
gum_arm64_relocator_rewrite_adr (GumArm64Relocator * self,
                                 GumCodeGenCtx * ctx)
{
  const cs_arm64_op * dst = &ctx->detail->operands[0];
  const cs_arm64_op * label = &ctx->detail->operands[1];

  g_assert (label->type == ARM64_OP_IMM);

  gum_arm64_writer_put_ldr_reg_address (ctx->output, dst->reg, label->imm);
  return TRUE;
}

static gboolean
gum_arm64_relocator_rewrite_b (GumArm64Relocator * self,
                               GumCodeGenCtx * ctx)
{
  const cs_arm64_op * target = &ctx->detail->operands[0];

  gum_arm64_writer_put_ldr_reg_address (ctx->output, ARM64_REG_X16,
      gum_arm64_writer_sign (ctx->output, target->imm));
  gum_arm64_writer_put_jmp_reg (ctx->output, ARM64_REG_X16);

  return TRUE;
}

static gboolean
gum_arm64_relocator_rewrite_b_cond (GumArm64Relocator * self,
                                    GumCodeGenCtx * ctx)
{
  const cs_arm64_op * target = &ctx->detail->operands[0];
  gsize unique_id = GPOINTER_TO_SIZE (ctx->output->code) << 1;
  gconstpointer is_true = GSIZE_TO_POINTER (unique_id | 1);
  gconstpointer is_false = GSIZE_TO_POINTER (unique_id | 0);

  gum_arm64_writer_put_b_cond_label (ctx->output, ctx->detail->cc, is_true);
  gum_arm64_writer_put_b_label (ctx->output, is_false);

  gum_arm64_writer_put_label (ctx->output, is_true);
  gum_arm64_writer_put_ldr_reg_address (ctx->output, ARM64_REG_X16,
      gum_arm64_writer_sign (ctx->output, target->imm));
  gum_arm64_writer_put_jmp_reg (ctx->output, ARM64_REG_X16);

  gum_arm64_writer_put_label (ctx->output, is_false);

  return TRUE;
}

static gboolean
gum_arm64_relocator_rewrite_bl (GumArm64Relocator * self,
                                GumCodeGenCtx * ctx)
{
  const cs_arm64_op * target = &ctx->detail->operands[0];

  gum_arm64_writer_put_ldr_reg_address (ctx->output, ARM64_REG_LR,
      gum_arm64_writer_sign (ctx->output, target->imm));
  gum_arm64_writer_put_blr_reg (ctx->output, ARM64_REG_LR);

  return TRUE;
}

static gboolean
gum_arm64_relocator_rewrite_cbz (GumArm64Relocator * self,
                                 GumCodeGenCtx * ctx)
{
  const cs_arm64_op * source = &ctx->detail->operands[0];
  const cs_arm64_op * target = &ctx->detail->operands[1];
  gsize unique_id = GPOINTER_TO_SIZE (ctx->output->code) << 1;
  gconstpointer is_true = GSIZE_TO_POINTER (unique_id | 1);
  gconstpointer is_false = GSIZE_TO_POINTER (unique_id | 0);

  if (ctx->insn->id == ARM64_INS_CBZ)
    gum_arm64_writer_put_cbz_reg_label (ctx->output, source->reg, is_true);
  else
    gum_arm64_writer_put_cbnz_reg_label (ctx->output, source->reg, is_true);
  gum_arm64_writer_put_b_label (ctx->output, is_false);

  gum_arm64_writer_put_label (ctx->output, is_true);
  gum_arm64_writer_put_ldr_reg_address (ctx->output, ARM64_REG_X16,
      gum_arm64_writer_sign (ctx->output, target->imm));
  gum_arm64_writer_put_jmp_reg (ctx->output, ARM64_REG_X16);

  gum_arm64_writer_put_label (ctx->output, is_false);

  return TRUE;
}

static gboolean
gum_arm64_relocator_rewrite_tbz (GumArm64Relocator * self,
                                 GumCodeGenCtx * ctx)
{
  const cs_arm64_op * source = &ctx->detail->operands[0];
  const cs_arm64_op * bit = &ctx->detail->operands[1];
  const cs_arm64_op * target = &ctx->detail->operands[2];
  gsize unique_id = GPOINTER_TO_SIZE (ctx->output->code) << 1;
  gconstpointer is_true = GSIZE_TO_POINTER (unique_id | 1);
  gconstpointer is_false = GSIZE_TO_POINTER (unique_id | 0);

  if (ctx->insn->id == ARM64_INS_TBZ)
  {
    gum_arm64_writer_put_tbz_reg_imm_label (ctx->output, source->reg, bit->imm,
        is_true);
  }
  else
  {
    gum_arm64_writer_put_tbnz_reg_imm_label (ctx->output, source->reg, bit->imm,
        is_true);
  }
  gum_arm64_writer_put_b_label (ctx->output, is_false);

  gum_arm64_writer_put_label (ctx->output, is_true);
  gum_arm64_writer_put_ldr_reg_address (ctx->output, ARM64_REG_X16,
      gum_arm64_writer_sign (ctx->output, target->imm));
  gum_arm64_writer_put_jmp_reg (ctx->output, ARM64_REG_X16);

  gum_arm64_writer_put_label (ctx->output, is_false);

  return TRUE;
}
