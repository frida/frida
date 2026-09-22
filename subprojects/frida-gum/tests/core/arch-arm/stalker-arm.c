/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2017 Antonio Ken Iannillo <ak.iannillo@gmail.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "stalker-arm-fixture.c"

#ifdef HAVE_LINUX
# include <errno.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/wait.h>
#endif

TESTLIST_BEGIN (stalker)
  TESTENTRY (trust_should_be_one_by_default)

  TESTENTRY (arm_no_events)
  TESTENTRY (thumb_no_events)
  TESTENTRY (arm_exec_events_generated)
  TESTENTRY (thumb_exec_events_generated)
  TESTENTRY (arm_call_events_generated)
  TESTENTRY (thumb_call_events_generated)
  TESTENTRY (arm_block_events_generated)
  TESTENTRY (thumb_block_events_generated)
  TESTENTRY (arm_nested_call_events_generated)
  TESTENTRY (thumb_nested_call_events_generated)
  TESTENTRY (arm_nested_ret_events_generated)
  TESTENTRY (thumb_nested_ret_events_generated)
  TESTENTRY (arm_unmodified_lr)
  TESTENTRY (thumb_unmodified_lr)
  TESTENTRY (arm_excluded_range)
  TESTENTRY (thumb_excluded_range)
  TESTENTRY (arm_excluded_range_call_events)
  TESTENTRY (thumb_excluded_range_call_events)
  TESTENTRY (arm_excluded_range_ret_events)
  TESTENTRY (thumb_excluded_range_ret_events)
  TESTENTRY (arm_pop_pc_ret_events_generated)
  TESTENTRY (thumb_pop_pc_ret_events_generated)
  TESTENTRY (arm_pop_just_pc_ret_events_generated)
  TESTENTRY (thumb_pop_just_pc_ret_events_generated)
  TESTENTRY (thumb_pop_just_pc2_ret_events_generated)
  TESTENTRY (arm_ldm_pc_ret_events_generated)
  TESTENTRY (thumb_ldm_pc_ret_events_generated)
  TESTENTRY (arm_branch_cc_block_events_generated)
  TESTENTRY (thumb_branch_cc_block_events_generated)

  TESTENTRY (thumb_cbz_cbnz_block_events_generated)

  TESTENTRY (thumb2_mov_pc_reg_exec_events_generated)
  TESTENTRY (thumb2_mov_pc_reg_without_thumb_bit_set)
  TESTENTRY (thumb2_mov_pc_reg_no_clobber_reg)

  /*
   * The following tests have no Thumb equivalent as Thumb does not support
   * conditional instructions nor is PC allowed as the destination register
   * for some opcodes.
   */
  TESTENTRY (arm_branch_link_cc_block_events_generated)
  TESTENTRY (arm_cc_excluded_range)
  TESTENTRY (arm_ldr_pc)
  TESTENTRY (arm_ldr_pc_pre_index_imm)
  TESTENTRY (arm_ldr_pc_post_index_imm)
  TESTENTRY (arm_ldr_pc_pre_index_imm_negative)
  TESTENTRY (arm_ldr_pc_post_index_imm_negative)
  TESTENTRY (arm_ldr_pc_shift)
  TESTENTRY (arm_sub_pc)
  TESTENTRY (arm_add_pc)
  TESTENTRY (arm_ldmia_pc)

  TESTENTRY (thumb_it_eq)
  TESTENTRY (thumb_it_al)
  TESTENTRY (thumb_it_eq_branch)
  TESTENTRY (thumb_itt_eq_branch)
  TESTENTRY (thumb_ite_eq_branch)
  TESTENTRY (thumb_it_eq_branch_link)
  TESTENTRY (thumb_it_eq_branch_link_excluded)
  TESTENTRY (thumb_it_eq_pop)
  TESTENTRY (thumb_itttt_eq_blx_reg)
  TESTENTRY (thumb_it_flags)
  TESTENTRY (thumb_it_flags2)
  TESTENTRY (thumb_tbb)
  TESTENTRY (thumb_tbh)

  TESTENTRY (arm_call_probe)
  TESTENTRY (thumb_call_probe)

  TESTENTRY (self_modifying_code_should_be_detected_with_threshold_minus_one)
  TESTENTRY (self_modifying_code_should_not_be_detected_with_threshold_zero)
  TESTENTRY (self_modifying_code_should_be_detected_with_threshold_one)

  TESTENTRY (call_thumb)
  TESTENTRY (branch_thumb)
  TESTENTRY (can_follow_workload)
  TESTENTRY (performance)

  TESTENTRY (custom_transformer)
  TESTENTRY (arm_callout)
  TESTENTRY (thumb_callout)
  TESTENTRY (arm_transformer_should_be_able_to_replace_call_with_callout)
  TESTENTRY (arm_transformer_should_be_able_to_replace_jumpout_with_callout)
  TESTENTRY (arm_many_callouts_should_not_overflow_literal_pool)
  TESTENTRY (thumb_many_callouts_should_not_overflow_literal_pool)
  TESTENTRY (unfollow_should_be_allowed_before_first_transform)
  TESTENTRY (unfollow_should_be_allowed_mid_first_transform)
  TESTENTRY (unfollow_should_be_allowed_after_first_transform)
  TESTENTRY (unfollow_should_be_allowed_before_second_transform)
  TESTENTRY (unfollow_should_be_allowed_mid_second_transform)
  TESTENTRY (unfollow_should_be_allowed_after_second_transform)
  TESTENTRY (follow_me_should_support_nullable_event_sink)
  TESTENTRY (arm_invalidation_for_current_thread_should_be_supported)
  TESTENTRY (thumb_invalidation_for_current_thread_should_be_supported)
  TESTENTRY (arm_invalidation_for_specific_thread_should_be_supported)
  TESTENTRY (thumb_invalidation_for_specific_thread_should_be_supported)
  TESTENTRY (arm_invalidation_should_allow_block_to_grow)
  TESTENTRY (thumb_invalidation_should_allow_block_to_grow)

  TESTENTRY (arm_exclusive_load_store_should_not_be_disturbed)
  TESTENTRY (thumb_exclusive_load_store_should_not_be_disturbed)

  TESTENTRY (follow_syscall)
  TESTENTRY (follow_thread)
  TESTENTRY (unfollow_should_handle_terminated_thread)
  TESTENTRY (pthread_create)
  TESTENTRY (heap_api)

#ifdef HAVE_LINUX
  TESTENTRY (prefetch)
#endif

  TESTGROUP_BEGIN ("RunOnThread")
    TESTENTRY (run_on_thread_current)
    TESTENTRY (run_on_thread_current_sync)
    TESTENTRY (run_on_thread_other)
    TESTENTRY (run_on_thread_other_sync)
  TESTGROUP_END ()
TESTLIST_END ()

typedef struct _CodePatcher CodePatcher;
typedef struct _RunOnThreadCtx RunOnThreadCtx;
typedef struct _TestThreadSyncData TestThreadSyncData;

