/*
 * Copyright (C) 2018-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_INSPECTOR_SERVER_H__
#define __GUM_INSPECTOR_SERVER_H__

#include <glib-object.h>
#include <gum/gumdefs.h>

G_BEGIN_DECLS

#define GUM_TYPE_INSPECTOR_SERVER (gum_inspector_server_get_type ())
G_DECLARE_FINAL_TYPE (GumInspectorServer, gum_inspector_server, GUM,
    INSPECTOR_SERVER, GObject)

GUM_API GumInspectorServer * gum_inspector_server_new (void);
GUM_API GumInspectorServer * gum_inspector_server_new_with_port (guint port);

GUM_API gboolean gum_inspector_server_start (GumInspectorServer * self,
    GError ** error);
GUM_API void gum_inspector_server_stop (GumInspectorServer * self);

GUM_API void gum_inspector_server_post_message (GumInspectorServer * self,
    const gchar * message);

G_END_DECLS

#endif
