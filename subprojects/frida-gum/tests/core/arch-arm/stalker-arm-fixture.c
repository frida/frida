/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2017 Antonio Ken Iannillo <ak.iannillo@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "fakeeventsink.h"
#include "gumarmwriter.h"
#include "gummemory.h"
#include "stalkerdummychannel.h"
#include "testutil.h"

#include <lzma.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LINUX
# include <glib-unix.h>
# include <sys/prctl.h>
#endif

#define TESTCASE(NAME) \
    void test_arm_stalker_ ## NAME (                                      \
    TestArmStalkerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Stalker", test_arm_stalker, NAME,       \
    TestArmStalkerFixture)

#define NTH_EVENT_AS_CALL(N) \
    (gum_fake_event_sink_get_nth_event_as_call (fixture->sink, N))
#define NTH_EVENT_AS_RET(N) \
    (gum_fake_event_sink_get_nth_event_as_ret (fixture->sink, N))
#define NTH_EXEC_EVENT_LOCATION(N) \
    (gum_fake_event_sink_get_nth_event_as_exec (fixture->sink, N)->location)

#define TESTCODE(NAME, ...) static const __attribute__((__aligned__((4)))) \
    guint8 NAME[] = { __VA_ARGS__ }
#define CODE_START(NAME) ((gconstpointer) NAME)
#define CODE_SIZE(NAME) sizeof (NAME)

#define DUP_TESTCODE(NAME) \
    test_arm_stalker_fixture_dup_code (fixture, \
        CODE_START (NAME), CODE_SIZE (NAME))
#define FOLLOW_AND_INVOKE(FUNC) \
    test_arm_stalker_fixture_follow_and_invoke (fixture, FUNC)
#define INVOKE_ARM_EXPECTING(EVENTS, NAME, RETVAL) \
    invoke_arm_expecting_return_value (fixture, EVENTS, CODE_START (NAME), \
        CODE_SIZE (NAME), RETVAL)
#define INVOKE_THUMB_EXPECTING(EVENTS, NAME, RETVAL) \
    invoke_thumb_expecting_return_value (fixture, EVENTS, CODE_START (NAME), \
        CODE_SIZE (NAME), RETVAL)

#define GUM_EVENT_TYPE_exec GumExecEvent
#define GUM_EVENT_TYPE_NAME_exec GUM_EXEC

#define GUM_EVENT_TYPE_call GumCallEvent
#define GUM_EVENT_TYPE_NAME_call GUM_CALL

#define GUM_EVENT_TYPE_block GumBlockEvent
#define GUM_EVENT_TYPE_NAME_block GUM_BLOCK

#define GUM_EVENT_TYPE_ret GumRetEvent
#define GUM_EVENT_TYPE_NAME_ret GUM_RET

#define GUM_ASSERT_EVENT_ADDR(TYPE, INDEX, FIELD, VALUE)                  \
    {                                                                     \
      GUM_EVENT_TYPE_ ## TYPE * ev;                                       \
                                                                          \
      g_assert_cmpuint (fixture->sink->events->length, >, INDEX);            \
      g_assert_cmpint (g_array_index (fixture->sink->events,              \
          GumEvent, INDEX).type, ==, GUM_EVENT_TYPE_NAME_ ## TYPE);       \
                                                                          \
      ev = &g_array_index (fixture->sink->events, GumEvent, INDEX).TYPE;  \
      GUM_ASSERT_CMPADDR (ev->FIELD, ==, VALUE);                          \
    }

/*
 * Total number of instructions in the invoker built by follow_and_invoke().
 * This is counted from the first instruction after the call to follow_me()
 * up to and including the call to unfollow_me().
 */
#define INVOKER_INSN_COUNT 7

/*
 * Total number of call instructions in the invoker built by
 * follow_and_invoke().
 */
#define INVOKER_CALL_INSN_COUNT 2

/*
 * Total number of blocks in the invoker built by follow_and_invoke().
 */
#define INVOKER_BLOCK_COUNT 3

/*
 * Index of block invoked by follow_and_invoke().
 */
#define INVOKEE_BLOCK_INDEX 1

/*
 * Offset of the first instruction within the invoker which should be stalked in
 * bytes.
 */
#define INVOKER_IMPL_OFFSET 24

#define MANY_CALLOUTS_INSN_COUNT 1500

typedef struct _TestArmStalkerFixture TestArmStalkerFixture;
typedef struct _UnfollowTransformContext UnfollowTransformContext;
typedef struct _InvalidationTransformContext InvalidationTransformContext;
typedef struct _InvalidationTarget InvalidationTarget;

struct _TestArmStalkerFixture
{
  GumStalker * stalker;
  GumStalkerTransformer * transformer;
  GumFakeEventSink * sink;
  const GumMemoryRange * runner_range;

  guint8 * code;
  gsize code_size;
  guint8 * invoker;
  gsize invoker_size;
};

struct _UnfollowTransformContext
{
  GumStalker * stalker;
  guint num_blocks_transformed;
  guint target_block;
  gint max_instructions;
};

struct _InvalidationTransformContext
{
  GumStalker * stalker;
  gconstpointer target_function;
  guint n;
};

struct _InvalidationTarget
{
  GumStalkerTransformer * transformer;
  InvalidationTransformContext ctx;

  GThread * thread;
  GumThreadId thread_id;
  StalkerDummyChannel channel;
  volatile gboolean finished;
};

static void test_arm_stalker_fixture_setup (TestArmStalkerFixture * fixture,
    gconstpointer data);
static void test_arm_stalker_fixture_teardown (TestArmStalkerFixture * fixture,
    gconstpointer data);
static GumAddress test_arm_stalker_fixture_dup_code (
    TestArmStalkerFixture * fixture, const guint32 * tpl_code, guint tpl_size);
static GumAddress invoke_arm_expecting_return_value (
    TestArmStalkerFixture * fixture, GumEventType mask, const guint32 * code,
    guint32 len, guint32 expected_return_value);
static GumAddress invoke_thumb_expecting_return_value (
    TestArmStalkerFixture * fixture, GumEventType mask, const guint32 * code,
    guint32 len, guint32 expected_return_value);
static guint32 test_arm_stalker_fixture_follow_and_invoke (
    TestArmStalkerFixture * fixture, GumAddress addr);

static void
test_arm_stalker_fixture_setup (TestArmStalkerFixture * fixture,
                                gconstpointer data)
{
  fixture->stalker = gum_stalker_new ();
  fixture->transformer = NULL;
  fixture->sink = GUM_FAKE_EVENT_SINK (gum_fake_event_sink_new ());
  fixture->runner_range = gum_module_get_range (gum_process_get_main_module ());
}

static void
test_arm_stalker_fixture_teardown (TestArmStalkerFixture * fixture,
                                   gconstpointer data)
{
  while (gum_stalker_garbage_collect (fixture->stalker))
    g_usleep (10000);

  g_object_unref (fixture->sink);
  g_clear_object (&fixture->transformer);
  g_object_unref (fixture->stalker);

  if (fixture->code != NULL)
    gum_memory_free (fixture->code, fixture->code_size);
}

static GumAddress
test_arm_stalker_fixture_dup_code (TestArmStalkerFixture * fixture,
                                   const guint32 * tpl_code,
                                   guint tpl_size)
{
  GumAddressSpec spec;
  gsize page_size;

  spec.near_address = gum_stalker_follow_me;
  spec.max_distance = G_MAXINT32 / 2;

  if (fixture->code != NULL)
    gum_memory_free (fixture->code, fixture->code_size);

  page_size = gum_query_page_size ();
  fixture->code_size = ((tpl_size / page_size) + 1) * page_size;
  fixture->code = gum_memory_allocate_near (&spec, fixture->code_size,
      page_size, GUM_PAGE_RW);
  memcpy (fixture->code, tpl_code, tpl_size);
  gum_memory_mark_code (fixture->code, tpl_size);

  return GUM_ADDRESS (fixture->code);
}

static GumAddress
invoke_arm_expecting_return_value (TestArmStalkerFixture * fixture,
                                   GumEventType mask,
                                   const guint32 * code,
                                   guint32 len,
                                   guint32 expected_return_value)
{
  GumAddress func;

  func = test_arm_stalker_fixture_dup_code (fixture, code, len);

  fixture->sink->mask = mask;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func), ==, expected_return_value);

  return func;
}

