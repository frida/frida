/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2017 Antonio Ken Iannillo <ak.iannillo@gmail.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "stalker-arm64-fixture.c"

#include <lzma.h>
#ifdef HAVE_LINUX
# include <errno.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/prctl.h>
# include <sys/wait.h>
#endif

TESTLIST_BEGIN (stalker)

#if defined (HAVE_LINUX) && !defined (HAVE_ANDROID)
  TESTGROUP_BEGIN ("ExceptionHandling")
    TESTENTRY (no_exceptions)
    TESTENTRY (try_and_catch)
    TESTENTRY (try_and_catch_excluded)
    TESTENTRY (try_and_dont_catch)
    TESTENTRY (try_and_dont_catch_excluded)
  TESTGROUP_END ()
#endif

  /* EVENTS */
  TESTENTRY (no_events)
  TESTENTRY (call)
  TESTENTRY (ret)
  TESTENTRY (exec)
  TESTENTRY (call_depth)

  /* PROBES */
  TESTENTRY (call_probe)

  /* TRANSFORMERS */
  TESTENTRY (custom_transformer)
  TESTENTRY (transformer_should_be_able_to_skip_call)
  TESTENTRY (transformer_should_be_able_to_replace_call_with_callout)
  TESTENTRY (transformer_should_be_able_to_replace_tailjump_with_callout)
  TESTENTRY (many_callouts_should_not_overflow_literal_pool)
  TESTENTRY (unfollow_should_be_allowed_before_first_transform)
  TESTENTRY (unfollow_should_be_allowed_mid_first_transform)
  TESTENTRY (unfollow_should_be_allowed_after_first_transform)
  TESTENTRY (unfollow_should_be_allowed_before_second_transform)
  TESTENTRY (unfollow_should_be_allowed_mid_second_transform)
  TESTENTRY (unfollow_should_be_allowed_after_second_transform)
  TESTENTRY (follow_me_should_support_nullable_event_sink)
  TESTENTRY (invalidation_for_current_thread_should_be_supported)
  TESTENTRY (invalidation_for_specific_thread_should_be_supported)
  TESTENTRY (invalidation_should_allow_block_to_grow)

  /* EXCLUSION */
  TESTENTRY (exclude_bl)
  TESTENTRY (exclude_blr)
  TESTENTRY (exclude_bl_with_unfollow)
  TESTENTRY (exclude_blr_with_unfollow)

  /* BRANCH */
  TESTENTRY (unconditional_branch)
  TESTENTRY (unconditional_branch_reg)
  TESTENTRY (conditional_branch)
  TESTENTRY (compare_and_branch)
  TESTENTRY (test_bit_and_branch)

  /* FOLLOWS */
  TESTENTRY (follow_std_call)
  TESTENTRY (follow_return)
  TESTENTRY (follow_misaligned_stack)
  TESTENTRY (follow_syscall)
  TESTENTRY (follow_thread)
  TESTENTRY (unfollow_should_handle_terminated_thread)
  TESTENTRY (self_modifying_code_should_be_detected_with_threshold_minus_one)
  TESTENTRY (self_modifying_code_should_not_be_detected_with_threshold_zero)
  TESTENTRY (self_modifying_code_should_be_detected_with_threshold_one)

  /* EXCLUSIVE LOADS/STORES */
  TESTENTRY (exclusive_load_store_should_not_be_disturbed)

  /* EXTRA */
#ifndef HAVE_WINDOWS
  TESTENTRY (pthread_create)
#endif
  TESTENTRY (heap_api)
  TESTENTRY (no_register_clobber)
  TESTENTRY (performance)

#ifdef HAVE_LINUX
  TESTENTRY (prefetch)
  TESTENTRY (observer)
#endif

  TESTGROUP_BEGIN ("RunOnThread")
    TESTENTRY (run_on_thread_current)
    TESTENTRY (run_on_thread_current_sync)
    TESTENTRY (run_on_thread_other)
    TESTENTRY (run_on_thread_other_sync)
  TESTGROUP_END ()
TESTLIST_END ()

#ifdef HAVE_LINUX

struct _GumTestStalkerObserver
{
  GObject parent;

  guint64 total;
};

#endif

typedef struct _CodePatcher CodePatcher;
typedef struct _RunOnThreadCtx RunOnThreadCtx;
typedef struct _TestThreadSyncData TestThreadSyncData;

struct _CodePatcher
{
  GThread * thread;
  volatile gint request;
  volatile gint done;
  volatile gboolean stop;
  gpointer target;
  guint32 insn;
};

struct _RunOnThreadCtx
{
  GumThreadId caller_id;
  GumThreadId thread_id;
};

struct _TestThreadSyncData
{
  GMutex mutex;
  GCond cond;
  gboolean started;
  GumThreadId thread_id;
  gboolean * done;
};

