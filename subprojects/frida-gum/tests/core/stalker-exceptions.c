/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/*
 * Shared Stalker exception tests, #included by the x86 and arm64 backends;
 * not built on its own. Helpers live in stalker-x86-exceptions.cpp.
 */

#if defined (HAVE_LINUX) && !defined (HAVE_ANDROID)

static void callback_at_end (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
static void callout_at_end (GumCpuContext * cpu_context, gpointer user_data);
static void test_check_followed (void);

extern void __cxa_throw (void * thrown_exception, void * type,
    void (* destructor) (void *));

void test_check_bit (guint32 * val, guint8 bit);
void test_try_and_catch (guint32 * val);
void test_try_and_dont_catch (guint32 * val);

TESTCASE (no_exceptions)
{
  guint32 val = 0;

  fixture->sink->mask = GUM_EXEC;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      callback_at_end, &val, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  test_check_followed ();
  gum_stalker_unfollow_me (fixture->stalker);

  g_assert_cmpuint (fixture->sink->events->length, >, 0);

  if (g_test_verbose ())
    g_print ("val: 0x%08x\n", val);

  test_check_bit (&val, 31);
  g_assert_cmpuint (val, ==, 0);
}

TESTCASE (try_and_catch)
{
  guint32 val = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      callback_at_end, &val, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  test_try_and_catch (&val);
  test_check_followed ();
  gum_stalker_unfollow_me (fixture->stalker);

  if (g_test_verbose ())
    g_print ("val: 0x%08x\n", val);

  test_check_bit (&val, 0);
  test_check_bit (&val, 2);
  test_check_bit (&val, 3);
  test_check_bit (&val, 31);
  g_assert_cmpuint (val, ==, 0);
}

TESTCASE (try_and_catch_excluded)
{
  guint32 val = 0;
  const GumMemoryRange range = {
    .base_address = GPOINTER_TO_SIZE (__cxa_throw),
    .size = 1
  };

  gum_stalker_exclude (fixture->stalker, &range);

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      callback_at_end, &val, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  test_try_and_catch (&val);
  test_check_followed ();
  gum_stalker_unfollow_me (fixture->stalker);

  if (g_test_verbose ())
    g_print ("val: 0x%08x\n", val);

  test_check_bit (&val, 0);
  test_check_bit (&val, 2);
  test_check_bit (&val, 3);
  test_check_bit (&val, 31);
  g_assert_cmpuint (val, ==, 0);
}

TESTCASE (try_and_dont_catch)
{
  guint32 val = 0;

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      callback_at_end, &val, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  test_try_and_dont_catch (&val);
  test_check_followed ();
  gum_stalker_unfollow_me (fixture->stalker);

  if (g_test_verbose ())
    g_print ("val: 0x%08x\n", val);

  test_check_bit (&val, 0);
  test_check_bit (&val, 1);
  test_check_bit (&val, 2);
  test_check_bit (&val, 5);
  test_check_bit (&val, 6);
  test_check_bit (&val, 7);
  test_check_bit (&val, 11);
  test_check_bit (&val, 31);
  g_assert_cmpuint (val, ==, 0);
}

TESTCASE (try_and_dont_catch_excluded)
{
  guint32 val = 0;
  const GumMemoryRange range = {
    .base_address = GPOINTER_TO_SIZE (__cxa_throw),
    .size = 1
  };

  gum_stalker_exclude (fixture->stalker, &range);

  fixture->transformer = gum_stalker_transformer_make_from_callback (
      callback_at_end, &val, NULL);

  gum_stalker_follow_me (fixture->stalker, fixture->transformer,
      GUM_EVENT_SINK (fixture->sink));
  test_try_and_dont_catch (&val);
  test_check_followed ();
  gum_stalker_unfollow_me (fixture->stalker);

  if (g_test_verbose ())
    g_print ("val: 0x%08x\n", val);

  test_check_bit (&val, 0);
  test_check_bit (&val, 1);
  test_check_bit (&val, 2);
  test_check_bit (&val, 5);
  test_check_bit (&val, 6);
  test_check_bit (&val, 7);
  test_check_bit (&val, 11);
  test_check_bit (&val, 31);
  g_assert_cmpuint (val, ==, 0);
}

static void
callback_at_end (GumStalkerIterator * iterator,
                 GumStalkerOutput * output,
                 gpointer user_data)
{
  guint32 * val = user_data;
  const cs_insn * insn;

  while (gum_stalker_iterator_next (iterator, &insn))
  {
    gum_stalker_iterator_keep (iterator);

    if (insn->address == GPOINTER_TO_SIZE (test_check_followed))
    {
      gum_stalker_iterator_put_callout (iterator, callout_at_end, val, NULL);
    }
  }
}

static void
callout_at_end (GumCpuContext * cpu_context,
                gpointer user_data)
{
  guint32 * val = user_data;
  *val += 1U << 31;
}

GUM_NOINLINE static void
test_check_followed (void)
{
  /* Avoid calls being optimized out */
  asm ("nop;");
}

#endif
