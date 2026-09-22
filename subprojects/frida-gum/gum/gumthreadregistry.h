/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_THREAD_REGISTRY_H__
#define __GUM_THREAD_REGISTRY_H__

#include <gum/gumprocess.h>

G_BEGIN_DECLS

#define GUM_TYPE_THREAD_REGISTRY (gum_thread_registry_get_type ())
G_DECLARE_FINAL_TYPE (GumThreadRegistry, gum_thread_registry, GUM,
                      THREAD_REGISTRY, GObject)

GUM_API GumThreadRegistry * gum_thread_registry_obtain (void);

GUM_API void gum_thread_registry_enumerate_threads (GumThreadRegistry * self,
    GumFoundThreadFunc func, gpointer user_data);

GUM_API void gum_thread_registry_lock (GumThreadRegistry * self);
GUM_API void gum_thread_registry_unlock (GumThreadRegistry * self);

G_END_DECLS

#endif
