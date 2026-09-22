/*
 * Copyright (C) 2010-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8stalker.h"

#include "gumv8eventsink.h"
#include "gumv8macros.h"
#include "gumv8scope.h"

#include <glib/gprintf.h>

#define GUMJS_MODULE_NAME Stalker

#define GUM_V8_TYPE_CALLBACK_TRANSFORMER \
    (gum_v8_callback_transformer_get_type ())
#define GUM_V8_CALLBACK_TRANSFORMER_CAST(obj) \
    ((GumV8CallbackTransformer *) (obj))

using namespace v8;

struct GumV8CallbackTransformer
{
  GObject parent;

  GumThreadId thread_id;
  Global<Function> * callback;

  GumV8Stalker * module;
};

struct GumV8CallbackTransformerClass
{
  GObjectClass parent_class;
};

struct GumV8StalkerIterator
{
  GumStalkerIterator * handle;
  GumV8InstructionValue * instruction;

  GumV8Stalker * module;
};

struct GumV8StalkerDefaultIterator
{
  GumV8DefaultWriter parent;
  GumV8StalkerIterator iterator;
};

struct GumV8StalkerSpecialIterator
{
  GumV8SpecialWriter parent;
  GumV8StalkerIterator iterator;
};

struct GumV8Callout
{
  Global<Function> * callback;

  GumV8Stalker * module;
};

struct GumV8CallProbe
{
  Global<Function> * callback;

  GumV8Stalker * module;
};

static gboolean gum_v8_stalker_on_flush_timer_tick (GumV8Stalker * self);

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

static void gum_v8_callback_transformer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_v8_callback_transformer_dispose (GObject * object);
G_DEFINE_TYPE_EXTENDED (GumV8CallbackTransformer,
                        gum_v8_callback_transformer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_STALKER_TRANSFORMER,
                            gum_v8_callback_transformer_iface_init))

static GumV8StalkerDefaultIterator *
    gum_v8_stalker_default_iterator_new_persistent (GumV8Stalker * parent);
static void gum_v8_stalker_default_iterator_release_persistent (
    GumV8StalkerDefaultIterator * self);
static void gum_v8_stalker_default_iterator_on_weak_notify (
    const WeakCallbackInfo<GumV8StalkerDefaultIterator> & info);
static void gum_v8_stalker_default_iterator_free (
    GumV8StalkerDefaultIterator * self);
static void gum_v8_stalker_default_iterator_reset (
    GumV8StalkerDefaultIterator * self, GumStalkerIterator * handle,
    GumStalkerOutput * output);
GUMJS_DECLARE_GETTER (gumjs_stalker_default_iterator_get_memory_access)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_default_iterator_next)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_default_iterator_keep)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_default_iterator_put_callout)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_default_iterator_put_chaining_return)

static GumV8StalkerSpecialIterator *
    gum_v8_stalker_special_iterator_new_persistent (GumV8Stalker * parent);
static void gum_v8_stalker_special_iterator_release_persistent (
    GumV8StalkerSpecialIterator * self);
static void gum_v8_stalker_special_iterator_on_weak_notify (
    const WeakCallbackInfo<GumV8StalkerSpecialIterator> & info);
static void gum_v8_stalker_special_iterator_free (
    GumV8StalkerSpecialIterator * self);
static void gum_v8_stalker_special_iterator_reset (
    GumV8StalkerSpecialIterator * self, GumStalkerIterator * handle,
    GumStalkerOutput * output);
GUMJS_DECLARE_GETTER (gumjs_stalker_special_iterator_get_memory_access)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_special_iterator_next)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_special_iterator_keep)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_special_iterator_put_callout)
GUMJS_DECLARE_FUNCTION (gumjs_stalker_special_iterator_put_chaining_return)

static void gum_v8_callout_free (GumV8Callout * callout);
static void gum_v8_callout_on_invoke (GumCpuContext * cpu_context,
    GumV8Callout * self);

static void gum_v8_call_probe_free (GumV8CallProbe * probe);
static void gum_v8_call_probe_on_fire (GumCallDetails * details,
    GumV8CallProbe * self);

static void gumjs_probe_args_get_nth (uint32_t index,
    const PropertyCallbackInfo<Value> & info);
static void gumjs_probe_args_set_nth (uint32_t index, Local<Value> value,
    const PropertyCallbackInfo<Value> & info);

static GumV8StalkerDefaultIterator * gum_v8_stalker_obtain_default_iterator (
    GumV8Stalker * self);
static void gum_v8_stalker_release_default_iterator (GumV8Stalker * self,
    GumV8StalkerDefaultIterator * iterator);
static GumV8StalkerSpecialIterator * gum_v8_stalker_obtain_special_iterator (
    GumV8Stalker * self);
static void gum_v8_stalker_release_special_iterator (GumV8Stalker * self,
    GumV8StalkerSpecialIterator * iterator);
static GumV8InstructionValue * gum_v8_stalker_obtain_instruction (
    GumV8Stalker * self);
static void gum_v8_stalker_release_instruction (GumV8Stalker * self,
    GumV8InstructionValue * value);

static Local<Value> gum_make_pointer (gpointer value, gboolean stringify,
    GumV8Core * core);

static const GumV8Property gumjs_stalker_values[] =
{
  {
    "trustThreshold",
    gumjs_stalker_get_trust_threshold,
    gumjs_stalker_set_trust_threshold
  },
  {
    "queueCapacity",
    gumjs_stalker_get_queue_capacity,
    gumjs_stalker_set_queue_capacity
  },
  {
    "queueDrainInterval",
    gumjs_stalker_get_queue_drain_interval,
    gumjs_stalker_set_queue_drain_interval
  },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_stalker_functions[] =
{
  { "flush", gumjs_stalker_flush },
  { "garbageCollect", gumjs_stalker_garbage_collect },
  { "_exclude", gumjs_stalker_exclude },
  { "_follow", gumjs_stalker_follow },
  { "unfollow", gumjs_stalker_unfollow },
  { "invalidate", gumjs_stalker_invalidate },
  { "addCallProbe", gumjs_stalker_add_call_probe },
  { "removeCallProbe", gumjs_stalker_remove_call_probe },
  { "_parse", gumjs_stalker_parse },

  { NULL, NULL }
};

static const GumV8Property gumjs_stalker_default_iterator_values[] =
{
  { "memoryAccess", gumjs_stalker_default_iterator_get_memory_access, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_stalker_default_iterator_functions[] =
{
  { "next", gumjs_stalker_default_iterator_next },
  { "keep", gumjs_stalker_default_iterator_keep },
  { "putCallout", gumjs_stalker_default_iterator_put_callout },
  { "putChainingReturn", gumjs_stalker_default_iterator_put_chaining_return },

  { NULL, NULL }
};

static const GumV8Property gumjs_stalker_special_iterator_values[] =
{
  { "memoryAccess", gumjs_stalker_special_iterator_get_memory_access, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_stalker_special_iterator_functions[] =
{
  { "next", gumjs_stalker_special_iterator_next },
  { "keep", gumjs_stalker_special_iterator_keep },
  { "putCallout", gumjs_stalker_special_iterator_put_callout },
  { "putChainingReturn", gumjs_stalker_special_iterator_put_chaining_return },

  { NULL, NULL }
};

void
_gum_v8_stalker_init (GumV8Stalker * self,
                      GumV8CodeWriter * writer,
                      GumV8Instruction * instruction,
                      GumV8Core * core,
                      Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->writer = writer;
  self->instruction = instruction;
  self->core = core;

  self->stalker = NULL;
  self->queue_capacity = 16384;
  self->queue_drain_interval = 250;

  self->flush_timer = NULL;

  self->default_iterators = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_stalker_default_iterator_free);
  self->special_iterators = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_stalker_special_iterator_free);

  auto module = External::New (isolate, self);

  auto stalker = _gum_v8_create_module ("Stalker", scope, isolate);
  _gum_v8_module_add (module, stalker, gumjs_stalker_values, isolate);
  _gum_v8_module_add (module, stalker, gumjs_stalker_functions, isolate);

  {
    auto iter = _gum_v8_create_class ("StalkerDefaultIterator", nullptr, scope,
        module, isolate);
    auto default_writer = Local<FunctionTemplate>::New (isolate,
        *writer->GUM_V8_DEFAULT_WRITER_FIELD);
    iter->Inherit (default_writer);
    _gum_v8_class_add (iter, gumjs_stalker_default_iterator_values, module,
        isolate);
    _gum_v8_class_add (iter, gumjs_stalker_default_iterator_functions, module,
        isolate);
    iter->InstanceTemplate ()->SetInternalFieldCount (2);
    self->default_iterator = new Global<FunctionTemplate> (isolate, iter);
  }

  {
    auto iter = _gum_v8_create_class ("StalkerSpecialIterator", nullptr, scope,
        module, isolate);
    auto special_writer = Local<FunctionTemplate>::New (isolate,
        *writer->GUM_V8_SPECIAL_WRITER_FIELD);
    iter->Inherit (special_writer);
    _gum_v8_class_add (iter, gumjs_stalker_special_iterator_values, module,
        isolate);
    _gum_v8_class_add (iter, gumjs_stalker_special_iterator_functions, module,
        isolate);
    iter->InstanceTemplate ()->SetInternalFieldCount (2);
    self->special_iterator = new Global<FunctionTemplate> (isolate, iter);
  }
}

void
_gum_v8_stalker_realize (GumV8Stalker * self)
{
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  {
    auto iter = Local<FunctionTemplate>::New (isolate, *self->default_iterator);
    auto iter_value = iter->GetFunction (context).ToLocalChecked ()
        ->NewInstance (context, 0, nullptr).ToLocalChecked ();
    self->default_iterator_value = new Global<Object> (isolate, iter_value);
  }

  {
    auto iter = Local<FunctionTemplate>::New (isolate, *self->special_iterator);
    auto iter_value = iter->GetFunction (context).ToLocalChecked ()
        ->NewInstance (context, 0, nullptr).ToLocalChecked ();
    self->special_iterator_value = new Global<Object> (isolate, iter_value);
  }

  auto args = ObjectTemplate::New (isolate);
  args->SetInternalFieldCount (2);
  args->SetIndexedPropertyHandler (gumjs_probe_args_get_nth,
      gumjs_probe_args_set_nth);
  self->probe_args = new Global<ObjectTemplate> (isolate, args);

  self->cached_default_iterator =
      gum_v8_stalker_default_iterator_new_persistent (self);
  self->cached_default_iterator_in_use = FALSE;

  self->cached_special_iterator =
      gum_v8_stalker_special_iterator_new_persistent (self);
  self->cached_special_iterator_in_use = FALSE;

  self->cached_instruction =
      _gum_v8_instruction_new_persistent (self->instruction);
  self->cached_instruction_in_use = FALSE;
}

void
_gum_v8_stalker_flush (GumV8Stalker * self)
{
  auto core = self->core;
  gboolean pending_garbage;

  if (self->stalker == NULL)
    return;

  {
    ScriptUnlocker unlocker (core);

    gum_stalker_stop (self->stalker);

    pending_garbage = gum_stalker_garbage_collect (self->stalker);
  }

  if (pending_garbage)
  {
    if (self->flush_timer == NULL)
    {
      auto source = g_timeout_source_new (10);
      g_source_set_callback (source,
          (GSourceFunc) gum_v8_stalker_on_flush_timer_tick, self, NULL);
      self->flush_timer = source;

      _gum_v8_core_pin (core);

      {
        ScriptUnlocker unlocker (core);

        g_source_attach (source,
            gum_script_scheduler_get_js_context (core->scheduler));
        g_source_unref (source);
      }
    }
  }
  else
  {
    g_object_unref (self->stalker);
    self->stalker = NULL;
  }
}

static gboolean
gum_v8_stalker_on_flush_timer_tick (GumV8Stalker * self)
{
  gboolean pending_garbage;

  pending_garbage = gum_stalker_garbage_collect (self->stalker);
  if (!pending_garbage)
  {
    GumV8Core * core = self->core;

    ScriptScope scope (core->script);
    _gum_v8_core_unpin (core);
    self->flush_timer = NULL;
  }

  return pending_garbage;
}

void
_gum_v8_stalker_dispose (GumV8Stalker * self)
{
  g_assert (self->flush_timer == NULL);

  _gum_v8_instruction_release_persistent (self->cached_instruction);
  self->cached_instruction = NULL;

  gum_v8_stalker_special_iterator_release_persistent (
      self->cached_special_iterator);
  self->cached_special_iterator = NULL;

  gum_v8_stalker_default_iterator_release_persistent (
      self->cached_default_iterator);
  self->cached_default_iterator = NULL;

  delete self->probe_args;
  self->probe_args = nullptr;

  delete self->special_iterator_value;
  self->special_iterator_value = nullptr;

  delete self->default_iterator_value;
  self->default_iterator_value = nullptr;

  delete self->special_iterator;
  self->special_iterator = nullptr;

  delete self->default_iterator;
  self->default_iterator = nullptr;

  g_hash_table_unref (self->special_iterators);
  self->special_iterators = NULL;

  g_hash_table_unref (self->default_iterators);
  self->default_iterators = NULL;
}

void
_gum_v8_stalker_finalize (GumV8Stalker * self)
{
}

GumStalker *
_gum_v8_stalker_get (GumV8Stalker * self)
{
  if (self->stalker == NULL)
    self->stalker = gum_stalker_new ();

  return self->stalker;
}

void
_gum_v8_stalker_process_pending (GumV8Stalker * self,
                                 ScriptStalkerScope * scope)
{
  if (scope->pending_level > 0)
  {
    gum_stalker_follow_me (_gum_v8_stalker_get (self), scope->transformer,
        scope->sink);
  }
  else if (scope->pending_level < 0)
  {
    gum_stalker_unfollow_me (_gum_v8_stalker_get (self));
  }
  scope->pending_level = 0;

  g_clear_object (&scope->sink);
  g_clear_object (&scope->transformer);
}

GUMJS_DEFINE_GETTER (gumjs_stalker_get_trust_threshold)
{
  auto stalker = _gum_v8_stalker_get (module);

  info.GetReturnValue ().Set (gum_stalker_get_trust_threshold (stalker));
}

GUMJS_DEFINE_SETTER (gumjs_stalker_set_trust_threshold)
{
  auto stalker = _gum_v8_stalker_get (module);

  gint threshold;
  if (!_gum_v8_int_get (value, &threshold, core))
    return;

  gum_stalker_set_trust_threshold (stalker, threshold);
}

GUMJS_DEFINE_GETTER (gumjs_stalker_get_queue_capacity)
{
  info.GetReturnValue ().Set (module->queue_capacity);
}

GUMJS_DEFINE_SETTER (gumjs_stalker_set_queue_capacity)
{
  guint capacity;
  if (!_gum_v8_uint_get (value, &capacity, core))
    return;

  module->queue_capacity = capacity;
}

GUMJS_DEFINE_GETTER (gumjs_stalker_get_queue_drain_interval)
{
  info.GetReturnValue ().Set (module->queue_drain_interval);
}

GUMJS_DEFINE_SETTER (gumjs_stalker_set_queue_drain_interval)
{
  guint interval;
  if (!_gum_v8_uint_get (value, &interval, core))
    return;

  module->queue_drain_interval = interval;
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_flush)
{
  auto stalker = _gum_v8_stalker_get (module);

  gum_stalker_flush (stalker);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_garbage_collect)
{
  auto stalker = _gum_v8_stalker_get (module);

  gum_stalker_garbage_collect (stalker);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_exclude)
{
  auto stalker = _gum_v8_stalker_get (module);

  gpointer base;
  gsize size;
  if (!_gum_v8_args_parse (args, "pZ", &base, &size))
    return;

  GumMemoryRange range;
  range.base_address = GUM_ADDRESS (base);
  range.size = size;

  gum_stalker_exclude (stalker, &range);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_follow)
{
  auto stalker = _gum_v8_stalker_get (module);

  GumThreadId thread_id;

  Local<Function> transformer_callback_js;
  GumStalkerTransformerCallback transformer_callback_c;

  GumV8EventSinkOptions so;
  so.core = core;
  so.main_context = gum_script_scheduler_get_js_context (core->scheduler);
  so.queue_capacity = module->queue_capacity;
  so.queue_drain_interval = module->queue_drain_interval;

  gpointer user_data;

  if (!_gum_v8_args_parse (args, "ZF*?uF?F?pp", &thread_id,
      &transformer_callback_js, &transformer_callback_c,
      &so.event_mask, &so.on_receive, &so.on_call_summary,
      &so.on_event, &user_data))
    return;

  so.user_data = user_data;

  GumStalkerTransformer * transformer = NULL;

  if (!transformer_callback_js.IsEmpty ())
  {
    auto cbt = (GumV8CallbackTransformer *)
        g_object_new (GUM_V8_TYPE_CALLBACK_TRANSFORMER, NULL);
    cbt->thread_id = thread_id;
    cbt->callback = new Global<Function> (isolate, transformer_callback_js);
    cbt->module = module;

    transformer = GUM_STALKER_TRANSFORMER (cbt);
  }
  else if (transformer_callback_c != NULL)
  {
    transformer = gum_stalker_transformer_make_from_callback (
        transformer_callback_c, user_data, NULL);
  }

  auto sink = gum_v8_event_sink_new (&so);
  if (thread_id == gum_process_get_current_thread_id ())
  {
    ScriptStalkerScope * scope = &core->current_scope->stalker_scope;

    scope->pending_level = 1;

    g_clear_object (&scope->transformer);
    g_clear_object (&scope->sink);
    scope->transformer = transformer;
    scope->sink = sink;
  }
  else
  {
    gum_stalker_follow (stalker, thread_id, transformer, sink);
    g_object_unref (sink);
    g_clear_object (&transformer);
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_unfollow)
{
  GumStalker * stalker;
  GumThreadId current_thread_id = gum_process_get_current_thread_id ();

  stalker = _gum_v8_stalker_get (module);

  GumThreadId thread_id = current_thread_id;
  if (!_gum_v8_args_parse (args, "|Z", &thread_id))
    return;

  if (thread_id == current_thread_id)
    core->current_scope->stalker_scope.pending_level--;
  else
    gum_stalker_unfollow (stalker, thread_id);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_invalidate)
{
  GumStalker * stalker = _gum_v8_stalker_get (module);

  if (info.Length () <= 1)
  {
    gpointer address;
    if (!_gum_v8_args_parse (args, "p", &address))
      return;

    gum_stalker_invalidate (stalker, address);
  }
  else
  {
    GumThreadId thread_id;
    gpointer address;
    if (!_gum_v8_args_parse (args, "Zp", &thread_id, &address))
      return;

    {
      ScriptUnlocker unlocker (core);

      gum_stalker_invalidate_for_thread (stalker, thread_id, address);
    }
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_add_call_probe)
{
  GumStalker * stalker = _gum_v8_stalker_get (module);

  gpointer target_address;
  Local<Function> callback_js;
  GumCallProbeCallback callback_c;
  gpointer user_data = NULL;
  if (!_gum_v8_args_parse (args, "pF*|p", &target_address, &callback_js,
      &callback_c, &user_data))
    return;

  GumProbeId id;
  if (!callback_js.IsEmpty ())
  {
    auto probe = g_slice_new (GumV8CallProbe);
    probe->callback = new Global<Function> (isolate, callback_js);
    probe->module = module;

    id = gum_stalker_add_call_probe (stalker, target_address,
        (GumCallProbeCallback) gum_v8_call_probe_on_fire, probe,
        (GDestroyNotify) gum_v8_call_probe_free);
  }
  else
  {
    id = gum_stalker_add_call_probe (stalker, target_address, callback_c,
        user_data, NULL);
  }

  info.GetReturnValue ().Set (id);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_remove_call_probe)
{
  GumProbeId id;
  if (!_gum_v8_args_parse (args, "u", &id))
    return;

  gum_stalker_remove_call_probe (_gum_v8_stalker_get (module), id);
}

GUMJS_DEFINE_FUNCTION (gumjs_stalker_parse)
{
  auto context = isolate->GetCurrentContext ();

  Local<Value> events_value;
  gboolean annotate, stringify;
  if (!_gum_v8_args_parse (args, "Vtt", &events_value, &annotate, &stringify))
    return;

  if (!events_value->IsArrayBuffer ())
  {
    _gum_v8_throw_ascii_literal (isolate, "expected an ArrayBuffer");
    return;
  }

  auto events_store = events_value.As<ArrayBuffer> ()->GetBackingStore ();
  const GumEvent * events = (const GumEvent *) events_store->Data ();
  size_t size = events_store->ByteLength ();
  if (size % sizeof (GumEvent) != 0)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid buffer shape");
    return;
  }

  size_t count = size / sizeof (GumEvent);

  auto rows = Array::New (isolate, (int) count);

  const GumEvent * ev;
  size_t row_index;
  for (ev = events, row_index = 0;
      row_index != count;
      ev++, row_index++)
  {
    Local<Array> row;
    guint column_index = 0;

    switch (ev->type)
    {
      case GUM_CALL:
      {
        const GumCallEvent * call = &ev->call;

        if (annotate)
        {
          row = Array::New (isolate, 4);
          row->Set (context, column_index++,
              _gum_v8_string_new_ascii (isolate, "call")).Check ();
        }
        else
        {
          row = Array::New (isolate, 3);
        }

        row->Set (context, column_index++,
            gum_make_pointer (call->location, stringify, core)).Check ();
        row->Set (context, column_index++,
            gum_make_pointer (call->target, stringify, core)).Check ();
        row->Set (context, column_index++, Integer::New (isolate, call->depth))
            .Check ();

        break;
      }
      case GUM_RET:
      {
        const GumRetEvent * ret = &ev->ret;

        if (annotate)
        {
          row = Array::New (isolate, 4);
          row->Set (context, column_index++,
              _gum_v8_string_new_ascii (isolate, "ret")).Check ();
        }
        else
        {
          row = Array::New (isolate, 3);
        }

        row->Set (context, column_index++,
            gum_make_pointer (ret->location, stringify, core)).Check ();
        row->Set (context, column_index++,
            gum_make_pointer (ret->target, stringify, core)).Check ();
        row->Set (context, column_index++, Integer::New (isolate, ret->depth))
            .Check ();

        break;
      }
      case GUM_EXEC:
      {
        const GumExecEvent * exec = &ev->exec;

        if (annotate)
        {
          row = Array::New (isolate, 2);
          row->Set (context, column_index++,
              _gum_v8_string_new_ascii (isolate, "exec")).Check ();
        }
        else
        {
          row = Array::New (isolate, 1);
        }

        row->Set (context, column_index++,
            gum_make_pointer (exec->location, stringify, core)).Check ();

        break;
      }
      case GUM_BLOCK:
      {
        const GumBlockEvent * block = &ev->block;

        if (annotate)
        {
          row = Array::New (isolate, 3);
          row->Set (context, column_index++,
              _gum_v8_string_new_ascii (isolate, "block")).Check ();
        }
        else
        {
          row = Array::New (isolate, 2);
        }

        row->Set (context, column_index++,
            gum_make_pointer (block->start, stringify, core)).Check ();
        row->Set (context, column_index++,
            gum_make_pointer (block->end, stringify, core)).Check ();

        break;
      }
      case GUM_COMPILE:
      {
        const GumCompileEvent * compile = &ev->compile;

        if (annotate)
        {
          row = Array::New (isolate, 3);
          row->Set (context, column_index++,
              _gum_v8_string_new_ascii (isolate, "compile")).Check ();
        }
        else
        {
          row = Array::New (isolate, 2);
        }

        row->Set (context, column_index++,
            gum_make_pointer (compile->start, stringify, core)).Check ();
        row->Set (context, column_index++,
            gum_make_pointer (compile->end, stringify, core)).Check ();

        break;
      }
      default:
        _gum_v8_throw_ascii_literal (isolate, "invalid event type");
        return;
    }

    rows->Set (context, (uint32_t) row_index, row).Check ();
  }

  info.GetReturnValue ().Set (rows);
}

static void
gum_v8_callback_transformer_transform_block (
    GumStalkerTransformer * transformer,
    GumStalkerIterator * iterator,
    GumStalkerOutput * output)
{
  GumV8SystemErrorPreservationScope error_scope;

  auto self = GUM_V8_CALLBACK_TRANSFORMER_CAST (transformer);
  auto module = self->module;
  auto core = module->core;

  gboolean transform_threw_an_exception;
  {
    ScriptScope scope (core->script);
    auto isolate = core->isolate;
    auto context = isolate->GetCurrentContext ();

    auto callback = Local<Function>::New (isolate, *self->callback);

    GumV8StalkerDefaultIterator * default_iter = NULL;
    GumV8StalkerSpecialIterator * special_iter = NULL;
    Global<v8::Object> * iter_object_handle;
    if (output->encoding == GUM_INSTRUCTION_DEFAULT)
    {
      default_iter = gum_v8_stalker_obtain_default_iterator (module);
      gum_v8_stalker_default_iterator_reset (default_iter, iterator, output);
      iter_object_handle = default_iter->parent.object;
    }
    else
    {
      special_iter = gum_v8_stalker_obtain_special_iterator (module);
      gum_v8_stalker_special_iterator_reset (special_iter, iterator, output);
      iter_object_handle = special_iter->parent.object;
    }

    auto iter_object = Local<Object>::New (isolate, *iter_object_handle);

    auto recv = Undefined (isolate);
    Local<Value> argv[] = { iter_object };
    auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
    transform_threw_an_exception = result.IsEmpty ();
    if (transform_threw_an_exception)
      scope.ProcessAnyPendingException ();

    if (default_iter != NULL)
    {
      gum_v8_stalker_default_iterator_reset (default_iter, NULL, NULL);
      gum_v8_stalker_release_default_iterator (module, default_iter);
    }
    else
    {
      gum_v8_stalker_special_iterator_reset (special_iter, NULL, NULL);
      gum_v8_stalker_release_special_iterator (module, special_iter);
    }
  }

  if (transform_threw_an_exception)
    gum_stalker_unfollow (module->stalker, self->thread_id);
}

static void
gum_v8_callback_transformer_class_init (GumV8CallbackTransformerClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_v8_callback_transformer_dispose;
}

static void
gum_v8_callback_transformer_iface_init (gpointer g_iface,
                                        gpointer iface_data)
{
  auto iface = (GumStalkerTransformerInterface *) g_iface;

  iface->transform_block = gum_v8_callback_transformer_transform_block;
}

static void
gum_v8_callback_transformer_init (GumV8CallbackTransformer * self)
{
}

static void
gum_v8_callback_transformer_dispose (GObject * object)
{
  auto self = GUM_V8_CALLBACK_TRANSFORMER_CAST (object);

  ScriptScope scope (self->module->core->script);

  delete self->callback;
  self->callback = nullptr;

  G_OBJECT_CLASS (gum_v8_callback_transformer_parent_class)->dispose (object);
}

static void
gum_v8_stalker_iterator_init (GumV8StalkerIterator * iter,
                              GumV8Stalker * parent)
{
  iter->handle = NULL;
  iter->instruction = NULL;

  iter->module = parent;
}

static void
gum_v8_stalker_iterator_reset (GumV8StalkerIterator * self,
                               GumStalkerIterator * handle)
{
  self->handle = handle;

  if (self->instruction != NULL)
  {
    self->instruction->insn = NULL;
    gum_v8_stalker_release_instruction (self->module, self->instruction);
  }
  self->instruction = (handle != NULL)
      ? gum_v8_stalker_obtain_instruction (self->module)
      : NULL;
}

static gboolean
gum_v8_stalker_iterator_check_valid (GumV8StalkerIterator * self,
                                     Isolate * isolate)
{
  if (self->handle == NULL)
  {
    _gum_v8_throw (isolate, "invalid operation");
    return FALSE;
  }

  return TRUE;
}

static void
gum_v8_stalker_iterator_get_memory_access (
    GumV8StalkerIterator * self,
    const PropertyCallbackInfo<Value> & info,
    Isolate * isolate)
{
  if (!gum_v8_stalker_iterator_check_valid (self, isolate))
    return;

  Local<String> val;
  switch (gum_stalker_iterator_get_memory_access (self->handle))
  {
    case GUM_MEMORY_ACCESS_OPEN:
      val = _gum_v8_string_new_ascii (isolate, "open");
      break;
    case GUM_MEMORY_ACCESS_EXCLUSIVE:
      val = _gum_v8_string_new_ascii (isolate, "exclusive");
      break;
    default:
      g_assert_not_reached ();
  }
  info.GetReturnValue ().Set (val);
}

static void
gum_v8_stalker_iterator_next (GumV8StalkerIterator * self,
                              const FunctionCallbackInfo<Value> & info,
                              Isolate * isolate)
{
  if (!gum_v8_stalker_iterator_check_valid (self, isolate))
    return;

  if (gum_stalker_iterator_next (self->handle, &self->instruction->insn))
  {
    info.GetReturnValue ().Set (
        Local<Object>::New (isolate, *self->instruction->object));
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

static void
gum_v8_stalker_iterator_keep (GumV8StalkerIterator * self,
                              Isolate * isolate)
{
  if (!gum_v8_stalker_iterator_check_valid (self, isolate))
    return;

  gum_stalker_iterator_keep (self->handle);
}

static void
gum_v8_stalker_iterator_put_callout (GumV8StalkerIterator * self,
                                     const GumV8Args * args,
                                     Isolate * isolate)
{
  if (!gum_v8_stalker_iterator_check_valid (self, isolate))
    return;

  Local<Function> callback_js;
  GumStalkerCallout callback_c;
  gpointer user_data = NULL;
  if (!_gum_v8_args_parse (args, "F*|p", &callback_js, &callback_c, &user_data))
    return;

  if (!callback_js.IsEmpty ())
  {
    auto callout = g_slice_new (GumV8Callout);
    callout->callback = new Global<Function> (isolate, callback_js);
    callout->module = self->module;

    gum_stalker_iterator_put_callout (self->handle,
        (GumStalkerCallout) gum_v8_callout_on_invoke, callout,
        (GDestroyNotify) gum_v8_callout_free);
  }
  else
  {
    gum_stalker_iterator_put_callout (self->handle, callback_c, user_data,
        NULL);
  }
}

static void
gum_v8_stalker_iterator_put_chaining_return (GumV8StalkerIterator * self,
                                             Isolate * isolate)
{
  if (!gum_v8_stalker_iterator_check_valid (self, isolate))
    return;

  gum_stalker_iterator_put_chaining_return (self->handle);
}

static GumV8StalkerDefaultIterator *
gum_v8_stalker_default_iterator_new_persistent (GumV8Stalker * parent)
{
  auto isolate = parent->core->isolate;

  auto iter = g_slice_new (GumV8StalkerDefaultIterator);

  auto writer = &iter->parent;
  _gum_v8_default_writer_init (writer, parent->writer);

  gum_v8_stalker_iterator_init (&iter->iterator, parent);

  auto iter_value =
      Local<Object>::New (isolate, *parent->default_iterator_value);
  auto object = iter_value->Clone ();
  object->SetAlignedPointerInInternalField (0, writer);
  object->SetAlignedPointerInInternalField (1, iter);
  writer->object = new Global<Object> (isolate, object);

  return iter;
}

static void
gum_v8_stalker_default_iterator_release_persistent (
    GumV8StalkerDefaultIterator * self)
{
  auto object = self->parent.object;

  object->SetWeak (self, gum_v8_stalker_default_iterator_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (self->iterator.module->default_iterators, self);
}

static void
gum_v8_stalker_default_iterator_on_weak_notify (
    const WeakCallbackInfo<GumV8StalkerDefaultIterator> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();

  g_hash_table_remove (self->iterator.module->default_iterators, self);
}

static void
gum_v8_stalker_default_iterator_free (GumV8StalkerDefaultIterator * self)
{
  _gum_v8_default_writer_finalize (&self->parent);

  g_slice_free (GumV8StalkerDefaultIterator, self);
}

static void
gum_v8_stalker_default_iterator_reset (GumV8StalkerDefaultIterator * self,
                                       GumStalkerIterator * handle,
                                       GumStalkerOutput * output)
{
  _gum_v8_default_writer_reset (&self->parent, (GumV8DefaultWriterImpl *)
      ((output != NULL) ? output->writer.instance : NULL));
  gum_v8_stalker_iterator_reset (&self->iterator, handle);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_GETTER (
    gumjs_stalker_default_iterator_get_memory_access,
    GumV8StalkerDefaultIterator)
{
  gum_v8_stalker_iterator_get_memory_access (&self->iterator, info, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (gumjs_stalker_default_iterator_next,
                                     GumV8StalkerDefaultIterator)
{
  gum_v8_stalker_iterator_next (&self->iterator, info, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (gumjs_stalker_default_iterator_keep,
                                     GumV8StalkerDefaultIterator)
{
  gum_v8_stalker_iterator_keep (&self->iterator, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (gumjs_stalker_default_iterator_put_callout,
                                     GumV8StalkerDefaultIterator)
{
  gum_v8_stalker_iterator_put_callout (&self->iterator, args, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (
    gumjs_stalker_default_iterator_put_chaining_return,
    GumV8StalkerDefaultIterator)
{
  gum_v8_stalker_iterator_put_chaining_return (&self->iterator, isolate);
}

static GumV8StalkerSpecialIterator *
gum_v8_stalker_special_iterator_new_persistent (GumV8Stalker * parent)
{
  auto isolate = parent->core->isolate;

  auto iter = g_slice_new (GumV8StalkerSpecialIterator);

  auto writer = &iter->parent;
  _gum_v8_special_writer_init (writer, parent->writer);

  gum_v8_stalker_iterator_init (&iter->iterator, parent);

  auto iter_value =
      Local<Object>::New (isolate, *parent->special_iterator_value);
  auto object = iter_value->Clone ();
  object->SetAlignedPointerInInternalField (0, writer);
  object->SetAlignedPointerInInternalField (1, iter);
  writer->object = new Global<Object> (isolate, object);

  return iter;
}

static void
gum_v8_stalker_special_iterator_release_persistent (
    GumV8StalkerSpecialIterator * self)
{
  auto object = self->parent.object;

  object->SetWeak (self, gum_v8_stalker_special_iterator_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (self->iterator.module->special_iterators, self);
}

static void
gum_v8_stalker_special_iterator_on_weak_notify (
    const WeakCallbackInfo<GumV8StalkerSpecialIterator> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();

  g_hash_table_remove (self->iterator.module->special_iterators, self);
}

static void
gum_v8_stalker_special_iterator_free (GumV8StalkerSpecialIterator * self)
{
  _gum_v8_special_writer_finalize (&self->parent);

  g_slice_free (GumV8StalkerSpecialIterator, self);
}

static void
gum_v8_stalker_special_iterator_reset (GumV8StalkerSpecialIterator * self,
                                       GumStalkerIterator * handle,
                                       GumStalkerOutput * output)
{
  _gum_v8_special_writer_reset (&self->parent, (GumV8SpecialWriterImpl *)
      ((output != NULL) ? output->writer.instance : NULL));
  gum_v8_stalker_iterator_reset (&self->iterator, handle);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_GETTER (
    gumjs_stalker_special_iterator_get_memory_access,
    GumV8StalkerSpecialIterator)
{
  gum_v8_stalker_iterator_get_memory_access (&self->iterator, info, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (gumjs_stalker_special_iterator_next,
                                     GumV8StalkerSpecialIterator)
{
  gum_v8_stalker_iterator_next (&self->iterator, info, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (gumjs_stalker_special_iterator_keep,
                                     GumV8StalkerSpecialIterator)
{
  gum_v8_stalker_iterator_keep (&self->iterator, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (gumjs_stalker_special_iterator_put_callout,
                                     GumV8StalkerSpecialIterator)
{
  gum_v8_stalker_iterator_put_callout (&self->iterator, args, isolate);
}

GUMJS_DEFINE_DIRECT_SUBCLASS_METHOD (
    gumjs_stalker_special_iterator_put_chaining_return,
    GumV8StalkerSpecialIterator)
{
  gum_v8_stalker_iterator_put_chaining_return (&self->iterator, isolate);
}

static void
gum_v8_callout_free (GumV8Callout * callout)
{
  ScriptScope scope (callout->module->core->script);

  delete callout->callback;

  g_slice_free (GumV8Callout, callout);
}

static void
gum_v8_callout_on_invoke (GumCpuContext * cpu_context,
                          GumV8Callout * self)
{
  GumV8SystemErrorPreservationScope error_scope;

  auto core = self->module->core;
  ScriptScope scope (core->script);
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto cpu_context_value = _gum_v8_cpu_context_new_mutable (cpu_context, core);

  auto callback (Local<Function>::New (isolate, *self->callback));
  auto recv = Undefined (isolate);
  Local<Value> argv[] = { cpu_context_value };
  auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
  if (result.IsEmpty ())
    scope.ProcessAnyPendingException ();

  _gum_v8_cpu_context_free_later (
      new Global<Object> (isolate, cpu_context_value), core);
}

static void
gum_v8_call_probe_free (GumV8CallProbe * probe)
{
  ScriptScope scope (probe->module->core->script);

  delete probe->callback;

  g_slice_free (GumV8CallProbe, probe);
}

static void
gum_v8_call_probe_on_fire (GumCallDetails * details,
                           GumV8CallProbe * self)
{
  GumV8SystemErrorPreservationScope error_scope;

  auto core = self->module->core;
  ScriptScope scope (core->script);
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto probe_args =
      Local<ObjectTemplate>::New (isolate, *self->module->probe_args);
  auto args = probe_args->NewInstance (context).ToLocalChecked ();
  args->SetAlignedPointerInInternalField (0, self);
  args->SetAlignedPointerInInternalField (1, details);

  auto callback (Local<Function>::New (isolate, *self->callback));
  auto recv = Undefined (isolate);
  Local<Value> argv[] = { args };
  auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
  if (result.IsEmpty ())
    scope.ProcessAnyPendingException ();

  args->SetAlignedPointerInInternalField (0, nullptr);
  args->SetAlignedPointerInInternalField (1, nullptr);
}

static void
gumjs_probe_args_get_nth (uint32_t index,
                          const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.This ();
  auto self =
      (GumV8CallProbe *) wrapper->GetAlignedPointerFromInternalField (0);
  auto call =
      (GumCallDetails *) wrapper->GetAlignedPointerFromInternalField (1);
  auto core = self->module->core;

  if (call == nullptr)
  {
    _gum_v8_throw_ascii_literal (core->isolate, "invalid operation");
    return;
  }

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (
          gum_cpu_context_get_nth_argument (call->cpu_context, index), core));
}

static void
gumjs_probe_args_set_nth (uint32_t index,
                          Local<Value> value,
                          const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.This ();
  auto self =
      (GumV8CallProbe *) wrapper->GetAlignedPointerFromInternalField (0);
  auto call =
      (GumCallDetails *) wrapper->GetAlignedPointerFromInternalField (1);
  auto core = self->module->core;

  if (call == nullptr)
  {
    _gum_v8_throw_ascii_literal (core->isolate, "invalid operation");
    return;
  }

  info.GetReturnValue ().Set (value);

  gpointer raw_value;
  if (!_gum_v8_native_pointer_get (value, &raw_value, core))
    return;

  gum_cpu_context_replace_nth_argument (call->cpu_context, index, raw_value);
}

static GumV8StalkerDefaultIterator *
gum_v8_stalker_obtain_default_iterator (GumV8Stalker * self)
{
  GumV8StalkerDefaultIterator * iterator;

  if (!self->cached_default_iterator_in_use)
  {
    iterator = self->cached_default_iterator;
    self->cached_default_iterator_in_use = TRUE;
  }
  else
  {
    iterator = gum_v8_stalker_default_iterator_new_persistent (self);
  }

  return iterator;
}

static void
gum_v8_stalker_release_default_iterator (GumV8Stalker * self,
                                         GumV8StalkerDefaultIterator * iterator)
{
  if (iterator == self->cached_default_iterator)
    self->cached_default_iterator_in_use = FALSE;
  else
    gum_v8_stalker_default_iterator_release_persistent (iterator);
}

static GumV8StalkerSpecialIterator *
gum_v8_stalker_obtain_special_iterator (GumV8Stalker * self)
{
  GumV8StalkerSpecialIterator * iterator;

  if (!self->cached_special_iterator_in_use)
  {
    iterator = self->cached_special_iterator;
    self->cached_special_iterator_in_use = TRUE;
  }
  else
  {
    iterator = gum_v8_stalker_special_iterator_new_persistent (self);
  }

  return iterator;
}

static void
gum_v8_stalker_release_special_iterator (GumV8Stalker * self,
                                         GumV8StalkerSpecialIterator * iterator)
{
  if (iterator == self->cached_special_iterator)
    self->cached_special_iterator_in_use = FALSE;
  else
    gum_v8_stalker_special_iterator_release_persistent (iterator);
}

static GumV8InstructionValue *
gum_v8_stalker_obtain_instruction (GumV8Stalker * self)
{
  GumV8InstructionValue * value;

  if (!self->cached_instruction_in_use)
  {
    value = self->cached_instruction;
    self->cached_instruction_in_use = TRUE;
  }
  else
  {
    value = _gum_v8_instruction_new_persistent (self->instruction);
  }

  return value;
}

static void
gum_v8_stalker_release_instruction (GumV8Stalker * self,
                                    GumV8InstructionValue * value)
{
  if (value == self->cached_instruction)
  {
    self->cached_instruction_in_use = FALSE;
  }
  else
  {
    _gum_v8_instruction_release_persistent (value);
  }
}

static Local<Value>
gum_make_pointer (gpointer value,
                  gboolean stringify,
                  GumV8Core * core)
{
  if (stringify)
  {
    gchar str[32];

    g_sprintf (str, "0x%" G_GSIZE_MODIFIER "x", GPOINTER_TO_SIZE (value));

    return _gum_v8_string_new_ascii (core->isolate, str);
  }
  else
  {
    return _gum_v8_native_pointer_new (value, core);
  }
}
