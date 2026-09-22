/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2019 Jon Wilson <jonwilson@zepler.net>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarmwriter.h"

#include "gumarmreg.h"
#include "gumlibc.h"
#include "gummemory.h"
#include "gumprocess.h"

typedef struct _GumArmLabelRef GumArmLabelRef;
typedef struct _GumArmLiteralRef GumArmLiteralRef;

struct _GumArmLabelRef
{
  gconstpointer id;
  guint32 * insn;
};

struct _GumArmLiteralRef
{
  guint32 * insn;
  guint32 val;
};

static void gum_arm_writer_reset_refs (GumArmWriter * self);

static void gum_arm_writer_put_argument_list_setup (GumArmWriter * self,
    guint n_args, const GumArgument * args);
static void gum_arm_writer_put_argument_list_setup_va (GumArmWriter * self,
    guint n_args, va_list args);
static void gum_arm_writer_put_argument_list_teardown (GumArmWriter * self,
    guint n_args);
static void gum_arm_writer_put_call_address_body (GumArmWriter * self,
    GumAddress address);
static gboolean gum_arm_writer_put_vector_push_or_pop_range (
    GumArmWriter * self, guint32 insn_template, arm_reg first_reg,
    arm_reg last_reg);

static gboolean gum_arm_writer_try_commit_label_refs (GumArmWriter * self);
static void gum_arm_writer_maybe_commit_literals (GumArmWriter * self);
static void gum_arm_writer_commit_literals (GumArmWriter * self);

static guint32 gum_arm_condify (arm_cc cc);
static guint32 gum_arm_shiftify (arm_shifter shifter);

GumArmWriter *
gum_arm_writer_new (gpointer code_address)
{
  GumArmWriter * writer;

  writer = g_slice_new (GumArmWriter);

  gum_arm_writer_init (writer, code_address);

  return writer;
}

GumArmWriter *
gum_arm_writer_ref (GumArmWriter * writer)
{
  g_atomic_int_inc (&writer->ref_count);

  return writer;
}

void
gum_arm_writer_unref (GumArmWriter * writer)
{
  if (g_atomic_int_dec_and_test (&writer->ref_count))
  {
    gum_arm_writer_clear (writer);

    g_slice_free (GumArmWriter, writer);
  }
}

void
gum_arm_writer_init (GumArmWriter * writer,
                     gpointer code_address)
{
  writer->ref_count = 1;
  writer->flush_on_destroy = TRUE;

  writer->target_os = gum_process_get_native_os ();
  writer->cpu_features = GUM_CPU_THUMB_INTERWORK;

  writer->label_defs = NULL;
  writer->label_refs.data = NULL;
  writer->literal_refs.data = NULL;

  gum_arm_writer_reset (writer, code_address);
}

static gboolean
gum_arm_writer_has_label_defs (GumArmWriter * self)
{
  return self->label_defs != NULL;
}

static gboolean
gum_arm_writer_has_label_refs (GumArmWriter * self)
{
  return self->label_refs.data != NULL;
}

static gboolean
gum_arm_writer_has_literal_refs (GumArmWriter * self)
{
  return self->literal_refs.data != NULL;
}

void
gum_arm_writer_clear (GumArmWriter * writer)
{
  if (writer->flush_on_destroy)
    gum_arm_writer_flush (writer);

  if (gum_arm_writer_has_label_defs (writer))
    gum_metal_hash_table_unref (writer->label_defs);

  if (gum_arm_writer_has_label_refs (writer))
    gum_metal_array_free (&writer->label_refs);

  if (gum_arm_writer_has_literal_refs (writer))
    gum_metal_array_free (&writer->literal_refs);
}

void
gum_arm_writer_reset (GumArmWriter * writer,
                      gpointer code_address)
{
  writer->base = code_address;
  writer->code = code_address;
  writer->pc = GUM_ADDRESS (code_address);

  if (gum_arm_writer_has_label_defs (writer))
    gum_metal_hash_table_remove_all (writer->label_defs);

  gum_arm_writer_reset_refs (writer);
}

static void
gum_arm_writer_reset_refs (GumArmWriter * self)
{
  if (gum_arm_writer_has_label_refs (self))
    gum_metal_array_remove_all (&self->label_refs);

  if (gum_arm_writer_has_literal_refs (self))
    gum_metal_array_remove_all (&self->literal_refs);

  self->earliest_literal_insn = NULL;
}

void
gum_arm_writer_set_target_os (GumArmWriter * self,
                              GumOS os)
{
  self->target_os = os;
}

