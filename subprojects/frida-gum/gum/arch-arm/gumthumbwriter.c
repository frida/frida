/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2019 Jon Wilson <jonwilson@zepler.net>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthumbwriter.h"

#include "gumarmreg.h"
#include "gumlibc.h"
#include "gummemory.h"
#include "gumprocess.h"

typedef guint GumThumbLabelRefType;
typedef struct _GumThumbLabelRef GumThumbLabelRef;
typedef struct _GumThumbLiteralRef GumThumbLiteralRef;
typedef guint GumThumbMemoryOperation;

enum _GumThumbLabelRefType
{
  GUM_THUMB_B_T1,
  GUM_THUMB_B_T2,
  GUM_THUMB_B_T3,
  GUM_THUMB_B_T4,
  GUM_THUMB_BL_T1,
  GUM_THUMB_CBZ_T1,
  GUM_THUMB_CBNZ_T1,
};

struct _GumThumbLabelRef
{
  gconstpointer id;
  GumThumbLabelRefType type;
  guint16 * insn;
};

struct _GumThumbLiteralRef
{
  guint32 val;
  guint16 * insn;
  GumAddress pc;
};

enum _GumThumbMemoryOperation
{
  GUM_THUMB_MEMORY_LOAD,
  GUM_THUMB_MEMORY_STORE
};

static void gum_thumb_writer_reset_refs (GumThumbWriter * self);

static void gum_thumb_writer_put_argument_list_setup (GumThumbWriter * self,
    guint n_args, const GumArgument * args);
static void gum_thumb_writer_put_argument_list_setup_va (GumThumbWriter * self,
    guint n_args, va_list args);
static void gum_thumb_writer_put_argument_list_teardown (GumThumbWriter * self,
    guint n_args);
static void gum_thumb_writer_put_branch_imm (GumThumbWriter * self,
    GumAddress target, gboolean link, gboolean thumb);
static gboolean gum_thumb_writer_put_push_or_pop_regs (GumThumbWriter * self,
    guint16 narrow_template, guint16 wide_template, GumArmMetaReg special_reg,
    guint n_regs, const arm_reg * regs);
static gboolean gum_thumb_writer_put_push_or_pop_regs_va (GumThumbWriter * self,
    guint16 narrow_template, guint16 wide_template, GumArmMetaReg special_reg,
    guint n_regs, arm_reg first_reg, va_list args);
static gboolean gum_thumb_writer_put_vector_push_or_pop_range (
    GumThumbWriter * self, guint16 upper_template, arm_reg first_reg,
    arm_reg last_reg);
static gboolean gum_thumb_writer_put_transfer_reg_reg_offset (
    GumThumbWriter * self, GumThumbMemoryOperation operation, arm_reg left_reg,
    arm_reg right_reg, gsize right_offset);
static void gum_thumb_writer_put_it_al (GumThumbWriter * self);

static gboolean gum_thumb_writer_try_commit_label_refs (GumThumbWriter * self);
static gboolean gum_thumb_writer_do_commit_label (GumThumbLabelRef * r,
    const guint16 * target_insn);
static void gum_thumb_writer_maybe_commit_literals (GumThumbWriter * self);
static void gum_thumb_writer_commit_literals (GumThumbWriter * self);

static gboolean gum_instruction_is_t1_load (guint16 instruction);

GumThumbWriter *
gum_thumb_writer_new (gpointer code_address)
{
  GumThumbWriter * writer;

  writer = g_slice_new (GumThumbWriter);

  gum_thumb_writer_init (writer, code_address);

  return writer;
}

GumThumbWriter *
gum_thumb_writer_ref (GumThumbWriter * writer)
{
  g_atomic_int_inc (&writer->ref_count);

  return writer;
}

void
gum_thumb_writer_unref (GumThumbWriter * writer)
{
  if (g_atomic_int_dec_and_test (&writer->ref_count))
  {
    gum_thumb_writer_clear (writer);

    g_slice_free (GumThumbWriter, writer);
  }
}

void
gum_thumb_writer_init (GumThumbWriter * writer,
                       gpointer code_address)
{
  writer->ref_count = 1;
  writer->flush_on_destroy = TRUE;

  writer->target_os = gum_process_get_native_os ();

  writer->label_defs = NULL;
  writer->label_refs.data = NULL;
  writer->literal_refs.data = NULL;

  gum_thumb_writer_reset (writer, code_address);
}

static gboolean
gum_thumb_writer_has_label_defs (GumThumbWriter * self)
{
  return self->label_defs != NULL;
}

static gboolean
gum_thumb_writer_has_label_refs (GumThumbWriter * self)
{
  return self->label_refs.data != NULL;
}

static gboolean
gum_thumb_writer_has_literal_refs (GumThumbWriter * self)
{
  return self->literal_refs.data != NULL;
}

void
gum_thumb_writer_clear (GumThumbWriter * writer)
{
  if (writer->flush_on_destroy)
    gum_thumb_writer_flush (writer);

  if (gum_thumb_writer_has_label_defs (writer))
    gum_metal_hash_table_unref (writer->label_defs);

  if (gum_thumb_writer_has_label_refs (writer))
    gum_metal_array_free (&writer->label_refs);

  if (gum_thumb_writer_has_literal_refs (writer))
    gum_metal_array_free (&writer->literal_refs);
}

void
gum_thumb_writer_reset (GumThumbWriter * writer,
                        gpointer code_address)
{
  writer->base = code_address;
  writer->code = code_address;
  writer->pc = GUM_ADDRESS (code_address);

  if (gum_thumb_writer_has_label_defs (writer))
    gum_metal_hash_table_remove_all (writer->label_defs);

  gum_thumb_writer_reset_refs (writer);
}

static void
gum_thumb_writer_reset_refs (GumThumbWriter * self)
{
  if (gum_thumb_writer_has_label_refs (self))
    gum_metal_array_remove_all (&self->label_refs);

  if (gum_thumb_writer_has_literal_refs (self))
    gum_metal_array_remove_all (&self->literal_refs);

  self->earliest_literal_insn = NULL;
}

void
gum_thumb_writer_set_target_os (GumThumbWriter * self,
                                GumOS os)
{
  self->target_os = os;
}

gpointer
gum_thumb_writer_cur (GumThumbWriter * self)
{
  return self->code;
}