struct _CodePatcher
{
  GThread * thread;
  volatile gint request;
  volatile gint done;
  volatile gboolean stop;
  GumAddress code;
  guint offset;
  GumAddress value;
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

static void code_patcher_start (CodePatcher * self);
static void code_patcher_stop (CodePatcher * self);
static void patch_code_pointer_on_thread (CodePatcher * patcher,
    GumAddress code, guint offset, GumAddress value);
static gpointer code_patcher_worker (gpointer data);

static guint32 pretend_workload (const GumMemoryRange * runner_range);
static guint32 crc32b (const guint8 * message, gsize size);
static gboolean test_log_fatal_func (const gchar * log_domain,
    GLogLevelFlags log_level, const gchar * message, gpointer user_data);
static GLogWriterOutput test_log_writer_func (GLogLevelFlags log_level,
    const GLogField * fields, gsize n_fields, gpointer user_data);
static void duplicate_adds (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void transform_arm_return_value (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void on_arm_ret (GumCpuContext * cpu_context, gpointer user_data);
static gboolean is_arm_mov_pc_lr (const guint8 * bytes, gsize size);
static void transform_thumb_return_value (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void on_thumb_ret (GumCpuContext * cpu_context,
    gpointer user_data);
static gboolean is_thumb_pop_pc (const guint8 * bytes, gsize size);
static void replace_call_with_callout (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void replace_jumpout_with_callout (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void callout_set_cool (GumCpuContext * cpu_context, gpointer user_data);
static void add_callout_for_every_instruction (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void bump_num_callouts (GumCpuContext * cpu_context,
    gpointer user_data);
static void unfollow_during_transform (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void test_invalidation_for_current_thread_with_target (GumAddress target,
    TestArmStalkerFixture * fixture);
static void modify_to_return_true_after_three_calls (
    GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void invalidate_after_three_calls (GumCpuContext * cpu_context,
    gpointer user_data);
static void test_invalidation_for_specific_thread_with_target (
    GumAddress target, TestArmStalkerFixture * fixture);
static void start_invalidation_target (InvalidationTarget * target,
    gconstpointer target_function, TestArmStalkerFixture * fixture);
static void join_invalidation_target (InvalidationTarget * target);
static gpointer run_stalked_until_finished (gpointer data);
static void modify_to_return_true_on_subsequent_transform (
    GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void test_invalidation_block_growth_with_target (GumAddress target,
    TestArmStalkerFixture * fixture);
static void add_n_return_value_increments (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void insert_callout_after_cmp (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void bump_num_cmp_callouts (GumCpuContext * cpu_context,
    gpointer user_data);
static gpointer run_stalked_briefly (gpointer data);
static gpointer run_stalked_into_termination (gpointer data);
static gpointer increment_integer (gpointer data);
static void patch_code_pointer (GumAddress code, guint offset,
    GumAddress value);
static void patch_code_pointer_slot (gpointer mem, gpointer user_data);

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
#endif

static void run_on_thread (const GumCpuContext * cpu_context,
    gpointer user_data);
static GThread * create_sleeping_dummy_thread_sync (gboolean * done,
    GumThreadId * thread_id);
static gpointer sleeping_dummy (gpointer data);

TESTCASE (trust_should_be_one_by_default)
{
  g_assert_cmpuint (gum_stalker_get_trust_threshold (fixture->stalker), ==, 1);
}

TESTCASE (deactivate_unsupported)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
      "Activate/deactivate unsupported");
  gum_stalker_deactivate (fixture->stalker);
  g_test_assert_expected_messages ();
}

TESTCASE (activate_unsupported)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
      "Activate/deactivate unsupported");
  gum_stalker_activate (fixture->stalker, NULL);
  g_test_assert_expected_messages ();
}

TESTCODE (arm_flat_code,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0  */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1   */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr      */
);

TESTCODE (thumb_flat_code,
  0x00, 0xb5,             /* push {lr}       */
  0x00, 0x1a,             /* subs r0, r0, r0 */
  0x01, 0x30,             /* adds r0, 1      */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xbd              /* pop {pc}        */
);

TESTCASE (arm_no_events)
{
  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCASE (thumb_no_events)
{
  INVOKE_THUMB_EXPECTING (GUM_NOTHING, thumb_flat_code, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCASE (arm_exec_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_EXEC, arm_flat_code, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_INSN_COUNT + (CODE_SIZE (arm_flat_code) / 4));

  GUM_ASSERT_EVENT_ADDR (exec, 0, location,
      fixture->invoker + INVOKER_IMPL_OFFSET);
  GUM_ASSERT_EVENT_ADDR (exec, 1, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 4);

  GUM_ASSERT_EVENT_ADDR (exec, 2, location, func);
  GUM_ASSERT_EVENT_ADDR (exec, 3, location, func + 4);
  GUM_ASSERT_EVENT_ADDR (exec, 4, location, func + 8);
  GUM_ASSERT_EVENT_ADDR (exec, 5, location, func + 12);

  GUM_ASSERT_EVENT_ADDR (exec, 6, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 8);
  GUM_ASSERT_EVENT_ADDR (exec, 7, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 12);
  GUM_ASSERT_EVENT_ADDR (exec, 8, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 16);
  GUM_ASSERT_EVENT_ADDR (exec, 9, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 20);
}

TESTCASE (thumb_exec_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_EXEC, thumb_flat_code, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_INSN_COUNT + (CODE_SIZE (thumb_flat_code) / 2));

  GUM_ASSERT_EVENT_ADDR (exec, 0, location,
      fixture->invoker + INVOKER_IMPL_OFFSET);
  GUM_ASSERT_EVENT_ADDR (exec, 1, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 4);

  GUM_ASSERT_EVENT_ADDR (exec, 2, location, func + 0 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 3, location, func + 2 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 4, location, func + 4 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 5, location, func + 6 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 6, location, func + 8 + 1);

  GUM_ASSERT_EVENT_ADDR (exec, 7, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 8);
  GUM_ASSERT_EVENT_ADDR (exec, 8, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 12);
  GUM_ASSERT_EVENT_ADDR (exec, 9, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 16);
  GUM_ASSERT_EVENT_ADDR (exec, 10, location,
      fixture->invoker + INVOKER_IMPL_OFFSET + 20);
}

TESTCASE (arm_call_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_CALL, arm_flat_code, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_CALL_INSN_COUNT);

  GUM_ASSERT_EVENT_ADDR (call, 0, target, func);
  GUM_ASSERT_EVENT_ADDR (call, 0, depth, 0);
}

TESTCASE (thumb_call_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_CALL, thumb_flat_code, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_CALL_INSN_COUNT);

  GUM_ASSERT_EVENT_ADDR (call, 0, target, func + 1);
  GUM_ASSERT_EVENT_ADDR (call, 0, depth, 0);
}

TESTCODE (arm_block_events,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x00, 0x00, 0x00, 0xea, /* b beach        */

  0xf0, 0x00, 0xf0, 0xe7, /* udf 0          */

  /* beach:                                 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr     */
);

TESTCASE (arm_block_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_BLOCK, arm_block_events, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + (3 * 4));
}

TESTCODE (thumb_block_events,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x01, 0x30, /* adds r0, 1      */
  0x00, 0xe0, /* b beach         */

  0x00, 0xde, /* udf 0           */

  /* beach:                      */
  0x01, 0x30, /* adds r0, 1      */
  0x00, 0xbd  /* pop {pc}        */
);

TESTCASE (thumb_block_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_BLOCK, thumb_block_events, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + (4 * 2) + 1);
}

TESTCODE (arm_nested_call,
  0x00, 0x40, 0x2d, 0xe9, /* stmdb sp!, {lr} */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0  */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1   */
  0x02, 0x00, 0x00, 0xeb, /* bl func_a       */
  0x06, 0x00, 0x00, 0xeb, /* bl func_b       */
  0x00, 0x40, 0xbd, 0xe8, /* ldm sp!, {lr}   */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr      */

  /* func_a:                                 */
  0x00, 0x40, 0x2d, 0xe9, /* stmdb sp!, {lr} */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1   */
  0x01, 0x00, 0x00, 0xeb, /* bl func_b       */
  0x00, 0x40, 0xbd, 0xe8, /* ldm sp!, {lr}   */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr      */

  /* func_b:                                 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1   */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr      */
);

TESTCASE (arm_nested_call_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_CALL, arm_nested_call, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_CALL_INSN_COUNT + 3);

  GUM_ASSERT_EVENT_ADDR (call, 0, target, func);
  GUM_ASSERT_EVENT_ADDR (call, 0, depth, 0);

  GUM_ASSERT_EVENT_ADDR (call, 1, location, func + (3 * 4));
  GUM_ASSERT_EVENT_ADDR (call, 1, target, func + (7 * 4));
  GUM_ASSERT_EVENT_ADDR (call, 1, depth, 1);

  GUM_ASSERT_EVENT_ADDR (call, 2, location, func + (9 * 4));
  GUM_ASSERT_EVENT_ADDR (call, 2, target, func + (12 * 4));
  GUM_ASSERT_EVENT_ADDR (call, 2, depth, 2);

  GUM_ASSERT_EVENT_ADDR (call, 3, location, func + (4 * 4));
  GUM_ASSERT_EVENT_ADDR (call, 3, target, func + (12 * 4));
  GUM_ASSERT_EVENT_ADDR (call, 3, depth, 1);
}

TESTCODE (thumb_nested_call,
  0x00, 0xb5,             /* push {lr}       */
  0x00, 0x1a,             /* subs r0, r0, r0 */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xf0, 0x03, 0xf8, /* bl func_a       */
  0x00, 0xf0, 0x06, 0xf8, /* bl func_b       */
  0x00, 0xbd,             /* pop {pc}        */

  /* func_a:                                 */
  0x00, 0xb5,             /* push {lr}       */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xf0, 0x01, 0xf8, /* bl func_b       */
  0x00, 0xbd,             /* pop {pc}        */

  /* func_b:                                 */
  0x00, 0xb5,             /* push {lr}       */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xbd              /* pop {pc}        */
);

TESTCASE (thumb_nested_call_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_CALL, thumb_nested_call, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_CALL_INSN_COUNT + 3);

  GUM_ASSERT_EVENT_ADDR (call, 0, target, func + 1);
  GUM_ASSERT_EVENT_ADDR (call, 0, depth, 0);

  GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 6 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 1, depth, 1);

  GUM_ASSERT_EVENT_ADDR (call, 2, location, func + 20 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 2, target, func + 26 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 2, depth, 2);

  GUM_ASSERT_EVENT_ADDR (call, 3, location, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 3, target, func + 26 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 3, depth, 1);
}

TESTCASE (arm_nested_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_RET, arm_nested_call, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==, 4);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + (13 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + (10 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 2);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + (11 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 1, target, func + (4 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 2, location, func + (13 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 2, target, func + (5 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 2, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 3, location, func + (6 * 4));
  GUM_ASSERT_EVENT_ADDR (ret, 3, depth, 0);
}

TESTCASE (thumb_nested_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_nested_call, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==, 4);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 30 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 24 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 2);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 24 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 1, target, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 2, location, func + 30 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 2, target, func + 14 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 2, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 3, location, func + 14 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 3, depth, 0);
}

typedef struct _CallProbeContext CallProbeContext;

struct _CallProbeContext
{
  guint num_calls;
  gpointer target_address;
  gpointer return_address;
};

static void test_call_probe (GumAddress func, GumAddress func_a,
    GumAddress return_address, TestArmStalkerFixture * fixture);
static void probe_func_a_invocation (GumCallDetails * details,
    gpointer user_data);

TESTCODE (arm_call_probe,
  0x00, 0x44, 0x2d, 0xe9, /* push {r10, lr}   */
  0xaa, 0xa0, 0xa0, 0xe3, /* mov r10, 0xaa    */
  0x44, 0x30, 0xa0, 0xe3, /* mov r3, 0x44     */
  0x33, 0x20, 0xa0, 0xe3, /* mov r2, 0x33     */
  0x22, 0x10, 0xa0, 0xe3, /* mov r1, 0x22     */
  0x11, 0x00, 0xa0, 0xe3, /* mov r0, 0x11     */
  0x03, 0x00, 0x2d, 0xe9, /* push {r0, r1}    */
  0x06, 0x00, 0x00, 0xeb, /* bl func_a        */
  0x03, 0x00, 0xbd, 0xe8, /* pop {r0, r1}     */
  0x88, 0x30, 0xa0, 0xe3, /* mov r3, 0x88     */
  0x77, 0x20, 0xa0, 0xe3, /* mov r2, 0x77     */
  0x66, 0x10, 0xa0, 0xe3, /* mov r1, 0x66     */
  0x55, 0x00, 0xa0, 0xe3, /* mov r0, 0x55     */
  0x02, 0x00, 0x00, 0xeb, /* bl func_b        */
  0x00, 0x84, 0xbd, 0xe8, /* pop {r10, pc}    */

  /* func_a: */
  0x88, 0x00, 0xa0, 0xe3, /* mov r0, 0x88     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr       */

  /* func_b: */
  0x99, 0x00, 0xa0, 0xe3, /* mov r0, 0x99     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr       */
);

TESTCODE (thumb_call_probe,
  0x2d, 0xe9, 0x00, 0x44, /* push.w {r10, lr} */
  0x4f, 0xf0, 0xaa, 0x0a, /* mov.w r10, 0xaa  */
  0x4f, 0xf0, 0x44, 0x03, /* mov.w r3, 0x44   */
  0x4f, 0xf0, 0x33, 0x02, /* mov.w r2, 0x33   */
  0x4f, 0xf0, 0x22, 0x01, /* mov.w r1, 0x22   */
  0x4f, 0xf0, 0x11, 0x00, /* mov.w r0, 0x11   */
  0x03, 0xb4,             /* push {r0, r1}    */
  0x00, 0xf0, 0x0d, 0xf8, /* bl func_a        */
  0x03, 0xbc,             /* pop {r0, r1}     */
  0x4f, 0xf0, 0x88, 0x03, /* mov.w r3, 0x88   */
  0x4f, 0xf0, 0x77, 0x02, /* mov.w r2, 0x77   */
  0x4f, 0xf0, 0x66, 0x01, /* mov.w r1, 0x66   */
  0x4f, 0xf0, 0x55, 0x00, /* mov.w r0, 0x55   */
  0x00, 0xf0, 0x05, 0xf8, /* bl func_b        */
  0xbd, 0xe8, 0x00, 0x84, /* pop.w {r10, pc}  */

  /* func_a: */
  0x4f, 0xf0, 0x88, 0x00, /* mov.w r0, 0x88   */
  0x70, 0x47,             /* bx lr            */

  /* func_b: */
  0x4f, 0xf0, 0x99, 0x00, /* mov.w r0, 0x99   */
  0x70, 0x47,             /* bx lr            */
);

TESTCASE (arm_call_probe)
{
  GumAddress func = DUP_TESTCODE (arm_call_probe);

  test_call_probe (
      func,
      func + 15 * 4,
      func + 8 * 4,
      fixture);
}

TESTCASE (thumb_call_probe)
{
  GumAddress func = DUP_TESTCODE (thumb_call_probe);

  test_call_probe (
      func + 1,
      func + 56 + 1,
      func + 30 + 1,
      fixture);
}

static void
test_call_probe (GumAddress func,
                 GumAddress func_a,
                 GumAddress return_address,
                 TestArmStalkerFixture * fixture)
{
  gpointer func_a_ptr;
  CallProbeContext probe_ctx, secondary_probe_ctx;
  GumProbeId probe_id;

  func_a_ptr = GSIZE_TO_POINTER (func_a);

  probe_ctx.num_calls = 0;
  probe_ctx.target_address = func_a_ptr;
  probe_ctx.return_address = GSIZE_TO_POINTER (return_address);
  probe_id = gum_stalker_add_call_probe (fixture->stalker, func_a_ptr,
      probe_func_a_invocation, &probe_ctx, NULL);
  FOLLOW_AND_INVOKE (func);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 1);

  secondary_probe_ctx.num_calls = 0;
  secondary_probe_ctx.target_address = probe_ctx.target_address;
  secondary_probe_ctx.return_address = probe_ctx.return_address;
  gum_stalker_add_call_probe (fixture->stalker, func_a_ptr,
      probe_func_a_invocation, &secondary_probe_ctx, NULL);
  FOLLOW_AND_INVOKE (func);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 2);
  g_assert_cmpuint (secondary_probe_ctx.num_calls, ==, 1);

  gum_stalker_remove_call_probe (fixture->stalker, probe_id);
  FOLLOW_AND_INVOKE (func);
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

  g_assert_cmphex (cpu_context->pc, ==,
      GPOINTER_TO_SIZE (ctx->target_address) & ~1);
  g_assert_cmphex (cpu_context->lr, ==,
      GPOINTER_TO_SIZE (ctx->return_address));
  g_assert_cmphex (cpu_context->r[0], ==, 0x11);
  g_assert_cmphex (cpu_context->r[1], ==, 0x22);
  g_assert_cmphex (cpu_context->r[2], ==, 0x33);
  g_assert_cmphex (cpu_context->r[3], ==, 0x44);
  g_assert_cmphex (cpu_context->r10, ==, 0xaa);
}

