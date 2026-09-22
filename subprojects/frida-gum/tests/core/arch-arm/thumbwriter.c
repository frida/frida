/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "thumbwriter-fixture.c"

TESTLIST_BEGIN (thumbwriter)
  TESTENTRY (cmp_reg_imm)
  TESTENTRY (beq_label)
  TESTENTRY (bne_label)
  TESTENTRY (b_cond_label_wide)
  TESTENTRY (cbz_reg_label)
  TESTENTRY (cbz_reg_label_too_short)
  TESTENTRY (cbz_reg_label_minimum)
  TESTENTRY (cbz_reg_label_maximum)
  TESTENTRY (cbz_reg_label_too_long)
  TESTENTRY (cbnz_reg_label)

  TESTENTRY (b_label_wide)
  TESTENTRY (bx_reg)
  TESTENTRY (bl_label)
  TESTENTRY (blx_reg)

  TESTENTRY (push_regs)
  TESTENTRY (pop_regs)
  TESTENTRY (vpush_range)
  TESTENTRY (vpop_range)
  TESTENTRY (ldr_u32)
#ifdef HAVE_ARM
  TESTENTRY (ldr_in_large_block)
#endif
  TESTENTRY (ldr_reg_reg_offset)
  TESTENTRY (ldr_reg_reg)
  TESTENTRY (ldrb_reg_reg)
  TESTENTRY (ldrh_reg_reg)
  TESTENTRY (vldr_reg_reg_offset)
  TESTENTRY (str_reg_reg_offset)
  TESTENTRY (str_reg_reg)
  TESTENTRY (mov_reg_reg)
  TESTENTRY (mov_reg_u8)
  TESTENTRY (add_reg_imm)
  TESTENTRY (add_reg_reg_reg)
  TESTENTRY (add_reg_reg)
  TESTENTRY (add_reg_reg_imm)
  TESTENTRY (sub_reg_imm)
  TESTENTRY (sub_reg_reg_reg)
  TESTENTRY (sub_reg_reg_imm)
  TESTENTRY (and_reg_reg_imm)
  TESTENTRY (lsls_reg_reg_imm)
  TESTENTRY (lsrs_reg_reg_imm)

  TESTENTRY (mrs_reg_reg)
  TESTENTRY (msr_reg_reg)

  TESTENTRY (nop)
TESTLIST_END ()

#ifdef HAVE_ARM
static void gum_emit_ldr_in_large_block (gpointer mem, gpointer user_data);
#endif

TESTCASE (cmp_reg_imm)
{
  gum_thumb_writer_put_cmp_reg_imm (&fixture->tw, ARM_REG_R7, 7);
  assert_output_n_equals (0, 0x2f07); /* cmp r7, 7 */
}

