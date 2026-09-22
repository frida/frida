/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcontrolflowgraph.h"

#include "gumprocess.h"

#define GUM_CONTROL_FLOW_GRAPH_MAX_RANGES 64

typedef struct _GumControlFlowGraphBlock GumControlFlowGraphBlock;
typedef struct _GumControlFlowGraphInsn GumControlFlowGraphInsn;
typedef struct _GumControlFlowGraphInsnRun GumControlFlowGraphInsnRun;

struct _GumControlFlowGraph
{
  GumAddress entry;
  cs_arch arch;

  csh capstone;

  GArray * ranges;
  GArray * insns;
  GArray * insn_runs;
  GArray * raw_targets;
  GArray * branch_targets;
  GArray * blocks;
  GArray * reverse_postorder;

  guint entry_block;
};

struct _GumControlFlowGraphBlock
{
  GumAddress start;
  GumAddress end;

  guint successors[2];
  guint num_successors;

  GArray * predecessors;

  guint idom;
  gint reverse_postorder;
};

struct _GumControlFlowGraphInsn
{
  GumAddress address;
  const cs_insn * raw;
  guint size;
  GumAddress target;
  gboolean ends_block;
  gboolean falls_through;
};

struct _GumControlFlowGraphInsnRun
{
  cs_insn * insns;
  gsize count;
};

static void gum_control_flow_graph_discover_ranges (GumControlFlowGraph * self,
    GumControlFlowGraphFindRangeFunc find_range, gpointer user_data);
static void gum_control_flow_graph_disassemble_range (
    GumControlFlowGraph * self, const GumMemoryRange * range);
static void gum_control_flow_graph_classify_insn (GumControlFlowGraph * self,
    const cs_insn * insn, GumControlFlowGraphInsn * ci);
static void gum_control_flow_graph_index_branch_targets (
    GumControlFlowGraph * self);
static void gum_control_flow_graph_build_blocks (GumControlFlowGraph * self);
static GumAddress gum_control_flow_graph_find_block_end (
    GumControlFlowGraph * self, GumAddress start, GumAddress next_leader);
static void gum_control_flow_graph_build_edges (GumControlFlowGraph * self);
static void gum_control_flow_graph_compute_dominators (
    GumControlFlowGraph * self);
static void gum_control_flow_graph_number_blocks (GumControlFlowGraph * self,
    guint index, guint8 * visited, GArray * postorder);
static guint gum_control_flow_graph_intersect (GumControlFlowGraph * self,
    guint a, guint b);

static gboolean gum_control_flow_graph_address_in_ranges (
    GumControlFlowGraph * self, GumAddress address);
static GumAddress gum_control_flow_graph_range_end_for (
    GumControlFlowGraph * self, GumAddress address);
static void gum_control_flow_graph_add_range (GumControlFlowGraph * self,
    const GumMemoryRange * range);

static guint gum_control_flow_graph_block_index_for_address (
    GumControlFlowGraph * self, GumAddress address);
static GumControlFlowGraphBlock * gum_control_flow_graph_get_block (
    GumControlFlowGraph * self, guint index);
static const GumControlFlowGraphInsn * gum_control_flow_graph_find_insn (
    GumControlFlowGraph * self, GumAddress address);
static GumAddress gum_control_flow_graph_next_branch_target (
    GumControlFlowGraph * self, GumAddress address);

static void gum_control_flow_graph_block_clear (gpointer data);
static void gum_control_flow_graph_insn_run_clear (gpointer data);

static void gum_control_flow_graph_add_leader (GArray * leaders,
    GumAddress address);
static gint gum_control_flow_graph_compare_addresses (gconstpointer a,
    gconstpointer b);
static gint gum_control_flow_graph_compare_block_to_address (
    const GumControlFlowGraphBlock * block, const GumAddress * address);
static gint gum_control_flow_graph_compare_insn_to_address (
    const GumControlFlowGraphInsn * insn, const GumAddress * address);
static gboolean gum_control_flow_graph_resolve_function_range (
    gconstpointer address, GumMemoryRange * range, gpointer user_data);

