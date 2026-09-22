/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_THUMB_WRITER_H__
#define __GUM_THUMB_WRITER_H__

#include <capstone.h>
#include <gum/gumdefs.h>
#include <gum/gummetalarray.h>
#include <gum/gummetalhash.h>

#define GUM_THUMB_B_MAX_DISTANCE 0x00fffffe

G_BEGIN_DECLS

typedef struct _GumThumbWriter GumThumbWriter;

struct _GumThumbWriter
{
  volatile gint ref_count;
  gboolean flush_on_destroy;

  GumOS target_os;

  guint16 * base;
  guint16 * code;
  GumAddress pc;

  GumMetalHashTable * label_defs;
  GumMetalArray label_refs;
  GumMetalArray literal_refs;
  const guint16 * earliest_literal_insn;
};

GUM_API GumThumbWriter * gum_thumb_writer_new (gpointer code_address);
GUM_API GumThumbWriter * gum_thumb_writer_ref (GumThumbWriter * writer);
GUM_API void gum_thumb_writer_unref (GumThumbWriter * writer);

GUM_API void gum_thumb_writer_init (GumThumbWriter * writer,
    gpointer code_address);
GUM_API void gum_thumb_writer_clear (GumThumbWriter * writer);

GUM_API void gum_thumb_writer_reset (GumThumbWriter * writer,
    gpointer code_address);
GUM_API void gum_thumb_writer_set_target_os (GumThumbWriter * self, GumOS os);

GUM_API gpointer gum_thumb_writer_cur (GumThumbWriter * self);
GUM_API guint gum_thumb_writer_offset (GumThumbWriter * self);
GUM_API void gum_thumb_writer_skip (GumThumbWriter * self, guint n_bytes);

GUM_API gboolean gum_thumb_writer_flush (GumThumbWriter * self);

GUM_API gboolean gum_thumb_writer_put_label (GumThumbWriter * self,
    gconstpointer id);
GUM_API gboolean gum_thumb_writer_commit_label (GumThumbWriter * self,
    gconstpointer id);

GUM_API void gum_thumb_writer_put_call_address_with_arguments (
    GumThumbWriter * self, GumAddress func, guint n_args, ...);
GUM_API void gum_thumb_writer_put_call_address_with_arguments_array (
    GumThumbWriter * self, GumAddress func, guint n_args,
    const GumArgument * args);
GUM_API void gum_thumb_writer_put_call_reg_with_arguments (
    GumThumbWriter * self, arm_reg reg, guint n_args, ...);
GUM_API void gum_thumb_writer_put_call_reg_with_arguments_array (
    GumThumbWriter * self, arm_reg reg, guint n_args, const GumArgument * args);

GUM_API void gum_thumb_writer_put_branch_address (GumThumbWriter * self,
    GumAddress address);

GUM_API gboolean gum_thumb_writer_can_branch_directly_between (
    GumThumbWriter * self, GumAddress from, GumAddress to);
GUM_API void gum_thumb_writer_put_b_imm (GumThumbWriter * self,
    GumAddress target);
GUM_API void gum_thumb_writer_put_b_label (GumThumbWriter * self,
    gconstpointer label_id);
GUM_API void gum_thumb_writer_put_b_label_wide (GumThumbWriter * self,
    gconstpointer label_id);
GUM_API void gum_thumb_writer_put_bx_reg (GumThumbWriter * self, arm_reg reg);
GUM_API void gum_thumb_writer_put_bl_imm (GumThumbWriter * self,
    GumAddress target);
GUM_API void gum_thumb_writer_put_bl_label (GumThumbWriter * self,
    gconstpointer label_id);
GUM_API void gum_thumb_writer_put_blx_imm (GumThumbWriter * self,
    GumAddress target);
GUM_API void gum_thumb_writer_put_blx_reg (GumThumbWriter * self, arm_reg reg);
GUM_API void gum_thumb_writer_put_cmp_reg_imm (GumThumbWriter * self,
    arm_reg reg, guint8 imm_value);
GUM_API void gum_thumb_writer_put_beq_label (GumThumbWriter * self,
    gconstpointer label_id);
GUM_API void gum_thumb_writer_put_bne_label (GumThumbWriter * self,
    gconstpointer label_id);
GUM_API void gum_thumb_writer_put_b_cond_label (GumThumbWriter * self,
    arm_cc cc, gconstpointer label_id);
GUM_API void gum_thumb_writer_put_b_cond_label_wide (GumThumbWriter * self,
    arm_cc cc, gconstpointer label_id);
GUM_API void gum_thumb_writer_put_cbz_reg_label (GumThumbWriter * self,
    arm_reg reg, gconstpointer label_id);
GUM_API void gum_thumb_writer_put_cbnz_reg_label (GumThumbWriter * self,
    arm_reg reg, gconstpointer label_id);

GUM_API gboolean gum_thumb_writer_put_push_regs (GumThumbWriter * self,
    guint n_regs, arm_reg first_reg, ...);