TESTCASE (beq_label)
{
  const gchar * again_lbl = "again";

  gum_thumb_writer_put_label (&fixture->tw, again_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_beq_label (&fixture->tw, again_lbl);

  gum_thumb_writer_flush (&fixture->tw);

  /* again: */
  assert_output_n_equals (0, 0xbf00); /* nop */
  assert_output_n_equals (1, 0xbf00); /* nop */
  assert_output_n_equals (2, 0xbf00); /* nop */
  assert_output_n_equals (3, 0xd0fb); /* beq again */
}

TESTCASE (bne_label)
{
  const gchar * again_lbl = "again";

  gum_thumb_writer_put_label (&fixture->tw, again_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_bne_label (&fixture->tw, again_lbl);

  gum_thumb_writer_flush (&fixture->tw);

  /* again: */
  assert_output_n_equals (0, 0xbf00); /* nop */
  assert_output_n_equals (1, 0xbf00); /* nop */
  assert_output_n_equals (2, 0xbf00); /* nop */
  assert_output_n_equals (3, 0xd1fb); /* bne again */
}

TESTCASE (b_cond_label_wide)
{
  const gchar * again_lbl = "again";

  gum_thumb_writer_put_label (&fixture->tw, again_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_b_cond_label_wide (&fixture->tw, ARM_CC_NE, again_lbl);

  gum_thumb_writer_flush (&fixture->tw);

  /* again: */
  assert_output_n_equals (0, 0xbf00); /* nop */
  assert_output_n_equals (1, 0xbf00); /* nop */
  assert_output_n_equals (2, 0xbf00); /* nop */
  assert_output_n_equals (3, 0xf47f); /* bne.w again */
  assert_output_n_equals (4, 0xaffb);
}

TESTCASE (cbz_reg_label)
{
  const gchar * beach_lbl = "beach";

  gum_thumb_writer_put_cbz_reg_label (&fixture->tw, ARM_REG_R7, beach_lbl);
  gum_thumb_writer_put_blx_reg (&fixture->tw, ARM_REG_R1);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);

  gum_thumb_writer_put_label (&fixture->tw, beach_lbl);
  gum_thumb_writer_put_pop_regs (&fixture->tw, 1, ARM_REG_PC);

  gum_thumb_writer_flush (&fixture->tw);

  assert_output_n_equals (0, 0xb11f); /* cbz r7, beach */
  assert_output_n_equals (1, 0x4788); /* blx r1 */
  assert_output_n_equals (2, 0xbf00); /* nop */
  assert_output_n_equals (3, 0xbf00); /* nop */
  assert_output_n_equals (4, 0xbf00); /* nop */
  /* beach: */
  assert_output_n_equals (5, 0xbd00); /* pop {pc} */
}

TESTCASE (cbz_reg_label_too_short)
{
  const gchar * beach_lbl = "beach";

  gum_thumb_writer_put_cbz_reg_label (&fixture->tw, ARM_REG_R7, beach_lbl);
  gum_thumb_writer_put_label (&fixture->tw, beach_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);

  g_assert_false (gum_thumb_writer_flush (&fixture->tw));
}

TESTCASE (cbz_reg_label_minimum)
{
  const gchar * beach_lbl = "beach";

  gum_thumb_writer_put_cbz_reg_label (&fixture->tw, ARM_REG_R7, beach_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_label (&fixture->tw, beach_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);

  g_assert_true (gum_thumb_writer_flush (&fixture->tw));
  assert_output_n_equals (0, 0xb107); /* cbz r7, beach */
}

TESTCASE (cbz_reg_label_maximum)
{
  const gchar * beach_lbl = "beach";
  guint i;

  gum_thumb_writer_put_cbz_reg_label (&fixture->tw, ARM_REG_R7, beach_lbl);
  for (i = 0; i != 64; i++)
    gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_label (&fixture->tw, beach_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);

  g_assert_true (gum_thumb_writer_flush (&fixture->tw));
  assert_output_n_equals (0, 0xb3ff); /* cbz r7, beach */
}

TESTCASE (cbz_reg_label_too_long)
{
  const gchar * beach_lbl = "beach";
  guint i;

  gum_thumb_writer_put_cbz_reg_label (&fixture->tw, ARM_REG_R7, beach_lbl);
  for (i = 0; i != 64; i++)
    gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_label (&fixture->tw, beach_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);

  g_assert_false (gum_thumb_writer_flush (&fixture->tw));
}

TESTCASE (cbnz_reg_label)
{
  const gchar * beach_lbl = "beach";

  gum_thumb_writer_put_cbnz_reg_label (&fixture->tw, ARM_REG_R0, beach_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_nop (&fixture->tw);
  gum_thumb_writer_put_label (&fixture->tw, beach_lbl);

  gum_thumb_writer_flush (&fixture->tw);

  assert_output_n_equals (0, 0xb910); /* cbnz r0, beach */
  assert_output_n_equals (1, 0xbf00); /* nop */
  assert_output_n_equals (2, 0xbf00); /* nop */
  assert_output_n_equals (3, 0xbf00); /* nop */
  /* beach: */
}

TESTCASE (b_label_wide)
{
  const gchar * next_lbl = "next";

  gum_thumb_writer_put_b_label_wide (&fixture->tw, next_lbl);
  gum_thumb_writer_put_label (&fixture->tw, next_lbl);
  gum_thumb_writer_put_nop (&fixture->tw);

  gum_thumb_writer_flush (&fixture->tw);

  assert_output_n_equals (0, 0xf000); /* b.w next */
  assert_output_n_equals (1, 0xb800);
  /* next: */
  assert_output_n_equals (2, 0xbf00); /* nop */
}

TESTCASE (bx_reg)
{
  gum_thumb_writer_put_bx_reg (&fixture->tw, ARM_REG_R0);
  assert_output_n_equals (0, 0x4700);

  gum_thumb_writer_put_bx_reg (&fixture->tw, ARM_REG_R7);
  assert_output_n_equals (1, 0x4738);
}

TESTCASE (blx_reg)
{
  gum_thumb_writer_put_blx_reg (&fixture->tw, ARM_REG_R0);
  assert_output_n_equals (0, 0x4780);

  gum_thumb_writer_put_blx_reg (&fixture->tw, ARM_REG_R3);
  assert_output_n_equals (1, 0x4798);
}

TESTCASE (bl_label)
{
  const gchar * next_lbl = "next";

  gum_thumb_writer_put_push_regs (&fixture->tw, 1, ARM_REG_LR);
  gum_thumb_writer_put_bl_label (&fixture->tw, next_lbl);
  gum_thumb_writer_put_pop_regs (&fixture->tw, 1, ARM_REG_PC);
  gum_thumb_writer_put_label (&fixture->tw, next_lbl);
  gum_thumb_writer_put_mov_reg_u8 (&fixture->tw, ARM_REG_R2, 0);

  gum_thumb_writer_flush (&fixture->tw);

  assert_output_n_equals (0, 0xb500); /* push {lr} */
  assert_output_n_equals (1, 0xf000); /* bl next */
  assert_output_n_equals (2, 0xf801);
  assert_output_n_equals (3, 0xbd00); /* pop {pc} */
  /* next: */
  assert_output_n_equals (4, 0xbfe8); /* it al */
  assert_output_n_equals (5, 0x2200); /* movs r2, 0 */
}

TESTCASE (push_regs)
{
  gum_thumb_writer_put_push_regs (&fixture->tw, 1, ARM_REG_R0);
  assert_output_n_equals (0, 0xb401);

  gum_thumb_writer_put_push_regs (&fixture->tw, 1, ARM_REG_R7);
  assert_output_n_equals (1, 0xb480);

  gum_thumb_writer_put_push_regs (&fixture->tw, 9, ARM_REG_R0, ARM_REG_R1,
      ARM_REG_R2, ARM_REG_R3, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_LR);
  assert_output_n_equals (2, 0xb5ff);

  gum_thumb_writer_put_push_regs (&fixture->tw, 2, ARM_REG_R8, ARM_REG_R9);
  assert_output_n_equals (3, 0xe92d);
  assert_output_n_equals (4, 0x0300);
}

TESTCASE (pop_regs)
{
  gum_thumb_writer_put_pop_regs (&fixture->tw, 1, ARM_REG_R0);
  assert_output_n_equals (0, 0xbc01);

  gum_thumb_writer_put_pop_regs (&fixture->tw, 9, ARM_REG_R0, ARM_REG_R1,
      ARM_REG_R2, ARM_REG_R3, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_PC);
  assert_output_n_equals (1, 0xbdff);

  gum_thumb_writer_put_pop_regs (&fixture->tw, 2, ARM_REG_R8, ARM_REG_R9);
  assert_output_n_equals (2, 0xe8bd);
  assert_output_n_equals (3, 0x0300);
}

TESTCASE (vpush_range)
{
  gum_thumb_writer_put_vpush_range (&fixture->tw, ARM_REG_Q8, ARM_REG_Q15);
  assert_output_n_equals (0, 0xed6d);
  assert_output_n_equals (1, 0x0b20);

  gum_thumb_writer_put_vpush_range (&fixture->tw, ARM_REG_D0, ARM_REG_D15);
  assert_output_n_equals (2, 0xed2d);
  assert_output_n_equals (3, 0x0b20);

  gum_thumb_writer_put_vpush_range (&fixture->tw, ARM_REG_D16, ARM_REG_D31);
  assert_output_n_equals (4, 0xed6d);
  assert_output_n_equals (5, 0x0b20);

  gum_thumb_writer_put_vpush_range (&fixture->tw, ARM_REG_S0, ARM_REG_S31);
  assert_output_n_equals (6, 0xed2d);
  assert_output_n_equals (7, 0x0a20);
}

TESTCASE (vpop_range)
{
  gum_thumb_writer_put_vpop_range (&fixture->tw, ARM_REG_Q8, ARM_REG_Q15);
  assert_output_n_equals (0, 0xecfd);
  assert_output_n_equals (1, 0x0b20);

  gum_thumb_writer_put_vpop_range (&fixture->tw, ARM_REG_D0, ARM_REG_D15);
  assert_output_n_equals (2, 0xecbd);
  assert_output_n_equals (3, 0x0b20);

  gum_thumb_writer_put_vpop_range (&fixture->tw, ARM_REG_D16, ARM_REG_D31);
  assert_output_n_equals (4, 0xecfd);
  assert_output_n_equals (5, 0x0b20);

  gum_thumb_writer_put_vpop_range (&fixture->tw, ARM_REG_S0, ARM_REG_S31);
  assert_output_n_equals (6, 0xecbd);
  assert_output_n_equals (7, 0x0a20);
}

TESTCASE (ldr_u32)
{
  gum_thumb_writer_put_ldr_reg_u32 (&fixture->tw, ARM_REG_R0, 0x1337);
  gum_thumb_writer_put_ldr_reg_u32 (&fixture->tw, ARM_REG_R1, 0x1227);
  gum_thumb_writer_put_ldr_reg_u32 (&fixture->tw, ARM_REG_R2, 0x1337);
  gum_thumb_writer_flush (&fixture->tw);
  assert_output_n_equals (0, 0x4801);
  assert_output_n_equals (1, 0x4902);
  assert_output_n_equals (2, 0x4a00);
  g_assert_cmphex (((guint32 *) fixture->output)[2], ==, 0x1337);
  g_assert_cmphex (((guint32 *) fixture->output)[3], ==, 0x1227);
}

#ifdef HAVE_ARM

TESTCASE (ldr_in_large_block)
{
  const gsize code_size_in_pages = 1;
  gsize page_size, code_size;
  gpointer code;
  gint (* impl) (void);

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
  gum_memory_patch_code (code, code_size, gum_emit_ldr_in_large_block, code);

  impl = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (code) | 1);
  g_assert_cmpint (impl (), ==, 0x1337);

  gum_memory_free (code, code_size);
}

static void
gum_emit_ldr_in_large_block (gpointer mem,
                             gpointer user_data)
{
  gpointer code = user_data;
  GumThumbWriter tw;
  guint i;

  gum_thumb_writer_init (&tw, mem);
  tw.pc = GUM_ADDRESS (code);

  gum_thumb_writer_put_ldr_reg_u32 (&tw, ARM_REG_R0, 0x1337);
  for (i = 0; i != 511; i++)
    gum_thumb_writer_put_nop (&tw);
  gum_thumb_writer_put_bx_reg (&tw, ARM_REG_LR);

  gum_thumb_writer_clear (&tw);
}

#endif

TESTCASE (ldr_reg_reg_offset)
{
  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_R0, 0);
  assert_output_n_equals (0, 0x6800);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R3,
      ARM_REG_R0, 0);
  assert_output_n_equals (1, 0x6803);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_R3, 0);
  assert_output_n_equals (2, 0x6818);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_R0, 12);
  assert_output_n_equals (3, 0x68c0);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R3,
      ARM_REG_R12, 16);
  assert_output_n_equals (4, 0xf8dc);
  assert_output_n_equals (5, 0x3010);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_SP, 0);
  assert_output_n_equals (6, 0x9800);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R5,
      ARM_REG_SP, 0);
  assert_output_n_equals (7, 0x9d00);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_SP, 12);
  assert_output_n_equals (8, 0x9803);

  gum_thumb_writer_put_ldr_reg_reg_offset (&fixture->tw, ARM_REG_R12,
      ARM_REG_SP, 12);
  assert_output_n_equals (9, 0xf8dd);
  assert_output_n_equals (10, 0xc00c);
}

