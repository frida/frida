/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2026 inforcqb <fanjiawei080615@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "arm64writer-fixture.c"

TESTLIST_BEGIN (arm64writer)
  TESTENTRY (cbz_reg_label)
  TESTENTRY (tbnz_reg_imm_imm)

  TESTENTRY (b_imm)
  TESTENTRY (b_label)
  TESTENTRY (bl_imm)
  TESTENTRY (bl_label)
  TESTENTRY (br_reg)
  TESTENTRY (jmp_reg)
  TESTENTRY (jmp_reg_with_ptrauth)
  TESTENTRY (jmp_reg_no_auth)
  TESTENTRY (jmp_reg_no_auth_with_ptrauth)
  TESTENTRY (bti)
  TESTENTRY (blr_reg)
  TESTENTRY (ret)

  TESTENTRY (push_reg_reg)
  TESTENTRY (pop_reg_reg)
  TESTENTRY (ldr_x_address)
  TESTENTRY (ldr_d_address)
#ifdef HAVE_ARM64
  TESTENTRY (ldr_in_large_block)
#endif
  TESTENTRY (ldr_integer_reg_reg_imm)
  TESTENTRY (ldr_integer_reg_reg_imm_mode)
  TESTENTRY (ldr_fp_reg_reg_imm)
  TESTENTRY (ldrsw_reg_reg_imm)
  TESTENTRY (str_integer_reg_reg_imm)
  TESTENTRY (str_integer_reg_reg_imm_mode)
  TESTENTRY (str_fp_reg_reg_imm)
  TESTENTRY (mov_reg_reg)
  TESTENTRY (uxtw_reg_reg)
  TESTENTRY (add_reg_reg_imm)
  TESTENTRY (sub_reg_reg_imm)
  TESTENTRY (sub_reg_reg_reg)
  TESTENTRY (and_reg_reg_imm)
  TESTENTRY (and_reg_reg_neg_imm)
  TESTENTRY (eor_reg_reg_reg)
  TESTENTRY (tst_reg_imm)
  TESTENTRY (cmp_reg_reg)

  TESTENTRY (call_reg)
TESTLIST_END ()

#ifdef HAVE_ARM64
static void gum_emit_ldr_in_large_block (gpointer mem, gpointer user_data);
#endif

TESTCASE (call_reg)
{
  gum_arm64_writer_put_call_reg_with_arguments (&fixture->aw, ARM64_REG_X3,
      2,
      GUM_ARG_REGISTER, ARM64_REG_X5,
      GUM_ARG_REGISTER, ARM64_REG_W7);
  assert_output_n_equals (0, 0xd3407ce1); /* uxtw x1, w7 */
  assert_output_n_equals (1, 0xaa0503e0); /* mov x0, x5 */
  assert_output_n_equals (2, 0xd63f0060); /* blr x3 */
}

TESTCASE (cbz_reg_label)
{
  const gchar * beach_lbl = "beach";

  gum_arm64_writer_put_cbz_reg_label (&fixture->aw, ARM64_REG_W5, beach_lbl);
  gum_arm64_writer_put_cbz_reg_label (&fixture->aw, ARM64_REG_X7, beach_lbl);
  gum_arm64_writer_put_brk_imm (&fixture->aw, 1);
  gum_arm64_writer_put_brk_imm (&fixture->aw, 2);
  gum_arm64_writer_put_brk_imm (&fixture->aw, 3);
  gum_arm64_writer_put_brk_imm (&fixture->aw, 4);
  gum_arm64_writer_put_brk_imm (&fixture->aw, 5);
  gum_arm64_writer_put_brk_imm (&fixture->aw, 6);

  gum_arm64_writer_put_label (&fixture->aw, beach_lbl);
  gum_arm64_writer_put_nop (&fixture->aw);

  gum_arm64_writer_flush (&fixture->aw);

  assert_output_n_equals (0, 0x34000105); /* cbz w5, beach */
  assert_output_n_equals (1, 0xb40000e7); /* cbz x7, beach */
  assert_output_n_equals (2, 0xd4200020); /* brk #1 */
  assert_output_n_equals (3, 0xd4200040); /* brk #2 */
  assert_output_n_equals (4, 0xd4200060); /* brk #3 */
  assert_output_n_equals (5, 0xd4200080); /* brk #4 */
  assert_output_n_equals (6, 0xd42000a0); /* brk #5 */
  assert_output_n_equals (7, 0xd42000c0); /* brk #6 */
  /* beach: */
  assert_output_n_equals (8, 0xd503201f); /* nop */
}

