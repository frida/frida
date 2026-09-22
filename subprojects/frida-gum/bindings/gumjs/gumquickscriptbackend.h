/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_SCRIPT_BACKEND_H__
#define __GUM_QUICK_SCRIPT_BACKEND_H__

#include "gumscriptbackend.h"

G_BEGIN_DECLS

#define GUM_QUICK_TYPE_SCRIPT_BACKEND (gum_quick_script_backend_get_type ())
G_DECLARE_FINAL_TYPE (GumQuickScriptBackend, gum_quick_script_backend,
    GUM_QUICK, SCRIPT_BACKEND, GObject)

G_END_DECLS

#endif
