/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "sampler-fixture.c"

TESTLIST_BEGIN (sampler)
  TESTENTRY (cycle)
  TESTENTRY (busy_cycle)
#if defined (HAVE_FRIDA_GLIB) && !defined (HAVE_ASAN)
  TESTENTRY (malloc_count)
#endif
  TESTENTRY (multiple_call_counters)
  TESTENTRY (wall_clock)
  TESTENTRY (user_time)
  TESTENTRY (user_time_by_id_self)
  TESTENTRY (user_time_by_id_other)
TESTLIST_END ()

typedef struct _MallocCountHelperContext MallocCountHelperContext;
typedef struct _TestThreadSyncData TestThreadSyncData;

struct _MallocCountHelperContext
{
  GumSampler * sampler;
  const GumHeapApi * api;
  volatile gboolean allowed_to_start;
  GumSample count;
};

struct _TestThreadSyncData
{
  GMutex mutex;
  GCond cond;
  const gchar * name;
  volatile gboolean started;
  volatile GumThreadId thread_id;
  volatile gboolean * volatile done;
};

static guint spin_for_one_tenth_second (void);
static void nop_function_a (void);
static void nop_function_b (void);

static gboolean check_user_time_testable (GumSampler * sampler);
static GThread * create_sleeping_dummy_thread_sync (const gchar * name,
    volatile gboolean * done, GumThreadId * thread_id);
static gpointer sleeping_dummy (gpointer data);
static void do_work (void);

TESTCASE (cycle)
{
  GumSample sample_a, sample_b;

  fixture->sampler = gum_cycle_sampler_new ();
  if (gum_cycle_sampler_is_available (GUM_CYCLE_SAMPLER (fixture->sampler)))
  {
    sample_a = gum_sampler_sample (fixture->sampler);
    sample_b = gum_sampler_sample (fixture->sampler);
    g_assert_cmpuint (sample_b, >=, sample_a);
  }
  else
  {
    g_test_message ("skipping test because of unsupported OS");
  }
}

TESTCASE (busy_cycle)
{
  GumSample spin_start, spin_diff;
  GumSample sleep_start, sleep_diff;

  fixture->sampler = gum_busy_cycle_sampler_new ();

  if (gum_busy_cycle_sampler_is_available (
      GUM_BUSY_CYCLE_SAMPLER (fixture->sampler)))
  {
    spin_start = gum_sampler_sample (fixture->sampler);
    spin_for_one_tenth_second ();
    spin_diff = gum_sampler_sample (fixture->sampler) - spin_start;

    sleep_start = gum_sampler_sample (fixture->sampler);
    g_usleep (G_USEC_PER_SEC / 10 / 10);
    sleep_diff = gum_sampler_sample (fixture->sampler) - sleep_start;

    g_assert_cmpuint (spin_diff, >, sleep_diff * 10);
  }
  else
  {
    g_test_message ("skipping test because of unsupported OS");
  }
}

#if defined (HAVE_FRIDA_GLIB) && !defined (HAVE_ASAN)

static gpointer malloc_count_helper_thread (gpointer data);

TESTCASE (malloc_count)
{
  const GumHeapApiList * heap_apis;
  const GumHeapApi * api;
  GumSample sample_a, sample_b;
  MallocCountHelperContext helper = { 0, };
  GThread * helper_thread;
  GumInterceptor * interceptor;
  volatile gpointer a, b, c = NULL;

#ifdef HAVE_QNX
  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }
#endif

  if (RUNNING_ON_VALGRIND)
  {
    g_print ("<skipping, not compatible with Valgrind> ");
    return;
  }

  heap_apis = test_util_heap_apis ();
  api = gum_heap_api_list_get_nth (heap_apis, 0);

  fixture->sampler = gum_malloc_count_sampler_new_with_heap_apis (heap_apis);

  helper.sampler = fixture->sampler;
  helper.api = api;
  helper_thread = g_thread_new ("sampler-test-malloc-count",
      malloc_count_helper_thread, &helper);

  interceptor = gum_interceptor_obtain ();

  sample_a = gum_sampler_sample (fixture->sampler);
  a = api->malloc (1);
  helper.allowed_to_start = TRUE;
  gum_interceptor_ignore_current_thread (interceptor);
  g_thread_join (helper_thread);
  gum_interceptor_unignore_current_thread (interceptor);
  b = api->calloc (2, 2);
  c = api->realloc (c, 6);
  api->free (c);
  api->free (b);
  api->free (a);
  sample_b = gum_sampler_sample (fixture->sampler);

  g_object_unref (interceptor);

  g_assert_cmpuint (sample_b, ==, sample_a + 3);
  g_assert_cmpuint (helper.count, ==, 1);
  g_assert_cmpuint (gum_call_count_sampler_peek_total_count (
      GUM_CALL_COUNT_SAMPLER (fixture->sampler)), >=, 3 + 1);
}

static gpointer
malloc_count_helper_thread (gpointer data)
{
  MallocCountHelperContext * helper = (MallocCountHelperContext *) data;
  const GumHeapApi * api = helper->api;
  GumSample sample_a, sample_b;
  volatile gpointer p;

  while (!helper->allowed_to_start)
    g_thread_yield ();

  sample_a = gum_sampler_sample (helper->sampler);
  p = api->malloc (3);
  api->free (p);
  sample_b = gum_sampler_sample (helper->sampler);

  helper->count = sample_b - sample_a;

  return NULL;
}

#endif

