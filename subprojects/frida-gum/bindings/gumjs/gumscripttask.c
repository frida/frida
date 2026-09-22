/*
 * Copyright (C) 2015-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumscripttask.h"

struct _GumScriptTask
{
  GObject parent;

  gboolean disposed;

  GumScriptTaskFunc func;
  gpointer source_object;
  gpointer source_tag;
  GCancellable * cancellable;
  GAsyncReadyCallback callback;
  gpointer callback_data;
  gpointer task_data;
  GDestroyNotify task_data_destroy;

  GMainContext * context;

  gboolean synchronous;
  GMutex mutex;
  GCond cond;

  volatile gboolean completed;
  gpointer result;
  GDestroyNotify result_destroy;
  GError * error;
};

static void gum_script_task_iface_init (GAsyncResultIface * iface);
static void gum_script_task_dispose (GObject * obj);
static void gum_script_task_finalize (GObject * obj);

static gpointer gum_script_task_get_user_data (GAsyncResult * res);
static GObject * gum_script_task_ref_source_object (GAsyncResult * res);
static gboolean gum_script_task_is_tagged (GAsyncResult * res,
    gpointer source_tag);

static void gum_script_task_return (GumScriptTask * self);

static gboolean gum_script_task_propagate_error (GumScriptTask * self,
    GError ** error);

static void gum_script_task_run (GumScriptTask * self);
static gboolean gum_script_task_complete (GumScriptTask * self);

G_DEFINE_TYPE_EXTENDED (GumScriptTask,
                        gum_script_task,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT,
                            gum_script_task_iface_init))

static void
gum_script_task_class_init (GumScriptTaskClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_script_task_dispose;
  object_class->finalize = gum_script_task_finalize;
}

static void
gum_script_task_iface_init (GAsyncResultIface * iface)
{
  iface->get_user_data = gum_script_task_get_user_data;
  iface->get_source_object = gum_script_task_ref_source_object;
  iface->is_tagged = gum_script_task_is_tagged;
}

static void
gum_script_task_init (GumScriptTask * self)
{
}

static void
gum_script_task_dispose (GObject * obj)
{
  GumScriptTask * self = GUM_SCRIPT_TASK (obj);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    g_main_context_unref (self->context);
    self->context = NULL;

    g_clear_object (&self->cancellable);

    g_clear_object (&self->source_object);
  }

  G_OBJECT_CLASS (gum_script_task_parent_class)->dispose (obj);
}

static void
gum_script_task_finalize (GObject * obj)
{
  GumScriptTask * self = GUM_SCRIPT_TASK (obj);

  if (self->error != NULL)
    g_error_free (self->error);

  if (self->result_destroy != NULL)
    self->result_destroy (self->result);

  if (self->task_data_destroy != NULL)
    self->task_data_destroy (self->task_data);

  G_OBJECT_CLASS (gum_script_task_parent_class)->finalize (obj);
}

GumScriptTask *
gum_script_task_new (GumScriptTaskFunc func,
                     gpointer source_object,
                     GCancellable * cancellable,
                     GAsyncReadyCallback callback,
                     gpointer callback_data)
{
  GumScriptTask * task;

  task = g_object_new (GUM_TYPE_SCRIPT_TASK, NULL);

  task->func = func;
  task->source_object =
      (source_object != NULL) ? g_object_ref (source_object) : NULL;
  task->cancellable =
      (cancellable != NULL) ? g_object_ref (cancellable) : NULL;
  task->callback = callback;
  task->callback_data = callback_data;

  task->context = g_main_context_ref_thread_default ();

  return task;
}

static gpointer
gum_script_task_get_user_data (GAsyncResult * res)
{
  return GUM_SCRIPT_TASK (res)->callback_data;
}

static GObject *
gum_script_task_ref_source_object (GAsyncResult * res)
{
  GumScriptTask * self = GUM_SCRIPT_TASK (res);

  if (self->source_object == NULL)
    return NULL;

  return g_object_ref (self->source_object);
}

static gboolean
gum_script_task_is_tagged (GAsyncResult * res,
                           gpointer source_tag)
{
  return GUM_SCRIPT_TASK (res)->source_tag == source_tag;
}

gpointer
gum_script_task_get_source_object (GumScriptTask * self)
{
  return self->source_object;
}

gpointer
gum_script_task_get_source_tag (GumScriptTask * self)
{
  return self->source_tag;
}

void
gum_script_task_set_source_tag (GumScriptTask * self,
                                gpointer source_tag)
{
  self->source_tag = source_tag;
}

GMainContext *
gum_script_task_get_context (GumScriptTask * self)
{
  return self->context;
}

void
gum_script_task_set_task_data (GumScriptTask * self,
                               gpointer task_data,
                               GDestroyNotify task_data_destroy)
{
  self->task_data = task_data;
  self->task_data_destroy = task_data_destroy;
}

void
gum_script_task_return_pointer (GumScriptTask * self,
                                gpointer result,
                                GDestroyNotify result_destroy)
{
  self->result = result;
  self->result_destroy = result_destroy;

  gum_script_task_return (self);
}

void
gum_script_task_return_error (GumScriptTask * self,
                              GError * error)
{
  self->error = error;

  gum_script_task_return (self);
}

static void
gum_script_task_return (GumScriptTask * self)
{
  if (self->synchronous)
  {
    g_mutex_lock (&self->mutex);
    self->completed = TRUE;
    g_cond_signal (&self->cond);
    g_mutex_unlock (&self->mutex);
  }
  else
  {
    GSource * source;

    source = g_idle_source_new ();
    g_source_set_callback (source, (GSourceFunc) gum_script_task_complete,
        g_object_ref (self), g_object_unref);
    g_source_attach (source, self->context);
    g_source_unref (source);
  }
}

gpointer
gum_script_task_propagate_pointer (GumScriptTask * self,
                                   GError ** error)
{
  if (gum_script_task_propagate_error (self, error))
    return NULL;

  self->result_destroy = NULL;

  return self->result;
}

static gboolean
gum_script_task_propagate_error (GumScriptTask * self,
                                 GError ** error)
{
  if (g_cancellable_set_error_if_cancelled (self->cancellable, error))
    return TRUE;

  if (self->error != NULL)
  {
    g_propagate_error (error, self->error);
    self->error = NULL;
    return TRUE;
  }

  return FALSE;
}

void
gum_script_task_run_in_js_thread (GumScriptTask * self,
                                  GumScriptScheduler * scheduler)
{
  gum_script_scheduler_push_job_on_js_thread (scheduler, G_PRIORITY_DEFAULT,
      (GumScriptJobFunc) gum_script_task_run, g_object_ref (self),
      g_object_unref);
}

void
gum_script_task_run_in_js_thread_sync (GumScriptTask * self,
                                       GumScriptScheduler * scheduler)
{
  self->synchronous = TRUE;

  g_mutex_init (&self->mutex);
  g_cond_init (&self->cond);

  gum_script_scheduler_push_job_on_js_thread (scheduler, G_PRIORITY_DEFAULT,
      (GumScriptJobFunc) gum_script_task_run, g_object_ref (self),
      g_object_unref);

  g_mutex_lock (&self->mutex);
  while (!self->completed)
    g_cond_wait (&self->cond, &self->mutex);
  g_mutex_unlock (&self->mutex);

  g_cond_clear (&self->cond);
  g_mutex_clear (&self->mutex);
}

static void
gum_script_task_run (GumScriptTask * self)
{
  if (self->cancellable == NULL ||
      !g_cancellable_is_cancelled (self->cancellable))
  {
    self->func (self, self->source_object, self->task_data, self->cancellable);
  }
}

static gboolean
gum_script_task_complete (GumScriptTask * self)
{
  if (self->callback != NULL)
  {
    self->callback (self->source_object, G_ASYNC_RESULT (self),
        self->callback_data);
  }

  return FALSE;
}
