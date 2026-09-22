/*
 * Copyright (C) 2010-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "thumbrelocator-fixture.c"

TESTLIST_BEGIN (thumbrelocator)
  TESTENTRY (one_to_one)
  TESTENTRY (handle_extended_instructions)

  TESTENTRY (ldrpc_t1_should_be_rewritten)
  TESTENTRY (ldrpc_t2_should_be_rewritten)
  TESTENTRY (vldrpc_t1_should_be_rewritten)
  TESTENTRY (vldrpc_t2_should_be_rewritten)
  TESTENTRY (adr_should_be_rewritten)
  TESTENTRY (adr_unaligned_should_be_rewritten)
  TESTENTRY (addh_should_be_rewritten_if_pc_relative)
  TESTENTRY (bl_sequence_should_be_rewritten)
  TESTENTRY (b_imm_t2_positive_should_be_rewritten)
  TESTENTRY (b_imm_t2_negative_should_be_rewritten)
  TESTENTRY (b_imm_t4_positive_should_be_rewritten)
  TESTENTRY (b_imm_t4_negative_should_be_rewritten)
  TESTENTRY (bl_imm_t1_positive_should_be_rewritten)
  TESTENTRY (bl_imm_t1_negative_should_be_rewritten)
  TESTENTRY (blx_imm_t2_positive_should_be_rewritten)
  TESTENTRY (blx_imm_t2_negative_should_be_rewritten)
  TESTENTRY (cbz_should_be_rewritten)
  TESTENTRY (cbnz_should_be_rewritten)
  TESTENTRY (b_cond_should_be_rewritten)
  TESTENTRY (it_block_with_pc_relative_load_should_be_rewritten)
  TESTENTRY (it_block_with_b_should_be_rewritten)
  TESTENTRY (it_block_should_be_rewritten_as_a_whole)
  TESTENTRY (it_block_with_eoi_insn_should_be_rewritten)
  TESTENTRY (eob_and_eoi_on_ret)
TESTLIST_END ()

TESTCASE (one_to_one)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xb580), /* push {r7, lr}  */
    GUINT16_TO_LE (0xaf00), /* add r7, sp, #0 */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_PUSH);
  assert_outbuf_still_zeroed_from_offset (0);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM_INS_ADD);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_cmpint (memcmp (fixture->output, input, 2), ==, 0);
  assert_outbuf_still_zeroed_from_offset (2);

  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_cmpint (memcmp (((guint8 *) fixture->output) + 2, input + 1, 2),
      ==, 0);
  assert_outbuf_still_zeroed_from_offset (4);

  g_assert_false (gum_thumb_relocator_write_one (&fixture->rl));
}

TESTCASE (handle_extended_instructions)
{
  const guint16 input[] = {
    /* stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr} */
    GUINT16_TO_LE (0xe92d), GUINT16_TO_LE (0x4ff0),
    GUINT16_TO_LE (0xb580), /* push {r7, lr}  */
    GUINT16_TO_LE (0xf241), GUINT16_TO_LE (0x3037), /* movw r0, #4919 */
  };

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, NULL), ==, 4);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, NULL), ==, 6);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, NULL), ==, 10);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_false (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_cmpint (memcmp (fixture->output, input, sizeof (input)), ==, 0);
}

