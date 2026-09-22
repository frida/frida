/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprofiler.h"

#include "fakesampler.h"
#include "lowlevelhelpers.h"
#include "testutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TESTCASE(NAME) \
    void test_profiler_ ## NAME ( \
        TestProfilerFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Prof/Profiler", test_profiler, NAME, \
        TestProfilerFixture)

#define REPORT_TESTCASE(NAME) \
    void test_profile_report_ ## NAME ( \
        TestProfileReportFixture * fixture, gconstpointer data)
#define REPORT_TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Prof/ProfileReport", test_profile_report, NAME, \
        TestProfileReportFixture)

typedef struct _TestProfilerFixture
{
  GumProfiler * profiler;

  GumSampler * sampler;
  GumFakeSampler * fake_sampler;
} TestProfilerFixture;

typedef struct _TestProfileReportFixture
{
  GumProfiler * profiler;

  GumSampler * sampler;
  GumFakeSampler * fake_sampler;

  GumProfileReport * report;
  const GPtrArray * root_nodes;
} TestProfileReportFixture;

static void
test_profiler_fixture_setup (TestProfilerFixture * fixture,
                             gconstpointer data)
{
  fixture->profiler = gum_profiler_new ();
  fixture->sampler = gum_fake_sampler_new ();
  fixture->fake_sampler = GUM_FAKE_SAMPLER (fixture->sampler);
}

static void
test_profiler_fixture_teardown (TestProfilerFixture * fixture,
                                gconstpointer data)
{
  g_object_unref (fixture->sampler);
  g_object_unref (fixture->profiler);
}

static void
test_profile_report_fixture_setup (TestProfileReportFixture * fixture,
                                   gconstpointer data)
{
  fixture->profiler = gum_profiler_new ();
  fixture->sampler = gum_fake_sampler_new ();
  fixture->fake_sampler = GUM_FAKE_SAMPLER (fixture->sampler);
  fixture->report = NULL;
  fixture->root_nodes = NULL;
}

static void
test_profile_report_fixture_teardown (TestProfileReportFixture * fixture,
                                      gconstpointer data)
{
  g_object_unref (fixture->report);
  g_object_unref (fixture->sampler);
  g_object_unref (fixture->profiler);
}

static const GPtrArray *
test_profile_report_fixture_get_root_nodes (TestProfileReportFixture * fixture)
{
  fixture->report = gum_profiler_generate_report (fixture->profiler);
  g_assert_nonnull (fixture->report);

  fixture->root_nodes =
      gum_profile_report_get_root_nodes_for_thread (fixture->report, 0);
  g_assert_nonnull (fixture->root_nodes);

  return fixture->root_nodes;
}

static void
assert_n_top_nodes (TestProfileReportFixture * fixture,
                    guint n,
                    ...)
{
  const GPtrArray * root_nodes;
  va_list args;
  guint i;

  root_nodes = test_profile_report_fixture_get_root_nodes (fixture);
  g_assert_cmpuint (root_nodes->len, ==, n);

  va_start (args, n);

  for (i = 0; i < n; i++)
  {
    const gchar * name, * child_name;
    GumProfileReportNode * node;

    name = va_arg (args, const gchar *);
    child_name = va_arg (args, const gchar *);

    node = (GumProfileReportNode *) g_ptr_array_index (root_nodes, i);
    g_assert_cmpstr (node->name, ==, name);
    g_assert_nonnull (node->child);
    g_assert_cmpstr (node->child->name, ==, child_name);
  }
}

void
assert_depth_from_root_node (TestProfileReportFixture * fixture,
                             guint root_node_index,
                             ...)
{
  GumProfileReportNode * root_node, * cur_node;
  va_list args;

  root_node = (GumProfileReportNode *) g_ptr_array_index (fixture->root_nodes,
      root_node_index);
  cur_node = root_node;

  va_start (args, root_node_index);

  while (TRUE)
  {
    const gchar * expected_node_name;

    expected_node_name = va_arg (args, const gchar *);
    if (expected_node_name == NULL)
    {
      g_assert_null (cur_node);
      break;
    }

    g_assert_cmpstr (cur_node->name, ==, expected_node_name);

    cur_node = cur_node->child;
  }
}

