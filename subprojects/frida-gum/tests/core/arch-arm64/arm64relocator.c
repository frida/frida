/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Haiwei Wang <haiwei.wang1109@gmail.com>
 * Copyright (C) 2026 inforcqb <fanjiawei080615@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "arm64relocator-fixture.c"

TESTLIST_BEGIN (arm64relocator)
  TESTENTRY (one_to_one)
  TESTENTRY (ldr_x_should_be_rewritten)
  TESTENTRY (ldr_w_should_be_rewritten)
  TESTENTRY (ldr_d_should_be_rewritten)
  TESTENTRY (ldrsw_x_should_be_rewritten)
  TESTENTRY (adr_should_be_rewritten)
  TESTENTRY (adrp_should_be_rewritten)
  TESTENTRY (cbz_should_be_rewritten)
  TESTENTRY (tbnz_should_be_rewritten)
  TESTENTRY (b_cond_should_be_rewritten)
  TESTENTRY (b_should_be_rewritten)
  TESTENTRY (bl_should_be_rewritten)
  TESTENTRY (cannot_relocate_with_early_br)
  TESTENTRY (cannot_relocate_with_internal_cbz_target)
  TESTENTRY (eob_and_eoi_on_br)
  TESTENTRY (eob_and_eoi_on_ret)
TESTLIST_END ()

TESTCASE (one_to_one)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0xa9be4ff4), /* stp x20, x19, [sp, #-32]! */
    GUINT32_TO_LE (0x92800210), /* movn x16, #0x10           */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  insn = NULL;
  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  assert_outbuf_still_zeroed_from_offset (0);

  insn = NULL;
  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 8);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  g_assert_cmpint (memcmp (fixture->output, input, 4), ==, 0);
  assert_outbuf_still_zeroed_from_offset (4);

  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  g_assert_cmpint (memcmp (fixture->output + 4, input + 1, 4), ==, 0);
  assert_outbuf_still_zeroed_from_offset (8);

  g_assert_false (gum_arm64_relocator_write_one (&fixture->rl));
}