TESTCASE (ldrpc_t1_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0x4a03), /* ldr r2, [pc, #12] */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0x4a00), /* ldr r2, [pc, #0] */
    GUINT16_TO_LE (0x6812), /* ldr r2, r2       */
    GUINT16_TO_LE (0xffff), /* <calculated PC   */
    GUINT16_TO_LE (0xffff), /*  goes here>      */
  };
  gchar expected_output[4 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = (fixture->rl.input_pc + 4 + 12) & ~(4 - 1);
  *((guint32 *) (expected_output + 4)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_LDR);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (ldrpc_t2_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xf8df), GUINT16_TO_LE (0x2768) /* ldr.w r2, [pc, #1896] */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0x4a00), /* ldr r2, [pc, #0] */
    GUINT16_TO_LE (0x6812), /* ldr r2, r2       */
    GUINT16_TO_LE (0xffff), /* <calculated PC   */
    GUINT16_TO_LE (0xffff), /*  goes here>      */
  };
  gchar expected_output[4 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = (fixture->rl.input_pc + 4 + 1896) & ~(4 - 1);
  *((guint32 *) (expected_output + 4)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM_INS_LDR);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (vldrpc_t1_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xeddf),
    GUINT16_TO_LE (0x0a00), /* vldr  s1, [pc, #0] */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xb401), /* push {r0}          */
    GUINT16_TO_LE (0x4802), /* ldr  r0, [pc, #8]  */
    GUINT16_TO_LE (0xedd0), /* ...                */
    GUINT16_TO_LE (0x0a00), /* vldr s1, [r0]      */
    GUINT16_TO_LE (0xbc01), /* pop  {r0}          */
    GUINT16_TO_LE (0xbf00), /* nop                */
    GUINT16_TO_LE (0xffff), /* <calculated PC     */
    GUINT16_TO_LE (0xffff), /*  goes here>        */
  };
  gchar expected_output[8 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));

  calculated_pc = (fixture->rl.input_pc + 4) & ~(4 - 1);
  *((guint32 *) (expected_output + 12)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM_INS_VLDR);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);

  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (vldrpc_t2_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xed9f),
    GUINT16_TO_LE (0x1b00), /* vldr  d1, [pc, #0] */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xb401), /* push {r0}          */
    GUINT16_TO_LE (0x4802), /* ldr  r0, [pc, #8]  */
    GUINT16_TO_LE (0xed90), /* ...                */
    GUINT16_TO_LE (0x1b00), /* vldr d1, [r0]      */
    GUINT16_TO_LE (0xbc01), /* pop  {r0}          */
    GUINT16_TO_LE (0xbf00), /* nop                */
    GUINT16_TO_LE (0xffff), /* <calculated PC     */
    GUINT16_TO_LE (0xffff), /*  goes here>        */
  };
  gchar expected_output[8 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));

  calculated_pc = (fixture->rl.input_pc + 4) & ~(4 - 1);
  *((guint32 *) (expected_output + 12)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM_INS_VLDR);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);

  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (adr_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xa107),   /* adr r1, #0x1c    */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xb401),   /* push {r0}        */
    GUINT16_TO_LE (0x4902),   /* ldr r1, [pc, #8] */
    GUINT16_TO_LE (0x4802),   /* ldr r0, [pc, #8] */
    GUINT16_TO_LE (0x4401),   /* add r1, r0       */
    GUINT16_TO_LE (0xbc01),   /* pop {r0}         */
    GUINT16_TO_LE (0xbf00),   /* nop              */
    GUINT16_TO_LE (0xffff),   /* <calculated PC   */
    GUINT16_TO_LE (0xffff),   /*  goes here>      */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    GUINT16_TO_LE (0x001c),   /* <immediate       */
    GUINT16_TO_LE (0x0000),   /*  goes here>      */
#else
    GUINT16_TO_BE (0x0000),   /* <immediate       */
    GUINT16_TO_BE (0x001c),   /*  goes here>      */
#endif
  };
  gchar expected_output[10 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 4;
  *((guint32 *) (expected_output + 12)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_ADR);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (adr_unaligned_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0x4600),   /* mov r0, r0       */
    GUINT16_TO_LE (0xa107),   /* adr r1, #0x1c    */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0x4600),   /* mov r0, r0       */
    GUINT16_TO_LE (0xb401),   /* push {r0}        */
    GUINT16_TO_LE (0x4901),   /* ldr r1, [pc, #4] */
    GUINT16_TO_LE (0x4802),   /* ldr r0, [pc, #8] */
    GUINT16_TO_LE (0x4401),   /* add r1, r0       */
    GUINT16_TO_LE (0xbc01),   /* pop {r0}         */
    GUINT16_TO_LE (0xffff),   /* <calculated PC   */
    GUINT16_TO_LE (0xffff),   /*  goes here>      */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    GUINT16_TO_LE (0x001c),   /* <immediate       */
    GUINT16_TO_LE (0x0000),   /*  goes here>      */
