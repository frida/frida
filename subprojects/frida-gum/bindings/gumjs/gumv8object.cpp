/*
 * Copyright (C) 2016-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8object.h"

#include "gumv8scope.h"

using namespace v8;

typedef GumV8Object<void, void> GumV8AnyObject;
typedef GumV8ObjectOperation<void, void> GumV8AnyObjectOperation;
typedef GumV8ModuleOperation<void> GumV8AnyModuleOperation;

struct GumV8TryScheduleIfIdleOperation : public GumV8ObjectOperation<void, void>
{
  GumV8AnyObjectOperation * blocked_operation;
};

static void gum_v8_object_on_weak_notify (
    const WeakCallbackInfo<GumV8AnyObject> & info);
static void gum_v8_object_free (GumV8AnyObject * self);

static void gum_v8_object_operation_free (GumV8AnyObjectOperation * self);
static void gum_v8_object_operation_try_schedule_when_idle (
    GumV8AnyObjectOperation * self);
static void gum_v8_try_schedule_if_idle_operation_perform (
    GumV8TryScheduleIfIdleOperation * self);

static void gum_v8_module_operation_free (GumV8AnyModuleOperation * self);

void
gum_v8_object_manager_init (GumV8ObjectManager * self)
{
  self->object_by_handle = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_object_free);
  self->cancellable = g_cancellable_new ();
}

void
gum_v8_object_manager_flush (GumV8ObjectManager * self)
{
  auto cancellables = g_ptr_array_new_full (
      g_hash_table_size (self->object_by_handle), g_object_unref);

  GHashTableIter iter;
  GumV8AnyObject * object;
  g_hash_table_iter_init (&iter, self->object_by_handle);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &object))
  {
    g_ptr_array_add (cancellables, g_object_ref (object->cancellable));
  }

  g_ptr_array_foreach (cancellables, (GFunc) g_cancellable_cancel, NULL);
  g_ptr_array_unref (cancellables);

  g_cancellable_cancel (self->cancellable);
}

void
gum_v8_object_manager_free (GumV8ObjectManager * self)
{
  g_hash_table_remove_all (self->object_by_handle);

  g_object_unref (self->cancellable);
  g_hash_table_unref (self->object_by_handle);
}

gpointer
_gum_v8_object_manager_add (GumV8ObjectManager * self,
                            Local<Object> wrapper,
                            gpointer handle,
                            gpointer module,
                            GumV8Core * core)
{
  auto object = g_slice_new (GumV8AnyObject);

  auto * w = new Global<Object> (core->isolate, wrapper);
  w->SetWeak (object, gum_v8_object_on_weak_notify,
      WeakCallbackType::kParameter);
  object->wrapper = w;
  object->handle = handle;
  object->cancellable = g_cancellable_new ();

  object->core = core;
  object->module = module;

  object->manager = self;
  object->num_active_operations = 0;
  object->pending_operations = g_queue_new ();

  wrapper->SetAlignedPointerInInternalField (0, object);

  g_hash_table_insert (self->object_by_handle, handle, object);

  return object;
}

gpointer
_gum_v8_object_manager_lookup (GumV8ObjectManager * self,
                               gpointer handle)
{
  return g_hash_table_lookup (self->object_by_handle, handle);
}

static void
gum_v8_object_on_weak_notify (
    const WeakCallbackInfo<GumV8AnyObject> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto object = info.GetParameter ();
  g_hash_table_remove (object->manager->object_by_handle, object->handle);
}

static void
gum_v8_object_free (GumV8AnyObject * self)
{
  g_assert (self->num_active_operations == 0);
  g_assert (g_queue_is_empty (self->pending_operations));
  g_queue_free (self->pending_operations);

  g_object_unref (self->cancellable);
  g_object_unref (self->handle);
  delete self->wrapper;

  g_slice_free (GumV8AnyObject, self);
}

gpointer
_gum_v8_object_operation_new (gsize size,
                              gpointer opaque_object,
                              Local<Value> callback,
                              GCallback perform,
                              GDestroyNotify dispose,
                              GumV8Core * core)
{
  auto object = (GumV8AnyObject *) opaque_object;
  auto isolate = core->isolate;

  auto op = (GumV8AnyObjectOperation *) g_slice_alloc (size);

  op->object = object;
  op->callback = new Global<Function> (isolate, callback.As<Function> ());

  op->core = core;

  op->wrapper = new Global<Object> (isolate, *object->wrapper);
  op->job = gum_script_job_new (core->scheduler, (GumScriptJobFunc) perform, op,
      (GDestroyNotify) gum_v8_object_operation_free);
  op->pending_dependencies = NULL;
  op->size = size;
  op->dispose = (void (*) (GumV8AnyObjectOperation * op)) dispose;

  _gum_v8_core_pin (core);

  return op;
}

static void
gum_v8_object_operation_free (GumV8AnyObjectOperation * self)
{
  auto object = self->object;
  auto core = object->core;

  g_assert (self->pending_dependencies == NULL);

  if (self->dispose != NULL)
    self->dispose (self);

  {
    ScriptScope scope (core->script);

    delete self->wrapper;
    delete self->callback;

    if (--object->num_active_operations == 0)
    {
      auto next = g_queue_pop_head (object->pending_operations);
      if (next != NULL)
        _gum_v8_object_operation_schedule (next);
    }

    _gum_v8_core_unpin (core);
  }

  g_slice_free1 (self->size, self);
}

void
_gum_v8_object_operation_schedule (gpointer opaque_self)
{
  auto self = (GumV8AnyObjectOperation *) opaque_self;

  self->object->num_active_operations++;
  gum_script_job_start_on_js_thread (self->job);
}

void
_gum_v8_object_operation_schedule_when_idle (gpointer opaque_self,
                                             GPtrArray * dependencies)
{
  auto self = (GumV8AnyObjectOperation *) opaque_self;

  if (dependencies != NULL)
  {
    for (guint i = 0; i != dependencies->len; i++)
    {
      auto dependency = (GumV8AnyObject *) g_ptr_array_index (dependencies, i);
      if (dependency->num_active_operations > 0)
      {
        auto op = gum_v8_object_operation_new (dependency, Local<Value> (),
            gum_v8_try_schedule_if_idle_operation_perform);
        op->blocked_operation = self;
        self->pending_dependencies =
            g_slist_prepend (self->pending_dependencies, op);
        gum_v8_object_operation_schedule_when_idle (op);
      }
    }
  }

  gum_v8_object_operation_try_schedule_when_idle (self);
}

static void
gum_v8_object_operation_try_schedule_when_idle (GumV8AnyObjectOperation * self)
{
  GumV8AnyObject * object = self->object;

  if (self->pending_dependencies != NULL)
    return;

  if (object->num_active_operations == 0)
    _gum_v8_object_operation_schedule (self);
  else
    g_queue_push_tail (object->pending_operations, self);
}

static void
gum_v8_try_schedule_if_idle_operation_perform (
    GumV8TryScheduleIfIdleOperation * self)
{
  GumV8AnyObjectOperation * op = self->blocked_operation;

  {
    ScriptScope scope (self->core->script);

    op->pending_dependencies = g_slist_remove (op->pending_dependencies, self);
    gum_v8_object_operation_try_schedule_when_idle (op);
  }

  gum_v8_object_operation_finish (self);
}

gpointer
_gum_v8_module_operation_new (gsize size,
                              gpointer module,
                              GumV8ObjectManager * manager,
                              Local<Value> callback,
                              GCallback perform,
                              GDestroyNotify dispose,
                              GumV8Core * core)
{
  auto isolate = core->isolate;

  auto op = (GumV8AnyModuleOperation *) g_slice_alloc (size);

  op->module = module;
  op->cancellable = manager->cancellable;
  op->callback = new Global<Function> (isolate, callback.As<Function> ());

  op->core = core;

  op->job = gum_script_job_new (core->scheduler, (GumScriptJobFunc) perform, op,
      (GDestroyNotify) gum_v8_module_operation_free);
  op->size = size;
  op->dispose = (void (*) (GumV8AnyModuleOperation * op)) dispose;

  _gum_v8_core_pin (core);

  return op;
}

static void
gum_v8_module_operation_free (GumV8AnyModuleOperation * self)
{
  auto core = self->core;

  if (self->dispose != NULL)
    self->dispose (self);

  {
    ScriptScope scope (core->script);

    delete self->callback;

    _gum_v8_core_unpin (core);
  }

  g_slice_free1 (self->size, self);
}