guint
gum_thumb_writer_offset (GumThumbWriter * self)
{
  return (guint) (self->code - self->base) * sizeof (guint16);
}

void
gum_thumb_writer_skip (GumThumbWriter * self,
                       guint n_bytes)
{
  self->code = (guint16 *) (((guint8 *) self->code) + n_bytes);
  self->pc += n_bytes;
}

gboolean
gum_thumb_writer_flush (GumThumbWriter * self)
{
  if (!gum_thumb_writer_try_commit_label_refs (self))
    goto error;

  gum_thumb_writer_commit_literals (self);

  return TRUE;

error:
  {
    gum_thumb_writer_reset_refs (self);

    return FALSE;
  }
}

gboolean
gum_thumb_writer_put_label (GumThumbWriter * self,
                            gconstpointer id)
{
  if (!gum_thumb_writer_has_label_defs (self))
    self->label_defs = gum_metal_hash_table_new (NULL, NULL);

  if (gum_metal_hash_table_lookup (self->label_defs, id) != NULL)
    return FALSE;

  gum_metal_hash_table_insert (self->label_defs, (gpointer) id, self->code);

  return TRUE;
}

gboolean
gum_thumb_writer_commit_label (GumThumbWriter * self,
                               gconstpointer id)
{
  guint num_refs, ref_index;

  if (!gum_thumb_writer_has_label_refs (self))
    return FALSE;

  if (!gum_thumb_writer_has_label_defs (self))
    return FALSE;

  num_refs = self->label_refs.length;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumThumbLabelRef * r;
    const guint16 * target_insn;

    r = gum_metal_array_element_at (&self->label_refs, ref_index);
    if (r->id != id)
      continue;

    target_insn = gum_metal_hash_table_lookup (self->label_defs, r->id);
    if (target_insn == NULL)
      return FALSE;

    if (!gum_thumb_writer_do_commit_label (r, target_insn))
      return FALSE;

    gum_metal_array_remove_at (&self->label_refs, ref_index);

    return TRUE;
  }

  return FALSE;
}

static void
gum_thumb_writer_add_label_reference_here (GumThumbWriter * self,
                                           gconstpointer id,
                                           GumThumbLabelRefType type)
{
  GumThumbLabelRef * r;

  if (!gum_thumb_writer_has_label_refs (self))
    gum_metal_array_init (&self->label_refs, sizeof (GumThumbLabelRef));

  r = gum_metal_array_append (&self->label_refs);
  r->id = id;
  r->type = type;
  r->insn = self->code;
}

static void
gum_thumb_writer_add_literal_reference_here (GumThumbWriter * self,
                                             guint32 val)
{
  GumThumbLiteralRef * r;

  if (!gum_thumb_writer_has_literal_refs (self))
    gum_metal_array_init (&self->literal_refs, sizeof (GumThumbLiteralRef));

  r = gum_metal_array_append (&self->literal_refs);
  r->val = val;
  r->insn = self->code;
  r->pc = self->pc + 4;

  if (self->earliest_literal_insn == NULL)
    self->earliest_literal_insn = r->insn;
}

void
gum_thumb_writer_put_call_address_with_arguments (GumThumbWriter * self,
                                                  GumAddress func,
                                                  guint n_args,
                                                  ...)
{
  va_list args;

  va_start (args, n_args);
  gum_thumb_writer_put_argument_list_setup_va (self, n_args, args);
  va_end (args);

  gum_thumb_writer_put_ldr_reg_address (self, ARM_REG_LR, func);
  gum_thumb_writer_put_blx_reg (self, ARM_REG_LR);

  gum_thumb_writer_put_argument_list_teardown (self, n_args);
}

void
gum_thumb_writer_put_call_address_with_arguments_array (
    GumThumbWriter * self,
    GumAddress func,
    guint n_args,
    const GumArgument * args)
{
  gum_thumb_writer_put_argument_list_setup (self, n_args, args);

  gum_thumb_writer_put_ldr_reg_address (self, ARM_REG_LR, func);
  gum_thumb_writer_put_blx_reg (self, ARM_REG_LR);

  gum_thumb_writer_put_argument_list_teardown (self, n_args);
}

void
gum_thumb_writer_put_call_reg_with_arguments (GumThumbWriter * self,
                                              arm_reg reg,
                                              guint n_args,
                                              ...)
{
  va_list args;

  va_start (args, n_args);
  gum_thumb_writer_put_argument_list_setup_va (self, n_args, args);
  va_end (args);

  gum_thumb_writer_put_blx_reg (self, reg);

  gum_thumb_writer_put_argument_list_teardown (self, n_args);
}

void
gum_thumb_writer_put_call_reg_with_arguments_array (GumThumbWriter * self,
                                                    arm_reg reg,
                                                    guint n_args,
                                                    const GumArgument * args)
{
  gum_thumb_writer_put_argument_list_setup (self, n_args, args);

  gum_thumb_writer_put_blx_reg (self, reg);

  gum_thumb_writer_put_argument_list_teardown (self, n_args);
}

static void
gum_thumb_writer_put_argument_list_setup (GumThumbWriter * self,
                                          guint n_args,
                                          const GumArgument * args)
{
  guint n_stack_args;
  gint arg_index;

  n_stack_args = MAX ((gint) n_args - 4, 0);
  if (n_stack_args % 2 != 0)
    gum_thumb_writer_put_sub_reg_imm (self, ARM_REG_SP, 4);

  for (arg_index = (gint) n_args - 1; arg_index >= 0; arg_index--)
  {
    const GumArgument * arg = &args[arg_index];
    arm_reg r = ARM_REG_R0 + arg_index;

    if (arg_index < 4)
    {
      if (arg->type == GUM_ARG_ADDRESS)
      {
        gum_thumb_writer_put_ldr_reg_address (self, r, arg->value.address);
      }
      else
      {
        if (arg->value.reg != r)
          gum_thumb_writer_put_mov_reg_reg (self, r, arg->value.reg);
      }
    }
    else
    {
      if (arg->type == GUM_ARG_ADDRESS)
      {
        gum_thumb_writer_put_ldr_reg_address (self, ARM_REG_R0,
            arg->value.address);
        gum_thumb_writer_put_push_regs (self, 1, ARM_REG_R0);
      }
      else
      {
        gum_thumb_writer_put_push_regs (self, 1, arg->value.reg);
      }
    }
  }
}

