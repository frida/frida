/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickcontrolflowgraph.h"

#include "gumquickmacros.h"

#include <gum/gumcontrolflowgraph.h>

typedef struct _GumQuickControlFlowGraphValue GumQuickControlFlowGraphValue;
typedef struct _GumQuickBasicBlockValue GumQuickBasicBlockValue;
typedef struct _GumQuickDominatingSitesContext GumQuickDominatingSitesContext;

struct _GumQuickControlFlowGraphValue
{
  GumControlFlowGraph * handle;
  gpointer entrypoint;
};

struct _GumQuickBasicBlockValue
{
  JSValue cfg;
  GumControlFlowGraph * handle;
  guint index;
};

struct _GumQuickDominatingSitesContext
{
  JSContext * ctx;
  GumQuickCore * core;

  JSValue elements;
  guint n;
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_control_flow_graph_construct)
GUMJS_DECLARE_FINALIZER (gumjs_control_flow_graph_finalize)
GUMJS_DECLARE_GETTER (gumjs_control_flow_graph_get_entrypoint)
GUMJS_DECLARE_GETTER (gumjs_control_flow_graph_get_entry_block)
GUMJS_DECLARE_GETTER (gumjs_control_flow_graph_get_blocks)
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_find_block_containing)
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_dominates)
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_enumerate_dominating_sites)
static gboolean gum_quick_collect_dominating_site (gconstpointer site,
    gsize capacity, GumQuickDominatingSitesContext * dc);
GUMJS_DECLARE_FUNCTION (gumjs_control_flow_graph_to_json)

static JSValue gum_quick_basic_block_new (JSContext * ctx,
    GumQuickControlFlowGraph * parent, JSValue cfg,
    GumControlFlowGraph * handle, guint index);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_basic_block_construct)
GUMJS_DECLARE_FINALIZER (gumjs_basic_block_finalize)
GUMJS_DECLARE_GC_MARKER (gumjs_basic_block_gc_mark)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_start)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_end)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_successors)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_predecessors)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_immediate_dominator)
GUMJS_DECLARE_GETTER (gumjs_basic_block_get_instructions)
GUMJS_DECLARE_FUNCTION (gumjs_basic_block_to_json)
static JSValue gum_quick_basic_block_start_new (JSContext * ctx,
    GumControlFlowGraph * handle, guint index, GumQuickCore * core);

static JSValue gum_quick_properties_to_json (JSContext * ctx, JSValue obj,
    const JSCFunctionListEntry * entries, guint n);

static const JSClassDef gumjs_control_flow_graph_def =
{
  .class_name = "ControlFlowGraph",
  .finalizer = gumjs_control_flow_graph_finalize,
};

static const JSCFunctionListEntry gumjs_control_flow_graph_entries[] =
{
  JS_CGETSET_DEF ("entrypoint", gumjs_control_flow_graph_get_entrypoint, NULL),
  JS_CGETSET_DEF ("entryBlock", gumjs_control_flow_graph_get_entry_block, NULL),
  JS_CGETSET_DEF ("blocks", gumjs_control_flow_graph_get_blocks, NULL),
  JS_CFUNC_DEF ("findBlockContaining", 0,
      gumjs_control_flow_graph_find_block_containing),
  JS_CFUNC_DEF ("dominates", 0, gumjs_control_flow_graph_dominates),
  JS_CFUNC_DEF ("enumerateDominatingSites", 0,
      gumjs_control_flow_graph_enumerate_dominating_sites),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_control_flow_graph_to_json),
};

static const JSClassDef gumjs_basic_block_def =
{
  .class_name = "BasicBlock",
  .finalizer = gumjs_basic_block_finalize,
  .gc_mark = gumjs_basic_block_gc_mark,
};

static const JSCFunctionListEntry gumjs_basic_block_entries[] =
{
  JS_CGETSET_DEF ("start", gumjs_basic_block_get_start, NULL),
  JS_CGETSET_DEF ("end", gumjs_basic_block_get_end, NULL),
  JS_CGETSET_DEF ("successors", gumjs_basic_block_get_successors, NULL),
  JS_CGETSET_DEF ("predecessors", gumjs_basic_block_get_predecessors, NULL),
  JS_CGETSET_DEF ("immediateDominator",
      gumjs_basic_block_get_immediate_dominator, NULL),
  JS_CGETSET_DEF ("instructions", gumjs_basic_block_get_instructions, NULL),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_basic_block_to_json),
};

