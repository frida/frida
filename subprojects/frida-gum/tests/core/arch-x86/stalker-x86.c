/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2010-2013 Karl Trygve Kalleberg <karltk@boblycat.org>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "stalker-x86-fixture.c"

#ifndef HAVE_WINDOWS
# include <lzma.h>
#endif

#ifdef HAVE_LINUX
# include <errno.h>
# include <fcntl.h>
# include <unistd.h>
# include <pthread.h>
# include <sys/wait.h>
# ifndef F_SETPIPE_SZ
#  define F_SETPIPE_SZ 1031
# endif
#endif

TESTLIST_BEGIN (stalker)
  TESTENTRY (no_events)
  TESTENTRY (call)
  TESTENTRY (ret)
  TESTENTRY (exec)
  TESTENTRY (call_depth)
  TESTENTRY (call_probe)
  TESTENTRY (custom_transformer)
  TESTENTRY (transformer_should_be_able_to_skip_call)
  TESTENTRY (transformer_should_be_able_to_replace_call_with_callout)
  TESTENTRY (transformer_should_be_able_to_replace_tailjump_with_callout)
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

  TESTENTRY (unconditional_jumps)
  TESTENTRY (short_conditional_jump_true)
  TESTENTRY (short_conditional_jump_false)
  TESTENTRY (short_conditional_jcxz_true)
  TESTENTRY (short_conditional_jcxz_false)
  TESTENTRY (long_conditional_jump)
  TESTENTRY (follow_return)
  TESTENTRY (follow_stdcall)
  TESTENTRY (follow_repne_ret)
  TESTENTRY (follow_repne_jb)
  TESTENTRY (unfollow_deep)
  TESTENTRY (call_followed_by_junk)
  TESTENTRY (indirect_call_with_immediate)
  TESTENTRY (indirect_call_with_register_and_no_immediate)
  TESTENTRY (indirect_call_with_register_and_positive_byte_immediate)
  TESTENTRY (indirect_call_with_register_and_negative_byte_immediate)
  TESTENTRY (indirect_call_with_register_and_positive_dword_immediate)
  TESTENTRY (indirect_call_with_register_and_negative_dword_immediate)
#if GLIB_SIZEOF_VOID_P == 8
  TESTENTRY (indirect_call_with_extended_registers_and_immediate)
#endif
  TESTENTRY (indirect_call_with_esp_and_byte_immediate)
  TESTENTRY (indirect_call_with_esp_and_dword_immediate)
  TESTENTRY (indirect_jump_with_immediate)
  TESTENTRY (indirect_jump_with_immediate_and_scaled_register)
  TESTENTRY (direct_call_with_register)
#if GLIB_SIZEOF_VOID_P == 8
  TESTENTRY (direct_call_with_extended_register)
#endif
  TESTENTRY (popcnt)
#if GLIB_SIZEOF_VOID_P == 4
  TESTENTRY (no_register_clobber)
#endif
  TESTENTRY (no_red_zone_clobber)
  TESTENTRY (big_block)

  TESTENTRY (heap_api)
  TESTENTRY (follow_syscall)
  TESTENTRY (follow_thread)
#ifdef HAVE_LINUX
  TESTENTRY (create_thread)
#endif
  TESTENTRY (unfollow_should_handle_terminated_thread)
  TESTENTRY (self_modifying_code_should_be_detected_with_threshold_minus_one)
  TESTENTRY (self_modifying_code_should_not_be_detected_with_threshold_zero)
  TESTENTRY (self_modifying_code_should_be_detected_with_threshold_one)
#ifndef HAVE_WINDOWS
  TESTENTRY (performance)
#endif

#ifdef HAVE_WINDOWS
# if GLIB_SIZEOF_VOID_P == 4
  TESTENTRY (win32_indirect_call_seg)
# endif
  TESTENTRY (win32_messagebeep_api)
  TESTENTRY (win32_follow_user_to_kernel_to_callback)
  TESTENTRY (win32_follow_callback_to_kernel_to_user)
#endif

#ifdef HAVE_LINUX
  TESTENTRY (prefetch)
  TESTENTRY (prefetch_backpatch)
  TESTENTRY (observer)
#endif

#ifndef HAVE_WINDOWS
  TESTENTRY (ic_var)
#endif

#if defined (HAVE_LINUX) && !defined (HAVE_ANDROID)
  TESTGROUP_BEGIN ("ExceptionHandling")
    TESTENTRY (no_exceptions)
    TESTENTRY (try_and_catch)
    TESTENTRY (try_and_catch_excluded)
    TESTENTRY (try_and_dont_catch)
    TESTENTRY (try_and_dont_catch_excluded)
  TESTGROUP_END ()
#endif

  TESTGROUP_BEGIN ("RunOnThread")
    TESTENTRY (run_on_thread_current)
    TESTENTRY (run_on_thread_current_sync)
    TESTENTRY (run_on_thread_other)
    TESTENTRY (run_on_thread_other_sync)
  TESTGROUP_END ()
TESTLIST_END ()

#ifdef HAVE_LINUX

#define GUM_TYPE_TEST_STALKER_OBSERVER (gum_test_stalker_observer_get_type ())
G_DECLARE_FINAL_TYPE (GumTestStalkerObserver, gum_test_stalker_observer, GUM,
                      TEST_STALKER_OBSERVER, GObject)

typedef struct _PrefetchBackpatchContext PrefetchBackpatchContext;

struct _GumTestStalkerObserver
{
  GObject parent;

  guint64 total;
};

struct _PrefetchBackpatchContext
{
  GumStalker * stalker;
  int pipes[2];
  GumTestStalkerObserver * observer;
  const GumMemoryRange * runner_range;
  GumStalkerTransformer * transformer;
  gboolean entry_reached;
  guint count;
};

#endif

typedef struct _RunOnThreadCtx RunOnThreadCtx;
typedef struct _TestThreadSyncData TestThreadSyncData;

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

