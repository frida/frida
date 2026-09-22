/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_REGISTRY_PRIV_H__
#define __GUM_MODULE_REGISTRY_PRIV_H__

#include "gummoduleregistry.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GUM_API GPtrArray * _gum_module_registry_get_modules (
    GumModuleRegistry * self);

G_GNUC_INTERNAL const guint * _gum_module_registry_get_rtld_notifier_offsets (
    guint * n_offsets);

G_GNUC_INTERNAL void _gum_module_registry_activate (GumModuleRegistry * self);
G_GNUC_INTERNAL void _gum_module_registry_deactivate (GumModuleRegistry * self);

G_GNUC_INTERNAL void _gum_module_registry_reset (GumModuleRegistry * self);
G_GNUC_INTERNAL void _gum_module_registry_register (GumModuleRegistry * self,
    GumModule * mod);
G_GNUC_INTERNAL void _gum_module_registry_unregister (GumModuleRegistry * self,
    GumAddress base_address);

G_END_DECLS

#endif