TESTCASE (ldr_reg_reg)
{
  gum_thumb_writer_put_ldr_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R0);
  assert_output_n_equals (0, 0x6800);

  gum_thumb_writer_put_ldr_reg_reg (&fixture->tw, ARM_REG_R12, ARM_REG_R12);
  assert_output_n_equals (1, 0xf8dc);
  assert_output_n_equals (2, 0xc000);
}

TESTCASE (ldrb_reg_reg)
{
  gum_thumb_writer_put_ldrb_reg_reg (&fixture->tw, ARM_REG_R1, ARM_REG_R3);
  assert_output_n_equals (0, 0x7819);
}

TESTCASE (ldrh_reg_reg)
{
  gum_thumb_writer_put_ldrh_reg_reg (&fixture->tw, ARM_REG_R1, ARM_REG_R3);
  assert_output_n_equals (0, 0x8819);
}

TESTCASE (vldr_reg_reg_offset)
{
  gum_thumb_writer_put_vldr_reg_reg_offset (&fixture->tw, ARM_REG_S1,
      ARM_REG_R2, 4);
  assert_output_n_equals (0, 0xedd2);
  assert_output_n_equals (1, 0x0a01);

  gum_thumb_writer_put_vldr_reg_reg_offset (&fixture->tw, ARM_REG_D2,
      ARM_REG_R3, 8);
  assert_output_n_equals (2, 0xed93);
  assert_output_n_equals (3, 0x2b02);

  gum_thumb_writer_put_vldr_reg_reg_offset (&fixture->tw, ARM_REG_D3,
      ARM_REG_R4, -4);
  assert_output_n_equals (4, 0xed14);
  assert_output_n_equals (5, 0x3b01);

  gum_thumb_writer_put_vldr_reg_reg_offset (&fixture->tw, ARM_REG_D17,
      ARM_REG_R5, -8);
  assert_output_n_equals (6, 0xed55);
  assert_output_n_equals (7, 0x1b02);
}