gpointer
gum_arm_writer_cur (GumArmWriter * self)
{
  return self->code;
}

guint
gum_arm_writer_offset (GumArmWriter * self)
{
  return (guint) (self->code - self->base) * sizeof (guint32);
}

void
gum_arm_writer_skip (GumArmWriter * self,
                     guint n_bytes)
{
  self->code = (guint32 *) (((guint8 *) self->code) + n_bytes);
  self->pc += n_bytes;
}

gboolean
gum_arm_writer_flush (GumArmWriter * self)
{
  if (!gum_arm_writer_try_commit_label_refs (self))
    goto error;

  gum_arm_writer_commit_literals (self);

  return TRUE;

error:
  {
    gum_arm_writer_reset_refs (self);

    return FALSE;
  }
}

gboolean
gum_arm_writer_put_label (GumArmWriter * self,
                          gconstpointer id)
{
  if (!gum_arm_writer_has_label_defs (self))
    self->label_defs = gum_metal_hash_table_new (NULL, NULL);

  if (gum_metal_hash_table_lookup (self->label_defs, id) != NULL)
    return FALSE;

  gum_metal_hash_table_insert (self->label_defs, (gpointer) id, self->code);

  return TRUE;
}

static void
gum_arm_writer_add_label_reference_here (GumArmWriter * self,
                                         gconstpointer id)
{
  GumArmLabelRef * r;

  if (!gum_arm_writer_has_label_refs (self))
    gum_metal_array_init (&self->label_refs, sizeof (GumArmLabelRef));

  r = gum_metal_array_append (&self->label_refs);
  r->id = id;
  r->insn = self->code;
}

static void
gum_arm_writer_add_literal_reference_here (GumArmWriter * self,
                                           guint32 val)
{
  GumArmLiteralRef * r;

  if (!gum_arm_writer_has_literal_refs (self))
    gum_metal_array_init (&self->literal_refs, sizeof (GumArmLiteralRef));

  r = gum_metal_array_append (&self->literal_refs);
  r->insn = self->code;
  r->val = val;

  if (self->earliest_literal_insn == NULL)
    self->earliest_literal_insn = r->insn;
}

void
gum_arm_writer_put_call_address_with_arguments (GumArmWriter * self,
                                                GumAddress func,
                                                guint n_args,
                                                ...)
{
  va_list args;

  va_start (args, n_args);
  gum_arm_writer_put_argument_list_setup_va (self, n_args, args);
  va_end (args);

  gum_arm_writer_put_call_address_body (self, func);

  gum_arm_writer_put_argument_list_teardown (self, n_args);
}

void
gum_arm_writer_put_call_address_with_arguments_array (GumArmWriter * self,
                                                      GumAddress func,
                                                      guint n_args,
                                                      const GumArgument * args)
{
  gum_arm_writer_put_argument_list_setup (self, n_args, args);

  gum_arm_writer_put_call_address_body (self, func);

  gum_arm_writer_put_argument_list_teardown (self, n_args);
}

void
gum_arm_writer_put_call_reg (GumArmWriter * self,
                             arm_reg reg)
{
  if ((self->cpu_features & GUM_CPU_THUMB_INTERWORK) != 0)
    gum_arm_writer_put_blx_reg (self, reg);
  else
    gum_arm_writer_put_bl_reg (self, reg);
}

void
gum_arm_writer_put_call_reg_with_arguments (GumArmWriter * self,
                                            arm_reg reg,
                                            guint n_args,
                                            ...)
{
  va_list args;

  va_start (args, n_args);
  gum_arm_writer_put_argument_list_setup_va (self, n_args, args);
  va_end (args);

  gum_arm_writer_put_call_reg (self, reg);

  gum_arm_writer_put_argument_list_teardown (self, n_args);
}

void
gum_arm_writer_put_call_reg_with_arguments_array (GumArmWriter * self,
                                                  arm_reg reg,
                                                  guint n_args,
                                                  const GumArgument * args)
{
  gum_arm_writer_put_argument_list_setup (self, n_args, args);

  gum_arm_writer_put_call_reg (self, reg);

  gum_arm_writer_put_argument_list_teardown (self, n_args);
}