#else
    GUINT16_TO_BE (0x0000),   /* <immediate       */
    GUINT16_TO_BE (0x001c),   /*  goes here>      */
#endif
  };
  gchar expected_output[10 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 4;
  *((guint32 *) (expected_output + 12)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_MOV);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpint (insn->id, ==, ARM_INS_ADR);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));

  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (addh_should_be_rewritten_if_pc_relative)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0x447a),   /* add r2, pc       */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xb401),   /* push {r0}        */
    GUINT16_TO_LE (0x4801),   /* ldr r0, [pc, #4] */
    GUINT16_TO_LE (0x4402),   /* add r2, r0       */
    GUINT16_TO_LE (0xbc01),   /* pop {r0}         */
    GUINT16_TO_LE (0xffff),   /* <calculated PC   */
    GUINT16_TO_LE (0xffff),   /*  goes here>      */
  };
  gchar expected_output[6 * sizeof (guint16)];

  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_pc = fixture->rl.input_pc + 4;
  *((guint32 *) (expected_output + 8)) = calculated_pc;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_ADD);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (bl_sequence_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xb573),      /* push {r0, r1, r4, r5, r6, lr} */
    GUINT16_TO_LE (0xf001), GUINT16_TO_LE (0xfbc9), /* bl 0x1543c */
    GUINT16_TO_LE (0xf7fb), GUINT16_TO_LE (0xeca0), /* blx 0xf5ec */
  };
  const guint16 expected_output_instructions[16] = {
    GUINT16_TO_LE (0xb573),      /* push {r0, r1, r4, r5, r6, lr} */
    GUINT16_TO_LE (0xb401),                  /* push {r0}         */
    GUINT16_TO_LE (0x4804),                  /* ldr r0, [pc, #16] */
    GUINT16_TO_LE (0x4686),                  /* mov lr, r0        */
    GUINT16_TO_LE (0xbc01),                  /* pop {r0}          */
    GUINT16_TO_LE (0x47f0),                  /* blx lr            */
    GUINT16_TO_LE (0xb401),                  /* push {r0}         */
    GUINT16_TO_LE (0x4803),                  /* ldr r0, [pc, #12] */
    GUINT16_TO_LE (0x4686),                  /* mov lr, r0        */
    GUINT16_TO_LE (0xbc01),                  /* pop {r0}          */
    GUINT16_TO_LE (0x47f0),                  /* blx lr            */
    GUINT16_TO_LE (0xbf00),                  /* <padding nop>     */
    GUINT16_TO_LE (0xffff),                  /* <calculated PC1   */
    GUINT16_TO_LE (0xffff),                  /*  goes here>       */
    GUINT16_TO_LE (0xffff),                  /* <calculated PC2   */
    GUINT16_TO_LE (0xffff),                  /*  goes here>       */
  };
  gchar expected_output[16 * sizeof (guint16)];

  const cs_insn * insn = NULL;

  fixture->tw.pc = 0x200000;
  SETUP_RELOCATOR_WITH (input);
  fixture->rl.input_pc = 0x13ca4;

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  *((guint32 *) (expected_output + 24)) = 0x1543c | 1;
  *((guint32 *) (expected_output + 28)) = 0xf5ec;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_PUSH);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 6);
  g_assert_cmpint (insn->id, ==, ARM_INS_BL);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 10);
  g_assert_cmpint (insn->id, ==, ARM_INS_BLX);
  gum_thumb_relocator_write_all (&fixture->rl);
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

typedef struct _BranchScenario BranchScenario;

struct _BranchScenario
{
  guint instruction_id;
  guint16 input[2];
  gsize input_length;
  gsize instruction_length;
  guint16 expected_output[8];
  gsize expected_output_length;
  gsize pc_offset;
  gssize expected_pc_distance;
};

static void branch_scenario_execute (BranchScenario * bs,
    TestThumbRelocatorFixture * fixture);