TESTCASE (str_reg_reg_offset)
{
  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_R0, 0);
  assert_output_n_equals (0, 0x6000);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R7,
      ARM_REG_R0, 0);
  assert_output_n_equals (1, 0x6007);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_R7, 0);
  assert_output_n_equals (2, 0x6038);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_R0, 24);
  assert_output_n_equals (3, 0x6180);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R4,
      ARM_REG_R11, 28);
  assert_output_n_equals (4, 0xf8cb);
  assert_output_n_equals (5, 0x401c);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_SP, 0);
  assert_output_n_equals (6, 0x9000);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R3,
      ARM_REG_SP, 0);
  assert_output_n_equals (7, 0x9300);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R0,
      ARM_REG_SP, 24);
  assert_output_n_equals (8, 0x9006);

  gum_thumb_writer_put_str_reg_reg_offset (&fixture->tw, ARM_REG_R12,
      ARM_REG_SP, 24);
  assert_output_n_equals (9, 0xf8cd);
  assert_output_n_equals (10, 0xc018);
}

TESTCASE (str_reg_reg)
{
  gum_thumb_writer_put_str_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R0);
  assert_output_equals (0x6000);
}

TESTCASE (mov_reg_reg)
{
  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R1);
  /* it al */
  assert_output_n_equals (0, 0xbfe8);
  /* adds r0, r1, #0 */
  assert_output_n_equals (1, 0x1c08);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R7);
  assert_output_n_equals (2, 0xbfe8);
  assert_output_n_equals (3, 0x1c38);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_R7, ARM_REG_R0);
  assert_output_n_equals (4, 0xbfe8);
  assert_output_n_equals (5, 0x1c07);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_SP);
  assert_output_n_equals (6, 0x4668);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_R1, ARM_REG_SP);
  assert_output_n_equals (7, 0x4669);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_LR);
  assert_output_n_equals (8, 0x4670);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_LR, ARM_REG_R0);
  assert_output_n_equals (9, 0x4686);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_LR, ARM_REG_SP);
  assert_output_n_equals (10, 0x46ee);

  gum_thumb_writer_put_mov_reg_reg (&fixture->tw, ARM_REG_PC, ARM_REG_LR);
  assert_output_n_equals (11, 0x46f7);
}