static void
gum_arm_writer_put_argument_list_setup (GumArmWriter * self,
                                        guint n_args,
                                        const GumArgument * args)
{
  guint n_stack_args;
  gint arg_index;

  n_stack_args = MAX ((gint) n_args - 4, 0);
  if (n_stack_args % 2 != 0)
    gum_arm_writer_put_sub_reg_u16 (self, ARM_REG_SP, 4);

  for (arg_index = (gint) n_args - 1; arg_index >= 0; arg_index--)
  {
    const GumArgument * arg = &args[arg_index];
    const arm_reg dst_reg = ARM_REG_R0 + arg_index;

    if (arg_index < 4)
    {
      if (arg->type == GUM_ARG_ADDRESS)
      {
        gum_arm_writer_put_ldr_reg_address (self, dst_reg, arg->value.address);
      }
      else
      {
        arm_reg src_reg = arg->value.reg;
        GumArmRegInfo rs;

        gum_arm_reg_describe (src_reg, &rs);

        if (src_reg != dst_reg)
          gum_arm_writer_put_mov_reg_reg (self, dst_reg, arg->value.reg);
      }
    }
    else
    {
      if (arg->type == GUM_ARG_ADDRESS)
      {
        gum_arm_writer_put_ldr_reg_address (self, ARM_REG_R0,
            arg->value.address);
        gum_arm_writer_put_push_regs (self, 1, ARM_REG_R0);
      }
      else
      {
        gum_arm_writer_put_push_regs (self, 1, arg->value.reg);
      }
    }
  }
}

static void
gum_arm_writer_put_argument_list_setup_va (GumArmWriter * self,
                                           guint n_args,
                                           va_list args)
{
  GumArgument * arg_values;
  guint arg_index;

  arg_values = g_newa (GumArgument, n_args);

  for (arg_index = 0; arg_index != n_args; arg_index++)
  {
    GumArgument * arg = &arg_values[arg_index];

    arg->type = va_arg (args, GumArgType);
    if (arg->type == GUM_ARG_ADDRESS)
      arg->value.address = va_arg (args, GumAddress);
    else if (arg->type == GUM_ARG_REGISTER)
      arg->value.reg = va_arg (args, arm_reg);
    else
      g_assert_not_reached ();
  }

  gum_arm_writer_put_argument_list_setup (self, n_args, arg_values);
}

static void
gum_arm_writer_put_argument_list_teardown (GumArmWriter * self,
                                           guint n_args)
{
  guint n_stack_args, n_stack_slots;

  n_stack_args = MAX ((gint) n_args - 4, 0);
  if (n_stack_args == 0)
    return;

  n_stack_slots = n_stack_args;
  if (n_stack_slots % 2 != 0)
    n_stack_slots++;

  gum_arm_writer_put_add_reg_u16 (self, ARM_REG_SP, n_stack_slots * 4);
}

static void
gum_arm_writer_put_call_address_body (GumArmWriter * self,
                                      GumAddress address)
{
  GumAddress aligned_address;

  aligned_address = address & ~GUM_ADDRESS (1);

  if (gum_arm_writer_can_branch_directly_between (self, self->pc,
      aligned_address))
  {
    if (aligned_address == address)
      gum_arm_writer_put_bl_imm (self, aligned_address);
    else
      gum_arm_writer_put_blx_imm (self, aligned_address);
  }
  else
  {
    gum_arm_writer_put_add_reg_reg_imm (self, ARM_REG_LR, ARM_REG_PC, 3 * 4);
    gum_arm_writer_put_push_regs (self, 2, ARM_REG_R0, ARM_REG_PC);
    gum_arm_writer_put_ldr_reg_address (self, ARM_REG_R0, address);
    gum_arm_writer_put_str_reg_reg_offset (self, ARM_REG_R0, ARM_REG_SP, 4);
    gum_arm_writer_put_pop_regs (self, 2, ARM_REG_R0, ARM_REG_PC);
  }
}

void
gum_arm_writer_put_branch_address (GumArmWriter * self,
                                   GumAddress address)
{
  if (gum_arm_writer_can_branch_directly_between (self, self->pc, address))
  {
    gum_arm_writer_put_b_imm (self, address);
  }
  else
  {
    gum_arm_writer_put_push_regs (self, 2, ARM_REG_R0, ARM_REG_PC);
    gum_arm_writer_put_ldr_reg_address (self, ARM_REG_R0, address);
    gum_arm_writer_put_str_reg_reg_offset (self, ARM_REG_R0, ARM_REG_SP, 4);
    gum_arm_writer_put_pop_regs (self, 2, ARM_REG_R0, ARM_REG_PC);
  }
}

gboolean
gum_arm_writer_can_branch_directly_between (GumArmWriter * self,
                                            GumAddress from,
                                            GumAddress to)
{
  gint64 distance = (gint64) to - (gint64) from;

  return GUM_IS_WITHIN_INT26_RANGE (distance);
}