TESTCODE (arm_unmodified_lr,
  0x00, 0x40, 0x2d, 0xe9, /* stmdb sp!, {lr} */
  0x00, 0x00, 0x00, 0xeb, /* bl part_two     */

  0xec, 0xec, 0xec, 0xec,

  /* part_two:                               */
  0x00, 0x00, 0x9e, 0xe5, /* ldr r0, [lr]    */
  0x00, 0x40, 0xbd, 0xe8, /* ldm sp!, {lr}   */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr      */
);

TESTCASE (arm_unmodified_lr)
{
  INVOKE_ARM_EXPECTING (0, arm_unmodified_lr, 0xecececec);
}

TESTCODE (thumb_unmodified_lr,
  0x00, 0xb5,             /* push {lr}       */
  0x00, 0xf0, 0x02, 0xf8, /* bl part_two     */

  0xec, 0xec, 0xec, 0xec,

  /* part_two:                               */
  0x49, 0x1a,             /* subs r1, r1, r1 */
  0x01, 0x31,             /* adds r1, 1      */
  0x70, 0x46,             /* mov r0, lr      */
  0x88, 0x43,             /* bics r0, r1     */
  0x00, 0x68,             /* ldr r0, [r0]    */
  0x00, 0xbd              /* pop {pc}        */
);

TESTCASE (thumb_unmodified_lr)
{
  INVOKE_THUMB_EXPECTING (0, thumb_unmodified_lr, 0xecececec);
}

TESTCODE (arm_excluded_range,
  0x00, 0x40, 0x2d, 0xe9, /* stmdb sp!, {lr}  */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x01, 0x00, 0x00, 0xeb, /* bl excluded_func */
  0x00, 0x40, 0xbd, 0xe8, /* ldm sp!, {lr}    */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr       */

  /* excluded_func:                           */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr       */
);

TESTCASE (arm_excluded_range)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_excluded_range);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 24,
      .size = 8
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_EXEC;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 2);

    g_assert_cmpuint (fixture->sink->events->length, ==,
        INVOKER_INSN_COUNT + 6);

    GUM_ASSERT_EVENT_ADDR (exec, 2, location, func);
    GUM_ASSERT_EVENT_ADDR (exec, 3, location, func + 4);
    GUM_ASSERT_EVENT_ADDR (exec, 4, location, func + 8);
    GUM_ASSERT_EVENT_ADDR (exec, 5, location, func + 12);
    GUM_ASSERT_EVENT_ADDR (exec, 6, location, func + 16);
    GUM_ASSERT_EVENT_ADDR (exec, 7, location, func + 20);
  }
}

TESTCODE (thumb_excluded_range,
  0x00, 0xb5,             /* push {lr}        */
  0x00, 0x1a,             /* subs r0, r0, r0  */
  0x01, 0x30,             /* adds r0, 1       */
  0x00, 0xf0, 0x01, 0xf8, /* bl excluded_func */
  0x00, 0xbd,             /* pop {pc}         */

  /* excluded_func:                           */
  0x00, 0xb5,             /* push {lr}        */
  0x01, 0x30,             /* adds r0, 1       */
  0x00, 0xbd              /* pop {pc}         */
);

TESTCASE (thumb_excluded_range)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb_excluded_range);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 12,
      .size = 6
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_EXEC;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 2);

    g_assert_cmpuint (fixture->sink->events->length, ==,
        INVOKER_INSN_COUNT + 5);

    GUM_ASSERT_EVENT_ADDR (exec, 2, location, func +  0 + 1);
    GUM_ASSERT_EVENT_ADDR (exec, 3, location, func +  2 + 1);
    GUM_ASSERT_EVENT_ADDR (exec, 4, location, func +  4 + 1);
    GUM_ASSERT_EVENT_ADDR (exec, 5, location, func +  6 + 1);
    GUM_ASSERT_EVENT_ADDR (exec, 6, location, func + 10 + 1);
  }
}

TESTCODE (arm_excluded_range_call,
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}         */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0    */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1     */
  0x05, 0x00, 0x00, 0xeb, /* bl func_c         */
  0x00, 0x00, 0x00, 0xeb, /* bl func_a         */
  0x04, 0xf0, 0x9d, 0xe4, /* pop {pc}          */

  /* func_a:                                   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr        */

  /* func_b:                                   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr        */

  /* func_c:                                   */
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}         */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1     */
  0xfa, 0xff, 0xff, 0xeb, /* bl func_b         */
  0x04, 0xf0, 0x9d, 0xe4  /* pop {pc}          */
);

TESTCASE (arm_excluded_range_call_events)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_excluded_range_call);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 40,
      .size = 16
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_CALL;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 4);

    g_assert_cmpuint (fixture->sink->events->length, ==,
        INVOKER_CALL_INSN_COUNT + 2);

    GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 12);
    GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 40);
    GUM_ASSERT_EVENT_ADDR (call, 1, depth, 1);

    GUM_ASSERT_EVENT_ADDR (call, 2, location, func + 16);
    GUM_ASSERT_EVENT_ADDR (call, 2, target, func + 24);
    GUM_ASSERT_EVENT_ADDR (call, 2, depth, 1);
  }
}

TESTCODE (thumb_excluded_range_call,
  0x00, 0xb5,             /* push {lr}       */
  0x00, 0x1a,             /* subs r0, r0, r0 */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xf0, 0x09, 0xf8, /* bl func_c       */
  0x00, 0xf0, 0x01, 0xf8, /* bl func_a       */
  0x00, 0xbd,             /* pop {pc}        */

  /* func_a:                                 */
  0x00, 0xb5,             /* push {lr}       */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xbd,             /* pop {pc}        */

  /* func_b:                                 */
  0x00, 0xb5,             /* push {lr}       */
  0x01, 0x30,             /* adds r0, 1      */
  0x00, 0xbd,             /* pop {pc}        */

  /* func_c:                                 */
  0x00, 0xb5,             /* push {lr}       */
  0x01, 0x30,             /* adds r0, 1      */
  0xff, 0xf7, 0xf9, 0xff, /* bl func_b       */
  0x00, 0xbd              /* pop {pc}        */
);

TESTCASE (thumb_excluded_range_call_events)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb_excluded_range_call);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 28,
      .size = 10
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_CALL;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 4);

    g_assert_cmpuint (fixture->sink->events->length, ==,
        INVOKER_CALL_INSN_COUNT + 2);

    GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 6 + 1);
    GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 28 + 1);
    GUM_ASSERT_EVENT_ADDR (call, 1, depth, 1);

    GUM_ASSERT_EVENT_ADDR (call, 2, location, func + 10 + 1);
    GUM_ASSERT_EVENT_ADDR (call, 2, target, func + 16 + 1);
    GUM_ASSERT_EVENT_ADDR (call, 2, depth, 1);
  }
}

TESTCASE (arm_excluded_range_ret_events)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_excluded_range_call);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 40,
      .size = 16
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_RET;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 4);

    g_assert_cmpuint (fixture->sink->events->length, ==, 2);

    GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 28);
    GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 20);
    GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

    GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 20);
    GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
  }
}

TESTCASE (thumb_excluded_range_ret_events)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb_excluded_range_call);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 28,
      .size = 10
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_RET;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 4);

    g_assert_cmpuint (fixture->sink->events->length, ==, 2);

    GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 20 + 1);
    GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 14 + 1);
    GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

    GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 14 + 1);
    GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
  }
}

TESTCODE (arm_pop_pc,
  0xf0, 0x41, 0x2d, 0xe9, /* push {r4-r8, lr} */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x00, 0x00, 0x00, 0xeb, /* bl inner         */
  0xf0, 0x81, 0xbd, 0xe8, /* pop {r4-r8, pc}  */

  /* inner:                                   */
  0x0e, 0x40, 0x2d, 0xe9, /* push {r1-r3, lr} */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x0e, 0x80, 0xbd, 0xe8  /* pop {r1-r3, pc}  */
);

TESTCASE (arm_pop_pc_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_RET, arm_pop_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 28);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 16);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 16);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
}

TESTCODE (thumb_pop_pc,
  0xf0, 0xb5,             /* push {r4-r7, lr} */
  0x00, 0x1a,             /* subs r0, r0, r0  */
  0x01, 0x30,             /* adds r0, 1       */
  0x00, 0xf0, 0x01, 0xf8, /* bl inner         */
  0xf0, 0xbd,             /* pop {r4-r7, pc}  */

  /* inner:                                   */
  0x0e, 0xb5,             /* push {r1-r3, lr} */
  0x01, 0x30,             /* adds r0, 1       */
  0x0e, 0xbd              /* pop {r1-r3, pc}  */
);

TESTCASE (thumb_pop_pc_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_pop_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
}

TESTCODE (arm_pop_just_pc,
  0xf0, 0x41, 0x2d, 0xe9, /* push {r4-r8, lr} */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x00, 0x00, 0x00, 0xeb, /* bl inner         */
  0xf0, 0x81, 0xbd, 0xe8, /* pop {r4-r8, pc}  */

  /* inner:                                   */
  0x00, 0x40, 0x2d, 0xe9, /* stmdb sp!, {lr}  */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x00, 0x80, 0xbd, 0xe8  /* ldm sp!, {pc}    */
);

TESTCASE (arm_pop_just_pc_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_RET, arm_pop_just_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 28);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 16);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 16);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
}

TESTCODE (thumb_pop_just_pc,
  0xf0, 0xb5,             /* push {r4-r7, lr} */
  0x00, 0x1a,             /* subs r0, r0, r0  */
  0x01, 0x30,             /* adds r0, 1       */
  0x00, 0xf0, 0x01, 0xf8, /* bl inner         */
  0xf0, 0xbd,             /* pop {r4-r7, pc}  */

  /* inner:                                   */
  0x00, 0xb5,             /* push {lr}        */
  0x01, 0x30,             /* adds r0, 1       */
  0x00, 0xbd              /* pop {pc}         */
);

TESTCASE (thumb_pop_just_pc_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_pop_just_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
}

TESTCODE (thumb_pop_just_pc2,
  0xf0, 0xb5,             /* push {r4-r7, lr} */
  0x00, 0x1a,             /* subs r0, r0, r0  */
  0x01, 0x30,             /* adds r0, 1       */
  0x00, 0xf0, 0x01, 0xf8, /* bl inner         */
  0xf0, 0xbd,             /* pop {r4-r7, pc}  */

  /* inner:                                   */
  0x00, 0xb5,             /* push {lr}        */
  0x01, 0x30,             /* adds r0, 1       */
  0x5d, 0xf8, 0x04, 0xfb, /* ldr pc, [sp], #4 */
);

TESTCASE (thumb_pop_just_pc2_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_pop_just_pc2, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 1);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 0);
}

TESTCODE (arm_ldm_pc,
  0xf0, 0x41, 0x2d, 0xe9, /* push {r4-r8, lr}       */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0         */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1          */
  0x00, 0x00, 0x00, 0xeb, /* bl inner               */
  0xf0, 0x81, 0xbd, 0xe8, /* pop {r4-r8, pc}        */

  /* inner:                                         */
  0x00, 0x30, 0x8d, 0xe2, /* add r3, sp, 0          */
  0xf0, 0x41, 0x23, 0xe9, /* stmdb r3!, {r4-r8, lr} */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1          */
  0xf0, 0x81, 0xb3, 0xe8  /* ldm r3!, {r4-r8, pc}   */
);

TESTCASE (arm_ldm_pc_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_RET, arm_ldm_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 32);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 16);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 16);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
}