TESTCASE (mov_reg_u8)
{
  gum_thumb_writer_put_mov_reg_u8 (&fixture->tw, ARM_REG_R0, 7);
  /* it al */
  assert_output_n_equals (0, 0xbfe8);
  /* movs r0, #7 */
  assert_output_n_equals (1, 0x2007);

  gum_thumb_writer_put_mov_reg_u8 (&fixture->tw, ARM_REG_R0, 255);
  assert_output_n_equals (2, 0xbfe8);
  assert_output_n_equals (3, 0x20ff);

  gum_thumb_writer_put_mov_reg_u8 (&fixture->tw, ARM_REG_R2, 5);
  assert_output_n_equals (4, 0xbfe8);
  assert_output_n_equals (5, 0x2205);
}

TESTCASE (add_reg_imm)
{
  gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_R0, 255);
  assert_output_n_equals (0, 0xbfe8);
  assert_output_n_equals (1, 0x30ff);

  gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_R3, 255);
  assert_output_n_equals (2, 0xbfe8);
  assert_output_n_equals (3, 0x33ff);

  gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_R0, 42);
  assert_output_n_equals (4, 0xbfe8);
  assert_output_n_equals (5, 0x302a);

  gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_R0, -42);
  assert_output_n_equals (6, 0xbfe8);
  assert_output_n_equals (7, 0x382a);

  gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_SP, 12);
  assert_output_n_equals (8, 0xb003);

  gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_SP, -12);
  assert_output_n_equals (9, 0xb083);

  g_assert_false (gum_thumb_writer_put_add_reg_imm (&fixture->tw, ARM_REG_R8,
      4));
}

