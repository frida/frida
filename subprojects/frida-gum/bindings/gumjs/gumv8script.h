/*
 * Copyright (C) 2015-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_SCRIPT_H__
#define __GUM_V8_SCRIPT_H__

#include "gumscript.h"

G_BEGIN_DECLS

#define GUM_V8_TYPE_SCRIPT (gum_v8_script_get_type ())
G_DECLARE_FINAL_TYPE (GumV8Script, gum_v8_script, GUM_V8, SCRIPT, GObject)

G_GNUC_INTERNAL gboolean gum_v8_script_create_context (GumV8Script * self,
    GError ** error);

G_END_DECLS

#endif