TESTCODE (thumb_ldm_pc,
  0xf0, 0xb5,             /* push {r4-r7, lr}        */
  0x00, 0x1a,             /* subs r0, r0, r0         */
  0x01, 0x30,             /* adds r0, 1              */
  0x00, 0xf0, 0x01, 0xf8, /* bl inner                */
  0xf0, 0xbd,             /* pop {r4-r7, pc}         */

  /* inner:                                          */
  0x00, 0xab,             /* add r3, sp, 0           */
  0x23, 0xe9, 0x06, 0x40, /* stmdb r3!, {r1, r2, lr} */
  0x01, 0x30,             /* adds r0, 1              */
  0xb3, 0xe8, 0x06, 0x80  /* ldm.w r3!, {r1, r2, pc} */
);

TESTCASE (thumb_ldm_pc_ret_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_ldm_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 2);

  g_assert_cmpint (
      g_array_index (fixture->sink->events, GumEvent, 0).type, ==, GUM_RET);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 20 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, depth, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 1, depth, 0);
}

TESTCODE (arm_b_cc,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x01, 0x10, 0x41, 0xe0, /* sub r1, r1, r1 */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0      */
  0x00, 0x00, 0x00, 0x0a, /* beq after_a    */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  /* after_a:                               */

  0x01, 0x00, 0x51, 0xe3, /* cmp r1, 1      */
  0x00, 0x00, 0x00, 0x0a, /* beq after_b    */
  0x02, 0x00, 0x80, 0xe2, /* add r0, r0, 2  */
  /* after_b:                               */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0      */
  0x00, 0x00, 0x00, 0xaa, /* bge after_c    */
  0x04, 0x00, 0x80, 0xe2, /* add r0, r0, 4  */
  /* after_c:                               */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0      */
  0x00, 0x00, 0x00, 0xba, /* blt after_d    */
  0x08, 0x00, 0x80, 0xe2, /* add r0, r0, 8  */
  /* after_d:                               */

  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr     */
);

TESTCASE (arm_branch_cc_block_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_BLOCK, arm_b_cc, 10);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 4);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 16);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, start, func + 20);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, end, func + 28);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, start, func + 28);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, end, func + 40);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, start, func + 44);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, end, func + 52);
}

TESTCODE (thumb_b_cc,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x49, 0x1a, /* subs r1, r1, r1 */

  0x00, 0x29, /* cmp r1, 0       */
  0x00, 0xd0, /* beq after_a     */
  0x01, 0x30, /* adds r0, 1      */
  /* after_a:                    */

  0x01, 0x29, /* cmp r1, 1       */
  0x00, 0xd0, /* beq after_b     */
  0x02, 0x30, /* adds r0, 2      */
  /* after_b:                    */

  0x00, 0x29, /* cmp r1, 0       */
  0x00, 0xda, /* bge after_c     */
  0x04, 0x30, /* adds r0, 4      */
  /* after_c:                    */

  0x00, 0x29, /* cmp r1, 0       */
  0x00, 0xdb, /* blt after_d     */
  0x08, 0x30, /* adds r0, 8      */
  /* after_d:                    */

  0x00, 0xbd  /* pop {pc}        */
);

TESTCASE (thumb_branch_cc_block_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_BLOCK, thumb_b_cc, 10);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 4);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 10 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, start, func + 12 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, end, func + 16 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, start, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, end, func + 22 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, start, func + 24 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, end, func + 28 + 1);
}

TESTCODE (thumb_cbz_cbnz,
  0x00, 0xb5, /* push {lr}        */
  0x00, 0x1a, /* subs r0, r0, r0  */
  0x49, 0x1a, /* subs r1, r1, r1  */
  0x92, 0x1a, /* subs r2, r2, r2  */
  0x01, 0x32, /* adds r2, 1       */

  0x01, 0xb1, /* cbz r1, after_a  */
  0x01, 0x30, /* adds r0, 1       */
  /* after_a:                     */

  0x01, 0xb9, /* cbnz r1, after_b */
  0x02, 0x30, /* adds r0, 2       */
  /* after_b:                     */

  0x02, 0xb1, /* cbz r2, after_c  */
  0x04, 0x30, /* adds r0, 4       */
  /* after_c:                     */

  0x02, 0xb9, /* cbnz r2, after_d */
  0x08, 0x30, /* adds r0, 8       */
  /* after_d:                     */

  0x00, 0xbd  /* pop {pc}         */
);

TESTCASE (thumb_cbz_cbnz_block_events_generated)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_BLOCK, thumb_cbz_cbnz, 6);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 4);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 12 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, start, func + 14 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, end, func + 16 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, start, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, end, func + 20 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, start, func + 20 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, end, func + 24 + 1);
}

TESTCODE (thumb2_mov_pc_reg,
  0x40, 0xb5, /* push {r6, lr}    */
  0x00, 0x1a, /* subs r0, r0, r0  */
  0x01, 0x4e, /* ldr r6, [pc, #4] */
  0xb7, 0x46, /* mov pc, r6       */

  0x0a, 0xde, /* udf 0x10         */
  0x0a, 0xde, /* udf 0x10         */
  /* inner_addr:                  */
  0xaa, 0xbb, 0xcc, 0xdd,

  /* inner:                       */
  0x01, 0x30, /* adds r0, #1      */
  0x40, 0xbd  /* pop {r6, pc}     */
);

TESTCASE (thumb2_mov_pc_reg_exec_events_generated)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb2_mov_pc_reg);
  patch_code_pointer (func, 6 * 2, func + (8 * 2) + 1);

  fixture->sink->mask = GUM_EXEC;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 1);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 6);

  GUM_ASSERT_EVENT_ADDR (exec, 2, location, func +  0 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 3, location, func +  2 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 4, location, func +  4 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 5, location, func +  6 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 6, location, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 7, location, func + 18 + 1);
}

TESTCASE (thumb2_mov_pc_reg_without_thumb_bit_set)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb2_mov_pc_reg);
  patch_code_pointer (func, 6 * 2, func + (8 * 2) + 0);

  fixture->sink->mask = GUM_EXEC;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 1);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 6);

  GUM_ASSERT_EVENT_ADDR (exec, 2, location, func +  0 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 3, location, func +  2 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 4, location, func +  4 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 5, location, func +  6 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 6, location, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 7, location, func + 18 + 1);
}

TESTCODE (thumb2_mov_pc_reg_no_clobber_reg,
  0x60, 0xb5, /* push {r5, r6, lr} */
  0x00, 0x1a, /* subs r0, r0, r0   */
  0x01, 0x4e, /* ldr r6, [pc, #4]  */
  0x35, 0x46, /* mov r5, r6        */
  0xb7, 0x46, /* mov pc, r6        */

  0x0a, 0xde, /* udf 0x10          */
  /* inner_addr:                   */
  0xaa, 0xbb, 0xcc, 0xdd,

  /* inner:                        */
  0xa8, 0x1b, /* subs r0, r5, r6   */
  0x60, 0xbd  /* pop {r5,r6, pc}   */
);

TESTCASE (thumb2_mov_pc_reg_no_clobber_reg)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb2_mov_pc_reg_no_clobber_reg);
  patch_code_pointer (func, 6 * 2, func + (8 * 2) + 0);

  fixture->sink->mask = GUM_EXEC;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 7);

  GUM_ASSERT_EVENT_ADDR (exec, 2, location, func +  0 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 3, location, func +  2 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 4, location, func +  4 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 5, location, func +  6 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 6, location, func +  8 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 7, location, func + 16 + 1);
  GUM_ASSERT_EVENT_ADDR (exec, 8, location, func + 18 + 1);
}

TESTCODE (arm_bl_cc,
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}         */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0    */
  0x01, 0x10, 0x41, 0xe0, /* sub r1, r1, r1    */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0         */
  0x06, 0x00, 0x00, 0x0b, /* bleq func_a       */

  0x01, 0x00, 0x51, 0xe3, /* cmp r1, 1         */
  0x06, 0x00, 0x00, 0x0b, /* bleq func_b       */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0         */
  0x06, 0x00, 0x00, 0xab, /* blge func_c       */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0         */
  0x06, 0x00, 0x00, 0xbb, /* bllt func_d       */

  0x04, 0xf0, 0x9d, 0xe4, /* pop {pc}          */

  /* func_a:                                   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr        */

  /* func_b:                                   */
  0x02, 0x00, 0x80, 0xe2, /* add r0, r0, 2     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr        */

  /* func_c:                                   */
  0x04, 0x00, 0x80, 0xe2, /* add r0, r0, 4     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr        */

  /* func_d:                                   */
  0x08, 0x00, 0x80, 0xe2, /* add r0, r0, 8     */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr        */
);

TESTCASE (arm_branch_link_cc_block_events_generated)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_CALL, arm_bl_cc, 5);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_CALL_INSN_COUNT + 2);

  GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 16);
  GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 48);

  GUM_ASSERT_EVENT_ADDR (call, 2, location, func + 32);
  GUM_ASSERT_EVENT_ADDR (call, 2, target, func + 64);
}

TESTCODE (arm_cc_excluded_range,
  0x00, 0x40, 0x2d, 0xe9, /* stmdb sp!, {lr}   */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0    */
  0x01, 0x10, 0x41, 0xe0, /* sub r1, r1, r1    */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0         */
  0x03, 0x00, 0x00, 0x0b, /* bleq func_a       */

  0x00, 0x00, 0x51, 0xe3, /* cmp r1, 0         */
  0x05, 0x00, 0x00, 0x1b, /* blne func_b       */

  0x00, 0x40, 0xbd, 0xe8, /* ldm sp!, {lr}     */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr        */

  /* func_a:                                   */
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}         */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1     */
  0x04, 0x00, 0x00, 0xeb, /* bl func_c         */
  0x04, 0xf0, 0x9d, 0xe4, /* pop {pc}          */

  /* func_b:                                   */
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}         */
  0x02, 0x00, 0x80, 0xe2, /* add r0, r0, 2     */
  0x00, 0x00, 0x00, 0xeb, /* bl func_c         */
  0x04, 0xf0, 0x9d, 0xe4, /* pop {pc}          */

  /* func_c:                                   */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr        */
);

TESTCASE (arm_cc_excluded_range)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_cc_excluded_range);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 36,
      .size = 36
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_CALL;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 1);

    g_assert_cmpuint (fixture->sink->events->length, ==,
        INVOKER_CALL_INSN_COUNT + 1);

    GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 16);
    GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 36);
  }
}

TESTCODE (arm_ldr_pc,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0     */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1      */
  0x04, 0xf0, 0x9f, 0xe5, /* ldr pc, inner_addr */
  0xf0, 0x01, 0xf0, 0xe7, /* udf 0x10           */

  0xec, 0xec, 0xec, 0xec,
  /* inner_addr:                                */
  0xaa, 0xbb, 0xcc, 0xdd,

  /* inner:                                     */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1      */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr         */
);

TESTCASE (arm_ldr_pc)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_ldr_pc);
  patch_code_pointer (func, 5 * 4, func + (6 * 4));

  fixture->sink->mask = GUM_BLOCK;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 12);
}

TESTCODE (arm_ldr_pc_pre_index_imm,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0   */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1    */
  0x04, 0x10, 0x8f, 0xe2, /* adr r1, imm_data */
  0x08, 0xf0, 0xb1, 0xe5, /* ldr pc, [r1, 8]! */
  0xf0, 0x01, 0xf0, 0xe7, /* udf 0x10         */

  /* imm_data:                                */
  0xec, 0xec, 0xec, 0xec,
  0xf0, 0xf0, 0xf0, 0xf0,
  /* inner_addr:                              */
  0xaa, 0xbb, 0xcc, 0xdd,
  0xba, 0xba, 0xba, 0xba,

  /* inner:                                   */
  0x04, 0x10, 0x91, 0xe5, /* ldr r1, [r1, 4]  */
  0x01, 0x00, 0x80, 0xe0, /* add r0, r0, r1   */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr       */
);

TESTCASE (arm_ldr_pc_pre_index_imm)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_ldr_pc_pre_index_imm);
  patch_code_pointer (func, 7 * 4, func + (9 * 4));

  fixture->sink->mask = GUM_BLOCK;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 0xbabababb);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 16);
}