TESTCASE (ldr_x_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x58000050)  /* ldr x16, [pc, #8]  */
  };
  const guint32 expected_output_instructions[] = {
    GUINT32_TO_LE (0x58000050), /* ldr x16, [pc, #8]  */
    GUINT32_TO_LE (0xf9400210), /* ldr x16, [x16]     */
    0xffffffff,                 /* <calculated PC     */
    0xffffffff                  /*  goes here>        */
  };
  gchar expected_output[4 * sizeof (guint32)];
  guint64 calculated_pc;
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 8;
  *((guint64 *) (expected_output + 8)) = calculated_pc;

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_LDR);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (ldr_w_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x18000042)  /* ldr w2, [pc, #8]   */
  };
  const guint32 expected_output_instructions[] = {
    GUINT32_TO_LE (0x58000042), /* ldr x2, [pc, #8]   */
    GUINT32_TO_LE (0xb9400042), /* ldr w2, [x2]       */
    0xffffffff,                 /* <calculated PC     */
    0xffffffff                  /*  goes here>        */
  };
  gchar expected_output[4 * sizeof (guint32)];
  guint64 calculated_pc;
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 8;
  *((guint64 *) (expected_output + 8)) = calculated_pc;

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_LDR);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (ldr_d_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x5c000041)  /* ldr d1, [pc, #8]   */
  };
  const guint32 expected_output_instructions[] = {
    GUINT32_TO_LE (0xa9bf07e0), /* push {x0, x1}      */
    GUINT32_TO_LE (0x58000060), /* ldr x0, [pc, #16]  */
    GUINT32_TO_LE (0xfd400001), /* ldr d1, [x0]       */
    GUINT32_TO_LE (0xa8c107e0), /* pop {x0, x1}       */
    0xffffffff,                 /* <calculated PC     */
    0xffffffff                  /*  goes here>        */
  };
  gchar expected_output[6 * sizeof (guint32)];
  guint64 calculated_pc;
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 8;
  *((guint64 *) (expected_output + 16)) = calculated_pc;

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_LDR);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (ldrsw_x_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x98000048)  /* ldrsw x8, [pc, #8] */
  };
  const guint32 expected_output_instructions[] = {
    GUINT32_TO_LE (0x58000048), /* ldr x8, [pc, #8]   */
    GUINT32_TO_LE (0xb9800108), /* ldrsw x8, [x8]     */
    0xffffffff,                 /* <calculated PC     */
    0xffffffff                  /*  goes here>        */
  };
  gchar expected_output[4 * sizeof (guint32)];
  guint64 calculated_pc;
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 8;
  *((guint64 *) (expected_output + 8)) = calculated_pc;

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_LDRSW);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (adr_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x5000a721)  /* adr x1, 0x14e6     */
  };
  const guint32 expected_output_instructions[] = {
    GUINT32_TO_LE (0x58000021), /* ldr x1, [pc, #4]   */
    0xffffffff,                 /* <calculated PC     */
    0xffffffff                  /*  goes here>        */
  };
  gchar expected_output[3 * sizeof (guint32)];
  guint64 calculated_pc;
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 0x14e6;
  *((guint64 *) (expected_output + 4)) = calculated_pc;

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_ADR);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (adrp_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0xd000a723)  /* adrp x3, 0x14e6000 */
  };
  const guint32 expected_output_instructions[] = {
    GUINT32_TO_LE (0x58000023), /* ldr x3, [pc, #4]   */
    0xffffffff,                 /* <calculated PC     */
    0xffffffff                  /*  goes here>        */
  };
  gchar expected_output[3 * sizeof (guint32)];
  guint64 calculated_pc;
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc =
      (fixture->rl.input_pc & ~G_GUINT64_CONSTANT (4096 - 1)) + 0x14e6000;
  *((guint64 *) (expected_output + 4)) = calculated_pc;

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_ADRP);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (cbz_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0xb40000c0)  /* cbz x0, #+6       */
  };
  const guint32 expected_output[] = {
    GUINT32_TO_LE (0xb4000040), /* cbz x0, #+2       */
    GUINT32_TO_LE (0x14000003), /* b +3              */
    GUINT32_TO_LE (0x58000050), /* ldr x16, [pc, #8] */
    GUINT32_TO_LE (0xd65f0200)  /* ret x16           */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_CBZ);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (tbnz_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x37480061)  /* tbnz w1, #9, #+3 */
  };
  const guint32 expected_output[] = {
    GUINT32_TO_LE (0x37480041), /* tbnz w1, #9, #+2  */
    GUINT32_TO_LE (0x14000003), /* b +3              */
    GUINT32_TO_LE (0x58000050), /* ldr x16, [pc, #8] */
    GUINT32_TO_LE (0xd65f0200)  /* ret x16           */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_TBNZ);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

TESTCASE (b_cond_should_be_rewritten)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0x540000c3)  /* b.lo #+6          */
  };
  const guint32 expected_output[] = {
    GUINT32_TO_LE (0x54000043), /* b.lo #+2          */
    GUINT32_TO_LE (0x14000003), /* b +3              */
    GUINT32_TO_LE (0x58000050), /* ldr x16, [pc, #8] */
    GUINT32_TO_LE (0xd65f0200)  /* ret x16           */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM64_INS_B);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, expected_output,
      sizeof (expected_output)), ==, 0);
}

typedef struct _BranchScenario BranchScenario;

struct _BranchScenario
{
  guint instruction_id;
  guint32 input[1];
  gsize input_length;
  guint32 expected_output[4];
  gsize expected_output_length;
  gsize pc_offset;
  gssize expected_pc_distance;
};

static void branch_scenario_execute (BranchScenario * bs,
    TestArm64RelocatorFixture * fixture);