TESTCASE (tbnz_reg_imm_imm)
{
  GumAddress target = GUM_ADDRESS (fixture->aw.pc + 8);

  gum_arm64_writer_put_tbnz_reg_imm_imm (&fixture->aw, ARM64_REG_X17, 0,
      target);
  assert_output_n_equals (0, 0x37000051);

  gum_arm64_writer_put_tbnz_reg_imm_imm (&fixture->aw, ARM64_REG_X17, 33,
      target);
  assert_output_n_equals (1, 0xb7080031);
}

TESTCASE (b_imm)
{
  GumArm64Writer * aw = &fixture->aw;

  GumAddress from = 1024;
  g_assert_true (gum_arm64_writer_can_branch_directly_between (aw, from,
      1024 + 134217727));
  g_assert_false (gum_arm64_writer_can_branch_directly_between (aw, from,
      1024 + 134217728));

  from = 1024 + 134217728;
  g_assert_true (gum_arm64_writer_can_branch_directly_between (aw, from,
      1024));
  g_assert_false (gum_arm64_writer_can_branch_directly_between (aw, from,
      1023));

  aw->pc = 1024;
  gum_arm64_writer_put_b_imm (aw, 2048);
  assert_output_n_equals (0, 0x14000100);
}

TESTCASE (b_label)
{
  const gchar * next_lbl = "next";

  gum_arm64_writer_put_b_label (&fixture->aw, next_lbl);
  gum_arm64_writer_put_nop (&fixture->aw);
  gum_arm64_writer_put_label (&fixture->aw, next_lbl);
  gum_arm64_writer_put_nop (&fixture->aw);

  gum_arm64_writer_flush (&fixture->aw);

  assert_output_n_equals (0, 0x14000002); /* b next */
  assert_output_n_equals (1, 0xd503201f); /* nop */
  /* next: */
  assert_output_n_equals (2, 0xd503201f); /* nop */
}

TESTCASE (bl_imm)
{
  fixture->aw.pc = 1024;
  gum_arm64_writer_put_bl_imm (&fixture->aw, 1028);
  assert_output_n_equals (0, 0x94000001);
}

TESTCASE (bl_label)
{
  const gchar * next_lbl = "next";

  gum_arm64_writer_put_bl_label (&fixture->aw, next_lbl);
  gum_arm64_writer_put_nop (&fixture->aw);
  gum_arm64_writer_put_label (&fixture->aw, next_lbl);
  gum_arm64_writer_put_nop (&fixture->aw);

  gum_arm64_writer_flush (&fixture->aw);

  assert_output_n_equals (0, 0x94000002); /* bl next */
  assert_output_n_equals (1, 0xd503201f); /* nop */
  /* next: */
  assert_output_n_equals (2, 0xd503201f); /* nop */
}

TESTCASE (br_reg)
{
  gum_arm64_writer_put_br_reg (&fixture->aw, ARM64_REG_X3);
  assert_output_n_equals (0, 0xd61f0060);
}

TESTCASE (jmp_reg)
{
  gum_arm64_writer_put_jmp_reg (&fixture->aw, ARM64_REG_X3);
  assert_output_n_equals (0, 0xd65f0060); /* ret x3 */
}

TESTCASE (jmp_reg_with_ptrauth)
{
  fixture->aw.ptrauth_support = GUM_PTRAUTH_SUPPORTED;

  gum_arm64_writer_put_jmp_reg (&fixture->aw, ARM64_REG_X3);
  assert_output_n_equals (0, 0xd61f087f); /* braaz x3 */
}

TESTCASE (jmp_reg_no_auth)
{
  gum_arm64_writer_put_jmp_reg_no_auth (&fixture->aw, ARM64_REG_X3);
  assert_output_n_equals (0, 0xd65f0060); /* ret x3 */
}