TESTCODE (arm_ldr_pc_post_index_imm,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0     */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1      */
  0x04, 0x10, 0x8f, 0xe2, /* adr r1, inner_addr */
  0x08, 0xf0, 0x91, 0xe4, /* ldr pc, [r1], 8    */
  0xf0, 0x01, 0xf0, 0xe7, /* udf 0x10           */

  /* inner_addr:                                */
  0xaa, 0xbb, 0xcc, 0xdd,
  0xf0, 0xf0, 0xf0, 0xf0,
  0xba, 0xba, 0xba, 0xba,

  /* inner:                                     */
  0x00, 0x10, 0x91, 0xe5, /* ldr r1, [r1]       */
  0x01, 0x00, 0x80, 0xe0, /* add r0, r0, r1     */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr         */
);

TESTCASE (arm_ldr_pc_post_index_imm)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_ldr_pc_post_index_imm);
  patch_code_pointer (func, 5 * 4, func + (8 * 4));

  fixture->sink->mask = GUM_BLOCK;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 0xbabababb);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 16);
}

TESTCODE (arm_ldr_pc_pre_index_imm_negative,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0        */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1         */
  0x0c, 0x10, 0x8f, 0xe2, /* adr r1, negative_data */
  0x08, 0xf0, 0x31, 0xe5, /* ldr pc, [r1, -8]!     */
  0xf0, 0x01, 0xf0, 0xe7, /* udf 0x10              */

  /* inner_addr:                                   */
  0xaa, 0xbb, 0xcc, 0xdd,
  0xec, 0xec, 0xec, 0xec,
  /* negative_data:                                */
  0xf0, 0xf0, 0xf0, 0xf0,
  0xba, 0xba, 0xba, 0xba,

  /* inner:                                        */
  0x0c, 0x10, 0x91, 0xe5, /* ldr r1, [r1, 12]      */
  0x01, 0x00, 0x80, 0xe0, /* add r0, r0, r1        */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr            */
);

TESTCASE (arm_ldr_pc_pre_index_imm_negative)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_ldr_pc_pre_index_imm_negative);
  patch_code_pointer (func, 5 * 4, func + (9 * 4));

  fixture->sink->mask = GUM_BLOCK;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 0xbabababb);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 16);
}

TESTCODE (arm_ldr_pc_post_index_imm_negative,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0     */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1      */
  0x0c, 0x10, 0x8f, 0xe2, /* adr r1, inner_addr */
  0x08, 0xf0, 0x11, 0xe4, /* ldr pc, [r1], -8   */
  0xf0, 0x01, 0xf0, 0xe7, /* udf 0x10           */

  0xba, 0xba, 0xba, 0xba,
  0xf0, 0xf0, 0xf0, 0xf0,
  /* inner_addr:                                */
  0xaa, 0xbb, 0xcc, 0xdd,

  /* inner:                                     */
  0x00, 0x10, 0x91, 0xe5, /* ldr r1, [r1]       */
  0x01, 0x00, 0x80, 0xe0, /* add r0, r0, r1     */
  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr         */
);

TESTCASE (arm_ldr_pc_post_index_imm_negative)
{
  GumAddress func;

  func = DUP_TESTCODE (arm_ldr_pc_post_index_imm_negative);
  patch_code_pointer (func, 7 * 4, func + (8 * 4));

  fixture->sink->mask = GUM_BLOCK;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 0xbabababb);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 16);
}

TESTCODE (arm_ldr_pc_shift_code,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0           */
  0x04, 0x00, 0x80, 0xe2, /* add r0, r0, 4            */
  0x00, 0xf1, 0x9f, 0xe7, /* ldr pc, [pc, r0, lsl #2] */
  0xf0, 0x01, 0xf0, 0xe7, /* udf 0x10                 */

  0x22, 0x22, 0x22, 0x22,
  0x44, 0x44, 0x44, 0x44,
  0x66, 0x66, 0x66, 0x66,
  0x88, 0x88, 0x88, 0x88,
  0xaa, 0xaa, 0xaa, 0xaa, /* Branch Target            */

  0x0e, 0xf0, 0xa0, 0xe1  /* mov pc, lr               */
);

TESTCASE (arm_ldr_pc_shift)
{
  GumAddress func;
  guint32 * code;

  func = DUP_TESTCODE (arm_ldr_pc_shift_code);
  code = GSIZE_TO_POINTER (func);
  g_assert_cmpuint (code[8], ==, 0xaaaaaaaa);
  patch_code_pointer (func, 8 * sizeof (gsize), func + (9 * sizeof (gsize)));

  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCODE (arm_sub_pc,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x01, 0x00, 0x00, 0xea, /* b part_two     */

  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr     */

  /* part_two:                              */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x14, 0xf0, 0x4f, 0xe2  /* sub pc, pc, 20 */
);

TESTCASE (arm_sub_pc)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_BLOCK, arm_sub_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 2);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 8);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, start, func + 16);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, end, func + 24);
}

TESTCODE (arm_add_pc,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x04, 0xf0, 0x8f, 0xe2, /* add pc, pc, 4  */

  /* beach:                                 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr     */

  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0xfb, 0xff, 0xff, 0xea  /* b beach        */
);

TESTCASE (arm_add_pc)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_BLOCK, arm_add_pc, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 2);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 8);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, start, func + 16);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, end, func + 24);
}

TESTCODE (arm_ldmia_pc,
  0x0d, 0xc0, 0xa0, 0xe1, /* mov r12, sp                                   */
  0x78, 0xd8, 0x2d, 0xe9, /* stmdb sp!, {r3, r4, r5, r6, r11, r12, lr, pc} */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0                                */
  0x78, 0xa8, 0x9d, 0xe8, /* ldmia sp, {r3, r4, r5, r6, r11, sp, pc}       */
);

TESTCASE (arm_ldmia_pc)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_BLOCK, arm_ldmia_pc, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 16);
}

TESTCODE (thumb_it_eq,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x00, 0x28, /* cmp r0, #0      */
  0x08, 0xbf, /* it eq           */
  0x01, 0x30, /* adds r0, #1     */

  /* part_two:                   */
  0x00, 0x28, /* cmp r0, #0      */
  0x08, 0xbf, /* it eq           */
  0x02, 0x30, /* adds r0, #2     */
  0x00, 0xbd, /* pop {pc}        */
);

TESTCASE (thumb_it_eq)
{
  INVOKE_THUMB_EXPECTING (GUM_NOTHING, thumb_it_eq, 1);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCODE (thumb_it_al,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0xe8, 0xbf, /* it al           */
  0x01, 0x30, /* adds r0, #1     */

  /* part_two:                   */
  0xe8, 0xbf, /* it al           */
  0x02, 0x30, /* adds r0, #2     */
  0x04, 0x30, /* adds r0, #4     */
  0x00, 0xbd, /* pop {pc}        */
);

TESTCASE (thumb_it_al)
{
  INVOKE_THUMB_EXPECTING (GUM_NOTHING, thumb_it_al, 7);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCODE (thumb_it_eq_branch,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x00, 0x28, /* cmp r0, #0      */
  0x08, 0xbf, /* it eq           */
  0x00, 0xe0, /* b part_two      */
  0x00, 0xde, /* udf 0           */

  /* part_two:                   */
  0x01, 0x28, /* cmp r0, #1      */
  0x08, 0xbf, /* it eq           */
  0x00, 0xe0, /* b part_three    */
  0x00, 0xbd, /* pop {pc}        */

  /* part_three:                 */
  0x00, 0xde  /* udf 0           */
);

TESTCASE (thumb_it_eq_branch)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_BLOCK, thumb_it_eq_branch, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 2);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 10 + 1);
}

TESTCODE (thumb_itt_eq_branch,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x49, 0x1a, /* subs r1, r1, r1 */
  0x00, 0x29, /* cmp r1, #0      */
  0x04, 0xbf, /* itt eq          */
  0x01, 0x30, /* add r0, #1      */
  0x00, 0xe0, /* b part_two      */
  0x00, 0xde, /* udf 0           */

  /* part_two:                   */
  0x01, 0x29, /* cmp r1, #1      */
  0x04, 0xbf, /* itt eq          */
  0x02, 0x30, /* add r0, #2      */
  0x00, 0xe0, /* b part_three    */
  0x00, 0xbd, /* pop {pc}        */

  /* part_three:                 */
  0x00, 0xde  /* udf 0           */
);

TESTCASE (thumb_itt_eq_branch)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_BLOCK, thumb_itt_eq_branch, 1);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 2);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 14 + 1);
}

TESTCODE (thumb_ite_eq_branch,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x49, 0x1a, /* subs r1, r1, r1 */
  0x01, 0x29, /* cmp r1, #1      */
  0x0c, 0xbf, /* ite eq          */
  0x01, 0x30, /* add r0, #1      */
  0x00, 0xe0, /* b part_two      */
  0x00, 0xde, /* udf 0           */

  /* part_two:                   */
  0x00, 0x29, /* cmp r1, #0      */
  0x0c, 0xbf, /* ite eq          */
  0x02, 0x30, /* add r0, #2      */
  0x00, 0xe0, /* b part_three    */
  0x00, 0xbd, /* pop {pc}        */

  /* part_three:                 */
  0x00, 0xde  /* udf 0           */
);

TESTCASE (thumb_ite_eq_branch)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_BLOCK, thumb_ite_eq_branch, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 2);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, start, func + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX, end, func + 14 + 1);
}

TESTCODE (thumb_it_eq_branch_link,
  0x00, 0xb5,             /* push {lr}       */
  0x00, 0x1a,             /* subs r0, r0, r0 */
  0x49, 0x1a,             /* subs r1, r1, r1 */
  0x01, 0x31,             /* adds r1, #1     */
  0x00, 0x28,             /* cmp r0, #0      */
  0x08, 0xbf,             /* it eq           */
  0x00, 0xf0, 0x06, 0xf8, /* bl part_three   */

  /* part_two:                               */
  0x01, 0x31,             /* adds r1, #1     */
  0x00, 0x28,             /* cmp r0, #0      */
  0x08, 0xbf,             /* it eq           */
  0x00, 0xf0, 0x01, 0xf8, /* bl part_three   */
  0x00, 0xbd,             /* pop {pc}        */

  /* part_three:                             */
  0x00, 0xb5,             /* push {lr}       */
  0x08, 0x44,             /* add r0, r1      */
  0x00, 0xbd,             /* pop {pc}        */
);

TESTCASE (thumb_it_eq_branch_link)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_CALL, thumb_it_eq_branch_link, 1);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_CALL_INSN_COUNT + 1);

  GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 10 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 28 + 1);
}

TESTCASE (thumb_it_eq_branch_link_excluded)
{
  GumAddress func;

  func = DUP_TESTCODE (thumb_it_eq_branch_link);

  {
    GumMemoryRange r = {
      .base_address = GUM_ADDRESS (func) + 28,
      .size = 6
    };

    gum_stalker_exclude (fixture->stalker, &r);
  }

  {
    fixture->sink->mask = GUM_EXEC;
    g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, 1);

    g_assert_cmpuint (fixture->sink->events->length, ==,
        INVOKER_INSN_COUNT + 10);

    GUM_ASSERT_EVENT_ADDR (exec,  2, location, func +  0 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  3, location, func +  2 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  4, location, func +  4 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  5, location, func +  6 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  6, location, func +  8 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  7, location, func + 10 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  8, location, func + 16 + 1);
    GUM_ASSERT_EVENT_ADDR (exec,  9, location, func + 18 + 1);
    GUM_ASSERT_EVENT_ADDR (exec, 10, location, func + 20 + 1);
    GUM_ASSERT_EVENT_ADDR (exec, 11, location, func + 26 + 1);
  }
}

