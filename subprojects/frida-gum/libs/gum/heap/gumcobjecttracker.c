/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcobjecttracker.h"

#include "gumcobject.h"
#include "guminterceptor.h"

#include <stdlib.h>
#include <string.h>

#define GUM_COBJECT_TRACKER_CAST(o) ((GumCObjectTracker *) (o))

typedef struct _ObjectType             ObjectType;
typedef struct _CObjectFunctionContext CObjectFunctionContext;
typedef struct _CObjectThreadContext   CObjectThreadContext;
typedef struct _CObjectHandlers        CObjectHandlers;

typedef void (* CObjectEnterHandler) (GumCObjectTracker * self,
    gpointer handler_context, CObjectThreadContext * thread_context,
    GumInvocationContext * invocation_context);
typedef void (* CObjectLeaveHandler) (GumCObjectTracker * self,
    gpointer handler_context, CObjectThreadContext * thread_context,
    GumInvocationContext * invocation_context);

struct _GumCObjectTracker
{
  GObject parent;

  gboolean disposed;

  GMutex mutex;
  GHashTable * types_ht;
  GHashTable * objects_ht;
  GumInterceptor * interceptor;
  GPtrArray * function_contexts;

  GumBacktracerInterface * backtracer_iface;
  GumBacktracer * backtracer_instance;
};

enum
{
  PROP_0,
  PROP_BACKTRACER,
};

struct _ObjectType
{
  gchar * name;
  guint count;
};

struct _CObjectHandlers
{
  CObjectEnterHandler enter_handler;
  CObjectLeaveHandler leave_handler;
};

struct _CObjectThreadContext
{
  gpointer data;
};

struct _CObjectFunctionContext
{
  CObjectHandlers handlers;
  gpointer context;
};

#define GUM_COBJECT_TRACKER_LOCK() g_mutex_lock (&self->mutex)
#define GUM_COBJECT_TRACKER_UNLOCK() g_mutex_unlock (&self->mutex)

static void gum_cobject_tracker_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_cobject_tracker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gum_cobject_tracker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_cobject_tracker_dispose (GObject * object);
static void gum_cobject_tracker_finalize (GObject * object);

static ObjectType * object_type_new (const gchar * name);
static void object_type_free (ObjectType * t);

static void gum_cobject_tracker_add_object (GumCObjectTracker * self,
    GumCObject * cobject);
static void gum_cobject_tracker_maybe_remove_object (GumCObjectTracker * self,
    gpointer address);

static void gum_cobject_tracker_attach_to_function (GumCObjectTracker * self,
    gpointer function_address, const CObjectHandlers * handlers,
    gpointer context);

static void gum_cobject_tracker_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void gum_cobject_tracker_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);

static void on_constructor_enter_handler (GumCObjectTracker * self,
    ObjectType * object_type, CObjectThreadContext * thread_context,
    GumInvocationContext * invocation_context);
static void on_constructor_leave_handler (GumCObjectTracker * self,
    ObjectType * object_type, CObjectThreadContext * thread_context,
    GumInvocationContext * invocation_context);
static void on_free_enter_handler (GumCObjectTracker * self,
    gpointer handler_context, CObjectThreadContext * thread_context,
    GumInvocationContext * invocation_context);
static void on_g_slice_free1_enter_handler (GumCObjectTracker * self,
    gpointer handler_context, CObjectThreadContext * thread_context,
    GumInvocationContext * invocation_context);

G_DEFINE_TYPE_EXTENDED (GumCObjectTracker,
                        gum_cobject_tracker,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_cobject_tracker_listener_iface_init))

static void
gum_cobject_tracker_class_init (GumCObjectTrackerClass * klass)
{
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec * pspec;

  gobject_class->set_property = gum_cobject_tracker_set_property;
  gobject_class->get_property = gum_cobject_tracker_get_property;
  gobject_class->dispose = gum_cobject_tracker_dispose;
  gobject_class->finalize = gum_cobject_tracker_finalize;

  pspec = g_param_spec_object ("backtracer", "Backtracer",
      "Backtracer Implementation", GUM_TYPE_BACKTRACER,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
      G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_BACKTRACER, pspec);
}

static void
gum_cobject_tracker_listener_iface_init (gpointer g_iface,
                                         gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_cobject_tracker_on_enter;
  iface->on_leave = gum_cobject_tracker_on_leave;
}

static const CObjectHandlers free_cobject_handlers =
{
  on_free_enter_handler, NULL
};

static const CObjectHandlers g_slice_free1_cobject_handlers =
{
  on_g_slice_free1_enter_handler, NULL
};