void
_gum_quick_control_flow_graph_init (GumQuickControlFlowGraph * self,
                                    JSValue ns,
                                    GumQuickInstruction * instruction,
                                    GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;
  self->instruction = instruction;

  _gum_quick_core_store_module_data (core, "control-flow-graph", self);

  _gum_quick_create_class (ctx, &gumjs_control_flow_graph_def, core,
      &self->control_flow_graph_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_control_flow_graph_construct,
      gumjs_control_flow_graph_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_control_flow_graph_entries,
      G_N_ELEMENTS (gumjs_control_flow_graph_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_control_flow_graph_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_basic_block_def, core,
      &self->basic_block_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_basic_block_construct,
      gumjs_basic_block_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_basic_block_entries,
      G_N_ELEMENTS (gumjs_basic_block_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_basic_block_def.class_name,
      ctor, JS_PROP_C_W_E);
}

void
_gum_quick_control_flow_graph_dispose (GumQuickControlFlowGraph * self)
{
}

void
_gum_quick_control_flow_graph_finalize (GumQuickControlFlowGraph * self)
{
}

static GumQuickControlFlowGraph *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "control-flow-graph");
}

static gboolean
gum_quick_control_flow_graph_get (JSContext * ctx,
                                  JSValue val,
                                  GumQuickControlFlowGraph * parent,
                                  GumQuickControlFlowGraphValue ** cfg)
{
  return _gum_quick_unwrap (ctx, val, parent->control_flow_graph_class,
      parent->core, (gpointer *) cfg);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_control_flow_graph_construct)
{
  JSValue wrapper;
  GumQuickControlFlowGraph * parent;
  gpointer entrypoint;
  GumControlFlowGraph * handle;
  JSValue proto;
  GumQuickControlFlowGraphValue * v;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "p", &entrypoint))
    return JS_EXCEPTION;

  handle = gum_control_flow_graph_new_for_function (entrypoint);
  if (gum_control_flow_graph_get_num_blocks (handle) == 0)
  {
    gum_control_flow_graph_free (handle);
    return _gum_quick_throw_literal (ctx,
        "unable to determine the bounds of the function");
  }

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto,
      parent->control_flow_graph_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
  {
    gum_control_flow_graph_free (handle);
    return JS_EXCEPTION;
  }

  v = g_slice_new (GumQuickControlFlowGraphValue);
  v->handle = handle;
  v->entrypoint = entrypoint;
  JS_SetOpaque (wrapper, v);

  return wrapper;
}

GUMJS_DEFINE_FINALIZER (gumjs_control_flow_graph_finalize)
{
  GumQuickControlFlowGraphValue * v;

  v = JS_GetOpaque (val,
      gumjs_get_parent_module (core)->control_flow_graph_class);
  if (v == NULL)
    return;

  gum_control_flow_graph_free (v->handle);

  g_slice_free (GumQuickControlFlowGraphValue, v);
}

GUMJS_DEFINE_GETTER (gumjs_control_flow_graph_get_entrypoint)
{
  GumQuickControlFlowGraph * parent;
  GumQuickControlFlowGraphValue * self;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_control_flow_graph_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, self->entrypoint, core);
}

GUMJS_DEFINE_GETTER (gumjs_control_flow_graph_get_entry_block)
{
  GumQuickControlFlowGraph * parent;
  GumQuickControlFlowGraphValue * self;
  guint index;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_control_flow_graph_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  index = gum_control_flow_graph_get_entry_block (self->handle);
  if (index == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
    return JS_NULL;

  return gum_quick_basic_block_new (ctx, parent, this_val, self->handle, index);
}

GUMJS_DEFINE_GETTER (gumjs_control_flow_graph_get_blocks)
{
  JSValue result;
  GumQuickControlFlowGraph * parent;
  GumQuickControlFlowGraphValue * self;
  guint n, i;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_control_flow_graph_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  result = JS_NewArray (ctx);
  n = gum_control_flow_graph_get_num_blocks (self->handle);
  for (i = 0; i != n; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, result, i,
        gum_quick_basic_block_new (ctx, parent, this_val, self->handle, i),
        JS_PROP_C_W_E);
  }

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_control_flow_graph_find_block_containing)
{
  GumQuickControlFlowGraph * parent;
  GumQuickControlFlowGraphValue * self;
  gpointer address;
  guint index;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_control_flow_graph_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "p", &address))
    return JS_EXCEPTION;

  index = gum_control_flow_graph_find_block_containing (self->handle, address);
  if (index == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
    return JS_NULL;

  return gum_quick_basic_block_new (ctx, parent, this_val, self->handle, index);
}

