/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_SCRIPT_H__
#define __GUM_QUICK_SCRIPT_H__

#include "gumscript.h"

G_BEGIN_DECLS

#define GUM_QUICK_TYPE_SCRIPT (gum_quick_script_get_type ())
G_DECLARE_FINAL_TYPE (GumQuickScript, gum_quick_script, GUM_QUICK, SCRIPT,
    GObject)

G_GNUC_INTERNAL gboolean gum_quick_script_create_context (GumQuickScript * self,
    GError ** error);

G_END_DECLS

#endif