static void insert_extra_add_after_sub (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void store_x0 (GumCpuContext * cpu_context, gpointer user_data);
static void skip_call (GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void replace_call_with_callout (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void replace_jmp_with_callout (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void callout_set_cool (GumCpuContext * cpu_context, gpointer user_data);
static void add_callout_for_every_instruction (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void bump_num_callouts (GumCpuContext * cpu_context,
    gpointer user_data);
static void unfollow_during_transform (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static gboolean test_is_finished (void);
static void modify_to_return_true_after_three_calls (
    GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void invalidate_after_three_calls (GumCpuContext * cpu_context,
    gpointer user_data);
static void start_invalidation_target (InvalidationTarget * target,
    TestArm64StalkerFixture * fixture);
static void join_invalidation_target (InvalidationTarget * target);
static gpointer run_stalked_until_finished (gpointer data);
static void modify_to_return_true_on_subsequent_transform (
    GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static int get_magic_number (void);
static void add_n_return_value_increments (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static gpointer run_stalked_briefly (gpointer data);
static gpointer run_stalked_into_termination (gpointer data);
static void insert_callout_after_cmp (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void bump_num_cmp_callouts (GumCpuContext * cpu_context,
    gpointer user_data);
static void code_patcher_start (CodePatcher * self);
static void code_patcher_stop (CodePatcher * self);
static void patch_instruction (CodePatcher * patcher, gpointer code,
    guint offset, guint32 insn);
static gpointer code_patcher_worker (gpointer data);
static void do_patch_instruction (gpointer mem, gpointer user_data);
#ifndef HAVE_WINDOWS
static gpointer increment_integer (gpointer data);
#endif
static void pretend_workload (const GumMemoryRange * runner_range);

volatile gboolean stalker_invalidation_test_is_finished = FALSE;
volatile gint stalker_invalidation_magic_number = 42;

#ifdef HAVE_LINUX
static void prefetch_on_event (const GumEvent * event,
    GumCpuContext * cpu_context, gpointer user_data);
static void prefetch_run_child (GumStalker * stalker,
    const GumMemoryRange * runner_range, int compile_fd, int execute_fd);
static void prefetch_activation_target (void);
static void prefetch_write_blocks (int fd, GHashTable * table);
static void prefetch_read_blocks (int fd, GHashTable * table);

static GHashTable * prefetch_compiled = NULL;
static GHashTable * prefetch_executed = NULL;

#define GUM_TYPE_TEST_STALKER_OBSERVER (gum_test_stalker_observer_get_type ())
G_DECLARE_FINAL_TYPE (GumTestStalkerObserver, gum_test_stalker_observer, GUM,
                      TEST_STALKER_OBSERVER, GObject)

static void gum_test_stalker_observer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_test_stalker_observer_class_init (
    GumTestStalkerObserverClass * klass);
static void gum_test_stalker_observer_init (GumTestStalkerObserver * self);
static void gum_test_stalker_observer_increment_total (
    GumStalkerObserver * observer);

G_DEFINE_TYPE_EXTENDED (GumTestStalkerObserver,
                        gum_test_stalker_observer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_STALKER_OBSERVER,
                            gum_test_stalker_observer_iface_init))
#endif

static void run_on_thread (const GumCpuContext * cpu_context,
    gpointer user_data);
static GThread * create_sleeping_dummy_thread_sync (gboolean * done,
    GumThreadId * thread_id);
static gpointer sleeping_dummy (gpointer data);

static const guint32 flat_code[] = {
    GUINT32_TO_LE (0xcb000000), /* sub w0, w0, w0 */
    GUINT32_TO_LE (0x91000400), /* add w0, w0, #1 */
    GUINT32_TO_LE (0x91000400), /* add w0, w0, #1 */
    GUINT32_TO_LE (0xd65f03c0)  /* ret            */
};

static StalkerTestFunc
invoke_flat_expecting_return_value (TestArm64StalkerFixture * fixture,
                                    GumEventType mask,
                                    guint expected_return_value)
{
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      flat_code, sizeof (flat_code));

  fixture->sink->mask = mask;
  ret = test_arm64_stalker_fixture_follow_and_invoke (fixture, func, -1);
  g_assert_cmpint (ret, ==, expected_return_value);

  return func;
}

static StalkerTestFunc
invoke_flat (TestArm64StalkerFixture * fixture,
             GumEventType mask)
{
  return invoke_flat_expecting_return_value (fixture, mask, 2);
}

TESTCASE (no_events)
{
  invoke_flat (fixture, GUM_NOTHING);
  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCASE (call)
{
  StalkerTestFunc func;
  GumCallEvent * ev;

  func = invoke_flat (fixture, GUM_CALL);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);
  g_assert_cmpint (g_array_index (fixture->sink->events, GumEvent,
      0).type, ==, GUM_CALL);
  ev = &g_array_index (fixture->sink->events, GumEvent, 0).call;
  GUM_ASSERT_CMPADDR (ev->location, ==, fixture->last_invoke_calladdr);
  GUM_ASSERT_CMPADDR (ev->target, ==, gum_strip_code_pointer (func));
}

TESTCASE (ret)
{
  StalkerTestFunc func;
  GumRetEvent * ev;

  func = invoke_flat (fixture, GUM_RET);

  g_assert_cmpuint (fixture->sink->events->length, ==, 1);
  g_assert_cmpint (g_array_index (fixture->sink->events, GumEvent,
      0).type, ==, GUM_RET);

  ev = &g_array_index (fixture->sink->events, GumEvent, 0).ret;

  GUM_ASSERT_CMPADDR (ev->location, ==,
      (guint8 *) gum_strip_code_pointer (func) + 3 * 4);
  GUM_ASSERT_CMPADDR (ev->target, ==, fixture->last_invoke_retaddr);
}

TESTCASE (exec)
{
  StalkerTestFunc func;
  GumRetEvent * ev;

  func = invoke_flat (fixture, GUM_EXEC);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 4);
  g_assert_cmpint (g_array_index (fixture->sink->events, GumEvent,
      INVOKER_IMPL_OFFSET).type, ==, GUM_EXEC);
  ev =
      &g_array_index (fixture->sink->events, GumEvent, INVOKER_IMPL_OFFSET).ret;
  GUM_ASSERT_CMPADDR (ev->location, ==, gum_strip_code_pointer (func));
}

TESTCASE (call_depth)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  gpointer func_a, func_b;
  const gchar * start_lbl = "start";
  StalkerTestFunc func;
  gint r;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_b_label (&cw, start_lbl);

  func_b = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 7);
  gum_arm64_writer_put_ret (&cw);

  func_a = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 3);
  gum_arm64_writer_put_bl_imm (&cw, GUM_ADDRESS (func_b));
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, start_lbl);
  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_bl_imm (&cw, GUM_ADDRESS (func_a));
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));
  r = func (2);

  g_assert_cmpint (r, ==, 12);
  g_assert_cmpuint (fixture->sink->events->length, ==, 5);
  g_assert_cmpint (NTH_EVENT_AS_CALL (0)->depth, ==, 0);
  g_assert_cmpint (NTH_EVENT_AS_CALL (1)->depth, ==, 1);
  g_assert_cmpint (NTH_EVENT_AS_RET (2)->depth, ==, 2);
  g_assert_cmpint (NTH_EVENT_AS_RET (3)->depth, ==, 1);

  gum_memory_free (code, page_size);
}

typedef struct _CallProbeContext CallProbeContext;

struct _CallProbeContext
{
  guint num_calls;
  gpointer target_address;
  gpointer return_address;
};

static void probe_func_a_invocation (GumCallDetails * details,
    gpointer user_data);

TESTCASE (call_probe)
{
  const guint32 code_template[] =
  {
    GUINT32_TO_LE (0xa9bf7bf3), /* push {x19, lr} */
    GUINT32_TO_LE (0xd2801553), /* mov x19, #0xaa */
    GUINT32_TO_LE (0xd2800883), /* mov x3, #0x44  */
    GUINT32_TO_LE (0xd2800662), /* mov x2, #0x33  */
    GUINT32_TO_LE (0xd2800441), /* mov x1, #0x22  */
    GUINT32_TO_LE (0xd2800220), /* mov x0, #0x11  */
    GUINT32_TO_LE (0xa9bf07e0), /* push {x0, x1}  */
    GUINT32_TO_LE (0x94000009), /* bl func_a      */
    GUINT32_TO_LE (0xa8c107e0), /* pop {x0, x1}   */
    GUINT32_TO_LE (0xd2801103), /* mov x3, #0x88  */
    GUINT32_TO_LE (0xd2800ee2), /* mov x2, #0x77  */
    GUINT32_TO_LE (0xd2800cc1), /* mov x1, #0x66  */
    GUINT32_TO_LE (0xd2800aa0), /* mov x0, #0x55  */
    GUINT32_TO_LE (0x94000005), /* bl func_b      */
    GUINT32_TO_LE (0xa8c17bf3), /* pop {x19, lr}  */
    GUINT32_TO_LE (0xd65f03c0), /* ret            */

    /* func_a: */
    GUINT32_TO_LE (0xd2801100), /* mov x0, #0x88  */
    GUINT32_TO_LE (0xd65f03c0), /* ret            */

    /* func_b: */
    GUINT32_TO_LE (0xd2801320), /* mov x0, #0x99  */
    GUINT32_TO_LE (0xd65f03c0), /* ret            */
  };
  StalkerTestFunc func;
  guint8 * func_a;
  CallProbeContext probe_ctx, secondary_probe_ctx;
  GumProbeId probe_id;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  func_a = fixture->code + (16 * 4);

  probe_ctx.num_calls = 0;
  probe_ctx.target_address = func_a;
  probe_ctx.return_address = fixture->code + (8 * 4);
  probe_id = gum_stalker_add_call_probe (fixture->stalker, func_a,
      probe_func_a_invocation, &probe_ctx, NULL);
  test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 1);

  secondary_probe_ctx.num_calls = 0;
  secondary_probe_ctx.target_address = probe_ctx.target_address;
  secondary_probe_ctx.return_address = probe_ctx.return_address;
  gum_stalker_add_call_probe (fixture->stalker, func_a, probe_func_a_invocation,
      &secondary_probe_ctx, NULL);
  test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 2);
  g_assert_cmpuint (secondary_probe_ctx.num_calls, ==, 1);

  gum_stalker_remove_call_probe (fixture->stalker, probe_id);
  test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 2);
  g_assert_cmpuint (secondary_probe_ctx.num_calls, ==, 2);
}

static void
probe_func_a_invocation (GumCallDetails * details,
                         gpointer user_data)
{
  CallProbeContext * ctx = user_data;
  gsize * stack_values = details->stack_data;
  GumCpuContext * cpu_context = details->cpu_context;

  ctx->num_calls++;

  GUM_ASSERT_CMPADDR (details->target_address, ==, ctx->target_address);
  GUM_ASSERT_CMPADDR (details->return_address, ==, ctx->return_address);

  g_assert_cmphex (GPOINTER_TO_SIZE (
      gum_cpu_context_get_nth_argument (cpu_context, 0)), ==, 0x11);
  g_assert_cmphex (GPOINTER_TO_SIZE (
      gum_cpu_context_get_nth_argument (cpu_context, 1)), ==, 0x22);

  g_assert_cmphex (stack_values[0], ==, 0x11);
  g_assert_cmphex (stack_values[1], ==, 0x22);

  g_assert_cmphex (cpu_context->pc, ==, GPOINTER_TO_SIZE (ctx->target_address));
  g_assert_cmphex (cpu_context->lr, ==, GPOINTER_TO_SIZE (ctx->return_address));
  g_assert_cmphex (cpu_context->x[0], ==, 0x11);
  g_assert_cmphex (cpu_context->x[1], ==, 0x22);
  g_assert_cmphex (cpu_context->x[2], ==, 0x33);
  g_assert_cmphex (cpu_context->x[3], ==, 0x44);
  g_assert_cmphex (cpu_context->x[19], ==, 0xaa);
}

TESTCASE (custom_transformer)
{
  guint64 last_x0 = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      insert_extra_add_after_sub, &last_x0, NULL);

  g_assert_cmpuint (last_x0, ==, 0);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 3);

  g_assert_cmpuint (last_x0, ==, 3);
}

static void
insert_extra_add_after_sub (GumStalkerIterator * iterator,
                            GumStalkerOutput * output,
                            gpointer user_data)
{
  guint64 * last_x0 = user_data;
  const cs_insn * insn;
  gboolean in_leaf_func;

  in_leaf_func = FALSE;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (in_leaf_func && insn->id == ARM64_INS_RET)
    {
      gum_stalker_iterator_put_callout (iterator, store_x0, last_x0, NULL);
    }

    gum_stalker_iterator_keep (iterator);

    if (insn->id == ARM64_INS_SUB)
    {
      in_leaf_func = TRUE;

      gum_arm64_writer_put_add_reg_reg_imm (output->writer.arm64, ARM64_REG_W0,
          ARM64_REG_W0, 1);
    }
  }
}

static void
store_x0 (GumCpuContext * cpu_context,
          gpointer user_data)
{
  guint64 * last_x0 = user_data;

  *last_x0 = cpu_context->x[0];
}

