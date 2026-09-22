/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8controlflowgraph.h"

#include "gumv8macros.h"

#include <gum/gumcontrolflowgraph.h>

#define GUMJS_MODULE_NAME ControlFlowGraph

using namespace v8;

struct GumV8ControlFlowGraphValue
{
  Global<Object> * object;
  GumControlFlowGraph * handle;
  gconstpointer entrypoint;

  GumV8ControlFlowGraph * module;
};

struct GumV8BasicBlockValue
{
  Global<Object> * object;
  Global<Object> * cfg;
  GumControlFlowGraph * handle;
  guint index;

  GumV8ControlFlowGraph * module;
};

struct GumV8DominatingSitesContext
{
  GumV8ControlFlowGraph * module;

  Local<Array> elements;
  guint n;
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_control_flow_graph_construct)
GUMJS_DECLARE_GETTER (gumjs_control_flow_graph_get_entrypoint)
GUMJS_DECLARE_GETTER (gumjs_control_flow_graph_get_entry_block)
GUMJS_DECLARE_GETTER (gumjs_control_flow_graph_get_blocks)
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_find_block_containing)
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_dominates)
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_enumerate_dominating_sites)
static gboolean gum_v8_collect_dominating_site (gconstpointer site,
    gsize capacity, GumV8DominatingSitesContext * dc);
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_to_json)
static GumV8ControlFlowGraphValue * gum_v8_control_flow_graph_value_new (
    Local<Object> wrapper, GumControlFlowGraph * handle,
    gconstpointer entrypoint, GumV8ControlFlowGraph * module);
static void gum_v8_control_flow_graph_value_free (
    GumV8ControlFlowGraphValue * self);
static void gum_v8_control_flow_graph_value_on_weak_notify (
    const WeakCallbackInfo<GumV8ControlFlowGraphValue> & info);

GUMJS_DECLARE_GETTER (gumjs_basic_block_get_start)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_end)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_successors)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_predecessors)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_immediate_dominator)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_instructions)
GUMJS_DECLARE_FUNCTION (gumjs_basic_block_to_json)
static Local<Object> gum_v8_basic_block_new (Local<Object> cfg,
    GumControlFlowGraph * handle, guint index, GumV8ControlFlowGraph * module);
static Local<Value> gum_v8_basic_block_start_new (GumControlFlowGraph * handle,
    guint index, GumV8Core * core);
static void gum_v8_basic_block_value_free (GumV8BasicBlockValue * self);
static void gum_v8_basic_block_value_on_weak_notify (
    const WeakCallbackInfo<GumV8BasicBlockValue> & info);

static Local<Object> gum_v8_properties_to_json (Local<Object> object,
    const GumV8Property * properties, Isolate * isolate);