gboolean
gum_arm_writer_put_b_imm (GumArmWriter * self,
                          GumAddress target)
{
  return gum_arm_writer_put_b_cond_imm (self, ARM_CC_AL, target);
}

gboolean
gum_arm_writer_put_b_cond_imm (GumArmWriter * self,
                               arm_cc cc,
                               GumAddress target)
{
  gint64 distance;

  distance = (gint64) target - (gint64) (self->pc + 8);
  if (!GUM_IS_WITHIN_INT26_RANGE (distance))
    return FALSE;

  gum_arm_writer_put_instruction (self, 0x0a000000 | gum_arm_condify (cc) |
      ((distance >> 2) & GUM_INT24_MASK));

  return TRUE;
}

void
gum_arm_writer_put_b_label (GumArmWriter * self,
                            gconstpointer label_id)
{
  gum_arm_writer_put_b_cond_label (self, ARM_CC_AL, label_id);
}

void
gum_arm_writer_put_b_cond_label (GumArmWriter * self,
                                 arm_cc cc,
                                 gconstpointer label_id)
{
  gum_arm_writer_add_label_reference_here (self, label_id);
  gum_arm_writer_put_instruction (self, 0x0a000000 | gum_arm_condify (cc));
}

gboolean
gum_arm_writer_put_bl_imm (GumArmWriter * self,
                           GumAddress target)
{
  gint64 distance;

  distance = (gint64) target - (gint64) (self->pc + 8);
  if (!GUM_IS_WITHIN_INT26_RANGE (distance))
    return FALSE;

  gum_arm_writer_put_instruction (self, 0xeb000000 |
      ((distance >> 2) & GUM_INT24_MASK));

  return TRUE;
}

gboolean
gum_arm_writer_put_blx_imm (GumArmWriter * self,
                            GumAddress target)
{
  gint64 distance;
  guint32 halfword_bit;

  distance = (gint64) target - (gint64) (self->pc + 8);
  if (!GUM_IS_WITHIN_INT26_RANGE (distance))
    return FALSE;

  halfword_bit = (distance >> 1) & 1;

  gum_arm_writer_put_instruction (self, 0xfa000000 | (halfword_bit << 24) |
      ((distance >> 2) & GUM_INT24_MASK));

  return TRUE;
}

void
gum_arm_writer_put_bl_label (GumArmWriter * self,
                             gconstpointer label_id)
{
  gum_arm_writer_add_label_reference_here (self, label_id);
  gum_arm_writer_put_instruction (self, 0xeb000000);
}

void
gum_arm_writer_put_bx_reg (GumArmWriter * self,
                           arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_put_instruction (self, 0xe12fff10 | ri.index);
}

void
gum_arm_writer_put_bl_reg (GumArmWriter * self,
                           arm_reg reg)
{
  gum_arm_writer_put_mov_reg_reg (self, ARM_REG_LR, ARM_REG_PC);
  gum_arm_writer_put_mov_reg_reg (self, ARM_REG_PC, reg);
}

void
gum_arm_writer_put_blx_reg (GumArmWriter * self,
                            arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_put_instruction (self, 0xe12fff30 | ri.index);
}

void
gum_arm_writer_put_ret (GumArmWriter * self)
{
  gum_arm_writer_put_instruction (self, 0xe1a0f00e);
}

void
gum_arm_writer_put_push_regs (GumArmWriter * self,
                              guint n,
                              ...)
{
  va_list args;
  guint16 mask;
  guint i;

  va_start (args, n);

  mask = 0;
  for (i = 0; i != n; i++)
  {
    arm_reg reg;
    GumArmRegInfo ri;

    reg = va_arg (args, arm_reg);
    gum_arm_reg_describe (reg, &ri);

    mask |= 1 << ri.index;
  }

  va_end (args);

  gum_arm_writer_put_instruction (self, 0xe92d0000 | mask);
}

void
gum_arm_writer_put_pop_regs (GumArmWriter * self,
                             guint n,
                             ...)
{
  va_list args;
  guint16 mask;
  guint i;

  va_start (args, n);

  mask = 0;
  for (i = 0; i != n; i++)
  {
    arm_reg reg;
    GumArmRegInfo ri;

    reg = va_arg (args, arm_reg);
    gum_arm_reg_describe (reg, &ri);

    mask |= 1 << ri.index;
  }

  va_end (args);

  gum_arm_writer_put_ldmia_reg_mask_wb (self, ARM_REG_SP, mask);
}