TESTCASE (b_imm_t2_positive_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_B,
    { 0xe004 }, 1, 2,           /* b pc + 8         */
    {
      0xb401,                   /* push {r0}        */
      0xb401,                   /* push {r0}        */
      0x4801,                   /* ldr r0, [pc, #4] */
      0x9001,                   /* str r0, [sp, #4] */
      0xbd01,                   /* pop {r0, pc}     */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, 9
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (b_imm_t2_negative_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_B,
    { 0xe7fc }, 1, 2,           /* b pc - 8         */
    {
      0xb401,                   /* push {r0}        */
      0xb401,                   /* push {r0}        */
      0x4801,                   /* ldr r0, [pc, #4] */
      0x9001,                   /* str r0, [sp, #4] */
      0xbd01,                   /* pop {r0, pc}     */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, -7
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (b_imm_t4_positive_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_B,
    { 0xf001, 0xb91a }, 2, 4,   /* b pc + 0x1234    */
    {
      0xb401,                   /* push {r0}        */
      0xb401,                   /* push {r0}        */
      0x4801,                   /* ldr r0, [pc, #4] */
      0x9001,                   /* str r0, [sp, #4] */
      0xbd01,                   /* pop {r0, pc}     */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, 0x1235
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (b_imm_t4_negative_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_B,
    { 0xf7fe, 0xbee6 }, 2, 4,   /* b pc - 0x1234    */
    {
      0xb401,                   /* push {r0}        */
      0xb401,                   /* push {r0}        */
      0x4801,                   /* ldr r0, [pc, #4] */
      0x9001,                   /* str r0, [sp, #4] */
      0xbd01,                   /* pop {r0, pc}     */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, -0x1233
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (bl_imm_t1_positive_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_BL,
    { 0xf001, 0xf91a }, 2, 4,   /* bl pc + 0x1234   */
    {
      0xb401,                   /* push {r0}        */
      0x4802,                   /* ldr r0, [pc, #8] */
      0x4686,                   /* mov lr, r0       */
      0xbc01,                   /* pop {r0}         */
      0x47f0,                   /* blx lr           */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, 0x1235
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (bl_imm_t1_negative_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_BL,
    { 0xf7fe, 0xfee6 }, 2, 4,   /* bl pc - 0x1234   */
    {
      0xb401,                   /* push {r0}        */
      0x4802,                   /* ldr r0, [pc, #8] */
      0x4686,                   /* mov lr, r0       */
      0xbc01,                   /* pop {r0}         */
      0x47f0,                   /* blx lr           */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, -0x1233
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (blx_imm_t2_positive_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_BLX,
    { 0xf001, 0xe91a }, 2, 4,   /* blx pc + 0x1234  */
    {
      0xb401,                   /* push {r0}        */
      0x4802,                   /* ldr r0, [pc, #8] */
      0x4686,                   /* mov lr, r0       */
      0xbc01,                   /* pop {r0}         */
      0x47f0,                   /* blx lr           */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, 0x1234
  };
  branch_scenario_execute (&bs, fixture);
}

TESTCASE (blx_imm_t2_negative_should_be_rewritten)
{
  BranchScenario bs = {
    ARM_INS_BLX,
    { 0xf7fe, 0xeee6 }, 2, 4,   /* blx pc - 0x1234  */
    {
      0xb401,                   /* push {r0}        */
      0x4802,                   /* ldr r0, [pc, #8] */
      0x4686,                   /* mov lr, r0       */
      0xbc01,                   /* pop {r0}         */
      0x47f0,                   /* blx lr           */
      0xbf00,                   /* <padding nop>    */
      0xffff,                   /* <calculated PC   */
      0xffff                    /*  goes here>      */
    }, 8,
    6, -0x1234
  };
  branch_scenario_execute (&bs, fixture);
}

static void
branch_scenario_execute (BranchScenario * bs,
                         TestThumbRelocatorFixture * fixture)
{
  gsize i;
  guint32 calculated_pc;
  const cs_insn * insn = NULL;

  for (i = 0; i != bs->input_length; i++)
    bs->input[i] = GUINT16_TO_LE (bs->input[i]);
  for (i = 0; i != bs->expected_output_length; i++)
    bs->expected_output[i] = GUINT16_TO_LE (bs->expected_output[i]);

  SETUP_RELOCATOR_WITH (bs->input);

  calculated_pc = fixture->rl.input_pc + 4 + bs->expected_pc_distance;
  memcpy (bs->expected_output + bs->pc_offset, &calculated_pc,
      sizeof (calculated_pc));

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn),
      ==, bs->instruction_length);
  g_assert_cmpint (insn->id, ==, bs->instruction_id);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (bs->input, bs->input_length, fixture->output,
      bs->expected_output, bs->expected_output_length * sizeof (guint16));
}

TESTCASE (cbz_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xb1e8),     /* cbz r0, #imm     */
    GUINT16_TO_LE (0xbd01)      /* pop {r0, pc}     */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xb100),     /* cbz r0, #imm     */
    /* if_false: jump to next instruction           */
    GUINT16_TO_LE (0xe004),     /* b pc + 8         */
    /* if_true:                                     */
    GUINT16_TO_LE (0xb401),     /* push {r0}        */
    GUINT16_TO_LE (0xb401),     /* push {r0}        */
    GUINT16_TO_LE (0x4801),     /* ldr r0, [pc, #4] */
    GUINT16_TO_LE (0x9001),     /* str r0, [sp, #4] */
    GUINT16_TO_LE (0xbd01),     /* pop {r0, pc}     */
    /* next instruction                             */
    GUINT16_TO_LE (0xbd01),     /* pop {r0, pc}     */
    GUINT16_TO_LE (0xffff),
    GUINT16_TO_LE (0xffff)
  };
  guint32 calculated_target;
  gchar expected_output[10 * sizeof (guint16)];
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_target = (fixture->rl.input_pc + 4 + ((0xe8 >> 3) << 1)) | 1;
  *((guint32 *) (expected_output + 16)) = calculated_target;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_CBZ);
  gum_thumb_relocator_read_one (&fixture->rl, &insn);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (cbnz_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xb912),     /* cbnz r2, #imm      */
    GUINT16_TO_LE (0xbd01)      /* pop {r0, pc}       */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xb902),     /* cbnz r2, #imm      */
    /* if_false:                                      */
    GUINT16_TO_LE (0xe004),     /* b next_instruction */
    /* if_true:                                       */
    GUINT16_TO_LE (0xb401),     /* push {r0}          */
    GUINT16_TO_LE (0xb401),     /* push {r0}          */
    GUINT16_TO_LE (0x4801),     /* ldr r0, [pc, #4]   */
    GUINT16_TO_LE (0x9001),     /* str r0, [sp, #4]   */
    GUINT16_TO_LE (0xbd01),     /* pop {r0, pc}       */
    /* next_instruction:                              */
    GUINT16_TO_LE (0xbd01),     /* pop {r0, pc}       */
    GUINT16_TO_LE (0xffff),
    GUINT16_TO_LE (0xffff)
  };
  guint32 calculated_target;
  gchar expected_output[10 * sizeof (guint16)];
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_target = (fixture->rl.input_pc + 4 + ((0x12 >> 3) << 1)) | 1;
  *((guint32 *) (expected_output + 16)) = calculated_target;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_CBNZ);
  gum_thumb_relocator_read_one (&fixture->rl, &insn);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (b_cond_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xd01b),     /* beq #imm           */
    GUINT16_TO_LE (0xbd01)      /* pop {r0, pc}       */
  };
  const guint16 expected_output_instructions[] = {
    GUINT16_TO_LE (0xd000),     /* beq #imm           */
    /* if_false:                                      */
    GUINT16_TO_LE (0xe004),     /* b next_instruction */
    /* if_true:                                       */
    GUINT16_TO_LE (0xb401),     /* push {r0}          */
    GUINT16_TO_LE (0xb401),     /* push {r0}          */
    GUINT16_TO_LE (0x4801),     /* ldr r0, [pc, #4]   */
    GUINT16_TO_LE (0x9001),     /* str r0, [sp, #4]   */
    GUINT16_TO_LE (0xbd01),     /* pop {r0, pc}       */
    /* next_instruction:                              */
    GUINT16_TO_LE (0xbd01),     /* pop {r0, pc}       */
    GUINT16_TO_LE (0xffff),
    GUINT16_TO_LE (0xffff)
  };
  guint32 calculated_target;
  gchar expected_output[10 * sizeof (guint16)];
  const cs_insn * insn = NULL;

  SETUP_RELOCATOR_WITH (input);

  memcpy (expected_output, expected_output_instructions,
      sizeof (expected_output_instructions));
  calculated_target = (fixture->rl.input_pc + 4 + (0x1b << 1)) | 1;

  *((guint32 *) (expected_output + 16)) = calculated_target;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_B);
  gum_thumb_relocator_read_one (&fixture->rl, &insn);
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  gum_thumb_writer_flush (&fixture->tw);
  check_output (input, sizeof (input), fixture->output,
      (guint16 *) expected_output, sizeof (expected_output));
}

TESTCASE (it_block_with_pc_relative_load_should_be_rewritten)
{
 const guint16 input[] = {
   GUINT16_TO_LE (0x2800),      /* cmp r0, #0         */
   GUINT16_TO_LE (0xbf06),      /* itte eq            */
   GUINT16_TO_LE (0x4801),      /* ldreq r0, [pc, #4] */
   GUINT16_TO_LE (0x3001),      /* addeq r0, #1       */
   GUINT16_TO_LE (0x3001),      /* addne r0, #1       */
 };
 const guint16 expected_output[] = {
   GUINT16_TO_LE (0x2800),      /* cmp r0, #0         */
   GUINT16_TO_LE (0xd001),      /* beq if_true        */
   /* if_false:                                       */
   GUINT16_TO_LE (0x3001),      /* adds r0, #1        */
   GUINT16_TO_LE (0xe002),      /* b next_instruction */
   /* if_true:                                        */
   GUINT16_TO_LE (0x4800),      /* ldr r0, [pc, #0]   */
   GUINT16_TO_LE (0x6800),      /* ldr r0, [r0, #0]   */
   GUINT16_TO_LE (0x3001),      /* adds r0, #1        */
   /* next_instruction:                               */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);
  insn = NULL;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 10);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_false (gum_thumb_relocator_write_one (&fixture->rl));

  check_output (input, sizeof (input), fixture->output, expected_output,
      sizeof (expected_output));
}

