/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_STALKER_PRIV_H__
#define __GUM_STALKER_PRIV_H__

#include "gumstalker.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void _gum_stalker_modify_to_run_on_thread (GumStalker * self,
    GumThreadId thread_id, GumCpuContext * cpu_context,
    GumStalkerRunOnThreadFunc func, gpointer data);

G_END_DECLS

#endif
