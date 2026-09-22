/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CONTROL_FLOW_GRAPH_H__
#define __GUM_V8_CONTROL_FLOW_GRAPH_H__

#include "gumv8instruction.h"

struct GumV8ControlFlowGraph
{
  GumV8Core * core;
  GumV8Instruction * instruction;

  GHashTable * graphs;
  GHashTable * blocks;

  v8::Global<v8::FunctionTemplate> * basic_block;
  v8::Global<v8::Object> * basic_block_value;
};

G_GNUC_INTERNAL void _gum_v8_control_flow_graph_init (
    GumV8ControlFlowGraph * self, GumV8Instruction * instruction,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_control_flow_graph_realize (
    GumV8ControlFlowGraph * self);
G_GNUC_INTERNAL void _gum_v8_control_flow_graph_dispose (
    GumV8ControlFlowGraph * self);
G_GNUC_INTERNAL void _gum_v8_control_flow_graph_finalize (
    GumV8ControlFlowGraph * self);

#endif