TESTCASE (it_block_with_b_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0xb580),                         /* push {r7, lr}       */
    GUINT16_TO_LE (0x2801),                         /* cmp r0, #1          */
    GUINT16_TO_LE (0xbf0a),                         /* itet eq             */
    GUINT16_TO_LE (0xf101), GUINT16_TO_LE (0x37ff), /* addeq.w r7, r1, #-1 */
    GUINT16_TO_LE (0x1c4f),                         /* addne r7, r1, #1    */
    GUINT16_TO_LE (0xf7ff), GUINT16_TO_LE (0xef08), /* blxeq.w xxxx        */
  };
  const guint16 expected_output[] = {
    GUINT16_TO_LE (0xb580),                         /* push {r7, lr}       */
    GUINT16_TO_LE (0x2801),                         /* cmp r0, #1          */
    GUINT16_TO_LE (0xd001),                         /* beq if_true         */
    /* if_false:                                                           */
    GUINT16_TO_LE (0x1c4f),                         /* adds r7, r1, #1     */
    GUINT16_TO_LE (0xe006),                         /* b next_instruction  */
    /* if_true:                                                            */
    GUINT16_TO_LE (0xf101), GUINT16_TO_LE (0x37ff), /* add.w r7, r1, #-1   */
    GUINT16_TO_LE (0xb401),                         /* push {r0}           */
    GUINT16_TO_LE (0x4800),                         /* ldr r0, [pc, #0]    */
    GUINT16_TO_LE (0x4686),                         /* mov lr, r0          */
    GUINT16_TO_LE (0xbc01),                         /* pop {r0}            */
    GUINT16_TO_LE (0x47f0),                         /* blx lr              */
    /* next_instruction:                                                   */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);
  insn = NULL;

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 4);
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 16);
  assert_outbuf_still_zeroed_from_offset (0);

  gum_thumb_relocator_write_all (&fixture->rl);

  check_output (input, sizeof (input), fixture->output, expected_output,
      sizeof (expected_output));
}