TESTCASE (transformer_should_be_able_to_skip_call)
{
  guint32 code_template[] =
  {
    GUINT32_TO_LE (0xa9bf7bfd), /* push {x29, x30} */
    GUINT32_TO_LE (0xd280a280), /* mov x0, #1300   */
    GUINT32_TO_LE (0x94000003), /* bl bump_number  */
    GUINT32_TO_LE (0xa8c17bfd), /* pop {x29, x30}  */
    GUINT32_TO_LE (0xd65f03c0), /* ret             */
    /* bump_number:                */
    GUINT32_TO_LE (0x91009400), /* add x0, x0, #37 */
    GUINT32_TO_LE (0xd65f03c0), /* ret             */
  };
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (skip_call,
      func, NULL);

  ret = test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (ret, ==, 1300);
}

static void
skip_call (GumStalkerIterator * iterator,
           GumStalkerOutput * output,
           gpointer user_data)
{
  const guint32 * func_start = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->address == GPOINTER_TO_SIZE (func_start + 2))
      continue;

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (transformer_should_be_able_to_replace_call_with_callout)
{
  guint32 code_template[] =
  {
    GUINT32_TO_LE (0xa9bf7bfd), /* push {x29, x30} */
    GUINT32_TO_LE (0xd280a280), /* mov x0, #1300   */
    GUINT32_TO_LE (0x94000003), /* bl bump_number  */
    GUINT32_TO_LE (0xa8c17bfd), /* pop {x29, x30}  */
    GUINT32_TO_LE (0xd65f03c0), /* ret             */
    /* bump_number:                */
    GUINT32_TO_LE (0x91009400), /* add x0, x0, #37 */
    GUINT32_TO_LE (0xd65f03c0), /* ret             */
  };
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      replace_call_with_callout, func, NULL);

  ret = test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (ret, ==, 0xc001);
}

static void
replace_call_with_callout (GumStalkerIterator * iterator,
                           GumStalkerOutput * output,
                           gpointer user_data)
{
  const guint32 * func_start = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->address == GPOINTER_TO_SIZE (func_start + 2))
    {
      gum_stalker_iterator_put_callout (iterator, callout_set_cool, NULL, NULL);
      continue;
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (transformer_should_be_able_to_replace_tailjump_with_callout)
{
  guint32 code_template[] =
  {
    GUINT32_TO_LE (0xd280a280), /* mov x0, #1300   */
    GUINT32_TO_LE (0x14000001), /* b bump_number   */
    /* bump_number:                */
    GUINT32_TO_LE (0x91009400), /* add x0, x0, #37 */
    GUINT32_TO_LE (0xd65f03c0), /* ret             */
  };
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      replace_jmp_with_callout, func, NULL);

  ret = test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (ret, ==, 0xc001);
}

static void
replace_jmp_with_callout (GumStalkerIterator * iterator,
                          GumStalkerOutput * output,
                          gpointer user_data)
{
  const guint32 * func_start = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->address == GPOINTER_TO_SIZE (func_start + 1))
    {
      gum_stalker_iterator_put_callout (iterator, callout_set_cool, NULL, NULL);
      gum_stalker_iterator_put_chaining_return (iterator);
      continue;
    }

    gum_stalker_iterator_keep (iterator);
  }
}

static void
callout_set_cool (GumCpuContext * cpu_context,
                  gpointer user_data)
{
  cpu_context->x[0] = 0xc001;
}

TESTCASE (many_callouts_should_not_overflow_literal_pool)
{
  gint num_callouts = 0;
  guint32 * code;
  GumArm64Writer cw;
  guint i;
  StalkerTestFunc func;
  gint ret;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      add_callout_for_every_instruction, &num_callouts, NULL);
  fixture->sink->mask = GUM_NOTHING;

  code = g_malloc ((MANY_CALLOUTS_INSN_COUNT + 1) * sizeof (guint32));
  gum_arm64_writer_init (&cw, code);
  for (i = 0; i != MANY_CALLOUTS_INSN_COUNT; i++)
    gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_ret (&cw);
  gum_arm64_writer_flush (&cw);

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture, code,
      gum_arm64_writer_offset (&cw));

  gum_arm64_writer_clear (&cw);
  g_free (code);

  ret = test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpint (ret, ==, MANY_CALLOUTS_INSN_COUNT);
  g_assert_cmpint (num_callouts, >=, MANY_CALLOUTS_INSN_COUNT);
}

static void
add_callout_for_every_instruction (GumStalkerIterator * iterator,
                                   GumStalkerOutput * output,
                                   gpointer user_data)
{
  gint * num_callouts = user_data;

  while (gum_stalker_iterator_next (iterator, NULL))
  {
    gum_stalker_iterator_put_callout (iterator, bump_num_callouts,
        num_callouts, NULL);
    gum_stalker_iterator_keep (iterator);
  }
}

static void
bump_num_callouts (GumCpuContext * cpu_context,
                   gpointer user_data)
{
  gint * num_callouts = user_data;

  (*num_callouts)++;
}

TESTCASE (unfollow_should_be_allowed_before_first_transform)
{
  UnfollowTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.num_blocks_transformed = 0;
  ctx.target_block = 0;
  ctx.max_instructions = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      unfollow_during_transform, &ctx, NULL);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 2);
}

TESTCASE (unfollow_should_be_allowed_mid_first_transform)
{
  UnfollowTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.num_blocks_transformed = 0;
  ctx.target_block = 0;
  ctx.max_instructions = 1;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      unfollow_during_transform, &ctx, NULL);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 2);
}

TESTCASE (unfollow_should_be_allowed_after_first_transform)
{
  UnfollowTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.num_blocks_transformed = 0;
  ctx.target_block = 0;
  ctx.max_instructions = -1;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      unfollow_during_transform, &ctx, NULL);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 2);
}

TESTCASE (unfollow_should_be_allowed_before_second_transform)
{
  UnfollowTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.num_blocks_transformed = 0;
  ctx.target_block = 1;
  ctx.max_instructions = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      unfollow_during_transform, &ctx, NULL);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 2);
}

TESTCASE (unfollow_should_be_allowed_mid_second_transform)
{
  UnfollowTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.num_blocks_transformed = 0;
  ctx.target_block = 1;
  ctx.max_instructions = 1;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      unfollow_during_transform, &ctx, NULL);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 2);
}

TESTCASE (unfollow_should_be_allowed_after_second_transform)
{
  UnfollowTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.num_blocks_transformed = 0;
  ctx.target_block = 1;
  ctx.max_instructions = -1;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      unfollow_during_transform, &ctx, NULL);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 2);
}

static void
unfollow_during_transform (GumStalkerIterator * iterator,
                           GumStalkerOutput * output,
                           gpointer user_data)
{
  UnfollowTransformContext * ctx = user_data;
  const cs_insn * insn;

  if (ctx->num_blocks_transformed == ctx->target_block)
  {
    gint n;

    for (n = 0; n != ctx->max_instructions &&
        gum_stalker_iterator_next (iterator, &insn); n++)
    {
      gum_stalker_iterator_keep (iterator);
    }

    gum_stalker_unfollow_me (ctx->stalker);
  }
  else
  {
    while (gum_stalker_iterator_next (iterator, &insn))
      gum_stalker_iterator_keep (iterator);
  }

  ctx->num_blocks_transformed++;
}

TESTCASE (follow_me_should_support_nullable_event_sink)
{
  gpointer p;

  gum_stalker_follow_me (fixture->stalker, NULL, NULL);
  p = malloc (1);
  free (p);
  gum_stalker_unfollow_me (fixture->stalker);
}

TESTCASE (invalidation_for_current_thread_should_be_supported)
{
  InvalidationTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.target_function = test_is_finished;
  ctx.n = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      modify_to_return_true_after_three_calls, &ctx, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer, NULL);

  while (!test_is_finished ())
  {
  }

  gum_stalker_unfollow_me (fixture->stalker);
}

static gboolean GUM_NOINLINE
test_is_finished (void)
{
  return stalker_invalidation_test_is_finished;
}

static void
modify_to_return_true_after_three_calls (GumStalkerIterator * iterator,
                                         GumStalkerOutput * output,
                                         gpointer user_data)
{
  InvalidationTransformContext * ctx = user_data;
  guint i;
  const cs_insn * insn;
  gboolean in_target_function = FALSE;

  for (i = 0; gum_stalker_iterator_next (iterator, &insn); i++)
  {
    if (i == 0)
    {
      in_target_function =
          insn->address == GPOINTER_TO_SIZE (ctx->target_function);

      if (in_target_function && ctx->n == 0)
      {
        gum_stalker_iterator_put_callout (iterator,
            invalidate_after_three_calls, ctx, NULL);
      }
    }

    if (insn->id == ARM64_INS_RET && in_target_function && ctx->n == 3)
    {
      gum_arm64_writer_put_ldr_reg_u32 (output->writer.arm64, ARM64_REG_W0,
          TRUE);
    }

    gum_stalker_iterator_keep (iterator);
  }
}

static void
invalidate_after_three_calls (GumCpuContext * cpu_context,
                              gpointer user_data)
{
  InvalidationTransformContext * ctx = user_data;

  if (++ctx->n == 3)
  {
    gum_stalker_invalidate (ctx->stalker, ctx->target_function);
  }
}