static void
gum_cobject_tracker_init (GumCObjectTracker * self)
{
  g_mutex_init (&self->mutex);

  self->types_ht = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) object_type_free);

  self->objects_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) gum_cobject_free);

  self->interceptor = gum_interceptor_obtain ();

  self->function_contexts = g_ptr_array_new ();

  gum_interceptor_begin_transaction (self->interceptor);

  gum_cobject_tracker_attach_to_function (self,
      GUM_FUNCPTR_TO_POINTER (free),
      &free_cobject_handlers, NULL);
  gum_cobject_tracker_attach_to_function (self,
      GUM_FUNCPTR_TO_POINTER (g_slice_free1),
      &g_slice_free1_cobject_handlers, NULL);

  gum_interceptor_end_transaction (self->interceptor);
}

static void
gum_cobject_tracker_set_property (GObject * object,
                                  guint property_id,
                                  const GValue * value,
                                  GParamSpec * pspec)
{
  GumCObjectTracker * self = GUM_COBJECT_TRACKER (object);

  switch (property_id)
  {
    case PROP_BACKTRACER:
      if (self->backtracer_instance != NULL)
        g_object_unref (self->backtracer_instance);
      self->backtracer_instance = g_value_dup_object (value);

      if (self->backtracer_instance != NULL)
      {
        self->backtracer_iface =
            GUM_BACKTRACER_GET_IFACE (self->backtracer_instance);
      }
      else
      {
        self->backtracer_iface = NULL;
      }

      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_cobject_tracker_get_property (GObject * object,
                                  guint property_id,
                                  GValue * value,
                                  GParamSpec * pspec)
{
  GumCObjectTracker * self = GUM_COBJECT_TRACKER (object);

  switch (property_id)
  {
    case PROP_BACKTRACER:
      g_value_set_object (value, self->backtracer_instance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_cobject_tracker_dispose (GObject * object)
{
  GumCObjectTracker * self = GUM_COBJECT_TRACKER (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    gum_interceptor_detach (self->interceptor, GUM_INVOCATION_LISTENER (self));
    g_object_unref (self->interceptor);
    self->interceptor = NULL;

    g_clear_object (&self->backtracer_instance);
    self->backtracer_iface = NULL;

    g_hash_table_unref (self->objects_ht);
    self->objects_ht = NULL;

    g_hash_table_unref (self->types_ht);
    self->types_ht = NULL;
  }

  G_OBJECT_CLASS (gum_cobject_tracker_parent_class)->dispose (object);
}

static void
gum_cobject_tracker_finalize (GObject * object)
{
  GumCObjectTracker * self = GUM_COBJECT_TRACKER (object);

  g_ptr_array_foreach (self->function_contexts, (GFunc) g_free, NULL);
  g_ptr_array_free (self->function_contexts, TRUE);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_cobject_tracker_parent_class)->finalize (object);
}

GumCObjectTracker *
gum_cobject_tracker_new (void)
{
  return g_object_new (GUM_TYPE_COBJECT_TRACKER, NULL);
}

GumCObjectTracker *
gum_cobject_tracker_new_with_backtracer (GumBacktracer * backtracer)
{
  return g_object_new (GUM_TYPE_COBJECT_TRACKER,
      "backtracer", backtracer,
      NULL);
}

static const CObjectHandlers object_type_cobject_handlers =
{
  (CObjectEnterHandler) on_constructor_enter_handler,
  (CObjectLeaveHandler) on_constructor_leave_handler
};

void
gum_cobject_tracker_track (GumCObjectTracker * self,
                           const gchar * type_name,
                           gpointer type_constructor)
{
  ObjectType * t;

  g_assert (strlen (type_name) <= GUM_MAX_TYPE_NAME);

  t = object_type_new (type_name);
  g_hash_table_insert (self->types_ht, g_strdup (type_name), t);

  gum_cobject_tracker_attach_to_function (self, type_constructor,
      &object_type_cobject_handlers, t);
}

void
gum_cobject_tracker_begin (GumCObjectTracker * self)
{
}

void
gum_cobject_tracker_end (GumCObjectTracker * self)
{
}

guint
gum_cobject_tracker_peek_total_count (GumCObjectTracker * self,
                                      const gchar * type_name)
{
  guint result;

  GUM_COBJECT_TRACKER_LOCK ();
  gum_interceptor_ignore_current_thread (self->interceptor);

  if (type_name != NULL)
  {
    ObjectType * object_type;

    object_type = g_hash_table_lookup (self->types_ht, type_name);
    g_assert (object_type != NULL);

    result = object_type->count;
  }
  else
  {
    result = g_hash_table_size (self->objects_ht);
  }

  gum_interceptor_unignore_current_thread (self->interceptor);
  GUM_COBJECT_TRACKER_UNLOCK ();

  return result;
}

GList *
gum_cobject_tracker_peek_object_list (GumCObjectTracker * self)
{
  GList * result = NULL, * cur;

  GUM_COBJECT_TRACKER_LOCK ();
  gum_interceptor_ignore_current_thread (self->interceptor);

  result = g_hash_table_get_values (self->objects_ht);
  for (cur = result; cur != NULL; cur = cur->next)
  {
    cur->data = gum_cobject_copy ((GumCObject *) cur->data);
  }

  gum_interceptor_unignore_current_thread (self->interceptor);
  GUM_COBJECT_TRACKER_UNLOCK ();

  return result;
}

static ObjectType *
object_type_new (const gchar * name)
{
  ObjectType * t;

  t = g_new0 (ObjectType, 1);
  t->name = g_strdup (name);

  return t;
}

static void
object_type_free (ObjectType * t)
{
  g_free (t->name);
  g_free (t);
}

static void
gum_cobject_tracker_add_object (GumCObjectTracker * self,
                                GumCObject * cobject)
{
  ObjectType * object_type = cobject->data;

  GUM_COBJECT_TRACKER_LOCK ();

  g_hash_table_insert (self->objects_ht, cobject->address, cobject);
  object_type->count++;

  GUM_COBJECT_TRACKER_UNLOCK ();
}

static void
gum_cobject_tracker_maybe_remove_object (GumCObjectTracker * self,
                                         gpointer address)
{
  GumCObject * cobject;

  GUM_COBJECT_TRACKER_LOCK ();

  cobject = g_hash_table_lookup (self->objects_ht, address);
  if (cobject != NULL)
  {
    ObjectType * object_type = cobject->data;
    object_type->count--;
    g_hash_table_remove (self->objects_ht, address);
  }

  GUM_COBJECT_TRACKER_UNLOCK ();
}

static void
gum_cobject_tracker_attach_to_function (GumCObjectTracker * self,
                                        gpointer function_address,
                                        const CObjectHandlers * handlers,
                                        gpointer context)
{
  CObjectFunctionContext * function_ctx;
  GumAttachOptions options = { 0, };

  function_ctx = g_new (CObjectFunctionContext, 1);
  function_ctx->handlers = *handlers;
  function_ctx->context = context;
  g_ptr_array_add (self->function_contexts, function_ctx);

  options.listener_function_data = function_ctx;
  gum_interceptor_attach (self->interceptor, function_address,
      GUM_INVOCATION_LISTENER (self), &options);
}

static void
gum_cobject_tracker_on_enter (GumInvocationListener * listener,
                              GumInvocationContext * context)
{
  GumCObjectTracker * self;
  CObjectFunctionContext * function_ctx;

  self = GUM_COBJECT_TRACKER_CAST (listener);
  function_ctx = GUM_IC_GET_FUNC_DATA (context, CObjectFunctionContext *);

  if (function_ctx->handlers.enter_handler != NULL)
  {
    function_ctx->handlers.enter_handler (self, function_ctx->context,
        GUM_IC_GET_INVOCATION_DATA (context, CObjectThreadContext), context);
  }
}

static void
gum_cobject_tracker_on_leave (GumInvocationListener * listener,
                              GumInvocationContext * context)
{
  GumCObjectTracker * self;
  CObjectFunctionContext * function_ctx;

  self = GUM_COBJECT_TRACKER_CAST (listener);
  function_ctx = GUM_IC_GET_FUNC_DATA (context, CObjectFunctionContext *);

  if (function_ctx->handlers.leave_handler != NULL)
  {
    function_ctx->handlers.leave_handler (self, function_ctx->context,
        GUM_IC_GET_INVOCATION_DATA (context, CObjectThreadContext), context);
  }
}

static void
on_constructor_enter_handler (GumCObjectTracker * self,
                              ObjectType * object_type,
                              CObjectThreadContext * thread_context,
                              GumInvocationContext * invocation_context)
{
  GumCObject * cobject;

  cobject = gum_cobject_new (NULL, object_type->name);
  cobject->data = object_type;

  if (self->backtracer_instance != NULL)
  {
    self->backtracer_iface->generate (self->backtracer_instance,
        invocation_context->cpu_context, &cobject->return_addresses,
        GUM_MAX_BACKTRACE_DEPTH);
  }

  thread_context->data = cobject;
}

static void
on_constructor_leave_handler (GumCObjectTracker * self,
                              ObjectType * object_type,
                              CObjectThreadContext * thread_context,
                              GumInvocationContext * invocation_context)
{
  GumCObject * cobject = (GumCObject *) thread_context->data;

  cobject->address =
      gum_invocation_context_get_return_value (invocation_context);
  gum_cobject_tracker_add_object (self, cobject);
}

static void
on_free_enter_handler (GumCObjectTracker * self,
                       gpointer handler_context,
                       CObjectThreadContext * thread_context,
                       GumInvocationContext * invocation_context)
{
  gpointer address;

  address = gum_invocation_context_get_nth_argument (invocation_context, 0);

  gum_cobject_tracker_maybe_remove_object (self, address);
}

static void
on_g_slice_free1_enter_handler (GumCObjectTracker * self,
                                gpointer handler_context,
                                CObjectThreadContext * thread_context,
                                GumInvocationContext * invocation_context)
{
  gpointer mem_block;

  mem_block = gum_invocation_context_get_nth_argument (invocation_context, 1);

  gum_cobject_tracker_maybe_remove_object (self, mem_block);
}
