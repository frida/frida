/*
 * Copyright (C) 2020-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickstalker.h"

#include "gumquickeventsink.h"
#include "gumquickmacros.h"

#include <string.h>
#include <glib/gprintf.h>

#define GUM_QUICK_TYPE_TRANSFORMER (gum_quick_transformer_get_type ())
#define GUM_QUICK_TRANSFORMER_CAST(obj) ((GumQuickTransformer *) (obj))

typedef struct _GumQuickTransformer GumQuickTransformer;
typedef struct _GumQuickTransformerClass GumQuickTransformerClass;
typedef struct _GumQuickIterator GumQuickIterator;
typedef struct _GumQuickCallout GumQuickCallout;
typedef struct _GumQuickCallProbe GumQuickCallProbe;

struct _GumQuickTransformer
{
  GObject object;

  GumThreadId thread_id;
  JSValue callback;

  GumQuickStalker * parent;
};

struct _GumQuickTransformerClass
{
  GObjectClass object_class;
};

struct _GumQuickIterator
{
  GumStalkerIterator * handle;
  GumQuickInstructionValue * instruction;

  GumQuickStalker * parent;
};

struct _GumQuickDefaultIterator
{
  GumQuickDefaultWriter writer;
  GumQuickIterator iterator;
};

struct _GumQuickSpecialIterator
{
  GumQuickSpecialWriter writer;
  GumQuickIterator iterator;
};

struct _GumQuickCallout
{
  JSValue callback;

  GumQuickStalker * parent;
};

struct _GumQuickCallProbe
{
  JSValue callback;

  GumQuickStalker * parent;
};

struct _GumQuickProbeArgs
{
  JSValue wrapper;
  GumCallDetails * call;
};

static gboolean gum_quick_stalker_on_flush_timer_tick (GumQuickStalker * self);

GUMJS_DECLARE_GETTER (gumjs_stalker_get_trust_threshold)
GUMJS_DECLARE_SETTER (gumjs_stalker_set_trust_threshold)

GUMJS_DECLARE_GETTER (gumjs_stalker_get_queue_capacity)
GUMJS_DECLARE_SETTER (gumjs_stalker_set_queue_capacity)

GUMJS_DECLARE_GETTER (gumjs_stalker_get_queue_drain_interval)
GUMJS_DECLARE_SETTER (gumjs_stalker_set_queue_drain_interval)

GUMJS_DECLARE_FUNCTION (gumjs_stalker_flush)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_garbage_collect)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_exclude)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_follow)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_unfollow)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_invalidate)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_add_call_probe)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_remove_call_probe)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_parse)

static void gum_quick_transformer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_quick_transformer_dispose (GObject * object);
G_DEFINE_TYPE_EXTENDED (GumQuickTransformer,
                        gum_quick_transformer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_STALKER_TRANSFORMER,
                            gum_quick_transformer_iface_init))

static JSValue gum_quick_default_iterator_new (GumQuickStalker * parent,
    GumQuickDefaultIterator ** iterator);
static void gum_quick_default_iterator_reset (GumQuickDefaultIterator * self,
    GumStalkerIterator * handle, GumStalkerOutput * output);
GUMJS_DECLARE_FINALIZER (gumjs_default_iterator_finalize)
GUMJS_DECLARE_GETTER (gumjs_default_iterator_get_memory_access)
GUMJS_DECLARE_FUNCTION (gumjs_default_iterator_next)
GUMJS_DECLARE_FUNCTION (gumjs_default_iterator_keep)
GUMJS_DECLARE_FUNCTION (gumjs_default_iterator_put_callout)
GUMJS_DECLARE_FUNCTION (gumjs_default_iterator_put_chaining_return)

static JSValue gum_quick_special_iterator_new (GumQuickStalker * parent,
    GumQuickSpecialIterator ** iterator);
static void gum_quick_special_iterator_reset (GumQuickSpecialIterator * self,
    GumStalkerIterator * handle, GumStalkerOutput * output);
GUMJS_DECLARE_FINALIZER (gumjs_special_iterator_finalize)
GUMJS_DECLARE_GETTER (gumjs_special_iterator_get_memory_access)
GUMJS_DECLARE_FUNCTION (gumjs_special_iterator_next)
GUMJS_DECLARE_FUNCTION (gumjs_special_iterator_keep)
GUMJS_DECLARE_FUNCTION (gumjs_special_iterator_put_callout)
GUMJS_DECLARE_FUNCTION (gumjs_special_iterator_put_chaining_return)

static void gum_quick_callout_free (GumQuickCallout * callout);
static void gum_quick_callout_on_invoke (GumCpuContext * cpu_context,
    GumQuickCallout * self);

static void gum_quick_call_probe_free (GumQuickCallProbe * probe);
static void gum_quick_call_probe_on_fire (GumCallDetails * details,
    GumQuickCallProbe * self);

static JSValue gum_quick_probe_args_new (GumQuickStalker * parent,
    GumQuickProbeArgs ** probe_args);
static void gum_quick_probe_args_reset (GumQuickProbeArgs * self,
    GumCallDetails * call);
GUMJS_DECLARE_FINALIZER (gumjs_probe_args_finalize)
static JSValue gumjs_probe_args_get_property (JSContext * ctx, JSValueConst obj,
    JSAtom atom, JSValueConst receiver);
static int gumjs_probe_args_set_property (JSContext * ctx, JSValueConst obj,
    JSAtom atom, JSValueConst value, JSValueConst receiver, int flags);

static GumQuickDefaultIterator * gum_quick_stalker_obtain_default_iterator (
    GumQuickStalker * self);
static void gum_quick_stalker_release_default_iterator (GumQuickStalker * self,
    GumQuickDefaultIterator * iterator);
static GumQuickSpecialIterator * gum_quick_stalker_obtain_special_iterator (
    GumQuickStalker * self);
static void gum_quick_stalker_release_special_iterator (GumQuickStalker * self,
    GumQuickSpecialIterator * iterator);
static GumQuickInstructionValue * gum_quick_stalker_obtain_instruction (
    GumQuickStalker * self);
static void gum_quick_stalker_release_instruction (GumQuickStalker * self,
    GumQuickInstructionValue * value);
static GumQuickCpuContext * gum_quick_stalker_obtain_cpu_context (
    GumQuickStalker * self);
static void gum_quick_stalker_release_cpu_context (GumQuickStalker * self,
    GumQuickCpuContext * cpu_context);
static GumQuickProbeArgs * gum_quick_stalker_obtain_probe_args (
    GumQuickStalker * self);
static void gum_quick_stalker_release_probe_args (GumQuickStalker * self,
    GumQuickProbeArgs * args);

static JSValue gum_encode_pointer (JSContext * ctx, gpointer value,
    gboolean stringify, GumQuickCore * core);

static const JSCFunctionListEntry gumjs_stalker_entries[] =
{
  JS_CGETSET_DEF ("trustThreshold", gumjs_stalker_get_trust_threshold,
      gumjs_stalker_set_trust_threshold),
  JS_CGETSET_DEF ("queueCapacity", gumjs_stalker_get_queue_capacity,
      gumjs_stalker_set_queue_capacity),
  JS_CGETSET_DEF ("queueDrainInterval", gumjs_stalker_get_queue_drain_interval,
      gumjs_stalker_set_queue_drain_interval),
  JS_CFUNC_DEF ("flush", 0, gumjs_stalker_flush),
  JS_CFUNC_DEF ("garbageCollect", 0, gumjs_stalker_garbage_collect),
  JS_CFUNC_DEF ("_exclude", 0, gumjs_stalker_exclude),
  JS_CFUNC_DEF ("_follow", 0, gumjs_stalker_follow),
  JS_CFUNC_DEF ("unfollow", 0, gumjs_stalker_unfollow),
  JS_CFUNC_DEF ("invalidate", 0, gumjs_stalker_invalidate),
  JS_CFUNC_DEF ("addCallProbe", 0, gumjs_stalker_add_call_probe),
  JS_CFUNC_DEF ("removeCallProbe", 0, gumjs_stalker_remove_call_probe),
  JS_CFUNC_DEF ("_parse", 0, gumjs_stalker_parse),
};

static const JSClassDef gumjs_default_iterator_def =
{
  .class_name = "DefaultIterator",
  .finalizer = gumjs_default_iterator_finalize,
};

static const JSCFunctionListEntry gumjs_default_iterator_entries[] =
{
  JS_CGETSET_DEF ("memoryAccess", gumjs_default_iterator_get_memory_access,
      NULL),
  JS_CFUNC_DEF ("next", 0, gumjs_default_iterator_next),
  JS_CFUNC_DEF ("keep", 0, gumjs_default_iterator_keep),
  JS_CFUNC_DEF ("putCallout", 0, gumjs_default_iterator_put_callout),
  JS_CFUNC_DEF ("putChainingReturn", 0,
      gumjs_default_iterator_put_chaining_return),
};

static const JSClassDef gumjs_special_iterator_def =
{
  .class_name = "SpecialIterator",
  .finalizer = gumjs_special_iterator_finalize,
};

static const JSCFunctionListEntry gumjs_special_iterator_entries[] =
{
  JS_CGETSET_DEF ("memoryAccess", gumjs_special_iterator_get_memory_access,
      NULL),
  JS_CFUNC_DEF ("next", 0, gumjs_special_iterator_next),
  JS_CFUNC_DEF ("keep", 0, gumjs_special_iterator_keep),
  JS_CFUNC_DEF ("putCallout", 0, gumjs_special_iterator_put_callout),
  JS_CFUNC_DEF ("putChainingReturn", 0,
      gumjs_special_iterator_put_chaining_return),
};

static const JSClassExoticMethods gumjs_probe_args_exotic_methods =
{
  .get_property = gumjs_probe_args_get_property,
  .set_property = gumjs_probe_args_set_property,
};

static const JSClassDef gumjs_probe_args_def =
{
  .class_name = "ProbeArguments",
  .finalizer = gumjs_probe_args_finalize,
  .exotic = (JSClassExoticMethods *) &gumjs_probe_args_exotic_methods,
};

void
_gum_quick_stalker_init (GumQuickStalker * self,
                         JSValue ns,
                         GumQuickCodeWriter * writer,
                         GumQuickInstruction * instruction,
                         GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj, proto;

  self->writer = writer;
  self->instruction = instruction;
  self->core = core;

  self->stalker = NULL;
  self->queue_capacity = 16384;
  self->queue_drain_interval = 250;

  self->flush_timer = NULL;

  _gum_quick_core_store_module_data (core, "stalker", self);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_stalker_entries,
      G_N_ELEMENTS (gumjs_stalker_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Stalker", obj, JS_PROP_C_W_E);

  _gum_quick_create_subclass (ctx, &gumjs_default_iterator_def,
      writer->G_PASTE (GUM_QUICK_DEFAULT_WRITER_FIELD, _class),
      writer->G_PASTE (GUM_QUICK_DEFAULT_WRITER_FIELD, _proto), core,
      &self->default_iterator_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto,
      gumjs_default_iterator_entries,
      G_N_ELEMENTS (gumjs_default_iterator_entries));

  _gum_quick_create_subclass (ctx, &gumjs_special_iterator_def,
      writer->G_PASTE (GUM_QUICK_SPECIAL_WRITER_FIELD, _class),
      writer->G_PASTE (GUM_QUICK_SPECIAL_WRITER_FIELD, _proto), core,
      &self->special_iterator_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto,
      gumjs_special_iterator_entries,
      G_N_ELEMENTS (gumjs_special_iterator_entries));

  _gum_quick_create_class (ctx, &gumjs_probe_args_def, core,
      &self->probe_args_class, &proto);

  gum_quick_default_iterator_new (self, &self->cached_default_iterator);
  self->cached_default_iterator_in_use = FALSE;

  gum_quick_special_iterator_new (self, &self->cached_special_iterator);
  self->cached_special_iterator_in_use = FALSE;

  _gum_quick_instruction_new (ctx, NULL, TRUE, NULL, 0, instruction,
      &self->cached_instruction);
  self->cached_instruction_in_use = FALSE;

  _gum_quick_cpu_context_new (ctx, NULL, GUM_CPU_CONTEXT_READWRITE, core,
      &self->cached_cpu_context);
  self->cached_cpu_context_in_use = FALSE;

  gum_quick_probe_args_new (self, &self->cached_probe_args);
  self->cached_probe_args_in_use = FALSE;
}

void
_gum_quick_stalker_flush (GumQuickStalker * self)
{
  GumQuickCore * core = self->core;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  gboolean pending_garbage;

  if (self->stalker == NULL)
    return;

  _gum_quick_scope_suspend (&scope);

  gum_stalker_stop (self->stalker);

  pending_garbage = gum_stalker_garbage_collect (self->stalker);

  _gum_quick_scope_resume (&scope);

  if (pending_garbage)
  {
    if (self->flush_timer == NULL)
    {
      GSource * source;

      source = g_timeout_source_new (10);
      g_source_set_callback (source,
          (GSourceFunc) gum_quick_stalker_on_flush_timer_tick, self, NULL);
      self->flush_timer = source;

      _gum_quick_core_pin (core);
      _gum_quick_scope_suspend (&scope);

      g_source_attach (source,
          gum_script_scheduler_get_js_context (core->scheduler));
      g_source_unref (source);

      _gum_quick_scope_resume (&scope);
    }
  }
  else
  {
    g_object_unref (self->stalker);
    self->stalker = NULL;
  }
}

static gboolean
gum_quick_stalker_on_flush_timer_tick (GumQuickStalker * self)
{
  gboolean pending_garbage;

  pending_garbage = gum_stalker_garbage_collect (self->stalker);
  if (!pending_garbage)
  {
    GumQuickCore * core = self->core;
    GumQuickScope scope;

    _gum_quick_scope_enter (&scope, core);
    _gum_quick_core_unpin (core);
    self->flush_timer = NULL;
    _gum_quick_scope_leave (&scope);
  }

  return pending_garbage;
}

void
_gum_quick_stalker_dispose (GumQuickStalker * self)
{
  JSContext * ctx = self->core->ctx;

  g_assert (self->flush_timer == NULL);

  JS_FreeValue (ctx, self->cached_probe_args->wrapper);
  JS_FreeValue (ctx, self->cached_cpu_context->wrapper);
  JS_FreeValue (ctx, self->cached_instruction->wrapper);
  JS_FreeValue (ctx, self->cached_special_iterator->writer.wrapper);
  JS_FreeValue (ctx, self->cached_default_iterator->writer.wrapper);
}

void
_gum_quick_stalker_finalize (GumQuickStalker * self)
{
}

GumStalker *
_gum_quick_stalker_get (GumQuickStalker * self)
{
  if (self->stalker == NULL)
    self->stalker = gum_stalker_new ();

  return self->stalker;
}

void
_gum_quick_stalker_process_pending (GumQuickStalker * self,
                                    GumQuickScope * scope)
{
  if (scope->pending_stalker_level > 0)
  {
    gum_stalker_follow_me (_gum_quick_stalker_get (self),
        scope->pending_stalker_transformer, scope->pending_stalker_sink);
  }
  else if (scope->pending_stalker_level < 0)
  {
    gum_stalker_unfollow_me (_gum_quick_stalker_get (self));
  }
  scope->pending_stalker_level = 0;

  g_clear_object (&scope->pending_stalker_sink);
  g_clear_object (&scope->pending_stalker_transformer);
}

static GumQuickStalker *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "stalker");
}

GUMJS_DEFINE_GETTER (gumjs_stalker_get_trust_threshold)
{
  GumStalker * stalker =
      _gum_quick_stalker_get (gumjs_get_parent_module (core));

  return JS_NewInt32 (ctx, gum_stalker_get_trust_threshold (stalker));
}

GUMJS_DEFINE_SETTER (gumjs_stalker_set_trust_threshold)
{
  GumStalker * stalker;
  gint threshold;

  stalker = _gum_quick_stalker_get (gumjs_get_parent_module (core));

  if (!_gum_quick_int_get (ctx, val, &threshold))
    return JS_EXCEPTION;

  gum_stalker_set_trust_threshold (stalker, threshold);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_GETTER (gumjs_stalker_get_queue_capacity)
{
  GumQuickStalker * self = gumjs_get_parent_module (core);

  return JS_NewInt32 (ctx, self->queue_capacity);
}

GUMJS_DEFINE_SETTER (gumjs_stalker_set_queue_capacity)
{
  GumQuickStalker * self = gumjs_get_parent_module (core);

  if (!_gum_quick_uint_get (ctx, val, &self->queue_capacity))
    return JS_EXCEPTION;

  return JS_UNDEFINED;
}

GUMJS_DEFINE_GETTER (gumjs_stalker_get_queue_drain_interval)
{
  GumQuickStalker * self = gumjs_get_parent_module (core);

  return JS_NewInt32 (ctx, self->queue_drain_interval);
}

GUMJS_DEFINE_SETTER (gumjs_stalker_set_queue_drain_interval)
{
  GumQuickStalker * self = gumjs_get_parent_module (core);

  if (!_gum_quick_uint_get (ctx, val, &self->queue_drain_interval))
    return JS_EXCEPTION;

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_flush)
{
  GumStalker * stalker =
      _gum_quick_stalker_get (gumjs_get_parent_module (core));

  gum_stalker_flush (stalker);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_garbage_collect)
{
  GumStalker * stalker =
      _gum_quick_stalker_get (gumjs_get_parent_module (core));

  gum_stalker_garbage_collect (stalker);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_exclude)
{
  GumStalker * stalker;
  gpointer base;
  gsize size;
  GumMemoryRange range;

  stalker = _gum_quick_stalker_get (gumjs_get_parent_module (core));

  if (!_gum_quick_args_parse (args, "pZ", &base, &size))
    return JS_EXCEPTION;

  range.base_address = GUM_ADDRESS (base);
  range.size = size;

  gum_stalker_exclude (stalker, &range);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_follow)
{
  GumQuickStalker * parent;
  GumStalker * stalker;
  GumThreadId thread_id;
  JSValue transformer_callback_js;
  GumStalkerTransformerCallback transformer_callback_c;
  GumQuickEventSinkOptions so;
  gpointer user_data;
  GumStalkerTransformer * transformer;
  GumEventSink * sink;

  parent = gumjs_get_parent_module (core);
  stalker = _gum_quick_stalker_get (parent);

  so.core = core;
  so.main_context = gum_script_scheduler_get_js_context (core->scheduler);
  so.queue_capacity = parent->queue_capacity;
  so.queue_drain_interval = parent->queue_drain_interval;

  if (!_gum_quick_args_parse (args, "ZF*?uF?F?pp", &thread_id,
      &transformer_callback_js, &transformer_callback_c, &so.event_mask,
      &so.on_receive, &so.on_call_summary, &so.on_event, &user_data))
    return JS_EXCEPTION;

  so.user_data = user_data;

  if (!JS_IsNull (transformer_callback_js))
  {
    GumQuickTransformer * cbt;

    cbt = g_object_new (GUM_QUICK_TYPE_TRANSFORMER, NULL);
    cbt->thread_id = thread_id;
    cbt->callback = JS_DupValue (ctx, transformer_callback_js);
    cbt->parent = parent;

    transformer = GUM_STALKER_TRANSFORMER (cbt);
  }
  else if (transformer_callback_c != NULL)
  {
    transformer = gum_stalker_transformer_make_from_callback (
        transformer_callback_c, user_data, NULL);
  }
  else
  {
    transformer = NULL;
  }

  sink = gum_quick_event_sink_new (ctx, &so);

  if (thread_id == gum_process_get_current_thread_id ())
  {
    GumQuickScope * scope = core->current_scope;

    scope->pending_stalker_level = 1;

    g_clear_object (&scope->pending_stalker_transformer);
    g_clear_object (&scope->pending_stalker_sink);
    scope->pending_stalker_transformer = transformer;
    scope->pending_stalker_sink = sink;
  }
  else
  {
    gum_stalker_follow (stalker, thread_id, transformer, sink);
    g_object_unref (sink);
    g_clear_object (&transformer);
  }

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_unfollow)
{
  GumQuickStalker * parent;
  GumStalker * stalker;
  GumThreadId current_thread_id, thread_id;

  parent = gumjs_get_parent_module (core);
  stalker = _gum_quick_stalker_get (parent);

  current_thread_id = gum_process_get_current_thread_id ();

  thread_id = current_thread_id;
  if (!_gum_quick_args_parse (args, "|Z", &thread_id))
    return JS_EXCEPTION;

  if (thread_id == current_thread_id)
    parent->core->current_scope->pending_stalker_level--;
  else
    gum_stalker_unfollow (stalker, thread_id);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_invalidate)
{
  GumQuickStalker * parent;
  GumStalker * stalker;
  gconstpointer address;

  parent = gumjs_get_parent_module (core);
  stalker = _gum_quick_stalker_get (parent);

  if (args->count <= 1)
  {
    if (!_gum_quick_args_parse (args, "p", &address))
      return JS_EXCEPTION;

    gum_stalker_invalidate (stalker, address);
  }
  else
  {
    GumThreadId thread_id;
    GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);

    if (!_gum_quick_args_parse (args, "Zp", &thread_id, &address))
      return JS_EXCEPTION;

    _gum_quick_scope_suspend (&scope);

    gum_stalker_invalidate_for_thread (stalker, thread_id, address);

    _gum_quick_scope_resume (&scope);
  }

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_add_call_probe)
{
  GumProbeId id;
  GumQuickStalker * parent;
  GumStalker * stalker;
  gpointer target_address;
  JSValue callback_js;
  GumCallProbeCallback callback_c;
  gpointer user_data;
  GumQuickCallProbe * probe;

  parent = gumjs_get_parent_module (core);
  stalker = _gum_quick_stalker_get (parent);

  user_data = NULL;
  if (!_gum_quick_args_parse (args, "pF*|p", &target_address, &callback_js,
      &callback_c, &user_data))
    return JS_EXCEPTION;

  if (!JS_IsNull (callback_js))
  {
    probe = g_slice_new (GumQuickCallProbe);
    probe->callback = JS_DupValue (ctx, callback_js);
    probe->parent = parent;

    id = gum_stalker_add_call_probe (stalker, target_address,
        (GumCallProbeCallback) gum_quick_call_probe_on_fire, probe,
        (GDestroyNotify) gum_quick_call_probe_free);
  }
  else
  {
    id = gum_stalker_add_call_probe (stalker, target_address, callback_c,
        user_data, NULL);
  }

  return JS_NewInt32 (ctx, id);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_remove_call_probe)
{
  GumQuickStalker * parent;
  GumProbeId id;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "u", &id))
    return JS_EXCEPTION;

  gum_stalker_remove_call_probe (_gum_quick_stalker_get (parent), id);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_parse)
{
  JSValue result = JS_NULL;
  JSValue events_value;
  gboolean annotate, stringify;
  const GumEvent * events;
  size_t size, count, row_index;
  const GumEvent * ev;
  JSValue row = JS_NULL;

  if (!_gum_quick_args_parse (args, "Vtt", &events_value, &annotate,
      &stringify))
    return JS_EXCEPTION;

  events = (const GumEvent *) JS_GetArrayBuffer (ctx, &size, events_value);
  if (events == NULL)
    return JS_EXCEPTION;

  if (size % sizeof (GumEvent) != 0)
    goto invalid_buffer_shape;

  count = size / sizeof (GumEvent);

  result = JS_NewArray (ctx);

  for (ev = events, row_index = 0; row_index != count; ev++, row_index++)
  {
    size_t column_index = 0;

    row = JS_NewArray (ctx);

#define GUM_APPEND_VAL(v) \
    JS_DefinePropertyValueUint32 (ctx, row, (uint32_t) column_index++, v, \
        JS_PROP_C_W_E)
#define GUM_APPEND_STR(s) \
    GUM_APPEND_VAL (JS_NewString (ctx, s))
#define GUM_APPEND_PTR(p) \
    GUM_APPEND_VAL (gum_encode_pointer (ctx, p, stringify, core))
#define GUM_APPEND_INT(v) \
    GUM_APPEND_VAL (JS_NewInt32 (ctx, v))

    switch (ev->type)
    {
      case GUM_CALL:
      {
        const GumCallEvent * call = &ev->call;

        if (annotate)
          GUM_APPEND_STR ("call");
        GUM_APPEND_PTR (call->location);
        GUM_APPEND_PTR (call->target);
        GUM_APPEND_INT (call->depth);

        break;
      }
      case GUM_RET:
      {
        const GumRetEvent * ret = &ev->ret;

        if (annotate)
          GUM_APPEND_STR ("ret");
        GUM_APPEND_PTR (ret->location);
        GUM_APPEND_PTR (ret->target);
        GUM_APPEND_INT (ret->depth);

        break;
      }
      case GUM_EXEC:
      {
        const GumExecEvent * exec = &ev->exec;

        if (annotate)
          GUM_APPEND_STR ("exec");
        GUM_APPEND_PTR (exec->location);

        break;
      }
      case GUM_BLOCK:
      {
        const GumBlockEvent * block = &ev->block;

        if (annotate)
          GUM_APPEND_STR ("block");
        GUM_APPEND_PTR (block->start);
        GUM_APPEND_PTR (block->end);

        break;
      }
      case GUM_COMPILE:
      {
        const GumCompileEvent * compile = &ev->compile;

        if (annotate)
          GUM_APPEND_STR ("compile");
        GUM_APPEND_PTR (compile->start);
        GUM_APPEND_PTR (compile->end);

        break;
      }
      default:
        goto invalid_event_type;
    }

#undef GUM_APPEND_VAL
#undef GUM_APPEND_STR
#undef GUM_APPEND_PTR
#undef GUM_APPEND_INT

    JS_DefinePropertyValueUint32 (ctx, result, (uint32_t) row_index, row,
        JS_PROP_C_W_E);
  }

  return result;

invalid_buffer_shape:
  {
    _gum_quick_throw_literal (ctx, "invalid buffer shape");
    goto propagate_exception;
  }
invalid_event_type:
  {
    _gum_quick_throw_literal (ctx, "invalid event type");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, row);
    JS_FreeValue (ctx, result);

    return JS_EXCEPTION;
  }
}

static void
gum_quick_transformer_transform_block (GumStalkerTransformer * transformer,
                                       GumStalkerIterator * iterator,
                                       GumStalkerOutput * output)
{
  GumQuickTransformer * self = GUM_QUICK_TRANSFORMER_CAST (transformer);
  GumQuickStalker * parent = self->parent;
  gint saved_system_error;
  GumQuickScope scope;
  GumQuickDefaultIterator * default_iter = NULL;
  GumQuickSpecialIterator * special_iter = NULL;
  JSValue iter_val;
  gboolean transform_threw_an_exception;

  saved_system_error = gum_thread_get_system_error ();

  _gum_quick_scope_enter (&scope, parent->core);

  if (output->encoding == GUM_INSTRUCTION_DEFAULT)
  {
    default_iter = gum_quick_stalker_obtain_default_iterator (parent);
    gum_quick_default_iterator_reset (default_iter, iterator, output);
    iter_val = default_iter->writer.wrapper;
  }
  else
  {
    special_iter = gum_quick_stalker_obtain_special_iterator (parent);
    gum_quick_special_iterator_reset (special_iter, iterator, output);
    iter_val = special_iter->writer.wrapper;
  }

  transform_threw_an_exception = !_gum_quick_scope_call_void (&scope,
      self->callback, JS_UNDEFINED, 1, &iter_val);

  if (default_iter != NULL)
  {
    gum_quick_default_iterator_reset (default_iter, NULL, NULL);
    gum_quick_stalker_release_default_iterator (parent, default_iter);
  }
  else
  {
    gum_quick_special_iterator_reset (special_iter, NULL, NULL);
    gum_quick_stalker_release_special_iterator (parent, special_iter);
  }

  _gum_quick_scope_leave (&scope);

  if (transform_threw_an_exception)
    gum_stalker_unfollow (parent->stalker, self->thread_id);

  gum_thread_set_system_error (saved_system_error);
}

static void
gum_quick_transformer_class_init (GumQuickTransformerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_quick_transformer_dispose;
}

static void
gum_quick_transformer_iface_init (gpointer g_iface,
                                  gpointer iface_data)
{
  GumStalkerTransformerInterface * iface = g_iface;

  iface->transform_block = gum_quick_transformer_transform_block;
}

static void
gum_quick_transformer_init (GumQuickTransformer * self)
{
}

static void
gum_quick_transformer_dispose (GObject * object)
{
  GumQuickTransformer * self = GUM_QUICK_TRANSFORMER_CAST (object);
  GumQuickCore * core = self->parent->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, core);

  if (!JS_IsNull (self->callback))
  {
    JS_FreeValue (core->ctx, self->callback);
    self->callback = JS_NULL;
  }

  _gum_quick_scope_leave (&scope);

  G_OBJECT_CLASS (gum_quick_transformer_parent_class)->dispose (object);
}

static void
gum_quick_stalker_iterator_init (GumQuickIterator * iter,
                                 GumQuickStalker * parent)
{
  iter->handle = NULL;
  iter->instruction = NULL;

  iter->parent = parent;
}

static void
gum_quick_stalker_iterator_reset (GumQuickIterator * self,
                                  GumStalkerIterator * handle)
{
  self->handle = handle;

  if (self->instruction != NULL)
  {
    self->instruction->insn = NULL;
    gum_quick_stalker_release_instruction (self->parent, self->instruction);
  }
  self->instruction = (handle != NULL)
      ? gum_quick_stalker_obtain_instruction (self->parent)
      : NULL;
}

static JSValue
gum_quick_stalker_iterator_get_memory_access (GumQuickIterator * self,
                                              JSContext * ctx)
{
  switch (gum_stalker_iterator_get_memory_access (self->handle))
  {
    case GUM_MEMORY_ACCESS_OPEN:
      return JS_NewString (ctx, "open");
    case GUM_MEMORY_ACCESS_EXCLUSIVE:
      return JS_NewString (ctx, "exclusive");
    default:
      g_assert_not_reached ();
  }

  return JS_NULL;
}

static JSValue
gum_quick_stalker_iterator_next (GumQuickIterator * self,
                                 JSContext * ctx)
{
  if (gum_stalker_iterator_next (self->handle, &self->instruction->insn))
    return JS_DupValue (ctx, self->instruction->wrapper);

  return JS_NULL;
}

static JSValue
gum_quick_stalker_iterator_keep (GumQuickIterator * self,
                                 JSContext * ctx)
{
  gum_stalker_iterator_keep (self->handle);

  return JS_UNDEFINED;
}

static JSValue
gum_quick_stalker_iterator_put_callout (GumQuickIterator * self,
                                        JSContext * ctx,
                                        GumQuickArgs * args)
{
  JSValue callback_js;
  GumStalkerCallout callback_c;
  gpointer user_data;

  user_data = NULL;
  if (!_gum_quick_args_parse (args, "F*|p", &callback_js, &callback_c,
      &user_data))
    return JS_EXCEPTION;

  if (!JS_IsNull (callback_js))
  {
    GumQuickCallout * callout;

    callout = g_slice_new (GumQuickCallout);
    callout->callback = JS_DupValue (ctx, callback_js);
    callout->parent = self->parent;

    gum_stalker_iterator_put_callout (self->handle,
        (GumStalkerCallout) gum_quick_callout_on_invoke, callout,
        (GDestroyNotify) gum_quick_callout_free);
  }
  else
  {
    gum_stalker_iterator_put_callout (self->handle, callback_c, user_data,
        NULL);
  }

  return JS_UNDEFINED;
}

static JSValue
gum_quick_stalker_iterator_put_chaining_return (GumQuickIterator * self,
                                                JSContext * ctx)
{
  gum_stalker_iterator_put_chaining_return (self->handle);

  return JS_UNDEFINED;
}

static JSValue
gum_quick_default_iterator_new (GumQuickStalker * parent,
                                GumQuickDefaultIterator ** iterator)
{
  JSValue wrapper;
  JSContext * ctx = parent->core->ctx;
  GumQuickDefaultIterator * iter;
  GumQuickDefaultWriter * writer;

  wrapper = JS_NewObjectClass (ctx, parent->default_iterator_class);

  iter = g_slice_new (GumQuickDefaultIterator);

  writer = &iter->writer;
  _gum_quick_default_writer_init (writer, ctx, parent->writer);
  writer->wrapper = wrapper;

  gum_quick_stalker_iterator_init (&iter->iterator, parent);

  JS_SetOpaque (wrapper, iter);

  *iterator = iter;

  return wrapper;
}

static void
gum_quick_default_iterator_release (GumQuickDefaultIterator * self)
{
  JS_FreeValue (self->writer.ctx, self->writer.wrapper);
}

static void
gum_quick_default_iterator_reset (GumQuickDefaultIterator * self,
                                  GumStalkerIterator * handle,
                                  GumStalkerOutput * output)
{
  _gum_quick_default_writer_reset (&self->writer,
      (output != NULL) ? output->writer.instance : NULL);
  gum_quick_stalker_iterator_reset (&self->iterator, handle);
}

static gboolean
gum_quick_default_iterator_get (JSContext * ctx,
                                JSValueConst val,
                                GumQuickCore * core,
                                GumQuickDefaultIterator ** iterator)
{
  GumQuickDefaultIterator * it;

  if (!_gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->default_iterator_class, core,
      (gpointer *) &it))
    return FALSE;

  if (it->iterator.handle == NULL)
  {
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }

  *iterator = it;
  return TRUE;
}

GUMJS_DEFINE_FINALIZER (gumjs_default_iterator_finalize)
{
  GumQuickDefaultIterator * it;

  it = JS_GetOpaque (val,
      gumjs_get_parent_module (core)->default_iterator_class);
  if (it == NULL)
    return;

  _gum_quick_default_writer_finalize (&it->writer);

  g_slice_free (GumQuickDefaultIterator, it);
}

GUMJS_DEFINE_GETTER (gumjs_default_iterator_get_memory_access)
{
  GumQuickDefaultIterator * self;

  if (!gum_quick_default_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_get_memory_access (&self->iterator, ctx);
}

GUMJS_DEFINE_FUNCTION (gumjs_default_iterator_next)
{
  GumQuickDefaultIterator * self;

  if (!gum_quick_default_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_next (&self->iterator, ctx);
}

GUMJS_DEFINE_FUNCTION (gumjs_default_iterator_keep)
{
  GumQuickDefaultIterator * self;

  if (!gum_quick_default_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_keep (&self->iterator, ctx);
}

GUMJS_DEFINE_FUNCTION (gumjs_default_iterator_put_callout)
{
  GumQuickDefaultIterator * self;

  if (!gum_quick_default_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_put_callout (&self->iterator, ctx, args);
}

GUMJS_DEFINE_FUNCTION (gumjs_default_iterator_put_chaining_return)
{
  GumQuickDefaultIterator * self;

  if (!gum_quick_default_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_put_chaining_return (&self->iterator, ctx);
}

static JSValue
gum_quick_special_iterator_new (GumQuickStalker * parent,
                                GumQuickSpecialIterator ** iterator)
{
  JSValue wrapper;
  JSContext * ctx = parent->core->ctx;
  GumQuickSpecialIterator * iter;
  GumQuickSpecialWriter * writer;

  wrapper = JS_NewObjectClass (ctx, parent->special_iterator_class);

  iter = g_slice_new (GumQuickSpecialIterator);

  writer = &iter->writer;
  _gum_quick_special_writer_init (writer, ctx, parent->writer);
  writer->wrapper = wrapper;

  gum_quick_stalker_iterator_init (&iter->iterator, parent);

  JS_SetOpaque (wrapper, iter);

  *iterator = iter;

  return wrapper;
}

static void
gum_quick_special_iterator_release (GumQuickSpecialIterator * self)
{
  JS_FreeValue (self->writer.ctx, self->writer.wrapper);
}

static void
gum_quick_special_iterator_reset (GumQuickSpecialIterator * self,
                                  GumStalkerIterator * handle,
                                  GumStalkerOutput * output)
{
  _gum_quick_special_writer_reset (&self->writer,
      (output != NULL) ? output->writer.instance : NULL);
  gum_quick_stalker_iterator_reset (&self->iterator, handle);
}

static gboolean
gum_quick_special_iterator_get (JSContext * ctx,
                                JSValueConst val,
                                GumQuickCore * core,
                                GumQuickSpecialIterator ** iterator)
{
  GumQuickSpecialIterator * it;

  if (!_gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->special_iterator_class, core,
      (gpointer *) &it))
    return FALSE;

  if (it->iterator.handle == NULL)
  {
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }

  *iterator = it;
  return TRUE;
}

GUMJS_DEFINE_FINALIZER (gumjs_special_iterator_finalize)
{
  GumQuickSpecialIterator * it;

  it = JS_GetOpaque (val,
      gumjs_get_parent_module (core)->special_iterator_class);
  if (it == NULL)
    return;

  _gum_quick_special_writer_finalize (&it->writer);

  g_slice_free (GumQuickSpecialIterator, it);
}

GUMJS_DEFINE_GETTER (gumjs_special_iterator_get_memory_access)
{
  GumQuickSpecialIterator * self;

  if (!gum_quick_special_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_get_memory_access (&self->iterator, ctx);
}

GUMJS_DEFINE_FUNCTION (gumjs_special_iterator_next)
{
  GumQuickSpecialIterator * self;

  if (!gum_quick_special_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_next (&self->iterator, ctx);
}

GUMJS_DEFINE_FUNCTION (gumjs_special_iterator_keep)
{
  GumQuickSpecialIterator * self;

  if (!gum_quick_special_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_keep (&self->iterator, ctx);
}

GUMJS_DEFINE_FUNCTION (gumjs_special_iterator_put_callout)
{
  GumQuickSpecialIterator * self;

  if (!gum_quick_special_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_put_callout (&self->iterator, ctx, args);
}

GUMJS_DEFINE_FUNCTION (gumjs_special_iterator_put_chaining_return)
{
  GumQuickSpecialIterator * self;

  if (!gum_quick_special_iterator_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return gum_quick_stalker_iterator_put_chaining_return (&self->iterator, ctx);
}

static void
gum_quick_callout_free (GumQuickCallout * callout)
{
  GumQuickCore * core = callout->parent->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, core);

  JS_FreeValue (core->ctx, callout->callback);

  _gum_quick_scope_leave (&scope);

  g_slice_free (GumQuickCallout, callout);
}

static void
gum_quick_callout_on_invoke (GumCpuContext * cpu_context,
                             GumQuickCallout * self)
{
  GumQuickStalker * parent = self->parent;
  gint saved_system_error;
  GumQuickScope scope;
  GumQuickCpuContext * cpu_context_value;

  saved_system_error = gum_thread_get_system_error ();

  _gum_quick_scope_enter (&scope, parent->core);

  cpu_context_value = gum_quick_stalker_obtain_cpu_context (parent);
  _gum_quick_cpu_context_reset (cpu_context_value, cpu_context,
      GUM_CPU_CONTEXT_READWRITE);

  _gum_quick_scope_call_void (&scope, self->callback, JS_UNDEFINED,
      1, &cpu_context_value->wrapper);

  _gum_quick_cpu_context_reset (cpu_context_value, NULL,
      GUM_CPU_CONTEXT_READWRITE);
  gum_quick_stalker_release_cpu_context (parent, cpu_context_value);

  _gum_quick_scope_leave (&scope);

  gum_thread_set_system_error (saved_system_error);
}

static void
gum_quick_call_probe_free (GumQuickCallProbe * probe)
{
  GumQuickCore * core = probe->parent->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, core);

  JS_FreeValue (core->ctx, probe->callback);

  _gum_quick_scope_leave (&scope);

  g_slice_free (GumQuickCallProbe, probe);
}

static void
gum_quick_call_probe_on_fire (GumCallDetails * details,
                              GumQuickCallProbe * self)
{
  GumQuickStalker * parent = self->parent;
  gint saved_system_error;
  GumQuickScope scope;
  GumQuickProbeArgs * args;

  saved_system_error = gum_thread_get_system_error ();

  _gum_quick_scope_enter (&scope, parent->core);

  args = gum_quick_stalker_obtain_probe_args (parent);
  gum_quick_probe_args_reset (args, details);

  _gum_quick_scope_call_void (&scope, self->callback, JS_UNDEFINED,
      1, &args->wrapper);

  gum_quick_probe_args_reset (args, NULL);
  gum_quick_stalker_release_probe_args (parent, args);

  _gum_quick_scope_leave (&scope);

  gum_thread_set_system_error (saved_system_error);
}

static JSValue
gum_quick_probe_args_new (GumQuickStalker * parent,
                          GumQuickProbeArgs ** probe_args)
{
  JSValue wrapper;
  JSContext * ctx = parent->core->ctx;
  GumQuickProbeArgs * args;

  wrapper = JS_NewObjectClass (ctx, parent->probe_args_class);

  args = g_slice_new (GumQuickProbeArgs);
  args->wrapper = wrapper;
  args->call = NULL;

  JS_SetOpaque (wrapper, args);

  *probe_args = args;

  return wrapper;
}

static void
gum_quick_probe_args_reset (GumQuickProbeArgs * self,
                            GumCallDetails * call)
{
  self->call = call;
}

static gboolean
gum_quick_probe_args_get (JSContext * ctx,
                          JSValueConst val,
                          GumQuickCore * core,
                          GumQuickProbeArgs ** probe_args)
{
  GumQuickProbeArgs * args;

  if (!_gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->probe_args_class, core,
      (gpointer *) &args))
    return FALSE;

  if (args->call == NULL)
  {
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }

  *probe_args = args;
  return TRUE;
}

GUMJS_DEFINE_FINALIZER (gumjs_probe_args_finalize)
{
  GumQuickProbeArgs * a;

  a = JS_GetOpaque (val, gumjs_get_parent_module (core)->probe_args_class);
  if (a == NULL)
    return;

  g_slice_free (GumQuickProbeArgs, a);
}

static JSValue
gumjs_probe_args_get_property (JSContext * ctx,
                               JSValueConst obj,
                               JSAtom atom,
                               JSValueConst receiver)
{
  JSValue result;
  const char * prop_name;

  prop_name = JS_AtomToCString (ctx, atom);

  if (strcmp (prop_name, "toJSON") == 0)
  {
    result = JS_NewString (ctx, "probe-args");
  }
  else
  {
    GumQuickCore * core;
    GumQuickProbeArgs * self;
    guint64 n;
    const gchar * end;

    core = JS_GetContextOpaque (ctx);

    if (!gum_quick_probe_args_get (ctx, receiver, core, &self))
      goto propagate_exception;

    n = g_ascii_strtoull (prop_name, (gchar **) &end, 10);
    if (end != prop_name + strlen (prop_name))
      goto invalid_array_index;

    result = _gum_quick_native_pointer_new (ctx,
        gum_cpu_context_get_nth_argument (self->call->cpu_context, n), core);
  }

  JS_FreeCString (ctx, prop_name);

  return result;

invalid_array_index:
  {
    JS_ThrowRangeError (ctx, "invalid array index");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeCString (ctx, prop_name);

    return JS_EXCEPTION;
  }
}

static int
gumjs_probe_args_set_property (JSContext * ctx,
                               JSValueConst obj,
                               JSAtom atom,
                               JSValueConst value,
                               JSValueConst receiver,
                               int flags)
{
  const char * prop_name;
  GumQuickCore * core;
  GumQuickProbeArgs * self;
  guint64 n;
  const gchar * end;
  gpointer v;

  prop_name = JS_AtomToCString (ctx, atom);

  core = JS_GetContextOpaque (ctx);

  if (!gum_quick_probe_args_get (ctx, receiver, core, &self))
    goto propagate_exception;

  n = g_ascii_strtoull (prop_name, (gchar **) &end, 10);
  if (end != prop_name + strlen (prop_name))
    goto invalid_array_index;

  if (!_gum_quick_native_pointer_get (ctx, value, core, &v))
    goto propagate_exception;

  gum_cpu_context_replace_nth_argument (self->call->cpu_context, n, v);

  JS_FreeCString (ctx, prop_name);

  return TRUE;

invalid_array_index:
  {
    JS_ThrowRangeError (ctx, "invalid array index");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeCString (ctx, prop_name);

    return -1;
  }
}

static GumQuickDefaultIterator *
gum_quick_stalker_obtain_default_iterator (GumQuickStalker * self)
{
  GumQuickDefaultIterator * iterator;

  if (!self->cached_default_iterator_in_use)
  {
    iterator = self->cached_default_iterator;
    self->cached_default_iterator_in_use = TRUE;
  }
  else
  {
    gum_quick_default_iterator_new (self, &iterator);
  }

  return iterator;
}

static void
gum_quick_stalker_release_default_iterator (GumQuickStalker * self,
                                            GumQuickDefaultIterator * iterator)
{
  if (iterator == self->cached_default_iterator)
  {
    self->cached_default_iterator_in_use = FALSE;
  }
  else
  {
    gum_quick_default_iterator_release (iterator);
  }
}

static GumQuickSpecialIterator *
gum_quick_stalker_obtain_special_iterator (GumQuickStalker * self)
{
  GumQuickSpecialIterator * iterator;

  if (!self->cached_special_iterator_in_use)
  {
    iterator = self->cached_special_iterator;
    self->cached_special_iterator_in_use = TRUE;
  }
  else
  {
    gum_quick_special_iterator_new (self, &iterator);
  }

  return iterator;
}

static void
gum_quick_stalker_release_special_iterator (GumQuickStalker * self,
                                            GumQuickSpecialIterator * iterator)
{
  if (iterator == self->cached_special_iterator)
  {
    self->cached_special_iterator_in_use = FALSE;
  }
  else
  {
    gum_quick_special_iterator_release (iterator);
  }
}

static GumQuickInstructionValue *
gum_quick_stalker_obtain_instruction (GumQuickStalker * self)
{
  GumQuickInstructionValue * value;

  if (!self->cached_instruction_in_use)
  {
    value = self->cached_instruction;
    self->cached_instruction_in_use = TRUE;
  }
  else
  {
    _gum_quick_instruction_new (self->core->ctx, NULL, TRUE, NULL, 0,
        self->instruction, &value);
  }

  return value;
}

static void
gum_quick_stalker_release_instruction (GumQuickStalker * self,
                                       GumQuickInstructionValue * value)
{
  if (value == self->cached_instruction)
  {
    self->cached_instruction_in_use = FALSE;
  }
  else
  {
    JS_FreeValue (self->core->ctx, value->wrapper);
  }
}

static GumQuickCpuContext *
gum_quick_stalker_obtain_cpu_context (GumQuickStalker * self)
{
  GumQuickCpuContext * cpu_context;

  if (!self->cached_cpu_context_in_use)
  {
    cpu_context = self->cached_cpu_context;
    self->cached_cpu_context_in_use = TRUE;
  }
  else
  {
    GumQuickCore * core = self->core;

    _gum_quick_cpu_context_new (core->ctx, NULL, GUM_CPU_CONTEXT_READWRITE,
        core, &cpu_context);
  }

  return cpu_context;
}

static void
gum_quick_stalker_release_cpu_context (GumQuickStalker * self,
                                       GumQuickCpuContext * cpu_context)
{
  if (cpu_context == self->cached_cpu_context)
  {
    self->cached_cpu_context_in_use = FALSE;
  }
  else
  {
    JS_FreeValue (self->core->ctx, cpu_context->wrapper);
  }
}

static GumQuickProbeArgs *
gum_quick_stalker_obtain_probe_args (GumQuickStalker * self)
{
  GumQuickProbeArgs * args;

  if (!self->cached_probe_args_in_use)
  {
    args = self->cached_probe_args;
    self->cached_probe_args_in_use = TRUE;
  }
  else
  {
    gum_quick_probe_args_new (self, &args);
  }

  return args;
}

static void
gum_quick_stalker_release_probe_args (GumQuickStalker * self,
                                      GumQuickProbeArgs * args)
{
  if (args == self->cached_probe_args)
    self->cached_probe_args_in_use = FALSE;
  else
    JS_FreeValue (self->core->ctx, args->wrapper);
}

static JSValue
gum_encode_pointer (JSContext * ctx,
                    gpointer value,
                    gboolean stringify,
                    GumQuickCore * core)
{
  if (stringify)
  {
    gchar str[32];

    g_sprintf (str, "0x%" G_GSIZE_MODIFIER "x", GPOINTER_TO_SIZE (value));

    return JS_NewString (ctx, str);
  }
  else
  {
    return _gum_quick_native_pointer_new (ctx, value, core);
  }
}