TESTCASE (add_reg_reg_reg)
{
  gum_thumb_writer_put_add_reg_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R1,
      ARM_REG_R2);
  /* it al */
  assert_output_n_equals (0, 0xbfe8);
  /* adds r0, r1, r2 */
  assert_output_n_equals (1, 0x1888);

  gum_thumb_writer_put_add_reg_reg_reg (&fixture->tw, ARM_REG_R7, ARM_REG_R1,
      ARM_REG_R2);
  assert_output_n_equals (2, 0xbfe8);
  assert_output_n_equals (3, 0x188f);

  gum_thumb_writer_put_add_reg_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R7,
      ARM_REG_R2);
  assert_output_n_equals (4, 0xbfe8);
  assert_output_n_equals (5, 0x18b8);

  gum_thumb_writer_put_add_reg_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R1,
      ARM_REG_R7);
  assert_output_n_equals (6, 0xbfe8);
  assert_output_n_equals (7, 0x19c8);

  gum_thumb_writer_put_add_reg_reg_reg (&fixture->tw, ARM_REG_R9, ARM_REG_R9,
      ARM_REG_R0);
  assert_output_n_equals (8, 0x4481);
}

TESTCASE (add_reg_reg)
{
  gum_thumb_writer_put_add_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R1);
  assert_output_n_equals (0, 0x4408);

  gum_thumb_writer_put_add_reg_reg (&fixture->tw, ARM_REG_R12, ARM_REG_R1);
  assert_output_n_equals (1, 0x448c);

  gum_thumb_writer_put_add_reg_reg (&fixture->tw, ARM_REG_R3, ARM_REG_R12);
  assert_output_n_equals (2, 0x4463);
}

TESTCASE (add_reg_reg_imm)
{
  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_SP,
      36);
  assert_output_n_equals (0, 0xa909);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R7, ARM_REG_SP,
      36);
  assert_output_n_equals (1, 0xaf09);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_PC,
      36);
  assert_output_n_equals (2, 0xa109);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_SP,
      12);
  assert_output_n_equals (3, 0xa903);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_R7,
      5);
  assert_output_n_equals (4, 0xbfe8);
  assert_output_n_equals (5, 0x1d79);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R5, ARM_REG_R7,
      5);
  assert_output_n_equals (6, 0xbfe8);
  assert_output_n_equals (7, 0x1d7d);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_R3,
      5);
  assert_output_n_equals (8, 0xbfe8);
  assert_output_n_equals (9, 0x1d59);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_R7,
      3);
  assert_output_n_equals (10, 0xbfe8);
  assert_output_n_equals (11, 0x1cf9);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_R7,
      -3);
  assert_output_n_equals (12, 0xbfe8);
  assert_output_n_equals (13, 0x1ef9);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_R0, ARM_REG_R0,
      255);
  assert_output_n_equals (14, 0xbfe8);
  assert_output_n_equals (15, 0x30ff);

  gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw, ARM_REG_SP, ARM_REG_SP,
      4);
  assert_output_n_equals (16, 0xb001);

  g_assert_false (gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw,
      ARM_REG_R0, ARM_REG_R8, 4));

  g_assert_false (gum_thumb_writer_put_add_reg_reg_imm (&fixture->tw,
      ARM_REG_R8, ARM_REG_R0, 4));
}

TESTCASE (sub_reg_imm)
{
  gum_thumb_writer_put_sub_reg_imm (&fixture->tw, ARM_REG_R0, 42);
  assert_output_n_equals (0, 0xbfe8);
  assert_output_n_equals (1, 0x382a);
}

