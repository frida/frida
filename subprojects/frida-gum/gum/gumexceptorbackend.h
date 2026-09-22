/*
 * Copyright (C) 2016-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_EXCEPTOR_BACKEND_H__
#define __GUM_EXCEPTOR_BACKEND_H__

#include "gumexceptor.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void _gum_exceptor_backend_prepare_to_fork (void);
G_GNUC_INTERNAL void _gum_exceptor_backend_recover_from_fork_in_parent (void);
G_GNUC_INTERNAL void _gum_exceptor_backend_recover_from_fork_in_child (void);

#define GUM_TYPE_EXCEPTOR_BACKEND (gum_exceptor_backend_get_type ())
G_DECLARE_FINAL_TYPE (GumExceptorBackend, gum_exceptor_backend, GUM,
                      EXCEPTOR_BACKEND, GObject)

G_GNUC_INTERNAL GumExceptorBackend * gum_exceptor_backend_new (
    GumExceptionHandler handler, gpointer user_data);

G_GNUC_INTERNAL GumExceptorMode _gum_exceptor_get_mode (void);

G_END_DECLS

#endif