TESTCASE (multiple_call_counters)
{
  GumSampler * sampler1, * sampler2;

  sampler1 = gum_call_count_sampler_new (nop_function_a, NULL);
  sampler2 = gum_call_count_sampler_new (nop_function_b, NULL);

  nop_function_a ();

  g_assert_cmpint (gum_sampler_sample (sampler1), ==, 1);
  g_assert_cmpint (gum_sampler_sample (sampler2), ==, 0);

  g_object_unref (sampler2);
  g_object_unref (sampler1);
}

TESTCASE (wall_clock)
{
  GumSample sample_a, sample_b;

  fixture->sampler = gum_wall_clock_sampler_new ();

  sample_a = gum_sampler_sample (fixture->sampler);
  g_usleep (G_USEC_PER_SEC / 30);
  sample_b = gum_sampler_sample (fixture->sampler);

  g_assert_cmpuint (sample_b, >, sample_a);
}

static guint
spin_for_one_tenth_second (void)
{
  guint b = 0;
  GTimer * timer;
  guint i;

  timer = g_timer_new ();

  do
  {
    for (i = 0; i != 1000000; i++)
      b += i * i;
  }
  while (g_timer_elapsed (timer, NULL) < 0.1);

  g_timer_destroy (timer);

  return b;
}

static gint dummy_variable_to_trick_optimizer = 0;

GUM_HOOK_TARGET static void
nop_function_a (void)
{
  dummy_variable_to_trick_optimizer += 3;
}

GUM_HOOK_TARGET static void
nop_function_b (void)
{
  dummy_variable_to_trick_optimizer -= 7;
}

TESTCASE (user_time)
{
  GumSample user_time_a, user_time_b;

  fixture->sampler = gum_user_time_sampler_new ();
  if (!check_user_time_testable (fixture->sampler))
    return;

  do_work ();
  user_time_a = gum_sampler_sample (fixture->sampler);

  do_work ();
  user_time_b = gum_sampler_sample (fixture->sampler);
  g_assert_cmpuint (user_time_a, !=, 0);
  g_assert_cmpuint (user_time_b, >, user_time_a);
}

TESTCASE (user_time_by_id_self)
{
  GumSample user_time_a, user_time_b;

  fixture->sampler = gum_user_time_sampler_new_with_thread_id (
      gum_process_get_current_thread_id ());
  if (!check_user_time_testable (fixture->sampler))
    return;

  do_work ();
  user_time_a = gum_sampler_sample (fixture->sampler);

  do_work ();
  user_time_b = gum_sampler_sample (fixture->sampler);
#if defined (HAVE_LINUX) || defined (HAVE_DARWIN) || defined (HAVE_FREEBSD) \
    || defined (HAVE_WINDOWS)
  g_assert_cmpuint (user_time_a, !=, 0);
  g_assert_cmpuint (user_time_b, >, user_time_a);
#else
  g_assert_cmpuint (user_time_a, ==, 0);
  g_assert_cmpuint (user_time_b, ==, 0);
#endif
}

TESTCASE (user_time_by_id_other)
{
  volatile gboolean done = FALSE;
  GThread * thread;
  GumThreadDetails d = { 0, };
  GumSample user_time_a, user_time_b;

  thread = create_sleeping_dummy_thread_sync ("user_time", &done, &d.id);

  fixture->sampler = gum_user_time_sampler_new_with_thread_id (d.id);
  if (!check_user_time_testable (fixture->sampler))
    return;

  g_usleep (250000);
  user_time_a = gum_sampler_sample (fixture->sampler);

  g_usleep (250000);
  user_time_b = gum_sampler_sample (fixture->sampler);

  g_assert_cmpuint (user_time_a, !=, 0);
  /*
   * The user-time clock can be coarse, so two samples taken close together
   * may land in the same tick. Only require that it does not go backwards.
   */
  g_assert_cmpuint (user_time_b, >=, user_time_a);

  done = TRUE;
  g_thread_join (thread);
}

static gboolean
check_user_time_testable (GumSampler * sampler)
{
  if (!gum_user_time_sampler_is_available (GUM_USER_TIME_SAMPLER (sampler)))
  {
    g_print ("<skipping, unsupported OS> ");
    return FALSE;
  }

  return TRUE;
}

static GThread *
create_sleeping_dummy_thread_sync (const gchar * name,
                                   volatile gboolean * done,
                                   GumThreadId * thread_id)
{
  TestThreadSyncData sync_data;
  GThread * thread;

  g_mutex_init (&sync_data.mutex);
  g_cond_init (&sync_data.cond);
  sync_data.started = FALSE;
  sync_data.thread_id = 0;
  sync_data.name = name;
  sync_data.done = done;

  g_mutex_lock (&sync_data.mutex);

  thread = g_thread_new (name, sleeping_dummy, &sync_data);

  while (!sync_data.started)
    g_cond_wait (&sync_data.cond, &sync_data.mutex);

  if (thread_id != NULL)
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
  volatile gboolean * done = sync_data->done;

  gum_ensure_current_thread_is_named (sync_data->name);

  g_mutex_lock (&sync_data->mutex);
  sync_data->started = TRUE;
  sync_data->thread_id = gum_process_get_current_thread_id ();
  g_cond_signal (&sync_data->cond);
  g_mutex_unlock (&sync_data->mutex);

  do_work ();

  while (!(*done))
    g_thread_yield ();

  return NULL;
}

static void
do_work (void)
{
  GTimer * timer;

  timer = g_timer_new ();

  while (g_timer_elapsed (timer, NULL) < 0.1)
    ;

  g_timer_destroy (timer);
}