TESTCASE (invalidation_for_specific_thread_should_be_supported)
{
  InvalidationTarget a, b;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  start_invalidation_target (&a, fixture);
  start_invalidation_target (&b, fixture);

  gum_stalker_invalidate_for_thread (fixture->stalker, a.thread_id,
      test_is_finished);
  join_invalidation_target (&a);

  g_usleep (50000);
  g_assert_false (b.finished);

  gum_stalker_invalidate_for_thread (fixture->stalker, b.thread_id,
      test_is_finished);
  join_invalidation_target (&b);
  g_assert_true (b.finished);
}

static void
start_invalidation_target (InvalidationTarget * target,
                           TestArm64StalkerFixture * fixture)
{
  InvalidationTransformContext * ctx = &target->ctx;
  StalkerDummyChannel * channel = &target->channel;

  ctx->stalker = fixture->stalker;
  ctx->target_function = test_is_finished;
  ctx->n = 0;

  target->transformer = gum_stalker_transformer_make_from_callback (
      modify_to_return_true_on_subsequent_transform, ctx, NULL);

  target->finished = FALSE;

  sdc_init (channel);

  target->thread = g_thread_new ("stalker-invalidation-target",
      run_stalked_until_finished, target);
  target->thread_id = sdc_await_thread_id (channel);

  gum_stalker_follow (ctx->stalker, target->thread_id, target->transformer,
      NULL);
  sdc_put_follow_confirmation (channel);

  sdc_await_run_confirmation (channel);
}

static void
join_invalidation_target (InvalidationTarget * target)
{
  GumStalker * stalker = target->ctx.stalker;

  g_thread_join (target->thread);

  gum_stalker_unfollow (stalker, target->thread_id);

  sdc_finalize (&target->channel);

  g_object_unref (target->transformer);
}

static gpointer
run_stalked_until_finished (gpointer data)
{
  InvalidationTarget * target = data;
  StalkerDummyChannel * channel = &target->channel;
  gboolean first_iteration;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  first_iteration = TRUE;

  while (!test_is_finished ())
  {
    if (first_iteration)
    {
      sdc_put_run_confirmation (channel);
      first_iteration = FALSE;
    }

    g_thread_yield ();
  }

  target->finished = TRUE;

  return NULL;
}

static void
modify_to_return_true_on_subsequent_transform (GumStalkerIterator * iterator,
                                               GumStalkerOutput * output,
                                               gpointer user_data)
{
  InvalidationTransformContext * ctx = user_data;
  guint i;
  const cs_insn * insn;
  gboolean in_target_function = FALSE;

  for (i = 0; gum_stalker_iterator_next (iterator, &insn); i++)
  {
    if (i == 0)
    {
      in_target_function =
          insn->address == GPOINTER_TO_SIZE (ctx->target_function);
      if (in_target_function)
        ctx->n++;
    }

    if (insn->id == ARM64_INS_RET && in_target_function && ctx->n > 1)
    {
      gum_arm64_writer_put_ldr_reg_u32 (output->writer.arm64, ARM64_REG_W0,
          TRUE);
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (invalidation_should_allow_block_to_grow)
{
  InvalidationTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.target_function = get_magic_number;
  ctx.n = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      add_n_return_value_increments, &ctx, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer, NULL);

  g_assert_cmpint (get_magic_number (), ==, 42);

  ctx.n = 1;
  gum_stalker_invalidate (fixture->stalker, ctx.target_function);
  g_assert_cmpint (get_magic_number (), ==, 43);
  g_assert_cmpint (get_magic_number (), ==, 43);

  ctx.n = 2;
  gum_stalker_invalidate (fixture->stalker, ctx.target_function);
  g_assert_cmpint (get_magic_number (), ==, 44);

  gum_stalker_unfollow_me (fixture->stalker);
}

static int GUM_NOINLINE
get_magic_number (void)
{
  return stalker_invalidation_magic_number;
}

static void
add_n_return_value_increments (GumStalkerIterator * iterator,
                               GumStalkerOutput * output,
                               gpointer user_data)
{
  InvalidationTransformContext * ctx = user_data;
  guint i;
  const cs_insn * insn;
  gboolean in_target_function = FALSE;

  for (i = 0; gum_stalker_iterator_next (iterator, &insn); i++)
  {
    if (i == 0)
    {
      in_target_function =
          insn->address == GPOINTER_TO_SIZE (ctx->target_function);
    }

    if (insn->id == ARM64_INS_RET && in_target_function)
    {
      guint increment_index;

      for (increment_index = 0; increment_index != ctx->n; increment_index++)
      {
        gum_arm64_writer_put_add_reg_reg_imm (output->writer.arm64,
            ARM64_REG_W0, ARM64_REG_W0, 1);
      }
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (exclude_bl)
{
  const guint32 code_template[] =
  {
    GUINT32_TO_LE (0xa9bf7bf3), /* push {x19, lr} */
    GUINT32_TO_LE (0xd2801553), /* mov x19, #0xaa */
    GUINT32_TO_LE (0xd2800883), /* mov x3, #0x44  */
    GUINT32_TO_LE (0xd2800662), /* mov x2, #0x33  */
    GUINT32_TO_LE (0xd2800441), /* mov x1, #0x22  */
    GUINT32_TO_LE (0xd2800220), /* mov x0, #0x11  */
    GUINT32_TO_LE (0xa9bf07e0), /* push {x0, x1}  */
    GUINT32_TO_LE (0x94000009), /* bl func_a      */
    GUINT32_TO_LE (0xa8c107e0), /* pop {x0, x1}   */
    GUINT32_TO_LE (0xd2801103), /* mov x3, #0x88  */
    GUINT32_TO_LE (0xd2800ee2), /* mov x2, #0x77  */
    GUINT32_TO_LE (0xd2800cc1), /* mov x1, #0x66  */
    GUINT32_TO_LE (0xd2800aa0), /* mov x0, #0x55  */
    GUINT32_TO_LE (0x94000005), /* bl func_b      */
    GUINT32_TO_LE (0xa8c17bf3), /* pop {x19, lr}  */
    GUINT32_TO_LE (0xd65f03c0), /* ret            */

    /* func_a: */
    GUINT32_TO_LE (0xd2801100), /* mov x0, #0x88  */
    GUINT32_TO_LE (0xd65f03c0), /* ret            */

    /* func_b: */
    GUINT32_TO_LE (0xd2801320), /* mov x0, #0x99  */
    GUINT32_TO_LE (0xd65f03c0), /* ret            */
  };
  StalkerTestFunc func;
  guint8 * func_a_address;
  GumMemoryRange memory_range;

  fixture->sink->mask = GUM_EXEC;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  func_a_address = fixture->code + (16 * 4);
  memory_range.base_address = GUM_ADDRESS (func_a_address);
  memory_range.size = 4 * 2;
  gum_stalker_exclude (fixture->stalker, &memory_range);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, 24);
}

TESTCASE (exclude_blr)
{
  gsize page_size;
  StalkerTestFunc func;
  guint8 * code;
  GumArm64Writer cw;
  gpointer func_a;
  GumMemoryRange memory_range;
  const gchar * start_lbl = "start";

  fixture->sink->mask = GUM_EXEC;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_b_label (&cw, start_lbl);

  func_a = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 10);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, start_lbl);
  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ldr_reg_address (&cw, ARM64_REG_X1,
      GUM_ADDRESS (gum_sign_code_pointer (func_a)));
  gum_arm64_writer_put_blr_reg (&cw, ARM64_REG_X1);
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  memory_range.base_address = GUM_ADDRESS (func_a);
  memory_range.size = 4 * 2;
  gum_stalker_exclude (fixture->stalker, &memory_range);

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  g_assert_cmpint (func (2), ==, 12);

#ifdef HAVE_DARWIN
  g_assert_cmpuint (fixture->sink->events->length, ==, 41);
#else
  g_assert_cmpuint (fixture->sink->events->length, ==, 42);
#endif

  gum_memory_free (code, page_size);
}

TESTCASE (exclude_bl_with_unfollow)
{
  gsize page_size;
  StalkerTestFunc func;
  guint8 * code;
  GumArm64Writer cw;
  gpointer func_a;
  GumMemoryRange memory_range;
  const gchar * start_lbl = "start";

  fixture->sink->mask = GUM_EXEC;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_b_label (&cw, start_lbl);

  func_a = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 10);
  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, start_lbl);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_bl_imm (&cw, GUM_ADDRESS (func_a));
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  memory_range.base_address = GUM_ADDRESS (func_a);
  memory_range.size = 4 * 20;
  gum_stalker_exclude (fixture->stalker, &memory_range);

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  g_assert_cmpint (func (2), ==, 12);

  g_assert_cmpuint (fixture->sink->events->length, ==, 20);

  gum_memory_free (code, page_size);
}

