/*
 * Copyright (C) 2014-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2017 Antonio Ken Iannillo <ak.iannillo@gmail.com>
 * Copyright (C) 2019 Jon Wilson <jonwilson@zepler.net>
 * Copyright (C) 2023-2026 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2023 Fabian Freyer <fabian.freyer@physik.tu-berlin.de>
 * Copyright (C) 2026 inforcqb <fanjiawei080615@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarm64writer.h"

#include "gumlibc.h"
#include "gummemory.h"
#include "gumprocess.h"

#ifdef _MSC_VER
# include <intrin.h>
#endif

typedef guint GumArm64LabelRefType;
typedef struct _GumArm64LabelRef GumArm64LabelRef;
typedef struct _GumArm64LiteralRef GumArm64LiteralRef;
typedef guint GumArm64LiteralWidth;
typedef guint GumArm64MemOperationType;
typedef guint GumArm64MemOperandType;
typedef guint GumArm64MetaReg;
typedef struct _GumArm64RegInfo GumArm64RegInfo;

enum _GumArm64LabelRefType
{
  GUM_ARM64_B,
  GUM_ARM64_B_COND,
  GUM_ARM64_BL,
  GUM_ARM64_CBZ,
  GUM_ARM64_CBNZ,
  GUM_ARM64_TBZ,
  GUM_ARM64_TBNZ,
};

struct _GumArm64LabelRef
{
  gconstpointer id;
  GumArm64LabelRefType type;
  guint32 * insn;
};

struct _GumArm64LiteralRef
{
  guint32 * insn;
  gint64 val;
  GumArm64LiteralWidth width;
};

enum _GumArm64LiteralWidth
{
  GUM_LITERAL_32BIT,
  GUM_LITERAL_64BIT
};

enum _GumArm64MemOperationType
{
  GUM_MEM_OPERATION_STORE = 0,
  GUM_MEM_OPERATION_LOAD = 1
};

enum _GumArm64MemOperandType
{
  GUM_MEM_OPERAND_I32,
  GUM_MEM_OPERAND_I64,
  GUM_MEM_OPERAND_S32,
  GUM_MEM_OPERAND_D64,
  GUM_MEM_OPERAND_Q128
};

enum _GumArm64MetaReg
{
  GUM_MREG_R0,
  GUM_MREG_R1,
  GUM_MREG_R2,
  GUM_MREG_R3,
  GUM_MREG_R4,
  GUM_MREG_R5,
  GUM_MREG_R6,
  GUM_MREG_R7,
  GUM_MREG_R8,
  GUM_MREG_R9,
  GUM_MREG_R10,
  GUM_MREG_R11,
  GUM_MREG_R12,
  GUM_MREG_R13,
  GUM_MREG_R14,
  GUM_MREG_R15,
  GUM_MREG_R16,
  GUM_MREG_R17,
  GUM_MREG_R18,
  GUM_MREG_R19,
  GUM_MREG_R20,
  GUM_MREG_R21,
  GUM_MREG_R22,
  GUM_MREG_R23,
  GUM_MREG_R24,
  GUM_MREG_R25,
  GUM_MREG_R26,
  GUM_MREG_R27,
  GUM_MREG_R28,
  GUM_MREG_R29,
  GUM_MREG_R30,
  GUM_MREG_R31,

  GUM_MREG_FP = GUM_MREG_R29,
  GUM_MREG_LR = GUM_MREG_R30,
  GUM_MREG_SP = GUM_MREG_R31,
  GUM_MREG_ZR = GUM_MREG_R31
};

struct _GumArm64RegInfo
{
  GumArm64MetaReg meta;
  gboolean is_integer;
  guint width;
  guint index;
  guint32 sf;
  GumArm64MemOperandType operand_type;
};

static void gum_arm64_writer_reset_refs (GumArm64Writer * self);

static void gum_arm64_writer_put_argument_list_setup (GumArm64Writer * self,
    guint n_args, const GumArgument * args);
static void gum_arm64_writer_put_argument_list_setup_va (GumArm64Writer * self,
    guint n_args, va_list args);
static void gum_arm64_writer_put_argument_list_teardown (GumArm64Writer * self,
    guint n_args);
static gboolean gum_arm64_writer_put_br_reg_with_extra (GumArm64Writer * self,
    arm64_reg reg, guint32 extra);
static gboolean gum_arm64_writer_put_blr_reg_with_extra (GumArm64Writer * self,
    arm64_reg reg, guint32 extra);
static gboolean gum_arm64_writer_put_cbx_op_reg_imm (GumArm64Writer * self,
    guint8 op, arm64_reg reg, GumAddress target);
static gboolean gum_arm64_writer_put_tbx_op_reg_imm_imm (GumArm64Writer * self,
    guint8 op, arm64_reg reg, guint bit, GumAddress target);
static gboolean gum_arm64_writer_put_ldr_reg_pcrel (GumArm64Writer * self,
    const GumArm64RegInfo * ri, GumAddress src_address);
static void gum_arm64_writer_put_load_store_pair (GumArm64Writer * self,
    GumArm64MemOperationType operation_type,
    GumArm64MemOperandType operand_type, guint rt, guint rt2, guint rn,
    gssize rn_offset, GumArm64IndexMode mode);

static GumAddress gum_arm64_writer_strip (GumArm64Writer * self,
    GumAddress value);

static gboolean gum_arm64_writer_try_commit_label_refs (GumArm64Writer * self);
static void gum_arm64_writer_maybe_commit_literals (GumArm64Writer * self);
static void gum_arm64_writer_commit_literals (GumArm64Writer * self);

static void gum_arm64_writer_describe_reg (GumArm64Writer * self,
    arm64_reg reg, GumArm64RegInfo * ri);

static GumArm64MemOperandType gum_arm64_mem_operand_type_from_reg_info (
    const GumArm64RegInfo * ri);

static gboolean gum_arm64_try_encode_logical_immediate (guint64 imm_value,
    guint reg_width, guint * imm_enc);
static guint gum_arm64_determine_logical_element_size (guint64 imm_value,
    guint reg_width);
static gboolean gum_arm64_try_determine_logical_rotation (guint64 imm_value,
    guint element_size, guint * num_rotations, guint * num_trailing_ones);

static gboolean gum_is_shifted_mask_64 (guint64 value);
static gboolean gum_is_mask_64 (guint64 value);

static guint gum_count_leading_zeros (guint64 value);
static guint gum_count_trailing_zeros (guint64 value);
static guint gum_count_leading_ones (guint64 value);
static guint gum_count_trailing_ones (guint64 value);

GumArm64Writer *
gum_arm64_writer_new (gpointer code_address)
{
  GumArm64Writer * writer;

  writer = g_slice_new (GumArm64Writer);

  gum_arm64_writer_init (writer, code_address);

  return writer;
}

GumArm64Writer *
gum_arm64_writer_ref (GumArm64Writer * writer)
{
  g_atomic_int_inc (&writer->ref_count);

  return writer;
}

void
gum_arm64_writer_unref (GumArm64Writer * writer)
{
  if (g_atomic_int_dec_and_test (&writer->ref_count))
  {
    gum_arm64_writer_clear (writer);

    g_slice_free (GumArm64Writer, writer);
  }
}

void
gum_arm64_writer_init (GumArm64Writer * writer,
                       gpointer code_address)
{
  writer->ref_count = 1;
  writer->flush_on_destroy = TRUE;

  writer->data_endian = G_BYTE_ORDER;
  writer->target_os = gum_process_get_native_os ();
  writer->ptrauth_support = gum_query_ptrauth_support ();
  writer->sign = gum_sign_code_address;

  writer->label_defs = NULL;
  writer->label_refs.data = NULL;
  writer->literal_refs.data = NULL;

  gum_arm64_writer_reset (writer, code_address);
}

static gboolean
gum_arm64_writer_has_label_defs (GumArm64Writer * self)
{
  return self->label_defs != NULL;
}

static gboolean
gum_arm64_writer_has_label_refs (GumArm64Writer * self)
{
  return self->label_refs.data != NULL;
}

static gboolean
gum_arm64_writer_has_literal_refs (GumArm64Writer * self)
{
  return self->literal_refs.data != NULL;
}

void
gum_arm64_writer_clear (GumArm64Writer * writer)
{
  if (writer->flush_on_destroy)
    gum_arm64_writer_flush (writer);

  if (gum_arm64_writer_has_label_defs (writer))
    gum_metal_hash_table_unref (writer->label_defs);

  if (gum_arm64_writer_has_label_refs (writer))
    gum_metal_array_free (&writer->label_refs);

  if (gum_arm64_writer_has_literal_refs (writer))
    gum_metal_array_free (&writer->literal_refs);
}

void
gum_arm64_writer_reset (GumArm64Writer * writer,
                        gpointer code_address)
{
  writer->base = code_address;
  writer->code = code_address;
  writer->pc = GUM_ADDRESS (code_address);

  if (gum_arm64_writer_has_label_defs (writer))
    gum_metal_hash_table_remove_all (writer->label_defs);

  gum_arm64_writer_reset_refs (writer);
}

static void
gum_arm64_writer_reset_refs (GumArm64Writer * self)
{
  if (gum_arm64_writer_has_label_refs (self))
    gum_metal_array_remove_all (&self->label_refs);

  if (gum_arm64_writer_has_literal_refs (self))
    gum_metal_array_remove_all (&self->literal_refs);

  self->earliest_literal_insn = NULL;
}

gpointer
gum_arm64_writer_cur (GumArm64Writer * self)
{
  return self->code;
}

guint
gum_arm64_writer_offset (GumArm64Writer * self)
{
  return (guint) (self->code - self->base) * sizeof (guint32);
}

void
gum_arm64_writer_skip (GumArm64Writer * self,
                       guint n_bytes)
{
  self->code = (guint32 *) (((guint8 *) self->code) + n_bytes);
  self->pc += n_bytes;
}

gboolean
gum_arm64_writer_flush (GumArm64Writer * self)
{
  if (!gum_arm64_writer_try_commit_label_refs (self))
    goto error;

  gum_arm64_writer_commit_literals (self);

  return TRUE;

error:
  {
    gum_arm64_writer_reset_refs (self);

    return FALSE;
  }
}

gboolean
gum_arm64_writer_put_label (GumArm64Writer * self,
                            gconstpointer id)
{
  if (!gum_arm64_writer_has_label_defs (self))
    self->label_defs = gum_metal_hash_table_new (NULL, NULL);

  if (gum_metal_hash_table_lookup (self->label_defs, id) != NULL)
    return FALSE;

  gum_metal_hash_table_insert (self->label_defs, (gpointer) id, self->code);

  return TRUE;
}

static void
gum_arm64_writer_add_label_reference_here (GumArm64Writer * self,
                                           gconstpointer id,
                                           GumArm64LabelRefType type)
{
  GumArm64LabelRef * r;

  if (!gum_arm64_writer_has_label_refs (self))
    gum_metal_array_init (&self->label_refs, sizeof (GumArm64LabelRef));

  r = gum_metal_array_append (&self->label_refs);
  r->id = id;
  r->type = type;
  r->insn = self->code;
}

static void
gum_arm64_writer_add_literal_reference_here (GumArm64Writer * self,
                                             guint64 val,
                                             GumArm64LiteralWidth width)
{
  GumArm64LiteralRef * r;

  if (!gum_arm64_writer_has_literal_refs (self))
    gum_metal_array_init (&self->literal_refs, sizeof (GumArm64LiteralRef));

  r = gum_metal_array_append (&self->literal_refs);
  r->insn = self->code;
  r->val = val;
  r->width = width;

  if (self->earliest_literal_insn == NULL)
    self->earliest_literal_insn = r->insn;
}

void
gum_arm64_writer_put_call_address_with_arguments (GumArm64Writer * self,
                                                  GumAddress func,
                                                  guint n_args,
                                                  ...)
{
  va_list args;

  va_start (args, n_args);
  gum_arm64_writer_put_argument_list_setup_va (self, n_args, args);
  va_end (args);

  if (gum_arm64_writer_can_branch_directly_between (self, self->pc, func))
  {
    gum_arm64_writer_put_bl_imm (self, func);
  }
  else
  {
    const arm64_reg target = ARM64_REG_X0 + n_args;
    gum_arm64_writer_put_ldr_reg_address (self, target, func);
    gum_arm64_writer_put_blr_reg (self, target);
  }

  gum_arm64_writer_put_argument_list_teardown (self, n_args);
}

void
gum_arm64_writer_put_call_address_with_arguments_array (
    GumArm64Writer * self,
    GumAddress func,
    guint n_args,
    const GumArgument * args)
{
  gum_arm64_writer_put_argument_list_setup (self, n_args, args);

  if (gum_arm64_writer_can_branch_directly_between (self, self->pc, func))
  {
    gum_arm64_writer_put_bl_imm (self, func);
  }
  else
  {
    const arm64_reg target = ARM64_REG_X0 + n_args;
    gum_arm64_writer_put_ldr_reg_address (self, target, func);
    gum_arm64_writer_put_blr_reg (self, target);
  }

  gum_arm64_writer_put_argument_list_teardown (self, n_args);
}

void
gum_arm64_writer_put_call_reg_with_arguments (GumArm64Writer * self,
                                              arm64_reg reg,
                                              guint n_args,
                                              ...)
{
  va_list args;

  va_start (args, n_args);
  gum_arm64_writer_put_argument_list_setup_va (self, n_args, args);
  va_end (args);

  gum_arm64_writer_put_blr_reg (self, reg);

  gum_arm64_writer_put_argument_list_teardown (self, n_args);
}

void
gum_arm64_writer_put_call_reg_with_arguments_array (GumArm64Writer * self,
                                                    arm64_reg reg,
                                                    guint n_args,
                                                    const GumArgument * args)
{
  gum_arm64_writer_put_argument_list_setup (self, n_args, args);

  gum_arm64_writer_put_blr_reg (self, reg);

  gum_arm64_writer_put_argument_list_teardown (self, n_args);
}

static void
gum_arm64_writer_put_argument_list_setup (GumArm64Writer * self,
                                          guint n_args,
                                          const GumArgument * args)
{
  gint arg_index;

  for (arg_index = (gint) n_args - 1; arg_index >= 0; arg_index--)
  {
    const GumArgument * arg = &args[arg_index];
    arm64_reg dst_reg = ARM64_REG_X0 + arg_index;

    if (arg->type == GUM_ARG_ADDRESS)
    {
      gum_arm64_writer_put_ldr_reg_address (self, dst_reg, arg->value.address);
    }
    else
    {
      arm64_reg src_reg = arg->value.reg;
      GumArm64RegInfo rs;

      gum_arm64_writer_describe_reg (self, src_reg, &rs);

      if (rs.width == 64)
      {
        if (src_reg != dst_reg)
          gum_arm64_writer_put_mov_reg_reg (self, dst_reg, arg->value.reg);
      }
      else
      {
        gum_arm64_writer_put_uxtw_reg_reg (self, dst_reg, src_reg);
      }
    }
  }
}

static void
gum_arm64_writer_put_argument_list_setup_va (GumArm64Writer * self,
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
      arg->value.reg = va_arg (args, arm64_reg);
    else
      g_assert_not_reached ();
  }

  gum_arm64_writer_put_argument_list_setup (self, n_args, arg_values);
}

static void
gum_arm64_writer_put_argument_list_teardown (GumArm64Writer * self,
                                             guint n_args)
{
}

void
gum_arm64_writer_put_branch_address (GumArm64Writer * self,
                                     GumAddress address)
{
  if (!gum_arm64_writer_can_branch_directly_between (self, self->pc, address))
  {
    const arm64_reg target = ARM64_REG_X16;

    gum_arm64_writer_put_ldr_reg_address (self, target, address);
    gum_arm64_writer_put_br_reg (self, target);

    return;
  }

  gum_arm64_writer_put_b_imm (self, address);
}

gboolean
gum_arm64_writer_can_branch_directly_between (GumArm64Writer * self,
                                              GumAddress from,
                                              GumAddress to)
{
  gint64 distance = (gint64) gum_arm64_writer_strip (self, to) -
      (gint64) gum_arm64_writer_strip (self, from);

  return GUM_IS_WITHIN_INT28_RANGE (distance);
}

gboolean
gum_arm64_writer_put_b_imm (GumArm64Writer * self,
                            GumAddress address)
{
  gint64 distance =
      (gint64) gum_arm64_writer_strip (self, address) - (gint64) self->pc;

  if (!GUM_IS_WITHIN_INT28_RANGE (distance) || distance % 4 != 0)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      0x14000000 | ((distance / 4) & GUM_INT26_MASK));

  return TRUE;
}

void
gum_arm64_writer_put_b_label (GumArm64Writer * self,
                              gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_B);
  gum_arm64_writer_put_instruction (self, 0x14000000);
}

void
gum_arm64_writer_put_b_cond_label (GumArm64Writer * self,
                                   arm64_cc cc,
                                   gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_B_COND);
  gum_arm64_writer_put_instruction (self, 0x54000000 | (cc - 1));
}

gboolean
gum_arm64_writer_put_bl_imm (GumArm64Writer * self,
                             GumAddress address)
{
  gint64 distance =
      (gint64) gum_arm64_writer_strip (self, address) - (gint64) self->pc;

  if (!GUM_IS_WITHIN_INT28_RANGE (distance) || distance % 4 != 0)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      0x94000000 | ((distance / 4) & GUM_INT26_MASK));

  return TRUE;
}

void
gum_arm64_writer_put_bl_label (GumArm64Writer * self,
                               gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_BL);
  gum_arm64_writer_put_instruction (self, 0x94000000);
}

gboolean
gum_arm64_writer_put_br_reg (GumArm64Writer * self,
                             arm64_reg reg)
{
  return gum_arm64_writer_put_br_reg_with_extra (self, reg,
      (self->ptrauth_support == GUM_PTRAUTH_SUPPORTED) ? 0x81f : 0);
}

gboolean
gum_arm64_writer_put_br_reg_no_auth (GumArm64Writer * self,
                                     arm64_reg reg)
{
  return gum_arm64_writer_put_br_reg_with_extra (self, reg, 0);
}

/*
 * Jump to the address held by the given register, by whichever means gets us
 * there. BR traps on a BTI-guarded target that has no landing pad waiting for
 * us -- the middle of a function we've hooked, say -- whereas RET performs the
 * same jump and is exempt from the check.
 *
 * Where pointer authentication is available we are on arm64e, which doesn't
 * guard pages this way, so we stick with BR.
 */