TESTCASE (b_should_be_rewritten)
{
  BranchScenario bs = {
    ARM64_INS_B,
    { 0x17ffff5a }, 1,  /* b #-664            */
    {
      0x58000050,       /* ldr x16, [pc, #8]  */
      0xd65f0200,       /* ret x16            */
      0xffffffff,       /* <calculated PC     */
      0xffffffff        /*  goes here>        */
    }, 4,
    2, -664
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (bl_should_be_rewritten)
{
  BranchScenario bs = {
    ARM64_INS_BL,
    { 0x97ffff5a }, 1,  /* bl #-664           */
    {
      0x5800005e,       /* ldr lr, [pc, #8]   */
      0xd63f03c0,       /* blr lr             */
      0xffffffff,       /* <calculated PC     */
      0xffffffff        /*  goes here>        */
    }, 4,
    2, -664
  };
  branch_scenario_execute (&bs, fixture);
}

static void
branch_scenario_execute (BranchScenario * bs,
                         TestArm64RelocatorFixture * fixture)
{
  gsize i;
  guint64 calculated_pc;
  const cs_insn * insn;

  for (i = 0; i != bs->input_length; i++)
    bs->input[i] = GUINT32_TO_LE (bs->input[i]);
  for (i = 0; i != bs->expected_output_length; i++)
    bs->expected_output[i] = GUINT32_TO_LE (bs->expected_output[i]);

  SETUP_RELOCATOR_WITH (bs->input);

  calculated_pc = fixture->rl.input_pc + bs->expected_pc_distance;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  bs->expected_output[bs->pc_offset + 0] =
      GUINT32_TO_LE ((calculated_pc >> 0) & 0xffffffff);
  bs->expected_output[bs->pc_offset + 1] =
      GUINT32_TO_LE ((calculated_pc >> 32) & 0xffffffff);
#else
  bs->expected_output[bs->pc_offset + 1] =
      GUINT32_TO_BE ((calculated_pc >> 0) & 0xffffffff);
  bs->expected_output[bs->pc_offset + 0] =
      GUINT32_TO_BE ((calculated_pc >> 32) & 0xffffffff);
#endif

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, bs->instruction_id);
  g_assert_true (gum_arm64_relocator_write_one (&fixture->rl));
  gum_arm64_writer_flush (&fixture->aw);
  g_assert_cmpint (memcmp (fixture->output, bs->expected_output,
      bs->expected_output_length * sizeof (guint32)), ==, 0);
}

TESTCASE (cannot_relocate_with_early_br)
{
  guint32 input[] = {
    GUINT32_TO_LE (0x58000050), /* ldr x16, [pc, #8] */
    GUINT32_TO_LE (0xd61f0200)  /* br x16            */
  };

  g_assert_false (gum_arm64_relocator_can_relocate (input, 16,
      GUM_SCENARIO_OFFLINE, GUM_RELOCATION_CHECKED, NULL, NULL));
}

TESTCASE (cannot_relocate_with_internal_cbz_target)
{
  /*
   * Mirrors libdispatch's dispatch_mach_msg_get_msg(): a leading CBZ that lands
   * inside the first 16 bytes. A full redirect would overwrite the target and
   * the rewritten CBZ would jump into the middle of the patch.
   */
  guint32 input[] = {
    GUINT32_TO_LE (0xb4000061), /* cbz x1, #+12 */
    GUINT32_TO_LE (0xf9402808), /* ldr x8, [x0, #0x50] */
    GUINT32_TO_LE (0xf9000028), /* str x8, [x1] */
    GUINT32_TO_LE (0xb9404808), /* ldr w8, [x0, #0x48] */
    GUINT32_TO_LE (0x91016000), /* add x0, x0, #0x58 */
    GUINT32_TO_LE (0x34000048), /* cbz w8, #+8 */
    GUINT32_TO_LE (0xf9400000), /* ldr x0, [x0] */
    GUINT32_TO_LE (0xd65f03c0)  /* ret */
  };
  guint maximum = 0;

  g_assert_false (gum_arm64_relocator_can_relocate (input, 16,
      GUM_SCENARIO_OFFLINE, GUM_RELOCATION_CHECKED, &maximum, NULL));
  g_assert_cmpuint (maximum, ==, 12);
  g_assert_true (gum_arm64_relocator_can_relocate (input, 8,
      GUM_SCENARIO_OFFLINE, GUM_RELOCATION_CHECKED, &maximum, NULL));
  g_assert_cmpuint (maximum, >=, 8);
}

TESTCASE (eob_and_eoi_on_br)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0xd61f0200)  /* br x16 */
  };

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, NULL), ==, 4);
  g_assert_true (gum_arm64_relocator_eob (&fixture->rl));
  g_assert_true (gum_arm64_relocator_eoi (&fixture->rl));
  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, NULL), ==, 0);
}

TESTCASE (eob_and_eoi_on_ret)
{
  const guint32 input[] = {
    GUINT32_TO_LE (0xd65f03c0)  /* ret */
  };

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, NULL), ==, 4);
  g_assert_true (gum_arm64_relocator_eob (&fixture->rl));
  g_assert_true (gum_arm64_relocator_eoi (&fixture->rl));
  g_assert_cmpuint (gum_arm64_relocator_read_one (&fixture->rl, NULL), ==, 0);
}