TESTCASE (exclude_blr_with_unfollow)
{
  gsize page_size;
  StalkerTestFunc func;
  guint8 * code;
  GumArm64Writer cw;
  gpointer func_a;
  GumMemoryRange memory_range;
  const gchar * start_lbl = "start";

  fixture->sink->mask = GUM_EXEC;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_b_label (&cw, start_lbl);

  func_a = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 10);
  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, start_lbl);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);
  gum_arm64_writer_put_ldr_reg_address (&cw, ARM64_REG_X1,
      GUM_ADDRESS (gum_sign_code_pointer (func_a)));
  gum_arm64_writer_put_blr_reg (&cw, ARM64_REG_X1);
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X19, ARM64_REG_LR);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  memory_range.base_address = GUM_ADDRESS (func_a);
  memory_range.size = 4 * 20;
  gum_stalker_exclude (fixture->stalker, &memory_range);

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  g_assert_cmpint (func (2), ==, 12);

  g_assert_cmpuint (fixture->sink->events->length, ==, 21);

  gum_memory_free (code, page_size);
}

TESTCASE (unconditional_branch)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  GumAddress address;
  const gchar * my_ken_lbl = "my_ken";
  StalkerTestFunc func;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_b_label (&cw, my_ken_lbl);

  address = GUM_ADDRESS (gum_arm64_writer_cur (&cw));
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 10);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_b_imm (&cw, address);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));

  g_assert_cmpint (func (2), ==, 13);

  gum_memory_free (code, page_size);
}

TESTCASE (unconditional_branch_reg)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  GumAddress address;
  const gchar * my_ken_lbl = "my_ken";
  StalkerTestFunc func;
  arm64_reg reg = ARM64_REG_X13;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_b_label (&cw, my_ken_lbl);

  address = GUM_ADDRESS (gum_arm64_writer_cur (&cw));
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 10);
  if (reg == ARM64_REG_X29 || reg == ARM64_REG_X30)
    gum_arm64_writer_put_pop_reg_reg (&cw, reg, ARM64_REG_XZR);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  if (reg == ARM64_REG_X29 || reg == ARM64_REG_X30)
    gum_arm64_writer_put_push_reg_reg (&cw, reg, reg);
  gum_arm64_writer_put_ldr_reg_address (&cw, reg, address);
  gum_arm64_writer_put_br_reg (&cw, reg);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));

  g_assert_cmpint (func (2), ==, 13);

  gum_memory_free (code, page_size);
}

TESTCASE (conditional_branch)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  GumAddress address;
  arm64_cc cc = ARM64_CC_EQ;
  const gchar * my_ken_lbl = "my_ken";
  StalkerTestFunc func;
  gint r;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_instruction (&cw, 0xF1000800);  /* SUBS X0, X0, #2 */
  gum_arm64_writer_put_b_cond_label (&cw, cc, my_ken_lbl);

  address = GUM_ADDRESS (gum_arm64_writer_cur (&cw));
  gum_arm64_writer_put_nop (&cw);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_b_imm (&cw, address);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));
  r = func (2);

  g_assert_cmpint (r, ==, 1);

  gum_memory_free (code, page_size);
}

TESTCASE (compare_and_branch)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  const gchar * my_ken_lbl = "my_ken";
  const gchar * my_nken_lbl = "my_nken";
  StalkerTestFunc func;
  gint r;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_sub_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 2);
  gum_arm64_writer_put_cbz_reg_label (&cw, ARM64_REG_X0, my_ken_lbl);

  gum_arm64_writer_put_label (&cw, my_nken_lbl);
  gum_arm64_writer_put_nop (&cw);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_cbnz_reg_label (&cw, ARM64_REG_X0, my_nken_lbl);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));
  r = func (2);

  g_assert_cmpint (r, ==, 1);

  gum_memory_free (code, page_size);
}

TESTCASE (test_bit_and_branch)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  const gchar * my_ken_lbl = "my_ken";
  const gchar * my_nken_lbl = "my_nken";
  StalkerTestFunc func;
  gint r;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_sub_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 2);
  gum_arm64_writer_put_tbz_reg_imm_label (&cw, ARM64_REG_W0, 0, my_ken_lbl);

  gum_arm64_writer_put_label (&cw, my_nken_lbl);
  gum_arm64_writer_put_nop (&cw);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_tbnz_reg_imm_label (&cw, ARM64_REG_W0, 0, my_nken_lbl);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));
  r = func (2);

  g_assert_cmpint (r, ==, 1);

  gum_memory_free (code, page_size);
}

TESTCASE (follow_std_call)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  GumAddress address;
  const gchar * my_ken_lbl = "my_ken";
  StalkerTestFunc func;
  gint r;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X30, ARM64_REG_X29);
  gum_arm64_writer_put_mov_reg_reg (&cw, ARM64_REG_X29, ARM64_REG_SP);

  gum_arm64_writer_put_b_label (&cw, my_ken_lbl);

  address = GUM_ADDRESS (gum_arm64_writer_cur (&cw));
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_bl_imm (&cw, address);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X30, ARM64_REG_X29);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));
  r = func (2);

  g_assert_cmpint (r, ==, 4);

  gum_memory_free (code, page_size);
}

TESTCASE (follow_return)
{
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  GumAddress address;
  const gchar * my_ken_lbl = "my_ken";
  StalkerTestFunc func;
  gint r;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X30, ARM64_REG_X29);
  gum_arm64_writer_put_mov_reg_reg (&cw, ARM64_REG_X29, ARM64_REG_SP);

  gum_arm64_writer_put_b_label (&cw, my_ken_lbl);

  address = GUM_ADDRESS (gum_arm64_writer_cur (&cw));
  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);
  /*
   * alternative for instruction RET X15
   * gum_arm64_writer_put_mov_reg_reg (&cw, ARM64_REG_X15, ARM64_REG_X30);
   * gum_arm64_writer_put_instruction (&cw, 0xD65F01E0);
   */
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_put_label (&cw, my_ken_lbl);
  gum_arm64_writer_put_nop (&cw);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);
  gum_arm64_writer_put_bl_imm (&cw, address);
  gum_arm64_writer_put_add_reg_reg_imm (&cw, ARM64_REG_X0, ARM64_REG_X0, 1);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X30, ARM64_REG_X29);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, gum_sign_code_pointer (code));
  r = func (2);

  g_assert_cmpint (r, ==, 4);

  gum_memory_free (code, page_size);
}

TESTCASE (follow_misaligned_stack)
{
  const guint32 code_template[] =
  {
    GUINT32_TO_LE (0xa9bf7bf4), /* stp x20, lr, [sp, #-0x10]! */
    GUINT32_TO_LE (0xd10023ff), /* sub sp, sp, #8             */
    GUINT32_TO_LE (0x14000002), /* b part_two                 */
    GUINT32_TO_LE (0xd4200540), /* brk #42                    */
    /* part_two:                              */
    GUINT32_TO_LE (0x94000009), /* bl get_base_value          */
    GUINT32_TO_LE (0x10000070), /* adr x16, part_three        */
    GUINT32_TO_LE (0xd61f0200), /* br x16                     */
    GUINT32_TO_LE (0xd4200560), /* brk #43                    */
    /* part_three:                            */
    GUINT32_TO_LE (0x100000f0), /* adr x16, add_other_value   */
    GUINT32_TO_LE (0xd63f0200), /* blr x16                    */
    GUINT32_TO_LE (0x910023ff), /* add sp, sp, #8             */
    GUINT32_TO_LE (0xa8c17bf4), /* ldp x20, lr, [sp], #0x10   */
    GUINT32_TO_LE (0xd65f03c0), /* ret                        */
    /* get_base_value:                        */
    GUINT32_TO_LE (0xd2800500), /* mov x0, #40                */
    GUINT32_TO_LE (0xd65f03c0), /* ret                        */
    /* add_other_value:                       */
    GUINT32_TO_LE (0x91000800), /* add x0, x0, #2             */
    GUINT32_TO_LE (0xd65f03c0), /* ret                        */
  };
  StalkerTestFunc func;

  /*
   * The misaligned-stack mitigation resumes execution with a deliberately
   * misaligned SP after emulating the faulting prolog/fast-path store. On
   * Windows ARM64 the kernel's NtContinue() rejects a context whose SP is not
   * 16-byte aligned (STATUS_INVALID_PARAMETER), so the mitigation crashes
   * there. Other platforms' sigreturn equivalents tolerate it. Gate this until
   * the mitigation is reworked to always resume on an aligned SP.
   */
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  fixture->sink->mask = GUM_EXEC;

  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 42);

  g_assert_cmpuint (fixture->sink->events->length, ==, 21);
}