gboolean
gum_arm64_writer_put_jmp_reg (GumArm64Writer * self,
                              arm64_reg reg)
{
  if (self->ptrauth_support == GUM_PTRAUTH_SUPPORTED)
    return gum_arm64_writer_put_br_reg (self, reg);

  return gum_arm64_writer_put_ret_reg (self, reg);
}

gboolean
gum_arm64_writer_put_jmp_reg_no_auth (GumArm64Writer * self,
                                      arm64_reg reg)
{
  if (self->ptrauth_support == GUM_PTRAUTH_SUPPORTED)
    return gum_arm64_writer_put_br_reg_no_auth (self, reg);

  return gum_arm64_writer_put_ret_reg (self, reg);
}

static gboolean
gum_arm64_writer_put_br_reg_with_extra (GumArm64Writer * self,
                                        arm64_reg reg,
                                        guint32 extra)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0xd61f0000 | (ri.index << 5) | extra);

  return TRUE;
}

gboolean
gum_arm64_writer_put_blr_reg (GumArm64Writer * self,
                              arm64_reg reg)
{
  return gum_arm64_writer_put_blr_reg_with_extra (self, reg,
      (self->ptrauth_support == GUM_PTRAUTH_SUPPORTED) ? 0x81f : 0);
}

gboolean
gum_arm64_writer_put_blr_reg_no_auth (GumArm64Writer * self,
                                      arm64_reg reg)
{
  return gum_arm64_writer_put_blr_reg_with_extra (self, reg, 0);
}

