/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickobject.h"

#include "gumquickmacros.h"

typedef struct _GumQuickTryScheduleIfIdleOperation
    GumQuickTryScheduleIfIdleOperation;

struct _GumQuickTryScheduleIfIdleOperation
{
  GumQuickObjectOperation parent;
  GumQuickObjectOperation * blocked_operation;
};

static void gum_quick_object_free (GumQuickObject * self);

static void gum_quick_object_operation_free (GumQuickObjectOperation * self);
static void gum_quick_object_operation_try_schedule_when_idle (
    GumQuickObjectOperation * self);
static void gum_quick_try_schedule_if_idle_operation_perform (
    GumQuickTryScheduleIfIdleOperation * self);

static void gum_quick_module_operation_free (GumQuickModuleOperation * self);

void
_gum_quick_object_manager_init (GumQuickObjectManager * self,
                                gpointer module,
                                GumQuickCore * core)
{
  self->module = module;
  self->core = core;
  self->object_by_handle = g_hash_table_new (NULL, NULL);
  self->cancellable = g_cancellable_new ();
}

void
_gum_quick_object_manager_flush (GumQuickObjectManager * self)
{
  GPtrArray * cancellables;
  GHashTableIter iter;
  GumQuickObject * object;

  cancellables = g_ptr_array_new_full (
      g_hash_table_size (self->object_by_handle), g_object_unref);
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
_gum_quick_object_manager_free (GumQuickObjectManager * self)
{
  GHashTableIter iter;
  GumQuickObject * object;

  g_hash_table_iter_init (&iter, self->object_by_handle);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &object))
  {
    object->manager = NULL;
  }
  g_hash_table_remove_all (self->object_by_handle);

  g_object_unref (self->cancellable);
  g_hash_table_unref (self->object_by_handle);
}

gpointer
_gum_quick_object_manager_add (GumQuickObjectManager * self,
                               JSContext * ctx,
                               JSValue wrapper,
                               gpointer handle)
{
  GumQuickCore * core = self->core;
  GumQuickObject * object;

  object = g_slice_new (GumQuickObject);
  object->wrapper = wrapper;
  object->handle = handle;
  object->cancellable = g_cancellable_new ();

  object->core = core;

  object->manager = self;
  object->num_active_operations = 0;
  object->pending_operations = g_queue_new ();

  g_hash_table_insert (self->object_by_handle, handle, object);

  JS_SetOpaque (wrapper, object);

  JS_DefinePropertyValue (ctx, wrapper,
      GUM_QUICK_CORE_ATOM (core, resource),
      _gum_quick_native_resource_new (ctx, object,
          (GDestroyNotify) gum_quick_object_free, core),
      0);

  return object;
}

gpointer
_gum_quick_object_manager_lookup (GumQuickObjectManager * self,
                                  gpointer handle)
{
  return g_hash_table_lookup (self->object_by_handle, handle);
}

static void
gum_quick_object_free (GumQuickObject * self)
{
  if (self->manager != NULL)
    g_hash_table_remove (self->manager->object_by_handle, self->handle);

  g_assert (self->num_active_operations == 0);
  g_assert (g_queue_is_empty (self->pending_operations));
  g_queue_free (self->pending_operations);

  g_object_unref (self->cancellable);
  g_object_unref (self->handle);

  g_slice_free (GumQuickObject, self);
}

gpointer
_gum_quick_object_operation_alloc (gsize size,
                                   GumQuickObject * object,
                                   JSValue callback,
                                   GumQuickObjectOperationFunc perform,
                                   GumQuickObjectOperationFunc dispose)
{
  GumQuickCore * core = object->core;
  JSContext * ctx = core->ctx;
  GumQuickObjectOperation * op;

  op = g_slice_alloc (size);

  op->object = object;
  op->callback = JS_DupValue (ctx, callback);

  op->core = core;

  op->wrapper = JS_DupValue (ctx, object->wrapper);
  op->job = gum_script_job_new (core->scheduler, (GumScriptJobFunc) perform, op,
      (GDestroyNotify) gum_quick_object_operation_free);
  op->pending_dependencies = NULL;
  op->size = size;
  op->dispose = dispose;

  _gum_quick_core_pin (core);

  return op;
}