static gpointer run_stalked_briefly (gpointer data);
#ifdef HAVE_LINUX
static gpointer run_spawned_thread (gpointer data);
#endif
static gpointer run_stalked_into_termination (gpointer data);
static void patch_code (gpointer code, gconstpointer new_code, gsize size);
static void do_patch_instruction (gpointer mem, gpointer user_data);
#ifndef HAVE_WINDOWS
static void pretend_workload (const GumMemoryRange * runner_range);
#endif
static void insert_extra_increment_after_xor (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void store_xax (GumCpuContext * cpu_context, gpointer user_data);
static void skip_call (GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void replace_call_with_callout (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void replace_jmp_with_callout (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void callout_set_cool (GumCpuContext * cpu_context, gpointer user_data);
static void unfollow_during_transform (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void modify_to_return_true_after_three_calls (
    GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void invalidate_after_three_calls (GumCpuContext * cpu_context,
    gpointer user_data);
static void start_invalidation_target (InvalidationTarget * target,
    gconstpointer target_function, TestStalkerFixture * fixture);
static void join_invalidation_target (InvalidationTarget * target);
static gpointer run_stalked_until_finished (gpointer data);
static void modify_to_return_true_on_subsequent_transform (
    GumStalkerIterator * iterator, GumStalkerOutput * output,
    gpointer user_data);
static void add_n_return_value_increments (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void invoke_follow_return_code (TestStalkerFixture * fixture);
static void invoke_unfollow_deep_code (TestStalkerFixture * fixture);

#ifdef HAVE_LINUX
static void prefetch_on_event (const GumEvent * event,
    GumCpuContext * cpu_context, gpointer user_data);
static void prefetch_run_child (GumStalker * stalker,
    const GumMemoryRange * runner_range, int compile_fd, int execute_fd);
static void prefetch_activation_target (void);
static void prefetch_write_blocks (int fd, GHashTable * table);
static void prefetch_read_blocks (int fd, GHashTable * table);

static void prefetch_backpatch_tranform (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void entry_callout (GumCpuContext * cpu_context, gpointer user_data);
static int prefetch_on_fork (void);
static void prefetch_backpatch_simple_workload (
    const GumMemoryRange * runner_range);

static void gum_test_stalker_observer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_test_stalker_observer_class_init (
    GumTestStalkerObserverClass * klass);
static void gum_test_stalker_observer_init (GumTestStalkerObserver * self);
static void gum_test_stalker_observer_increment_total (
    GumStalkerObserver * observer);
static void gum_test_stalker_observer_notify_backpatch (
    GumStalkerObserver * self, const GumBackpatch * backpatch, gsize size);

static gsize get_max_pipe_size (void);

G_DEFINE_TYPE_EXTENDED (GumTestStalkerObserver,
                        gum_test_stalker_observer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_STALKER_OBSERVER,
                            gum_test_stalker_observer_iface_init))

static GHashTable * prefetch_compiled = NULL;
static GHashTable * prefetch_executed = NULL;
static PrefetchBackpatchContext bp_ctx;

#endif

static void run_on_thread (const GumCpuContext * cpu_context,
    gpointer user_data);
static GThread * create_sleeping_dummy_thread_sync (gboolean * done,
    GumThreadId * thread_id);
static gpointer sleeping_dummy (gpointer data);

static const guint8 flat_code[] = {
  0x33, 0xc0, /* xor eax, eax */
  0xff, 0xc0, /* inc eax      */
  0xff, 0xc0, /* inc eax      */
  0xc3        /* retn         */
};

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

  /*gum_fake_event_sink_dump (fixture->sink);*/
}

TESTCASE (follow_syscall)
{
  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  g_usleep (1);
  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);

  /*gum_fake_event_sink_dump (fixture->sink);*/
}

TESTCASE (follow_thread)
{
  StalkerDummyChannel channel;
  GThread * thread;
  GumThreadId thread_id;

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

#ifdef HAVE_LINUX

TESTCASE (create_thread)
{
  pthread_t thread;
  gpointer result;

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  pthread_create (&thread, NULL, run_spawned_thread, NULL);
  pthread_join (thread, &result);

  gum_stalker_unfollow_me (fixture->stalker);

  g_assert (result == GSIZE_TO_POINTER (0xdeadface));
}

static gpointer
run_spawned_thread (gpointer data)
{
  return GSIZE_TO_POINTER (0xdeadface);
}

#endif

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
  guint8 mov_eax_imm_plus_nop[] = {
    0xb8, 0x00, 0x00, 0x00, 0x00, /* mov eax, <imm> */
    0x90                          /* nop padding    */
  };

  f = GUM_POINTER_TO_FUNCPTR (FlatFunc,
      test_stalker_fixture_dup_code (fixture, flat_code, sizeof (flat_code)));

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_set_trust_threshold (fixture->stalker, -1);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_assert_cmpuint (f (), ==, 2);

  *((guint32 *) (mov_eax_imm_plus_nop + 1)) = 42;
  patch_code (f, mov_eax_imm_plus_nop, sizeof (mov_eax_imm_plus_nop));
  g_assert_cmpuint (f (), ==, 42);
  f ();
  f ();

  *((guint32 *) (mov_eax_imm_plus_nop + 1)) = 1337;
  patch_code (f, mov_eax_imm_plus_nop, sizeof (mov_eax_imm_plus_nop));
  g_assert_cmpuint (f (), ==, 1337);

  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (self_modifying_code_should_not_be_detected_with_threshold_zero)
{
  FlatFunc f;
  guint8 mov_eax_imm_plus_nop[] = {
    0xb8, 0x00, 0x00, 0x00, 0x00, /* mov eax, <imm> */
    0x90                          /* nop padding    */
  };

  f = GUM_POINTER_TO_FUNCPTR (FlatFunc,
      test_stalker_fixture_dup_code (fixture, flat_code, sizeof (flat_code)));

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_set_trust_threshold (fixture->stalker, 0);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_assert_cmpuint (f (), ==, 2);

  *((guint32 *) (mov_eax_imm_plus_nop + 1)) = 42;
  patch_code (f, mov_eax_imm_plus_nop, sizeof (mov_eax_imm_plus_nop));
  g_assert_cmpuint (f (), ==, 2);

  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

TESTCASE (self_modifying_code_should_be_detected_with_threshold_one)
{
  FlatFunc f;
  guint8 mov_eax_imm_plus_nop[] = {
    0xb8, 0x00, 0x00, 0x00, 0x00, /* mov eax, <imm> */
    0x90                          /* nop padding    */
  };

  f = GUM_POINTER_TO_FUNCPTR (FlatFunc,
      test_stalker_fixture_dup_code (fixture, flat_code, sizeof (flat_code)));

  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_set_trust_threshold (fixture->stalker, 1);
  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));

  g_assert_cmpuint (f (), ==, 2);

  *((guint32 *) (mov_eax_imm_plus_nop + 1)) = 42;
  patch_code (f, mov_eax_imm_plus_nop, sizeof (mov_eax_imm_plus_nop));
  g_assert_cmpuint (f (), ==, 42);
  f ();
  f ();

  *((guint32 *) (mov_eax_imm_plus_nop + 1)) = 1337;
  patch_code (f, mov_eax_imm_plus_nop, sizeof (mov_eax_imm_plus_nop));
  g_assert_cmpuint (f (), ==, 42);

  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);
}

static void
patch_code (gpointer code,
            gconstpointer new_code,
            gsize size)
{
  PatchCodeContext ctx = { new_code, size };

  gum_memory_patch_code (code, size, do_patch_instruction, &ctx);
}

static void
do_patch_instruction (gpointer mem,
                      gpointer user_data)
{
  PatchCodeContext * ctx = user_data;

  memcpy (mem, ctx->code, ctx->size);
}

#ifndef HAVE_WINDOWS

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

#endif

static StalkerTestFunc
invoke_flat_expecting_return_value (TestStalkerFixture * fixture,
                                    GumEventType mask,
                                    guint expected_return_value)
{
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, flat_code, sizeof (flat_code)));

  fixture->sink->mask = mask;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, -1);
  g_assert_cmpint (ret, ==, expected_return_value);

  return func;
}

static StalkerTestFunc
invoke_flat (TestStalkerFixture * fixture,
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
  g_assert_cmpint (g_array_index (fixture->sink->events, GumEvent, 0).type,
      ==, GUM_CALL);
  ev = &g_array_index (fixture->sink->events, GumEvent, 0).call;
  GUM_ASSERT_CMPADDR (ev->location, ==, fixture->last_invoke_calladdr);
  GUM_ASSERT_CMPADDR (ev->target, ==, func);
}

TESTCASE (ret)
{
  StalkerTestFunc func;
  GumRetEvent * ev;

  func = invoke_flat (fixture, GUM_RET);

  g_assert_cmpuint (fixture->sink->events->length, ==, 1);
  g_assert_cmpint (g_array_index (fixture->sink->events, GumEvent, 0).type,
      ==, GUM_RET);
  ev = &g_array_index (fixture->sink->events, GumEvent, 0).ret;
  GUM_ASSERT_CMPADDR (ev->location,
      ==, ((guint8 *) GSIZE_TO_POINTER (func)) + 6);
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
  ev = &g_array_index (fixture->sink->events, GumEvent,
      INVOKER_IMPL_OFFSET).ret;
  GUM_ASSERT_CMPADDR (ev->location, ==, func);
}

TESTCASE (call_depth)
{
  const guint8 code[] =
  {
    0xb8, 0x07, 0x00, 0x00, 0x00, /* mov eax, 7 */
    0xff, 0xc8,                   /* dec eax    */
    0x74, 0x05,                   /* jz +5      */
    0xe8, 0xf7, 0xff, 0xff, 0xff, /* call -9    */
    0xc3,                         /* ret        */
    0xcc,                         /* int3       */
  };
  StalkerTestFunc func;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code, sizeof (code)));

  fixture->sink->mask = GUM_CALL | GUM_RET;
  test_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, 7 + 7 + 1);
  g_assert_cmpint (NTH_EVENT_AS_CALL (0)->depth, ==, 0);
  g_assert_cmpint (NTH_EVENT_AS_CALL (1)->depth, ==, 1);
  g_assert_cmpint (NTH_EVENT_AS_CALL (2)->depth, ==, 2);
  g_assert_cmpint (NTH_EVENT_AS_CALL (3)->depth, ==, 3);
  g_assert_cmpint (NTH_EVENT_AS_CALL (4)->depth, ==, 4);
  g_assert_cmpint (NTH_EVENT_AS_CALL (5)->depth, ==, 5);
  g_assert_cmpint (NTH_EVENT_AS_CALL (6)->depth, ==, 6);
  g_assert_cmpint (NTH_EVENT_AS_RET (7)->depth, ==, 7);
  g_assert_cmpint (NTH_EVENT_AS_RET (8)->depth, ==, 6);
  g_assert_cmpint (NTH_EVENT_AS_RET (9)->depth, ==, 5);
  g_assert_cmpint (NTH_EVENT_AS_RET (10)->depth, ==, 4);
  g_assert_cmpint (NTH_EVENT_AS_RET (11)->depth, ==, 3);
  g_assert_cmpint (NTH_EVENT_AS_RET (12)->depth, ==, 2);
  g_assert_cmpint (NTH_EVENT_AS_RET (13)->depth, ==, 1);
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
  const guint8 code_template[] =
  {
    0x68, 0x44, 0x44, 0xaa, 0xaa, /* push 0xaaaa4444     */
    0x68, 0x33, 0x33, 0xaa, 0xaa, /* push 0xaaaa3333     */
    0xba, 0x22, 0x22, 0xaa, 0xaa, /* mov edx, 0xaaaa2222 */
    0xb9, 0x11, 0x11, 0xaa, 0xaa, /* mov ecx, 0xaaaa1111 */
    0xe8, 0x1b, 0x00, 0x00, 0x00, /* call func_a         */
    0x68, 0x44, 0x44, 0xaa, 0xaa, /* push 0xbbbb4444     */
    0x68, 0x33, 0x33, 0xaa, 0xaa, /* push 0xbbbb3333     */
    0xba, 0x22, 0x22, 0xaa, 0xaa, /* mov edx, 0xbbbb2222 */
    0xb9, 0x11, 0x11, 0xaa, 0xaa, /* mov ecx, 0xbbbb1111 */
    0xe8, 0x06, 0x00, 0x00, 0x00, /* call func_b         */
    0xc3,                         /* ret                 */

    0xcc,                         /* int 3               */

    /* func_a: */
    0xc2, 2 * sizeof (gpointer), 0x00, /* ret x          */

    0xcc,                         /* int 3               */

    /* func_b: */
    0xc2, 2 * sizeof (gpointer), 0x00, /* ret x          */
  };
  StalkerTestFunc func;
  guint8 * func_a;
  CallProbeContext probe_ctx, secondary_probe_ctx;
  GumProbeId probe_id;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code_template,
          sizeof (code_template)));

  func_a = fixture->code + 52;

  probe_ctx.num_calls = 0;
  probe_ctx.target_address = fixture->code + 52;
  probe_ctx.return_address = fixture->code + 25;
  probe_id = gum_stalker_add_call_probe (fixture->stalker, func_a,
      probe_func_a_invocation, &probe_ctx, NULL);
  test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 1);

  secondary_probe_ctx.num_calls = 0;
  secondary_probe_ctx.target_address = probe_ctx.target_address;
  secondary_probe_ctx.return_address = probe_ctx.return_address;
  gum_stalker_add_call_probe (fixture->stalker, func_a, probe_func_a_invocation,
      &secondary_probe_ctx, NULL);
  test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (probe_ctx.num_calls, ==, 2);
  g_assert_cmpuint (secondary_probe_ctx.num_calls, ==, 1);

  gum_stalker_remove_call_probe (fixture->stalker, probe_id);
  test_stalker_fixture_follow_and_invoke (fixture, func, 0);
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

