/*
 * Copyright (C) 2024-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_UNWIND_BROKER_PRIV_H__
#define __GUM_UNWIND_BROKER_PRIV_H__

#include "gumunwindbroker.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL gboolean _gum_unwind_broker_dispatch_sections (
    GumAddress address, gpointer info);
G_GNUC_INTERNAL GumAddress _gum_unwind_broker_dispatch_translate (
    GumAddress code_address);
G_GNUC_INTERNAL gboolean _gum_unwind_broker_dispatch_install_resume_context (
    gpointer unwind_context, GumAddress real_resume_ip);

G_GNUC_INTERNAL void _gum_unwind_broker_set_ip (gpointer unwind_context,
    GumAddress ip);

G_GNUC_INTERNAL void _gum_unwind_broker_backend_activate (void);
G_GNUC_INTERNAL void _gum_unwind_broker_backend_deactivate (void);

G_END_DECLS

#endif
