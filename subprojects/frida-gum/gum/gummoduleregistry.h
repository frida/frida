/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_REGISTRY_H__
#define __GUM_MODULE_REGISTRY_H__

#include <gum/gumprocess.h>

G_BEGIN_DECLS

#define GUM_TYPE_MODULE_REGISTRY (gum_module_registry_get_type ())
G_DECLARE_FINAL_TYPE (GumModuleRegistry, gum_module_registry, GUM,
                      MODULE_REGISTRY, GObject)

GUM_API void gum_module_registry_set_rtld_notifier_offsets (
    const guint * offsets, guint n_offsets);

GUM_API GumModuleRegistry * gum_module_registry_obtain (void);

GUM_API void gum_module_registry_enumerate_modules (GumModuleRegistry * self,
    GumFoundModuleFunc func, gpointer user_data);

GUM_API void gum_module_registry_lock (GumModuleRegistry * self);
GUM_API void gum_module_registry_unlock (GumModuleRegistry * self);

G_END_DECLS

#endif