static GumAddress
invoke_thumb_expecting_return_value (TestArmStalkerFixture * fixture,
                                     GumEventType mask,
                                     const guint32 * code,
                                     guint32 len,
                                     guint32 expected_return_value)
{
  GumAddress func;

  func = test_arm_stalker_fixture_dup_code (fixture, code, len);

  fixture->sink->mask = mask;
  g_assert_cmpuint (FOLLOW_AND_INVOKE (func + 1), ==, expected_return_value);

  return func;
}

static guint32
test_arm_stalker_fixture_follow_and_invoke (TestArmStalkerFixture * fixture,
                                            GumAddress addr)
{
  guint32 retval;
  GumAddressSpec spec;
  gsize page_size;
  GumArmWriter cw;
  GCallback stalked_func;

  spec.near_address = gum_stalker_follow_me;
  spec.max_distance = G_MAXINT32 / 2;

  page_size = gum_query_page_size ();
  fixture->invoker_size = page_size;
  fixture->invoker = gum_memory_allocate_near (&spec, fixture->invoker_size,
      page_size, GUM_PAGE_RW);

  gum_arm_writer_init (&cw, fixture->invoker);

  /*
   * The ABI dictates that the stack here is 8 byte aligned. We need to store
   * LR, so that we can return to our caller, but we additionally push R0 as we
   * need to push an even number of registers to maintain alignment. We
   * otherwise would not need to store R0 since it is a caller rather than
   * callee saved register.
   */
  gum_arm_writer_put_push_regs (&cw, 2, ARM_REG_R0, ARM_REG_LR);

  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R3,
      GUM_ADDRESS (gum_stalker_follow_me));
  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R0,
      GUM_ADDRESS (fixture->stalker));
  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R1,
      GUM_ADDRESS (fixture->transformer));
  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R2,
      GUM_ADDRESS (fixture->sink));
  gum_arm_writer_put_blx_reg (&cw, ARM_REG_R3);

  gum_arm_writer_put_ldr_reg_u32 (&cw, ARM_REG_R0, GUM_ADDRESS (addr));
  gum_arm_writer_put_blx_reg (&cw, ARM_REG_R0);

  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R1, GUM_ADDRESS (&retval));
  gum_arm_writer_put_str_reg_reg_offset (&cw, ARM_REG_R0, ARM_REG_R1, 0);

  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R1,
      GUM_ADDRESS (gum_stalker_unfollow_me));
  gum_arm_writer_put_ldr_reg_address (&cw, ARM_REG_R0,
      GUM_ADDRESS (fixture->stalker));
  gum_arm_writer_put_blx_reg (&cw, ARM_REG_R1);

  gum_arm_writer_put_pop_regs (&cw, 2, ARM_REG_R0, ARM_REG_LR);
  gum_arm_writer_put_ret (&cw);

  gum_arm_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_arm_writer_offset (&cw));
  gum_arm_writer_clear (&cw);

  stalked_func = GUM_POINTER_TO_FUNCPTR (GCallback, fixture->invoker);
  stalked_func ();

  gum_memory_free (fixture->invoker, fixture->invoker_size);

  return retval;
}