#if GLIB_SIZEOF_VOID_P == 4
  g_assert_cmphex (GPOINTER_TO_SIZE (
      gum_cpu_context_get_nth_argument (cpu_context, 0)), ==, 0xaaaa3333);
  g_assert_cmphex (GPOINTER_TO_SIZE (
      gum_cpu_context_get_nth_argument (cpu_context, 1)), ==, 0xaaaa4444);
#endif

  g_assert_cmphex (stack_values[0], ==, GPOINTER_TO_SIZE (ctx->return_address));
  g_assert_cmphex (stack_values[1] & 0xffffffff, ==, 0xaaaa3333);
  g_assert_cmphex (stack_values[2] & 0xffffffff, ==, 0xaaaa4444);

  g_assert_cmphex (GUM_CPU_CONTEXT_XIP (cpu_context),
      ==, GPOINTER_TO_SIZE (ctx->target_address));
#if GLIB_SIZEOF_VOID_P == 4
  g_assert_cmphex (cpu_context->ecx, ==, 0xaaaa1111);
  g_assert_cmphex (cpu_context->edx, ==, 0xaaaa2222);
#else
  g_assert_cmphex (cpu_context->rcx & 0xffffffff, ==, 0xaaaa1111);
  g_assert_cmphex (cpu_context->rdx & 0xffffffff, ==, 0xaaaa2222);
#endif
}

static const guint8 jumpy_code[] = {
  0x31, 0xc0,                   /* xor eax, eax */
  0xeb, 0x01,                   /* jmp short +1 */
  0xcc,                         /* int3         */
  0xff, 0xc0,                   /* inc eax      */
  0xe9, 0x02, 0x00, 0x00, 0x00, /* jmp near +2  */
  0xcc,                         /* int3         */
  0xcc,                         /* int3         */
  0xc3                          /* ret          */
};

static StalkerTestFunc
invoke_jumpy (TestStalkerFixture * fixture,
              GumEventType mask)
{
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, jumpy_code, sizeof (jumpy_code)));

  fixture->sink->mask = mask;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, -1);
  g_assert_cmpint (ret, ==, 1);

  return func;
}

TESTCASE (custom_transformer)
{
  gsize last_xax = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      insert_extra_increment_after_xor, &last_xax, NULL);

  g_assert_cmpuint (last_xax, ==, 0);

  invoke_flat_expecting_return_value (fixture, GUM_NOTHING, 3);

  g_assert_cmpuint (last_xax, ==, 3);
}

static void
insert_extra_increment_after_xor (GumStalkerIterator * iterator,
                                  GumStalkerOutput * output,
                                  gpointer user_data)
{
  gsize * last_xax = user_data;
  const cs_insn * insn;
  gboolean in_leaf_func;

  in_leaf_func = FALSE;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (in_leaf_func && insn->id == X86_INS_RET)
    {
      gum_stalker_iterator_put_callout (iterator, store_xax, last_xax, NULL);
    }

    gum_stalker_iterator_keep (iterator);

    if (insn->id == X86_INS_XOR)
    {
      in_leaf_func = TRUE;

      gum_x86_writer_put_inc_reg (output->writer.x86, GUM_X86_EAX);
    }
  }
}

static void
store_xax (GumCpuContext * cpu_context,
           gpointer user_data)
{
  gsize * last_xax = user_data;

  *last_xax = GUM_CPU_CONTEXT_XAX (cpu_context);
}

TESTCASE (transformer_should_be_able_to_skip_call)
{
  guint8 code_template[] =
  {
    0xb8, 0x14, 0x05, 0x00, 0x00, /* mov eax, 1300    */
    0xe8, 0x01, 0x00, 0x00, 0x00, /* call bump_number */
    0xc3,                         /* ret              */
    /* bump_number:                                   */
    0x83, 0xc0, 0x25,             /* add eax, 37      */
    0xc3,                         /* ret              */
  };
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (skip_call,
      func, NULL);

  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (ret, ==, 1300);
}

static void
skip_call (GumStalkerIterator * iterator,
           GumStalkerOutput * output,
           gpointer user_data)
{
  const guint8 * func_start = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->address == GPOINTER_TO_SIZE (func_start + 5))
      continue;

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (transformer_should_be_able_to_replace_call_with_callout)
{
  guint8 code_template[] =
  {
    0xb8, 0x14, 0x05, 0x00, 0x00, /* mov eax, 1300    */
    0xe8, 0x01, 0x00, 0x00, 0x00, /* call bump_number */
    0xc3,                         /* ret              */
    /* bump_number:                                   */
    0x83, 0xc0, 0x25,             /* add eax, 37      */
    0xc3,                         /* ret              */
  };
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      replace_call_with_callout, func, NULL);

  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (ret, ==, 0xc001);
}

static void
replace_call_with_callout (GumStalkerIterator * iterator,
                           GumStalkerOutput * output,
                           gpointer user_data)
{
  const guint8 * func_start = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->address == GPOINTER_TO_SIZE (func_start + 5))
    {
      gum_stalker_iterator_put_callout (iterator, callout_set_cool, NULL, NULL);
      continue;
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (transformer_should_be_able_to_replace_tailjump_with_callout)
{
  guint8 code_template[] =
  {
    0xb8, 0x14, 0x05, 0x00, 0x00, /* mov eax, 1300   */
    0xeb, 0x01,                   /* jmp bump_number */
    0x90,                         /* nop             */
    /* bump_number:                                  */
    0x83, 0xc0, 0x25,             /* add eax, 37     */
    0xc3,                         /* ret             */
  };
  StalkerTestFunc func;
  gint ret;

  func = (StalkerTestFunc) test_stalker_fixture_dup_code (fixture,
      code_template, sizeof (code_template));

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      replace_jmp_with_callout, func, NULL);

  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpuint (ret, ==, 0xc001);
}

static void
replace_jmp_with_callout (GumStalkerIterator * iterator,
                          GumStalkerOutput * output,
                          gpointer user_data)
{
  const guint8 * func_start = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    if (insn->address == GPOINTER_TO_SIZE (func_start + 5))
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
  GUM_CPU_CONTEXT_XAX (cpu_context) = 0xc001;
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

static const guint8 test_is_finished_code[] = {
  0x33, 0xc0, /* xor eax, eax */
  0xc3,       /* ret          */
};

TESTCASE (invalidation_for_current_thread_should_be_supported)
{
  TestIsFinishedFunc test_is_finished;
  InvalidationTransformContext ctx;

  test_is_finished = GUM_POINTER_TO_FUNCPTR (TestIsFinishedFunc,
      test_stalker_fixture_dup_code (fixture, test_is_finished_code,
          sizeof (test_is_finished_code)));

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

    if (insn->id == X86_INS_RET && in_target_function && ctx->n == 3)
    {
      gum_x86_writer_put_mov_reg_u32 (output->writer.x86, GUM_X86_EAX, TRUE);
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
  TestIsFinishedFunc test_is_finished;
  InvalidationTarget a, b;

  test_is_finished = GUM_POINTER_TO_FUNCPTR (TestIsFinishedFunc,
      test_stalker_fixture_dup_code (fixture, test_is_finished_code,
          sizeof (test_is_finished_code)));

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
                           TestStalkerFixture * fixture)
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
  TestIsFinishedFunc test_is_finished =
      GUM_POINTER_TO_FUNCPTR (TestIsFinishedFunc, target->ctx.target_function);
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

    if (insn->id == X86_INS_RET && in_target_function && ctx->n > 1)
    {
      gum_x86_writer_put_mov_reg_u32 (output->writer.x86, GUM_X86_EAX, TRUE);
    }

    gum_stalker_iterator_keep (iterator);
  }
}

static const guint8 get_magic_number_code[] = {
  0xb8, 0x2a, 0x00, 0x00, 0x00, /* mov eax, 42 */
  0xc3,                         /* ret         */
};