gboolean
gum_arm_writer_put_vpush_range (GumArmWriter * self,
                                arm_reg first_reg,
                                arm_reg last_reg)
{
  return gum_arm_writer_put_vector_push_or_pop_range (self, 0x0d2d0a00,
      first_reg, last_reg);
}

gboolean
gum_arm_writer_put_vpop_range (GumArmWriter * self,
                               arm_reg first_reg,
                               arm_reg last_reg)
{
  return gum_arm_writer_put_vector_push_or_pop_range (self, 0x0cbd0a00,
      first_reg, last_reg);
}

static gboolean
gum_arm_writer_put_vector_push_or_pop_range (GumArmWriter * self,
                                             guint32 insn_template,
                                             arm_reg first_reg,
                                             arm_reg last_reg)
{
  GumArmRegInfo rf, rl;
  guint8 count, imm8;

  gum_arm_reg_describe (first_reg, &rf);
  gum_arm_reg_describe (last_reg, &rl);

  if (rl.width != rf.width || rl.index < rf.index)
    return FALSE;

  if (rf.width == 128)
  {
    rf.width = 64;
    rf.index *= 2;
    rf.meta = GUM_ARM_MREG_D0 + rf.index;

    rl.width = 64;
    rl.index *= 2;
    if (rl.index % 2 == 0)
      rl.index++;
    rl.meta = GUM_ARM_MREG_D0 + rl.index;
  }

  count = rl.index - rf.index + 1;
  if (rf.width == 64)
  {
    if (count > 16)
      return FALSE;
    imm8 = 2 * count;
  }
  else
  {
    imm8 = count;
  }

  gum_arm_writer_put_instruction (self, insn_template | (0xe << 28) |
      ((rf.index >> 4) << 22) | ((rf.index & GUM_INT4_MASK) << 12) |
      ((rf.width == 64) << 8) | imm8);

  return TRUE;
}

gboolean
gum_arm_writer_put_ldr_reg_address (GumArmWriter * self,
                                    arm_reg reg,
                                    GumAddress address)
{
  return gum_arm_writer_put_ldr_reg_u32 (self, reg, (guint32) address);
}

gboolean
gum_arm_writer_put_ldr_reg_u32 (GumArmWriter * self,
                                arm_reg reg,
                                guint32 val)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_add_literal_reference_here (self, val);
  gum_arm_writer_put_instruction (self, 0xe51f0000 | (ri.index << 12));

  return TRUE;
}

gboolean
gum_arm_writer_put_ldr_reg_reg (GumArmWriter * self,
                                arm_reg dst_reg,
                                arm_reg src_reg)
{
  return gum_arm_writer_put_ldr_reg_reg_offset (self, dst_reg, src_reg, 0);
}

gboolean
gum_arm_writer_put_ldr_reg_reg_offset (GumArmWriter * self,
                                       arm_reg dst_reg,
                                       arm_reg src_reg,
                                       gssize src_offset)
{
  return gum_arm_writer_put_ldr_cond_reg_reg_offset (self, ARM_CC_AL, dst_reg,
      src_reg, src_offset);
}

gboolean
gum_arm_writer_put_ldr_cond_reg_reg_offset (GumArmWriter * self,
                                            arm_cc cc,
                                            arm_reg dst_reg,
                                            arm_reg src_reg,
                                            gssize src_offset)
{
  GumArmRegInfo rd, rs;
  gboolean is_positive;
  gsize abs_src_offset;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg, &rs);

  is_positive = src_offset >= 0;

  abs_src_offset = ABS (src_offset);
  if (abs_src_offset >= 4096)
    return FALSE;

  gum_arm_writer_put_instruction (self, 0x05100000 | gum_arm_condify (cc) |
      (is_positive << 23) | (rd.index << 12) | (rs.index << 16) |
      abs_src_offset);

  return TRUE;
}

void
gum_arm_writer_put_ldmia_reg_mask (GumArmWriter * self,
                                   arm_reg reg,
                                   guint16 mask)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_put_instruction (self, 0xe8900000 | (ri.index << 16) | mask);
}

void
gum_arm_writer_put_ldmia_reg_mask_wb (GumArmWriter * self,
                                      arm_reg reg,
                                      guint16 mask)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_put_instruction (self, 0xe8b00000 | (ri.index << 16) | mask);
}