static gboolean
gum_arm64_writer_put_blr_reg_with_extra (GumArm64Writer * self,
                                         arm64_reg reg,
                                         guint32 extra)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0xd63f0000 | (ri.index << 5) | extra);

  return TRUE;
}

void
gum_arm64_writer_put_ret (GumArm64Writer * self)
{
  gum_arm64_writer_put_instruction (self, 0xd65f0000 | (GUM_MREG_LR << 5));
}

gboolean
gum_arm64_writer_put_ret_reg (GumArm64Writer * self,
                              arm64_reg reg)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0xd65f0000 | (ri.index << 5));

  return TRUE;
}

gboolean
gum_arm64_writer_put_cbz_reg_imm (GumArm64Writer * self,
                                  arm64_reg reg,
                                  GumAddress target)
{
  return gum_arm64_writer_put_cbx_op_reg_imm (self, 0, reg, target);
}

gboolean
gum_arm64_writer_put_cbnz_reg_imm (GumArm64Writer * self,
                                   arm64_reg reg,
                                   GumAddress target)
{
  return gum_arm64_writer_put_cbx_op_reg_imm (self, 1, reg, target);
}

void
gum_arm64_writer_put_cbz_reg_label (GumArm64Writer * self,
                                    arm64_reg reg,
                                    gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_CBZ);
  gum_arm64_writer_put_cbx_op_reg_imm (self, 0, reg, 0);
}

void
gum_arm64_writer_put_cbnz_reg_label (GumArm64Writer * self,
                                     arm64_reg reg,
                                     gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_CBNZ);
  gum_arm64_writer_put_cbx_op_reg_imm (self, 1, reg, 0);
}

static gboolean
gum_arm64_writer_put_cbx_op_reg_imm (GumArm64Writer * self,
                                     guint8 op,
                                     arm64_reg reg,
                                     GumAddress target)
{
  GumArm64RegInfo ri;
  gint64 imm19;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (target != 0)
  {
    const gint64 distance = (gint64) target - (gint64) self->pc;
    imm19 = distance / 4;
    if (distance % 4 != 0 || !GUM_IS_WITHIN_INT19_RANGE (imm19))
      return FALSE;
  }
  else
  {
    imm19 = 0;
  }

  gum_arm64_writer_put_instruction (self,
      ri.sf |
      0x34000000 |
      (guint32) op << 24 |
      (imm19 & GUM_INT19_MASK) << 5 |
      ri.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_tbz_reg_imm_imm (GumArm64Writer * self,
                                      arm64_reg reg,
                                      guint bit,
                                      GumAddress target)
{
  return gum_arm64_writer_put_tbx_op_reg_imm_imm (self, 0, reg, bit, target);
}

gboolean
gum_arm64_writer_put_tbnz_reg_imm_imm (GumArm64Writer * self,
                                       arm64_reg reg,
                                       guint bit,
                                       GumAddress target)
{
  return gum_arm64_writer_put_tbx_op_reg_imm_imm (self, 1, reg, bit, target);
}

void
gum_arm64_writer_put_tbz_reg_imm_label (GumArm64Writer * self,
                                        arm64_reg reg,
                                        guint bit,
                                        gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_TBZ);
  gum_arm64_writer_put_tbx_op_reg_imm_imm (self, 0, reg, bit, 0);
}

void
gum_arm64_writer_put_tbnz_reg_imm_label (GumArm64Writer * self,
                                         arm64_reg reg,
                                         guint bit,
                                         gconstpointer label_id)
{
  gum_arm64_writer_add_label_reference_here (self, label_id, GUM_ARM64_TBNZ);
  gum_arm64_writer_put_tbx_op_reg_imm_imm (self, 1, reg, bit, 0);
}

static gboolean
gum_arm64_writer_put_tbx_op_reg_imm_imm (GumArm64Writer * self,
                                         guint8 op,
                                         arm64_reg reg,
                                         guint bit,
                                         GumAddress target)
{
  GumArm64RegInfo ri;
  gint64 imm14;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (bit >= ri.width)
    return FALSE;

  if (target != 0)
  {
    const gint64 distance = (gint64) target - (gint64) self->pc;
    imm14 = distance / 4;
    if (distance % 4 != 0 || !GUM_IS_WITHIN_INT14_RANGE (imm14))
      return FALSE;
  }
  else
  {
    imm14 = 0;
  }

  gum_arm64_writer_put_instruction (self,
      ((bit >> 5) << 31) |
      0x36000000 |
      (guint32) op << 24 |
      ((bit & GUM_INT5_MASK) << 19) |
      (imm14 & GUM_INT14_MASK) << 5 |
      ri.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_push_reg_reg (GumArm64Writer * self,
                                   arm64_reg reg_a,
                                   arm64_reg reg_b)
{
  GumArm64RegInfo ra, rb, sp;

  gum_arm64_writer_describe_reg (self, reg_a, &ra);
  gum_arm64_writer_describe_reg (self, reg_b, &rb);
  gum_arm64_writer_describe_reg (self, ARM64_REG_SP, &sp);

  if (ra.width != rb.width)
    return FALSE;

  gum_arm64_writer_put_load_store_pair (self, GUM_MEM_OPERATION_STORE,
      gum_arm64_mem_operand_type_from_reg_info (&ra), ra.index, rb.index,
      sp.index, -(2 * ((gint) ra.width / 8)), GUM_INDEX_PRE_ADJUST);

  return TRUE;
}

gboolean
gum_arm64_writer_put_pop_reg_reg (GumArm64Writer * self,
                                  arm64_reg reg_a,
                                  arm64_reg reg_b)
{
  GumArm64RegInfo ra, rb, sp;

  gum_arm64_writer_describe_reg (self, reg_a, &ra);
  gum_arm64_writer_describe_reg (self, reg_b, &rb);
  gum_arm64_writer_describe_reg (self, ARM64_REG_SP, &sp);

  if (ra.width != rb.width)
    return FALSE;

  gum_arm64_writer_put_load_store_pair (self, GUM_MEM_OPERATION_LOAD,
      gum_arm64_mem_operand_type_from_reg_info (&ra), ra.index, rb.index,
      sp.index, 2 * (ra.width / 8), GUM_INDEX_POST_ADJUST);

  return TRUE;
}

void
gum_arm64_writer_put_push_all_x_registers (GumArm64Writer * self)
{
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X0, ARM64_REG_X1);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X2, ARM64_REG_X3);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X4, ARM64_REG_X5);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X6, ARM64_REG_X7);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X8, ARM64_REG_X9);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X10, ARM64_REG_X11);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X12, ARM64_REG_X13);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X14, ARM64_REG_X15);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X16, ARM64_REG_X17);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X18, ARM64_REG_X19);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X20, ARM64_REG_X21);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X22, ARM64_REG_X23);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X24, ARM64_REG_X25);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X26, ARM64_REG_X27);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X28, ARM64_REG_X29);
  gum_arm64_writer_put_mov_reg_nzcv (self, ARM64_REG_X15);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_X30, ARM64_REG_X15);
}