TESTCASE (invalidation_should_allow_block_to_grow)
{
  GetMagicNumberFunc get_magic_number;
  InvalidationTransformContext ctx;

  get_magic_number = GUM_POINTER_TO_FUNCPTR (GetMagicNumberFunc,
      test_stalker_fixture_dup_code (fixture, get_magic_number_code,
          sizeof (get_magic_number_code)));

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
          insn->address == GPOINTER_TO_SIZE (ctx->target_function);
    }

    if (insn->id == X86_INS_RET && in_target_function)
    {
      guint increment_index;

      for (increment_index = 0; increment_index != ctx->n; increment_index++)
      {
        gum_x86_writer_put_inc_reg (output->writer.x86, GUM_X86_EAX);
      }
    }

    gum_stalker_iterator_keep (iterator);
  }
}

TESTCASE (unconditional_jumps)
{
  invoke_jumpy (fixture, GUM_EXEC);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 5);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 0),
      ==, fixture->code + 0);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 1),
      ==, fixture->code + 2);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 2),
      ==, fixture->code + 5);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 3),
      ==, fixture->code + 7);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 4),
      ==, fixture->code + 14);
}

static StalkerTestFunc
invoke_short_condy (TestStalkerFixture * fixture,
                    GumEventType mask,
                    gint arg)
{
  const guint8 code[] = {
    0x83, 0xf9, 0x2a,             /* cmp ecx, 42    */
    0x74, 0x05,                   /* jz +5          */
    0xe9, 0x06, 0x00, 0x00, 0x00, /* jmp dword +6   */

    0xb8, 0x39, 0x05, 0x00, 0x00, /* mov eax, 1337  */
    0xc3,                         /* ret            */

    0xb8, 0xcb, 0x04, 0x00, 0x00, /* mov eax, 1227  */
    0xc3,                         /* ret            */
  };
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code, sizeof (code)));

  fixture->sink->mask = mask;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, arg);

  g_assert_cmpint (ret, ==, (arg == 42) ? 1337 : 1227);

  return func;
}

TESTCASE (short_conditional_jump_true)
{
  invoke_short_condy (fixture, GUM_EXEC, 42);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 4);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 0),
      ==, fixture->code + 0);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 1),
      ==, fixture->code + 3);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 2),
      ==, fixture->code + 10);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 3),
      ==, fixture->code + 15);
}

TESTCASE (short_conditional_jump_false)
{
  invoke_short_condy (fixture, GUM_EXEC, 43);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 5);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 0),
      ==, fixture->code + 0);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 1),
      ==, fixture->code + 3);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 2),
      ==, fixture->code + 5);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 3),
      ==, fixture->code + 16);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 4),
      ==, fixture->code + 21);
}

static StalkerTestFunc
invoke_short_jcxz (TestStalkerFixture * fixture,
                   GumEventType mask,
                   gint arg)
{
  const guint8 code[] = {
    0xe3, 0x05,                   /* jecxz/jrcxz +5 */
    0xe9, 0x06, 0x00, 0x00, 0x00, /* jmp dword +6   */

    0xb8, 0x39, 0x05, 0x00, 0x00, /* mov eax, 1337  */
    0xc3,                         /* ret            */

    0xb8, 0xcb, 0x04, 0x00, 0x00, /* mov eax, 1227  */
    0xc3,                         /* ret            */
  };
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code, sizeof (code)));

  fixture->sink->mask = mask;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, arg);

  g_assert_cmpint (ret, ==, (arg == 0) ? 1337 : 1227);

  return func;
}

TESTCASE (short_conditional_jcxz_true)
{
  invoke_short_jcxz (fixture, GUM_EXEC, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 3);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 0),
      ==, fixture->code + 0);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 1),
      ==, fixture->code + 7);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 2),
      ==, fixture->code + 12);
}

TESTCASE (short_conditional_jcxz_false)
{
  invoke_short_jcxz (fixture, GUM_EXEC, 0x11223344);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 4);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 0),
      ==, fixture->code + 0);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 1),
      ==, fixture->code + 2);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 2),
      ==, fixture->code + 13);
  GUM_ASSERT_CMPADDR (NTH_EXEC_EVENT_LOCATION (INVOKER_IMPL_OFFSET + 3),
      ==, fixture->code + 18);
}

static StalkerTestFunc
invoke_long_condy (TestStalkerFixture * fixture,
                   GumEventType mask,
                   gint arg)
{
  const guint8 code[] = {
    0xe9, 0x0c, 0x01, 0x00, 0x00,         /* jmp +268             */

    0xb8, 0x39, 0x05, 0x00, 0x00,         /* mov eax, 1337        */
    0xc3,                                 /* ret                  */

    0xb8, 0xcb, 0x04, 0x00, 0x00,         /* mov eax, 1227        */
    0xc3,                                 /* ret                  */

    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,

    0x81, 0xc1, 0xff, 0xff, 0xff, 0xff,   /* add ecx, G_MAXUINT32 */
    0x0f, 0x83, 0xee, 0xfe, 0xff, 0xff,   /* jnc dword -274       */
    0xe9, 0xe3, 0xfe, 0xff, 0xff,         /* jmp dword -285       */
  };
  StalkerTestFunc func;
  gint ret;

  g_assert_true (arg == FALSE || arg == TRUE);

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code, sizeof (code)));

  fixture->sink->mask = mask;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, arg);

  g_assert_cmpint (ret, ==, (arg == TRUE) ? 1337 : 1227);

  return func;
}

TESTCASE (long_conditional_jump)
{
  invoke_long_condy (fixture, GUM_EXEC, TRUE);
  invoke_long_condy (fixture, GUM_EXEC, FALSE);
}

#if GLIB_SIZEOF_VOID_P == 4
# define FOLLOW_RETURN_EXTRA_INSN_COUNT 2
#elif GLIB_SIZEOF_VOID_P == 8
# if GUM_NATIVE_ABI_IS_WINDOWS
#  define FOLLOW_RETURN_EXTRA_INSN_COUNT 3
# else
#  define FOLLOW_RETURN_EXTRA_INSN_COUNT 1
# endif
#endif

TESTCASE (follow_return)
{
  fixture->sink->mask = GUM_EXEC;

  invoke_follow_return_code (fixture);

  g_assert_cmpuint (fixture->sink->events->length,
      ==, 5 + FOLLOW_RETURN_EXTRA_INSN_COUNT);
}

static void
invoke_follow_return_code (TestStalkerFixture * fixture)
{
  GumAddressSpec spec;
  guint8 * code;
  gsize page_size;
  GumX86Writer cw;
#if GLIB_SIZEOF_VOID_P == 4
  guint align_correction_follow = 12;
  guint align_correction_unfollow = 8;
#else
  guint align_correction_follow = 0;
  guint align_correction_unfollow = 8;
#endif
  const gchar * start_following_lbl = "start_following";
  GCallback invoke_func;

  spec.near_address = gum_stalker_follow_me;
  spec.max_distance = G_MAXINT32 / 2;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate_near (&spec, page_size, page_size, GUM_PAGE_RW);

  gum_x86_writer_init (&cw, code);

  gum_x86_writer_put_call_near_label (&cw, start_following_lbl);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_unfollow);
  gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_unfollow);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_put_label (&cw, start_following_lbl);
  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_follow);
  gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_follow);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_x86_writer_offset (&cw));
  gum_x86_writer_clear (&cw);

  invoke_func = GUM_POINTER_TO_FUNCPTR (GCallback, code);
  invoke_func ();

  gum_memory_free (code, page_size);
}

TESTCASE (follow_stdcall)
{
  const guint8 stdcall_code[] =
  {
    0x68, 0xef, 0xbe, 0x00, 0x00, /* push dword 0xbeef */
    0xe8, 0x02, 0x00, 0x00, 0x00, /* call func         */
    0xc3,                         /* ret               */
    0xcc,                         /* int3              */

  /* func: */
    0x8b, 0x44, 0x24,             /* mov eax, [esp+X]  */
          sizeof (gpointer),
    0xc2, sizeof (gpointer), 0x00 /* ret X             */
  };
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, stdcall_code,
          sizeof (stdcall_code)));

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 5);

  g_assert_cmpint (ret, ==, 0xbeef);
}

TESTCASE (follow_repne_ret)
{
  const guint8 repne_ret_code[] =
  {
    0xb8, 0xef, 0xbe, 0x00, 0x00, /* mov eax, 0xbeef     */
    0xf2, 0xc3,                   /* repne ret           */
    0xcc,                         /* int3                */
  };
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, repne_ret_code,
          sizeof (repne_ret_code)));

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 2);

  g_assert_cmpint (ret, ==, 0xbeef);
}

TESTCASE (follow_repne_jb)
{
  const guint8 repne_jb_code[] =
  {
    0x68, 0xef, 0xbe, 0x00, 0x00, /* push dword 0xbeef   */
    0xb8, 0xff, 0x00, 0x00, 0x00, /* mov eax, 0xff       */
    0xb9, 0xfe, 0x00, 0x00, 0x00, /* mov ecx, 0xfe       */
    0x3b, 0xc8,                   /* cmp ecx, eax        */
    0xf2, 0x72, 0x02,             /* repne jb short func */
    0xc3,                         /* ret                 */
    0xcc,                         /* int3                */

                                  /* func:               */
    0x58,                         /* pop eax             */
    0xc3,                         /* ret                 */
  };
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, repne_jb_code,
          sizeof (repne_jb_code)));

  g_assert_cmpint (func (0), ==, 0xbeef);

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 7);

  g_assert_cmpint (ret, ==, 0xbeef);
}