static void
gum_thumb_writer_put_argument_list_setup_va (GumThumbWriter * self,
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

  gum_thumb_writer_put_argument_list_setup (self, n_args, arg_values);
}

static void
gum_thumb_writer_put_argument_list_teardown (GumThumbWriter * self,
                                             guint n_args)
{
  guint n_stack_args, n_stack_slots;

  n_stack_args = MAX ((gint) n_args - 4, 0);
  if (n_stack_args == 0)
    return;

  n_stack_slots = n_stack_args;
  if (n_stack_slots % 2 != 0)
    n_stack_slots++;

  gum_thumb_writer_put_add_reg_imm (self, ARM_REG_SP, n_stack_slots * 4);
}

void
gum_thumb_writer_put_branch_address (GumThumbWriter * self,
                                     GumAddress address)
{
  if (gum_thumb_writer_can_branch_directly_between (self, self->pc, address))
  {
    gum_thumb_writer_put_b_imm (self, address);
  }
  else
  {
    gum_thumb_writer_put_push_regs (self, 2, ARM_REG_R0, ARM_REG_R1);
    gum_thumb_writer_put_ldr_reg_address (self, ARM_REG_R0, address | 1);
    gum_thumb_writer_put_str_reg_reg_offset (self, ARM_REG_R0, ARM_REG_SP, 4);
    gum_thumb_writer_put_pop_regs (self, 2, ARM_REG_R0, ARM_REG_PC);
  }
}

gboolean
gum_thumb_writer_can_branch_directly_between (GumThumbWriter * self,
                                              GumAddress from,
                                              GumAddress to)
{
  gint64 distance = (gint64) to - (gint64) from;

  return GUM_IS_WITHIN_INT24_RANGE (distance);
}

void
gum_thumb_writer_put_b_imm (GumThumbWriter * self,
                            GumAddress target)
{
  gum_thumb_writer_put_branch_imm (self, target, FALSE, TRUE);
}

void
gum_thumb_writer_put_b_label (GumThumbWriter * self,
                              gconstpointer label_id)
{
  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_B_T2);
  gum_thumb_writer_put_instruction (self, 0xe000);
}

void
gum_thumb_writer_put_b_label_wide (GumThumbWriter * self,
                                   gconstpointer label_id)
{
  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_B_T4);
  gum_thumb_writer_put_branch_imm (self, 0, FALSE, TRUE);
}

void
gum_thumb_writer_put_bx_reg (GumThumbWriter * self,
                             arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_put_instruction (self, 0x4700 | (ri.index << 3));
}

void
gum_thumb_writer_put_bl_imm (GumThumbWriter * self,
                             GumAddress target)
{
  gum_thumb_writer_put_branch_imm (self, target, TRUE, TRUE);
}

void
gum_thumb_writer_put_bl_label (GumThumbWriter * self,
                               gconstpointer label_id)
{
  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_BL_T1);
  gum_thumb_writer_put_branch_imm (self, 0, TRUE, TRUE);
}

void
gum_thumb_writer_put_blx_imm (GumThumbWriter * self,
                              GumAddress target)
{
  gum_thumb_writer_put_branch_imm (self, target, TRUE, FALSE);
}

void
gum_thumb_writer_put_blx_reg (GumThumbWriter * self,
                              arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_put_instruction (self, 0x4780 | (ri.index << 3));
}

static void
gum_thumb_writer_put_branch_imm (GumThumbWriter * self,
                                 GumAddress target,
                                 gboolean link,
                                 gboolean thumb)
{
  guint16 s, j1, j2, imm10, imm11;

  if (target != 0)
  {
    union
    {
      gint32 i;
      guint32 u;
    } distance;
    guint16 i1, i2;

    distance.i = ((gint32) (target & ~((GumAddress) 1)) -
        (gint32) (self->pc + 4)) / 2;

    s =  (distance.u >> 23) & 1;
    i1 = (distance.u >> 22) & 1;
    i2 = (distance.u >> 21) & 1;
    j1 = (i1 ^ 1) ^ s;
    j2 = (i2 ^ 1) ^ s;

    imm10 = (distance.u >> 11) & GUM_INT10_MASK;
    imm11 =  distance.u        & GUM_INT11_MASK;
  }
  else
  {
    s = 0;
    j1 = 0;
    j2 = 0;
    imm10 = 0;
    imm11 = 0;
  }

  gum_thumb_writer_put_instruction_wide (self,
      0xf000 | (s << 10) | imm10,
      0x8000 | (link << 14) | (j1 << 13) | (thumb << 12) | (j2 << 11) | imm11);
}

void
gum_thumb_writer_put_cmp_reg_imm (GumThumbWriter * self,
                                  arm_reg reg,
                                  guint8 imm_value)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_put_instruction (self, 0x2800 | (ri.index << 8) | imm_value);
}

void
gum_thumb_writer_put_beq_label (GumThumbWriter * self,
                                gconstpointer label_id)
{
  gum_thumb_writer_put_b_cond_label (self, ARM_CC_EQ, label_id);
}

void
gum_thumb_writer_put_bne_label (GumThumbWriter * self,
                                gconstpointer label_id)
{
  gum_thumb_writer_put_b_cond_label (self, ARM_CC_NE, label_id);
}

void
gum_thumb_writer_put_b_cond_label (GumThumbWriter * self,
                                   arm_cc cc,
                                   gconstpointer label_id)
{
  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_B_T1);
  gum_thumb_writer_put_instruction (self, 0xd000 | ((cc - 1) << 8));
}

void
gum_thumb_writer_put_b_cond_label_wide (GumThumbWriter * self,
                                        arm_cc cc,
                                        gconstpointer label_id)
{
  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_B_T3);
  gum_thumb_writer_put_instruction_wide (self,
      0xf000 | ((cc - 1) << 6),
      0x8000);
}

void
gum_thumb_writer_put_cbz_reg_label (GumThumbWriter * self,
                                    arm_reg reg,
                                    gconstpointer label_id)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_CBZ_T1);
  gum_thumb_writer_put_instruction (self, 0xb100 | ri.index);
}

