/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2017 Antonio Ken Iannillo <ak.iannillo@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "fakeeventsink.h"
#include "gumarm64writer.h"
#include "gummemory.h"
#include "stalkerdummychannel.h"
#include "testutil.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LINUX
# include <glib-unix.h>
#endif

#define TESTCASE(NAME) \
    void test_arm64_stalker_ ## NAME ( \
    TestArm64StalkerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Stalker", test_arm64_stalker, NAME, \
    TestArm64StalkerFixture)

#define NTH_EVENT_AS_CALL(N) \
    (gum_fake_event_sink_get_nth_event_as_call (fixture->sink, N))
#define NTH_EVENT_AS_RET(N) \
    (gum_fake_event_sink_get_nth_event_as_ret (fixture->sink, N))
#define NTH_EXEC_EVENT_LOCATION(N) \
    (gum_fake_event_sink_get_nth_event_as_exec (fixture->sink, N)->location)

#define MANY_CALLOUTS_INSN_COUNT 1500

typedef struct _TestArm64StalkerFixture
{
  GumStalker * stalker;
  GumStalkerTransformer * transformer;
  GumFakeEventSink * sink;
  const GumMemoryRange * runner_range;

  guint8 * code;
  gsize code_size;
  guint8 * last_invoke_calladdr;
  guint8 * last_invoke_retaddr;
} TestArm64StalkerFixture;

typedef gint (* StalkerTestFunc) (gint arg);
typedef guint (* FlatFunc) (void);

static void silence_warnings (void);

static void
debug_hello (gpointer pointer)
{
  g_print ("* pointer: %p *\n", pointer);
}

static void
put_debug_print_pointer (GumArm64Writer * cw,
                         gpointer pointer)
{
  gum_arm64_writer_put_push_all_x_registers (cw);
  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (debug_hello), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (pointer));
  gum_arm64_writer_put_pop_all_x_registers (cw);
}

static void
put_debug_print_reg (GumArm64Writer * cw,
                     arm64_reg reg)
{
  gum_arm64_writer_put_push_all_x_registers (cw);
  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (debug_hello), 1,
      GUM_ARG_REGISTER, reg);
  gum_arm64_writer_put_pop_all_x_registers (cw);
}

static void
test_arm64_stalker_fixture_setup (TestArm64StalkerFixture * fixture,
                                  gconstpointer data)
{
  fixture->stalker = gum_stalker_new ();
  fixture->transformer = NULL;
  fixture->sink = GUM_FAKE_EVENT_SINK (gum_fake_event_sink_new ());
  fixture->runner_range = gum_module_get_range (gum_process_get_main_module ());

  silence_warnings ();
}

static void
test_arm64_stalker_fixture_teardown (TestArm64StalkerFixture * fixture,
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

static GCallback
test_arm64_stalker_fixture_dup_code (TestArm64StalkerFixture * fixture,
                                     const guint32 * tpl_code,
                                     guint tpl_size)
{
  GumAddressSpec spec;
  gsize page_size;

  spec.near_address = gum_strip_code_pointer (gum_stalker_follow_me);
  spec.max_distance = G_MAXINT32 / 2;

  if (fixture->code != NULL)
    gum_memory_free (fixture->code, fixture->code_size);
  page_size = gum_query_page_size ();
  fixture->code_size = ((tpl_size / page_size) + 1) * page_size;
  fixture->code = gum_memory_allocate_near (&spec, fixture->code_size,
      page_size, GUM_PAGE_RW);
  memcpy (fixture->code, tpl_code, tpl_size);
  gum_memory_mark_code (fixture->code, tpl_size);

  return GUM_POINTER_TO_FUNCPTR (GCallback,
      gum_sign_code_pointer (fixture->code));
}

#define INVOKER_INSN_COUNT 6
#define INVOKER_IMPL_OFFSET 2

/* custom invoke code as we want to stalk a deterministic code sequence */
static gint
test_arm64_stalker_fixture_follow_and_invoke (TestArm64StalkerFixture * fixture,
                                              StalkerTestFunc func,
                                              gint arg)
{
  GumAddressSpec spec;
  gsize page_size;
  guint8 * code;
  GumArm64Writer cw;
  gint ret;
  GCallback invoke_func;

  spec.near_address = gum_strip_code_pointer (gum_stalker_follow_me);
  spec.max_distance = G_MAXINT32 / 2;
  page_size = gum_query_page_size ();
  code = gum_memory_allocate_near (&spec, page_size, page_size, GUM_PAGE_RW);

  gum_arm64_writer_init (&cw, code);

  gum_arm64_writer_put_push_reg_reg (&cw, ARM64_REG_X29, ARM64_REG_X30);
  gum_arm64_writer_put_mov_reg_reg (&cw, ARM64_REG_X29, ARM64_REG_SP);

  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));

  /* call function -int func(int x)- and save address before and after call */
  gum_arm64_writer_put_ldr_reg_address (&cw, ARM64_REG_X0, GUM_ADDRESS (arg));
  fixture->last_invoke_calladdr = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_call_address_with_arguments (&cw, GUM_ADDRESS (func), 0);
  fixture->last_invoke_retaddr = gum_arm64_writer_cur (&cw);
  gum_arm64_writer_put_ldr_reg_address (&cw, ARM64_REG_X1, GUM_ADDRESS (&ret));
  gum_arm64_writer_put_str_reg_reg_offset (&cw, ARM64_REG_W0, ARM64_REG_X1, 0);

  gum_arm64_writer_put_call_address_with_arguments (&cw,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));

  gum_arm64_writer_put_pop_reg_reg (&cw, ARM64_REG_X29, ARM64_REG_X30);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_arm64_writer_offset (&cw));
  gum_arm64_writer_clear (&cw);

  invoke_func =
      GUM_POINTER_TO_FUNCPTR (GCallback, gum_sign_code_pointer (code));
  invoke_func ();

  gum_memory_free (code, page_size);

  return ret;
}

static void
silence_warnings (void)
{
  (void) put_debug_print_pointer;
  (void) put_debug_print_reg;
  (void) test_arm64_stalker_fixture_dup_code;
  (void) test_arm64_stalker_fixture_follow_and_invoke;
}

typedef struct _UnfollowTransformContext UnfollowTransformContext;
typedef struct _InvalidationTransformContext InvalidationTransformContext;
typedef struct _InvalidationTarget InvalidationTarget;

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