void
gum_arm64_writer_put_pop_all_x_registers (GumArm64Writer * self)
{
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X30, ARM64_REG_X15);
  gum_arm64_writer_put_mov_nzcv_reg (self, ARM64_REG_X15);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X28, ARM64_REG_X29);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X26, ARM64_REG_X27);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X24, ARM64_REG_X25);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X22, ARM64_REG_X23);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X20, ARM64_REG_X21);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X18, ARM64_REG_X19);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X16, ARM64_REG_X17);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X14, ARM64_REG_X15);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X12, ARM64_REG_X13);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X10, ARM64_REG_X11);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X8, ARM64_REG_X9);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X6, ARM64_REG_X7);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X4, ARM64_REG_X5);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X2, ARM64_REG_X3);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_X0, ARM64_REG_X1);
}

void
gum_arm64_writer_put_push_all_q_registers (GumArm64Writer * self)
{
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q0, ARM64_REG_Q1);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q2, ARM64_REG_Q3);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q4, ARM64_REG_Q5);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q6, ARM64_REG_Q7);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q8, ARM64_REG_Q9);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q10, ARM64_REG_Q11);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q12, ARM64_REG_Q13);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q14, ARM64_REG_Q15);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q16, ARM64_REG_Q17);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q18, ARM64_REG_Q19);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q20, ARM64_REG_Q21);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q22, ARM64_REG_Q23);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q24, ARM64_REG_Q25);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q26, ARM64_REG_Q27);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q28, ARM64_REG_Q29);
  gum_arm64_writer_put_push_reg_reg (self, ARM64_REG_Q30, ARM64_REG_Q31);
}

void
gum_arm64_writer_put_pop_all_q_registers (GumArm64Writer * self)
{
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q30, ARM64_REG_Q31);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q28, ARM64_REG_Q29);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q26, ARM64_REG_Q27);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q24, ARM64_REG_Q25);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q22, ARM64_REG_Q23);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q20, ARM64_REG_Q21);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q18, ARM64_REG_Q19);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q16, ARM64_REG_Q17);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q14, ARM64_REG_Q15);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q12, ARM64_REG_Q13);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q10, ARM64_REG_Q11);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q8, ARM64_REG_Q9);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q6, ARM64_REG_Q7);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q4, ARM64_REG_Q5);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q2, ARM64_REG_Q3);
  gum_arm64_writer_put_pop_reg_reg (self, ARM64_REG_Q0, ARM64_REG_Q1);
}

gboolean
gum_arm64_writer_put_ldr_reg_address (GumArm64Writer * self,
                                      arm64_reg reg,
                                      GumAddress address)
{
  return gum_arm64_writer_put_ldr_reg_u64 (self, reg, (guint64) address);
}

gboolean
gum_arm64_writer_put_ldr_reg_u32 (GumArm64Writer * self,
                                  arm64_reg reg,
                                  guint32 val)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.is_integer && val == 0)
    return gum_arm64_writer_put_mov_reg_reg (self, reg, ARM64_REG_WZR);

  if (ri.width != 32)
    return FALSE;

  gum_arm64_writer_add_literal_reference_here (self, val, GUM_LITERAL_32BIT);
  gum_arm64_writer_put_ldr_reg_pcrel (self, &ri, 0);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ldr_reg_u64 (GumArm64Writer * self,
                                  arm64_reg reg,
                                  guint64 val)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.is_integer && val == 0)
    return gum_arm64_writer_put_mov_reg_reg (self, reg, ARM64_REG_XZR);

  if (ri.width != 64)
    return FALSE;

  gum_arm64_writer_add_literal_reference_here (self, val, GUM_LITERAL_64BIT);
  gum_arm64_writer_put_ldr_reg_pcrel (self, &ri, 0);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ldr_reg_u32_ptr (GumArm64Writer * self,
                                      arm64_reg reg,
                                      GumAddress src_address)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 32)
    return FALSE;

  return gum_arm64_writer_put_ldr_reg_pcrel (self, &ri, src_address);
}

gboolean
gum_arm64_writer_put_ldr_reg_u64_ptr (GumArm64Writer * self,
                                      arm64_reg reg,
                                      GumAddress src_address)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 64)
    return FALSE;

  return gum_arm64_writer_put_ldr_reg_pcrel (self, &ri, src_address);
}

guint
gum_arm64_writer_put_ldr_reg_ref (GumArm64Writer * self,
                                  arm64_reg reg)
{
  GumArm64RegInfo ri;
  guint ref;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  ref = gum_arm64_writer_offset (self);

  gum_arm64_writer_put_ldr_reg_pcrel (self, &ri, 0);

  return ref;
}

void
gum_arm64_writer_put_ldr_reg_value (GumArm64Writer * self,
                                    guint ref,
                                    GumAddress value)
{
  guint distance;
  guint32 * insn;

  distance = gum_arm64_writer_offset (self) - ref;

  insn = self->base + (ref / 4);
  *insn = GUINT32_TO_LE (GUINT32_FROM_LE (*insn) |
      (((distance / 4) & GUM_INT19_MASK) << 5));

  *((guint64 *) self->code) = GUINT64_TO_LE (value);
  self->code += 2;
  self->pc += 8;
}

