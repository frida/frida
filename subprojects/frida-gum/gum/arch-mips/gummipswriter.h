/*
 * Copyright (C) 2014-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2019 Jon Wilson <jonwilson@zepler.net>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MIPS_WRITER_H__
#define __GUM_MIPS_WRITER_H__

#include <capstone.h>
#include <gum/gumdefs.h>
#include <gum/gummetalarray.h>
#include <gum/gummetalhash.h>

#define GUM_MIPS_J_MAX_DISTANCE (1 << 28)

G_BEGIN_DECLS

typedef struct _GumMipsWriter GumMipsWriter;

struct _GumMipsWriter
{
  volatile gint ref_count;
  gboolean flush_on_destroy;

  guint32 * base;
  guint32 * code;
  GumAddress pc;

  GumMetalHashTable * label_defs;
  GumMetalArray label_refs;
};

GUM_API GumMipsWriter * gum_mips_writer_new (gpointer code_address);
GUM_API GumMipsWriter * gum_mips_writer_ref (GumMipsWriter * writer);
GUM_API void gum_mips_writer_unref (GumMipsWriter * writer);

GUM_API void gum_mips_writer_init (GumMipsWriter * writer,
    gpointer code_address);
GUM_API void gum_mips_writer_clear (GumMipsWriter * writer);

GUM_API void gum_mips_writer_reset (GumMipsWriter * writer,
    gpointer code_address);

GUM_API gpointer gum_mips_writer_cur (GumMipsWriter * self);
GUM_API guint gum_mips_writer_offset (GumMipsWriter * self);
GUM_API void gum_mips_writer_skip (GumMipsWriter * self, guint n_bytes);

GUM_API gboolean gum_mips_writer_flush (GumMipsWriter * self);

GUM_API gboolean gum_mips_writer_put_label (GumMipsWriter * self,
    gconstpointer id);

GUM_API void gum_mips_writer_put_call_address_with_arguments (
    GumMipsWriter * self, GumAddress func, guint n_args, ...);
GUM_API void gum_mips_writer_put_call_address_with_arguments_array (
    GumMipsWriter * self, GumAddress func, guint n_args,
    const GumArgument * args);
GUM_API void gum_mips_writer_put_call_reg_with_arguments (GumMipsWriter * self,
    mips_reg reg, guint n_args, ...);
GUM_API void gum_mips_writer_put_call_reg_with_arguments_array (
    GumMipsWriter * self, mips_reg reg, guint n_args, const GumArgument * args);

GUM_API gboolean gum_mips_writer_can_branch_directly_between (GumAddress from,
    GumAddress to);
GUM_API gboolean gum_mips_writer_put_j_address (GumMipsWriter * self,
    GumAddress address);
GUM_API gboolean gum_mips_writer_put_j_address_without_nop (
    GumMipsWriter * self, GumAddress address);
GUM_API void gum_mips_writer_put_j_label (GumMipsWriter * self,
    gconstpointer label_id);
GUM_API void gum_mips_writer_put_jr_reg (GumMipsWriter * self, mips_reg reg);
GUM_API void gum_mips_writer_put_jal_address (GumMipsWriter * self,
    guint32 address);
GUM_API void gum_mips_writer_put_jalr_reg (GumMipsWriter * self, mips_reg reg);
GUM_API void gum_mips_writer_put_b_offset (GumMipsWriter * self, gint32 offset);
GUM_API void gum_mips_writer_put_beq_reg_reg_label (GumMipsWriter * self,
    mips_reg right_reg, mips_reg left_reg, gconstpointer label_id);
GUM_API void gum_mips_writer_put_ret (GumMipsWriter * self);

GUM_API void gum_mips_writer_put_la_reg_address (GumMipsWriter * self,
    mips_reg reg, GumAddress address);
GUM_API void gum_mips_writer_put_lui_reg_imm (GumMipsWriter * self,
    mips_reg reg, guint imm);
GUM_API void gum_mips_writer_put_dsll_reg_reg (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg src_reg, guint amount);
GUM_API void gum_mips_writer_put_ori_reg_reg_imm (GumMipsWriter * self,
    mips_reg rt, mips_reg rs, guint imm);
GUM_API void gum_mips_writer_put_ld_reg_reg_offset (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg src_reg, gsize src_offset);
GUM_API void gum_mips_writer_put_lw_reg_reg_offset (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg src_reg, gsize src_offset);
GUM_API void gum_mips_writer_put_sw_reg_reg_offset (GumMipsWriter * self,
    mips_reg src_reg, mips_reg dst_reg, gsize dst_offset);
GUM_API void gum_mips_writer_put_move_reg_reg (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg src_reg);
GUM_API void gum_mips_writer_put_addu_reg_reg_reg (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg left_reg, mips_reg right_reg);
GUM_API void gum_mips_writer_put_addi_reg_reg_imm (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg left_reg, gint32 imm);
GUM_API void gum_mips_writer_put_addi_reg_imm (GumMipsWriter * self,
    mips_reg dst_reg, gint32 imm);
GUM_API void gum_mips_writer_put_sub_reg_reg_imm (GumMipsWriter * self,
    mips_reg dst_reg, mips_reg left_reg, gint32 imm);

GUM_API void gum_mips_writer_put_push_reg (GumMipsWriter * self, mips_reg reg);
GUM_API void gum_mips_writer_put_pop_reg (GumMipsWriter * self, mips_reg reg);

GUM_API void gum_mips_writer_put_mfhi_reg (GumMipsWriter * self, mips_reg reg);
GUM_API void gum_mips_writer_put_mflo_reg (GumMipsWriter * self, mips_reg reg);
GUM_API void gum_mips_writer_put_mthi_reg (GumMipsWriter * self, mips_reg reg);
GUM_API void gum_mips_writer_put_mtlo_reg (GumMipsWriter * self, mips_reg reg);

GUM_API void gum_mips_writer_put_nop (GumMipsWriter * self);
GUM_API void gum_mips_writer_put_break (GumMipsWriter * self);

GUM_API void gum_mips_writer_put_prologue_trampoline (GumMipsWriter * self,
    mips_reg reg, GumAddress address);

GUM_API void gum_mips_writer_put_instruction (GumMipsWriter * self,
    guint32 insn);
GUM_API gboolean gum_mips_writer_put_bytes (GumMipsWriter * self,
    const guint8 * data, guint n);

G_END_DECLS

#endif
