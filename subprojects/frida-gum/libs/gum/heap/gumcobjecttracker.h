/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_COBJECT_TRACKER_H__
#define __GUM_COBJECT_TRACKER_H__

#include <glib-object.h>
#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_COBJECT_TRACKER (gum_cobject_tracker_get_type ())
G_DECLARE_FINAL_TYPE (GumCObjectTracker, gum_cobject_tracker, GUM,
    COBJECT_TRACKER, GObject)

GUM_API GumCObjectTracker * gum_cobject_tracker_new (void);
GUM_API GumCObjectTracker * gum_cobject_tracker_new_with_backtracer (
    GumBacktracer * backtracer);

GUM_API void gum_cobject_tracker_track (GumCObjectTracker * self,
    const gchar * type_name, gpointer type_constructor);

GUM_API void gum_cobject_tracker_begin (GumCObjectTracker * self);
GUM_API void gum_cobject_tracker_end (GumCObjectTracker * self);

GUM_API guint gum_cobject_tracker_peek_total_count (GumCObjectTracker * self,
    const gchar * type_name);
GUM_API GList * gum_cobject_tracker_peek_object_list (GumCObjectTracker * self);

G_END_DECLS

#endif