void
gum_thumb_writer_put_cbnz_reg_label (GumThumbWriter * self,
                                     arm_reg reg,
                                     gconstpointer label_id)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_add_label_reference_here (self, label_id, GUM_THUMB_CBNZ_T1);
  gum_thumb_writer_put_instruction (self, 0xb900 | ri.index);
}

gboolean
gum_thumb_writer_put_push_regs (GumThumbWriter * self,
                                guint n_regs,
                                arm_reg first_reg,
                                ...)
{
  gboolean success;
  va_list args;

  va_start (args, first_reg);
  success = gum_thumb_writer_put_push_or_pop_regs_va (self, 0xb400, 0xe92d,
      GUM_ARM_MREG_LR, n_regs, first_reg, args);
  va_end (args);

  return success;
}

gboolean
gum_thumb_writer_put_push_regs_array (GumThumbWriter * self,
                                      guint n_regs,
                                      const arm_reg * regs)
{
  return gum_thumb_writer_put_push_or_pop_regs (self, 0xb400, 0xe92d,
      GUM_ARM_MREG_LR, n_regs, regs);
}

gboolean
gum_thumb_writer_put_pop_regs (GumThumbWriter * self,
                               guint n_regs,
                               arm_reg first_reg,
                               ...)
{
  gboolean success;
  va_list args;

  va_start (args, first_reg);
  success = gum_thumb_writer_put_push_or_pop_regs_va (self, 0xbc00, 0xe8bd,
      GUM_ARM_MREG_PC, n_regs, first_reg, args);
  va_end (args);

  return success;
}

gboolean
gum_thumb_writer_put_pop_regs_array (GumThumbWriter * self,
                                     guint n_regs,
                                     const arm_reg * regs)
{
  return gum_thumb_writer_put_push_or_pop_regs (self, 0xbc00, 0xe8bd,
      GUM_ARM_MREG_PC, n_regs, regs);
}

static gboolean
gum_thumb_writer_put_push_or_pop_regs (GumThumbWriter * self,
                                       guint16 narrow_template,
                                       guint16 wide_template,
                                       GumArmMetaReg special_reg,
                                       guint n_regs,
                                       const arm_reg * regs)
{
  GumArmRegInfo * items;
  gboolean need_wide_instruction;
  guint reg_index;

  if (n_regs == 0)
    return FALSE;

  items = g_newa (GumArmRegInfo, n_regs);
  need_wide_instruction = FALSE;
  for (reg_index = 0; reg_index != n_regs; reg_index++)
  {
    GumArmRegInfo * ri = &items[reg_index];
    gboolean is_low_reg;

    gum_arm_reg_describe (regs[reg_index], ri);

    is_low_reg = (ri->meta >= GUM_ARM_MREG_R0 && ri->meta <= GUM_ARM_MREG_R7);
    if (!is_low_reg && ri->meta != special_reg)
      need_wide_instruction = TRUE;
  }

  if (need_wide_instruction)
  {
    guint16 mask = 0;

    gum_thumb_writer_put_instruction (self, wide_template);

    for (reg_index = 0; reg_index != n_regs; reg_index++)
    {
      const GumArmRegInfo * ri = &items[reg_index];

      mask |= (1 << ri->index);
    }

    gum_thumb_writer_put_instruction (self, mask);
  }
  else
  {
    guint16 insn = narrow_template;

    for (reg_index = 0; reg_index != n_regs; reg_index++)
    {
      const GumArmRegInfo * ri = &items[reg_index];

      if (ri->meta == special_reg)
        insn |= 0x0100;
      else
        insn |= (1 << ri->index);
    }

    gum_thumb_writer_put_instruction (self, insn);
  }

  return TRUE;
}

static gboolean
gum_thumb_writer_put_push_or_pop_regs_va (GumThumbWriter * self,
                                          guint16 narrow_template,
                                          guint16 wide_template,
                                          GumArmMetaReg special_reg,
                                          guint n_regs,
                                          arm_reg first_reg,
                                          va_list args)
{
  arm_reg * regs;
  guint reg_index;

  g_assert (n_regs != 0);

  regs = g_newa (arm_reg, n_regs);

  for (reg_index = 0; reg_index != n_regs; reg_index++)
  {
    regs[reg_index] = (reg_index == 0) ? first_reg : va_arg (args, arm_reg);
  }

  return gum_thumb_writer_put_push_or_pop_regs (self, narrow_template,
      wide_template, special_reg, n_regs, regs);
}

gboolean
gum_thumb_writer_put_vpush_range (GumThumbWriter * self,
                                  arm_reg first_reg,
                                  arm_reg last_reg)
{
  return gum_thumb_writer_put_vector_push_or_pop_range (self, 0xed2d, first_reg,
      last_reg);
}

gboolean
gum_thumb_writer_put_vpop_range (GumThumbWriter * self,
                                 arm_reg first_reg,
                                 arm_reg last_reg)
{
  return gum_thumb_writer_put_vector_push_or_pop_range (self, 0xecbd, first_reg,
      last_reg);
}

static gboolean
gum_thumb_writer_put_vector_push_or_pop_range (GumThumbWriter * self,
                                               guint16 upper_template,
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

  gum_thumb_writer_put_instruction_wide (self,
      upper_template | ((rf.index >> 4) << 6) |
      ((rf.index & GUM_INT4_MASK) << 12),
      0x0a00 | ((rf.width == 64) << 8) | imm8);

  return TRUE;
}

gboolean
gum_thumb_writer_put_ldr_reg_address (GumThumbWriter * self,
                                      arm_reg reg,
                                      GumAddress address)
{
  return gum_thumb_writer_put_ldr_reg_u32 (self, reg, (guint32) address);
}

gboolean
gum_thumb_writer_put_ldr_reg_u32 (GumThumbWriter * self,
                                  arm_reg reg,
                                  guint32 val)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_add_literal_reference_here (self, val);

  if (ri.meta <= GUM_ARM_MREG_R7)
  {
    gum_thumb_writer_put_instruction (self, 0x4800 | (ri.index << 8));
  }
  else
  {
    gboolean add = TRUE;

    gum_thumb_writer_put_instruction_wide (self,
        0xf85f | (add << 7),
        (ri.index << 12));
  }

  return TRUE;
}

