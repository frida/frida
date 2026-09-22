/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_SCRIPT_PRIV_H__
#define __GUM_QUICK_SCRIPT_PRIV_H__

#include "gumquickscript.h"

#include <quickjs.h>

G_BEGIN_DECLS

typedef struct _GumQuickWorker GumQuickWorker;

G_GNUC_INTERNAL GumQuickWorker * _gum_quick_script_make_worker (
    GumQuickScript * self, const gchar * url, JSValue on_message);
G_GNUC_INTERNAL GumQuickWorker * _gum_quick_worker_ref (
    GumQuickWorker * worker);
G_GNUC_INTERNAL void _gum_quick_worker_unref (GumQuickWorker * worker);
G_GNUC_INTERNAL void _gum_quick_worker_terminate (GumQuickWorker * self);
G_GNUC_INTERNAL void _gum_quick_worker_post (GumQuickWorker * self,
    const gchar * message, GBytes * data);

G_GNUC_INTERNAL void _gum_quick_script_on_scope_entered (GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_script_on_scope_left (GumQuickCore * core);

G_GNUC_INTERNAL JSValue _gum_quick_script_rethrow_parse_error_with_decorations (
    GumQuickScript * self, JSContext * ctx, const gchar * name);

G_GNUC_INTERNAL void _gum_quick_panic (JSContext * ctx, const gchar * prefix);

G_END_DECLS

#endif