static const GumV8Property gumjs_control_flow_graph_values[] =
{
  { "entrypoint", gumjs_control_flow_graph_get_entrypoint, NULL },
  { "entryBlock", gumjs_control_flow_graph_get_entry_block, NULL },
  { "blocks", gumjs_control_flow_graph_get_blocks, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_control_flow_graph_functions[] =
{
  { "findBlockContaining", gumjs_control_flow_graph_find_block_containing },
  { "dominates", gumjs_control_flow_graph_dominates },
  { "enumerateDominatingSites",
      gumjs_control_flow_graph_enumerate_dominating_sites },
  { "toJSON", gumjs_control_flow_graph_to_json },

  { NULL, NULL }
};

static const GumV8Property gumjs_basic_block_values[] =
{
  { "start", gumjs_basic_block_get_start, NULL },
  { "end", gumjs_basic_block_get_end, NULL },
  { "successors", gumjs_basic_block_get_successors, NULL },
  { "predecessors", gumjs_basic_block_get_predecessors, NULL },
  { "immediateDominator", gumjs_basic_block_get_immediate_dominator, NULL },
  { "instructions", gumjs_basic_block_get_instructions, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_basic_block_functions[] =
{
  { "toJSON", gumjs_basic_block_to_json },

  { NULL, NULL }
};

void
_gum_v8_control_flow_graph_init (GumV8ControlFlowGraph * self,
                                 GumV8Instruction * instruction,
                                 GumV8Core * core,
                                 Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;
  self->instruction = instruction;

  auto module = External::New (isolate, self);

  auto cfg = _gum_v8_create_class ("ControlFlowGraph",
      gumjs_control_flow_graph_construct, scope, module, isolate);
  _gum_v8_class_add (cfg, gumjs_control_flow_graph_values, module, isolate);
  _gum_v8_class_add (cfg, gumjs_control_flow_graph_functions, module, isolate);

  auto basic_block = _gum_v8_create_class ("BasicBlock", nullptr, scope, module,
      isolate);
  _gum_v8_class_add (basic_block, gumjs_basic_block_values, module, isolate);
  _gum_v8_class_add (basic_block, gumjs_basic_block_functions, module, isolate);
  self->basic_block = new Global<FunctionTemplate> (isolate, basic_block);
}

void
_gum_v8_control_flow_graph_realize (GumV8ControlFlowGraph * self)
{
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  self->graphs = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_control_flow_graph_value_free);
  self->blocks = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_basic_block_value_free);

  auto basic_block = Local<FunctionTemplate>::New (isolate, *self->basic_block);
  auto basic_block_value = basic_block->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 0, nullptr).ToLocalChecked ();
  self->basic_block_value = new Global<Object> (isolate, basic_block_value);
}

void
_gum_v8_control_flow_graph_dispose (GumV8ControlFlowGraph * self)
{
  g_hash_table_unref (self->blocks);
  self->blocks = NULL;

  g_hash_table_unref (self->graphs);
  self->graphs = NULL;

  delete self->basic_block_value;
  self->basic_block_value = nullptr;

  delete self->basic_block;
  self->basic_block = nullptr;
}

void
_gum_v8_control_flow_graph_finalize (GumV8ControlFlowGraph * self)
{
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_control_flow_graph_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new ControlFlowGraph()` to create a new instance");
    return;
  }

  gpointer entrypoint;
  if (!_gum_v8_args_parse (args, "p", &entrypoint))
    return;

  auto handle = gum_control_flow_graph_new_for_function (entrypoint);
  if (gum_control_flow_graph_get_num_blocks (handle) == 0)
  {
    gum_control_flow_graph_free (handle);
    _gum_v8_throw_ascii_literal (isolate,
        "unable to determine the bounds of the function");
    return;
  }

  auto cfg = gum_v8_control_flow_graph_value_new (wrapper, handle, entrypoint,
      module);
  wrapper->SetAlignedPointerInInternalField (0, cfg);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_control_flow_graph_get_entrypoint,
                           GumV8ControlFlowGraphValue)
{
  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new ((gpointer) self->entrypoint, core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_control_flow_graph_get_entry_block,
                           GumV8ControlFlowGraphValue)
{
  guint index = gum_control_flow_graph_get_entry_block (self->handle);
  if (index == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
  {
    info.GetReturnValue ().SetNull ();
    return;
  }

  info.GetReturnValue ().Set (
      gum_v8_basic_block_new (wrapper, self->handle, index, module));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_control_flow_graph_get_blocks,
                           GumV8ControlFlowGraphValue)
{
  auto context = isolate->GetCurrentContext ();

  auto result = Array::New (isolate);
  guint n = gum_control_flow_graph_get_num_blocks (self->handle);
  for (guint i = 0; i != n; i++)
  {
    result->Set (context, i,
        gum_v8_basic_block_new (wrapper, self->handle, i, module)).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_control_flow_graph_find_block_containing,
                           GumV8ControlFlowGraphValue)
{
  gpointer address;
  if (!_gum_v8_args_parse (args, "p", &address))
    return;

  guint index =
      gum_control_flow_graph_find_block_containing (self->handle, address);
  if (index == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
  {
    info.GetReturnValue ().SetNull ();
    return;
  }

  info.GetReturnValue ().Set (
      gum_v8_basic_block_new (wrapper, self->handle, index, module));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_control_flow_graph_dominates,
                           GumV8ControlFlowGraphValue)
{
  gpointer a, b;
  if (!_gum_v8_args_parse (args, "pp", &a, &b))
    return;

  info.GetReturnValue ().Set (
      (bool) gum_control_flow_graph_dominates (self->handle, a, b));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_control_flow_graph_enumerate_dominating_sites,
                           GumV8ControlFlowGraphValue)
{
  gpointer target;
  if (!_gum_v8_args_parse (args, "p", &target))
    return;

  GumV8DominatingSitesContext dc;
  dc.module = module;
  dc.elements = Array::New (isolate);
  dc.n = 0;

  gum_control_flow_graph_enumerate_dominating_sites (self->handle, target,
      (GumFoundDominatingSiteFunc) gum_v8_collect_dominating_site, &dc);

  info.GetReturnValue ().Set (dc.elements);
}

static gboolean
gum_v8_collect_dominating_site (gconstpointer site,
                                gsize capacity,
                                GumV8DominatingSitesContext * dc)
{
  auto core = dc->module->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto element = Object::New (isolate);
  _gum_v8_object_set_pointer (element, "address", (gpointer) site, core);
  _gum_v8_object_set_uint (element, "capacity", capacity, core);

  dc->elements->Set (context, dc->n++, element).Check ();

  return TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_control_flow_graph_to_json)
{
  info.GetReturnValue ().Set (gum_v8_properties_to_json (info.This (),
      gumjs_control_flow_graph_values, isolate));
}

static GumV8ControlFlowGraphValue *
gum_v8_control_flow_graph_value_new (Local<Object> wrapper,
                                     GumControlFlowGraph * handle,
                                     gconstpointer entrypoint,
                                     GumV8ControlFlowGraph * module)
{
  auto cfg = g_slice_new (GumV8ControlFlowGraphValue);
  cfg->object = new Global<Object> (module->core->isolate, wrapper);
  cfg->object->SetWeak (cfg, gum_v8_control_flow_graph_value_on_weak_notify,
      WeakCallbackType::kParameter);
  cfg->handle = handle;
  cfg->entrypoint = entrypoint;
  cfg->module = module;

  g_hash_table_add (module->graphs, cfg);

  return cfg;
}

static void
gum_v8_control_flow_graph_value_free (GumV8ControlFlowGraphValue * self)
{
  gum_control_flow_graph_free (self->handle);

  delete self->object;

  g_slice_free (GumV8ControlFlowGraphValue, self);
}

static void
gum_v8_control_flow_graph_value_on_weak_notify (
    const WeakCallbackInfo<GumV8ControlFlowGraphValue> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->graphs, self);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_basic_block_get_start, GumV8BasicBlockValue)
{
  GumAddress start, end;
  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (start), core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_basic_block_get_end, GumV8BasicBlockValue)
{
  GumAddress start, end;
  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (end), core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_basic_block_get_successors,
                           GumV8BasicBlockValue)
{
  auto context = isolate->GetCurrentContext ();
  auto cfg = Local<Object>::New (isolate, *self->cfg);

  const guint * successors;
  guint n = gum_control_flow_graph_get_block_successors (self->handle,
      self->index, &successors);
  auto result = Array::New (isolate);
  for (guint i = 0; i != n; i++)
  {
    result->Set (context, i,
        gum_v8_basic_block_new (cfg, self->handle, successors[i],
            module)).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_basic_block_get_predecessors,
                           GumV8BasicBlockValue)
{
  auto context = isolate->GetCurrentContext ();
  auto cfg = Local<Object>::New (isolate, *self->cfg);

  const guint * predecessors;
  guint n = gum_control_flow_graph_get_block_predecessors (self->handle,
      self->index, &predecessors);
  auto result = Array::New (isolate);
  for (guint i = 0; i != n; i++)
  {
    result->Set (context, i,
        gum_v8_basic_block_new (cfg, self->handle, predecessors[i],
            module)).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_basic_block_get_immediate_dominator,
                           GumV8BasicBlockValue)
{
  guint index = gum_control_flow_graph_get_block_immediate_dominator (
      self->handle, self->index);
  if (index == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
  {
    info.GetReturnValue ().SetNull ();
    return;
  }

  auto cfg = Local<Object>::New (isolate, *self->cfg);
  info.GetReturnValue ().Set (
      gum_v8_basic_block_new (cfg, self->handle, index, module));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_basic_block_get_instructions,
                           GumV8BasicBlockValue)
{
  auto context = isolate->GetCurrentContext ();

  GumAddress start, end;
  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  auto result = Array::New (isolate);
  guint i = 0;
  for (GumAddress address = start; address != end; )
  {
    auto insn = gum_control_flow_graph_find_instruction_containing (
        self->handle, GSIZE_TO_POINTER (address));

    result->Set (context, i++,
        _gum_v8_instruction_new (module->instruction->capstone, insn, FALSE,
            GSIZE_TO_POINTER (address), module->instruction)).Check ();

    address += insn->size;
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_basic_block_to_json)
{
  auto self = (GumV8BasicBlockValue *)
      info.This ()->GetAlignedPointerFromInternalField (0);
  auto context = isolate->GetCurrentContext ();

  GumAddress start, end;
  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  auto result = Object::New (isolate);

  result->Set (context, _gum_v8_string_new_ascii (isolate, "start"),
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (start), core)).Check ();
  result->Set (context, _gum_v8_string_new_ascii (isolate, "end"),
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (end), core)).Check ();

  const guint * neighbors;

  guint n = gum_control_flow_graph_get_block_successors (self->handle,
      self->index, &neighbors);
  auto successors = Array::New (isolate);
  for (guint i = 0; i != n; i++)
  {
    successors->Set (context, i,
        gum_v8_basic_block_start_new (self->handle, neighbors[i], core))
        .Check ();
  }
  result->Set (context, _gum_v8_string_new_ascii (isolate, "successors"),
      successors).Check ();

  n = gum_control_flow_graph_get_block_predecessors (self->handle, self->index,
      &neighbors);
  auto predecessors = Array::New (isolate);
  for (guint i = 0; i != n; i++)
  {
    predecessors->Set (context, i,
        gum_v8_basic_block_start_new (self->handle, neighbors[i], core))
        .Check ();
  }
  result->Set (context, _gum_v8_string_new_ascii (isolate, "predecessors"),
      predecessors).Check ();

  guint dominator = gum_control_flow_graph_get_block_immediate_dominator (
      self->handle, self->index);
  Local<Value> immediate_dominator;
  if (dominator == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
    immediate_dominator = Null (isolate);
  else
    immediate_dominator = gum_v8_basic_block_start_new (self->handle, dominator,
        core);
  result->Set (context,
      _gum_v8_string_new_ascii (isolate, "immediateDominator"),
      immediate_dominator).Check ();

  auto instructions = info.This ()->Get (context,
      _gum_v8_string_new_ascii (isolate, "instructions")).ToLocalChecked ();
  result->Set (context, _gum_v8_string_new_ascii (isolate, "instructions"),
      instructions).Check ();

  info.GetReturnValue ().Set (result);
}

static Local<Value>
gum_v8_basic_block_start_new (GumControlFlowGraph * handle,
                              guint index,
                              GumV8Core * core)
{
  GumAddress start, end;

  gum_control_flow_graph_get_block_bounds (handle, index, &start, &end);

  return _gum_v8_native_pointer_new (GSIZE_TO_POINTER (start), core);
}

static Local<Object>
gum_v8_basic_block_new (Local<Object> cfg,
                        GumControlFlowGraph * handle,
                        guint index,
                        GumV8ControlFlowGraph * module)
{
  auto isolate = module->core->isolate;

  auto value = g_slice_new (GumV8BasicBlockValue);

  auto template_object = Local<Object>::New (isolate,
      *module->basic_block_value);
  auto object = template_object->Clone ();
  value->object = new Global<Object> (isolate, object);
  value->object->SetWeak (value, gum_v8_basic_block_value_on_weak_notify,
      WeakCallbackType::kParameter);
  object->SetAlignedPointerInInternalField (0, value);

  value->cfg = new Global<Object> (isolate, cfg);
  value->handle = handle;
  value->index = index;
  value->module = module;

  g_hash_table_add (module->blocks, value);

  return object;
}

static void
gum_v8_basic_block_value_free (GumV8BasicBlockValue * self)
{
  delete self->cfg;
  delete self->object;

  g_slice_free (GumV8BasicBlockValue, self);
}

static void
gum_v8_basic_block_value_on_weak_notify (
    const WeakCallbackInfo<GumV8BasicBlockValue> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->blocks, self);
}

static Local<Object>
gum_v8_properties_to_json (Local<Object> object,
                           const GumV8Property * properties,
                           Isolate * isolate)
{
  auto context = isolate->GetCurrentContext ();

  auto result = Object::New (isolate);
  for (const GumV8Property * p = properties; p->name != NULL; p++)
  {
    auto name = _gum_v8_string_new_ascii (isolate, p->name);
    auto value = object->Get (context, name).ToLocalChecked ();
    result->Set (context, name, value).Check ();
  }

  return result;
}
