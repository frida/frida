/*
 * Copyright (C) 2015-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumscriptscheduler.h"

struct _GumScriptScheduler
{
  GObject parent;

  gboolean disposed;

  gboolean enable_background_thread;
  GThread * js_thread;
  GMainLoop * js_loop;
  GMainContext * js_context;
  volatile gint start_request_seqno;

  GThreadPool * thread_pool;
};

struct _GumScriptJob
{
  GumScriptJobFunc func;
  gpointer data;
  GDestroyNotify data_destroy;

  GumScriptScheduler * scheduler;
};

static void gum_script_scheduler_dispose (GObject * obj);

static gboolean gum_script_scheduler_perform_js_job (
    GumScriptJob * job);
static void gum_script_scheduler_perform_pool_job (GumScriptJob * job,
    GumScriptScheduler * self);

static gpointer gum_script_scheduler_run_js_loop (GumScriptScheduler * self);

G_DEFINE_TYPE (GumScriptScheduler, gum_script_scheduler, G_TYPE_OBJECT)

static void
gum_script_scheduler_class_init (GumScriptSchedulerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_script_scheduler_dispose;
}

static void
gum_script_scheduler_init (GumScriptScheduler * self)
{
  self->enable_background_thread = TRUE;

  self->js_context = g_main_context_new ();
}

static void
gum_script_scheduler_dispose (GObject * obj)
{
  GumScriptScheduler * self = GUM_SCRIPT_SCHEDULER (obj);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    if (self->thread_pool != NULL)
    {
      g_thread_pool_free (self->thread_pool, FALSE, TRUE);
      self->thread_pool = NULL;
    }

    gum_script_scheduler_stop (self);

    g_main_context_unref (self->js_context);
    self->js_context = NULL;
  }

  G_OBJECT_CLASS (gum_script_scheduler_parent_class)->dispose (obj);
}

GumScriptScheduler *
gum_script_scheduler_new (void)
{
  return g_object_new (GUM_TYPE_SCRIPT_SCHEDULER, NULL);
}

void
gum_script_scheduler_enable_background_thread (GumScriptScheduler * self)
{
  self->enable_background_thread = TRUE;

  gum_script_scheduler_start (self);
}

void
gum_script_scheduler_disable_background_thread (GumScriptScheduler * self)
{
  gum_script_scheduler_stop (self);

  self->enable_background_thread = FALSE;
}

void
gum_script_scheduler_start (GumScriptScheduler * self)
{
  if (self->disposed)
    return;

  if (self->enable_background_thread && self->js_thread == NULL &&
      g_atomic_int_add (&self->start_request_seqno, 1) == 0)
  {
    self->js_loop = g_main_loop_new (self->js_context, TRUE);

    self->js_thread = g_thread_new ("gum-js-loop",
        (GThreadFunc) gum_script_scheduler_run_js_loop, self);
  }
}

void
gum_script_scheduler_stop (GumScriptScheduler * self)
{
  if (self->js_thread != NULL)
  {
    gum_script_scheduler_push_job_on_js_thread (self, G_PRIORITY_LOW,
        (GumScriptJobFunc) g_main_loop_quit, self->js_loop, NULL);
    g_thread_join (self->js_thread);
    self->js_thread = NULL;

    g_main_loop_unref (self->js_loop);
    self->js_loop = NULL;

    g_atomic_int_set (&self->start_request_seqno, 0);
  }
}

GMainContext *
gum_script_scheduler_get_js_context (GumScriptScheduler * self)
{
  return self->js_context;
}

void
gum_script_scheduler_push_job_on_js_thread (GumScriptScheduler * self,
                                            gint priority,
                                            GumScriptJobFunc func,
                                            gpointer data,
                                            GDestroyNotify data_destroy)
{
  GumScriptJob * job;
  GSource * source;

  job = gum_script_job_new (self, func, data, data_destroy);

  source = g_idle_source_new ();
  g_source_set_priority (source, priority);
  g_source_set_callback (source,
      (GSourceFunc) gum_script_scheduler_perform_js_job,
      job,
      (GDestroyNotify) gum_script_job_free);
  g_source_attach (source, self->js_context);
  g_source_unref (source);

  gum_script_scheduler_start (self);
}

void
gum_script_scheduler_push_job_on_thread_pool (GumScriptScheduler * self,
                                              GumScriptJobFunc func,
                                              gpointer data,
                                              GDestroyNotify data_destroy)
{
  if (self->thread_pool == NULL)
  {
    self->thread_pool = g_thread_pool_new (
        (GFunc) gum_script_scheduler_perform_pool_job,
        self,
        4,
        FALSE,
        NULL);
  }

  g_thread_pool_push (self->thread_pool,
      gum_script_job_new (self, func, data, data_destroy),
      NULL);
}

static gboolean
gum_script_scheduler_perform_js_job (GumScriptJob * job)
{
  job->func (job->data);

  return FALSE;
}

static void
gum_script_scheduler_perform_pool_job (GumScriptJob * job,
                                       GumScriptScheduler * self)
{
  job->func (job->data);

  gum_script_job_free (job);
}

static gpointer
gum_script_scheduler_run_js_loop (GumScriptScheduler * self)
{
  g_main_context_push_thread_default (self->js_context);
  g_main_loop_run (self->js_loop);
  g_main_context_pop_thread_default (self->js_context);

  return NULL;
}

GumScriptJob *
gum_script_job_new (GumScriptScheduler * scheduler,
                    GumScriptJobFunc func,
                    gpointer data,
                    GDestroyNotify data_destroy)
{
  GumScriptJob * job;

  job = g_slice_new (GumScriptJob);
  job->func = func;
  job->data = data;
  job->data_destroy = data_destroy;

  job->scheduler = scheduler;

  return job;
}

void
gum_script_job_free (GumScriptJob * job)
{
  if (job->data_destroy != NULL)
    job->data_destroy (job->data);

  g_slice_free (GumScriptJob, job);
}

void
gum_script_job_start_on_js_thread (GumScriptJob * job)
{
  GMainContext * js_context = job->scheduler->js_context;

  if (g_main_context_is_owner (js_context))
  {
    job->func (job->data);
  }
  else
  {
    GSource * source;

    source = g_idle_source_new ();
    g_source_set_callback (source,
        (GSourceFunc) gum_script_scheduler_perform_js_job,
        job,
        NULL);
    g_source_attach (source, js_context);
    g_source_unref (source);

    gum_script_scheduler_start (job->scheduler);
  }
}
