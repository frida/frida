/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_THREAD_REGISTRY_PRIV_H__
#define __GUM_THREAD_REGISTRY_PRIV_H__

#include "gumthreadregistry.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void _gum_thread_registry_activate (GumThreadRegistry * self);
G_GNUC_INTERNAL void _gum_thread_registry_deactivate (GumThreadRegistry * self);

G_GNUC_INTERNAL void _gum_thread_registry_register (GumThreadRegistry * self,
    const GumThreadDetails * thread);
G_GNUC_INTERNAL void _gum_thread_registry_unregister (GumThreadRegistry * self,
    GumThreadId id);
G_GNUC_INTERNAL void _gum_thread_registry_rename (GumThreadRegistry * self,
    GumThreadId id, const gchar * name);

G_END_DECLS

#endif
