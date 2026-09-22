/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthreadregistry-priv.h"

#include "gum/gumbarebone.h"

void
_gum_thread_registry_activate (GumThreadRegistry * self)
{
  gum_barebone_on_thread_registry_activating (self);
}

void
_gum_thread_registry_deactivate (GumThreadRegistry * self)
{
  gum_barebone_on_thread_registry_deactivating (self);
}

G_GNUC_WEAK void
gum_barebone_on_thread_registry_activating (GumThreadRegistry * registry)
{
}

G_GNUC_WEAK void
gum_barebone_on_thread_registry_deactivating (GumThreadRegistry * registry)
{
}

void
gum_barebone_register_thread (GumThreadRegistry * registry,
                              const GumThreadDetails * thread)
{
  _gum_thread_registry_register (registry, thread);
}

void
gum_barebone_unregister_thread (GumThreadRegistry * registry,
                                GumThreadId id)
{
  _gum_thread_registry_unregister (registry, id);
}

void
gum_barebone_rename_thread (GumThreadRegistry * registry,
                            GumThreadId id,
                            const gchar * name)
{
  _gum_thread_registry_rename (registry, id, name);
}