static gboolean
gum_arm64_writer_put_ldr_reg_pcrel (GumArm64Writer * self,
                                    const GumArm64RegInfo * ri,
                                    GumAddress src_address)
{
  gint64 imm19;

  if (src_address != 0)
  {
    const gint64 distance = (gint64) src_address - (gint64) self->pc;
    imm19 = distance / 4;
    if (distance % 4 != 0 || !GUM_IS_WITHIN_INT19_RANGE (imm19))
      return FALSE;
  }
  else
  {
    imm19 = 0;
  }

  gum_arm64_writer_put_instruction (self,
      (ri->width == 64 ? 0x50000000 : 0x10000000) |
      (ri->is_integer  ? 0x08000000 : 0x0c000000) |
      (imm19 & GUM_INT19_MASK) << 5 |
      ri->index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ldr_reg_reg (GumArm64Writer * self,
                                  arm64_reg dst_reg,
                                  arm64_reg src_reg)
{
  return gum_arm64_writer_put_ldr_reg_reg_offset (self, dst_reg, src_reg, 0);
}

gboolean
gum_arm64_writer_put_ldr_reg_reg_offset (GumArm64Writer * self,
                                         arm64_reg dst_reg,
                                         arm64_reg src_reg,
                                         gsize src_offset)
{
  GumArm64RegInfo rd, rs;
  guint32 size, v, opc;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, src_reg, &rs);

  opc = 1;
  if (rd.is_integer)
  {
    size = (rd.width == 64) ? 3 : 2;
    v = 0;
  }
  else
  {
    if (rd.width == 128)
    {
      size = 0;
      opc |= 2;
    }
    else
    {
      size = (rd.width == 64) ? 3 : 2;
    }
    v = 1;
  }

  if (rs.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0x39000000 |
      (size << 30) | (v << 26) | (opc << 22) |
      ((guint32) src_offset / (rd.width / 8)) << 10 |
      (rs.index << 5) | rd.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ldr_reg_reg_offset_mode (GumArm64Writer * self,
                                              arm64_reg dst_reg,
                                              arm64_reg src_reg,
                                              gssize src_offset,
                                              GumArm64IndexMode mode)
{
  GumArm64RegInfo rd, rs;
  guint32 opc, size, v;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, src_reg, &rs);

  opc = 1;
  if (rd.is_integer)
  {
    size = (rd.width == 64) ? 3 : 2;
    v = 0;
  }
  else
  {
    if (rd.width == 128)
    {
      size = 0;
      opc |= 2;
    }
    else
    {
      size = (rd.width == 64) ? 3 : 2;
    }
    v = 1;
  }

  if (rs.width != 64)
    return FALSE;

  if (src_offset < -256 || src_offset > 255)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0x38000000 |
      (size << 30) | (v << 26) | (opc << 22) |
      (((guint32) src_offset) & 0x1ff) << 12 |
      mode << 10 |
      (rs.index << 5) | rd.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ldrsw_reg_reg_offset (GumArm64Writer * self,
                                           arm64_reg dst_reg,
                                           arm64_reg src_reg,
                                           gsize src_offset)
{
  GumArm64RegInfo rd, rs;
  gsize immediate;
  gboolean immediate_fits_in_12_bits;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, src_reg, &rs);

  if (rd.width != 64 || rs.width != 64)
    return FALSE;
  if (!rd.is_integer || !rs.is_integer)
    return FALSE;

  immediate = src_offset / sizeof (guint32);

  immediate_fits_in_12_bits = (immediate >> 12) == 0;
  if (!immediate_fits_in_12_bits)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0xb9800000 | (immediate << 10) |
      (rs.index << 5) | rd.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_adrp_reg_address (GumArm64Writer * self,
                                       arm64_reg reg,
                                       GumAddress address)
{
  GumArm64RegInfo ri;
  union
  {
    gint64 i;
    guint64 u;
  } distance;
  guint32 imm_hi, imm_lo;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 64)
    return FALSE;

  distance.i = (gint64) gum_arm64_writer_strip (self, address) -
      (gint64) (self->pc & ~((GumAddress) (4096 - 1)));
  if (distance.i % 4096 != 0)
    return FALSE;
  distance.i /= 4096;

  if (!GUM_IS_WITHIN_INT21_RANGE (distance.i))
    return FALSE;

  imm_hi = (distance.u & G_GUINT64_CONSTANT (0x1ffffc)) >> 2;
  imm_lo = (distance.u & G_GUINT64_CONSTANT (0x3));

  gum_arm64_writer_put_instruction (self, 0x90000000 |
      (imm_lo << 29) | (imm_hi << 5) | ri.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_str_reg_reg (GumArm64Writer * self,
                                  arm64_reg src_reg,
                                  arm64_reg dst_reg)
{
  return gum_arm64_writer_put_str_reg_reg_offset (self, src_reg, dst_reg, 0);
}

gboolean
gum_arm64_writer_put_str_reg_reg_offset (GumArm64Writer * self,
                                         arm64_reg src_reg,
                                         arm64_reg dst_reg,
                                         gsize dst_offset)
{
  GumArm64RegInfo rs, rd;
  guint32 size, v, opc;

  gum_arm64_writer_describe_reg (self, src_reg, &rs);
  gum_arm64_writer_describe_reg (self, dst_reg, &rd);

  opc = 0;
  if (rs.is_integer)
  {
    size = (rs.width == 64) ? 3 : 2;
    v = 0;
  }
  else
  {
    if (rs.width == 128)
    {
      size = 0;
      opc |= 2;
    }
    else
    {
      size = (rs.width == 64) ? 3 : 2;
    }
    v = 1;
  }

  if (rd.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0x39000000 |
      (size << 30) | (v << 26) | (opc << 22) |
      ((guint32) dst_offset / (rs.width / 8)) << 10 |
      (rd.index << 5) | rs.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_str_reg_reg_offset_mode (GumArm64Writer * self,
                                              arm64_reg src_reg,
                                              arm64_reg dst_reg,
                                              gssize dst_offset,
                                              GumArm64IndexMode mode)
{
  GumArm64RegInfo rs, rd;
  guint32 opc, size, v;

  gum_arm64_writer_describe_reg (self, src_reg, &rs);
  gum_arm64_writer_describe_reg (self, dst_reg, &rd);

  opc = 0;
  if (rs.is_integer)
  {
    size = (rs.width == 64) ? 3 : 2;
    v = 0;
  }
  else
  {
    if (rs.width == 128)
    {
      size = 0;
      opc |= 2;
    }
    else
    {
      size = (rs.width == 64) ? 3 : 2;
    }
    v = 1;
  }

  if (rd.width != 64)
    return FALSE;

  if (dst_offset < -256 || dst_offset > 255)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0x38000000 |
      (size << 30) | (v << 26) | (opc << 22) |
      (((guint32) dst_offset) & 0x1ff) << 12 |
      mode << 10 |
      (rd.index << 5) | rs.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ldp_reg_reg_reg_offset (GumArm64Writer * self,
                                             arm64_reg reg_a,
                                             arm64_reg reg_b,
                                             arm64_reg reg_src,
                                             gssize src_offset,
                                             GumArm64IndexMode mode)
{
  GumArm64RegInfo ra, rb, rs;

  gum_arm64_writer_describe_reg (self, reg_a, &ra);
  gum_arm64_writer_describe_reg (self, reg_b, &rb);
  gum_arm64_writer_describe_reg (self, reg_src, &rs);

  if (ra.width != rb.width)
    return FALSE;

  gum_arm64_writer_put_load_store_pair (self, GUM_MEM_OPERATION_LOAD,
      gum_arm64_mem_operand_type_from_reg_info (&ra), ra.index, rb.index,
      rs.index, src_offset, mode);

  return TRUE;
}

gboolean
gum_arm64_writer_put_stp_reg_reg_reg_offset (GumArm64Writer * self,
                                             arm64_reg reg_a,
                                             arm64_reg reg_b,
                                             arm64_reg reg_dst,
                                             gssize dst_offset,
                                             GumArm64IndexMode mode)
{
  GumArm64RegInfo ra, rb, rd;

  gum_arm64_writer_describe_reg (self, reg_a, &ra);
  gum_arm64_writer_describe_reg (self, reg_b, &rb);
  gum_arm64_writer_describe_reg (self, reg_dst, &rd);

  if (ra.width != rb.width)
    return FALSE;

  gum_arm64_writer_put_load_store_pair (self, GUM_MEM_OPERATION_STORE,
      gum_arm64_mem_operand_type_from_reg_info (&ra), ra.index, rb.index,
      rd.index, dst_offset, mode);

  return TRUE;
}

gboolean
gum_arm64_writer_put_mov_reg_reg (GumArm64Writer * self,
                                  arm64_reg dst_reg,
                                  arm64_reg src_reg)
{
  GumArm64RegInfo rd, rs;
  gboolean src_is_zero_reg;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, src_reg, &rs);

  if (rd.width != rs.width)
    return FALSE;

  src_is_zero_reg = src_reg == ARM64_REG_XZR || src_reg == ARM64_REG_WZR;

  if (rd.meta == GUM_MREG_SP || (!src_is_zero_reg && rs.meta == GUM_MREG_SP))
  {
    gum_arm64_writer_put_instruction (self, 0x91000000 | rd.index |
        (rs.index << 5));
  }
  else
  {
    gum_arm64_writer_put_instruction (self, rd.sf | 0x2a000000 | rd.index |
        (GUM_MREG_ZR << 5) | (rs.index << 16));
  }

  return TRUE;
}

gboolean
gum_arm64_writer_put_movk_reg_imm (GumArm64Writer * self,
                                   arm64_reg reg,
                                   guint16 imm,
                                   guint shift)
{
  GumArm64RegInfo ri;
  guint hw;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (shift % 16 != 0 || shift > 48)
    return FALSE;

  hw = shift / 16;
  if (ri.width == 32 && hw > 1)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      ri.sf | 0x72800000 | (hw << 21) | (imm << 5) | ri.index);

  return TRUE;
}

void
gum_arm64_writer_put_mov_reg_nzcv (GumArm64Writer * self,
                                   arm64_reg reg)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  gum_arm64_writer_put_instruction (self, 0xd53b4200 | ri.index);
}

void
gum_arm64_writer_put_mov_nzcv_reg (GumArm64Writer * self,
                                   arm64_reg reg)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  gum_arm64_writer_put_instruction (self, 0xd51b4200 | ri.index);
}

gboolean
gum_arm64_writer_put_uxtw_reg_reg (GumArm64Writer * self,
                                   arm64_reg dst_reg,
                                   arm64_reg src_reg)
{
  GumArm64RegInfo rd, rs;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, src_reg, &rs);

  if (rd.width != 64 || rs.width != 32)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0xd3407c00 | (rs.index << 5) |
      rd.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_add_reg_reg_imm (GumArm64Writer * self,
                                      arm64_reg dst_reg,
                                      arm64_reg left_reg,
                                      gsize right_value)
{
  GumArm64RegInfo rd, rl;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, left_reg, &rl);

  if (rd.width != rl.width)
    return FALSE;

  gum_arm64_writer_put_instruction (self, rd.sf | 0x11000000 | rd.index |
      (rl.index << 5) | (right_value << 10));

  return TRUE;
}

gboolean
gum_arm64_writer_put_add_reg_reg_reg (GumArm64Writer * self,
                                      arm64_reg dst_reg,
                                      arm64_reg left_reg,
                                      arm64_reg right_reg)
{
  GumArm64RegInfo rd, rl, rr;
  guint32 flags = 0;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, left_reg, &rl);
  gum_arm64_writer_describe_reg (self, right_reg, &rr);

  if (rd.width != rl.width || rd.width != rr.width)
    return FALSE;

  if (rd.width == 64)
    flags |= 0x8000000;

  gum_arm64_writer_put_instruction (self, rd.sf | 0xb000000 | flags | rd.index |
      (rl.index << 5) | (rr.index << 16));

  return TRUE;
}

gboolean
gum_arm64_writer_put_sub_reg_reg_imm (GumArm64Writer * self,
                                      arm64_reg dst_reg,
                                      arm64_reg left_reg,
                                      gsize right_value)
{
  GumArm64RegInfo rd, rl;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, left_reg, &rl);

  if (rd.width != rl.width)
    return FALSE;

  gum_arm64_writer_put_instruction (self, rd.sf | 0x51000000 | rd.index |
      (rl.index << 5) | (right_value << 10));

  return TRUE;
}