GUMJS_DEFINE_FUNCTION (gumjs_control_flow_graph_dominates)
{
  GumQuickControlFlowGraph * parent;
  GumQuickControlFlowGraphValue * self;
  gpointer a, b;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_control_flow_graph_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "pp", &a, &b))
    return JS_EXCEPTION;

  return JS_NewBool (ctx,
      gum_control_flow_graph_dominates (self->handle, a, b));
}

GUMJS_DEFINE_FUNCTION (gumjs_control_flow_graph_enumerate_dominating_sites)
{
  GumQuickControlFlowGraph * parent;
  GumQuickControlFlowGraphValue * self;
  gpointer target;
  GumQuickDominatingSitesContext dc;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_control_flow_graph_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "p", &target))
    return JS_EXCEPTION;

  dc.ctx = ctx;
  dc.core = core;
  dc.elements = JS_NewArray (ctx);
  dc.n = 0;

  gum_control_flow_graph_enumerate_dominating_sites (self->handle, target,
      (GumFoundDominatingSiteFunc) gum_quick_collect_dominating_site, &dc);

  return dc.elements;
}

static gboolean
gum_quick_collect_dominating_site (gconstpointer site,
                                   gsize capacity,
                                   GumQuickDominatingSitesContext * dc)
{
  JSContext * ctx = dc->ctx;
  GumQuickCore * core = dc->core;
  JSValue element;

  element = JS_NewObject (ctx);
  JS_DefinePropertyValue (ctx, element, GUM_QUICK_CORE_ATOM (core, address),
      _gum_quick_native_pointer_new (ctx, (gpointer) site, core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValueStr (ctx, element, "capacity",
      JS_NewInt64 (ctx, capacity), JS_PROP_C_W_E);

  JS_DefinePropertyValueUint32 (ctx, dc->elements, dc->n++, element,
      JS_PROP_C_W_E);

  return TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_control_flow_graph_to_json)
{
  return gum_quick_properties_to_json (ctx, this_val,
      gumjs_control_flow_graph_entries,
      G_N_ELEMENTS (gumjs_control_flow_graph_entries));
}

static JSValue
gum_quick_basic_block_new (JSContext * ctx,
                           GumQuickControlFlowGraph * parent,
                           JSValue cfg,
                           GumControlFlowGraph * handle,
                           guint index)
{
  JSValue wrapper;
  GumQuickBasicBlockValue * v;

  wrapper = JS_NewObjectClass (ctx, parent->basic_block_class);

  v = g_slice_new (GumQuickBasicBlockValue);
  v->cfg = JS_DupValue (ctx, cfg);
  v->handle = handle;
  v->index = index;
  JS_SetOpaque (wrapper, v);

  return wrapper;
}

static gboolean
gum_quick_basic_block_get (JSContext * ctx,
                           JSValue val,
                           GumQuickControlFlowGraph * parent,
                           GumQuickBasicBlockValue ** block)
{
  return _gum_quick_unwrap (ctx, val, parent->basic_block_class, parent->core,
      (gpointer *) block);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_basic_block_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FINALIZER (gumjs_basic_block_finalize)
{
  GumQuickBasicBlockValue * v;

  v = JS_GetOpaque (val, gumjs_get_parent_module (core)->basic_block_class);
  if (v == NULL)
    return;

  JS_FreeValue (core->ctx, v->cfg);

  g_slice_free (GumQuickBasicBlockValue, v);
}

GUMJS_DEFINE_GC_MARKER (gumjs_basic_block_gc_mark)
{
  GumQuickBasicBlockValue * v;

  v = JS_GetOpaque (val, gumjs_get_parent_module (core)->basic_block_class);
  if (v == NULL)
    return;

  JS_MarkValue (rt, v->cfg, mark_func);
}

GUMJS_DEFINE_GETTER (gumjs_basic_block_get_start)
{
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  GumAddress start, end;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  return _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (start), core);
}

GUMJS_DEFINE_GETTER (gumjs_basic_block_get_end)
{
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  GumAddress start, end;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  return _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (end), core);
}

GUMJS_DEFINE_GETTER (gumjs_basic_block_get_successors)
{
  JSValue result;
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  const guint * successors;
  guint n, i;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  result = JS_NewArray (ctx);
  n = gum_control_flow_graph_get_block_successors (self->handle, self->index,
      &successors);
  for (i = 0; i != n; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, result, i,
        gum_quick_basic_block_new (ctx, parent, self->cfg, self->handle,
            successors[i]),
        JS_PROP_C_W_E);
  }

  return result;
}

