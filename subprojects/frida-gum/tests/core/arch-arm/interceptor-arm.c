/*
 * Copyright (C) 2016-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "interceptor-arm-fixture.c"

TESTLIST_BEGIN (interceptor_arm)
#ifndef HAVE_IOS
  TESTENTRY (attach_to_unaligned_function)
#endif
  TESTENTRY (attach_to_thumb_thunk_reading_lr)
  TESTENTRY (attach_to_thumb_function_reading_lr)
TESTLIST_END ()

#ifndef HAVE_IOS

/*
 * XXX: Although this problem also applies to iOS we don't want to run this
 *      test there until we have an easy JIT API for hiding the annoying
 *      details necessary to deal with code-signing.
 */

#include "gumthumbwriter.h"

TESTCASE (attach_to_unaligned_function)
{
  gpointer page, code;
  gsize page_size;
  GumThumbWriter tw;
  gint (* f) (void);

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RWX);
  code = page + 2;

  /* Aligned on a 2 byte boundary and minimum 8 bytes long */
  gum_thumb_writer_init (&tw, code);
  gum_thumb_writer_put_push_regs (&tw, 8,
      ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_LR);
  gum_thumb_writer_put_push_regs (&tw, 5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);
  gum_thumb_writer_put_pop_regs (&tw, 5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);
  gum_thumb_writer_put_ldr_reg_u32 (&tw, ARM_REG_R0, 1337);
  gum_thumb_writer_put_pop_regs (&tw, 8,
      ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_PC);
  gum_thumb_writer_flush (&tw);
  gum_clear_cache (tw.base, gum_thumb_writer_offset (&tw));
  gum_thumb_writer_clear (&tw);

  f = code + 1;

  interceptor_fixture_attach (fixture, 0, f, '>', '<');
  g_assert_cmpint (f (), ==, 1337);
  g_assert_cmpstr (fixture->result->str, ==, "><");

  g_string_truncate (fixture->result, 0);
  interceptor_fixture_detach (fixture, 0);
  g_assert_cmpint (f (), ==, 1337);
  g_assert_cmpstr (fixture->result->str, ==, "");

  gum_memory_free (page, page_size);
}

#endif

typedef struct _GumEmitLrThunkContext GumEmitLrThunkContext;
typedef struct _GumEmitLrFuncContext GumEmitLrFuncContext;

struct _GumEmitLrThunkContext
{
  gpointer code;
  gsize (* run) (void);
  gsize (* thunk) (void);
  gsize expected_lr;
};

struct _GumEmitLrFuncContext
{
  gpointer code;
  gsize (* run) (void);
  gsize (* func) (void);
  gsize caller_lr;
};

static void gum_emit_lr_thunk (gpointer mem, gpointer user_data);
static void gum_emit_lr_func (gpointer mem, gpointer user_data);

TESTCASE (attach_to_thumb_thunk_reading_lr)
{
  GumAddressSpec spec;
  gsize page_size, code_size;
  GumEmitLrThunkContext ctx;

  spec.near_address = GSIZE_TO_POINTER (
      gum_module_get_range (gum_process_get_main_module ())->base_address);
  spec.max_distance = GUM_THUMB_B_MAX_DISTANCE - 4096;

  page_size = gum_query_page_size ();
  code_size = page_size;

  ctx.code = gum_memory_allocate_near (&spec, code_size, page_size,
      GUM_PAGE_RW);
  ctx.run = NULL;
  ctx.thunk = NULL;
  ctx.expected_lr = 0;

  gum_memory_patch_code (ctx.code, code_size, gum_emit_lr_thunk, &ctx);

  g_assert_cmphex (ctx.run (), ==, ctx.expected_lr);

  interceptor_fixture_attach (fixture, 0, ctx.thunk, '>', '<');
  g_assert_cmphex (ctx.run (), ==, ctx.expected_lr);
  g_assert_cmpstr (fixture->result->str, ==, "><");

  interceptor_fixture_detach (fixture, 0);
  gum_memory_free (ctx.code, code_size);
}

