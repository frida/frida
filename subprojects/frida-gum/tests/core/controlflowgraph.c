/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcontrolflowgraph.h"

#include "testutil.h"

#define TESTCASE(NAME) \
    void test_cfg_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/ControlFlowGraph", test_cfg, NAME)

TESTLIST_BEGIN (cfg)
  TESTENTRY (single_range_dominators_and_sites)
  TESTENTRY (multi_range_cold_rejoin_excludes_bypassed_site)
TESTLIST_END ()

typedef struct _GumTestCode GumTestCode;
typedef struct _GumCollectedSites GumCollectedSites;

struct _GumTestCode
{
  const guint8 * a;
  gsize a_size;
  const guint8 * b;
  gsize b_size;
};

struct _GumCollectedSites
{
  gconstpointer sites[16];
  gsize capacities[16];
  guint n;
};

static gboolean gum_test_find_range (gconstpointer address,
    GumMemoryRange * range, gpointer user_data);
static gboolean gum_test_collect_site (gconstpointer site, gsize capacity,
    gpointer user_data);

/*
 * A start_thread-shaped function: a conditional branch to a 2-byte call that is
 * a branch target with no fall-through predecessor.
 *
 *   0x00: test rax, rax
 *   0x03: je   0x09
 *   0x05: call rax
 *   0x07: jmp  0x0c
 *   0x09: call rax        <- target
 *   0x0b: nop
 *   0x0c: ret
 */
TESTCASE (single_range_dominators_and_sites)
{
  static const guint8 code[] = {
    0x48, 0x85, 0xc0, 0x74, 0x04, 0xff, 0xd0, 0xeb, 0x03, 0xff, 0xd0, 0x90,
    0xc3,
  };
  GumTestCode layout = { code, sizeof (code), NULL, 0 };
  GumControlFlowGraph * cfg;
  GumCollectedSites sites = { { NULL, }, };

  cs_arch_register_x86 ();

  cfg = gum_control_flow_graph_new (code, CS_ARCH_X86, CS_MODE_64,
      gum_test_find_range, &layout);

  g_assert_true (
      gum_control_flow_graph_dominates (cfg, code + 0x00, code + 0x09));
  g_assert_true (
      gum_control_flow_graph_dominates (cfg, code + 0x03, code + 0x09));
  g_assert_false (
      gum_control_flow_graph_dominates (cfg, code + 0x05, code + 0x09));

  gum_control_flow_graph_enumerate_dominating_sites (cfg, code + 0x09,
      gum_test_collect_site, &sites);

  g_assert_cmpuint (sites.n, ==, 3);
  g_assert_true (sites.sites[0] == code + 0x09 && sites.capacities[0] == 3);
  g_assert_true (sites.sites[1] == code + 0x03 && sites.capacities[1] == 6);
  g_assert_true (sites.sites[2] == code + 0x00 && sites.capacities[2] == 9);

  gum_control_flow_graph_free (cfg);
}

/*
 * A function split into a hot body and a cold fragment that rejoins the hot
 * path past one instruction, so that instruction is on the fall-through path
 * but bypassed by the cold path — it must not be reported as dominating.
 *
 *   hot  0x00: test rax, rax
 *        0x03: jne  <cold>
 *        0x09: nop            <- bypassed by the cold path
 *        0x0a: call rax       <- target (rejoin point)
 *        0x0c: ret
 *   cold 0x00: nop
 *        0x01: jmp  hot+0x0a
 */
TESTCASE (multi_range_cold_rejoin_excludes_bypassed_site)
{
  static guint8 hot[] = {
    0x48, 0x85, 0xc0, 0x0f, 0x85, 0, 0, 0, 0, 0x90, 0xff, 0xd0, 0xc3,
  };
  static guint8 cold[] = {
    0x90, 0xe9, 0, 0, 0, 0,
  };
  GumTestCode layout = { hot, sizeof (hot), cold, sizeof (cold) };
  GumControlFlowGraph * cfg;
  GumCollectedSites sites = { { NULL, }, };
  guint i;

  cs_arch_register_x86 ();

  *(gint32 *) (hot + 5) = GINT32_TO_LE (
      (gint32) (GUM_ADDRESS (cold) - (GUM_ADDRESS (hot) + 0x09)));
  *(gint32 *) (cold + 2) = GINT32_TO_LE (
      (gint32) ((GUM_ADDRESS (hot) + 0x0a) - (GUM_ADDRESS (cold) + 0x06)));

  cfg = gum_control_flow_graph_new (hot, CS_ARCH_X86, CS_MODE_64,
      gum_test_find_range, &layout);

  g_assert_true (
      gum_control_flow_graph_dominates (cfg, hot + 0x00, hot + 0x0a));
  g_assert_true (
      gum_control_flow_graph_dominates (cfg, hot + 0x03, hot + 0x0a));
  g_assert_false (
      gum_control_flow_graph_dominates (cfg, hot + 0x09, hot + 0x0a));
  g_assert_false (
      gum_control_flow_graph_dominates (cfg, cold + 0x00, hot + 0x0a));

  gum_control_flow_graph_enumerate_dominating_sites (cfg, hot + 0x0a,
      gum_test_collect_site, &sites);

  for (i = 0; i != sites.n; i++)
  {
    g_assert_true (sites.sites[i] == hot + 0x0a ||
        sites.sites[i] == hot + 0x00 || sites.sites[i] == hot + 0x03);
  }

  gum_control_flow_graph_free (cfg);
}

static gboolean
gum_test_find_range (gconstpointer address,
                     GumMemoryRange * range,
                     gpointer user_data)
{
  GumTestCode * code = user_data;
  GumAddress needle = GUM_ADDRESS (address);

  if (needle >= GUM_ADDRESS (code->a) &&
      needle < GUM_ADDRESS (code->a) + code->a_size)
  {
    range->base_address = GUM_ADDRESS (code->a);
    range->size = code->a_size;
    return TRUE;
  }

  if (code->b != NULL &&
      needle >= GUM_ADDRESS (code->b) &&
      needle < GUM_ADDRESS (code->b) + code->b_size)
  {
    range->base_address = GUM_ADDRESS (code->b);
    range->size = code->b_size;
    return TRUE;
  }

  return FALSE;
}

static gboolean
gum_test_collect_site (gconstpointer site,
                       gsize capacity,
                       gpointer user_data)
{
  GumCollectedSites * collected = user_data;

  collected->sites[collected->n] = site;
  collected->capacities[collected->n] = capacity;
  collected->n++;

  return TRUE;
}
