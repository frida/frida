/*
 * Copyright (C) 2016-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_OBJECT_H__
#define __GUM_V8_OBJECT_H__

#include "gumv8core.h"

struct GumV8ObjectManager
{
  GHashTable * object_by_handle;
  GCancellable * cancellable;
};

template<typename O, typename M>
struct GumV8Object
{
  v8::Global<v8::Object> * wrapper;
  O * handle;
  GCancellable * cancellable;

  GumV8Core * core;
  M * module;

  GumV8ObjectManager * manager;
  guint num_active_operations;
  GQueue * pending_operations;
};

template<typename O, typename M>
struct GumV8ObjectOperation
{
  GumV8Object<O, M> * object;
  v8::Global<v8::Function> * callback;

  GumV8Core * core;

  v8::Global<v8::Object> * wrapper;
  GumScriptJob * job;
  GSList * pending_dependencies;
  gsize size;
  void (* dispose) (GumV8ObjectOperation<O, M> * op);
};

template<typename M>
struct GumV8ModuleOperation
{
  M * module;
  GCancellable * cancellable;
  v8::Global<v8::Function> * callback;

  GumV8Core * core;

  GumScriptJob * job;
  gsize size;
  void (* dispose) (GumV8ModuleOperation<M> * op);
};

G_GNUC_INTERNAL void gum_v8_object_manager_init (GumV8ObjectManager * self);
G_GNUC_INTERNAL void gum_v8_object_manager_flush (GumV8ObjectManager * self);
G_GNUC_INTERNAL void gum_v8_object_manager_free (GumV8ObjectManager * self);
G_GNUC_INTERNAL gpointer _gum_v8_object_manager_add (GumV8ObjectManager * self,
    v8::Local<v8::Object> wrapper, gpointer handle, gpointer module,
    GumV8Core * core);
G_GNUC_INTERNAL gpointer _gum_v8_object_manager_lookup (
    GumV8ObjectManager * self, gpointer handle);

G_GNUC_INTERNAL gpointer _gum_v8_object_operation_new (gsize size,
    gpointer opaque_object, v8::Local<v8::Value> callback, GCallback perform,
    GDestroyNotify dispose, GumV8Core * core);
G_GNUC_INTERNAL void _gum_v8_object_operation_schedule (gpointer opaque_self);
G_GNUC_INTERNAL void _gum_v8_object_operation_schedule_when_idle (
    gpointer opaque_self, GPtrArray * dependencies);

G_GNUC_INTERNAL gpointer _gum_v8_module_operation_new (gsize size,
    gpointer module, GumV8ObjectManager * manager,
    v8::Local<v8::Value> callback, GCallback perform, GDestroyNotify dispose,
    GumV8Core * core);

template<typename T>
T *
gum_v8_object_get (const v8::FunctionCallbackInfo<v8::Value> & info)
{
  return (T *) info.Holder ()->GetAlignedPointerFromInternalField (0);
}

template<typename O, typename M>
GumV8Object<O, M> *
gum_v8_object_manager_add (GumV8ObjectManager * self,
                           v8::Local<v8::Object> wrapper,
                           O * handle,
                           M * module)
{
  return (GumV8Object<O, M> *) _gum_v8_object_manager_add (self, wrapper,
      handle, module, module->core);
}

template<typename O, typename M>
GumV8Object<O, M> *
gum_v8_object_manager_lookup (GumV8ObjectManager * self,
                              O * handle)
{
  return (GumV8Object<O, M> *) _gum_v8_object_manager_lookup (self, handle);
}

template<typename T, typename O, typename M>
T *
gum_v8_object_operation_new (GumV8Object<O, M> * object,
                             v8::Local<v8::Value> callback,
                             void (* perform) (T * operation),
                             void (* dispose) (T * operation) = nullptr)
{
  return (T *) _gum_v8_object_operation_new (sizeof (T), object, callback,
      (GCallback) perform, (GDestroyNotify) dispose, object->core);
}

template<typename O, typename M>
void
gum_v8_object_operation_schedule (GumV8ObjectOperation<O, M> * self)
{
  _gum_v8_object_operation_schedule (self);
}

template<typename O, typename M>
void
gum_v8_object_operation_schedule_when_idle (GumV8ObjectOperation<O, M> * self)
{
  _gum_v8_object_operation_schedule_when_idle (self, NULL);
}

template<typename O, typename M>
void
gum_v8_object_operation_schedule_when_idle (GumV8ObjectOperation<O, M> * self,
                                            GPtrArray * dependencies)
{
  _gum_v8_object_operation_schedule_when_idle (self, dependencies);
}

template<typename O, typename M>
void
gum_v8_object_operation_finish (GumV8ObjectOperation<O, M> * self)
{
  gum_script_job_free (self->job);
}

template<typename T, typename M>
T *
gum_v8_module_operation_new (M * module,
                             v8::Local<v8::Value> callback,
                             void (* perform) (T * operation),
                             void (* dispose) (T * operation) = nullptr)
{
  return (T *) _gum_v8_module_operation_new (sizeof (T), module,
      &module->objects, callback, (GCallback) perform, (GDestroyNotify) dispose,
      module->core);
}

template<typename M>
void
gum_v8_module_operation_schedule (GumV8ModuleOperation<M> * self)
{
  gum_script_job_start_on_js_thread (self->job);
}

template<typename M>
void
gum_v8_module_operation_finish (GumV8ModuleOperation<M> * self)
{
  gum_script_job_free (self->job);
}

#endif