gboolean
gum_arm_writer_put_str_reg_reg (GumArmWriter * self,
                                arm_reg src_reg,
                                arm_reg dst_reg)
{
  return gum_arm_writer_put_str_reg_reg_offset (self, src_reg, dst_reg, 0);
}

gboolean
gum_arm_writer_put_str_reg_reg_offset (GumArmWriter * self,
                                       arm_reg src_reg,
                                       arm_reg dst_reg,
                                       gssize dst_offset)
{
  return gum_arm_writer_put_str_cond_reg_reg_offset (self, ARM_CC_AL, src_reg,
      dst_reg, dst_offset);
}

gboolean
gum_arm_writer_put_str_cond_reg_reg_offset (GumArmWriter * self,
                                            arm_cc cc,
                                            arm_reg src_reg,
                                            arm_reg dst_reg,
                                            gssize dst_offset)
{
  GumArmRegInfo rs, rd;
  gboolean is_positive;
  gsize abs_dst_offset;

  gum_arm_reg_describe (src_reg, &rs);
  gum_arm_reg_describe (dst_reg, &rd);

  is_positive = dst_offset >= 0;

  abs_dst_offset = ABS (dst_offset);
  if (abs_dst_offset >= 4096)
    return FALSE;

  gum_arm_writer_put_instruction (self, 0x05000000 | gum_arm_condify (cc) |
      (is_positive << 23) | (rs.index << 12) | (rd.index << 16) |
      abs_dst_offset);

  return TRUE;
}

void
gum_arm_writer_put_mov_reg_reg (GumArmWriter * self,
                                arm_reg dst_reg,
                                arm_reg src_reg)
{
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, src_reg, 0);
}

void
gum_arm_writer_put_mov_reg_reg_shift (GumArmWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg src_reg,
                                      arm_shifter shift,
                                      guint16 shift_value)
{
  GumArmRegInfo rd, rs;
  gboolean is_noop;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg, &rs);

  is_noop = dst_reg == src_reg && shift_value == 0;
  if (is_noop)
    return;

  gum_arm_writer_put_instruction (self, 0xe1a00000 | (rd.index << 12) |
      ((shift_value & 0x1f) << 7) | gum_arm_shiftify (shift) | rs.index);
}

void
gum_arm_writer_put_mov_reg_cpsr (GumArmWriter * self,
                                 arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_put_instruction (self, 0xe10f0000 | ri.index << 12);
}

void
gum_arm_writer_put_mov_cpsr_reg (GumArmWriter * self,
                                 arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_arm_writer_put_instruction (self, 0xe129f000 | ri.index);
}

void
gum_arm_writer_put_add_reg_u16 (GumArmWriter * self,
                                arm_reg dst_reg,
                                guint16 val)
{
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, dst_reg,
      0xc00 | ((val >> 8) & 0xff));
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, dst_reg,
      val & 0xff);
}

void
gum_arm_writer_put_add_reg_u32 (GumArmWriter * self,
                                arm_reg dst_reg,
                                guint32 val)
{
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, dst_reg,
      0x400 | ((val >> 24) & 0xff));
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, dst_reg,
      0x800 | ((val >> 16) & 0xff));
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, dst_reg,
      0xc00 | ((val >> 8) & 0xff));
  gum_arm_writer_put_add_reg_reg_imm (self, dst_reg, dst_reg,
      val & 0xff);
}

void
gum_arm_writer_put_add_reg_reg_imm (GumArmWriter * self,
                                    arm_reg dst_reg,
                                    arm_reg src_reg,
                                    guint32 imm_val)
{
  GumArmRegInfo rd, rs;
  gboolean is_noop;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg, &rs);

  is_noop = dst_reg == src_reg && (imm_val & GUM_INT8_MASK) == 0;
  if (is_noop)
    return;

  gum_arm_writer_put_instruction (self, 0xe2800000 | (rd.index << 12) |
      (rs.index << 16) | (imm_val & GUM_INT12_MASK));
}

void
gum_arm_writer_put_add_reg_reg_reg (GumArmWriter * self,
                                    arm_reg dst_reg,
                                    arm_reg src_reg1,
                                    arm_reg src_reg2)
{
  GumArmRegInfo rd, rs1, rs2;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg1, &rs1);
  gum_arm_reg_describe (src_reg2, &rs2);

  gum_arm_writer_put_instruction (self, 0xe0800000 | (rd.index << 12) |
      (rs1.index << 16) | rs2.index);
}

