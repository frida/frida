/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminstancetracker.h"

#include "guminterceptor.h"
#include "gumprocess.h"

typedef enum _FunctionId FunctionId;

struct _GumInstanceTracker
{
  GObject parent;

  gboolean disposed;

  GMutex mutex;
  GHashTable * counter_ht;
  GHashTable * instances_ht;
  GumInterceptor * interceptor;

  gboolean is_active;
  GumInstanceVTable vtable;

  GumFilterInstanceTypeFunc type_filter_func;
  gpointer type_filter_func_user_data;
};

enum _FunctionId
{
  FUNCTION_ID_CREATE_INSTANCE,
  FUNCTION_ID_FREE_INSTANCE
};

#define GUM_INSTANCE_TRACKER_LOCK() g_mutex_lock (&self->mutex)
#define GUM_INSTANCE_TRACKER_UNLOCK() g_mutex_unlock (&self->mutex)

#define COUNTER_TABLE_GET(gtype) GPOINTER_TO_UINT (g_hash_table_lookup (\
    self->counter_ht, GUINT_TO_POINTER (gtype)))
#define COUNTER_TABLE_SET(gtype, count) g_hash_table_insert (\
    self->counter_ht, GUINT_TO_POINTER (gtype), GUINT_TO_POINTER (count))

static void gum_instance_tracker_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_instance_tracker_dispose (GObject * object);
static void gum_instance_tracker_finalize (GObject * object);

static void gum_instance_tracker_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void gum_instance_tracker_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);

G_DEFINE_TYPE_EXTENDED (GumInstanceTracker,
                        gum_instance_tracker,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_instance_tracker_listener_iface_init))

static void
gum_instance_tracker_class_init (GumInstanceTrackerClass * klass)
{
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gum_instance_tracker_dispose;
  gobject_class->finalize = gum_instance_tracker_finalize;
}

static void
gum_instance_tracker_listener_iface_init (gpointer g_iface,
                                          gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_instance_tracker_on_enter;
  iface->on_leave = gum_instance_tracker_on_leave;
}

static void
gum_instance_tracker_init (GumInstanceTracker * self)
{
  g_mutex_init (&self->mutex);

  self->counter_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, NULL);
  g_assert (self->counter_ht != NULL);

  self->instances_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, NULL);

  self->interceptor = gum_interceptor_obtain ();
}

static void
gum_instance_tracker_dispose (GObject * object)
{
  GumInstanceTracker * self = GUM_INSTANCE_TRACKER (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    if (self->is_active)
      gum_instance_tracker_end (self);

    g_object_unref (self->interceptor);

    g_hash_table_unref (self->counter_ht);
    self->counter_ht = NULL;

    g_hash_table_unref (self->instances_ht);
    self->instances_ht = NULL;
  }

  G_OBJECT_CLASS (gum_instance_tracker_parent_class)->dispose (object);
}

static void
gum_instance_tracker_finalize (GObject * object)
{
  GumInstanceTracker * self = GUM_INSTANCE_TRACKER (object);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_instance_tracker_parent_class)->finalize (object);
}

GumInstanceTracker *
gum_instance_tracker_new (void)
{
  return g_object_new (GUM_TYPE_INSTANCE_TRACKER, NULL);
}

static gboolean
gum_instance_tracker_fill_vtable_if_module_is_gobject (
    GumModule * module,
    gpointer user_data)
{
  GumInstanceTracker * self;
  GumInstanceVTable * vtable;
  gchar * name_lowercase;

  self = GUM_INSTANCE_TRACKER (user_data);
  vtable = &self->vtable;

  name_lowercase = g_ascii_strdown (gum_module_get_name (module), -1);

  if (g_strstr_len (name_lowercase, -1, "gobject-2.0") != NULL)
  {
#define GUM_ASSIGN(type, field, name) \
    vtable->field = GUM_POINTER_TO_FUNCPTR (type, \
        gum_module_find_export_by_name (module, G_STRINGIFY (name)))

    GUM_ASSIGN (GumCreateInstanceFunc, create_instance, g_type_create_instance);
    GUM_ASSIGN (GumFreeInstanceFunc, free_instance, g_type_free_instance);
    GUM_ASSIGN (GumTypeIdToNameFunc, type_id_to_name, g_type_name);

#undef GUM_ASSIGN
  }

  g_free (name_lowercase);

  return TRUE;
}

void
gum_instance_tracker_begin (GumInstanceTracker * self,
                            GumInstanceVTable * vtable)
{
  GumAttachOptions options = { 0, };

  g_assert (!self->is_active);

  if (vtable != NULL)
  {
    self->vtable = *vtable;
  }
  else
  {
    gum_process_enumerate_modules (
        gum_instance_tracker_fill_vtable_if_module_is_gobject, self);

    if (self->vtable.create_instance == NULL)
    {
      self->vtable.create_instance = g_type_create_instance;
      self->vtable.free_instance = g_type_free_instance;
      self->vtable.type_id_to_name = g_type_name;
    }
  }

  gum_interceptor_begin_transaction (self->interceptor);

  options.listener_function_data =
      GUINT_TO_POINTER (FUNCTION_ID_CREATE_INSTANCE);
  gum_interceptor_attach (self->interceptor,
      GUM_FUNCPTR_TO_POINTER (self->vtable.create_instance),
      GUM_INVOCATION_LISTENER (self), &options);

  options.listener_function_data =
      GUINT_TO_POINTER (FUNCTION_ID_FREE_INSTANCE);
  gum_interceptor_attach (self->interceptor,
      GUM_FUNCPTR_TO_POINTER (self->vtable.free_instance),
      GUM_INVOCATION_LISTENER (self), &options);

  gum_interceptor_end_transaction (self->interceptor);

  self->is_active = TRUE;
}