TESTCASE (it_block_should_be_rewritten_as_a_whole)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0x2800), /* cmp r0, #0         */
    GUINT16_TO_LE (0xbf1c), /* itt ne             */
    GUINT16_TO_LE (0x6800), /* ldrne r0, [r0]     */
    GUINT16_TO_LE (0x2800)  /* cmpne r0, #0       */
  };
  const guint16 expected_output[] = {
    GUINT16_TO_LE (0x2800), /* cmp r0, #0         */
    GUINT16_TO_LE (0xd100), /* bne if_true        */
    /* if_false:                                  */
    GUINT16_TO_LE (0xe001), /* b next_instruction */
    /* if_true:                                   */
    GUINT16_TO_LE (0x6800), /* ldr r0, [r0, #0]   */
    GUINT16_TO_LE (0x2800), /* cmp r0, #0         */
    /* next_instruction:                          */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_CMP);
  assert_outbuf_still_zeroed_from_offset (0);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 8);
  g_assert_cmpint (insn->id, ==, ARM_INS_IT);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_false (gum_thumb_relocator_write_one (&fixture->rl));

  check_output (input, sizeof (input), fixture->output, expected_output,
      sizeof (expected_output));
}

TESTCASE (it_block_with_eoi_insn_should_be_rewritten)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0x2800),                         /* cmp r0, #0         */
    GUINT16_TO_LE (0xbf18),                         /* it ne              */
    GUINT16_TO_LE (0xe8bd), GUINT16_TO_LE (0x8010), /* pop.w {r4, pc}     */
    GUINT16_TO_LE (0x3001),                         /* adds r0, #1        */
  };
  const guint16 expected_output[] = {
    GUINT16_TO_LE (0x2800),                         /* cmp r0, #0         */
    GUINT16_TO_LE (0xd100),                         /* bne if_true        */
    /* if_false:                                                          */
    GUINT16_TO_LE (0xe001),                         /* b next_instruction */
    /* if_true:                                                           */
    GUINT16_TO_LE (0xe8bd), GUINT16_TO_LE (0x8010), /* pop.w {r4, pc}     */
    /* next_instruction:                                                  */
    GUINT16_TO_LE (0x3001),                         /* adds r0, #1        */
  };
  const cs_insn * insn;

  SETUP_RELOCATOR_WITH (input);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 2);
  g_assert_cmpint (insn->id, ==, ARM_INS_CMP);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_false (fixture->rl.eob);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 8);
  g_assert_cmpint (insn->id, ==, ARM_INS_IT);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_true (fixture->rl.eob);

  insn = NULL;
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, &insn), ==, 10);
  g_assert_cmpint (insn->id, ==, ARM_INS_ADD);
  assert_outbuf_still_zeroed_from_offset (0);

  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_true (gum_thumb_relocator_write_one (&fixture->rl));
  g_assert_false (gum_thumb_relocator_write_one (&fixture->rl));

  check_output (input, sizeof (input), fixture->output, expected_output,
      sizeof (expected_output));
}

TESTCASE (eob_and_eoi_on_ret)
{
  const guint16 input[] = {
    GUINT16_TO_LE (0x4770)  /* bx lr */
  };

  SETUP_RELOCATOR_WITH (input);

  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, NULL), ==, 2);
  g_assert_true (gum_thumb_relocator_eob (&fixture->rl));
  g_assert_true (gum_thumb_relocator_eoi (&fixture->rl));
  g_assert_cmpuint (gum_thumb_relocator_read_one (&fixture->rl, NULL), ==, 0);
}