void
gum_thumb_writer_put_ldr_reg_reg (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  arm_reg src_reg)
{
  gum_thumb_writer_put_ldr_reg_reg_offset (self, dst_reg, src_reg, 0);
}

gboolean
gum_thumb_writer_put_ldr_reg_reg_offset (GumThumbWriter * self,
                                         arm_reg dst_reg,
                                         arm_reg src_reg,
                                         gsize src_offset)
{
  return gum_thumb_writer_put_transfer_reg_reg_offset (self,
      GUM_THUMB_MEMORY_LOAD, dst_reg, src_reg, src_offset);
}

void
gum_thumb_writer_put_ldrb_reg_reg (GumThumbWriter * self,
                                   arm_reg dst_reg,
                                   arm_reg src_reg)
{
  GumArmRegInfo dst, src;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (src_reg, &src);

  gum_thumb_writer_put_instruction (self, 0x7800 | (src.index << 3) |
      dst.index);
}

void
gum_thumb_writer_put_ldrh_reg_reg (GumThumbWriter * self,
                                   arm_reg dst_reg,
                                   arm_reg src_reg)
{
  GumArmRegInfo dst, src;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (src_reg, &src);

  gum_thumb_writer_put_instruction (self, 0x8800 | (src.index << 3) |
      dst.index);
}

gboolean
gum_thumb_writer_put_vldr_reg_reg_offset (GumThumbWriter * self,
                                          arm_reg dst_reg,
                                          arm_reg src_reg,
                                          gssize src_offset)
{
  GumArmRegInfo dst, src;
  guint16 u, d, vd, size;
  gsize abs_src_offset;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (src_reg, &src);

  u = src_offset >= 0;

  abs_src_offset = ABS (src_offset) / 4;
  if (abs_src_offset > G_MAXUINT8)
    return FALSE;

  if (dst.meta >= GUM_ARM_MREG_S0 && dst.meta <= GUM_ARM_MREG_S31)
  {
    vd = (dst.index >> 1) & GUM_INT4_MASK;
    d = dst.index & 1;

    size = 0x2;
  }
  else
  {
    d = (dst.index >> 4) & 1;
    vd = dst.index & GUM_INT4_MASK;

    size = 0x3;
  }

  gum_thumb_writer_put_instruction_wide (self,
      0xed10 | (u << 7) | (d << 6) | src.index,
      0x0800 | (vd << 12) | (size << 8) | abs_src_offset);

  return TRUE;
}

void
gum_thumb_writer_put_ldmia_reg_mask (GumThumbWriter * self,
                                     arm_reg reg,
                                     guint16 mask)
{
  GumArmRegInfo ri;
  const guint16 valid_short_reg_mask = 0x80ff;

  gum_arm_reg_describe (reg, &ri);

  if (reg == ARM_REG_SP && (mask & ~valid_short_reg_mask) == 0)
  {
    const gboolean includes_pc = (mask & 0x8000) != 0;

    gum_thumb_writer_put_instruction (self, 0xbc00 | (includes_pc << 8) |
        (mask & GUM_INT8_MASK));
  }
  else
  {
    gum_thumb_writer_put_instruction_wide (self, 0xe8b0 | ri.index, mask);
  }
}

void
gum_thumb_writer_put_str_reg_reg (GumThumbWriter * self,
                                  arm_reg src_reg,
                                  arm_reg dst_reg)
{
  gum_thumb_writer_put_str_reg_reg_offset (self, src_reg, dst_reg, 0);
}

gboolean
gum_thumb_writer_put_str_reg_reg_offset (GumThumbWriter * self,
                                         arm_reg src_reg,
                                         arm_reg dst_reg,
                                         gsize dst_offset)
{
  return gum_thumb_writer_put_transfer_reg_reg_offset (self,
      GUM_THUMB_MEMORY_STORE, src_reg, dst_reg, dst_offset);
}

static gboolean
gum_thumb_writer_put_transfer_reg_reg_offset (GumThumbWriter * self,
                                              GumThumbMemoryOperation operation,
                                              arm_reg left_reg,
                                              arm_reg right_reg,
                                              gsize right_offset)
{
  GumArmRegInfo lr, rr;

  gum_arm_reg_describe (left_reg, &lr);
  gum_arm_reg_describe (right_reg, &rr);

  if (lr.meta <= GUM_ARM_MREG_R7 &&
      (rr.meta <= GUM_ARM_MREG_R7 || rr.meta == GUM_ARM_MREG_SP) &&
      ((rr.meta == GUM_ARM_MREG_SP && right_offset <= 1020) ||
       (rr.meta != GUM_ARM_MREG_SP && right_offset <= 124)) &&
      (right_offset % 4) == 0)
  {
    guint16 insn;

    if (rr.meta == GUM_ARM_MREG_SP)
      insn = 0x9000 | (lr.index << 8) | (right_offset / 4);
    else
      insn = 0x6000 | (right_offset / 4) << 6 | (rr.index << 3) | lr.index;

    if (operation == GUM_THUMB_MEMORY_LOAD)
      insn |= 0x0800;

    gum_thumb_writer_put_instruction (self, insn);
  }
  else
  {
    if (right_offset > 4095)
      return FALSE;

    gum_thumb_writer_put_instruction_wide (self,
        0xf8c0 | ((operation == GUM_THUMB_MEMORY_LOAD) ? 0x0010 : 0x0000) |
            rr.index,
        (lr.index << 12) | right_offset);
  }

  return TRUE;
}

void
gum_thumb_writer_put_mov_reg_reg (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  arm_reg src_reg)
{
  GumArmRegInfo dst, src;
  guint16 insn;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (src_reg, &src);

  if (dst.meta <= GUM_ARM_MREG_R7 && src.meta <= GUM_ARM_MREG_R7)
  {
    insn = 0x1c00 | (src.index << 3) | dst.index;

    /* Here we emit “ADDS Rd, Rm, #0” so need to suppress flags */
    gum_thumb_writer_put_it_al (self);
  }
  else
  {
    guint16 dst_is_high;
    guint dst_index;

    if (dst.meta > GUM_ARM_MREG_R7)
    {
      dst_is_high = 1;
      dst_index = dst.index - GUM_ARM_MREG_R8;
    }
    else
    {
      dst_is_high = 0;
      dst_index = dst.index;
    }

    insn = 0x4600 | (dst_is_high << 7) | (src.index << 3) | dst_index;
  }

  gum_thumb_writer_put_instruction (self, insn);
}