gboolean
gum_arm64_writer_put_sub_reg_reg_reg (GumArm64Writer * self,
                                      arm64_reg dst_reg,
                                      arm64_reg left_reg,
                                      arm64_reg right_reg)
{
  GumArm64RegInfo rd, rl, rr;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, left_reg, &rl);
  gum_arm64_writer_describe_reg (self, right_reg, &rr);

  if (rd.width != rl.width || rd.width != rr.width)
    return FALSE;

  gum_arm64_writer_put_instruction (self, rd.sf | 0x4b000000 | rd.index |
      (rl.index << 5) | (rr.index << 16));

  return TRUE;
}

gboolean
gum_arm64_writer_put_and_reg_reg_imm (GumArm64Writer * self,
                                      arm64_reg dst_reg,
                                      arm64_reg left_reg,
                                      guint64 right_value)
{
  GumArm64RegInfo rd, rl;
  guint right_value_encoded;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, left_reg, &rl);

  if (rd.width != rl.width)
    return FALSE;

  if (!gum_arm64_try_encode_logical_immediate (right_value, rd.width,
      &right_value_encoded))
    return FALSE;

  gum_arm64_writer_put_instruction (self, rd.sf | 0x12000000 | rd.index |
      (rl.index << 5) | (right_value_encoded << 10));

  return TRUE;
}

gboolean
gum_arm64_writer_put_eor_reg_reg_reg (GumArm64Writer * self,
                                      arm64_reg dst_reg,
                                      arm64_reg left_reg,
                                      arm64_reg right_reg)
{
  GumArm64RegInfo rd, rl, rr;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, left_reg, &rl);
  gum_arm64_writer_describe_reg (self, right_reg, &rr);

  if (rl.width != rd.width || rr.width != rd.width)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      (rd.width == 64 ? 0x80000000 : 0x00000000) |
      0x4a000000 |
      (rr.index << 16) |
      (rl.index << 5) |
      rd.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_ubfm (GumArm64Writer * self,
                           arm64_reg dst_reg,
                           arm64_reg src_reg,
                           guint8 immr,
                           guint8 imms)
{
  GumArm64RegInfo rd, rn;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, src_reg, &rn);

  if (rn.width != rd.width)
    return FALSE;

  if (((imms | immr) & 0xc0) != 0)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      (rd.width == 64 ? 0x80400000 : 0x00000000) |
      0x53000000 |
      (immr << 16) |
      (imms << 10) |
      (rn.index << 5) |
      rd.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_lsl_reg_imm (GumArm64Writer * self,
                                  arm64_reg dst_reg,
                                  arm64_reg src_reg,
                                  guint8 shift)
{
  GumArm64RegInfo rd;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);

  if (rd.width == 32 && (shift & 0xe0) != 0)
    return FALSE;

  if (rd.width == 64 && (shift & 0xc0) != 0)
    return FALSE;

  return gum_arm64_writer_put_ubfm (self, dst_reg, src_reg,
      -shift % rd.width, (rd.width - 1) - shift);
}

gboolean
gum_arm64_writer_put_lsr_reg_imm (GumArm64Writer * self,
                                  arm64_reg dst_reg,
                                  arm64_reg src_reg,
                                  guint8 shift)
{
  GumArm64RegInfo rd;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);

  if (rd.width == 32 && (shift & 0xe0) != 0)
    return FALSE;

  return gum_arm64_writer_put_ubfm (self, dst_reg, src_reg,
      shift, rd.width - 1);
}

gboolean
gum_arm64_writer_put_tst_reg_imm (GumArm64Writer * self,
                                  arm64_reg reg,
                                  guint64 imm_value)
{
  GumArm64RegInfo ri;
  guint imm_value_encoded;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (!gum_arm64_try_encode_logical_immediate (imm_value, ri.width,
      &imm_value_encoded))
    return FALSE;

  gum_arm64_writer_put_instruction (self, ri.sf | 0x7200001f | (ri.index << 5) |
      (imm_value_encoded << 10));

  return TRUE;
}

gboolean
gum_arm64_writer_put_cmp_reg_reg (GumArm64Writer * self,
                                  arm64_reg reg_a,
                                  arm64_reg reg_b)
{
  GumArm64RegInfo ra, rb;

  gum_arm64_writer_describe_reg (self, reg_a, &ra);
  gum_arm64_writer_describe_reg (self, reg_b, &rb);

  if (ra.width != rb.width)
    return FALSE;

  gum_arm64_writer_put_instruction (self, ra.sf | 0x6b00001f | (ra.index << 5) |
      (rb.index << 16));

  return TRUE;
}

gboolean
gum_arm64_writer_put_xpaci_reg (GumArm64Writer * self,
                                arm64_reg reg)
{
  GumArm64RegInfo ri;

  gum_arm64_writer_describe_reg (self, reg, &ri);

  if (ri.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self, 0xdac143e0 | ri.index);

  return TRUE;
}

gboolean
gum_arm64_writer_put_pacia_reg_reg (GumArm64Writer * self,
                                    arm64_reg dst_reg,
                                    arm64_reg mod_reg)
{
  GumArm64RegInfo rd, rm;

  gum_arm64_writer_describe_reg (self, dst_reg, &rd);
  gum_arm64_writer_describe_reg (self, mod_reg, &rm);

  if (rd.width != 64 || rm.width != 64)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      0xdac10000 | rd.index | (rm.index << 5));

  return TRUE;
}

void
gum_arm64_writer_put_nop (GumArm64Writer * self)
{
  gum_arm64_writer_put_instruction (self, 0xd503201f);
}

/*
 * Emit a landing pad for the code we're about to generate, so that indirect
 * branches may reach it on a BTI-guarded page. We accept both jumps and calls,
 * as our entries are reached in both ways.
 */
void
gum_arm64_writer_put_bti (GumArm64Writer * self)
{
  gum_arm64_writer_put_instruction (self, 0xd50324df);
}

void
gum_arm64_writer_put_brk_imm (GumArm64Writer * self,
                              guint16 imm)
{
  gum_arm64_writer_put_instruction (self, 0xd4200000 | (imm << 5));
}

