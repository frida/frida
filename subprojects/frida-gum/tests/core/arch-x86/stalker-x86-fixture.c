/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2010-2013 Karl Trygve Kalleberg <karltk@boblycat.org>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "fakeeventsink.h"
#include "gumx86writer.h"
#include "gummemory.h"
#include "stalkerdummychannel.h"
#include "testutil.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WINDOWS
# define VC_EXTRALEAN
# include <windows.h>
#endif
#ifdef HAVE_LINUX
# include <glib-unix.h>
#endif

#define TESTCASE(NAME) \
    void test_stalker_ ## NAME ( \
        TestStalkerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Stalker", test_stalker, NAME, \
        TestStalkerFixture)

#if defined (HAVE_WINDOWS) && GLIB_SIZEOF_VOID_P == 4
# define STALKER_TESTFUNC __fastcall
#else
# define STALKER_TESTFUNC
#endif

#define NTH_EVENT_AS_CALL(N) \
    (gum_fake_event_sink_get_nth_event_as_call (fixture->sink, N))
#define NTH_EVENT_AS_RET(N) \
    (gum_fake_event_sink_get_nth_event_as_ret (fixture->sink, N))
#define NTH_EXEC_EVENT_LOCATION(N) \
    (gum_fake_event_sink_get_nth_event_as_exec (fixture->sink, N)->location)

typedef struct _TestStalkerFixture
{
  GumStalker * stalker;
  GumStalkerTransformer * transformer;
  GumFakeEventSink * sink;
  const GumMemoryRange * runner_range;

  guint8 * code;
  gsize code_size;
  guint8 * last_invoke_calladdr;
  guint8 * last_invoke_retaddr;
} TestStalkerFixture;

typedef gint (STALKER_TESTFUNC * StalkerTestFunc) (gint arg);
typedef guint (* FlatFunc) (void);
typedef gboolean (* TestIsFinishedFunc) (void);
typedef gint (* GetMagicNumberFunc) (void);

static void silence_warnings (void);

static void
test_stalker_fixture_setup (TestStalkerFixture * fixture,
                            gconstpointer data)
{
  fixture->stalker = gum_stalker_new ();
  fixture->transformer = NULL;
  fixture->sink = GUM_FAKE_EVENT_SINK (gum_fake_event_sink_new ());
  fixture->runner_range = gum_module_get_range (gum_process_get_main_module ());

#ifdef HAVE_WINDOWS
  if (IsDebuggerPresent ())
  {
    static gboolean shown_once = FALSE;

    if (!shown_once)
    {
      g_print ("\n\nWARNING:\tRunning Stalker tests with debugger attached "
          "is not supported.\n\t\tSome tests will fail.\n\n");
      shown_once = TRUE;
    }
  }
#endif

  silence_warnings ();
}

static void
test_stalker_fixture_teardown (TestStalkerFixture * fixture,
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

static guint8 *
test_stalker_fixture_dup_code (TestStalkerFixture * fixture,
                               const guint8 * tpl_code,
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

  return fixture->code;
}

#if GLIB_SIZEOF_VOID_P == 4
# define INVOKER_INSN_COUNT 11
# define INVOKER_IMPL_OFFSET 5
#elif GLIB_SIZEOF_VOID_P == 8
# if GUM_NATIVE_ABI_IS_WINDOWS
#  define INVOKER_INSN_COUNT 12
#  define INVOKER_IMPL_OFFSET 5
# else
#  define INVOKER_INSN_COUNT 10
#  define INVOKER_IMPL_OFFSET 4
# endif
#endif

/* custom invoke code as we want to stalk a deterministic code sequence */
static gint
test_stalker_fixture_follow_and_invoke (TestStalkerFixture * fixture,
                                        StalkerTestFunc func,
                                        gint arg)
{
  GumAddressSpec spec;
  gint ret;
  guint8 * code;
  gsize page_size;
  GumX86Writer cw;
#if GLIB_SIZEOF_VOID_P == 4
  guint align_correction_follow = 0;
  guint align_correction_call = 12;
  guint align_correction_unfollow = 8;
#else
  guint align_correction_follow = 8;
  guint align_correction_call = 0;
  guint align_correction_unfollow = 8;
#endif
  GCallback invoke_func;

  spec.near_address = gum_stalker_follow_me;
  spec.max_distance = G_MAXINT32 / 2;

  page_size = gum_query_page_size ();
  code = gum_memory_allocate_near (&spec, page_size, page_size, GUM_PAGE_RW);

  gum_x86_writer_init (&cw, code);

  gum_x86_writer_put_pushax (&cw);

  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_follow);
  gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_stalker_follow_me), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->transformer),
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->sink));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_follow);

  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_call);
  gum_x86_writer_put_mov_reg_address (&cw, GUM_X86_XCX, GUM_ADDRESS (arg));
  fixture->last_invoke_calladdr = (guint8 *) gum_x86_writer_cur (&cw);
  gum_x86_writer_put_call_address (&cw, GUM_ADDRESS (func));
  fixture->last_invoke_retaddr = (guint8 *) gum_x86_writer_cur (&cw);
  gum_x86_writer_put_mov_reg_address (&cw, GUM_X86_XCX, GUM_ADDRESS (&ret));
  gum_x86_writer_put_mov_reg_ptr_reg (&cw, GUM_X86_XCX, GUM_X86_EAX);
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_call);

  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, align_correction_unfollow);
  gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_stalker_unfollow_me), 1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (fixture->stalker));
  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, align_correction_unfollow);

  gum_x86_writer_put_popax (&cw);

  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  gum_memory_mark_code (cw.base, gum_x86_writer_offset (&cw));
  gum_x86_writer_clear (&cw);

  invoke_func = GUM_POINTER_TO_FUNCPTR (GCallback, code);
  invoke_func ();

  gum_memory_free (code, page_size);

  return ret;
}

static void
silence_warnings (void)
{
  (void) test_stalker_fixture_dup_code;
  (void) test_stalker_fixture_follow_and_invoke;
}

typedef struct _PatchCodeContext PatchCodeContext;
typedef struct _UnfollowTransformContext UnfollowTransformContext;
typedef struct _InvalidationTransformContext InvalidationTransformContext;
typedef struct _InvalidationTarget InvalidationTarget;

struct _PatchCodeContext
{
  gconstpointer code;
  gsize size;
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