void
gum_thumb_writer_put_mov_reg_u8 (GumThumbWriter * self,
                                 arm_reg dst_reg,
                                 guint8 imm_value)
{
  GumArmRegInfo dst;

  gum_arm_reg_describe (dst_reg, &dst);

  gum_thumb_writer_put_it_al (self);
  gum_thumb_writer_put_instruction (self, 0x2000 | (dst.index << 8) |
      imm_value);
}

void
gum_thumb_writer_put_mov_reg_cpsr (GumThumbWriter * self,
                                   arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_put_instruction (self, 0xf3ef);
  gum_thumb_writer_put_instruction (self, 0x8000 | ri.index << 8);
}

void
gum_thumb_writer_put_mov_cpsr_reg (GumThumbWriter * self,
                                   arm_reg reg)
{
  GumArmRegInfo ri;

  gum_arm_reg_describe (reg, &ri);

  gum_thumb_writer_put_instruction (self, 0xf380 | ri.index);
  gum_thumb_writer_put_instruction (self, 0x8900);
}

gboolean
gum_thumb_writer_put_add_reg_imm (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  gssize imm_value)
{
  GumArmRegInfo dst;
  guint16 sign_mask, insn;

  gum_arm_reg_describe (dst_reg, &dst);

  if (dst_reg != ARM_REG_SP && (dst_reg < ARM_REG_R0 || dst_reg > ARM_REG_R7))
    return FALSE;

  sign_mask = 0x0000;
  if (dst.meta == GUM_ARM_MREG_SP)
  {
    if (imm_value % 4 != 0)
      return FALSE;

    if (imm_value < 0)
      sign_mask = 0x0080;

    insn = 0xb000 | sign_mask | ABS (imm_value / 4);
  }
  else
  {
    if (imm_value < 0)
      sign_mask = 0x0800;

    insn = 0x3000 | sign_mask | (dst.index << 8) | ABS (imm_value);
    gum_thumb_writer_put_it_al (self);
  }

  gum_thumb_writer_put_instruction (self, insn);

  return TRUE;
}

void
gum_thumb_writer_put_add_reg_reg (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  arm_reg src_reg)
{
  gum_thumb_writer_put_add_reg_reg_reg (self, dst_reg, dst_reg, src_reg);
}

void
gum_thumb_writer_put_add_reg_reg_reg (GumThumbWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg left_reg,
                                      arm_reg right_reg)
{
  GumArmRegInfo dst, left, right;
  guint16 insn;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);
  gum_arm_reg_describe (right_reg, &right);

  if (left.meta == dst.meta)
  {
    insn = 0x4400;

    if (dst.meta <= GUM_ARM_MREG_R7)
      insn |= dst.index;
    else
      insn |= 0x0080 | (dst.index - GUM_ARM_MREG_R8);
    insn |= (right.index << 3);
  }
  else
  {
    insn = 0x1800 | (right.index << 6) | (left.index << 3) | dst.index;
    gum_thumb_writer_put_it_al (self);
  }

  gum_thumb_writer_put_instruction (self, insn);
}

gboolean
gum_thumb_writer_put_add_reg_reg_imm (GumThumbWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg left_reg,
                                      gssize right_value)
{
  GumArmRegInfo dst, left;
  guint16 insn;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);

  if (left.meta == dst.meta)
  {
    return gum_thumb_writer_put_add_reg_imm (self, dst_reg, right_value);
  }

  if (dst_reg < ARM_REG_R0 || dst_reg > ARM_REG_R7)
    return FALSE;

  if (left_reg != ARM_REG_SP && left_reg != ARM_REG_PC &&
      (left_reg < ARM_REG_R0 || left_reg > ARM_REG_R7))
  {
    return FALSE;
  }

  if (left.meta == GUM_ARM_MREG_SP || left.meta == GUM_ARM_MREG_PC)
  {
    guint16 base_mask;

    if (right_value < 0 || right_value % 4 != 0)
      return FALSE;

    if (left.meta == GUM_ARM_MREG_SP)
      base_mask = 0x0800;
    else
      base_mask = 0x0000;

    /* ADR instruction doesn't modify flags */
    insn = 0xa000 | base_mask | (dst.index << 8) | (right_value / 4);
  }
  else
  {
    guint16 sign_mask = 0x0000;

    if (ABS (right_value) > 7)
      return FALSE;

    if (right_value < 0)
      sign_mask = 0x0200;

    insn = 0x1c00 | sign_mask | (ABS (right_value) << 6) | (left.index << 3) |
        dst.index;
    gum_thumb_writer_put_it_al (self);
  }

  gum_thumb_writer_put_instruction (self, insn);

  return TRUE;
}

static void
gum_thumb_writer_put_it_al (GumThumbWriter * self)
{
  gum_thumb_writer_put_instruction (self, 0xbfe8);
}

gboolean
gum_thumb_writer_put_sub_reg_imm (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  gssize imm_value)
{
  return gum_thumb_writer_put_add_reg_imm (self, dst_reg, -imm_value);
}

void
gum_thumb_writer_put_sub_reg_reg (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  arm_reg src_reg)
{
  gum_thumb_writer_put_sub_reg_reg_reg (self, dst_reg, dst_reg, src_reg);
}

void
gum_thumb_writer_put_sub_reg_reg_reg (GumThumbWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg left_reg,
                                      arm_reg right_reg)
{
  GumArmRegInfo dst, left, right;
  guint16 insn;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);
  gum_arm_reg_describe (right_reg, &right);

  insn = 0x1a00 | (right.index << 6) | (left.index << 3) | dst.index;

  gum_thumb_writer_put_it_al (self);
  gum_thumb_writer_put_instruction (self, insn);
}

gboolean
gum_thumb_writer_put_sub_reg_reg_imm (GumThumbWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg left_reg,
                                      gssize right_value)
{
  return gum_thumb_writer_put_add_reg_reg_imm (self, dst_reg, left_reg,
      -right_value);
}

