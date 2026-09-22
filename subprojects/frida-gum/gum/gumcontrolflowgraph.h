/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_CONTROL_FLOW_GRAPH_H__
#define __GUM_CONTROL_FLOW_GRAPH_H__

#include <gum/gumdefs.h>
#include <gum/gummemory.h>

#include <capstone.h>

G_BEGIN_DECLS

#define GUM_CONTROL_FLOW_GRAPH_NO_BLOCK G_MAXUINT

typedef struct _GumControlFlowGraph GumControlFlowGraph;

/**
 * GumControlFlowGraphFindRangeFunc:
 * @address: a code address to locate
 * @range: (out): the contiguous code range covering @address
 * @user_data: data passed to gum_control_flow_graph_new()
 *
 * Resolves the contiguous range of code that @address belongs to. Called for
 * the entry, and for every direct branch target that falls outside the ranges
 * discovered so far — this is how a function split across multiple ranges (e.g.
 * a hot body plus a cold .text.unlikely fragment) gets stitched together.
 *
 * Returns: %TRUE if a range was found
 */
typedef gboolean (* GumControlFlowGraphFindRangeFunc) (gconstpointer address,
    GumMemoryRange * range, gpointer user_data);

/**
 * GumFoundDominatingSiteFunc:
 * @site: instruction-aligned address that dominates the target
 * @capacity: number of contiguous bytes at @site with no incoming branch and
 *          within a single range — how much may be overwritten by a redirect
 *          without another control-flow edge landing inside it
 * @user_data: data passed to the dominating-site enumerator
 *
 * Returns: %TRUE to keep enumerating, %FALSE to stop
 */
typedef gboolean (* GumFoundDominatingSiteFunc) (gconstpointer site,
    gsize capacity, gpointer user_data);

GUM_API GumControlFlowGraph * gum_control_flow_graph_new (gconstpointer entry,
    cs_arch arch, cs_mode mode, GumControlFlowGraphFindRangeFunc find_range,
    gpointer user_data);
/*
 * Convenience constructor that resolves ranges via the platform's unwind
 * tables and derives the disassembly mode from the native architecture, with
 * the low bit selecting Thumb on 32-bit ARM.
 */
GUM_API GumControlFlowGraph * gum_control_flow_graph_new_for_function (
    gconstpointer entry_point);
GUM_API void gum_control_flow_graph_free (GumControlFlowGraph * self);

GUM_API gboolean gum_control_flow_graph_dominates (GumControlFlowGraph * self,
    gconstpointer a, gconstpointer b);

GUM_API void gum_control_flow_graph_enumerate_dominating_sites (
    GumControlFlowGraph * self, gconstpointer target,
    GumFoundDominatingSiteFunc func, gpointer user_data);

GUM_API guint gum_control_flow_graph_get_num_blocks (
    GumControlFlowGraph * self);
GUM_API guint gum_control_flow_graph_get_entry_block (
    GumControlFlowGraph * self);
GUM_API guint gum_control_flow_graph_find_block_containing (
    GumControlFlowGraph * self, gconstpointer address);
GUM_API void gum_control_flow_graph_get_block_bounds (
    GumControlFlowGraph * self, guint index, GumAddress * start,
    GumAddress * end);
GUM_API guint gum_control_flow_graph_get_block_immediate_dominator (
    GumControlFlowGraph * self, guint index);
GUM_API guint gum_control_flow_graph_get_block_successors (
    GumControlFlowGraph * self, guint index, const guint ** successors);
GUM_API guint gum_control_flow_graph_get_block_predecessors (
    GumControlFlowGraph * self, guint index, const guint ** predecessors);

GUM_API const cs_insn * gum_control_flow_graph_find_instruction_containing (
    GumControlFlowGraph * self, gconstpointer address);

G_END_DECLS

#endif