TESTCODE (thumb_it_eq_pop,
  0x00, 0xb5,             /* push {lr}       */
  0x00, 0x1a,             /* subs r0, r0, r0 */
  0x01, 0x30,             /* adds r0, #1     */
  0x00, 0xf0, 0x03, 0xf8, /* bl part_two     */
  0x00, 0xf0, 0x01, 0xf8, /* bl part_two     */
  0x00, 0xbd,             /* pop {pc}        */

  /* part_two:                               */
  0x04, 0xb5,             /* push {r2, lr}   */
  0x02, 0x28,             /* cmp r0, #2      */
  0x08, 0xbf,             /* it eq           */
  0x04, 0xbd,             /* pop {r2, pc}    */
  0x01, 0x30,             /* adds r0, #1     */
  0x04, 0xbd,             /* pop {r2, pc}    */
);

TESTCASE (thumb_it_eq_pop)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_it_eq_pop, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 3);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 26 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 0, target, func + 10 + 1);

  GUM_ASSERT_EVENT_ADDR (ret, 1, location, func + 20 + 1);
  GUM_ASSERT_EVENT_ADDR (ret, 1, target, func + 14 + 1);

  GUM_ASSERT_EVENT_ADDR (ret, 2, location, func + 14 + 1);
}

TESTCODE (thumb_itttt_eq_blx_reg,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x49, 0x1a, /* subs r1, r1, r1 */
  0x79, 0x44, /* add r1, pc      */
  0x1b, 0x31, /* adds r1, #27    */
  0x00, 0x28, /* cmp r0, #0      */
  0x01, 0xbf, /* itttt eq        */
  0x01, 0x30, /* adds r0, #1     */
  0x01, 0x30, /* adds r0, #1     */
  0x01, 0x30, /* adds r0, #1     */
  0x88, 0x47, /* blx r1          */

  /* part_two:                   */
  0x00, 0x28, /* cmp r0, #0      */
  0x01, 0xbf, /* itttt eq        */
  0x02, 0x30, /* adds r0, #2     */
  0x02, 0x30, /* adds r0, #2     */
  0x02, 0x30, /* adds r0, #2     */
  0x88, 0x47, /* blx r1          */
  0x00, 0xbd, /* pop {pc}        */

  /* part_three:                 */
  0x00, 0xb5, /* push {lr}       */
  0x01, 0x30, /* adds r0, #1     */
  0x00, 0xbd, /* pop {pc}        */
);

TESTCASE (thumb_itttt_eq_blx_reg)
{
  INVOKE_THUMB_EXPECTING (GUM_EXEC | GUM_CALL, thumb_itttt_eq_blx_reg, 4);
  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 16);
}

TESTCODE (thumb_it_flags,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x00, 0x28, /* cmp r0, #0      */
  0x08, 0xbf, /* it eq           */
  0x01, 0x30, /* adds r0, #1     */

  /* part_two:                   */
  0x08, 0xbf, /* it eq           */
  0x02, 0x30, /* adds r0, #2     */
  0x00, 0xbd, /* pop {pc}        */
);

TESTCASE (thumb_it_flags)
{
  INVOKE_THUMB_EXPECTING (GUM_NOTHING, thumb_it_flags, 3);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCODE (thumb_it_flags2,
  0x00, 0xb5, /* push {lr}       */
  0x00, 0x1a, /* subs r0, r0, r0 */
  0x00, 0x28, /* cmp r0, #0      */
  0x08, 0xbf, /* it eq           */
  0x01, 0x28, /* cmp.eq r0, #1   */

  /* part_two:                   */
  0x18, 0xbf, /* it ne           */
  0x02, 0x30, /* adds r0, #2     */
  0x00, 0xbd, /* pop {pc}        */
);

TESTCASE (thumb_it_flags2)
{
  INVOKE_THUMB_EXPECTING (GUM_NOTHING, thumb_it_flags2, 2);

  g_assert_cmpuint (fixture->sink->events->length, ==, 0);
}

TESTCODE (thumb_tbb,
  0x00, 0xb5,             /* push {lr}            */
  0x00, 0x20,             /* movs r0, 0           */

  0x01, 0x21,             /* movs r1, 1           */
  0xdf, 0xe8, 0x01, 0xf0, /* tbb [pc, r1]         */

  /* table1:                                      */
  0x02,                   /* (one - table1) / 2   */
  0x03,                   /* (two - table1) / 2   */
  0x04,                   /* (three - table1) / 2 */
  0xff,                   /* <alignment padding>  */

  /* one:                                         */
  0x40, 0x1c,             /* adds r0, r0, 1       */
  /* two:                                         */
  0x80, 0x1c,             /* adds r0, r0, 2       */
  /* three:                                       */
  0xc0, 0x1c,             /* adds r0, r0, 3       */

  0x00, 0xbd,             /* pop {pc}             */
);

TESTCASE (thumb_tbb)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_tbb, 5);

  g_assert_cmpuint (fixture->sink->events->length, ==, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 20 + 1);
}

TESTCODE (thumb_tbh,
  0x00, 0xb5,             /* push {lr}            */
  0x00, 0x20,             /* movs r0, 0           */

  0x5f, 0xf0, 0x02, 0x0c, /* movs.w ip, 2         */
  0xdf, 0xe8, 0x1c, 0xf0, /* tbh [pc, ip, lsl 1]  */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  /* table1:                                      */
  0x03, 0x00,             /* (one - table1) / 2   */
  0x04, 0x00,             /* (two - table1) / 2   */
  0x05, 0x00,             /* (three - table1) / 2 */
#else
  /* table1:                                      */
  0x00, 0x03,             /* (one - table1) / 2   */
  0x00, 0x04,             /* (two - table1) / 2   */
  0x00, 0x05,             /* (three - table1) / 2 */
#endif

  /* one:                                         */
  0x40, 0x1c,             /* adds r0, r0, 1       */
  /* two:                                         */
  0x80, 0x1c,             /* adds r0, r0, 2       */
  /* three:                                       */
  0xc0, 0x1c,             /* adds r0, r0, 3       */

  0x00, 0xbd,             /* pop {pc}             */
);

TESTCASE (thumb_tbh)
{
  GumAddress func;

  func = INVOKE_THUMB_EXPECTING (GUM_RET, thumb_tbh, 3);

  g_assert_cmpuint (fixture->sink->events->length, ==, 1);

  GUM_ASSERT_EVENT_ADDR (ret, 0, location, func + 24 + 1);
}

TESTCODE (self_modifying_code_should_be_detected,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr     */
);

TESTCASE (self_modifying_code_should_be_detected_with_threshold_minus_one)
{
  GumAddress func;
  guint (* f) (void);
  guint value;
  CodePatcher patcher;

  func = DUP_TESTCODE (self_modifying_code_should_be_detected);
  f = GUM_POINTER_TO_FUNCPTR (guint (*) (void), func);

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  code_patcher_start (&patcher);
  gum_stalker_set_trust_threshold (fixture->stalker, -1);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  value = f ();
  g_assert_cmpuint (value, ==, 1);

  patch_code_pointer_on_thread (&patcher, func, 4, GSIZE_TO_LE (0xe2800002));
  value = f ();
  g_assert_cmpuint (value, ==, 2);
  f ();
  f ();

  patch_code_pointer_on_thread (&patcher, func, 4, GSIZE_TO_LE (0xe2800003));
  value = f ();
  g_assert_cmpuint (value, ==, 3);

  gum_stalker_unfollow_me (fixture->stalker);

  code_patcher_stop (&patcher);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (self_modifying_code_should_not_be_detected_with_threshold_zero)
{
  GumAddress func;
  guint (* f) (void);
  guint value;
  CodePatcher patcher;

  func = DUP_TESTCODE (self_modifying_code_should_be_detected);
  f = GUM_POINTER_TO_FUNCPTR (guint (*) (void), func);

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  code_patcher_start (&patcher);
  gum_stalker_set_trust_threshold (fixture->stalker, 0);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  value = f ();
  g_assert_cmpuint (value, ==, 1);

  patch_code_pointer_on_thread (&patcher, func, 4, GSIZE_TO_LE (0xe2800002));
  value = f ();
  g_assert_cmpuint (value, ==, 1);

  gum_stalker_unfollow_me (fixture->stalker);

  code_patcher_stop (&patcher);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (self_modifying_code_should_be_detected_with_threshold_one)
{
  GumAddress func;
  guint (* f) (void);
  guint value;
  CodePatcher patcher;

  func = DUP_TESTCODE (self_modifying_code_should_be_detected);
  f = GUM_POINTER_TO_FUNCPTR (guint (*) (void), func);

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  code_patcher_start (&patcher);
  gum_stalker_set_trust_threshold (fixture->stalker, 1);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  value = f ();
  g_assert_cmpuint (value, ==, 1);

  patch_code_pointer_on_thread (&patcher, func, 4, GSIZE_TO_LE (0xe2800002));
  value = f ();
  g_assert_cmpuint (value, ==, 2);
  f ();
  f ();

  patch_code_pointer_on_thread (&patcher, func, 4, GSIZE_TO_LE (0xe2800003));
  value = f ();
  g_assert_cmpuint (value, ==, 2);

  gum_stalker_unfollow_me (fixture->stalker);

  code_patcher_stop (&patcher);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCODE (call_thumb,
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}      */
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x05, 0x00, 0x00, 0xfa, /* blx func_c     */
  0x00, 0x00, 0x00, 0xeb, /* bl func_a      */
  0x04, 0xf0, 0x9d, 0xe4, /* pop {pc}       */

  /* func_a:                                */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr     */

  /* func_b:                                */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr     */

  /* func_c:                                */
  0x00, 0xb5,             /* push {lr}      */
  0x01, 0x30,             /* adds r0, 1     */
  0xff, 0xf7, 0xf8, 0xef, /* blx func_b     */
  0x00, 0xbd,             /* pop {pc}       */
);

TESTCASE (call_thumb)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_CALL, call_thumb, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==,
      INVOKER_CALL_INSN_COUNT + 3);

  GUM_ASSERT_EVENT_ADDR (call, 1, location, func + 12);
  GUM_ASSERT_EVENT_ADDR (call, 1, target, func + 40 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 1, depth, 1);

  GUM_ASSERT_EVENT_ADDR (call, 2, location, func + 44 + 1);
  GUM_ASSERT_EVENT_ADDR (call, 2, target, func + 32);
  GUM_ASSERT_EVENT_ADDR (call, 2, depth, 2);

  GUM_ASSERT_EVENT_ADDR (call, 3, location, func + 16);
  GUM_ASSERT_EVENT_ADDR (call, 3, target, func + 24);
  GUM_ASSERT_EVENT_ADDR (call, 3, depth, 1);
}

TESTCODE (branch_thumb,
  0x00, 0x00, 0x40, 0xe0, /* sub r0, r0, r0 */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0x08, 0x10, 0x8f, 0xe2, /* adr r1, func_a */
  0x14, 0x20, 0x8f, 0xe2, /* adr r2, func_c */
  0x01, 0x20, 0x82, 0xe2, /* add r2, r2, 1  */
  0x12, 0xff, 0x2f, 0xe1, /* bx r2          */

  /* func_a:                                */
  0x00, 0x00, 0x00, 0xea, /* b func_b       */

  /* beach:                                 */
  0x0e, 0xf0, 0xa0, 0xe1, /* mov pc, lr     */

  /* func_b:                                */
  0x01, 0x00, 0x80, 0xe2, /* add r0, r0, 1  */
  0xfc, 0xff, 0xff, 0xea, /* b beach        */

  /* func_c:                                */
  0x01, 0x30,             /* adds r0, 1     */
  0x08, 0x47              /* bx r1          */
);

TESTCASE (branch_thumb)
{
  GumAddress func;

  func = INVOKE_ARM_EXPECTING (GUM_BLOCK, branch_thumb, 3);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_BLOCK_COUNT + 4);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, start, func);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 0, end, func + 24);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, start, func + 40 + 1);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 1, end, func + 44 + 1);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, start, func + 24);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 2, end, func + 28);

  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, start, func + 32);
  GUM_ASSERT_EVENT_ADDR (block, INVOKEE_BLOCK_INDEX + 3, end, func + 40);
}