#if GLIB_SIZEOF_VOID_P == 4
#define UNFOLLOW_DEEP_EXTRA_INSN_COUNT 1
#elif GLIB_SIZEOF_VOID_P == 8
# if GUM_NATIVE_ABI_IS_WINDOWS
#  define UNFOLLOW_DEEP_EXTRA_INSN_COUNT 2
# else
#  define UNFOLLOW_DEEP_EXTRA_INSN_COUNT 0
# endif
#endif

TESTCASE (unfollow_deep)
{
  fixture->sink->mask = GUM_EXEC;

  invoke_unfollow_deep_code (fixture);

  g_assert_cmpuint (fixture->sink->events->length,
      ==, 7 + UNFOLLOW_DEEP_EXTRA_INSN_COUNT);
}

static void
invoke_unfollow_deep_code (TestStalkerFixture * fixture)
{
  GumAddressSpec spec;
  guint8 * code;
  gsize page_size;
  GumX86Writer cw;
#if GLIB_SIZEOF_VOID_P == 4
  guint align_correction_follow = 0;
  guint align_correction_unfollow = 12;
#else
  guint align_correction_follow = 8;
  guint align_correction_unfollow = 0;
#endif
  const gchar * func_a_lbl = "func_a";
  const gchar * func_b_lbl = "func_b";
  const gchar * func_c_lbl = "func_c";
  GCallback invoke_func;

  spec.near_address = gum_stalker_follow_me;
  spec.max_distance = G_MAXINT32 / 2;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate_near (&spec, page_size, page_size, GUM_PAGE_RW);

  gum_x86_writer_init (&cw, code);

  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_follow);
  gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_follow);
  gum_x86_writer_put_call_near_label (&cw, func_a_lbl);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_put_label (&cw, func_a_lbl);
  gum_x86_writer_put_call_near_label (&cw, func_b_lbl);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_put_label (&cw, func_b_lbl);
  gum_x86_writer_put_call_near_label (&cw, func_c_lbl);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_put_label (&cw, func_c_lbl);
  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_unfollow);
  gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_unfollow);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_x86_writer_offset (&cw));
  gum_x86_writer_clear (&cw);

  invoke_func = GUM_POINTER_TO_FUNCPTR (GCallback, code);
  invoke_func ();

  gum_memory_free (code, page_size);
}

TESTCASE (call_followed_by_junk)
{
  const guint8 code[] =
  {
    0xe8, 0x05, 0x00, 0x00, 0x00, /* call func         */
    0xff, 0xff, 0xff, 0xff, 0xff, /* <junk>            */
    0x58,                         /* pop eax           */
    0x68, 0xef, 0xbe, 0x00, 0x00, /* push dword 0xbeef */
    0x58,                         /* pop eax           */
    0xc3                          /* ret               */
  };
  StalkerTestFunc func;
  gint ret;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code, sizeof (code)));

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmpuint (fixture->sink->events->length,
      ==, INVOKER_INSN_COUNT + 5);

  g_assert_cmpint (ret, ==, 0xbeef);
}

typedef struct _CallTemplate CallTemplate;

struct _CallTemplate
{
  const guint8 * code_template;
  guint code_size;
  guint call_site_offset;
  guint target_mov_offset;
  guint target_address_offset;
  gboolean target_address_offset_points_directly_to_function;
  guint target_func_offset;
  gint target_func_immediate_fixup;
  guint instruction_count;
  guint ia32_padding_instruction_count;
  gboolean enable_probe;
};

static void probe_template_func_invocation (GumCallDetails * details,
    gpointer user_data);

static StalkerTestFunc
invoke_call_from_template (TestStalkerFixture * fixture,
                           const CallTemplate * call_template)
{
  guint8 * code;
  StalkerTestFunc func;
  gpointer target_func_address;
  gsize target_actual_address;
  guint expected_insn_count;
  gint ret;
  GumProbeId probe_id;

  code = test_stalker_fixture_dup_code (fixture,
      call_template->code_template, call_template->code_size);
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, code);

  gum_mprotect (code, call_template->code_size, GUM_PAGE_RW);

  target_func_address = code + call_template->target_func_offset;
  if (call_template->target_address_offset_points_directly_to_function)
    target_actual_address = GPOINTER_TO_SIZE (target_func_address);
  else
    target_actual_address = GPOINTER_TO_SIZE (&target_func_address);
  *((gsize *) (code + call_template->target_address_offset)) =
      target_actual_address + call_template->target_func_immediate_fixup;

#if GLIB_SIZEOF_VOID_P == 8
  if (call_template->target_mov_offset != 0)
    *(code + call_template->target_mov_offset - 1) = 0x48;
#endif

  gum_memory_mark_code (code, call_template->code_size);

  expected_insn_count = INVOKER_INSN_COUNT + call_template->instruction_count;
#if GLIB_SIZEOF_VOID_P == 4
  expected_insn_count += call_template->ia32_padding_instruction_count;
#endif

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpint (ret, ==, 1337);
  g_assert_cmpuint (fixture->sink->events->length, ==, expected_insn_count);

  gum_fake_event_sink_reset (fixture->sink);

  fixture->sink->mask = GUM_CALL;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpint (ret, ==, 1337);
  g_assert_cmpuint (fixture->sink->events->length, ==, 2 + 1);
  GUM_ASSERT_CMPADDR (NTH_EVENT_AS_CALL (1)->location,
      ==, code + call_template->call_site_offset);
  GUM_ASSERT_CMPADDR (NTH_EVENT_AS_CALL (1)->target,
      ==, code + call_template->target_func_offset);

  probe_id = gum_stalker_add_call_probe (fixture->stalker, target_func_address,
      probe_template_func_invocation, NULL, NULL);
  fixture->sink->mask = GUM_NOTHING;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpint (ret, == , 1337);
  gum_stalker_remove_call_probe (fixture->stalker, probe_id);

  return func;
}

static void
probe_template_func_invocation (GumCallDetails * details,
                                gpointer user_data)
{
}