static void
gum_emit_lr_thunk (gpointer mem,
                   gpointer user_data)
{
  GumEmitLrThunkContext * ctx = user_data;
  GumThumbWriter tw;
  const gchar * thunk_start = "thunk_start";
  const gchar * inner_start = "inner_start";

  gum_thumb_writer_init (&tw, mem);
  tw.pc = GUM_ADDRESS (ctx->code);

  ctx->run = GSIZE_TO_POINTER (tw.pc | 1);
  gum_thumb_writer_put_push_regs (&tw, 1, ARM_REG_LR);
  gum_thumb_writer_put_bl_label (&tw, thunk_start);
  ctx->expected_lr = tw.pc | 1;
  gum_thumb_writer_put_pop_regs (&tw, 1, ARM_REG_PC);

  ctx->thunk = GSIZE_TO_POINTER (tw.pc | 1);
  gum_thumb_writer_put_label (&tw, thunk_start);
  gum_thumb_writer_put_mov_reg_reg (&tw, ARM_REG_R3, ARM_REG_LR);
  gum_thumb_writer_put_b_label (&tw, inner_start);

  gum_thumb_writer_put_label (&tw, inner_start);
  gum_thumb_writer_put_mov_reg_reg (&tw, ARM_REG_R0, ARM_REG_R3);
  gum_thumb_writer_put_bx_reg (&tw, ARM_REG_LR);

  gum_thumb_writer_clear (&tw);
}

TESTCASE (attach_to_thumb_function_reading_lr)
{
  GumAddressSpec spec;
  gsize page_size, code_size;
  GumEmitLrFuncContext ctx;

  spec.near_address = GSIZE_TO_POINTER (
      gum_module_get_range (gum_process_get_main_module ())->base_address);
  spec.max_distance = GUM_THUMB_B_MAX_DISTANCE - 4096;

  page_size = gum_query_page_size ();
  code_size = page_size;

  ctx.code = gum_memory_allocate_near (&spec, code_size, page_size,
      GUM_PAGE_RW);
  ctx.run = NULL;
  ctx.func = NULL;
  ctx.caller_lr = 0;

  gum_memory_patch_code (ctx.code, code_size, gum_emit_lr_func, &ctx);

  g_assert_cmphex (ctx.run (), ==, ctx.caller_lr);

  interceptor_fixture_attach (fixture, 0, ctx.func, '>', '<');
  g_assert_cmphex (ctx.run (), !=, ctx.caller_lr);
  g_assert_cmpstr (fixture->result->str, ==, "><");

  interceptor_fixture_detach (fixture, 0);
  gum_memory_free (ctx.code, code_size);
}

static void
gum_emit_lr_func (gpointer mem,
                  gpointer user_data)
{
  GumEmitLrFuncContext * ctx = user_data;
  GumThumbWriter tw;
  const gchar * func_start = "func_start";

  gum_thumb_writer_init (&tw, mem);
  tw.pc = GUM_ADDRESS (ctx->code);

  ctx->run = GSIZE_TO_POINTER (tw.pc | 1);
  gum_thumb_writer_put_push_regs (&tw, 1, ARM_REG_LR);
  gum_thumb_writer_put_bl_label (&tw, func_start);
  ctx->caller_lr = tw.pc | 1;
  gum_thumb_writer_put_pop_regs (&tw, 1, ARM_REG_PC);

  ctx->func = GSIZE_TO_POINTER (tw.pc | 1);
  gum_thumb_writer_put_label (&tw, func_start);
  gum_thumb_writer_put_push_regs (&tw, 1, ARM_REG_LR);
  gum_thumb_writer_put_nop (&tw);
  gum_thumb_writer_put_nop (&tw);
  gum_thumb_writer_put_mov_reg_reg (&tw, ARM_REG_R0, ARM_REG_LR);
  gum_thumb_writer_put_pop_regs (&tw, 1, ARM_REG_PC);

  gum_thumb_writer_clear (&tw);
}