static void
gum_quick_object_operation_free (GumQuickObjectOperation * self)
{
  GumQuickObject * object = self->object;
  GumQuickCore * core = object->core;
  JSContext * ctx = core->ctx;
  GumQuickScope scope;

  g_assert (self->pending_dependencies == NULL);

  if (self->dispose != NULL)
    self->dispose (self);

  _gum_quick_scope_enter (&scope, core);

  if (--object->num_active_operations == 0)
  {
    gpointer next;

    next = g_queue_pop_head (object->pending_operations);
    if (next != NULL)
      _gum_quick_object_operation_schedule (next);
  }

  JS_FreeValue (ctx, self->wrapper);
  JS_FreeValue (ctx, self->callback);

  _gum_quick_core_unpin (core);

  _gum_quick_scope_leave (&scope);

  g_slice_free1 (self->size, self);
}

void
_gum_quick_object_operation_schedule (gpointer self)
{
  GumQuickObjectOperation * op = self;

  op->object->num_active_operations++;
  gum_script_job_start_on_js_thread (op->job);
}

void
_gum_quick_object_operation_schedule_when_idle (gpointer self,
                                                GPtrArray * dependencies)
{
  GumQuickObjectOperation * op = self;

  if (dependencies != NULL)
  {
    guint i;

    for (i = 0; i != dependencies->len; i++)
    {
      GumQuickObject * dependency = g_ptr_array_index (dependencies, i);

      if (dependency->num_active_operations > 0)
      {
        GumQuickTryScheduleIfIdleOperation * try_schedule;

        try_schedule = _gum_quick_object_operation_new (
            GumQuickTryScheduleIfIdleOperation, dependency, JS_NULL,
            gum_quick_try_schedule_if_idle_operation_perform, NULL);
        try_schedule->blocked_operation = op;
        op->pending_dependencies =
            g_slist_prepend (op->pending_dependencies, try_schedule);
        _gum_quick_object_operation_schedule_when_idle (try_schedule, NULL);
      }
    }
  }

  gum_quick_object_operation_try_schedule_when_idle (op);
}

static void
gum_quick_object_operation_try_schedule_when_idle (
    GumQuickObjectOperation * self)
{
  GumQuickObject * object = self->object;

  if (self->pending_dependencies != NULL)
    return;

  if (object->num_active_operations == 0)
    _gum_quick_object_operation_schedule (self);
  else
    g_queue_push_tail (object->pending_operations, self);
}

static void
gum_quick_try_schedule_if_idle_operation_perform (
    GumQuickTryScheduleIfIdleOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickObjectOperation * blocked = self->blocked_operation;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, op->core);

  blocked->pending_dependencies =
      g_slist_remove (blocked->pending_dependencies, self);
  gum_quick_object_operation_try_schedule_when_idle (blocked);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

void
_gum_quick_object_operation_finish (GumQuickObjectOperation * self)
{
  gum_script_job_free (self->job);
}

gpointer
_gum_quick_module_operation_alloc (gsize size,
                                   gpointer module,
                                   GumQuickObjectManager * manager,
                                   JSValue callback,
                                   GumQuickModuleOperationFunc perform,
                                   GumQuickModuleOperationFunc dispose)
{
  GumQuickCore * core = manager->core;
  GumQuickModuleOperation * op;

  op = g_slice_alloc (size);

  op->module = module;
  op->cancellable = manager->cancellable;
  op->callback = JS_DupValue (core->ctx, callback);

  op->core = core;

  op->job = gum_script_job_new (core->scheduler, (GumScriptJobFunc) perform, op,
      (GDestroyNotify) gum_quick_module_operation_free);
  op->size = size;
  op->dispose = dispose;

  _gum_quick_core_pin (core);

  return op;
}

static void
gum_quick_module_operation_free (GumQuickModuleOperation * self)
{
  GumQuickCore * core = self->core;
  GumQuickScope scope;

  if (self->dispose != NULL)
    self->dispose (self);

  _gum_quick_scope_enter (&scope, core);

  JS_FreeValue (core->ctx, self->callback);
  _gum_quick_core_unpin (core);

  _gum_quick_scope_leave (&scope);

  g_slice_free1 (self->size, self);
}

void
_gum_quick_module_operation_schedule (gpointer self)
{
  GumQuickModuleOperation * op = self;

  gum_script_job_start_on_js_thread (op->job);
}

void
_gum_quick_module_operation_finish (GumQuickModuleOperation * self)
{
  gum_script_job_free (self->job);
}
