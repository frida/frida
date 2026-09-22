/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Fabian Freyer <fabian.freyer@physik.tu-berlin.de>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_X86_WRITER_H__
#define __GUM_X86_WRITER_H__

#include <gum/gumdefs.h>
#include <gum/gummetalarray.h>
#include <gum/gummetalhash.h>

#include <capstone.h>

G_BEGIN_DECLS

typedef struct _GumX86Writer GumX86Writer;
typedef guint GumX86Reg;
typedef guint GumX86PtrTarget;

struct _GumX86Writer
{
  volatile gint ref_count;
  gboolean flush_on_destroy;

  GumCpuType target_cpu;
  GumAbiType target_abi;
  GumCpuFeatures cpu_features;

  guint8 * base;
  guint8 * code;
  GumAddress pc;

  GumMetalHashTable * label_defs;
  GumMetalArray label_refs;
};

enum _GumX86Reg
{
  /* 32-bit */
  GUM_X86_EAX = 0,
  GUM_X86_ECX,
  GUM_X86_EDX,
  GUM_X86_EBX,
  GUM_X86_ESP,
  GUM_X86_EBP,
  GUM_X86_ESI,
  GUM_X86_EDI,

  GUM_X86_R8D,
  GUM_X86_R9D,
  GUM_X86_R10D,
  GUM_X86_R11D,
  GUM_X86_R12D,
  GUM_X86_R13D,
  GUM_X86_R14D,
  GUM_X86_R15D,

  GUM_X86_EIP,

  /* 64-bit */
  GUM_X86_RAX,
  GUM_X86_RCX,
  GUM_X86_RDX,
  GUM_X86_RBX,
  GUM_X86_RSP,
  GUM_X86_RBP,
  GUM_X86_RSI,
  GUM_X86_RDI,

  GUM_X86_R8,
  GUM_X86_R9,
  GUM_X86_R10,
  GUM_X86_R11,
  GUM_X86_R12,
  GUM_X86_R13,
  GUM_X86_R14,
  GUM_X86_R15,

  GUM_X86_RIP,

  /* Meta */
  GUM_X86_XAX,
  GUM_X86_XCX,
  GUM_X86_XDX,
  GUM_X86_XBX,
  GUM_X86_XSP,
  GUM_X86_XBP,
  GUM_X86_XSI,
  GUM_X86_XDI,

  GUM_X86_XIP,

  GUM_X86_NONE
};

enum _GumX86PtrTarget
{
  GUM_X86_PTR_BYTE,
  GUM_X86_PTR_DWORD,
  GUM_X86_PTR_QWORD
};

GUM_API GumX86Writer * gum_x86_writer_new (gpointer code_address);
GUM_API GumX86Writer * gum_x86_writer_ref (GumX86Writer * writer);
GUM_API void gum_x86_writer_unref (GumX86Writer * writer);

GUM_API void gum_x86_writer_init (GumX86Writer * writer,
    gpointer code_address);
GUM_API void gum_x86_writer_clear (GumX86Writer * writer);

GUM_API void gum_x86_writer_reset (GumX86Writer * writer,
    gpointer code_address);
GUM_API void gum_x86_writer_set_target_cpu (GumX86Writer * self,
    GumCpuType cpu_type);
GUM_API void gum_x86_writer_set_target_abi (GumX86Writer * self,
    GumAbiType abi_type);

GUM_API gpointer gum_x86_writer_cur (GumX86Writer * self);
GUM_API guint gum_x86_writer_offset (GumX86Writer * self);

GUM_API gboolean gum_x86_writer_flush (GumX86Writer * self);

GUM_API GumX86Reg gum_x86_writer_get_cpu_register_for_nth_argument (
    GumX86Writer * self, guint n);

GUM_API gboolean gum_x86_writer_put_label (GumX86Writer * self,
    gconstpointer id);

GUM_API gboolean gum_x86_writer_can_branch_directly_between (GumAddress from,
    GumAddress to);
GUM_API gboolean gum_x86_writer_put_call_address_with_arguments (
    GumX86Writer * self, GumCallingConvention conv, GumAddress func,
    guint n_args, ...);
GUM_API gboolean gum_x86_writer_put_call_address_with_arguments_array (
    GumX86Writer * self, GumCallingConvention conv, GumAddress func,
    guint n_args, const GumArgument * args);
GUM_API gboolean gum_x86_writer_put_call_address_with_aligned_arguments (
    GumX86Writer * self, GumCallingConvention conv, GumAddress func,
    guint n_args, ...);
GUM_API gboolean gum_x86_writer_put_call_address_with_aligned_arguments_array (
    GumX86Writer * self, GumCallingConvention conv, GumAddress func,
    guint n_args, const GumArgument * args);
