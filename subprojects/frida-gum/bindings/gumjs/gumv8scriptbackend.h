/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_SCRIPT_BACKEND_H__
#define __GUM_V8_SCRIPT_BACKEND_H__

#include "gumscriptbackend.h"
#include "gumscriptscheduler.h"

G_BEGIN_DECLS

#define GUM_V8_TYPE_SCRIPT_BACKEND (gum_v8_script_backend_get_type ())
G_DECLARE_FINAL_TYPE (GumV8ScriptBackend, gum_v8_script_backend, GUM_V8,
    SCRIPT_BACKEND, GObject)

G_GNUC_INTERNAL gpointer gum_v8_script_backend_get_platform (
    GumV8ScriptBackend * self);
G_GNUC_INTERNAL GumScriptScheduler * gum_v8_script_backend_get_scheduler (
    GumV8ScriptBackend * self);
G_GNUC_INTERNAL gboolean gum_v8_script_backend_is_scope_mutex_trapped (
    GumV8ScriptBackend * self);
G_GNUC_INTERNAL void gum_v8_script_backend_mark_scope_mutex_trapped (
    GumV8ScriptBackend * self);
G_GNUC_INTERNAL void gum_v8_script_backend_unmark_scope_mutex_trapped (
    GumV8ScriptBackend * self);

G_END_DECLS

#endif
