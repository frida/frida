/*
 * Copyright (C) 2019-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "interceptor-arm64-fixture.c"

TESTLIST_BEGIN (interceptor_arm64)
  TESTENTRY (attach_to_thunk_reading_lr)
  TESTENTRY (attach_to_function_reading_lr)
  TESTENTRY (attach_with_custom_scratch_register)
  TESTENTRY (attach_rejects_scratch_register_used_by_prologue)
TESTLIST_END ()

typedef struct _GumEmitLrThunkContext GumEmitLrThunkContext;
typedef struct _GumEmitLrFuncContext GumEmitLrFuncContext;
typedef struct _GumEmitConstFuncContext GumEmitConstFuncContext;

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

struct _GumEmitConstFuncContext
{
  gpointer code;
  gsize (* run) (void);
  gboolean clobber_x7;
};

static void gum_emit_lr_thunk (gpointer mem, gpointer user_data);
static void gum_emit_lr_func (gpointer mem, gpointer user_data);
static void gum_emit_const_func (gpointer mem, gpointer user_data);

TESTCASE (attach_to_thunk_reading_lr)
{
  const gsize code_size_in_pages = 1;
  gsize page_size, code_size;
  GumEmitLrThunkContext ctx;

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  ctx.code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
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
  GumArm64Writer aw;
  const gchar * thunk_start = "thunk_start";
  const gchar * inner_start = "inner_start";

  gum_arm64_writer_init (&aw, mem);
  aw.pc = GUM_ADDRESS (ctx->code);

  ctx->run = gum_sign_code_pointer (GSIZE_TO_POINTER (aw.pc));
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_bl_label (&aw, thunk_start);
  ctx->expected_lr = aw.pc;
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&aw);

  ctx->thunk = GSIZE_TO_POINTER (aw.pc);
  gum_arm64_writer_put_label (&aw, thunk_start);
  gum_arm64_writer_put_mov_reg_reg (&aw, ARM64_REG_X3, ARM64_REG_LR);
  gum_arm64_writer_put_b_label (&aw, inner_start);

  gum_arm64_writer_put_label (&aw, inner_start);
  gum_arm64_writer_put_mov_reg_reg (&aw, ARM64_REG_X0, ARM64_REG_X3);
  gum_arm64_writer_put_ret (&aw);

  gum_arm64_writer_clear (&aw);
}

TESTCASE (attach_to_function_reading_lr)
{
  const gsize code_size_in_pages = 1;
  gsize page_size, code_size;
  GumEmitLrFuncContext ctx;

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  ctx.code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
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
  GumArm64Writer aw;
  const gchar * func_start = "func_start";

  gum_arm64_writer_init (&aw, mem);
  aw.pc = GUM_ADDRESS (ctx->code);

  ctx->run = gum_sign_code_pointer (GSIZE_TO_POINTER (aw.pc));
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_bl_label (&aw, func_start);
  ctx->caller_lr = aw.pc;
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&aw);

  ctx->func = GSIZE_TO_POINTER (aw.pc);
  gum_arm64_writer_put_label (&aw, func_start);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_nop (&aw);
  gum_arm64_writer_put_nop (&aw);
  gum_arm64_writer_put_mov_reg_reg (&aw, ARM64_REG_X0, ARM64_REG_LR);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_ret (&aw);

  gum_arm64_writer_clear (&aw);
}

TESTCASE (attach_with_custom_scratch_register)
{
  const gsize code_size_in_pages = 1;
  gsize page_size, code_size;
  GumEmitConstFuncContext ctx;
  Arm64ListenerContext * lc;
  GumAttachOptions options = { 0, };

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  ctx.code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
  ctx.run = NULL;
  ctx.clobber_x7 = FALSE;

  gum_memory_patch_code (ctx.code, code_size, gum_emit_const_func, &ctx);

  g_assert_cmphex (ctx.run (), ==, 1337);

  lc = g_slice_new0 (Arm64ListenerContext);
  lc->listener = test_callback_listener_new ();
  lc->listener->on_enter =
      (TestCallbackListenerFunc) arm64_listener_context_on_enter;
  lc->listener->on_leave =
      (TestCallbackListenerFunc) arm64_listener_context_on_leave;
  lc->listener->user_data = lc;
  lc->fixture = fixture;
  lc->enter_char = '>';
  lc->leave_char = '<';

  options.instrumentation.scratch_register = ARM64_REG_X7;
  g_assert_cmpint (gum_interceptor_attach (fixture->interceptor, ctx.run,
      GUM_INVOCATION_LISTENER (lc->listener), &options), ==, GUM_ATTACH_OK);

  g_assert_cmphex (ctx.run (), ==, 1337);
  g_assert_cmpstr (fixture->result->str, ==, "><");

  gum_interceptor_detach (fixture->interceptor,
      GUM_INVOCATION_LISTENER (lc->listener));
  arm64_listener_context_free (lc);
  gum_memory_free (ctx.code, code_size);
}

TESTCASE (attach_rejects_scratch_register_used_by_prologue)
{
  const gsize code_size_in_pages = 1;
  gsize page_size, code_size;
  GumEmitConstFuncContext ctx;
  TestCallbackListener * listener;
  GumAttachOptions options = { 0, };

  page_size = gum_query_page_size ();
  code_size = code_size_in_pages * page_size;
  ctx.code = gum_memory_allocate (NULL, code_size, page_size, GUM_PAGE_RW);
  ctx.run = NULL;
  ctx.clobber_x7 = TRUE;

  gum_memory_patch_code (ctx.code, code_size, gum_emit_const_func, &ctx);

  listener = test_callback_listener_new ();

  options.instrumentation.scratch_register = ARM64_REG_X7;
  g_assert_cmpint (gum_interceptor_attach (fixture->interceptor, ctx.run,
      GUM_INVOCATION_LISTENER (listener), &options), ==,
      GUM_ATTACH_WRONG_SIGNATURE);

  g_object_unref (listener);
  gum_memory_free (ctx.code, code_size);
}

static void
gum_emit_const_func (gpointer mem,
                     gpointer user_data)
{
  GumEmitConstFuncContext * ctx = user_data;
  GumArm64Writer aw;

  gum_arm64_writer_init (&aw, mem);
  aw.pc = GUM_ADDRESS (ctx->code);

  ctx->run = gum_sign_code_pointer (GSIZE_TO_POINTER (aw.pc));
  if (ctx->clobber_x7)
    gum_arm64_writer_put_mov_reg_reg (&aw, ARM64_REG_X7, ARM64_REG_X7);
  gum_arm64_writer_put_ldr_reg_u64 (&aw, ARM64_REG_X0, 1337);
  gum_arm64_writer_put_nop (&aw);
  gum_arm64_writer_put_nop (&aw);
  gum_arm64_writer_put_nop (&aw);
  gum_arm64_writer_put_ret (&aw);

  gum_arm64_writer_clear (&aw);
}
