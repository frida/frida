/*
 * Copyright (C) 2008-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_INSTANCE_TRACKER_H__
#define __GUM_INSTANCE_TRACKER_H__

#include <glib-object.h>
#include <gum/gumdefs.h>

G_BEGIN_DECLS

#define GUM_TYPE_INSTANCE_TRACKER (gum_instance_tracker_get_type ())
G_DECLARE_FINAL_TYPE (GumInstanceTracker, gum_instance_tracker, GUM,
    INSTANCE_TRACKER, GObject)

typedef struct _GumInstanceVTable GumInstanceVTable;
typedef struct _GumInstanceDetails GumInstanceDetails;

typedef GTypeInstance * (* GumCreateInstanceFunc) (GType type);
typedef void (* GumFreeInstanceFunc) (GTypeInstance * instance);
typedef const gchar * (* GumTypeIdToNameFunc) (GType type);
typedef gboolean (* GumFilterInstanceTypeFunc) (
    GumInstanceTracker * tracker, GType gtype, gpointer user_data);
typedef void (* GumWalkInstanceFunc) (GumInstanceDetails * id,
    gpointer user_data);

struct _GumInstanceVTable
{
  GumCreateInstanceFunc create_instance;
  GumFreeInstanceFunc free_instance;

  GumTypeIdToNameFunc type_id_to_name;
};

struct _GumInstanceDetails
{
  gconstpointer address;
  guint ref_count;
  const gchar * type_name;
};

GUM_API GumInstanceTracker * gum_instance_tracker_new (void);

GUM_API void gum_instance_tracker_begin (GumInstanceTracker * self,
    GumInstanceVTable * vtable);
GUM_API void gum_instance_tracker_end (GumInstanceTracker * self);

GUM_API const GumInstanceVTable * gum_instance_tracker_get_current_vtable (
    GumInstanceTracker * self);

GUM_API void gum_instance_tracker_set_type_filter_function (
    GumInstanceTracker * self, GumFilterInstanceTypeFunc filter,
    gpointer user_data);

GUM_API guint gum_instance_tracker_peek_total_count (GumInstanceTracker * self,
    const gchar * type_name);
GUM_API GList * gum_instance_tracker_peek_instances (GumInstanceTracker * self);
GUM_API void gum_instance_tracker_walk_instances (GumInstanceTracker * self,
    GumWalkInstanceFunc func, gpointer user_data);

/*< Internal API */
void gum_instance_tracker_add_instance (GumInstanceTracker * self,
    gpointer instance, GType instance_type);
void gum_instance_tracker_remove_instance (GumInstanceTracker * self,
    gpointer instance, GType instance_type);

G_END_DECLS

#endif