GumControlFlowGraph *
gum_control_flow_graph_new (gconstpointer entry,
                            cs_arch arch,
                            cs_mode mode,
                            GumControlFlowGraphFindRangeFunc find_range,
                            gpointer user_data)
{
  GumControlFlowGraph * self;

  self = g_slice_new0 (GumControlFlowGraph);
  self->entry = GUM_ADDRESS (entry);
  self->arch = arch;

  cs_open (arch, mode, &self->capstone);
  cs_option (self->capstone, CS_OPT_DETAIL, CS_OPT_ON);
  cs_option (self->capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

  self->ranges = g_array_new (FALSE, FALSE, sizeof (GumMemoryRange));
  self->insns = g_array_new (FALSE, FALSE, sizeof (GumControlFlowGraphInsn));
  self->insn_runs =
      g_array_new (FALSE, FALSE, sizeof (GumControlFlowGraphInsnRun));
  g_array_set_clear_func (self->insn_runs,
      gum_control_flow_graph_insn_run_clear);
  self->raw_targets = g_array_new (FALSE, FALSE, sizeof (GumAddress));
  self->branch_targets = g_array_new (FALSE, FALSE, sizeof (GumAddress));
  self->blocks = g_array_new (FALSE, FALSE, sizeof (GumControlFlowGraphBlock));
  g_array_set_clear_func (self->blocks, gum_control_flow_graph_block_clear);
  self->reverse_postorder = g_array_new (FALSE, FALSE, sizeof (guint));

  gum_control_flow_graph_discover_ranges (self, find_range, user_data);
  g_array_sort (self->insns, gum_control_flow_graph_compare_addresses);
  gum_control_flow_graph_index_branch_targets (self);
  gum_control_flow_graph_build_blocks (self);
  gum_control_flow_graph_build_edges (self);
  gum_control_flow_graph_compute_dominators (self);

  return self;
}

GumControlFlowGraph *
gum_control_flow_graph_new_for_function (gconstpointer entry_point)
{
  gconstpointer entry = entry_point;
  cs_mode mode = GUM_DEFAULT_CS_MODE;

  gum_cs_arch_register_native ();

#ifdef HAVE_ARM
  if ((GPOINTER_TO_SIZE (entry_point) & 1) != 0)
  {
    mode = (cs_mode) (CS_MODE_THUMB | CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN);
    entry = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (entry_point) & ~(gsize) 1);
  }
#endif

  return gum_control_flow_graph_new (entry, GUM_DEFAULT_CS_ARCH, mode,
      gum_control_flow_graph_resolve_function_range, NULL);
}

void
gum_control_flow_graph_free (GumControlFlowGraph * self)
{
  if (self == NULL)
    return;

  g_array_free (self->reverse_postorder, TRUE);
  g_array_free (self->blocks, TRUE);
  g_array_free (self->branch_targets, TRUE);
  g_array_free (self->raw_targets, TRUE);
  g_array_free (self->insn_runs, TRUE);
  g_array_free (self->insns, TRUE);
  g_array_free (self->ranges, TRUE);

  cs_close (&self->capstone);

  g_slice_free (GumControlFlowGraph, self);
}