void
gum_instance_tracker_end (GumInstanceTracker * self)
{
  g_assert (self->is_active);

  gum_interceptor_detach (self->interceptor, GUM_INVOCATION_LISTENER (self));

  self->is_active = FALSE;
}

const GumInstanceVTable *
gum_instance_tracker_get_current_vtable (GumInstanceTracker * self)
{
  return &self->vtable;
}

void
gum_instance_tracker_set_type_filter_function (GumInstanceTracker * self,
                                               GumFilterInstanceTypeFunc filter,
                                               gpointer user_data)
{
  self->type_filter_func = filter;
  self->type_filter_func_user_data = user_data;
}

guint
gum_instance_tracker_peek_total_count (GumInstanceTracker * self,
                                       const gchar * type_name)
{
  guint result = 0;

  if (type_name != NULL)
  {
    GType gtype = g_type_from_name (type_name);

    if (gtype != 0)
    {
      GUM_INSTANCE_TRACKER_LOCK ();
      result = COUNTER_TABLE_GET (gtype);
      GUM_INSTANCE_TRACKER_UNLOCK ();
    }
  }
  else
  {
    GUM_INSTANCE_TRACKER_LOCK ();
    result = g_hash_table_size (self->instances_ht);
    GUM_INSTANCE_TRACKER_UNLOCK ();
  }

  return result;
}

GList *
gum_instance_tracker_peek_instances (GumInstanceTracker * self)
{
  GList * result;

  GUM_INSTANCE_TRACKER_LOCK ();
  result = g_hash_table_get_keys (self->instances_ht);
  GUM_INSTANCE_TRACKER_UNLOCK ();

  return result;
}

void
gum_instance_tracker_walk_instances (GumInstanceTracker * self,
                                     GumWalkInstanceFunc func,
                                     gpointer user_data)
{
  GHashTableIter iter;
  gpointer key, value;
  GType gobject_type;

  gobject_type = G_TYPE_OBJECT;

  GUM_INSTANCE_TRACKER_LOCK ();

  g_hash_table_iter_init (&iter, self->instances_ht);
  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    const GTypeInstance * instance = (const GTypeInstance *) key;
    GType type;
    GumInstanceDetails details;

    type = G_TYPE_FROM_INSTANCE (instance);

    details.address = instance;
    if (g_type_is_a (type, gobject_type))
      details.ref_count = ((const GObject *) instance)->ref_count;
    else
      details.ref_count = 1;
    details.type_name = self->vtable.type_id_to_name (type);

    func (&details, user_data);
  }

  GUM_INSTANCE_TRACKER_UNLOCK ();
}

void
gum_instance_tracker_add_instance (GumInstanceTracker * self,
                                   gpointer instance,
                                   GType instance_type)
{
  guint count;

  if (instance_type == G_TYPE_FROM_INSTANCE (self))
    return;

  if (self->type_filter_func != NULL)
  {
    if (!self->type_filter_func (self, instance_type,
        self->type_filter_func_user_data))
    {
      return;
    }
  }

  GUM_INSTANCE_TRACKER_LOCK ();

  g_assert (g_hash_table_lookup (self->instances_ht, instance) == NULL);
  g_hash_table_add (self->instances_ht, instance);

  count = COUNTER_TABLE_GET (instance_type);
  COUNTER_TABLE_SET (instance_type, count + 1);

  GUM_INSTANCE_TRACKER_UNLOCK ();
}

void
gum_instance_tracker_remove_instance (GumInstanceTracker * self,
                                      gpointer instance,
                                      GType instance_type)
{
  guint count;

  GUM_INSTANCE_TRACKER_LOCK ();

  if (g_hash_table_remove (self->instances_ht, instance))
  {
    count = COUNTER_TABLE_GET (instance_type);
    if (count > 0)
      COUNTER_TABLE_SET (instance_type, count - 1);
  }

  GUM_INSTANCE_TRACKER_UNLOCK ();
}

static void
gum_instance_tracker_on_enter (GumInvocationListener * listener,
                               GumInvocationContext * context)
{
  GumInstanceTracker * self;
  FunctionId function_id;

  self = GUM_INSTANCE_TRACKER (listener);
  function_id = GPOINTER_TO_INT (
      gum_invocation_context_get_listener_function_data (context));

  if (function_id == FUNCTION_ID_FREE_INSTANCE)
  {
    GTypeInstance * instance;
    GType gtype;

    instance = (GTypeInstance *)
        gum_invocation_context_get_nth_argument (context, 0);
    gtype = G_TYPE_FROM_INSTANCE (instance);

    gum_instance_tracker_remove_instance (self, instance, gtype);
  }
}

static void
gum_instance_tracker_on_leave (GumInvocationListener * listener,
                               GumInvocationContext * context)
{
  GumInstanceTracker * self;
  FunctionId function_id;

  self = GUM_INSTANCE_TRACKER (listener);
  function_id = GPOINTER_TO_INT (
      gum_invocation_context_get_listener_function_data (context));

  if (function_id == FUNCTION_ID_CREATE_INSTANCE)
  {
    GTypeInstance * instance;
    GType gtype;

    instance = (GTypeInstance *)
        gum_invocation_context_get_return_value (context);
    gtype = G_TYPE_FROM_INSTANCE (instance);

    gum_instance_tracker_add_instance (self, instance, gtype);
  }
}