GUM_API gboolean gum_thumb_writer_put_push_regs_array (GumThumbWriter * self,
    guint n_regs, const arm_reg * regs);
GUM_API gboolean gum_thumb_writer_put_pop_regs (GumThumbWriter * self,
    guint n_regs, arm_reg first_reg, ...);
GUM_API gboolean gum_thumb_writer_put_pop_regs_array (GumThumbWriter * self,
    guint n_regs, const arm_reg * regs);
GUM_API gboolean gum_thumb_writer_put_vpush_range (GumThumbWriter * self,
    arm_reg first_reg, arm_reg last_reg);
GUM_API gboolean gum_thumb_writer_put_vpop_range (GumThumbWriter * self,
    arm_reg first_reg, arm_reg last_reg);
GUM_API gboolean gum_thumb_writer_put_ldr_reg_address (GumThumbWriter * self,
    arm_reg reg, GumAddress address);
GUM_API gboolean gum_thumb_writer_put_ldr_reg_u32 (GumThumbWriter * self,
    arm_reg reg, guint32 val);
GUM_API void gum_thumb_writer_put_ldr_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg src_reg);
GUM_API gboolean gum_thumb_writer_put_ldr_reg_reg_offset (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg src_reg, gsize src_offset);
GUM_API void gum_thumb_writer_put_ldrb_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg src_reg);
void gum_thumb_writer_put_ldrh_reg_reg (GumThumbWriter * self, arm_reg dst_reg,
    arm_reg src_reg);
GUM_API gboolean gum_thumb_writer_put_vldr_reg_reg_offset (
    GumThumbWriter * self, arm_reg dst_reg, arm_reg src_reg, gssize src_offset);
GUM_API void gum_thumb_writer_put_ldmia_reg_mask (GumThumbWriter * self,
    arm_reg reg, guint16 mask);
GUM_API void gum_thumb_writer_put_str_reg_reg (GumThumbWriter * self,
    arm_reg src_reg, arm_reg dst_reg);
GUM_API gboolean gum_thumb_writer_put_str_reg_reg_offset (GumThumbWriter * self,
    arm_reg src_reg, arm_reg dst_reg, gsize dst_offset);
GUM_API void gum_thumb_writer_put_mov_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg src_reg);
GUM_API void gum_thumb_writer_put_mov_reg_u8 (GumThumbWriter * self,
    arm_reg dst_reg, guint8 imm_value);
GUM_API void gum_thumb_writer_put_mov_reg_cpsr (GumThumbWriter * self,
    arm_reg reg);
GUM_API void gum_thumb_writer_put_mov_cpsr_reg (GumThumbWriter * self,
    arm_reg reg);
GUM_API gboolean gum_thumb_writer_put_add_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, gssize imm_value);
GUM_API void gum_thumb_writer_put_add_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg src_reg);
GUM_API void gum_thumb_writer_put_add_reg_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, arm_reg right_reg);
GUM_API gboolean gum_thumb_writer_put_add_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, gssize right_value);
GUM_API gboolean gum_thumb_writer_put_sub_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, gssize imm_value);
GUM_API void gum_thumb_writer_put_sub_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg src_reg);
GUM_API void gum_thumb_writer_put_sub_reg_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, arm_reg right_reg);
GUM_API gboolean gum_thumb_writer_put_sub_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, gssize right_value);
GUM_API gboolean gum_thumb_writer_put_and_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, gssize right_value);
GUM_API gboolean gum_thumb_writer_put_or_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, gssize right_value);
GUM_API gboolean gum_thumb_writer_put_lsl_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, guint8 right_value);
GUM_API gboolean gum_thumb_writer_put_lsls_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, guint8 right_value);
GUM_API gboolean gum_thumb_writer_put_lsrs_reg_reg_imm (GumThumbWriter * self,
    arm_reg dst_reg, arm_reg left_reg, guint8 right_value);
GUM_API gboolean gum_thumb_writer_put_mrs_reg_reg (GumThumbWriter * self,
    arm_reg dst_reg, arm_sysreg src_reg);
GUM_API gboolean gum_thumb_writer_put_msr_reg_reg (GumThumbWriter * self,
    arm_sysreg dst_reg, arm_reg src_reg);

GUM_API void gum_thumb_writer_put_nop (GumThumbWriter * self);
GUM_API void gum_thumb_writer_put_bkpt_imm (GumThumbWriter * self, guint8 imm);
GUM_API void gum_thumb_writer_put_breakpoint (GumThumbWriter * self);

GUM_API void gum_thumb_writer_put_instruction (GumThumbWriter * self,
    guint16 insn);
GUM_API void gum_thumb_writer_put_instruction_wide (GumThumbWriter * self,
    guint16 upper, guint16 lower);
GUM_API gboolean gum_thumb_writer_put_bytes (GumThumbWriter * self,
    const guint8 * data, guint n);

G_END_DECLS

#endif