TESTCASE (jmp_reg_no_auth_with_ptrauth)
{
  fixture->aw.ptrauth_support = GUM_PTRAUTH_SUPPORTED;

  gum_arm64_writer_put_jmp_reg_no_auth (&fixture->aw, ARM64_REG_X3);
  assert_output_n_equals (0, 0xd61f0060); /* br x3 */
}

TESTCASE (bti)
{
  gum_arm64_writer_put_bti (&fixture->aw);
  assert_output_n_equals (0, 0xd50324df); /* bti jc */
}

TESTCASE (blr_reg)
{
  gum_arm64_writer_put_blr_reg (&fixture->aw, ARM64_REG_X5);
  assert_output_n_equals (0, 0xd63f00a0);
}

TESTCASE (ret)
{
  gum_arm64_writer_put_ret (&fixture->aw);
  assert_output_n_equals (0, 0xd65f03c0);
}

TESTCASE (push_reg_reg)
{
  gum_arm64_writer_put_push_reg_reg (&fixture->aw, ARM64_REG_X3, ARM64_REG_X5);
  assert_output_n_equals (0, 0xa9bf17e3);

  gum_arm64_writer_put_push_reg_reg (&fixture->aw, ARM64_REG_W3, ARM64_REG_W5);
  assert_output_n_equals (1, 0x29bf17e3);

  gum_arm64_writer_put_push_reg_reg (&fixture->aw, ARM64_REG_Q6, ARM64_REG_Q7);
  assert_output_n_equals (2, 0xadbf1fe6);
}

TESTCASE (pop_reg_reg)
{
  gum_arm64_writer_put_pop_reg_reg (&fixture->aw, ARM64_REG_X7, ARM64_REG_X12);
  assert_output_n_equals (0, 0xa8c133e7);

  gum_arm64_writer_put_pop_reg_reg (&fixture->aw, ARM64_REG_W7, ARM64_REG_W12);
  assert_output_n_equals (1, 0x28c133e7);

  gum_arm64_writer_put_pop_reg_reg (&fixture->aw, ARM64_REG_Q6, ARM64_REG_Q7);
  assert_output_n_equals (2, 0xacc11fe6);
}

TESTCASE (ldr_x_address)
{
  gum_arm64_writer_put_ldr_reg_address (&fixture->aw, ARM64_REG_X7,
      0x123456789abcdef0);
  gum_arm64_writer_flush (&fixture->aw);
  assert_output_n_equals (0, 0x58000027);
  g_assert_cmphex (
      *((guint64 *) (((guint8 *) fixture->output) + 4)),
      ==, 0x123456789abcdef0);
}

TESTCASE (ldr_d_address)
{
  gum_arm64_writer_put_ldr_reg_address (&fixture->aw, ARM64_REG_D1,
      0x123456789abcdef0);
  gum_arm64_writer_flush (&fixture->aw);
  assert_output_n_equals (0, 0x5c000021);
  g_assert_cmphex (
      *((guint64 *) (((guint8 *) fixture->output) + 4)),
      ==, 0x123456789abcdef0);
}

#ifdef HAVE_ARM64

TESTCASE (ldr_in_large_block)
{
  const gsize code_size_in_pages = 512;
  gsize page_size, code_size;
  gpointer code;
  gint (* impl) (void);

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
  gum_memory_patch_code (code, code_size, gum_emit_ldr_in_large_block, code);

  impl = gum_sign_code_pointer (code);
  g_assert_cmpint (impl (), ==, 0x1337);

  gum_memory_free (code, code_size);
}

static void
gum_emit_ldr_in_large_block (gpointer mem,
                             gpointer user_data)
{
  gpointer code = user_data;
  GumArm64Writer aw;
  guint i;

  gum_arm64_writer_init (&aw, mem);
  aw.pc = GUM_ADDRESS (code);

  gum_arm64_writer_put_ldr_reg_address (&aw, ARM64_REG_X0, 0x1337);
  for (i = 0; i != 262142; i++)
    gum_arm64_writer_put_nop (&aw);
  gum_arm64_writer_put_ret (&aw);

  gum_arm64_writer_clear (&aw);
}