GUM_API gboolean gum_x86_writer_put_call_reg_with_arguments (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    guint n_args, ...);
GUM_API gboolean gum_x86_writer_put_call_reg_with_arguments_array (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    guint n_args, const GumArgument * args);
GUM_API gboolean gum_x86_writer_put_call_reg_with_aligned_arguments (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    guint n_args, ...);
GUM_API gboolean gum_x86_writer_put_call_reg_with_aligned_arguments_array (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    guint n_args, const GumArgument * args);
GUM_API gboolean gum_x86_writer_put_call_reg_offset_ptr_with_arguments (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    gssize offset, guint n_args, ...);
GUM_API gboolean gum_x86_writer_put_call_reg_offset_ptr_with_arguments_array (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    gssize offset, guint n_args, const GumArgument * args);
GUM_API gboolean gum_x86_writer_put_call_reg_offset_ptr_with_aligned_arguments (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    gssize offset, guint n_args, ...);
GUM_API gboolean
    gum_x86_writer_put_call_reg_offset_ptr_with_aligned_arguments_array (
    GumX86Writer * self, GumCallingConvention conv, GumX86Reg reg,
    gssize offset, guint n_args, const GumArgument * args);
GUM_API gboolean gum_x86_writer_put_call_address (GumX86Writer * self,
    GumAddress address);