TESTCASE (follow_syscall)
{
  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  g_usleep (1);
  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (follow_thread)
{
  StalkerDummyChannel channel;
  GThread * thread;
  GumThreadId thread_id;
#ifdef HAVE_LINUX
  int prev_dumpable;

  /* Android spawns non-debuggable applications as not dumpable by default. */
  prev_dumpable = prctl (PR_GET_DUMPABLE);
  prctl (PR_SET_DUMPABLE, 0);
#endif

  sdc_init (&channel);

  thread = g_thread_new ("stalker-test-target", run_stalked_briefly, &channel);
  thread_id = sdc_await_thread_id (&channel);

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;
  gum_stalker_follow (fixture->stalker, thread_id, NULL,
      GUM_EVENT_SINK (fixture->sink));
  sdc_put_follow_confirmation (&channel);

  sdc_await_run_confirmation (&channel);
  g_assert_cmpuint (fixture->sink->events->length, >, 0);

  gum_stalker_unfollow (fixture->stalker, thread_id);
  sdc_put_unfollow_confirmation (&channel);

  sdc_await_flush_confirmation (&channel);
  gum_fake_event_sink_reset (fixture->sink);

  sdc_put_finish_confirmation (&channel);

  g_thread_join (thread);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  sdc_finalize (&channel);

#ifdef HAVE_LINUX
  prctl (PR_SET_DUMPABLE, prev_dumpable);
#endif
}

static gpointer
run_stalked_briefly (gpointer data)
{
  StalkerDummyChannel * channel = data;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  sdc_put_run_confirmation (channel);

  sdc_await_unfollow_confirmation (channel);

  sdc_put_flush_confirmation (channel);

  sdc_await_finish_confirmation (channel);

  return NULL;
}

TESTCASE (unfollow_should_handle_terminated_thread)
{
  guint i;

  for (i = 0; i != 10; i++)
  {
    StalkerDummyChannel channel;
    GThread * thread;
    GumThreadId thread_id;

    sdc_init (&channel);

    thread = g_thread_new ("stalker-test-target", run_stalked_into_termination,
        &channel);
    thread_id = sdc_await_thread_id (&channel);

    fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;
    gum_stalker_follow (fixture->stalker, thread_id, NULL,
        GUM_EVENT_SINK (fixture->sink));
    sdc_put_follow_confirmation (&channel);

    g_thread_join (thread);

    if (i % 2 == 0)
      g_usleep (50000);

    gum_stalker_unfollow (fixture->stalker, thread_id);

    sdc_finalize (&channel);

    while (gum_stalker_garbage_collect (fixture->stalker))
      g_usleep (10000);
  }
}

static gpointer
run_stalked_into_termination (gpointer data)
{
  StalkerDummyChannel * channel = data;

  sdc_put_thread_id (channel, gum_process_get_current_thread_id ());

  sdc_await_follow_confirmation (channel);

  return NULL;
}

TESTCASE (self_modifying_code_should_be_detected_with_threshold_minus_one)
{
  FlatFunc f;
  CodePatcher patcher;

  f = (FlatFunc) test_arm64_stalker_fixture_dup_code (fixture, flat_code,
      sizeof (flat_code));

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  code_patcher_start (&patcher);
  gum_stalker_set_trust_threshold (fixture->stalker, -1);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_assert_cmpuint (f (), ==, 2);

  patch_instruction (&patcher, f, 4, 0x1100a400);
  g_assert_cmpuint (f (), ==, 42);
  f ();
  f ();

  patch_instruction (&patcher, f, 4, 0x1114e000);
  g_assert_cmpuint (f (), ==, 1337);

  gum_stalker_unfollow_me (fixture->stalker);

  code_patcher_stop (&patcher);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (self_modifying_code_should_not_be_detected_with_threshold_zero)
{
  FlatFunc f;
  CodePatcher patcher;

  f = (FlatFunc) test_arm64_stalker_fixture_dup_code (fixture, flat_code,
      sizeof (flat_code));

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  code_patcher_start (&patcher);
  gum_stalker_set_trust_threshold (fixture->stalker, 0);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_assert_cmpuint (f (), ==, 2);

  patch_instruction (&patcher, f, 4, 0x1100a400);
  g_assert_cmpuint (f (), ==, 2);

  gum_stalker_unfollow_me (fixture->stalker);

  code_patcher_stop (&patcher);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (self_modifying_code_should_be_detected_with_threshold_one)
{
  FlatFunc f;
  CodePatcher patcher;

  f = (FlatFunc) test_arm64_stalker_fixture_dup_code (fixture, flat_code,
      sizeof (flat_code));

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  code_patcher_start (&patcher);
  gum_stalker_set_trust_threshold (fixture->stalker, 1);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_assert_cmpuint (f (), ==, 2);

  patch_instruction (&patcher, f, 4, 0x1100a400);
  g_assert_cmpuint (f (), ==, 42);
  f ();
  f ();

  patch_instruction (&patcher, f, 4, 0x1114e000);
  g_assert_cmpuint (f (), ==, 42);

  gum_stalker_unfollow_me (fixture->stalker);

  code_patcher_stop (&patcher);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (exclusive_load_store_should_not_be_disturbed)
{
  guint32 code_template[] =
  {
    GUINT32_TO_LE (0x58000200), /* ldr x0, [pointer_to_value] */
    /* retry:                                 */
    GUINT32_TO_LE (0xc85f7c01), /* ldxr x1, [x0]              */
    GUINT32_TO_LE (0xf100043f), /* cmp x1, #1                 */
    GUINT32_TO_LE (0x54000160), /* b.eq nope                  */
    GUINT32_TO_LE (0xf100083f), /* cmp x1, #2                 */
    GUINT32_TO_LE (0x54000120), /* b.eq nope                  */
    GUINT32_TO_LE (0xf1000c3f), /* cmp x1, #3                 */
    GUINT32_TO_LE (0x540000e0), /* b.eq nope                  */
    GUINT32_TO_LE (0xf100103f), /* cmp x1, #4                 */
    GUINT32_TO_LE (0x540000a0), /* b.eq nope                  */
    GUINT32_TO_LE (0x91000421), /* add x1, x1, #1             */
    GUINT32_TO_LE (0xc8027c01), /* stxr w2, x1, [x0]          */
    GUINT32_TO_LE (0x35fffea2), /* cbnz w2, retry             */
    GUINT32_TO_LE (0xd65f03c0), /* ret                        */
    /* nope:                                                  */
    GUINT32_TO_LE (0xd5033f5f), /* clrex                      */
    GUINT32_TO_LE (0xd65f03c0), /* ret                        */
    /* pointer_to_value:                                      */
    0x44332211, 0x88776655,
  };
  StalkerTestFunc func;
  guint64 val;
  gint num_cmp_callouts;

  fixture->sink->mask = GUM_EXEC;

  *((guint64 **) (code_template + G_N_ELEMENTS (code_template) - 2)) = &val;
  func = (StalkerTestFunc) test_arm64_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      insert_callout_after_cmp, &num_cmp_callouts, NULL);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);

  val = 5;
  num_cmp_callouts = 0;
  test_arm64_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (val, ==, 6);
  g_assert_cmpint (num_cmp_callouts, ==, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==, 17);
}

static void
insert_callout_after_cmp (GumStalkerIterator * iterator,
                          GumStalkerOutput * output,
                          gpointer user_data)
{
  gint * num_cmp_callouts = user_data;
  GumMemoryAccess access;
  const cs_insn * insn;

  access = gum_stalker_iterator_get_memory_access (iterator);

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    gum_stalker_iterator_keep (iterator);

    if (insn->id == ARM64_INS_CMP && access == GUM_MEMORY_ACCESS_OPEN)
    {
      gum_stalker_iterator_put_callout (iterator, bump_num_cmp_callouts,
          num_cmp_callouts, NULL);
    }
  }
}

static void
bump_num_cmp_callouts (GumCpuContext * cpu_context,
                       gpointer user_data)
{
  gint * num_cmp_callouts = user_data;

  g_atomic_int_inc (num_cmp_callouts);
}

static void
code_patcher_start (CodePatcher * self)
{
  self->request = 0;
  self->done = 0;
  self->stop = FALSE;
  self->thread = g_thread_new ("code-patcher", code_patcher_worker, self);
}

static void
code_patcher_stop (CodePatcher * self)
{
  self->stop = TRUE;
  g_thread_join (self->thread);
}

static void
patch_instruction (CodePatcher * patcher,
                   gpointer code,
                   guint offset,
                   guint32 insn)
{
  gint request;

  /*
   * Run the patch on the unstalked patcher thread: tracing patch_code()'s
   * /proc/self/maps read never settles at trust threshold -1.
   */
  patcher->target = (guint8 *) code + offset;
  patcher->insn = insn;

  request = patcher->request + 1;
  patcher->request = request;

  while (patcher->done < request)
    ;
}

static gpointer
code_patcher_worker (gpointer data)
{
  CodePatcher * self = data;
  gint served = 0;

  while (!self->stop)
  {
    if (self->request > served)
    {
      gum_memory_patch_code (self->target, sizeof (self->insn),
          do_patch_instruction, GSIZE_TO_POINTER (self->insn));

      served++;
      self->done = served;
    }
    else
    {
      g_thread_yield ();
    }
  }

  return NULL;
}

static void
do_patch_instruction (gpointer mem,
                      gpointer user_data)
{
  guint32 * insn = mem;
  guint32 new_insn = GPOINTER_TO_SIZE (user_data);

  *insn = GUINT32_TO_LE (new_insn);
}

#ifndef HAVE_WINDOWS

TESTCASE (pthread_create)
{
  int ret;
  pthread_t thread;
  int number = 0;

  fixture->sink->mask = GUM_COMPILE;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  ret = pthread_create (&thread, NULL, increment_integer, (gpointer) &number);
  g_assert_cmpint (ret, ==, 0);

  ret = pthread_join (thread, NULL);
  g_assert_cmpint (ret, ==, 0);

  g_assert_cmpint (number, ==, 1);

  gum_stalker_unfollow_me (fixture->stalker);
}

static gpointer
increment_integer (gpointer data)
{
  int * number = (int *) data;
  *number += 1;
  return NULL;
}

#endif

TESTCASE (heap_api)
{
  gpointer p;

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  p = malloc (1);
  free (p);
  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

typedef void (* ClobberFunc) (GumCpuContext * ctx);

TESTCASE (no_register_clobber)
{
#ifndef HAVE_DARWIN
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  gint i;
  ClobberFunc func;
  GumCpuContext ctx;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_all_x_registers (&cw);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  for (i = ARM64_REG_X0; i <= ARM64_REG_X28; i++)
  {
    gboolean is_platform_register = i == ARM64_REG_X18;
    if (is_platform_register)
      continue;
    gum_arm64_writer_put_ldr_reg_u64 (&cw, i, i);
  }
  gum_arm64_writer_put_ldr_reg_u64 (&cw, ARM64_REG_FP, ARM64_REG_FP);
  gum_arm64_writer_put_ldr_reg_u64 (&cw, ARM64_REG_LR, ARM64_REG_LR);

  gum_arm64_writer_put_push_all_x_registers (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_arm64_writer_put_pop_all_x_registers (&cw);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_FP, ARM64_REG_SP,
      (2 + 30) * sizeof (gpointer));
  for (i = ARM64_REG_X0; i <= ARM64_REG_X28; i++)
  {
    gum_arm64_writer_put_str_reg_reg_offset (&cw, i, ARM64_REG_FP,
        G_STRUCT_OFFSET (GumCpuContext, x[i - ARM64_REG_X0]));
  }
  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_FP, ARM64_REG_LR);

  gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_X0, ARM64_REG_SP,
      30 * sizeof (gpointer));
  gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_FP, ARM64_REG_X0,
      G_STRUCT_OFFSET (GumCpuContext, fp));
  gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_LR, ARM64_REG_X0,
      G_STRUCT_OFFSET (GumCpuContext, lr));

  gum_arm64_writer_put_pop_all_x_registers (&cw);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (code, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (ClobberFunc, code);
  func (&ctx);

  for (i = ARM64_REG_X0; i <= ARM64_REG_X28; i++)
  {
    gboolean is_platform_register = i == ARM64_REG_X18;
    if (is_platform_register)
      continue;
    g_assert_cmphex (ctx.x[i - ARM64_REG_X0], ==, i);
  }
  g_assert_cmphex (ctx.fp, ==, ARM64_REG_FP);
  g_assert_cmphex (ctx.lr, ==, ARM64_REG_LR);

  gum_memory_free (code, page_size);
#endif
}