GUMJS_DEFINE_GETTER (gumjs_basic_block_get_predecessors)
{
  JSValue result;
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  const guint * predecessors;
  guint n, i;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  result = JS_NewArray (ctx);
  n = gum_control_flow_graph_get_block_predecessors (self->handle,
      self->index, &predecessors);
  for (i = 0; i != n; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, result, i,
        gum_quick_basic_block_new (ctx, parent, self->cfg, self->handle,
            predecessors[i]),
        JS_PROP_C_W_E);
  }

  return result;
}

GUMJS_DEFINE_GETTER (gumjs_basic_block_get_immediate_dominator)
{
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  guint index;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  index = gum_control_flow_graph_get_block_immediate_dominator (self->handle,
      self->index);
  if (index == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
    return JS_NULL;

  return gum_quick_basic_block_new (ctx, parent, self->cfg, self->handle,
      index);
}

GUMJS_DEFINE_GETTER (gumjs_basic_block_get_instructions)
{
  JSValue result;
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  GumAddress start, end, address;
  guint i;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);

  result = JS_NewArray (ctx);
  i = 0;
  for (address = start; address != end; )
  {
    const cs_insn * insn = gum_control_flow_graph_find_instruction_containing (
        self->handle, GSIZE_TO_POINTER (address));

    JS_DefinePropertyValueUint32 (ctx, result, i++,
        _gum_quick_instruction_new (ctx, insn, FALSE,
            GSIZE_TO_POINTER (address), parent->instruction->capstone,
            parent->instruction, NULL),
        JS_PROP_C_W_E);

    address += insn->size;
  }

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_basic_block_to_json)
{
  GumQuickControlFlowGraph * parent;
  GumQuickBasicBlockValue * self;
  JSValue result, successors, predecessors;
  GumAddress start, end;
  const guint * neighbors;
  guint n, i, dominator;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_basic_block_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  result = JS_NewObject (ctx);

  gum_control_flow_graph_get_block_bounds (self->handle, self->index, &start,
      &end);
  JS_SetPropertyStr (ctx, result, "start",
      _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (start), core));
  JS_SetPropertyStr (ctx, result, "end",
      _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (end), core));

  successors = JS_NewArray (ctx);
  n = gum_control_flow_graph_get_block_successors (self->handle, self->index,
      &neighbors);
  for (i = 0; i != n; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, successors, i,
        gum_quick_basic_block_start_new (ctx, self->handle, neighbors[i], core),
        JS_PROP_C_W_E);
  }
  JS_SetPropertyStr (ctx, result, "successors", successors);

  predecessors = JS_NewArray (ctx);
  n = gum_control_flow_graph_get_block_predecessors (self->handle, self->index,
      &neighbors);
  for (i = 0; i != n; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, predecessors, i,
        gum_quick_basic_block_start_new (ctx, self->handle, neighbors[i], core),
        JS_PROP_C_W_E);
  }
  JS_SetPropertyStr (ctx, result, "predecessors", predecessors);

  dominator = gum_control_flow_graph_get_block_immediate_dominator (
      self->handle, self->index);
  JS_SetPropertyStr (ctx, result, "immediateDominator",
      (dominator == GUM_CONTROL_FLOW_GRAPH_NO_BLOCK)
          ? JS_NULL
          : gum_quick_basic_block_start_new (ctx, self->handle, dominator,
                core));

  JS_SetPropertyStr (ctx, result, "instructions",
      JS_GetPropertyStr (ctx, this_val, "instructions"));

  return result;
}

static JSValue
gum_quick_basic_block_start_new (JSContext * ctx,
                                 GumControlFlowGraph * handle,
                                 guint index,
                                 GumQuickCore * core)
{
  GumAddress start, end;

  gum_control_flow_graph_get_block_bounds (handle, index, &start, &end);

  return _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (start), core);
}

static JSValue
gum_quick_properties_to_json (JSContext * ctx,
                              JSValue obj,
                              const JSCFunctionListEntry * entries,
                              guint n)
{
  JSValue result;
  guint i;

  result = JS_NewObject (ctx);

  for (i = 0; i != n; i++)
  {
    const JSCFunctionListEntry * e = &entries[i];
    JSValue val;

    if (e->def_type != JS_DEF_CGETSET)
      continue;

    val = JS_GetPropertyStr (ctx, obj, e->name);
    if (JS_IsException (val))
      goto propagate_exception;
    JS_SetPropertyStr (ctx, result, e->name, val);
  }

  return result;

propagate_exception:
  {
    JS_FreeValue (ctx, result);

    return JS_EXCEPTION;
  }
}
