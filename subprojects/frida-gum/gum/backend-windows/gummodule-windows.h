/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_WINDOWS_H__
#define __GUM_MODULE_WINDOWS_H__

#include <gum/gumdbghelp.h>
#include <gum/gummodule.h>
#include <gum/gumwindows.h>

G_BEGIN_DECLS

#define GUM_TYPE_NATIVE_MODULE (gum_native_module_get_type ())
G_DECLARE_FINAL_TYPE (GumNativeModule, gum_native_module, GUM, NATIVE_MODULE,
                      GObject)

G_GNUC_INTERNAL GumNativeModule * _gum_native_module_make (HMODULE handle);

G_END_DECLS

#endif