TESTCASE (performance)
{
  GTimer * timer;
  gdouble duration_direct, duration_stalked;

  timer = g_timer_new ();
  pretend_workload (fixture->runner_range);

  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  duration_direct = g_timer_elapsed (timer, NULL);

  fixture->sink->mask = GUM_NOTHING;

  gum_stalker_set_trust_threshold (fixture->stalker, 0);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  /* warm-up */
  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  g_timer_elapsed (timer, NULL);

  /* the real deal */
  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  duration_stalked = g_timer_elapsed (timer, NULL);

  gum_stalker_unfollow_me (fixture->stalker);

  g_timer_destroy (timer);

  g_print ("<duration_direct=%f duration_stalked=%f ratio=%f> ",
      duration_direct, duration_stalked, duration_stalked / duration_direct);
}

GUM_NOINLINE static void
pretend_workload (const GumMemoryRange * runner_range)
{
  lzma_stream stream = LZMA_STREAM_INIT;
  const uint32_t preset = 9 | LZMA_PRESET_EXTREME;
  lzma_ret ret;
  guint8 * outbuf;
  gsize outbuf_size;
  const gsize outbuf_size_increment = 1024 * 1024;

  ret = lzma_easy_encoder (&stream, preset, LZMA_CHECK_CRC64);
  g_assert_cmpint (ret, ==, LZMA_OK);

  outbuf_size = outbuf_size_increment;
  outbuf = malloc (outbuf_size);

  stream.next_in = GSIZE_TO_POINTER (runner_range->base_address);
  stream.avail_in = MIN (runner_range->size, 65536);
  stream.next_out = outbuf;
  stream.avail_out = outbuf_size;

  while (TRUE)
  {
    ret = lzma_code (&stream, LZMA_FINISH);

    if (stream.avail_out == 0)
    {
      gsize compressed_size;

      compressed_size = outbuf_size;

      outbuf_size += outbuf_size_increment;
      outbuf = realloc (outbuf, outbuf_size);

      stream.next_out = outbuf + compressed_size;
      stream.avail_out = outbuf_size - compressed_size;
    }

    if (ret != LZMA_OK)
    {
      g_assert_cmpint (ret, ==, LZMA_STREAM_END);
      break;
    }
  }

  lzma_end (&stream);

  free (outbuf);
}

#ifdef HAVE_LINUX

TESTCASE (prefetch)
{
  gint trust;
  int compile_pipes[2] = { -1, -1 };
  int execute_pipes[2] = { -1, -1 };
  GumEventSink * sink;
  GHashTable * compiled_run1;
  GHashTable * executed_run1;
  guint compiled_size_run1;
  guint executed_size_run1;
  GHashTableIter iter;
  gpointer iter_key, iter_value;
  GHashTable * compiled_run2;
  GHashTable * executed_run2;
  guint compiled_size_run2;
  guint executed_size_run2;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  /* Initialize Stalker */
  gum_stalker_set_trust_threshold (fixture->stalker, 3);
  trust = gum_stalker_get_trust_threshold (fixture->stalker);

  /*
   * Create IPC.
   *
   * The pipes by default are 64 KB in size. At 8-bytes per-block, (the block
   * address) we thus have capacity to communicate up to 8192 blocks back to the
   * parent before the child's write() call blocks and we deadlock in waitpid().
   *
   * We can increase the size of these pipes using fcntl(F_SETPIPE_SZ), but we
   * need to be careful so we don't exceed the limit set in
   * /proc/sys/fs/pipe-max-size.
   *
   * Since our test has approx 1800 blocks, we don't need to worry about this.
   * However, production implementations may need to handle this error.
   */
  g_assert_cmpint (pipe (compile_pipes), ==, 0);
  g_assert_cmpint (pipe (execute_pipes), ==, 0);
  g_assert_true (g_unix_set_fd_nonblocking (compile_pipes[0], TRUE, NULL));
  g_assert_true (g_unix_set_fd_nonblocking (compile_pipes[1], TRUE, NULL));
  g_assert_true (g_unix_set_fd_nonblocking (execute_pipes[0], TRUE, NULL));
  g_assert_true (g_unix_set_fd_nonblocking (execute_pipes[1], TRUE, NULL));

  /* Configure Stalker */
  sink = gum_event_sink_make_from_callback (GUM_COMPILE | GUM_BLOCK,
      prefetch_on_event, NULL, NULL);
  gum_stalker_follow_me (fixture->stalker, NULL, sink);
  gum_stalker_deactivate (fixture->stalker);

  /* Run the child */
  prefetch_run_child (fixture->stalker, fixture->runner_range,
      compile_pipes[STDOUT_FILENO], execute_pipes[STDOUT_FILENO]);

  /* Read the results */
  compiled_run1 = g_hash_table_new (NULL, NULL);
  prefetch_read_blocks (compile_pipes[STDIN_FILENO], compiled_run1);
  executed_run1 = g_hash_table_new (NULL, NULL);
  prefetch_read_blocks (execute_pipes[STDIN_FILENO], executed_run1);

  compiled_size_run1 = g_hash_table_size (compiled_run1);
  executed_size_run1 = g_hash_table_size (executed_run1);

  if (g_test_verbose ())
  {
    g_print ("\tcompiled: %d\n", compiled_size_run1);
    g_print ("\texecuted: %d\n", executed_size_run1);
  }

  g_assert_cmpuint (compiled_size_run1, >, 0);
  g_assert_cmpuint (compiled_size_run1, ==, executed_size_run1);

  /* Prefetch the blocks */
  g_hash_table_iter_init (&iter, compiled_run1);
  while (g_hash_table_iter_next (&iter, &iter_key, &iter_value))
  {
    gum_stalker_prefetch (fixture->stalker, iter_key, trust);
  }

  /* Run the child again */
  prefetch_run_child (fixture->stalker, fixture->runner_range,
      compile_pipes[STDOUT_FILENO], execute_pipes[STDOUT_FILENO]);

  /* Read the results */
  compiled_run2 = g_hash_table_new (NULL, NULL);
  prefetch_read_blocks (compile_pipes[STDIN_FILENO], compiled_run2);
  executed_run2 = g_hash_table_new (NULL, NULL);
  prefetch_read_blocks (execute_pipes[STDIN_FILENO], executed_run2);

  compiled_size_run2 = g_hash_table_size (compiled_run2);
  executed_size_run2 = g_hash_table_size (executed_run2);

  if (g_test_verbose ())
  {
    g_print ("\tcompiled2: %d\n", compiled_size_run2);
    g_print ("\texecuted2: %d\n", executed_size_run2);
  }

  g_assert_cmpuint (compiled_size_run2, ==, 0);
  g_assert_cmpuint (executed_size_run2, ==, executed_size_run1);

  /* Free resources */
  g_hash_table_unref (compiled_run2);
  g_hash_table_unref (executed_run2);
  g_hash_table_unref (compiled_run1);
  g_hash_table_unref (executed_run1);

  close (execute_pipes[STDIN_FILENO]);
  close (execute_pipes[STDOUT_FILENO]);
  close (compile_pipes[STDIN_FILENO]);
  close (compile_pipes[STDOUT_FILENO]);

  gum_stalker_unfollow_me (fixture->stalker);
  g_object_unref (sink);
}