TESTCASE (sub_reg_reg_reg)
{
  gum_thumb_writer_put_sub_reg_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R1,
      ARM_REG_R2);
  /* it al */
  assert_output_n_equals (0, 0xbfe8);
  /* subs r0, r1, r2 */
  assert_output_n_equals (1, 0x1a88);

  gum_thumb_writer_put_sub_reg_reg_reg (&fixture->tw, ARM_REG_R7, ARM_REG_R1,
      ARM_REG_R2);
  assert_output_n_equals (2, 0xbfe8);
  assert_output_n_equals (3, 0x1a8f);

  gum_thumb_writer_put_sub_reg_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R7,
      ARM_REG_R2);
  assert_output_n_equals (4, 0xbfe8);
  assert_output_n_equals (5, 0x1ab8);

  gum_thumb_writer_put_sub_reg_reg_reg (&fixture->tw, ARM_REG_R0, ARM_REG_R1,
      ARM_REG_R7);
  assert_output_n_equals (6, 0xbfe8);
  assert_output_n_equals (7, 0x1bc8);
}

TESTCASE (sub_reg_reg_imm)
{
  gum_thumb_writer_put_sub_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_R7,
      5);
  assert_output_n_equals (0, 0xbfe8);
  assert_output_n_equals (1, 0x1f79);
}

TESTCASE (and_reg_reg_imm)
{
  g_assert_false (gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw,
      ARM_REG_R0, ARM_REG_R0, -1));

  g_assert_false (gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw,
      ARM_REG_R0, ARM_REG_R0, 256));

  gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw, ARM_REG_R0, ARM_REG_R0,
      0);
  assert_output_n_equals (0, 0xf000);
  assert_output_n_equals (1, 0x0000);

  gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw, ARM_REG_R0, ARM_REG_R0,
      255);
  assert_output_n_equals (2, 0xf000);
  assert_output_n_equals (3, 0x00ff);

  gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw, ARM_REG_R0, ARM_REG_R7,
      0);
  assert_output_n_equals (4, 0xf007);
  assert_output_n_equals (5, 0x0000);

  gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw, ARM_REG_R7, ARM_REG_R0,
      0);
  assert_output_n_equals (6, 0xf000);
  assert_output_n_equals (7, 0x0700);

  gum_thumb_writer_put_and_reg_reg_imm (&fixture->tw, ARM_REG_R5, ARM_REG_R3,
      53);
  assert_output_n_equals (8, 0xf003);
  assert_output_n_equals (9, 0x0535);
}

TESTCASE (lsls_reg_reg_imm)
{
  gum_thumb_writer_put_lsls_reg_reg_imm (&fixture->tw, ARM_REG_R1, ARM_REG_R3,
      7);
  assert_output_n_equals (0, 0x01d9);
}

TESTCASE (lsrs_reg_reg_imm)
{
  gum_thumb_writer_put_lsrs_reg_reg_imm (&fixture->tw, ARM_REG_R3, ARM_REG_R7,
      9);
  assert_output_n_equals (0, 0x0a7b);
}

TESTCASE (mrs_reg_reg)
{
  gum_thumb_writer_put_mrs_reg_reg (&fixture->tw, ARM_REG_R1,
      ARM_SYSREG_APSR_NZCVQ);
  assert_output_n_equals (0, 0xf3ef);
  assert_output_n_equals (1, 0x8100);

  gum_thumb_writer_put_mrs_reg_reg (&fixture->tw, ARM_REG_R7,
      ARM_SYSREG_APSR_NZCVQ);
  assert_output_n_equals (2, 0xf3ef);
  assert_output_n_equals (3, 0x8700);
}

TESTCASE (msr_reg_reg)
{
  gum_thumb_writer_put_msr_reg_reg (&fixture->tw, ARM_SYSREG_APSR_NZCVQ,
      ARM_REG_R1);
  assert_output_n_equals (0, 0xf381);
  assert_output_n_equals (1, 0x8800);

  gum_thumb_writer_put_msr_reg_reg (&fixture->tw, ARM_SYSREG_APSR_NZCVQ,
      ARM_REG_R7);
  assert_output_n_equals (2, 0xf387);
  assert_output_n_equals (3, 0x8800);
}

TESTCASE (nop)
{
  gum_thumb_writer_put_nop (&fixture->tw);
  assert_output_equals (0xbf00);
}
