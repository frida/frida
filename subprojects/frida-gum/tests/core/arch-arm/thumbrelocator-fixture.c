/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthumbrelocator.h"

#include "gummemory.h"
#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void test_thumb_relocator_ ## NAME ( \
        TestThumbRelocatorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/ThumbRelocator", test_thumb_relocator, \
        NAME, TestThumbRelocatorFixture)

#define TEST_OUTBUF_SIZE 32

typedef struct _TestThumbRelocatorFixture
{
  guint8 * output;
  gsize output_size;
  GumThumbWriter tw;
  GumThumbRelocator rl;
} TestThumbRelocatorFixture;

static void show_disassembly (const guint16 * input, gsize length);

static void
test_thumb_relocator_fixture_setup (TestThumbRelocatorFixture * fixture,
                                    gconstpointer data)
{
  gsize page_size;

  page_size = gum_query_page_size ();
  fixture->output_size = page_size;
  fixture->output = (guint8 *) gum_memory_allocate (NULL, fixture->output_size,
      page_size, GUM_PAGE_RW);

  gum_thumb_writer_init (&fixture->tw, fixture->output);
  fixture->tw.pc = 1024;
}

static void
test_thumb_relocator_fixture_teardown (TestThumbRelocatorFixture * fixture,
                                       gconstpointer data)
{
  gum_thumb_relocator_clear (&fixture->rl);
  gum_thumb_writer_clear (&fixture->tw);
  gum_memory_free (fixture->output, fixture->output_size);
}

static void
check_output (const guint16 * input,
              gsize input_length,
              const guint8 * output,
              const guint16 * expected_output,
              gsize expected_length)
{
  gboolean same_content;
  gchar * diff;

  same_content = memcmp (output, expected_output, expected_length) == 0;

  diff = test_util_diff_binary ((guint8 *) expected_output, expected_length,
      output, expected_length);

  if (!same_content)
  {
    g_print ("\n\nGenerated code is not equal to expected code:\n\n%s\n", diff);

    g_print ("\n\nInput:\n\n");
    show_disassembly (input, input_length);

    g_print ("\n\nExpected:\n\n");
    show_disassembly (expected_output, expected_length);

    g_print ("\n\nWrong:\n\n");
    show_disassembly ((guint16 *) output, expected_length);
  }

  g_assert_true (same_content);
}

static void
show_disassembly (const guint16 * input,
                  gsize length)
{
  csh capstone;
  cs_insn * insn;
  const uint8_t * code;
  size_t size;
  uint64_t address;

  cs_open (CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_V8, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);
  insn = cs_malloc (capstone);

  code = (const uint8_t *) input;
  size = length;
  address = GPOINTER_TO_SIZE (input);

  while (cs_disasm_iter (capstone, &code, &size, &address, insn))
  {
    guint16 raw_insn;

    memcpy (&raw_insn, insn->bytes, sizeof (raw_insn));

    g_print ("0x%" G_GINT64_MODIFIER "x\t0x%04x,               /* %s %s */\n",
        insn->address, raw_insn, insn->mnemonic, insn->op_str);
  }

  cs_free (insn, 1);
  cs_close (&capstone);
}

static const guint8 cleared_outbuf[TEST_OUTBUF_SIZE] = { 0, };

#define SETUP_RELOCATOR_WITH(CODE) \
    gum_thumb_relocator_init (&fixture->rl, CODE, &fixture->tw); \
    fixture->rl.input_pc = 2048

#define assert_outbuf_still_zeroed_from_offset(OFF) \
    g_assert_cmpint (memcmp (fixture->output + OFF, cleared_outbuf + OFF, \
        sizeof (cleared_outbuf) - OFF), ==, 0)