static void
prefetch_on_event (const GumEvent * event,
                   GumCpuContext * cpu_context,
                   gpointer user_data)
{
  switch (event->type)
  {
    case GUM_COMPILE:
    {
      const GumCompileEvent * compile = &event->compile;

      if (prefetch_compiled != NULL)
        g_hash_table_add (prefetch_compiled, compile->start);

      break;
    }
    case GUM_BLOCK:
    {
      const GumBlockEvent * block = &event->block;

      if (prefetch_executed != NULL)
        g_hash_table_add (prefetch_executed, block->start);

      break;
    }
    default:
      break;
  }
}

static void
prefetch_run_child (GumStalker * stalker,
                    const GumMemoryRange * runner_range,
                    int compile_fd,
                    int execute_fd)
{
  pid_t pid;
  int res;
  int status;

  pid = fork ();
  g_assert_cmpint (pid, >=, 0);

  if (pid == 0)
  {
    /* Child */

    prefetch_compiled = g_hash_table_new (NULL, NULL);
    prefetch_executed = g_hash_table_new (NULL, NULL);

    gum_stalker_activate (stalker, prefetch_activation_target);
    prefetch_activation_target ();
    pretend_workload (runner_range);
    gum_stalker_unfollow_me (stalker);

    prefetch_write_blocks (compile_fd, prefetch_compiled);
    prefetch_write_blocks (execute_fd, prefetch_executed);

    exit (0);
  }

  /* Wait for the child */
  res = waitpid (pid, &status, 0);
  g_assert_cmpint (res, ==, pid);
  g_assert_cmpint (WIFEXITED (status), !=, 0);
  g_assert_cmpint (WEXITSTATUS (status), ==, 0);
}

GUM_NOINLINE static void
prefetch_activation_target (void)
{
  /* Avoid calls being optimized out */
  asm ("");
}

static void
prefetch_write_blocks (int fd,
                       GHashTable * table)
{
  GHashTableIter iter;
  gpointer iter_key, iter_value;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &iter_key, &iter_value))
  {
    int res = write (fd, &iter_key, sizeof (gpointer));
    g_assert_cmpint (res, ==, sizeof (gpointer));
  }
}

static void
prefetch_read_blocks (int fd,
                      GHashTable * table)
{
  gpointer block_address;

  while (read (fd, &block_address, sizeof (gpointer)) == sizeof (gpointer))
  {
    g_hash_table_add (table, block_address);
  }
}

TESTCASE (observer)
{
  GumTestStalkerObserver * test_observer;
  GumStalkerObserver * observer;
  guint sum, i;

  test_observer = g_object_new (GUM_TYPE_TEST_STALKER_OBSERVER, NULL);

  observer = GUM_STALKER_OBSERVER (test_observer);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  gum_stalker_deactivate (fixture->stalker);

  gum_stalker_set_observer (fixture->stalker, observer);

  gum_stalker_activate (fixture->stalker, prefetch_activation_target);
  prefetch_activation_target ();

  sum = 0;
  for (i = 0; i != 10; i++)
    sum += i;

  gum_stalker_unfollow_me (fixture->stalker);

  if (g_test_verbose ())
    g_print ("total: %" G_GINT64_MODIFIER "u\n", test_observer->total);

  g_assert_cmpuint (sum, ==, 45);
  g_assert_cmpuint (test_observer->total, !=, 0);
}

static void
gum_test_stalker_observer_iface_init (gpointer g_iface,
                                      gpointer iface_data)
{
  GumStalkerObserverInterface * iface = g_iface;

  iface->increment_total = gum_test_stalker_observer_increment_total;
}

static void
gum_test_stalker_observer_class_init (GumTestStalkerObserverClass * klass)
{
}

static void
gum_test_stalker_observer_init (GumTestStalkerObserver * self)
{
}

static void
gum_test_stalker_observer_increment_total (GumStalkerObserver * observer)
{
  GUM_TEST_STALKER_OBSERVER (observer)->total++;
}

#endif

TESTCASE (run_on_thread_current)
{
  GumThreadId thread_id;
  RunOnThreadCtx ctx;
  gboolean accepted;

  thread_id = gum_process_get_current_thread_id ();
  ctx.caller_id = thread_id;
  ctx.thread_id = G_MAXSIZE;

  accepted = gum_stalker_run_on_thread (fixture->stalker, thread_id,
      run_on_thread, &ctx, NULL);
  g_assert_true (accepted);
  g_assert_cmpuint (ctx.thread_id, ==, thread_id);
}

TESTCASE (run_on_thread_current_sync)
{
  GumThreadId thread_id;
  RunOnThreadCtx ctx;
  gboolean accepted;

  thread_id = gum_process_get_current_thread_id ();
  ctx.caller_id = thread_id;
  ctx.thread_id = G_MAXSIZE;

  accepted = gum_stalker_run_on_thread_sync (fixture->stalker, thread_id,
      run_on_thread, &ctx);
  g_assert_true (accepted);
  g_assert_cmpuint (thread_id, ==, ctx.thread_id);
}

static void
run_on_thread (const GumCpuContext * cpu_context,
               gpointer user_data)
{
  RunOnThreadCtx * ctx = user_data;

  g_usleep (250000);
  ctx->thread_id = gum_process_get_current_thread_id ();

  if (ctx->thread_id == ctx->caller_id)
    g_assert_null (cpu_context);
  else
    g_assert_nonnull (cpu_context);
}

TESTCASE (run_on_thread_other)
{
  GThread * thread;
  gboolean done = FALSE;
  GumThreadId other_id, this_id;
  RunOnThreadCtx ctx;
  gboolean accepted;

  thread = create_sleeping_dummy_thread_sync (&done, &other_id);

  this_id = gum_process_get_current_thread_id ();
  g_assert_cmphex (this_id, !=, other_id);
  ctx.caller_id = this_id;
  ctx.thread_id = G_MAXSIZE;

  accepted = gum_stalker_run_on_thread (fixture->stalker, other_id,
      run_on_thread, &ctx, NULL);
  g_assert_true (accepted);
  done = TRUE;
  g_thread_join (thread);
  g_assert_cmphex (ctx.thread_id, ==, other_id);
}

TESTCASE (run_on_thread_other_sync)
{
  GThread * thread;
  gboolean done = FALSE;
  GumThreadId other_id, this_id;
  RunOnThreadCtx ctx;
  gboolean accepted;

  thread = create_sleeping_dummy_thread_sync (&done, &other_id);

  this_id = gum_process_get_current_thread_id ();
  g_assert_cmphex (this_id, !=, other_id);
  ctx.caller_id = this_id;
  ctx.thread_id = G_MAXSIZE;

  accepted = gum_stalker_run_on_thread_sync (fixture->stalker, other_id,
      run_on_thread, &ctx);
  g_assert_true (accepted);
  done = TRUE;
  g_thread_join (thread);
  g_assert_cmpuint (ctx.thread_id, ==, other_id);
}

static GThread *
create_sleeping_dummy_thread_sync (gboolean * done,
                                   GumThreadId * thread_id)
{
  GThread * thread;
  TestThreadSyncData sync_data;

  g_mutex_init (&sync_data.mutex);
  g_cond_init (&sync_data.cond);
  sync_data.started = FALSE;
  sync_data.thread_id = 0;
  sync_data.done = done;

  g_mutex_lock (&sync_data.mutex);

  thread = g_thread_new ("sleepy", sleeping_dummy, &sync_data);

  while (!sync_data.started)
    g_cond_wait (&sync_data.cond, &sync_data.mutex);

  *thread_id = sync_data.thread_id;

  g_mutex_unlock (&sync_data.mutex);

  g_cond_clear (&sync_data.cond);
  g_mutex_clear (&sync_data.mutex);

  return thread;
}

static gpointer
sleeping_dummy (gpointer data)
{
  TestThreadSyncData * sync_data = data;
  gboolean * done = sync_data->done;

  g_mutex_lock (&sync_data->mutex);
  sync_data->started = TRUE;
  sync_data->thread_id = gum_process_get_current_thread_id ();
  g_cond_signal (&sync_data->cond);
  g_mutex_unlock (&sync_data->mutex);

  while (!(*done))
    g_thread_yield ();

  return NULL;
}

#include "../stalker-exceptions.c"
