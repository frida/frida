/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_OBJECT_H__
#define __GUM_QUICK_OBJECT_H__

#include "gumquickvalue.h"

#define GUM_QUICK_OBJECT_OPERATION(o) ((GumQuickObjectOperation *) (o))
#define GUM_QUICK_MODULE_OPERATION(o) ((GumQuickModuleOperation *) (o))

G_BEGIN_DECLS

typedef struct _GumQuickObjectManager GumQuickObjectManager;
typedef struct _GumQuickObject GumQuickObject;
typedef struct _GumQuickObjectOperation GumQuickObjectOperation;
typedef struct _GumQuickModuleOperation GumQuickModuleOperation;

typedef void (* GumQuickObjectOperationFunc) (GumQuickObjectOperation * op);
typedef void (* GumQuickModuleOperationFunc) (GumQuickModuleOperation * op);

struct _GumQuickObjectManager
{
  gpointer module;
  GumQuickCore * core;
  GHashTable * object_by_handle;
  GCancellable * cancellable;
};

struct _GumQuickObject
{
  JSValue wrapper;
  gpointer handle;
  GCancellable * cancellable;

  GumQuickCore * core;

  GumQuickObjectManager * manager;
  guint num_active_operations;
  GQueue * pending_operations;
};

struct _GumQuickObjectOperation
{
  GumQuickObject * object;
  JSValue callback;

  GumQuickCore * core;

  JSValue wrapper;
  GumScriptJob * job;
  GSList * pending_dependencies;
  gsize size;
  GumQuickObjectOperationFunc dispose;
};

struct _GumQuickModuleOperation
{
  gpointer module;
  GCancellable * cancellable;
  JSValue callback;

  GumQuickCore * core;

  GumScriptJob * job;
  gsize size;
  GumQuickModuleOperationFunc dispose;
};

G_GNUC_INTERNAL void _gum_quick_object_manager_init (
    GumQuickObjectManager * self, gpointer module, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_object_manager_flush (
    GumQuickObjectManager * self);
G_GNUC_INTERNAL void _gum_quick_object_manager_free (
    GumQuickObjectManager * self);
G_GNUC_INTERNAL gpointer _gum_quick_object_manager_add (
    GumQuickObjectManager * self, JSContext * ctx, JSValue wrapper,
    gpointer handle);
G_GNUC_INTERNAL gpointer _gum_quick_object_manager_lookup (
    GumQuickObjectManager * self, gpointer handle);

#define _gum_quick_object_operation_new(type, object, callback, perform, \
    dispose) _gum_quick_object_operation_alloc (sizeof (type), object, \
        callback, (GumQuickObjectOperationFunc) perform, \
        (GumQuickObjectOperationFunc) dispose)
G_GNUC_INTERNAL gpointer _gum_quick_object_operation_alloc (gsize size,
    GumQuickObject * object, JSValue callback,
    GumQuickObjectOperationFunc perform, GumQuickObjectOperationFunc dispose);
G_GNUC_INTERNAL void _gum_quick_object_operation_schedule (gpointer self);
G_GNUC_INTERNAL void _gum_quick_object_operation_schedule_when_idle (
    gpointer self, GPtrArray * dependencies);
G_GNUC_INTERNAL void _gum_quick_object_operation_finish (
    GumQuickObjectOperation * self);

#define _gum_quick_module_operation_new(type, module, callback, perform, \
    dispose) _gum_quick_module_operation_alloc (sizeof (type), module, \
        &(module)->objects, callback, (GumQuickModuleOperationFunc) perform, \
        (GumQuickModuleOperationFunc) dispose)
G_GNUC_INTERNAL gpointer _gum_quick_module_operation_alloc (gsize size,
    gpointer module, GumQuickObjectManager * manager, JSValue callback,
    GumQuickModuleOperationFunc perform, GumQuickModuleOperationFunc dispose);
G_GNUC_INTERNAL void _gum_quick_module_operation_schedule (gpointer self);
G_GNUC_INTERNAL void _gum_quick_module_operation_finish (
    GumQuickModuleOperation * self);

G_END_DECLS

#endif