GUM_API gboolean gum_x86_writer_put_call_reg (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_call_reg_offset_ptr (GumX86Writer * self,
    GumX86Reg reg, gssize offset);
GUM_API gboolean gum_x86_writer_put_call_indirect (GumX86Writer * self,
    GumAddress addr);
GUM_API gboolean gum_x86_writer_put_call_indirect_label (GumX86Writer * self,
    gconstpointer label_id);
GUM_API void gum_x86_writer_put_call_near_label (GumX86Writer * self,
    gconstpointer label_id);
GUM_API void gum_x86_writer_put_leave (GumX86Writer * self);
GUM_API void gum_x86_writer_put_ret (GumX86Writer * self);
GUM_API void gum_x86_writer_put_ret_imm (GumX86Writer * self,
    guint16 imm_value);
GUM_API gboolean gum_x86_writer_put_jmp_address (GumX86Writer * self,
    GumAddress address);
GUM_API void gum_x86_writer_put_jmp_short_label (GumX86Writer * self,
    gconstpointer label_id);
GUM_API void gum_x86_writer_put_jmp_near_label (GumX86Writer * self,
    gconstpointer label_id);
GUM_API gboolean gum_x86_writer_put_jmp_reg (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_jmp_reg_ptr (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_jmp_reg_offset_ptr (GumX86Writer * self,
    GumX86Reg reg, gssize offset);
GUM_API gboolean gum_x86_writer_put_jmp_near_ptr (GumX86Writer * self,
    GumAddress address);
GUM_API gboolean gum_x86_writer_put_jcc_short (GumX86Writer * self,
    x86_insn instruction_id, gconstpointer target, GumBranchHint hint);
GUM_API gboolean gum_x86_writer_put_jcc_near (GumX86Writer * self,
    x86_insn instruction_id, gconstpointer target, GumBranchHint hint);
GUM_API void gum_x86_writer_put_jcc_short_label (GumX86Writer * self,
    x86_insn instruction_id, gconstpointer label_id, GumBranchHint hint);
GUM_API void gum_x86_writer_put_jcc_near_label (GumX86Writer * self,
    x86_insn instruction_id, gconstpointer label_id, GumBranchHint hint);

GUM_API gboolean gum_x86_writer_put_add_reg_imm (GumX86Writer * self,
    GumX86Reg reg, gssize imm_value);
GUM_API gboolean gum_x86_writer_put_add_reg_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_add_reg_near_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumAddress src_address);
GUM_API gboolean gum_x86_writer_put_sub_reg_imm (GumX86Writer * self,
    GumX86Reg reg, gssize imm_value);
GUM_API gboolean gum_x86_writer_put_sub_reg_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_sub_reg_near_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumAddress src_address);
GUM_API gboolean gum_x86_writer_put_inc_reg (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_dec_reg (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_inc_reg_ptr (GumX86Writer * self,
    GumX86PtrTarget target, GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_dec_reg_ptr (GumX86Writer * self,
    GumX86PtrTarget target, GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_lock_xadd_reg_ptr_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_lock_cmpxchg_reg_ptr_reg (
    GumX86Writer * self, GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_lock_inc_imm32_ptr (GumX86Writer * self,
    gpointer target);
GUM_API gboolean gum_x86_writer_put_lock_dec_imm32_ptr (GumX86Writer * self,
    gpointer target);

GUM_API gboolean gum_x86_writer_put_and_reg_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_and_reg_u32 (GumX86Writer * self,
    GumX86Reg reg, guint32 imm_value);
GUM_API gboolean gum_x86_writer_put_shl_reg_u8 (GumX86Writer * self,
    GumX86Reg reg, guint8 imm_value);
GUM_API gboolean gum_x86_writer_put_shr_reg_u8 (GumX86Writer * self,
    GumX86Reg reg, guint8 imm_value);
GUM_API gboolean gum_x86_writer_put_xor_reg_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);

GUM_API gboolean gum_x86_writer_put_mov_reg_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_mov_reg_u32 (GumX86Writer * self,
    GumX86Reg dst_reg, guint32 imm_value);
GUM_API gboolean gum_x86_writer_put_mov_reg_u64 (GumX86Writer * self,
    GumX86Reg dst_reg, guint64 imm_value);
GUM_API void gum_x86_writer_put_mov_reg_address (GumX86Writer * self,
    GumX86Reg dst_reg, GumAddress address);
GUM_API void gum_x86_writer_put_mov_reg_ptr_u32 (GumX86Writer * self,
    GumX86Reg dst_reg, guint32 imm_value);
GUM_API gboolean gum_x86_writer_put_mov_reg_offset_ptr_u32 (GumX86Writer * self,
    GumX86Reg dst_reg, gssize dst_offset, guint32 imm_value);
GUM_API void gum_x86_writer_put_mov_reg_ptr_reg (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_mov_reg_offset_ptr_reg (GumX86Writer * self,
    GumX86Reg dst_reg, gssize dst_offset, GumX86Reg src_reg);
GUM_API void gum_x86_writer_put_mov_reg_reg_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_mov_reg_reg_offset_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg, gssize src_offset);
GUM_API gboolean gum_x86_writer_put_mov_reg_base_index_scale_offset_ptr (
    GumX86Writer * self, GumX86Reg dst_reg, GumX86Reg base_reg,
    GumX86Reg index_reg, guint8 scale, gssize offset);

GUM_API gboolean gum_x86_writer_put_mov_reg_near_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumAddress src_address);
GUM_API gboolean gum_x86_writer_put_mov_near_ptr_reg (GumX86Writer * self,
    GumAddress dst_address, GumX86Reg src_reg);

GUM_API gboolean gum_x86_writer_put_mov_fs_u32_ptr_reg (GumX86Writer * self,
    guint32 fs_offset, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_mov_reg_fs_u32_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, guint32 fs_offset);
GUM_API void gum_x86_writer_put_mov_fs_reg_ptr_reg (GumX86Writer * self,
    GumX86Reg fs_offset, GumX86Reg src_reg);
GUM_API void gum_x86_writer_put_mov_reg_fs_reg_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg fs_offset);
GUM_API gboolean gum_x86_writer_put_mov_gs_u32_ptr_reg (GumX86Writer * self,
    guint32 fs_offset, GumX86Reg src_reg);
GUM_API gboolean gum_x86_writer_put_mov_reg_gs_u32_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, guint32 fs_offset);
GUM_API void gum_x86_writer_put_mov_gs_reg_ptr_reg (GumX86Writer * self,
    GumX86Reg gs_offset, GumX86Reg src_reg);
GUM_API void gum_x86_writer_put_mov_reg_gs_reg_ptr (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg gs_offset);

GUM_API void gum_x86_writer_put_movq_xmm0_esp_offset_ptr (GumX86Writer * self,
    gint8 offset);
GUM_API void gum_x86_writer_put_movq_eax_offset_ptr_xmm0 (GumX86Writer * self,
    gint8 offset);
GUM_API void gum_x86_writer_put_movdqu_xmm0_esp_offset_ptr (GumX86Writer * self,
    gint8 offset);
GUM_API void gum_x86_writer_put_movdqu_eax_offset_ptr_xmm0 (GumX86Writer * self,
    gint8 offset);

GUM_API gboolean gum_x86_writer_put_lea_reg_reg_offset (GumX86Writer * self,
    GumX86Reg dst_reg, GumX86Reg src_reg, gssize src_offset);

GUM_API gboolean gum_x86_writer_put_xchg_reg_reg_ptr (GumX86Writer * self,
    GumX86Reg left_reg, GumX86Reg right_reg);

GUM_API void gum_x86_writer_put_push_u32 (GumX86Writer * self,
    guint32 imm_value);
GUM_API gboolean gum_x86_writer_put_push_near_ptr (GumX86Writer * self,
    GumAddress address);
GUM_API gboolean gum_x86_writer_put_push_reg (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_pop_reg (GumX86Writer * self,
    GumX86Reg reg);
GUM_API void gum_x86_writer_put_push_imm_ptr (GumX86Writer * self,
    gconstpointer imm_ptr);
GUM_API void gum_x86_writer_put_pushax (GumX86Writer * self);
GUM_API void gum_x86_writer_put_popax (GumX86Writer * self);
GUM_API void gum_x86_writer_put_pushfx (GumX86Writer * self);
GUM_API void gum_x86_writer_put_popfx (GumX86Writer * self);
GUM_API void gum_x86_writer_put_sahf (GumX86Writer * self);
GUM_API void gum_x86_writer_put_lahf (GumX86Writer * self);

GUM_API gboolean gum_x86_writer_put_test_reg_reg (GumX86Writer * self,
    GumX86Reg reg_a, GumX86Reg reg_b);
GUM_API gboolean gum_x86_writer_put_test_reg_u32 (GumX86Writer * self,
    GumX86Reg reg, guint32 imm_value);
GUM_API gboolean gum_x86_writer_put_cmp_reg_i32 (GumX86Writer * self,
    GumX86Reg reg, gint32 imm_value);
GUM_API gboolean gum_x86_writer_put_cmp_reg_offset_ptr_reg (GumX86Writer * self,
    GumX86Reg reg_a, gssize offset, GumX86Reg reg_b);
GUM_API void gum_x86_writer_put_cmp_imm_ptr_imm_u32 (GumX86Writer * self,
    gconstpointer imm_ptr, guint32 imm_value);
GUM_API gboolean gum_x86_writer_put_cmp_reg_reg (GumX86Writer * self,
    GumX86Reg reg_a, GumX86Reg reg_b);
GUM_API void gum_x86_writer_put_clc (GumX86Writer * self);
GUM_API void gum_x86_writer_put_stc (GumX86Writer * self);
GUM_API void gum_x86_writer_put_cld (GumX86Writer * self);
GUM_API void gum_x86_writer_put_std (GumX86Writer * self);

GUM_API void gum_x86_writer_put_cpuid (GumX86Writer * self);
GUM_API void gum_x86_writer_put_lfence (GumX86Writer * self);
GUM_API void gum_x86_writer_put_rdtsc (GumX86Writer * self);
GUM_API void gum_x86_writer_put_pause (GumX86Writer * self);
GUM_API void gum_x86_writer_put_nop (GumX86Writer * self);
GUM_API void gum_x86_writer_put_endbr (GumX86Writer * self);
GUM_API void gum_x86_writer_put_breakpoint (GumX86Writer * self);
GUM_API void gum_x86_writer_put_padding (GumX86Writer * self, guint n);
GUM_API void gum_x86_writer_put_nop_padding (GumX86Writer * self, guint n);

GUM_API gboolean gum_x86_writer_put_fxsave_reg_ptr (GumX86Writer * self,
    GumX86Reg reg);
GUM_API gboolean gum_x86_writer_put_fxrstor_reg_ptr (GumX86Writer * self,
    GumX86Reg reg);

GUM_API gboolean gum_x86_writer_put_vmovdqu64_reg_offset_ptr_zmm (
    GumX86Writer * self, GumX86Reg dst_base, gssize dst_offset, guint src_zmm);
GUM_API gboolean gum_x86_writer_put_vmovdqu64_zmm_reg_offset_ptr (
    GumX86Writer * self, guint dst_zmm, GumX86Reg src_base, gssize src_offset);
GUM_API gboolean gum_x86_writer_put_vextracti64x4_reg_offset_ptr_zmm (
    GumX86Writer * self, GumX86Reg dst_base, gssize dst_offset, guint src_zmm,
    guint8 imm);
GUM_API gboolean gum_x86_writer_put_vinserti64x4_zmm_reg_offset_ptr (
    GumX86Writer * self, guint dst_zmm, GumX86Reg src_base, gssize src_offset,
    guint8 imm);
GUM_API gboolean gum_x86_writer_put_kmovq_reg_offset_ptr_kreg (
    GumX86Writer * self, GumX86Reg dst_base, gssize dst_offset, guint src_kreg);
GUM_API gboolean gum_x86_writer_put_kmovq_kreg_reg_offset_ptr (
    GumX86Writer * self, guint dst_kreg, GumX86Reg src_base, gssize src_offset);

GUM_API void gum_x86_writer_put_u8 (GumX86Writer * self, guint8 value);
GUM_API void gum_x86_writer_put_s8 (GumX86Writer * self, gint8 value);
GUM_API void gum_x86_writer_put_bytes (GumX86Writer * self, const guint8 * data,
    guint n);

G_END_DECLS

#endif