TESTCODE (call_workload,
  0x02, 0x40, 0x2d, 0xe9, /* push {r1, lr}         */
  0x04, 0x10, 0x9f, 0xe5, /* ldr r1, workload_addr */
  0x31, 0xff, 0x2f, 0xe1, /* blx r1                */
  0x02, 0x80, 0xbd, 0xe8  /* pop {r1, pc}          */
);

TESTCASE (can_follow_workload)
{
  GumAddress func;
  guint32 (* call_workload_impl) (const GumMemoryRange * runner_range);
  guint32 crc, crc_followed;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  func = DUP_TESTCODE (call_workload);
  patch_code_pointer (func, 4 * 4, GUM_ADDRESS (pretend_workload));
  call_workload_impl = GSIZE_TO_POINTER (func);

  crc = call_workload_impl (fixture->runner_range);

  g_test_log_set_fatal_handler (test_log_fatal_func, NULL);
  g_log_set_writer_func (test_log_writer_func, NULL, NULL);

  fixture->sink->mask = GUM_RET;
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  call_workload_impl (fixture->runner_range);

  gum_stalker_unfollow_me (fixture->stalker);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  crc_followed = call_workload_impl (fixture->runner_range);

  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (crc_followed, ==, crc);

  GUM_ASSERT_EVENT_ADDR (ret, fixture->sink->events->length - 1, location,
      func + 12);
}

TESTCASE (performance)
{
  GTimer * timer;
  gdouble normal_cold, normal_hot;
  gdouble stalker_cold, stalker_hot;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  timer = g_timer_new ();
  pretend_workload (fixture->runner_range);

  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  normal_cold = g_timer_elapsed (timer, NULL);

  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  normal_hot = g_timer_elapsed (timer, NULL);

  g_test_log_set_fatal_handler (test_log_fatal_func, NULL);
  g_log_set_writer_func (test_log_writer_func, NULL, NULL);

  fixture->sink->mask = GUM_NOTHING;
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  stalker_cold = g_timer_elapsed (timer, NULL);

  g_timer_reset (timer);
  pretend_workload (fixture->runner_range);
  stalker_hot = g_timer_elapsed (timer, NULL);

  gum_stalker_unfollow_me (fixture->stalker);

  g_timer_destroy (timer);

  g_print ("\n");
  g_print ("\t<normal_cold=%f>\n", normal_cold);
  g_print ("\t<normal_hot=%f>\n", normal_hot);
  g_print ("\t<stalker_cold=%f>\n", stalker_cold);
  g_print ("\t<stalker_hot=%f>\n", stalker_hot);
  g_print ("\t<ratio_cold=%f>\n", stalker_cold / normal_hot);
  g_print ("\t<ratio_hot=%f>\n", stalker_hot / normal_hot);
}

GUM_NOINLINE static guint32
pretend_workload (const GumMemoryRange * runner_range)
{
  guint32 crc;
  lzma_stream stream = LZMA_STREAM_INIT;
  const uint32_t preset = LZMA_PRESET_DEFAULT;
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

  crc = crc32b (outbuf, stream.total_out);

  free (outbuf);

  return crc;
}

static guint32
crc32b (const guint8 * message,
        gsize size)
{
  guint32 crc;
  gint i;

  crc = 0xffffffff;

  for (i = 0; i != size; i++)
  {
    guint32 byte;
    gint j;

    byte = message[i];

    crc = crc ^ byte;

    for (j = 7; j >= 0; j--)
    {
      guint32 mask = -(crc & 1);

      crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
  }

  return ~crc;
}

static gboolean
test_log_fatal_func (const gchar * log_domain,
                     GLogLevelFlags log_level,
                     const gchar * message,
                     gpointer user_data)
{
  return FALSE;
}

static GLogWriterOutput
test_log_writer_func (GLogLevelFlags log_level,
                      const GLogField * fields,
                      gsize n_fields,
                      gpointer user_data)
{
  return G_LOG_WRITER_HANDLED;
}

TESTCASE (custom_transformer)
{
  fixture->transformer = gum_stalker_transformer_make_from_callback (
      duplicate_adds, NULL, NULL);

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 4);
}

static void
duplicate_adds (GumStalkerIterator * iterator,
                GumStalkerOutput * output,
                gpointer user_data)
{
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    gum_stalker_iterator_keep (iterator);

    if (insn->id == ARM_INS_ADD)
      gum_arm_writer_put_bytes (output->writer.arm, insn->bytes, insn->size);
  }
}

TESTCASE (arm_callout)
{
  gpointer magic = GSIZE_TO_POINTER (0xbaadface);

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      transform_arm_return_value, magic, NULL);

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 42);
}

static void
transform_arm_return_value (GumStalkerIterator * iterator,
                            GumStalkerOutput * output,
                            gpointer user_data)
{
  gpointer magic = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (is_arm_mov_pc_lr (insn->bytes, insn->size))
    {
      gum_stalker_iterator_put_callout (iterator, on_arm_ret, magic, NULL);
    }

    gum_stalker_iterator_keep (iterator);
  }
}

static void
on_arm_ret (GumCpuContext * cpu_context,
            gpointer user_data)
{
  gpointer magic = user_data;
  const guint8 * bytes = GSIZE_TO_POINTER (cpu_context->pc);

  g_assert_cmphex (GPOINTER_TO_SIZE (magic), ==, 0xbaadface);
  g_assert_cmpuint (cpu_context->r[0], ==, 2);
  g_assert_true (is_arm_mov_pc_lr (bytes, 4));

  cpu_context->r[0] = 42;
}

static gboolean
is_arm_mov_pc_lr (const guint8 * bytes,
                  gsize size)
{
  const guint8 mov_pc_lr[] = { 0x0e, 0xf0, 0xa0, 0xe1 };

  if (size != sizeof (mov_pc_lr))
    return FALSE;

  return memcmp (bytes, mov_pc_lr, size) == 0;
}

TESTCASE (thumb_callout)
{
  gpointer magic = GSIZE_TO_POINTER (0xfacef00d);

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      transform_thumb_return_value, magic, NULL);

  INVOKE_THUMB_EXPECTING (GUM_NOTHING, thumb_flat_code, 24);
}

static void
transform_thumb_return_value (GumStalkerIterator * iterator,
                              GumStalkerOutput * output,
                              gpointer user_data)
{
  gpointer magic = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (is_thumb_pop_pc (insn->bytes, insn->size))
    {
      gum_stalker_iterator_put_callout (iterator, on_thumb_ret, magic, NULL);
    }

    gum_stalker_iterator_keep (iterator);
  }
}

static void
on_thumb_ret (GumCpuContext * cpu_context,
              gpointer user_data)
{
  gpointer magic = user_data;
  const guint8 * bytes = GSIZE_TO_POINTER (cpu_context->pc);

  g_assert_cmphex (GPOINTER_TO_SIZE (magic), ==, 0xfacef00d);
  g_assert_cmpuint (cpu_context->r[0], ==, 2);
  g_assert_true (is_thumb_pop_pc (bytes, 2));

  cpu_context->r[0] = 24;
}

static gboolean
is_thumb_pop_pc (const guint8 * bytes,
                 gsize size)
{
  guint8 pop_pc[] = { 0x00, 0xbd };

  if (size != sizeof (pop_pc))
    return FALSE;

  return memcmp (bytes, pop_pc, size) == 0;
}

TESTCODE (arm_simple_call,
  0x04, 0xe0, 0x2d, 0xe5, /* push {lr}      */
  0x0d, 0x00, 0x00, 0xe3, /* mov r0, 13     */
  0x00, 0x00, 0x00, 0xeb, /* bl bump_number */
  0x04, 0xf0, 0x9d, 0xe4, /* pop {pc}       */
  /* bump_number:                           */
  0x25, 0x00, 0x80, 0xe2, /* add r0, 37     */
  0x1e, 0xff, 0x2f, 0xe1, /* bx lr          */
);

TESTCASE (arm_transformer_should_be_able_to_replace_call_with_callout)
{
  fixture->transformer = gum_stalker_transformer_make_from_callback (
      replace_call_with_callout, NULL, NULL);

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_simple_call, 0xc001);
}

static void
replace_call_with_callout (GumStalkerIterator * iterator,
                           GumStalkerOutput * output,
                           gpointer user_data)
{
  const cs_insn * insn;
  static int insn_num = 0;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn_num == 4)
      gum_stalker_iterator_put_callout (iterator, callout_set_cool, NULL, NULL);
    else
      gum_stalker_iterator_keep (iterator);
    insn_num++;
  }
}

TESTCODE (arm_simple_jumpout,
  0x0d, 0x00, 0x00, 0xe3, /* mov r0, 13    */
  0xff, 0xff, 0xff, 0xea, /* b bump_number */
  /* bump_number:                          */
  0x25, 0x00, 0x80, 0xe2, /* add r0, 37    */
  0x1e, 0xff, 0x2f, 0xe1, /* bx lr         */
);

TESTCASE (arm_transformer_should_be_able_to_replace_jumpout_with_callout)
{
  fixture->transformer = gum_stalker_transformer_make_from_callback (
      replace_jumpout_with_callout, NULL, NULL);

  INVOKE_ARM_EXPECTING (GUM_EXEC, arm_simple_jumpout, 0xc001);
}

static void
replace_jumpout_with_callout (GumStalkerIterator * iterator,
                              GumStalkerOutput * output,
                              gpointer user_data)
{
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->id == ARM_INS_B)
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
  cpu_context->r[0] = 0xc001;
}

TESTCASE (arm_many_callouts_should_not_overflow_literal_pool)
{
  gint num_callouts = 0;
  guint32 * code;
  GumArmWriter cw;
  guint i;
  GumAddress func;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      add_callout_for_every_instruction, &num_callouts, NULL);
  fixture->sink->mask = GUM_NOTHING;

  code = g_malloc ((MANY_CALLOUTS_INSN_COUNT + 1) * sizeof (guint32));
  gum_arm_writer_init (&cw, code);
  for (i = 0; i != MANY_CALLOUTS_INSN_COUNT; i++)
    gum_arm_writer_put_add_reg_u16 (&cw, ARM_REG_R0, 1);
  gum_arm_writer_put_ret (&cw);
  gum_arm_writer_flush (&cw);

  func = test_arm_stalker_fixture_dup_code (fixture, code,
      gum_arm_writer_offset (&cw));

  gum_arm_writer_clear (&cw);
  g_free (code);

  FOLLOW_AND_INVOKE (func);

  g_assert_cmpint (num_callouts, >=, MANY_CALLOUTS_INSN_COUNT);
}

TESTCASE (thumb_many_callouts_should_not_overflow_literal_pool)
{
  gint num_callouts = 0;
  guint16 * code;
  GumThumbWriter cw;
  guint i;
  GumAddress func;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      add_callout_for_every_instruction, &num_callouts, NULL);
  fixture->sink->mask = GUM_NOTHING;

  /*
   * Each add emits an IT AL prefix plus the add itself, i.e. two halfwords,
   * so reserve two slots per instruction in addition to the trailing bx.
   */
  code = g_malloc (((MANY_CALLOUTS_INSN_COUNT * 2) + 1) * sizeof (guint16));
  gum_thumb_writer_init (&cw, code);
  for (i = 0; i != MANY_CALLOUTS_INSN_COUNT; i++)
    gum_thumb_writer_put_add_reg_imm (&cw, ARM_REG_R0, 1);
  gum_thumb_writer_put_bx_reg (&cw, ARM_REG_LR);
  gum_thumb_writer_flush (&cw);

  func = test_arm_stalker_fixture_dup_code (fixture, (guint32 *) code,
      gum_thumb_writer_offset (&cw));

  gum_thumb_writer_clear (&cw);
  g_free (code);

  FOLLOW_AND_INVOKE (func + 1);

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

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);
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

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);
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

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);
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

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);
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

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);
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

  INVOKE_ARM_EXPECTING (GUM_NOTHING, arm_flat_code, 2);
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

