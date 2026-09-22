/*
 * Copyright (C) 2008-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_BOUNDS_CHECKER_H__
#define __GUM_BOUNDS_CHECKER_H__

#include <glib-object.h>
#include <gum/gumbacktracer.h>
#include <gum/gumheapapi.h>

G_BEGIN_DECLS

#define GUM_TYPE_BOUNDS_CHECKER (gum_bounds_checker_get_type ())
G_DECLARE_FINAL_TYPE (GumBoundsChecker, gum_bounds_checker, GUM, BOUNDS_CHECKER,
    GObject)

typedef void (* GumBoundsOutputFunc) (const gchar * text, gpointer user_data);

GUM_API GumBoundsChecker * gum_bounds_checker_new (GumBacktracer * backtracer,
    GumBoundsOutputFunc func, gpointer user_data);

GUM_API guint gum_bounds_checker_get_pool_size (GumBoundsChecker * self);
GUM_API void gum_bounds_checker_set_pool_size (GumBoundsChecker * self,
  guint pool_size);
GUM_API guint gum_bounds_checker_get_front_alignment (GumBoundsChecker * self);
GUM_API void gum_bounds_checker_set_front_alignment (GumBoundsChecker * self,
  guint pool_size);

GUM_API void gum_bounds_checker_attach (GumBoundsChecker * self);
GUM_API void gum_bounds_checker_attach_to_apis (GumBoundsChecker * self,
    const GumHeapApiList * apis);
GUM_API void gum_bounds_checker_detach (GumBoundsChecker * self);

G_END_DECLS

#endif