TESTCASE (indirect_call_with_immediate)
{
  const guint8 code[] = {
    0xeb, 0x08,                         /* jmp +8          */

    0x00, 0x00, 0x00, 0x00,             /* address padding */
    0x00, 0x00, 0x00, 0x00,

    0xff, 0x15, 0xf2, 0xff, 0xff, 0xff, /* call            */
    0xc3,                               /* ret             */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337   */
    0xc3,                               /* ret             */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_address_offset = 12;
  call_template.target_func_offset = 17;
  call_template.instruction_count = 5;

#if GLIB_SIZEOF_VOID_P == 8
  call_template.target_address_offset -= 10;
  call_template.target_address_offset_points_directly_to_function = TRUE;
#endif

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (indirect_call_with_register_and_no_immediate)
{
  const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, X           */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0x10,                         /* call [xax]           */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3,                               /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_func_offset = 13;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (indirect_call_with_register_and_positive_byte_immediate)
{
  const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, X           */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0x50, 0x54,                   /* call [xax + 0x54]    */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3,                               /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_func_offset = 14;
  call_template.target_func_immediate_fixup = -0x54;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (indirect_call_with_register_and_negative_byte_immediate)
{
  const guint8 code[] = {
    0x90, 0xbd, 0x00, 0x00, 0x00, 0x00, /* mov xbp, X           */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0x55, 0xe4,                   /* call [xbp - 0x1c]    */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3,                               /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_func_offset = 14;
  call_template.target_func_immediate_fixup = 0x1c;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (indirect_call_with_register_and_positive_dword_immediate)
{
  const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, X           */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0x90, 0x54, 0x00, 0x00, 0x00, /* call [xax + 0x54]    */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3,                               /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_func_offset = 17;
  call_template.target_func_immediate_fixup = -0x54;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (indirect_call_with_register_and_negative_dword_immediate)
{
  const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, X           */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0x90, 0xbe, 0xab, 0xff, 0xff, /* call [xax - 0x5442]  */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3,                               /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_func_offset = 17;
  call_template.target_func_immediate_fixup = 0x5442;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

#if GLIB_SIZEOF_VOID_P == 8

TESTCASE (indirect_call_with_extended_registers_and_immediate)
{
  const guint8 code[] = {
    0x49, 0xbb, 0x00, 0x00, 0x00, 0x00, /* mov r11, X                   */
                0x00, 0x00, 0x00, 0x00,
    0x49, 0xba, 0x39, 0x05, 0x00, 0x00, /* mov r10, 1337                */
                0x00, 0x00, 0x00, 0x00,
    0x43, 0xff, 0x94, 0xd3,             /* call [r11 + r10*8 + 0x270e0] */
                0xe0, 0x70, 0x02, 0x00,
    0xc3,                               /* ret                          */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337                */
    0xc3,                               /* ret                          */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 20;
  call_template.target_address_offset = 2;
  call_template.target_func_offset = 29;
  call_template.target_func_immediate_fixup = -((1337 * 8) + 0x270e0);
  call_template.instruction_count = 6;

  invoke_call_from_template (fixture, &call_template);
}

#endif

TESTCASE (indirect_call_with_esp_and_byte_immediate)
{
const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, X          */
                0x90, 0x90, 0x90, 0x90,
    0x50,                               /* push xax            */
    0x56,                               /* push xsi            */
    0x57,                               /* push xdi            */
    0xff, 0x54, 0x24,                   /* call [xsp + Y]      */
          2 * sizeof (gpointer),
    0x5F,                               /* pop xdi             */
    0x5E,                               /* pop xsi             */
    0x59,                               /* pop xcx             */
    0xc3,                               /* ret                 */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337       */
    0xc3,                               /* ret                 */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 13;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_address_offset_points_directly_to_function = TRUE;
  call_template.target_func_offset = 21;
  call_template.instruction_count = 11;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (indirect_call_with_esp_and_dword_immediate)
{
  const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00,         /* mov xax, X          */
                0x90, 0x90, 0x90, 0x90,
    0x50,                                       /* push xax            */
    0x56,                                       /* push xsi            */
    0x57,                                       /* push xdi            */
    0xff, 0x94, 0x24,                           /* call [xsp + Y]      */
          2 * sizeof (gpointer), 0x00, 0x00, 0x00,
    0x5F,                                       /* pop xdi             */
    0x5E,                                       /* pop xsi             */
    0x59,                                       /* pop xcx             */
    0xc3,                                       /* ret                 */

    0xb8, 0x39, 0x05, 0x00, 0x00,               /* mov eax, 1337       */
    0xc3,                                       /* ret                 */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 13;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_address_offset_points_directly_to_function = TRUE;
  call_template.target_func_offset = 24;
  call_template.instruction_count = 11;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

TESTCASE (direct_call_with_register)
{
  const guint8 code[] = {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, X          */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0xd0,                         /* call xax             */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3                                /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 1;
  call_template.target_address_offset = 2;
  call_template.target_address_offset_points_directly_to_function = TRUE;
  call_template.target_func_offset = 13;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 5;

  invoke_call_from_template (fixture, &call_template);
}

#if GLIB_SIZEOF_VOID_P == 8

TESTCASE (direct_call_with_extended_register)
{
  const guint8 code[] = {
    0x49, 0xb9, 0x00, 0x00, 0x00, 0x00, /* mov r9, X            */
                0x00, 0x00, 0x00, 0x00,
    0x41, 0xff, 0xd1,                   /* call r9              */
    0xc3,                               /* ret                  */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337        */
    0xc3,                               /* ret                  */
  };
  CallTemplate call_template = { 0, };

  call_template.code_template = code;
  call_template.code_size = sizeof (code);
  call_template.call_site_offset = 10;
  call_template.target_mov_offset = 0;
  call_template.target_address_offset = 2;
  call_template.target_address_offset_points_directly_to_function = TRUE;
  call_template.target_func_offset = 14;
  call_template.instruction_count = 5;
  call_template.ia32_padding_instruction_count = 0;

  invoke_call_from_template (fixture, &call_template);
}

#endif

TESTCASE (popcnt)
{
  const guint8 code[] =
  {
    0xf3, 0x0f, 0xb8, 0xcb, /* popcnt ecx, ebx */
    0xc3,                   /* ret             */
    0xcc,                   /* int3            */
  };
  StalkerTestFunc func;

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code, sizeof (code)));

  fixture->sink->mask = GUM_NOTHING;
  test_stalker_fixture_follow_and_invoke (fixture, func, 0);
}

typedef struct _JumpTemplate JumpTemplate;

struct _JumpTemplate
{
  const guint8 * code_template;
  guint code_size;
  guint offset_of_target_pointer;
  gboolean offset_of_target_pointer_points_directly;
  guint offset_of_target;
  gint target_immediate_fixup;
  guint instruction_count;
  guint ia32_padding_instruction_count;
};

static StalkerTestFunc
invoke_jump (TestStalkerFixture * fixture,
             JumpTemplate * jump_template)
{
  guint8 * code;
  StalkerTestFunc func;
  gpointer target_address;
  gsize target_actual_address;
  guint expected_insn_count;
  gint ret;

  code = test_stalker_fixture_dup_code (fixture, jump_template->code_template,
      jump_template->code_size);
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, code);

  gum_mprotect (code, jump_template->code_size, GUM_PAGE_RW);

  target_address = code + jump_template->offset_of_target;
  if (jump_template->offset_of_target_pointer_points_directly)
    target_actual_address = GPOINTER_TO_SIZE (target_address);
  else
    target_actual_address = GPOINTER_TO_SIZE (&target_address);
  *((gsize *) (code + jump_template->offset_of_target_pointer)) =
      target_actual_address + jump_template->target_immediate_fixup;

  gum_memory_mark_code (code, jump_template->code_size);

  expected_insn_count = INVOKER_INSN_COUNT + jump_template->instruction_count;
#if GLIB_SIZEOF_VOID_P == 4
  expected_insn_count += jump_template->ia32_padding_instruction_count;
#endif

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
  g_assert_cmpint (ret, ==, 1337);
  g_assert_cmpuint (fixture->sink->events->length, ==, expected_insn_count);

  return func;
}

TESTCASE (indirect_jump_with_immediate)
{
  const guint8 code[] = {
    0xeb, 0x08,                         /* jmp +8          */

    0x00, 0x00, 0x00, 0x00,             /* address padding */
    0x00, 0x00, 0x00, 0x00,

    0xff, 0x25, 0xf2, 0xff, 0xff, 0xff, /* jmp             */
    0xcc,                               /* int3            */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337   */
    0xc3,                               /* ret             */
  };
  JumpTemplate jump_template = { 0, };

  jump_template.code_template = code;
  jump_template.code_size = sizeof (code);
  jump_template.offset_of_target_pointer = 12;
  jump_template.offset_of_target = 17;
  jump_template.instruction_count = 4;

#if GLIB_SIZEOF_VOID_P == 8
  jump_template.offset_of_target_pointer -= 10;
  jump_template.offset_of_target_pointer_points_directly = TRUE;
#endif

  invoke_jump (fixture, &jump_template);
}

TESTCASE (indirect_jump_with_immediate_and_scaled_register)
{
  guint8 code[] = {
    0x90, 0xbe, 0x00, 0x00, 0x00, 0x00, /* mov xsi, addr           */
                0x90, 0x90, 0x90, 0x90,
    0x90, 0xb8, 0x03, 0x00, 0x00, 0x00, /* mov xax, 3              */
                0x90, 0x90, 0x90, 0x90,
    0xff, 0x64, 0x86, 0xf9,             /* jmp [xsi + xax * 4 - 7] */
    0xcc,                               /* int3                    */

    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov eax, 1337           */
    0xc3,                               /* ret                     */
  };
  JumpTemplate jump_template = { 0, };

  jump_template.code_template = code;
  jump_template.code_size = sizeof (code);
  jump_template.offset_of_target_pointer = 2;
  jump_template.offset_of_target = 25;
  jump_template.target_immediate_fixup = -5;
  jump_template.instruction_count = 5;
  jump_template.ia32_padding_instruction_count = 10;

#if GLIB_SIZEOF_VOID_P == 8
  code[0] = 0x48;

  code[10] = 0x48;
  memset (code + 10 + 6, 0, 4);

  jump_template.ia32_padding_instruction_count = 5;
#endif

  invoke_jump (fixture, &jump_template);
}

#if GLIB_SIZEOF_VOID_P == 4

typedef void (* ClobberFunc) (GumCpuContext * ctx);

TESTCASE (no_register_clobber)
{
  guint8 * code;
  gsize page_size;
  GumX86Writer cw;
  const gchar * my_func_lbl = "my_func";
  const gchar * my_beach_lbl = "my_beach";
  ClobberFunc func;
  GumCpuContext ctx;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  gum_x86_writer_init (&cw, code);

  gum_x86_writer_put_pushax (&cw);

  gum_x86_writer_put_pushax (&cw);
  gum_x86_writer_put_push_u32 (&cw, (guint32) fixture->sink);
  gum_x86_writer_put_push_u32 (&cw, (guint32) fixture->transformer);
  gum_x86_writer_put_push_u32 (&cw, (guint32) fixture->stalker);
  gum_x86_writer_put_call_address (&cw, GUM_ADDRESS (gum_stalker_follow_me));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_ESP, 3 * sizeof (gpointer));
  gum_x86_writer_put_popax (&cw);

  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_EAX, 0xcafebabe);
  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_ECX, 0xbeefbabe);
  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_EDX, 0xb00bbabe);
  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_EBX, 0xf001babe);
  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_EBP, 0xababe);
  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_ESI, 0x1337);
  gum_x86_writer_put_mov_reg_u32 (&cw, GUM_X86_EDI, 0x1227);

  gum_x86_writer_put_call_near_label (&cw, my_func_lbl);

  gum_x86_writer_put_pushax (&cw);
  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_ESP, 2 * sizeof (gpointer));
  gum_x86_writer_put_push_u32 (&cw, (guint32) fixture->stalker);
  gum_x86_writer_put_call_address (&cw, GUM_ADDRESS (gum_stalker_unfollow_me));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_ESP, 3 * sizeof (gpointer));
  gum_x86_writer_put_popax (&cw);

  gum_x86_writer_put_push_reg (&cw, GUM_X86_ECX);
  gum_x86_writer_put_mov_reg_reg_offset_ptr (&cw, GUM_X86_ECX,
      GUM_X86_ESP, sizeof (gpointer) + (8 * sizeof (gpointer))
      + sizeof (gpointer));
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, eax), GUM_X86_EAX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, edx), GUM_X86_EDX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ebx), GUM_X86_EBX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ebp), GUM_X86_EBP);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, esi), GUM_X86_ESI);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, edi), GUM_X86_EDI);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_EAX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw,
      GUM_X86_ECX, G_STRUCT_OFFSET (GumCpuContext, ecx), GUM_X86_EAX);

  gum_x86_writer_put_popax (&cw);

  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_put_label (&cw, my_func_lbl);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_jmp_short_label (&cw, my_beach_lbl);
  gum_x86_writer_put_breakpoint (&cw);

  gum_x86_writer_put_label (&cw, my_beach_lbl);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_x86_writer_offset (&cw));
  gum_x86_writer_clear (&cw);

  fixture->sink->mask = GUM_CALL | GUM_RET | GUM_EXEC;
  func = GUM_POINTER_TO_FUNCPTR (ClobberFunc, code);
  func (&ctx);

  g_assert_cmphex (ctx.eax, ==, 0xcafebabe);
  g_assert_cmphex (ctx.ecx, ==, 0xbeefbabe);
  g_assert_cmphex (ctx.edx, ==, 0xb00bbabe);
  g_assert_cmphex (ctx.ebx, ==, 0xf001babe);
  g_assert_cmphex (ctx.ebp, ==, 0xababe);
  g_assert_cmphex (ctx.esi, ==, 0x1337);
  g_assert_cmphex (ctx.edi, ==, 0x1227);

  gum_memory_free (code, page_size);
}