void
assert_same_xml (TestProfileReportFixture * fixture,
                 const gchar * expected_xml)
{
  gchar * generated_xml;

  fixture->report = gum_profiler_generate_report (fixture->profiler);
  g_assert_nonnull (fixture->report);

  generated_xml = gum_profile_report_emit_xml (fixture->report);
  if (strcmp (generated_xml, expected_xml) != 0)
  {
    GString * message;
    gchar * diff;

    message = g_string_new ("Generated XML not like expected:\n\n");

    diff = test_util_diff_xml (expected_xml, generated_xml);
    g_string_append (message, diff);
    g_free (diff);

    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,
        message->str);

    g_string_free (message, TRUE);
  }

  g_free (generated_xml);
}

/*
 * Guinea pig functions:
 */

static void example_b (GumFakeSampler * sampler);
static void example_c (GumFakeSampler * sampler);
static void example_f (GumFakeSampler * sampler);
static void example_g (GumFakeSampler * sampler);
static void example_cyclic_b (GumFakeSampler * sampler, gint flag);
static void deep_recursive_caller (gint count);
static void example_b_dynamic (GumFakeSampler * sampler, guint cost);

gint dummy_variable_to_trick_optimizer = 0;

GUM_HOOK_TARGET static void
sleepy_function (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 1000);
}

GUM_HOOK_TARGET static void
example_a (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 2);
  example_c (sampler);
  example_b (sampler);
}

GUM_HOOK_TARGET static void
example_b (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 3);
}

GUM_HOOK_TARGET static void
example_c (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 4);
}

GUM_HOOK_TARGET static void
example_d (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 7);
  example_c (sampler);
}

GUM_HOOK_TARGET static void
example_e (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 3);
  example_f (sampler);
}

GUM_HOOK_TARGET static void
example_f (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 4);
  example_g (sampler);
}

GUM_HOOK_TARGET static void
example_g (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 5);
}

GUM_HOOK_TARGET static void
example_cyclic_a (GumFakeSampler * sampler,
                  gint flag)
{
  gum_fake_sampler_advance (sampler, 1);

  if (flag)
    example_cyclic_b (sampler, 0);
}

GUM_HOOK_TARGET static void
example_cyclic_b (GumFakeSampler * sampler,
                  gint flag)
{
  gum_fake_sampler_advance (sampler, 2);
  example_cyclic_a (sampler, flag);
}

static gboolean
exclude_simple_stdcall_50 (const gchar * match,
                           gpointer user_data)
{
  return strcmp (match, "simple_stdcall_50") != 0;
}

GUM_HOOK_TARGET static void GUM_CDECL
simple_cdecl_42 (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 42);
}

GUM_HOOK_TARGET static void GUM_STDCALL
simple_stdcall_48 (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 48);
}

GUM_HOOK_TARGET static void GUM_STDCALL
simple_stdcall_50 (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 50);
}

static void
recursive_function (gint count)
{
  if (--count > 0)
  {
    recursive_function (count);
    dummy_variable_to_trick_optimizer += 3;
  }
  else
  {
    dummy_variable_to_trick_optimizer += 5;
  }
}

static void
deep_recursive_function (gint count)
{
  dummy_variable_to_trick_optimizer += 42;

  if (--count > 0)
  {
    deep_recursive_function (count);
    deep_recursive_caller (count);

    dummy_variable_to_trick_optimizer += 1337;
  }
}

static void
deep_recursive_caller (gint count)
{
  if (count == 1)
    deep_recursive_function (count);
}

G_GNUC_UNUSED static void
spin_for_one_tenth_second (void)
{
  GTimer * timer;
  guint i;
  guint b = 0;

  timer = g_timer_new ();

  do
  {
    for (i = 0; i < 1000000; i++)
      b += i * i;
  }
  while (g_timer_elapsed (timer, NULL) < 0.1);

  dummy_variable_to_trick_optimizer += b;

  g_timer_destroy (timer);
}

