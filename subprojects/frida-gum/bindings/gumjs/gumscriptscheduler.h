/*
 * Copyright (C) 2015-2017 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SCRIPT_SCHEDULER_H__
#define __GUM_SCRIPT_SCHEDULER_H__

#include <glib-object.h>
#include <gum/gumdefs.h>

G_BEGIN_DECLS

#define GUM_TYPE_SCRIPT_SCHEDULER (gum_script_scheduler_get_type ())
G_DECLARE_FINAL_TYPE (GumScriptScheduler, gum_script_scheduler, GUM,
    SCRIPT_SCHEDULER, GObject)

typedef struct _GumScriptJob GumScriptJob;
typedef void (* GumScriptJobFunc) (gpointer data);

GUM_API GumScriptScheduler * gum_script_scheduler_new (void);

GUM_API void gum_script_scheduler_enable_background_thread (
    GumScriptScheduler * self);
GUM_API void gum_script_scheduler_disable_background_thread (
    GumScriptScheduler * self);
GUM_API void gum_script_scheduler_start (GumScriptScheduler * self);
GUM_API void gum_script_scheduler_stop (GumScriptScheduler * self);

GUM_API GMainContext * gum_script_scheduler_get_js_context (
    GumScriptScheduler * self);

GUM_API void gum_script_scheduler_push_job_on_js_thread (
    GumScriptScheduler * self, gint priority, GumScriptJobFunc func,
    gpointer data, GDestroyNotify data_destroy);
GUM_API void gum_script_scheduler_push_job_on_thread_pool (
    GumScriptScheduler * self, GumScriptJobFunc func, gpointer data,
    GDestroyNotify data_destroy);

GUM_API GumScriptJob * gum_script_job_new (GumScriptScheduler * scheduler,
    GumScriptJobFunc func, gpointer data, GDestroyNotify data_destroy);
GUM_API void gum_script_job_free (GumScriptJob * job);
GUM_API void gum_script_job_start_on_js_thread (GumScriptJob * job);

G_END_DECLS

#endif