gboolean
gum_control_flow_graph_dominates (GumControlFlowGraph * self,
                                  gconstpointer a,
                                  gconstpointer b)
{
  GumAddress addr_a = GUM_ADDRESS (a);
  GumAddress addr_b = GUM_ADDRESS (b);
  guint block_a, block_b, cur;

  block_a = gum_control_flow_graph_block_index_for_address (self, addr_a);
  block_b = gum_control_flow_graph_block_index_for_address (self, addr_b);
  if (block_a == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK ||
      block_b == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
    return FALSE;

  if (block_a == block_b)
    return addr_a <= addr_b;

  cur = block_b;
  while (cur != GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
  {
    GumControlFlowGraphBlock * block =
        gum_control_flow_graph_get_block (self, cur);
    if (block->idom == cur)
      break;
    cur = block->idom;
    if (cur == block_a)
      return TRUE;
  }

  return FALSE;
}

void
gum_control_flow_graph_enumerate_dominating_sites (
    GumControlFlowGraph * self,
    gconstpointer target,
    GumFoundDominatingSiteFunc func,
    gpointer user_data)
{
  GumAddress target_address = GUM_ADDRESS (target);
  guint cur;
  GumAddress ceiling;

  cur = gum_control_flow_graph_block_index_for_address (self, target_address);
  if (cur == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
    return;

  /*
   * Walk from the target's block up its dominator chain. Every instruction of a
   * dominating basic block lies on every path to the target, so each one is a
   * valid site; we emit them nearest-first.
   */
  ceiling = target_address;
  while (cur != GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
  {
    GumControlFlowGraphBlock * block =
        gum_control_flow_graph_get_block (self, cur);
    GArray * boundaries;
    GumAddress address;
    guint i;

    boundaries = g_array_new (FALSE, FALSE, sizeof (GumAddress));
    for (address = block->start; address != block->end && address <= ceiling; )
    {
      const GumControlFlowGraphInsn * insn =
          gum_control_flow_graph_find_insn (self, address);
      if (insn == NULL)
        break;
      g_array_append_val (boundaries, address);
      address += insn->size;
    }

    for (i = boundaries->len; i != 0; i--)
    {
      GumAddress site = g_array_index (boundaries, GumAddress, i - 1);
      gsize capacity =
          gum_control_flow_graph_next_branch_target (self, site) - site;

      if (!func (GSIZE_TO_POINTER (site), capacity, user_data))
      {
        g_array_free (boundaries, TRUE);
        return;
      }
    }

    g_array_free (boundaries, TRUE);

    if (block->idom == cur)
      break;
    cur = block->idom;
    ceiling = G_MAXUINT64;
  }
}

guint
gum_control_flow_graph_get_num_blocks (GumControlFlowGraph * self)
{
  return self->blocks->len;
}

guint
gum_control_flow_graph_get_entry_block (GumControlFlowGraph * self)
{
  if (self->blocks->len == 0)
    return GUM_CONTROL_FLOW_GRAPH_NO_BLOCK;

  return self->entry_block;
}

guint
gum_control_flow_graph_find_block_containing (GumControlFlowGraph * self,
                                   gconstpointer address)
{
  return gum_control_flow_graph_block_index_for_address (self,
      GUM_ADDRESS (address));
}

void
gum_control_flow_graph_get_block_bounds (GumControlFlowGraph * self,
                                         guint index,
                                         GumAddress * start,
                                         GumAddress * end)
{
  GumControlFlowGraphBlock * block =
      gum_control_flow_graph_get_block (self, index);

  *start = block->start;
  *end = block->end;
}

guint
gum_control_flow_graph_get_block_immediate_dominator (
    GumControlFlowGraph * self,
    guint index)
{
  GumControlFlowGraphBlock * block =
      gum_control_flow_graph_get_block (self, index);

  if (block->idom == index)
    return GUM_CONTROL_FLOW_GRAPH_NO_BLOCK;

  return block->idom;
}

guint
gum_control_flow_graph_get_block_successors (GumControlFlowGraph * self,
                                             guint index,
                                             const guint ** successors)
{
  GumControlFlowGraphBlock * block =
      gum_control_flow_graph_get_block (self, index);

  *successors = block->successors;
  return block->num_successors;
}

guint
gum_control_flow_graph_get_block_predecessors (GumControlFlowGraph * self,
                                               guint index,
                                               const guint ** predecessors)
{
  GumControlFlowGraphBlock * block =
      gum_control_flow_graph_get_block (self, index);

  *predecessors = (const guint *) block->predecessors->data;
  return block->predecessors->len;
}

const cs_insn *
gum_control_flow_graph_find_instruction_containing (GumControlFlowGraph * self,
                                         gconstpointer address)
{
  const GumControlFlowGraphInsn * insn =
      gum_control_flow_graph_find_insn (self, GUM_ADDRESS (address));

  if (insn == NULL)
    return NULL;

  return insn->raw;
}

static void
gum_control_flow_graph_discover_ranges (
    GumControlFlowGraph * self,
    GumControlFlowGraphFindRangeFunc find_range,
    gpointer user_data)
{
  GumMemoryRange range;
  guint i;

  if (!find_range (GSIZE_TO_POINTER (self->entry), &range, user_data))
    return;
  gum_control_flow_graph_add_range (self, &range);

  i = 0;
  while (i != self->ranges->len)
  {
    GumMemoryRange current = g_array_index (self->ranges, GumMemoryRange, i);
    guint t;

    gum_control_flow_graph_disassemble_range (self, &current);
    i++;

    if (self->ranges->len >= GUM_CONTROL_FLOW_GRAPH_MAX_RANGES)
      break;

    for (t = 0; t != self->raw_targets->len; t++)
    {
      GumAddress target = g_array_index (self->raw_targets, GumAddress, t);
      GumMemoryRange discovered;

      if (gum_control_flow_graph_address_in_ranges (self, target))
        continue;

      if (find_range (GSIZE_TO_POINTER (target), &discovered, user_data) &&
          !gum_control_flow_graph_address_in_ranges (self,
              discovered.base_address))
      {
        if (self->ranges->len >= GUM_CONTROL_FLOW_GRAPH_MAX_RANGES)
          break;
        gum_control_flow_graph_add_range (self, &discovered);
      }
    }
  }
}

static void
gum_control_flow_graph_disassemble_range (GumControlFlowGraph * self,
                                          const GumMemoryRange * range)
{
  cs_insn * insns;
  size_t count, i;
  GumControlFlowGraphInsnRun run;

  count = cs_disasm (self->capstone,
      GSIZE_TO_POINTER (range->base_address), range->size,
      range->base_address, 0, &insns);
  if (count == 0)
    return;

  for (i = 0; i != count; i++)
  {
    GumControlFlowGraphInsn ci;

    gum_control_flow_graph_classify_insn (self, &insns[i], &ci);
    g_array_append_val (self->insns, ci);

    if (ci.ends_block && ci.target != 0)
      g_array_append_val (self->raw_targets, ci.target);
  }

  run.insns = insns;
  run.count = count;
  g_array_append_val (self->insn_runs, run);
}

static void
gum_control_flow_graph_classify_insn (GumControlFlowGraph * self,
                                      const cs_insn * insn,
                                      GumControlFlowGraphInsn * ci)
{
  csh capstone = self->capstone;

  ci->raw = insn;
  ci->address = insn->address;
  ci->size = insn->size;
  ci->target = 0;
  ci->ends_block = FALSE;
  ci->falls_through = TRUE;

  if (cs_insn_group (capstone, insn, CS_GRP_RET) ||
      cs_insn_group (capstone, insn, CS_GRP_IRET))
  {
    ci->ends_block = TRUE;
    ci->falls_through = FALSE;
    return;
  }

  if (!cs_insn_group (capstone, insn, CS_GRP_JUMP))
    return;

  ci->ends_block = TRUE;

  switch (self->arch)
  {
    case CS_ARCH_X86:
    {
      const cs_x86 * x86 = &insn->detail->x86;

      ci->falls_through = (insn->id != X86_INS_JMP);

      if (x86->op_count >= 1 && x86->operands[0].type == X86_OP_IMM)
        ci->target = x86->operands[0].imm;

      break;
    }
    case CS_ARCH_ARM64:
    {
      const cs_arm64 * arm64 = &insn->detail->arm64;

      ci->falls_through = (arm64->cc != ARM64_CC_INVALID &&
          arm64->cc != ARM64_CC_AL) || insn->id == ARM64_INS_CBZ ||
          insn->id == ARM64_INS_CBNZ || insn->id == ARM64_INS_TBZ ||
          insn->id == ARM64_INS_TBNZ;

      if (arm64->op_count >= 1 &&
          arm64->operands[arm64->op_count - 1].type == ARM64_OP_IMM)
      {
        ci->target = arm64->operands[arm64->op_count - 1].imm;
      }

      break;
    }
    default:
      ci->falls_through = FALSE;
      break;
  }
}

static void
gum_control_flow_graph_index_branch_targets (GumControlFlowGraph * self)
{
  guint i;

  for (i = 0; i != self->raw_targets->len; i++)
  {
    GumAddress target = g_array_index (self->raw_targets, GumAddress, i);
    if (gum_control_flow_graph_address_in_ranges (self, target))
      g_array_append_val (self->branch_targets, target);
  }

  g_array_sort (self->branch_targets, gum_control_flow_graph_compare_addresses);
}

static void
gum_control_flow_graph_build_blocks (GumControlFlowGraph * self)
{
  GArray * leaders;
  guint i;

  leaders = g_array_new (FALSE, FALSE, sizeof (GumAddress));
  gum_control_flow_graph_add_leader (leaders, self->entry);

  for (i = 0; i != self->ranges->len; i++)
  {
    GumMemoryRange * r = &g_array_index (self->ranges, GumMemoryRange, i);
    gum_control_flow_graph_add_leader (leaders, r->base_address);
  }

  for (i = 0; i != self->insns->len; i++)
  {
    GumControlFlowGraphInsn * insn =
        &g_array_index (self->insns, GumControlFlowGraphInsn, i);
    if (!insn->ends_block)
      continue;

    if (insn->target != 0 &&
        gum_control_flow_graph_address_in_ranges (self, insn->target))
      gum_control_flow_graph_add_leader (leaders, insn->target);

    if (insn->falls_through &&
        gum_control_flow_graph_find_insn (self,
            insn->address + insn->size) != NULL)
    {
      gum_control_flow_graph_add_leader (leaders, insn->address + insn->size);
    }
  }

  g_array_sort (leaders, gum_control_flow_graph_compare_addresses);

  for (i = 0; i != leaders->len; i++)
  {
    GumControlFlowGraphBlock block;
    GumAddress next_leader;

    block.start = g_array_index (leaders, GumAddress, i);
    next_leader = (i + 1 < leaders->len)
        ? g_array_index (leaders, GumAddress, i + 1)
        : G_MAXUINT64;
    block.end = gum_control_flow_graph_find_block_end (self, block.start,
        next_leader);

    block.num_successors = 0;
    block.predecessors = g_array_new (FALSE, FALSE, sizeof (guint));
    block.idom = GUM_CONTROL_FLOW_GRAPH_NO_BLOCK;
    block.reverse_postorder = -1;

    if (self->entry >= block.start && self->entry < block.end)
      self->entry_block = self->blocks->len;

    g_array_append_val (self->blocks, block);
  }

  g_array_free (leaders, TRUE);
}

static GumAddress
gum_control_flow_graph_find_block_end (GumControlFlowGraph * self,
                                       GumAddress start,
                                       GumAddress next_leader)
{
  GumAddress address = start;

  for (;;)
  {
    const GumControlFlowGraphInsn * insn =
        gum_control_flow_graph_find_insn (self, address);
    GumAddress next;

    if (insn == NULL)
      return address;

    next = address + insn->size;

    if (insn->ends_block)
      return next;
    if (gum_control_flow_graph_find_insn (self, next) == NULL)
      return next;
    if (next == next_leader)
      return next;

    address = next;
  }
}

static void
gum_control_flow_graph_build_edges (GumControlFlowGraph * self)
{
  guint i;

  for (i = 0; i != self->blocks->len; i++)
  {
    GumControlFlowGraphBlock * block =
        gum_control_flow_graph_get_block (self, i);
    const GumControlFlowGraphInsn * last = NULL;
    GumAddress address;
    GumAddress fallthrough;

    for (address = block->start; address != block->end; )
    {
      last = gum_control_flow_graph_find_insn (self, address);
      if (last == NULL)
        break;
      address += last->size;
    }
    if (last == NULL)
      continue;

    fallthrough = last->address + last->size;

    if (last->ends_block)
    {
      if (last->target != 0)
      {
        guint t = gum_control_flow_graph_block_index_for_address (self,
            last->target);
        if (t != GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
          block->successors[block->num_successors++] = t;
      }

      if (last->falls_through &&
          gum_control_flow_graph_find_insn (self, fallthrough) != NULL)
      {
        guint n = gum_control_flow_graph_block_index_for_address (self,
            fallthrough);
        if (n != GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
          block->successors[block->num_successors++] = n;
      }
    }
    else if (gum_control_flow_graph_find_insn (self, fallthrough) != NULL)
    {
      guint n = gum_control_flow_graph_block_index_for_address (self,
          fallthrough);
      if (n != GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
        block->successors[block->num_successors++] = n;
    }
  }

  for (i = 0; i != self->blocks->len; i++)
  {
    GumControlFlowGraphBlock * block =
        gum_control_flow_graph_get_block (self, i);
    guint s;

    for (s = 0; s != block->num_successors; s++)
    {
      GumControlFlowGraphBlock * succ =
          gum_control_flow_graph_get_block (self, block->successors[s]);
      g_array_append_val (succ->predecessors, i);
    }
  }
}

static void
gum_control_flow_graph_compute_dominators (GumControlFlowGraph * self)
{
  guint n = self->blocks->len;
  guint8 * visited;
  GArray * postorder;
  guint i;
  gboolean changed;

  if (n == 0)
    return;

  visited = g_new0 (guint8, n);
  postorder = g_array_new (FALSE, FALSE, sizeof (guint));
  gum_control_flow_graph_number_blocks (self, self->entry_block, visited,
      postorder);

  for (i = postorder->len; i != 0; i--)
  {
    guint index = g_array_index (postorder, guint, i - 1);
    guint reverse_postorder = postorder->len - i;
    gum_control_flow_graph_get_block (self, index)->reverse_postorder =
        reverse_postorder;
    g_array_append_val (self->reverse_postorder, index);
  }

  gum_control_flow_graph_get_block (self, self->entry_block)->idom =
      self->entry_block;

  do
  {
    changed = FALSE;

    for (i = 0; i != self->reverse_postorder->len; i++)
    {
      guint index = g_array_index (self->reverse_postorder, guint, i);
      GumControlFlowGraphBlock * block;
      guint new_idom = GUM_CONTROL_FLOW_GRAPH_NO_BLOCK;
      guint p;

      if (index == self->entry_block)
        continue;

      block = gum_control_flow_graph_get_block (self, index);

      for (p = 0; p != block->predecessors->len; p++)
      {
        guint pred = g_array_index (block->predecessors, guint, p);
        if (gum_control_flow_graph_get_block (self, pred)->idom ==
            GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
          continue;

        new_idom = (new_idom == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
            ? pred
            : gum_control_flow_graph_intersect (self, pred, new_idom);
      }

      if (new_idom != GUM_CONTROL_FLOW_GRAPH_NO_BLOCK &&
          block->idom != new_idom)
      {
        block->idom = new_idom;
        changed = TRUE;
      }
    }
  }
  while (changed);

  g_array_free (postorder, TRUE);
  g_free (visited);
}

static void
gum_control_flow_graph_number_blocks (GumControlFlowGraph * self,
                                      guint index,
                                      guint8 * visited,
                                      GArray * postorder)
{
  GumControlFlowGraphBlock * block =
      gum_control_flow_graph_get_block (self, index);
  guint s;

  visited[index] = TRUE;

  for (s = 0; s != block->num_successors; s++)
  {
    guint succ = block->successors[s];
    if (!visited[succ])
      gum_control_flow_graph_number_blocks (self, succ, visited, postorder);
  }

  g_array_append_val (postorder, index);
}

static guint
gum_control_flow_graph_intersect (GumControlFlowGraph * self,
                                  guint a,
                                  guint b)
{
  while (a != b)
  {
    while (gum_control_flow_graph_get_block (self, a)->reverse_postorder >
        gum_control_flow_graph_get_block (self, b)->reverse_postorder)
      a = gum_control_flow_graph_get_block (self, a)->idom;
    while (gum_control_flow_graph_get_block (self, b)->reverse_postorder >
        gum_control_flow_graph_get_block (self, a)->reverse_postorder)
      b = gum_control_flow_graph_get_block (self, b)->idom;
  }

  return a;
}

static gboolean
gum_control_flow_graph_address_in_ranges (GumControlFlowGraph * self,
                                          GumAddress address)
{
  guint i;

  for (i = 0; i != self->ranges->len; i++)
  {
    GumMemoryRange * r = &g_array_index (self->ranges, GumMemoryRange, i);
    if (address >= r->base_address && address < r->base_address + r->size)
      return TRUE;
  }

  return FALSE;
}

static GumAddress
gum_control_flow_graph_range_end_for (GumControlFlowGraph * self,
                                      GumAddress address)
{
  guint i;

  for (i = 0; i != self->ranges->len; i++)
  {
    GumMemoryRange * r = &g_array_index (self->ranges, GumMemoryRange, i);
    if (address >= r->base_address && address < r->base_address + r->size)
      return r->base_address + r->size;
  }

  return address;
}

static void
gum_control_flow_graph_add_range (GumControlFlowGraph * self,
                                  const GumMemoryRange * range)
{
  g_array_append_val (self->ranges, *range);
}

static guint
gum_control_flow_graph_block_index_for_address (GumControlFlowGraph * self,
                                                GumAddress address)
{
  guint index;

  if (!g_array_binary_search (self->blocks, &address,
        (GCompareFunc) gum_control_flow_graph_compare_block_to_address, &index))
    return GUM_CONTROL_FLOW_GRAPH_NO_BLOCK;

  return index;
}

static GumControlFlowGraphBlock *
gum_control_flow_graph_get_block (GumControlFlowGraph * self,
                                  guint index)
{
  return &g_array_index (self->blocks, GumControlFlowGraphBlock, index);
}

static const GumControlFlowGraphInsn *
gum_control_flow_graph_find_insn (GumControlFlowGraph * self,
                                  GumAddress address)
{
  guint index;

  if (!g_array_binary_search (self->insns, &address,
        (GCompareFunc) gum_control_flow_graph_compare_insn_to_address, &index))
    return NULL;

  return &g_array_index (self->insns, GumControlFlowGraphInsn, index);
}

static GumAddress
gum_control_flow_graph_next_branch_target (GumControlFlowGraph * self,
                                           GumAddress address)
{
  GArray * targets = self->branch_targets;
  guint lo = 0, hi = targets->len;
  GumAddress next, range_end;

  while (lo < hi)
  {
    guint mid = (lo + hi) / 2;
    GumAddress t = g_array_index (targets, GumAddress, mid);

    if (t <= address)
      lo = mid + 1;
    else
      hi = mid;
  }

  next = (lo < targets->len)
      ? g_array_index (targets, GumAddress, lo)
      : G_MAXUINT64;

  /* A redirect window must not span a gap between ranges. */
  range_end = gum_control_flow_graph_range_end_for (self, address);

  return MIN (next, range_end);
}

static void
gum_control_flow_graph_block_clear (gpointer data)
{
  GumControlFlowGraphBlock * block = data;

  g_array_free (block->predecessors, TRUE);
}

static void
gum_control_flow_graph_insn_run_clear (gpointer data)
{
  GumControlFlowGraphInsnRun * run = data;

  cs_free (run->insns, run->count);
}

static void
gum_control_flow_graph_add_leader (GArray * leaders,
                                   GumAddress address)
{
  guint i;

  for (i = 0; i != leaders->len; i++)
  {
    if (g_array_index (leaders, GumAddress, i) == address)
      return;
  }

  g_array_append_val (leaders, address);
}

static gint
gum_control_flow_graph_compare_addresses (gconstpointer a,
                                          gconstpointer b)
{
  GumAddress addr_a = *(const GumAddress *) a;
  GumAddress addr_b = *(const GumAddress *) b;

  if (addr_a < addr_b)
    return -1;
  if (addr_a > addr_b)
    return 1;
  return 0;
}

static gint
gum_control_flow_graph_compare_block_to_address (
    const GumControlFlowGraphBlock * block,
    const GumAddress * address)
{
  if (block->end <= *address)
    return -1;
  if (*address < block->start)
    return 1;
  return 0;
}

static gint
gum_control_flow_graph_compare_insn_to_address (
    const GumControlFlowGraphInsn * insn,
    const GumAddress * address)
{
  if (insn->address < *address)
    return -1;
  if (insn->address > *address)
    return 1;
  return 0;
}

static gboolean
gum_control_flow_graph_resolve_function_range (gconstpointer address,
                                               GumMemoryRange * range,
                                               gpointer user_data)
{
  return gum_process_find_function_range (address, range);
}