GUM_HOOK_TARGET static void
example_a_calls_b_thrice (GumFakeSampler * sampler)
{
  example_b_dynamic (sampler, 1);
  example_b_dynamic (sampler, 3);
  example_b_dynamic (sampler, 2);
}

GUM_HOOK_TARGET static void
example_b_dynamic (GumFakeSampler * sampler,
                   guint cost)
{
  gum_fake_sampler_advance (sampler, cost);
}

GUM_HOOK_TARGET static void
example_worst_case_info (GumFakeSampler * sampler,
                         const gchar * magic,
                         GumSample cost)
{
  gum_fake_sampler_advance (sampler, cost);

  dummy_variable_to_trick_optimizer += GPOINTER_TO_SIZE (magic) & 7;
}

GUM_HOOK_TARGET static void
example_worst_case_recursive (gint count,
                              GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 1);

  if (count > 0)
    example_worst_case_recursive (count - 1, sampler);
}

static void
inspect_worst_case_info (GumInvocationContext * context,
                         gchar * output_buf,
                         guint output_buf_len,
                         gpointer user_data)
{
  const gchar * magic;

  magic = (gchar *) gum_invocation_context_get_nth_argument (context, 1);

#ifdef _MSC_VER
  strcpy_s (output_buf, output_buf_len, magic);
#else
  strcpy (output_buf, magic);
#endif
}

static void
inspect_recursive_worst_case_info (GumInvocationContext * context,
                                   gchar * output_buf,
                                   guint output_buf_len,
                                   gpointer user_data)
{
  gint count;

  count = GPOINTER_TO_INT (
      gum_invocation_context_get_nth_argument (context, 0));

#ifdef _MSC_VER
  sprintf_s (output_buf, output_buf_len, "%d", count);
#else
  sprintf (output_buf, "%d", count);
#endif
}

GUM_HOOK_TARGET static void
dummy (GumFakeSampler * sampler)
{
  (void) sampler;
}

/* These three should be kept in this order to increase the likelihood of
 * function addresses being non-consecutive... */

GUM_HOOK_TARGET static void
simple_2 (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 2);

  dummy_variable_to_trick_optimizer += 2;
}

GUM_HOOK_TARGET static void
simple_1 (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 1);

  dummy_variable_to_trick_optimizer += 1;
}

GUM_HOOK_TARGET static void
simple_3 (GumFakeSampler * sampler)
{
  gum_fake_sampler_advance (sampler, 3);

  dummy_variable_to_trick_optimizer += 3;
}

#define INSTRUMENT_FUNCTION(f) \
    gum_profiler_instrument_function (fixture->profiler, f, fixture->sampler)

static void
instrument_example_functions (TestProfileReportFixture * fixture)
{
  INSTRUMENT_FUNCTION (example_a);
  INSTRUMENT_FUNCTION (example_b);
  INSTRUMENT_FUNCTION (example_c);
  INSTRUMENT_FUNCTION (example_d);
  INSTRUMENT_FUNCTION (example_e);
  INSTRUMENT_FUNCTION (example_f);
  INSTRUMENT_FUNCTION (example_g);
  INSTRUMENT_FUNCTION (example_cyclic_a);
  INSTRUMENT_FUNCTION (example_cyclic_b);
  INSTRUMENT_FUNCTION (example_a_calls_b_thrice);
  INSTRUMENT_FUNCTION (example_b_dynamic);
  INSTRUMENT_FUNCTION (example_worst_case_info);
}

static void
instrument_simple_functions (TestProfileReportFixture * fixture)
{
  INSTRUMENT_FUNCTION (simple_cdecl_42);
  INSTRUMENT_FUNCTION (simple_stdcall_48);
  INSTRUMENT_FUNCTION (simple_stdcall_50);
  INSTRUMENT_FUNCTION (simple_2);
  INSTRUMENT_FUNCTION (simple_1);
  INSTRUMENT_FUNCTION (simple_3);
}