#endif

TESTCASE (ldr_integer_reg_reg_imm)
{
  gum_arm64_writer_put_ldr_reg_reg_offset (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 16);
  assert_output_n_equals (0, 0xf94008a3);

  gum_arm64_writer_put_ldr_reg_reg_offset (&fixture->aw, ARM64_REG_W3,
      ARM64_REG_X5, 16);
  assert_output_n_equals (1, 0xb94010a3);
}

TESTCASE (ldr_integer_reg_reg_imm_mode)
{
  gum_arm64_writer_put_ldr_reg_reg_offset_mode (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 16, GUM_INDEX_POST_ADJUST);
  assert_output_n_equals (0, 0xf84104a3);

  gum_arm64_writer_put_ldr_reg_reg_offset_mode (&fixture->aw, ARM64_REG_W3,
      ARM64_REG_X5, -16, GUM_INDEX_PRE_ADJUST);
  assert_output_n_equals (1, 0xb85f0ca3);
}

TESTCASE (ldr_fp_reg_reg_imm)
{
  gum_arm64_writer_put_ldr_reg_reg_offset (&fixture->aw, ARM64_REG_S3,
      ARM64_REG_X7, 16);
  assert_output_n_equals (0, 0xbd4010e3);

  gum_arm64_writer_put_ldr_reg_reg_offset (&fixture->aw, ARM64_REG_D3,
      ARM64_REG_X7, 16);
  assert_output_n_equals (1, 0xfd4008e3);

  gum_arm64_writer_put_ldr_reg_reg_offset (&fixture->aw, ARM64_REG_Q3,
      ARM64_REG_X7, 16);
  assert_output_n_equals (2, 0x3dc004e3);
}

TESTCASE (ldrsw_reg_reg_imm)
{
  gum_arm64_writer_put_ldrsw_reg_reg_offset (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 16);
  assert_output_n_equals (0, 0xb98010a3);
}

TESTCASE (str_integer_reg_reg_imm)
{
  gum_arm64_writer_put_str_reg_reg_offset (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 16);
  assert_output_n_equals (0, 0xf90008a3);

  gum_arm64_writer_put_str_reg_reg_offset (&fixture->aw, ARM64_REG_W3,
      ARM64_REG_X5, 16);
  assert_output_n_equals (1, 0xb90010a3);
}

TESTCASE (str_integer_reg_reg_imm_mode)
{
  gum_arm64_writer_put_str_reg_reg_offset_mode (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 16, GUM_INDEX_POST_ADJUST);
  assert_output_n_equals (0, 0xf80104a3);

  gum_arm64_writer_put_str_reg_reg_offset_mode (&fixture->aw, ARM64_REG_W3,
      ARM64_REG_X5, -16, GUM_INDEX_PRE_ADJUST);
  assert_output_n_equals (1, 0xb81f0ca3);
}

TESTCASE (str_fp_reg_reg_imm)
{
  gum_arm64_writer_put_str_reg_reg_offset (&fixture->aw, ARM64_REG_S3,
      ARM64_REG_X7, 16);
  assert_output_n_equals (0, 0xbd0010e3);

  gum_arm64_writer_put_str_reg_reg_offset (&fixture->aw, ARM64_REG_D3,
      ARM64_REG_X7, 16);
  assert_output_n_equals (1, 0xfd0008e3);

  gum_arm64_writer_put_str_reg_reg_offset (&fixture->aw, ARM64_REG_Q3,
      ARM64_REG_X7, 16);
  assert_output_n_equals (2, 0x3d8004e3);
}