gboolean
gum_thumb_writer_put_and_reg_reg_imm (GumThumbWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg left_reg,
                                      gssize right_value)
{
  GumArmRegInfo dst, left;
  guint16 imm8, insn_high, insn_low;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);

  /*
   * Thumb does allow up to a 12bit immediate, but the encoded form for this is
   * complex and we don't yet need it for our use-cases.
   */
  if (!GUM_IS_WITHIN_UINT8_RANGE (right_value))
    return FALSE;

  imm8 = right_value & 0xff;
  insn_high = 0xf000 | left.index;
  insn_low = (dst.index << 8) | imm8;

  gum_thumb_writer_put_instruction_wide (self, insn_high, insn_low);

  return TRUE;
}

gboolean
gum_thumb_writer_put_or_reg_reg_imm (GumThumbWriter * self,
                                     arm_reg dst_reg,
                                     arm_reg left_reg,
                                     gssize right_value)
{
  GumArmRegInfo dst, left;
  guint16 imm8, insn_high, insn_low;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);

  /*
   * Thumb does allow up to a 12bit immediate, but the encoded form for this is
   * complex and we don't yet need it for our use-cases.
   */
  if (!GUM_IS_WITHIN_UINT8_RANGE (right_value))
    return FALSE;

  imm8 = right_value & 0xff;
  insn_high = 0xf040 | left.index;
  insn_low = (dst.index << 8) | imm8;

  gum_thumb_writer_put_instruction_wide (self, insn_high, insn_low);

  return TRUE;
}

gboolean
gum_thumb_writer_put_lsl_reg_reg_imm (GumThumbWriter * self,
                                      arm_reg dst_reg,
                                      arm_reg left_reg,
                                      guint8 right_value)
{
  gum_thumb_writer_put_it_al (self);

  return gum_thumb_writer_put_lsls_reg_reg_imm (self, dst_reg, left_reg,
      right_value);
}

gboolean
gum_thumb_writer_put_lsls_reg_reg_imm (GumThumbWriter * self,
                                       arm_reg dst_reg,
                                       arm_reg left_reg,
                                       guint8 right_value)
{
  GumArmRegInfo dst, left;

  if (right_value == 0 || right_value > 31)
    return FALSE;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);

  gum_thumb_writer_put_instruction (self, 0x0000 | (right_value << 6) |
      (left.index << 3) | dst.index);

  return TRUE;
}

gboolean
gum_thumb_writer_put_lsrs_reg_reg_imm (GumThumbWriter * self,
                                       arm_reg dst_reg,
                                       arm_reg left_reg,
                                       guint8 right_value)
{
  GumArmRegInfo dst, left;

  if (right_value == 0 || right_value > 31)
    return FALSE;

  gum_arm_reg_describe (dst_reg, &dst);
  gum_arm_reg_describe (left_reg, &left);

  gum_thumb_writer_put_instruction (self, 0x0800 | (right_value << 6) |
      (left.index << 3) | dst.index);

  return TRUE;
}

gboolean
gum_thumb_writer_put_mrs_reg_reg (GumThumbWriter * self,
                                  arm_reg dst_reg,
                                  arm_sysreg src_reg)
{
  GumArmRegInfo dst;

  gum_arm_reg_describe (dst_reg, &dst);

  if (dst.meta > GUM_ARM_MREG_R12)
    return FALSE;
  if (src_reg != ARM_SYSREG_APSR_NZCVQ)
    return FALSE;

  gum_thumb_writer_put_instruction_wide (self,
      0xf3ef,
      0x8000 | (dst.index << 8));

  return TRUE;
}

gboolean
gum_thumb_writer_put_msr_reg_reg (GumThumbWriter * self,
                                  arm_sysreg dst_reg,
                                  arm_reg src_reg)
{
  GumArmRegInfo src;

  gum_arm_reg_describe (src_reg, &src);

  if (dst_reg != ARM_SYSREG_APSR_NZCVQ)
    return FALSE;
  if (src.meta > GUM_ARM_MREG_R12)
    return FALSE;

  gum_thumb_writer_put_instruction_wide (self,
      0xf380 | src.index,
      0x8800);

  return TRUE;
}

void
gum_thumb_writer_put_nop (GumThumbWriter * self)
{
  gum_thumb_writer_put_instruction (self, 0xbf00);
}

void
gum_thumb_writer_put_bkpt_imm (GumThumbWriter * self,
                               guint8 imm)
{
  gum_thumb_writer_put_instruction (self, 0xbe00 | imm);
}

void
gum_thumb_writer_put_breakpoint (GumThumbWriter * self)
{
  switch (self->target_os)
  {
    case GUM_OS_LINUX:
    case GUM_OS_ANDROID:
      gum_thumb_writer_put_instruction (self, 0xde01);
      break;
    default:
      gum_thumb_writer_put_bkpt_imm (self, 0);
      gum_thumb_writer_put_bx_reg (self, ARM_REG_LR);
      break;
  }
}

void
gum_thumb_writer_put_instruction (GumThumbWriter * self,
                                  guint16 insn)
{
  *self->code++ = GUINT16_TO_LE (insn);
  self->pc += 2;

  gum_thumb_writer_maybe_commit_literals (self);
}

void
gum_thumb_writer_put_instruction_wide (GumThumbWriter * self,
                                       guint16 upper,
                                       guint16 lower)
{
  *self->code++ = GUINT16_TO_LE (upper);
  *self->code++ = GUINT16_TO_LE (lower);
  self->pc += 4;

  gum_thumb_writer_maybe_commit_literals (self);
}

gboolean
gum_thumb_writer_put_bytes (GumThumbWriter * self,
                            const guint8 * data,
                            guint n)
{
  if (n % 2 != 0)
    return FALSE;

  gum_memcpy (self->code, data, n);
  self->code += n / sizeof (guint16);
  self->pc += n;

  gum_thumb_writer_maybe_commit_literals (self);

  return TRUE;
}

static gboolean
gum_thumb_writer_try_commit_label_refs (GumThumbWriter * self)
{
  guint num_refs, ref_index;

  if (!gum_thumb_writer_has_label_refs (self))
    return TRUE;

  if (!gum_thumb_writer_has_label_defs (self))
    return FALSE;

  num_refs = self->label_refs.length;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumThumbLabelRef * r;
    const guint16 * target_insn;

    r = gum_metal_array_element_at (&self->label_refs, ref_index);

    target_insn = gum_metal_hash_table_lookup (self->label_defs, r->id);
    if (target_insn == NULL)
      return FALSE;

    if (!gum_thumb_writer_do_commit_label (r, target_insn))
      return FALSE;
  }

  gum_metal_array_remove_all (&self->label_refs);

  return TRUE;
}

