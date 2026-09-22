/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_REGISTRY_ELF_H__
#define __GUM_MODULE_REGISTRY_ELF_H__

#include "guminvocationcontext.h"
#include "gummoduleregistry-priv.h"

G_BEGIN_DECLS

typedef struct _GumRtldNotifierDetails GumRtldNotifierDetails;

typedef void (* GumFoundRtldNotifierFunc) (
    const GumRtldNotifierDetails * details, gpointer user_data);
typedef void (* GumSynchronizeModulesFunc) (void);

struct _GumRtldNotifierDetails
{
  gpointer location;
  GumPointCut point_cut;
};

G_GNUC_INTERNAL void _gum_module_registry_enumerate_loaded_modules (
    GumFoundModuleFunc func, gpointer user_data);
G_GNUC_INTERNAL void _gum_module_registry_enumerate_rtld_notifiers (
    GumFoundRtldNotifierFunc func, gpointer user_data);
G_GNUC_INTERNAL void _gum_module_registry_handle_rtld_notification (
    GumSynchronizeModulesFunc sync, GumInvocationContext * ic);

G_END_DECLS

#endif