gboolean
gum_arm64_writer_put_mrs (GumArm64Writer * self,
                          arm64_reg dst_reg,
                          guint16 system_reg)
{
  GumArm64RegInfo rt;

  gum_arm64_writer_describe_reg (self, dst_reg, &rt);

  if (rt.width != 64 || (system_reg & 0x8000) != 0)
    return FALSE;

  gum_arm64_writer_put_instruction (self,
      0xd5300000 |
      (system_reg << 5) |
      rt.index);

  return TRUE;
}

static void
gum_arm64_writer_put_load_store_pair (GumArm64Writer * self,
                                      GumArm64MemOperationType operation_type,
                                      GumArm64MemOperandType operand_type,
                                      guint rt,
                                      guint rt2,
                                      guint rn,
                                      gssize rn_offset,
                                      GumArm64IndexMode mode)
{
  guint opc;
  gboolean is_vector;
  gsize shift;

  switch (operand_type)
  {
    case GUM_MEM_OPERAND_I32:
      opc = 0;
      is_vector = FALSE;
      shift = 2;
      break;
    case GUM_MEM_OPERAND_I64:
      opc = 2;
      is_vector = FALSE;
      shift = 3;
      break;
    case GUM_MEM_OPERAND_S32:
      opc = 0;
      is_vector = TRUE;
      shift = 2;
      break;
    case GUM_MEM_OPERAND_D64:
      opc = 1;
      is_vector = TRUE;
      shift = 3;
      break;
    case GUM_MEM_OPERAND_Q128:
      opc = 2;
      is_vector = TRUE;
      shift = 4;
      break;
    default:
      opc = 0;
      is_vector = FALSE;
      shift = 0;
      g_assert_not_reached ();
  }

  gum_arm64_writer_put_instruction (self, (opc << 30) | (5 << 27) |
      (is_vector << 26) | (mode << 23) | (operation_type << 22) |
      (((rn_offset >> shift) & 0x7f) << 15) |
      (rt2 << 10) | (rn << 5) | rt);
}

void
gum_arm64_writer_put_instruction (GumArm64Writer * self,
                                  guint32 insn)
{
  *self->code++ = GUINT32_TO_LE (insn);
  self->pc += 4;

  gum_arm64_writer_maybe_commit_literals (self);
}

gboolean
gum_arm64_writer_put_bytes (GumArm64Writer * self,
                            const guint8 * data,
                            guint n)
{
  if (n % 4 != 0)
    return FALSE;

  gum_memcpy (self->code, data, n);
  self->code += n / sizeof (guint32);
  self->pc += n;

  gum_arm64_writer_maybe_commit_literals (self);

  return TRUE;
}

GumAddress
gum_arm64_writer_sign (GumArm64Writer * self,
                       GumAddress value)
{
  if (self->ptrauth_support != GUM_PTRAUTH_SUPPORTED)
    return value;

  return self->sign (value);
}

static GumAddress
gum_arm64_writer_strip (GumArm64Writer * self,
                        GumAddress value)
{
  if (self->ptrauth_support != GUM_PTRAUTH_SUPPORTED)
    return value;

  switch (self->target_os)
  {
    case GUM_OS_MACOS:
    case GUM_OS_IOS:
    case GUM_OS_XROS:
      return value & G_GUINT64_CONSTANT (0x7fffffffff);
    default:
      break;
  }

  return value;
}

static gboolean
gum_arm64_writer_try_commit_label_refs (GumArm64Writer * self)
{
  guint num_refs, ref_index;

  if (!gum_arm64_writer_has_label_refs (self))
    return TRUE;

  if (!gum_arm64_writer_has_label_defs (self))
    return FALSE;

  num_refs = self->label_refs.length;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumArm64LabelRef * r;
    const guint32 * target_insn;
    gssize distance;
    guint32 insn;

    r = gum_metal_array_element_at (&self->label_refs, ref_index);

    target_insn = gum_metal_hash_table_lookup (self->label_defs, r->id);
    if (target_insn == NULL)
      return FALSE;

    distance = target_insn - r->insn;

    insn = GUINT32_FROM_LE (*r->insn);
    switch (r->type)
    {
      case GUM_ARM64_B:
      case GUM_ARM64_BL:
        if (!GUM_IS_WITHIN_INT26_RANGE (distance))
          return FALSE;
        insn |= distance & GUM_INT26_MASK;
        break;
      case GUM_ARM64_B_COND:
      case GUM_ARM64_CBZ:
      case GUM_ARM64_CBNZ:
        if (!GUM_IS_WITHIN_INT19_RANGE (distance))
          return FALSE;
        insn |= (distance & GUM_INT19_MASK) << 5;
        break;
      case GUM_ARM64_TBZ:
      case GUM_ARM64_TBNZ:
        if (!GUM_IS_WITHIN_INT14_RANGE (distance))
          return FALSE;
        insn |= (distance & GUM_INT14_MASK) << 5;
        break;
      default:
        g_assert_not_reached ();
    }

    *r->insn = GUINT32_TO_LE (insn);
  }

  gum_metal_array_remove_all (&self->label_refs);

  return TRUE;
}

static void
gum_arm64_writer_maybe_commit_literals (GumArm64Writer * self)
{
  gsize space_used;
  gconstpointer after_literals = self->code;

  if (self->earliest_literal_insn == NULL)
    return;

  space_used = (self->code - self->earliest_literal_insn) * sizeof (guint32);
  space_used += self->literal_refs.length * sizeof (guint64);
  if (space_used <= 1048572)
    return;

  self->earliest_literal_insn = NULL;

  gum_arm64_writer_put_b_label (self, after_literals);
  gum_arm64_writer_commit_literals (self);
  gum_arm64_writer_put_label (self, after_literals);
}

static void
gum_arm64_writer_commit_literals (GumArm64Writer * self)
{
  guint num_refs, ref_index;
  gpointer first_slot, last_slot;
  GumArm64DataEndian data_endian;

  if (!gum_arm64_writer_has_literal_refs (self))
    return;

  num_refs = self->literal_refs.length;
  if (num_refs == 0)
    return;

  first_slot = self->code;
  last_slot = first_slot;

  data_endian = self->data_endian;

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumArm64LiteralRef * r;
    gint64 literal, * slot, distance;
    guint32 insn;

    r = gum_metal_array_element_at (&self->literal_refs, ref_index);

    if (r->width != GUM_LITERAL_64BIT)
      continue;

    literal = r->val;

    for (slot = first_slot; slot != last_slot; slot++)
    {
      gint64 candidate = (data_endian == G_LITTLE_ENDIAN)
          ? GINT64_FROM_LE (*slot)
          : GINT64_FROM_BE (*slot);
      if (candidate == literal)
        break;
    }

    if (slot == last_slot)
    {
      *slot = (data_endian == G_LITTLE_ENDIAN)
          ? GINT64_TO_LE (literal)
          : GINT64_TO_BE (literal);
      last_slot = slot + 1;
    }

    distance = (gint64) GPOINTER_TO_SIZE (slot) -
        (gint64) GPOINTER_TO_SIZE (r->insn);

    insn = GUINT32_FROM_LE (*r->insn);
    insn |= ((distance / 4) & GUM_INT19_MASK) << 5;
    *r->insn = GUINT32_TO_LE (insn);
  }

  for (ref_index = 0; ref_index != num_refs; ref_index++)
  {
    GumArm64LiteralRef * r;
    gint32 literal, * slot;
    gint64 distance;
    guint32 insn;

    r = gum_metal_array_element_at (&self->literal_refs, ref_index);

    if (r->width != GUM_LITERAL_32BIT)
      continue;

    literal = r->val;

    for (slot = first_slot; slot != last_slot; slot++)
    {
      gint32 candidate = (data_endian == G_LITTLE_ENDIAN)
          ? GINT32_FROM_LE (*slot)
          : GINT32_FROM_BE (*slot);
      if (candidate == literal)
        break;
    }

    if (slot == last_slot)
    {
      *slot = (data_endian == G_LITTLE_ENDIAN)
          ? GINT32_TO_LE (literal)
          : GINT32_TO_BE (literal);
      last_slot = slot + 1;
    }

    distance = (gint64) GPOINTER_TO_SIZE (slot) -
        (gint64) GPOINTER_TO_SIZE (r->insn);

    insn = GUINT32_FROM_LE (*r->insn);
    insn |= ((distance / 4) & GUM_INT19_MASK) << 5;
    *r->insn = GUINT32_TO_LE (insn);
  }

  self->code = (guint32 *) last_slot;
  self->pc += (guint8 *) last_slot - (guint8 *) first_slot;

  gum_metal_array_remove_all (&self->literal_refs);
}