TESTCODE (arm_test_is_finished,
  0x00, 0x00, 0xa0, 0xe3, /* mov r0, 0       */
  0x1e, 0xff, 0x2f, 0xe1, /* bx lr           */
);

TESTCODE (thumb_test_is_finished,
  0x00, 0x1a,             /* subs r0, r0, r0 */
  0x70, 0x47,             /* bx lr           */
);

TESTCASE (arm_invalidation_for_current_thread_should_be_supported)
{
  test_invalidation_for_current_thread_with_target (
      DUP_TESTCODE (arm_test_is_finished),
      fixture);
}

TESTCASE (thumb_invalidation_for_current_thread_should_be_supported)
{
  test_invalidation_for_current_thread_with_target (
      DUP_TESTCODE (thumb_test_is_finished) + 1,
      fixture);
}

static void
test_invalidation_for_current_thread_with_target (
    GumAddress target,
    TestArmStalkerFixture * fixture)
{
  gboolean (* test_is_finished) (void) = GSIZE_TO_POINTER (target);
  InvalidationTransformContext ctx;

  ctx.stalker = fixture->stalker;
  ctx.target_function = test_is_finished;
  ctx.n = 0;

  g_clear_object (&fixture->transformer);
  fixture->transformer = gum_stalker_transformer_make_from_callback (
      modify_to_return_true_after_three_calls, &ctx, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer, NULL);

  while (!test_is_finished ())
  {
  }

  gum_stalker_unfollow_me (fixture->stalker);
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
          insn->address == (GPOINTER_TO_SIZE (ctx->target_function) & ~1);

      if (in_target_function && ctx->n == 0)
      {
        gum_stalker_iterator_put_callout (iterator,
            invalidate_after_three_calls, ctx, NULL);
      }
    }

    if (insn->id == ARM_INS_BX && in_target_function && ctx->n == 3)
    {
      if (output->encoding == GUM_INSTRUCTION_SPECIAL)
      {
        gum_thumb_writer_put_mov_reg_u8 (output->writer.thumb, ARM_REG_R0,
            TRUE);
      }
      else
      {
        gum_arm_writer_put_ldr_reg_address (output->writer.arm, ARM_REG_R0,
            TRUE);
      }
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

TESTCASE (arm_invalidation_for_specific_thread_should_be_supported)
{
  test_invalidation_for_specific_thread_with_target (
      DUP_TESTCODE (arm_test_is_finished),
      fixture);
}

TESTCASE (thumb_invalidation_for_specific_thread_should_be_supported)
{
  test_invalidation_for_specific_thread_with_target (
      DUP_TESTCODE (thumb_test_is_finished) + 1,
      fixture);
}

static void
test_invalidation_for_specific_thread_with_target (
    GumAddress target,
    TestArmStalkerFixture * fixture)
{
  gboolean (* test_is_finished) (void) = GSIZE_TO_POINTER (target);
  InvalidationTarget a, b;

  start_invalidation_target (&a, test_is_finished, fixture);
  start_invalidation_target (&b, test_is_finished, fixture);

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
                           gconstpointer target_function,
                           TestArmStalkerFixture * fixture)
{
  InvalidationTransformContext * ctx = &target->ctx;
  StalkerDummyChannel * channel = &target->channel;

  ctx->stalker = fixture->stalker;
  ctx->target_function = target_function;
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
  gboolean (* test_is_finished) (void) = target->ctx.target_function;
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
          insn->address == (GPOINTER_TO_SIZE (ctx->target_function) & ~1);
      if (in_target_function)
        ctx->n++;
    }

    if (insn->id == ARM_INS_BX && in_target_function && ctx->n > 1)
    {
      if (output->encoding == GUM_INSTRUCTION_SPECIAL)
      {
        gum_thumb_writer_put_mov_reg_u8 (output->writer.thumb, ARM_REG_R0,
            TRUE);
      }
      else
      {
        gum_arm_writer_put_ldr_reg_address (output->writer.arm, ARM_REG_R0,
            TRUE);
      }
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCODE (arm_get_magic_number,
  0x2a, 0x00, 0xa0, 0xe3, /* mov r0, 42  */
  0x1e, 0xff, 0x2f, 0xe1, /* bx lr       */
);

TESTCODE (thumb_get_magic_number,
  0x2a, 0x20,             /* movs r0, 42 */
  0x70, 0x47,             /* bx lr       */
);

TESTCASE (arm_invalidation_should_allow_block_to_grow)
{
  test_invalidation_block_growth_with_target (
      DUP_TESTCODE (arm_get_magic_number),
      fixture);
}

TESTCASE (thumb_invalidation_should_allow_block_to_grow)
{
  test_invalidation_block_growth_with_target (
      DUP_TESTCODE (thumb_get_magic_number) + 1,
      fixture);
}

static void
test_invalidation_block_growth_with_target (GumAddress target,
                                            TestArmStalkerFixture * fixture)
{
  int (* get_magic_number) (void) = GSIZE_TO_POINTER (target);
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
          insn->address == (GPOINTER_TO_SIZE (ctx->target_function) & ~1);
    }

    if (insn->id == ARM_INS_BX && in_target_function)
    {
      guint increment_index;

      for (increment_index = 0; increment_index != ctx->n; increment_index++)
      {
        if (output->encoding == GUM_INSTRUCTION_SPECIAL)
        {
          gum_thumb_writer_put_add_reg_imm (output->writer.thumb, ARM_REG_R0,
              1);
        }
        else
        {
          gum_arm_writer_put_add_reg_u16 (output->writer.arm, ARM_REG_R0, 1);
        }
      }
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCODE (arm_ldrex_strex,
  0x44, 0x00, 0x9f, 0xe5, /* ldr r0, [pointer_to_value] */
  /* retry:                                             */
  0x9f, 0x1f, 0x90, 0xe1, /* ldrex r1, [r0]             */
  0x01, 0x00, 0x51, 0xe3, /* cmp r1, 1                  */
  0x0b, 0x00, 0x00, 0x0a, /* beq nope                   */
  0x02, 0x00, 0x51, 0xe3, /* cmp r1, 2                  */
  0x09, 0x00, 0x00, 0x0a, /* beq nope                   */
  0x03, 0x00, 0x51, 0xe3, /* cmp r1, 3                  */
  0x07, 0x00, 0x00, 0x0a, /* beq nope                   */
  0x04, 0x00, 0x51, 0xe3, /* cmp r1, 4                  */
  0x05, 0x00, 0x00, 0x0a, /* beq nope                   */
  0x01, 0x10, 0x81, 0xe2, /* add r1, r1, 1              */
  0x91, 0x2f, 0x80, 0xe1, /* strex r2, r1, [r0]         */
  0x00, 0x00, 0x52, 0xe3, /* cmp r2, 0                  */
  0xf2, 0xff, 0xff, 0x1a, /* bne retry                  */
  0x01, 0x00, 0xa0, 0xe3, /* mov r0, 1                  */
  0x1e, 0xff, 0x2f, 0xe1, /* bx lr                      */
  /* nope:                                              */
  0x1f, 0xf0, 0x7f, 0xf5, /* clrex                      */
  0x00, 0x00, 0xa0, 0xe3, /* mov r0, 0                  */
  0x1e, 0xff, 0x2f, 0xe1, /* bx lr                      */
  /* pointer_to_value:                                  */
  0x11, 0x22, 0x33, 0x44,
);

TESTCASE (arm_exclusive_load_store_should_not_be_disturbed)
{
  guint32 code[CODE_SIZE (arm_ldrex_strex) / sizeof (guint32)], val;
  gint num_cmp_callouts;

  memcpy (code, arm_ldrex_strex, CODE_SIZE (arm_ldrex_strex));
  code[G_N_ELEMENTS (code) - 1] = GPOINTER_TO_SIZE (&val);

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      insert_callout_after_cmp, &num_cmp_callouts, NULL);

  val = 5;
  num_cmp_callouts = 0;
  INVOKE_ARM_EXPECTING (GUM_EXEC, code, 1);
  g_assert_cmpint (val, ==, 6);
  g_assert_cmpint (num_cmp_callouts, ==, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==, 19);
}

TESTCODE (thumb_ldrex_strex,
  0x0c, 0x48,             /* ldr r0, [pointer_to_value] */
  /* retry:                                             */
  0x50, 0xe8, 0x00, 0x1f, /* ldrex r1, [r0]             */
  0x01, 0x29,             /* cmp r1, 1                  */
  0x0e, 0xd0,             /* beq nope                   */
  0x02, 0x29,             /* cmp r1, 2                  */
  0x0c, 0xd0,             /* beq nope                   */
  0x03, 0x29,             /* cmp r1, 3                  */
  0x0a, 0xd0,             /* beq nope                   */
  0x04, 0x29,             /* cmp r1, 4                  */
  0x08, 0xd0,             /* beq nope                   */
  0x01, 0xf1, 0x01, 0x01, /* add.w r1, r1, 1            */
  0x40, 0xe8, 0x00, 0x12, /* strex r2, r1, [r0]         */
  0x00, 0x2a,             /* cmp r2, 0                  */
  0xef, 0xd1,             /* bne retry                  */
  0x4f, 0xf0, 0x01, 0x00, /* mov.w r0, 1                */
  0x70, 0x47,             /* bx lr                      */
  /* nope:                                              */
  0xbf, 0xf3, 0x2f, 0x8f, /* clrex                      */
  0x4f, 0xf0, 0x00, 0x00, /* mov.w r0, 0                */
  0x70, 0x47,             /* bx lr                      */
  0x00, 0x00,             /* <alignment padding>        */
  /* pointer_to_value:                                  */
  0x11, 0x22, 0x33, 0x44,
);

TESTCASE (thumb_exclusive_load_store_should_not_be_disturbed)
{
  guint32 code[CODE_SIZE (thumb_ldrex_strex) / sizeof (guint32)], val;
  gint num_cmp_callouts;

  memcpy (code, thumb_ldrex_strex, CODE_SIZE (thumb_ldrex_strex));
  code[G_N_ELEMENTS (code) - 1] = GPOINTER_TO_SIZE (&val);

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      insert_callout_after_cmp, &num_cmp_callouts, NULL);

  val = 5;
  num_cmp_callouts = 0;
  INVOKE_THUMB_EXPECTING (GUM_EXEC, code, 1);
  g_assert_cmpint (val, ==, 6);
  g_assert_cmpint (num_cmp_callouts, ==, 4);

  g_assert_cmpuint (fixture->sink->events->length, ==, 19);
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

    if (insn->id == ARM_INS_CMP && access == GUM_MEMORY_ACCESS_OPEN)
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

    fixture->sink->mask = GUM_CALL | GUM_RET;
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

TESTCASE (pthread_create)
{
  int ret;
  pthread_t thread;
  int number = 0;

  fixture->sink->mask = GUM_NOTHING;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  ret = pthread_create (&thread, NULL, increment_integer, &number);
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

TESTCASE (heap_api)
{
  gpointer p;

#if defined (HAVE_ANDROID) && defined (HAVE_ARM)
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  p = malloc (1);
  free (p);
  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
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
patch_code_pointer_on_thread (CodePatcher * patcher,
                              GumAddress code,
                              guint offset,
                              GumAddress value)
{
  gint request;

  /*
   * Run the patch on the unstalked patcher thread: tracing patch_code()'s
   * /proc/self/maps read never settles at trust threshold -1.
   */
  patcher->code = code;
  patcher->offset = offset;
  patcher->value = value;

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
      patch_code_pointer (self->code, self->offset, self->value);

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
patch_code_pointer (GumAddress code,
                    guint offset,
                    GumAddress value)
{
  gum_memory_patch_code (GSIZE_TO_POINTER (code + offset), sizeof (gpointer),
      patch_code_pointer_slot, GSIZE_TO_POINTER (value));
}

static void
patch_code_pointer_slot (gpointer mem,
                         gpointer user_data)
{
  gpointer * slot = mem;
  gpointer value = user_data;

  *slot = value;
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
   * Since our test has approx 1300 blocks, we don't need to worry about this.
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
