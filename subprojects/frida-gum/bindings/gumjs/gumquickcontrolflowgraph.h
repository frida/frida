/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CONTROL_FLOW_GRAPH_H__
#define __GUM_QUICK_CONTROL_FLOW_GRAPH_H__

#include "gumquickinstruction.h"

G_BEGIN_DECLS

typedef struct _GumQuickControlFlowGraph GumQuickControlFlowGraph;

struct _GumQuickControlFlowGraph
{
  GumQuickCore * core;
  GumQuickInstruction * instruction;

  JSClassID control_flow_graph_class;
  JSClassID basic_block_class;
};

G_GNUC_INTERNAL void _gum_quick_control_flow_graph_init (
    GumQuickControlFlowGraph * self, JSValue ns,
    GumQuickInstruction * instruction, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_control_flow_graph_dispose (
    GumQuickControlFlowGraph * self);
G_GNUC_INTERNAL void _gum_quick_control_flow_graph_finalize (
    GumQuickControlFlowGraph * self);

G_END_DECLS

#endif