void
gum_arm_writer_put_add_reg_reg_reg_shift (GumArmWriter * self,
                                          arm_reg dst_reg,
                                          arm_reg src_reg1,
                                          arm_reg src_reg2,
                                          arm_shifter shift,
                                          guint16 shift_value)
{
  GumArmRegInfo rd, rs1, rs2;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg1, &rs1);
  gum_arm_reg_describe (src_reg2, &rs2);

  gum_arm_writer_put_instruction (self, 0xe0800000 | (rd.index << 12) |
      (rs1.index << 16) | ((shift_value & 0x1f) << 7) |
      gum_arm_shiftify (shift) | rs2.index);
}

void
gum_arm_writer_put_sub_reg_u16 (GumArmWriter * self,
                                arm_reg dst_reg,
                                guint16 val)
{
  gum_arm_writer_put_sub_reg_reg_imm (self, dst_reg, dst_reg,
      0xc00 | ((val >> 8) & 0xff));
  gum_arm_writer_put_sub_reg_reg_imm (self, dst_reg, dst_reg,
      val & 0xff);
}

void
gum_arm_writer_put_sub_reg_u32 (GumArmWriter * self,
                                arm_reg dst_reg,
                                guint32 val)
{
  gum_arm_writer_put_sub_reg_reg_imm (self, dst_reg, dst_reg,
      0x400 | ((val >> 24) & 0xff));
  gum_arm_writer_put_sub_reg_reg_imm (self, dst_reg, dst_reg,
      0x800 | ((val >> 16) & 0xff));
  gum_arm_writer_put_sub_reg_reg_imm (self, dst_reg, dst_reg,
      0xc00 | ((val >> 8) & 0xff));
  gum_arm_writer_put_sub_reg_reg_imm (self, dst_reg, dst_reg,
      val & 0xff);
}

void
gum_arm_writer_put_sub_reg_reg_imm (GumArmWriter * self,
                                    arm_reg dst_reg,
                                    arm_reg src_reg,
                                    guint32 imm_val)
{
  GumArmRegInfo rd, rs;
  gboolean is_noop;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg, &rs);

  is_noop = dst_reg == src_reg && (imm_val & GUM_INT8_MASK) == 0;
  if (is_noop)
    return;

  gum_arm_writer_put_instruction (self, 0xe2400000 | (rd.index << 12) |
      (rs.index << 16) | (imm_val & GUM_INT12_MASK));
}

void
gum_arm_writer_put_sub_reg_reg_reg (GumArmWriter * self,
                                    arm_reg dst_reg,
                                    arm_reg src_reg1,
                                    arm_reg src_reg2)
{
  GumArmRegInfo rd, rs1, rs2;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg1, &rs1);
  gum_arm_reg_describe (src_reg2, &rs2);

  gum_arm_writer_put_instruction (self, 0xe0400000 | (rd.index << 12) |
      (rs1.index << 16) | rs2.index);
}

void
gum_arm_writer_put_rsb_reg_reg_imm (GumArmWriter * self,
                                    arm_reg dst_reg,
                                    arm_reg src_reg,
                                    guint32 imm_val)
{
  GumArmRegInfo rd, rs;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg, &rs);

  gum_arm_writer_put_instruction (self, 0xe2600000 | (rd.index << 12) |
      (rs.index << 16) | (imm_val & GUM_INT12_MASK));
}

void
gum_arm_writer_put_ands_reg_reg_imm (GumArmWriter * self,
                                     arm_reg dst_reg,
                                     arm_reg src_reg,
                                     guint32 imm_val)
{
  GumArmRegInfo rd, rs;

  gum_arm_reg_describe (dst_reg, &rd);
  gum_arm_reg_describe (src_reg, &rs);

  gum_arm_writer_put_instruction (self, 0xe2100000 | (rd.index << 12) |
      (rs.index << 16) | (imm_val & GUM_INT8_MASK));
}

void
gum_arm_writer_put_cmp_reg_imm (GumArmWriter * self,
                                arm_reg dst_reg,
                                guint32 imm_val)
{
  GumArmRegInfo rd;

  gum_arm_reg_describe (dst_reg, &rd);

  gum_arm_writer_put_instruction (self, 0xe3500000 | (rd.index << 16) |
      imm_val);
}

void
gum_arm_writer_put_nop (GumArmWriter * self)
{
  gum_arm_writer_put_instruction (self, 0xe1a00000);
}

void
gum_arm_writer_put_breakpoint (GumArmWriter * self)
{
  switch (self->target_os)
  {
    case GUM_OS_LINUX:
    case GUM_OS_ANDROID:
    default: /* TODO: handle other OSes */
      gum_arm_writer_put_brk_imm (self, 0x10);
      break;
  }
}

