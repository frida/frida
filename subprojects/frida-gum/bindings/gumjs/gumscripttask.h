/*
 * Copyright (C) 2015-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SCRIPT_TASK_H__
#define __GUM_SCRIPT_TASK_H__

#include "gumscriptscheduler.h"

#include <gio/gio.h>

G_BEGIN_DECLS

#define GUM_TYPE_SCRIPT_TASK (gum_script_task_get_type ())
G_DECLARE_FINAL_TYPE (GumScriptTask, gum_script_task, GUM, SCRIPT_TASK, GObject)

typedef void (* GumScriptTaskFunc) (GumScriptTask * task,
    gpointer source_object, gpointer task_data, GCancellable * cancellable);

G_GNUC_INTERNAL GumScriptTask * gum_script_task_new (GumScriptTaskFunc func,
    gpointer source_object, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer callback_data);
G_GNUC_INTERNAL gpointer gum_script_task_get_source_object (
    GumScriptTask * self);
G_GNUC_INTERNAL gpointer gum_script_task_get_source_tag (GumScriptTask * self);
G_GNUC_INTERNAL void gum_script_task_set_source_tag (GumScriptTask * self,
    gpointer source_tag);
G_GNUC_INTERNAL GMainContext * gum_script_task_get_context (
    GumScriptTask * self);
G_GNUC_INTERNAL void gum_script_task_set_task_data (GumScriptTask * self,
    gpointer task_data, GDestroyNotify task_data_destroy);

G_GNUC_INTERNAL void gum_script_task_return_pointer (GumScriptTask * self,
    gpointer result, GDestroyNotify result_destroy);
G_GNUC_INTERNAL void gum_script_task_return_error (GumScriptTask * self,
    GError * error);

G_GNUC_INTERNAL gpointer gum_script_task_propagate_pointer (
    GumScriptTask * self, GError ** error);

G_GNUC_INTERNAL void gum_script_task_run_in_js_thread (GumScriptTask * self,
    GumScriptScheduler * scheduler);
G_GNUC_INTERNAL void gum_script_task_run_in_js_thread_sync (
    GumScriptTask * self, GumScriptScheduler * scheduler);

G_END_DECLS

#endif