static void
gum_arm64_writer_describe_reg (GumArm64Writer * self,
                               arm64_reg reg,
                               GumArm64RegInfo * ri)
{
  if (reg >= ARM64_REG_X0 && reg <= ARM64_REG_X28)
  {
    ri->meta = GUM_MREG_R0 + (reg - ARM64_REG_X0);
    ri->is_integer = TRUE;
    ri->width = 64;
    ri->sf = 0x80000000;
  }
  else if (reg == ARM64_REG_X29)
  {
    ri->meta = GUM_MREG_R29;
    ri->is_integer = TRUE;
    ri->width = 64;
    ri->sf = 0x80000000;
  }
  else if (reg == ARM64_REG_X30)
  {
    ri->meta = GUM_MREG_R30;
    ri->is_integer = TRUE;
    ri->width = 64;
    ri->sf = 0x80000000;
  }
  else if (reg == ARM64_REG_SP)
  {
    ri->meta = GUM_MREG_SP;
    ri->is_integer = TRUE;
    ri->width = 64;
    ri->sf = 0x80000000;
  }
  else if (reg >= ARM64_REG_W0 && reg <= ARM64_REG_W30)
  {
    ri->meta = GUM_MREG_R0 + (reg - ARM64_REG_W0);
    ri->is_integer = TRUE;
    ri->width = 32;
    ri->sf = 0x00000000;
  }
  else if (reg >= ARM64_REG_S0 && reg <= ARM64_REG_S31)
  {
    ri->meta = GUM_MREG_R0 + (reg - ARM64_REG_S0);
    ri->is_integer = FALSE;
    ri->width = 32;
    ri->sf = 0x00000000;
  }
  else if (reg >= ARM64_REG_D0 && reg <= ARM64_REG_D31)
  {
    ri->meta = GUM_MREG_R0 + (reg - ARM64_REG_D0);
    ri->is_integer = FALSE;
    ri->width = 64;
    ri->sf = 0x00000000;
  }
  else if (reg >= ARM64_REG_Q0 && reg <= ARM64_REG_Q31)
  {
    ri->meta = GUM_MREG_R0 + (reg - ARM64_REG_Q0);
    ri->is_integer = FALSE;
    ri->width = 128;
    ri->sf = 0x00000000;
  }
  else if (reg == ARM64_REG_XZR)
  {
    ri->meta = GUM_MREG_ZR;
    ri->is_integer = TRUE;
    ri->width = 64;
    ri->sf = 0x80000000;
  }
  else if (reg == ARM64_REG_WZR)
  {
    ri->meta = GUM_MREG_ZR;
    ri->is_integer = TRUE;
    ri->width = 32;
    ri->sf = 0x00000000;
  }
  else
  {
    g_assert_not_reached ();
  }
  ri->index = ri->meta - GUM_MREG_R0;
}

static GumArm64MemOperandType
gum_arm64_mem_operand_type_from_reg_info (const GumArm64RegInfo * ri)
{
  if (ri->is_integer)
  {
    switch (ri->width)
    {
      case 32: return GUM_MEM_OPERAND_I32;
      case 64: return GUM_MEM_OPERAND_I64;
    }
  }
  else
  {
    switch (ri->width)
    {
      case 32: return GUM_MEM_OPERAND_S32;
      case 64: return GUM_MEM_OPERAND_D64;
      case 128: return GUM_MEM_OPERAND_Q128;
    }
  }

  g_assert_not_reached ();
  return GUM_MEM_OPERAND_I32;
}

static gboolean
gum_arm64_try_encode_logical_immediate (guint64 imm_value,
                                        guint reg_width,
                                        guint * imm_enc)
{
  guint element_size, num_rotations, num_trailing_ones;
  guint immr, imms, n;

  if (imm_value == 0 || imm_value == ~G_GUINT64_CONSTANT (0))
    return FALSE;
  if (reg_width == 32)
  {
    if ((imm_value >> 32) != 0 || imm_value == ~0U)
      return FALSE;
  }

  element_size =
      gum_arm64_determine_logical_element_size (imm_value, reg_width);

  if (!gum_arm64_try_determine_logical_rotation (imm_value, element_size,
      &num_rotations, &num_trailing_ones))
    return FALSE;

  immr = (element_size - num_rotations) & (element_size - 1);

  imms = ~(element_size - 1) << 1;
  imms |= num_trailing_ones - 1;

  n = ((imms >> 6) & 1) ^ 1;

  *imm_enc = (n << 12) | (immr << 6) | (imms & 0x3f);

  return TRUE;
}

static guint
gum_arm64_determine_logical_element_size (guint64 imm_value,
                                          guint reg_width)
{
  guint size = reg_width;

  do
  {
    guint next_size;
    guint64 mask;

    next_size = size / 2;

    mask = (G_GUINT64_CONSTANT (1) << next_size) - 1;
    if ((imm_value & mask) != ((imm_value >> next_size) & mask))
      break;

    size = next_size;
  }
  while (size > 2);

  return size;
}

static gboolean
gum_arm64_try_determine_logical_rotation (guint64 imm_value,
                                          guint element_size,
                                          guint * num_rotations,
                                          guint * num_trailing_ones)
{
  guint64 mask;

  mask = ((guint64) G_GINT64_CONSTANT (-1)) >> (64 - element_size);

  imm_value &= mask;

  if (gum_is_shifted_mask_64 (imm_value))
  {
    *num_rotations = gum_count_trailing_zeros (imm_value);
    *num_trailing_ones = gum_count_trailing_ones (imm_value >> *num_rotations);
  }
  else
  {
    guint num_leading_ones;

    imm_value |= ~mask;
    if (!gum_is_shifted_mask_64 (~imm_value))
      return FALSE;

    num_leading_ones = gum_count_leading_ones (imm_value);
    *num_rotations = 64 - num_leading_ones;
    *num_trailing_ones = num_leading_ones +
        gum_count_trailing_ones (imm_value) -
        (64 - element_size);
  }

  return TRUE;
}

static gboolean
gum_is_shifted_mask_64 (guint64 value)
{
  if (value == 0)
    return FALSE;

  return gum_is_mask_64 ((value - 1) | value);
}

static gboolean
gum_is_mask_64 (guint64 value)
{
  if (value == 0)
    return FALSE;

  return ((value + 1) & value) == 0;
}

static guint
gum_count_leading_zeros (guint64 value)
{
  if (value == 0)
    return 64;

#if defined (_MSC_VER) && GLIB_SIZEOF_VOID_P == 4
  {
    unsigned long index;

    if (_BitScanReverse (&index, value >> 32))
      return 31 - index;

    _BitScanReverse (&index, value & 0xffffffff);

    return 63 - index;
  }
#elif defined (_MSC_VER) && GLIB_SIZEOF_VOID_P == 8
  {
    unsigned long index;

    _BitScanReverse64 (&index, value);

    return 63 - index;
  }
#elif defined (HAVE_CLTZ)
  return __builtin_clzll (value);
#else
  guint num_zeros = 0;
  guint64 bits = value;

  while ((bits & (G_GUINT64_CONSTANT (1) << 63)) == 0)
  {
    num_zeros++;
    bits <<= 1;
  }

  return num_zeros;
#endif
}

static guint
gum_count_trailing_zeros (guint64 value)
{
  if (value == 0)
    return 64;

#if defined (_MSC_VER) && GLIB_SIZEOF_VOID_P == 4
  {
    unsigned long index;

    if (_BitScanForward (&index, value & 0xffffffff))
      return index;

    _BitScanForward (&index, value >> 32);

    return 32 + index;
  }
#elif defined (_MSC_VER) && GLIB_SIZEOF_VOID_P == 8
  {
    unsigned long index;

    _BitScanForward64 (&index, value);

    return index;
  }
#elif defined (HAVE_CLTZ)
  return __builtin_ctzll (value);
#else
  guint num_zeros = 0;
  guint64 bits = value;

  while ((bits & G_GUINT64_CONSTANT (1)) == 0)
  {
    num_zeros++;
    bits >>= 1;
  }

  return num_zeros;
#endif
}

static guint
gum_count_leading_ones (guint64 value)
{
  return gum_count_leading_zeros (~value);
}

static guint
gum_count_trailing_ones (guint64 value)
{
  return gum_count_trailing_zeros (~value);
}