void
gum_arm_writer_put_brk_imm (GumArmWriter * self,
                            guint16 imm)
{
  gum_arm_writer_put_instruction (self, 0xe7f000f0 |
      ((imm >> 4) << 8) | (imm & 0xf));
}

void
gum_arm_writer_put_instruction (GumArmWriter * self,
                                guint32 insn)
{
  *self->code++ = GUINT32_TO_LE (insn);
  self->pc += 4;

  gum_arm_writer_maybe_commit_literals (self);
}

gboolean
gum_arm_writer_put_bytes (GumArmWriter * self,
                          const guint8 * data,
                          guint n)
{
  if (n % 4 != 0)
    return FALSE;

  gum_memcpy (self->code, data, n);
  self->code += n / sizeof (guint32);
  self->pc += n;

  gum_arm_writer_maybe_commit_literals (self);

  return TRUE;
}

static gboolean
gum_arm_writer_try_commit_label_refs (GumArmWriter * self)
{
  guint num_refs, ref_index;

  if (!gum_arm_writer_has_label_refs (self))
    return TRUE;

  if (!gum_arm_writer_has_label_defs (self))
    return FALSE;

  num_refs = self->label_refs.length;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumArmLabelRef * r;
    const guint32 * target_insn;
    gssize distance;
    guint32 insn;

    r = gum_metal_array_element_at (&self->label_refs, ref_index);

    target_insn = gum_metal_hash_table_lookup (self->label_defs, r->id);
    if (target_insn == NULL)
      return FALSE;

    distance = target_insn - (r->insn + 2);
    if (!GUM_IS_WITHIN_INT24_RANGE (distance))
      return FALSE;

    insn = GUINT32_FROM_LE (*r->insn);
    insn |= distance & GUM_INT24_MASK;
    *r->insn = GUINT32_TO_LE (insn);
  }

  gum_metal_array_remove_all (&self->label_refs);

  return TRUE;
}

static void
gum_arm_writer_maybe_commit_literals (GumArmWriter * self)
{
  gsize space_used;
  gconstpointer after_literals = self->code;

  if (self->earliest_literal_insn == NULL)
    return;

  space_used = (self->code - self->earliest_literal_insn) * sizeof (guint32);
  space_used += self->literal_refs.length * sizeof (guint32);
  if (space_used <= 4096)
    return;

  self->earliest_literal_insn = NULL;

  gum_arm_writer_put_b_label (self, after_literals);
  gum_arm_writer_commit_literals (self);
  gum_arm_writer_put_label (self, after_literals);
}

static void
gum_arm_writer_commit_literals (GumArmWriter * self)
{
  guint num_refs, ref_index;
  guint32 * first_slot, * last_slot;

  if (!gum_arm_writer_has_literal_refs (self))
    return;

  num_refs = self->literal_refs.length;
  if (num_refs == 0)
    return;

  first_slot = self->code;
  last_slot = first_slot;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumArmLiteralRef * r;
    guint32 * cur_slot;
    gint64 distance_in_words;
    guint32 insn;

    r = gum_metal_array_element_at (&self->literal_refs, ref_index);

    for (cur_slot = first_slot; cur_slot != last_slot; cur_slot++)
    {
      if (*cur_slot == r->val)
        break;
    }

    if (cur_slot == last_slot)
    {
      *cur_slot = r->val;
      last_slot++;
    }

    distance_in_words = cur_slot - (r->insn + 2);

    insn = GUINT32_FROM_LE (*r->insn);
    insn |= ABS (distance_in_words) * 4;
    if (distance_in_words >= 0)
      insn |= 1 << 23;
    *r->insn = GUINT32_TO_LE (insn);
  }

  self->code = last_slot;
  self->pc += (guint8 *) last_slot - (guint8 *) first_slot;

  gum_metal_array_remove_all (&self->literal_refs);
}

static guint32
gum_arm_condify (arm_cc cc)
{
  return (cc - 1) << 28;
}

static guint32
gum_arm_shiftify (arm_shifter shifter)
{
  guint32 code = 0;

  switch (shifter)
  {
    case ARM_SFT_INVALID:
    case ARM_SFT_LSL:
      code = 0;
      break;
    case ARM_SFT_LSR:
      code = 1;
      break;
    case ARM_SFT_ASR:
      code = 2;
      break;
    case ARM_SFT_ROR:
      code = 3;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return code << 5;
}