TESTCASE (mov_reg_reg)
{
  gum_arm64_writer_put_mov_reg_reg (&fixture->aw, ARM64_REG_X3, ARM64_REG_X5);
  assert_output_n_equals (0, 0xaa0503e3);

  gum_arm64_writer_put_mov_reg_reg (&fixture->aw, ARM64_REG_W3, ARM64_REG_W5);
  assert_output_n_equals (1, 0x2a0503e3);

  gum_arm64_writer_put_mov_reg_reg (&fixture->aw, ARM64_REG_X7, ARM64_REG_SP);
  assert_output_n_equals (2, 0x910003e7);

  gum_arm64_writer_put_mov_reg_reg (&fixture->aw, ARM64_REG_SP, ARM64_REG_X12);
  assert_output_n_equals (3, 0x9100019f);

  gum_arm64_writer_put_mov_reg_reg (&fixture->aw, ARM64_REG_X7, ARM64_REG_XZR);
  assert_output_n_equals (4, 0xaa1f03e7);
}

TESTCASE (uxtw_reg_reg)
{
  gum_arm64_writer_put_uxtw_reg_reg (&fixture->aw, ARM64_REG_X3, ARM64_REG_W5);
  assert_output_n_equals (0, 0xd3407ca3);

  gum_arm64_writer_put_uxtw_reg_reg (&fixture->aw, ARM64_REG_X7, ARM64_REG_W12);
  assert_output_n_equals (1, 0xd3407d87);
}

TESTCASE (add_reg_reg_imm)
{
  gum_arm64_writer_put_add_reg_reg_imm (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 7);
  assert_output_n_equals (0, 0x91001ca3);

  gum_arm64_writer_put_add_reg_reg_imm (&fixture->aw, ARM64_REG_X7,
      ARM64_REG_X12, 16);
  assert_output_n_equals (1, 0x91004187);

  gum_arm64_writer_put_add_reg_reg_imm (&fixture->aw, ARM64_REG_W7,
      ARM64_REG_W12, 16);
  assert_output_n_equals (2, 0x11004187);
}

TESTCASE (sub_reg_reg_imm)
{
  gum_arm64_writer_put_sub_reg_reg_imm (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 7);
  assert_output_n_equals (0, 0xd1001ca3);

  gum_arm64_writer_put_sub_reg_reg_imm (&fixture->aw, ARM64_REG_X7,
      ARM64_REG_X12, 16);
  assert_output_n_equals (1, 0xd1004187);

  gum_arm64_writer_put_sub_reg_reg_imm (&fixture->aw, ARM64_REG_W7,
      ARM64_REG_W12, 16);
  assert_output_n_equals (2, 0x51004187);
}

TESTCASE (sub_reg_reg_reg)
{
  gum_arm64_writer_put_sub_reg_reg_reg (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, ARM64_REG_X7);
  assert_output_n_equals (0, 0xcb0700a3);
}

TESTCASE (and_reg_reg_imm)
{
  gum_arm64_writer_put_and_reg_reg_imm (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, 63);
  assert_output_n_equals (0, 0x924014a3);
}

TESTCASE (and_reg_reg_neg_imm)
{
  gum_arm64_writer_put_and_reg_reg_imm (&fixture->aw, ARM64_REG_X0,
      ARM64_REG_X0, (guint64) -0x10);
  assert_output_n_equals (0, 0x927cec00);
}

TESTCASE (eor_reg_reg_reg)
{
  gum_arm64_writer_put_eor_reg_reg_reg (&fixture->aw, ARM64_REG_X3,
      ARM64_REG_X5, ARM64_REG_X7);
  assert_output_n_equals (0, 0xca0700a3);

  gum_arm64_writer_put_eor_reg_reg_reg (&fixture->aw, ARM64_REG_W3,
      ARM64_REG_W5, ARM64_REG_W7);
  assert_output_n_equals (1, 0x4a0700a3);
}

TESTCASE (tst_reg_imm)
{
  gum_arm64_writer_put_tst_reg_imm (&fixture->aw, ARM64_REG_X3, 16383);
  assert_output_n_equals (0, 0xf240347f);

  gum_arm64_writer_put_tst_reg_imm (&fixture->aw, ARM64_REG_W7, 31);
  assert_output_n_equals (1, 0x720010ff);
}

TESTCASE (cmp_reg_reg)
{
  gum_arm64_writer_put_cmp_reg_reg (&fixture->aw, ARM64_REG_X3, ARM64_REG_X5);
  assert_output_n_equals (0, 0xeb05007f);
}