static gboolean
gum_thumb_writer_do_commit_label (GumThumbLabelRef * r,
                                  const guint16 * target_insn)
{
  gssize distance;
  guint16 insn;

  distance = target_insn - (r->insn + 2);

  insn = GUINT16_FROM_LE (*r->insn);
  switch (r->type)
  {
    case GUM_THUMB_B_T1:
      if (!GUM_IS_WITHIN_INT8_RANGE (distance))
        return FALSE;
      insn |= distance & GUM_INT8_MASK;
      break;
    case GUM_THUMB_B_T2:
      if (!GUM_IS_WITHIN_INT11_RANGE (distance))
        return FALSE;
      insn |= distance & GUM_INT11_MASK;
      break;
    case GUM_THUMB_B_T3:
    {
      union
      {
        gint32 i;
        guint32 u;
      } distance_word;
      guint32 s, j2, j1, imm6, imm11;
      guint16 insn_low;

      if (!GUM_IS_WITHIN_INT20_RANGE (distance))
        return FALSE;

      insn_low = GUINT16_FROM_LE (r->insn[1]);

      distance_word.i = distance;

      s =  (distance_word.u >> 23) & 1;
      j2 = (distance_word.u >> 18) & 1;
      j1 = (distance_word.u >> 17) & 1;
      imm6 = (distance_word.u >> 11) & GUM_INT6_MASK;
      imm11 = distance_word.u        & GUM_INT11_MASK;

      insn     |=  (s << 10) | imm6;
      insn_low |= (j1 << 13) | (j2 << 11) | imm11;

      r->insn[1] = GUINT16_TO_LE (insn_low);

      break;
    }
    case GUM_THUMB_B_T4:
    case GUM_THUMB_BL_T1:
    {
      union
      {
        gint32 i;
        guint32 u;
      } distance_word;
      guint16 s, i1, i2, j1, j2, imm10, imm11;
      guint16 insn_low;

      if (!GUM_IS_WITHIN_INT24_RANGE (distance))
        return FALSE;

      insn_low = GUINT16_FROM_LE (r->insn[1]);

      distance_word.i = distance;

      s =  (distance_word.u >> 23) & 1;
      i1 = (distance_word.u >> 22) & 1;
      i2 = (distance_word.u >> 21) & 1;
      j1 = (i1 ^ 1) ^ s;
      j2 = (i2 ^ 1) ^ s;

      imm10 = (distance_word.u >> 11) & GUM_INT10_MASK;
      imm11 =  distance_word.u        & GUM_INT11_MASK;

      insn     |=  (s << 10) | imm10;
      insn_low |= (j1 << 13) | (j2 << 11) | imm11;

      r->insn[1] = GUINT16_TO_LE (insn_low);

      break;
    }
    case GUM_THUMB_CBZ_T1:
    case GUM_THUMB_CBNZ_T1:
    {
      guint16 i, imm5;

      if (!GUM_IS_WITHIN_UINT7_RANGE (distance * sizeof (guint16)))
        return FALSE;

      i = (distance >> 5) & 1;
      imm5 = distance & GUM_INT5_MASK;

      insn |= (i << 9) | (imm5 << 3);

      break;
    }
    default:
      g_assert_not_reached ();
  }

  *r->insn = GUINT16_TO_LE (insn);

  return TRUE;
}

static void
gum_thumb_writer_maybe_commit_literals (GumThumbWriter * self)
{
  gsize space_used;
  gconstpointer after_literals = self->code;

  if (self->earliest_literal_insn == NULL)
    return;

  space_used = (self->code - self->earliest_literal_insn) * sizeof (guint16);
  space_used += self->literal_refs.length * sizeof (guint32);
  if (space_used <= 1024)
    return;

  self->earliest_literal_insn = NULL;

  gum_thumb_writer_put_b_label (self, after_literals);
  gum_thumb_writer_commit_literals (self);
  gum_thumb_writer_put_label (self, after_literals);
}

static void
gum_thumb_writer_commit_literals (GumThumbWriter * self)
{
  guint num_refs, ref_index;
  gboolean need_alignment_padding;
  guint32 * first_slot, * last_slot;

  if (!gum_thumb_writer_has_literal_refs (self))
    return;

  num_refs = self->literal_refs.length;
  if (num_refs == 0)
    return;

  need_alignment_padding = (self->pc & 3) != 0;
  if (need_alignment_padding)
  {
    gum_thumb_writer_put_nop (self);
  }

  first_slot = (guint32 *) self->code;
  last_slot = first_slot;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumThumbLiteralRef * r;
    guint16 insn;
    guint32 * cur_slot;
    GumAddress slot_pc;
    gsize distance_in_bytes;

    r = gum_metal_array_element_at (&self->literal_refs, ref_index);
    insn = GUINT16_FROM_LE (r->insn[0]);

    for (cur_slot = first_slot; cur_slot != last_slot; cur_slot++)
    {
      if (*cur_slot == r->val)
        break;
    }

    if (cur_slot == last_slot)
    {
      *cur_slot = r->val;
      self->code += 2;
      self->pc += 4;
      last_slot++;
    }

    slot_pc = self->pc - ((guint8 *) last_slot - (guint8 *) first_slot) +
        ((guint8 *) cur_slot - (guint8 *) first_slot);

    distance_in_bytes = slot_pc - (r->pc & ~((GumAddress) 3));

    if (gum_instruction_is_t1_load (insn))
    {
      r->insn[0] = GUINT16_TO_LE (insn | (distance_in_bytes / 4));
    }
    else
    {
      r->insn[1] = GUINT16_TO_LE (GUINT16_FROM_LE (r->insn[1]) |
          distance_in_bytes);
    }
  }

  gum_metal_array_remove_all (&self->literal_refs);
}

static gboolean
gum_instruction_is_t1_load (guint16 instruction)
{
  return (instruction & 0xf800) == 0x4800;
}