#endif

TESTCASE (no_red_zone_clobber)
{
  guint8 code_template[] =
  {
    0x90, 0xb8, 0x00, 0x00, 0x00, 0x00, /* mov xax, <addr>    */
                0x90, 0x90, 0x90, 0x90,
    0x90, 0x89, 0x44, 0x24, 0xf8,       /* mov [rsp - 8], xax */
    0x90, 0x8b, 0x44, 0x24, 0xf8,       /* mov xax, [rsp - 8] */
    0xff, 0xe0,                         /* jmp rax            */
    0xcc,                               /* int3               */
    0xb8, 0x39, 0x05, 0x00, 0x00,       /* mov rax, 1337      */
    0xc3                                /* ret                */
  };
  guint8 * code;
  StalkerTestFunc func;
  gint ret;

#if GLIB_SIZEOF_VOID_P == 8
  code_template[0] = 0x48;
  code_template[10] = 0x48;
  code_template[15] = 0x48;
#endif

  code = test_stalker_fixture_dup_code (fixture, code_template,
      sizeof (code_template));
  gum_mprotect (code, sizeof (code_template), GUM_PAGE_RW);
  *((gpointer *) (code + 2)) = code + 23;
  gum_memory_mark_code (code, sizeof (code_template));

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, code);
  ret = func (42);
  g_assert_cmpint (ret, ==, 1337);

  fixture->sink->mask = GUM_EXEC;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);
#if GLIB_SIZEOF_VOID_P == 8
  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 6);
#else
  g_assert_cmpuint (fixture->sink->events->length, ==, INVOKER_INSN_COUNT + 13);
#endif
  g_assert_cmpint (ret, ==, 1337);
}

TESTCASE (big_block)
{
  const guint nop_instruction_count = 1000000;
  guint8 * code;
  gsize page_size;
  GumX86Writer cw;
  guint i;
  StalkerTestFunc func;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate (NULL,
      ((nop_instruction_count / page_size) + 1) * page_size,
      page_size, GUM_PAGE_RW);
  gum_x86_writer_init (&cw, code);

  for (i = 0; i != nop_instruction_count; i++)
    gum_x86_writer_put_nop (&cw);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_x86_writer_offset (&cw));

  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc,
      test_stalker_fixture_dup_code (fixture, code,
          gum_x86_writer_offset (&cw)));

  gum_x86_writer_clear (&cw);
  gum_memory_free (code,
      ((nop_instruction_count / page_size) + 1) * page_size);

  test_stalker_fixture_follow_and_invoke (fixture, func, -1);
}

#ifdef HAVE_WINDOWS

typedef struct _TestWindow TestWindow;

typedef void (* TestWindowMessageHandler) (TestWindow * window,
    gpointer user_data);

struct _TestWindow
{
  ATOM klass;
  HWND handle;
  GumStalker * stalker;

  TestWindowMessageHandler handler;
  gpointer user_data;
};

static void do_follow (TestWindow * window, gpointer user_data);
static void do_unfollow (TestWindow * window, gpointer user_data);

static TestWindow * create_test_window (GumStalker * stalker);
static void destroy_test_window (TestWindow * window);
static void send_message_and_pump_messages_briefly (TestWindow * window,
    TestWindowMessageHandler handler, gpointer user_data);

static LRESULT CALLBACK test_window_proc (HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam);

#if GLIB_SIZEOF_VOID_P == 4

static StalkerTestFunc
invoke_indirect_call_seg (TestStalkerFixture * fixture,
                          GumEventType mask)
{
  const guint8 code_template[] = {
    0x64, 0xff, 0x35,                   /* push dword [dword fs:0x700] */
        0x00, 0x07, 0x00, 0x00,
    0x64, 0xc7, 0x05,                   /* mov dword [dword fs:0x700], */
        0x00, 0x07, 0x00, 0x00,         /*     <addr>                  */
        0xaa, 0xbb, 0xcc, 0xdd,

    0x64, 0xff, 0x15,                   /* call fs:700h                */
        0x00, 0x07, 0x00, 0x00,

    0x50,                               /* push eax                    */
    0x8b, 0x44, 0x24, 0x04,             /* mov eax, [esp+0x4]          */
    0x64, 0xa3, 0x00, 0x07, 0x00, 0x00, /* mov [fs:0x700],eax          */
    0x58,                               /* pop eax                     */
    0x81, 0xc4, 0x04, 0x00, 0x00, 0x00, /* add esp, 0x4                */

    0xc3,                               /* ret                         */

    0xb8, 0xbe, 0xba, 0xfe, 0xca,       /* mov eax, 0xcafebabe         */
    0xc3,                               /* ret                         */
  };
  guint8 * code;
  StalkerTestFunc func;
  guint ret;

  code = test_stalker_fixture_dup_code (fixture, code_template,
      sizeof (code_template));
  func = GUM_POINTER_TO_FUNCPTR (StalkerTestFunc, code);

  gum_mprotect (code, sizeof (code_template), GUM_PAGE_RW);
  *((gpointer *) (code + 14)) = code + sizeof (code_template) - 1 - 5;
  gum_memory_mark_code (code, sizeof (code_template));

  fixture->sink->mask = mask;
  ret = test_stalker_fixture_follow_and_invoke (fixture, func, 0);

  g_assert_cmphex (ret, ==, 0xcafebabe);

  return func;
}

TESTCASE (win32_indirect_call_seg)
{
  invoke_indirect_call_seg (fixture, GUM_EXEC);

  g_assert_cmpuint (fixture->sink->events->length,
      ==, INVOKER_INSN_COUNT + 11);
}

#endif

TESTCASE (win32_messagebeep_api)
{
  fixture->sink->mask = GUM_EXEC | GUM_CALL | GUM_RET;

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  MessageBeep (MB_ICONINFORMATION);
  gum_stalker_unfollow_me (fixture->stalker);
}

TESTCASE (win32_follow_user_to_kernel_to_callback)
{
  TestWindow * window;

  window = create_test_window (fixture->stalker);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  send_message_and_pump_messages_briefly (window, do_unfollow,
      fixture->stalker);
  g_assert_false (gum_stalker_is_following_me (fixture->stalker));

  destroy_test_window (window);
}

TESTCASE (win32_follow_callback_to_kernel_to_user)
{
  TestWindow * window;

  window = create_test_window (fixture->stalker);

  send_message_and_pump_messages_briefly (window, do_follow, fixture->sink);
  g_assert_true (gum_stalker_is_following_me (fixture->stalker));
  gum_stalker_unfollow_me (fixture->stalker);

  destroy_test_window (window);
}

static void
do_follow (TestWindow * window, gpointer user_data)
{
  gum_stalker_follow_me (window->stalker, NULL, GUM_EVENT_SINK (user_data));
}

static void
do_unfollow (TestWindow * window, gpointer user_data)
{
  gum_stalker_unfollow_me (window->stalker);
}

