/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "armwriter-fixture.c"

TESTLIST_BEGIN (armwriter)
  TESTENTRY (ldr_u32)
  TESTENTRY (ldr_pc_u32)
#ifdef HAVE_ARM
  TESTENTRY (ldr_in_large_block)
#endif
  TESTENTRY (nop)
  TESTENTRY (ldmia_with_rn_in_reglist)
  TESTENTRY (ldmia_with_rn_in_reglist_wb)
  TESTENTRY (vpush_range)
  TESTENTRY (vpop_range)
TESTLIST_END ()

#ifdef HAVE_ARM
static void gum_emit_ldr_in_large_block (gpointer mem, gpointer user_data);
#endif

TESTCASE (ldr_u32)
{
  gum_arm_writer_put_ldr_reg_u32 (&fixture->aw, ARM_REG_R0, 0x1337);
  gum_arm_writer_put_ldr_reg_u32 (&fixture->aw, ARM_REG_R1, 0x1227);
  gum_arm_writer_put_ldr_reg_u32 (&fixture->aw, ARM_REG_R2, 0x1337);
  gum_arm_writer_flush (&fixture->aw);
  assert_output_n_equals (0, 0xe59f0004);
  assert_output_n_equals (1, 0xe59f1004);
  assert_output_n_equals (2, 0xe51f2004);
  g_assert_cmphex (fixture->output[3 + 0], ==, 0x1337);
  g_assert_cmphex (fixture->output[3 + 1], ==, 0x1227);
}

TESTCASE (ldr_pc_u32)
{
  gum_arm_writer_put_ldr_reg_u32 (&fixture->aw, ARM_REG_PC, 0xdeadbeef);
  gum_arm_writer_flush (&fixture->aw);
  assert_output_n_equals (0, 0xe51ff004);
  g_assert_cmphex (fixture->output[1 + 0], ==, 0xdeadbeef);
}

#ifdef HAVE_ARM

TESTCASE (ldr_in_large_block)
{
  const gsize code_size_in_pages = 2;
  gsize page_size, code_size;
  gpointer code;
  gint (* impl) (void);

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
  gum_memory_patch_code (code, code_size, gum_emit_ldr_in_large_block, code);

  impl = code;
  g_assert_cmpint (impl (), ==, 0x1337);

  gum_memory_free (code, code_size);
}

static void
gum_emit_ldr_in_large_block (gpointer mem,
                             gpointer user_data)
{
  gpointer code = user_data;
  GumArmWriter aw;
  guint i;

  gum_arm_writer_init (&aw, mem);
  aw.pc = GUM_ADDRESS (code);

  gum_arm_writer_put_ldr_reg_u32 (&aw, ARM_REG_R0, 0x1337);
  for (i = 0; i != 1024; i++)
    gum_arm_writer_put_nop (&aw);
  gum_arm_writer_put_bx_reg (&aw, ARM_REG_LR);

  gum_arm_writer_clear (&aw);
}

#endif

TESTCASE (nop)
{
  gum_arm_writer_put_nop (&fixture->aw);
  assert_output_equals (0xe1a00000); /* mov r0, r0 */
}

TESTCASE (ldmia_with_rn_in_reglist)
{
  GumArmRegInfo ri;
  guint16 mask = 0;

  gum_arm_reg_describe (ARM_REG_R4, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R5, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R6, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R7, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R8, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R9, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R10, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R11, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R12, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_SP, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_PC, &ri);
  mask |= 1 << ri.index;

  gum_arm_writer_put_ldmia_reg_mask (&fixture->aw, ARM_REG_SP, mask);
  gum_arm_writer_flush (&fixture->aw);
  /* pop {r4, r5, r6, r7, r8, sb, sl, fp, ip, sp, pc} */
  assert_output_n_equals (0, 0xe89dbff0);
}

TESTCASE (ldmia_with_rn_in_reglist_wb)
{
  GumArmRegInfo ri;
  guint16 mask = 0;

  gum_arm_reg_describe (ARM_REG_R4, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R5, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R6, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R7, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R8, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R9, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R10, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R11, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_R12, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_SP, &ri);
  mask |= 1 << ri.index;
  gum_arm_reg_describe (ARM_REG_PC, &ri);
  mask |= 1 << ri.index;

  gum_arm_writer_put_ldmia_reg_mask_wb (&fixture->aw, ARM_REG_SP, mask);
  gum_arm_writer_flush (&fixture->aw);
  /* pop {r4, r5, r6, r7, r8, sb, sl, fp, ip, sp, pc} */
  assert_output_n_equals (0, 0xe8bdbff0);
}

TESTCASE (vpush_range)
{
  gum_arm_writer_put_vpush_range (&fixture->aw, ARM_REG_Q8, ARM_REG_Q15);
  assert_output_n_equals (0, 0xed6d0b20);

  gum_arm_writer_put_vpush_range (&fixture->aw, ARM_REG_D0, ARM_REG_D15);
  assert_output_n_equals (1, 0xed2d0b20);

  gum_arm_writer_put_vpush_range (&fixture->aw, ARM_REG_D16, ARM_REG_D31);
  assert_output_n_equals (2, 0xed6d0b20);

  gum_arm_writer_put_vpush_range (&fixture->aw, ARM_REG_S0, ARM_REG_S31);
  assert_output_n_equals (3, 0xed2d0a20);
}

TESTCASE (vpop_range)
{
  gum_arm_writer_put_vpop_range (&fixture->aw, ARM_REG_Q8, ARM_REG_Q15);
  assert_output_n_equals (0, 0xecfd0b20);

  gum_arm_writer_put_vpop_range (&fixture->aw, ARM_REG_D0, ARM_REG_D15);
  assert_output_n_equals (1, 0xecbd0b20);

  gum_arm_writer_put_vpop_range (&fixture->aw, ARM_REG_D16, ARM_REG_D31);
  assert_output_n_equals (2, 0xecfd0b20);

  gum_arm_writer_put_vpop_range (&fixture->aw, ARM_REG_S0, ARM_REG_S31);
  assert_output_n_equals (3, 0xecbd0a20);
}