static TestWindow *
create_test_window (GumStalker * stalker)
{
  TestWindow * window;
  WNDCLASSW wc = { 0, };

  window = g_slice_new (TestWindow);

  window->stalker = stalker;

  wc.lpfnWndProc = test_window_proc;
  wc.hInstance = GetModuleHandle (NULL);
  wc.lpszClassName = L"GumTestWindowClass";
  window->klass = RegisterClassW (&wc);
  g_assert_cmpuint (window->klass, !=, 0);

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4306)
#endif
  window->handle = CreateWindowW ((LPCWSTR) window->klass, L"GumTestWindow",
      WS_CAPTION, 10, 10, 320, 240, HWND_MESSAGE, NULL,
      GetModuleHandleW (NULL), NULL);
#ifdef _MSC_VER
# pragma warning (pop)
#endif
  g_assert_nonnull (window->handle);

  SetWindowLongPtr (window->handle, GWLP_USERDATA, (LONG_PTR) window);
  ShowWindow (window->handle, SW_SHOWNORMAL);

  return window;
}

static void
destroy_test_window (TestWindow * window)
{
  g_assert_true (UnregisterClassW ((LPCWSTR) window->klass,
        GetModuleHandleW (NULL)));

  g_slice_free (TestWindow, window);
}

static void
send_message_and_pump_messages_briefly (TestWindow * window,
                                        TestWindowMessageHandler handler,
                                        gpointer user_data)
{
  MSG msg;

  window->handler = handler;
  window->user_data = user_data;

  SendMessage (window->handle, WM_USER, 0, 0);

  while (GetMessage (&msg, NULL, 0, 0))
  {
    TranslateMessage (&msg);
    DispatchMessage (&msg);
  }
}

static LRESULT CALLBACK
test_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  if (msg == WM_USER)
  {
    TestWindow * window;

    window = (TestWindow *) GetWindowLongPtr (hwnd, GWLP_USERDATA);
    window->handler (window, window->user_data);

    SetTimer (hwnd, 1, USER_TIMER_MINIMUM, NULL);

    return 0;
  }
  else if (msg == WM_TIMER)
  {
    KillTimer (hwnd, 1);
    DestroyWindow (hwnd);
  }
  else if (msg == WM_DESTROY)
  {
    PostQuitMessage (0);
    return 0;
  }

  return DefWindowProc (hwnd, msg, wparam, lparam);
}

#endif

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

TESTCASE (prefetch_backpatch)
{
  gsize pipe_size;
  void * fork_addr;
  GumInterceptor * interceptor;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  bp_ctx.stalker = fixture->stalker;

  g_assert_cmpint (pipe (bp_ctx.pipes), ==, 0);
  g_assert_true (g_unix_set_fd_nonblocking (bp_ctx.pipes[0], TRUE, NULL));
  g_assert_true (g_unix_set_fd_nonblocking (bp_ctx.pipes[1], TRUE, NULL));

  pipe_size = get_max_pipe_size ();

  g_assert_cmpint (fcntl (bp_ctx.pipes[0], F_SETPIPE_SZ, pipe_size), ==,
      pipe_size);
  g_assert_cmpint (fcntl (bp_ctx.pipes[1], F_SETPIPE_SZ, pipe_size), ==,
      pipe_size);

  bp_ctx.observer = g_object_new (GUM_TYPE_TEST_STALKER_OBSERVER, NULL);
  bp_ctx.runner_range = fixture->runner_range;
  bp_ctx.transformer = gum_stalker_transformer_make_from_callback (
      prefetch_backpatch_tranform, NULL, NULL);

  fork_addr = GSIZE_TO_POINTER (gum_module_find_global_export_by_name ("fork"));
  interceptor = gum_interceptor_obtain ();
  gum_interceptor_begin_transaction (interceptor);
  g_assert_cmpint (gum_interceptor_replace (interceptor, fork_addr,
      prefetch_on_fork, NULL, NULL), ==, GUM_REPLACE_OK);
  gum_interceptor_end_transaction (interceptor);

  g_object_unref (interceptor);

  gum_stalker_set_trust_threshold (fixture->stalker, 0);

  gum_stalker_follow_me (bp_ctx.stalker, bp_ctx.transformer, NULL);

  gum_stalker_set_observer (bp_ctx.stalker,
      GUM_STALKER_OBSERVER (bp_ctx.observer));

  /*
   * Our maximum pipe size is likely to be fairly modest (without reconfiguring
   * the system). So we use a relatively simple workload so that we don't
   * saturate it.
   */
  prefetch_backpatch_simple_workload (bp_ctx.runner_range);

  _exit (0);
}

static void
prefetch_backpatch_tranform (GumStalkerIterator * iterator,
                             GumStalkerOutput * output,
                             gpointer user_data)
{
  const cs_insn * instr;

  while (gum_stalker_iterator_next (iterator, &instr))
  {
    if (instr->address == GPOINTER_TO_SIZE (pretend_workload))
    {
      gum_stalker_iterator_put_callout (iterator, entry_callout, NULL, NULL);
    }

    gum_stalker_iterator_keep (iterator);
  }
}

static void
entry_callout (GumCpuContext * cpu_context,
               gpointer user_data)
{
  guint counts[3], i;

  for (i = 0; i != G_N_ELEMENTS (counts); i++)
  {
    pid_t pid;
    int res, status;

    pid = fork ();
    g_assert_cmpint (pid, >=, 0);

    if (pid == 0)
    {
      /* Child */
      bp_ctx.entry_reached = TRUE;
      return;
    }

    /* Parent */
    counts[i] = bp_ctx.count;
    res = waitpid (pid, &status, 0);
    g_assert_cmpint (res, ==, pid);
    g_assert_cmpint (WIFEXITED (status), !=, 0);
    g_assert_cmpint (WEXITSTATUS (status), ==, 0);
  }

  /*
   * When we fork the first child, we shouldn't have any backpatches to
   * prefetch.
   */
  g_assert_cmpuint (counts[0], ==, 0);

  /*
   * Just as we fork the second child, we should prefetch the backpatches from
   * the first time the child ran.
   */
  g_assert_cmpuint (counts[1], >, 0);

  /*
   * Before we fork the third child, we should prefetch the new backpatches from
   * the second run of the child, there should be less since the child should
   * have already inherited the backpatches we applied from the first run.
   */
  g_assert_cmpuint (counts[2], <, counts[1]);

  gum_stalker_unfollow_me (bp_ctx.stalker);

  close (bp_ctx.pipes[STDIN_FILENO]);
  close (bp_ctx.pipes[STDOUT_FILENO]);

  _exit (0);
}

static int
prefetch_on_fork (void)
{
  int n;
  gsize size;
  char buf[PIPE_BUF] = { 0, };

  bp_ctx.count = 0;
  for (n = read (bp_ctx.pipes[STDIN_FILENO], &size, sizeof (size));
       n >= 0;
       n = read (bp_ctx.pipes[STDIN_FILENO], &size, sizeof (size)))
  {
    g_assert_cmpint (read (bp_ctx.pipes[STDIN_FILENO], buf, size), ==, size);
    gum_stalker_prefetch_backpatch (bp_ctx.stalker, (const GumBackpatch *) buf);
    bp_ctx.count++;
  }
  g_assert_cmpint (n, ==, -1);
  g_assert_cmpint (errno, ==, EAGAIN);

  if (g_test_verbose ())
    g_print ("Prefetches (%u)\n", bp_ctx.count);

  return fork ();
}

GUM_NOINLINE static void
prefetch_backpatch_simple_workload (const GumMemoryRange * runner_range)
{
  const guint8 * buf;
  gsize limit, i;
  guint8 val;

  buf = GSIZE_TO_POINTER (runner_range->base_address);
  limit = MIN (runner_range->size, 65536);

  val = 0;
  for (i = 0; i != limit; i++)
  {
    val = val ^ buf[i];
  }

  if (g_test_verbose ())
    g_print ("Result: 0x%02x\n", val);
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
  iface->notify_backpatch = gum_test_stalker_observer_notify_backpatch;
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

static void
gum_test_stalker_observer_notify_backpatch (GumStalkerObserver * self,
                                            const GumBackpatch * backpatch,
                                            gsize size)
{
  int written;

  if (!bp_ctx.entry_reached)
    return;

  written = write (bp_ctx.pipes[STDOUT_FILENO], &size, sizeof (size));
  g_assert_cmpint (written, ==, sizeof (size));

  written = write (bp_ctx.pipes[STDOUT_FILENO], backpatch, size);
  g_assert_cmpint (written, ==, size);
}

static gsize
get_max_pipe_size (void)
{
  guint64 val;
  gchar * contents;

  g_assert_true (g_file_get_contents ("/proc/sys/fs/pipe-max-size", &contents,
      NULL, NULL));

  val = g_ascii_strtoull (contents, NULL, 10);
  g_assert_cmpuint (val, <=, G_MAXINT32);

  g_free (contents);

  return (gsize) val;
}

#endif

#ifndef HAVE_WINDOWS

TESTCASE (ic_var)
{
  GumStalker * stalker;

  stalker = g_object_new (GUM_TYPE_STALKER,
      "ic-entries", 32,
      NULL);

  gum_stalker_follow_me (stalker, NULL, NULL);
  pretend_workload (fixture->runner_range);
  gum_stalker_unfollow_me (stalker);

  while (gum_stalker_garbage_collect (stalker))
    g_usleep (10000);

  g_object_unref (stalker);
}

#endif

#include "../stalker-exceptions.c"

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
