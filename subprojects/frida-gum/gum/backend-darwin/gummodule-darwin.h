/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_DARWIN_H__
#define __GUM_MODULE_DARWIN_H__

#include "gummodule.h"
#include "gum/gumdarwinmoduleresolver.h"

G_BEGIN_DECLS

#define GUM_TYPE_NATIVE_MODULE (gum_native_module_get_type ())
G_DECLARE_FINAL_TYPE (GumNativeModule, gum_native_module, GUM, NATIVE_MODULE,
                      GObject)

G_GNUC_INTERNAL GumNativeModule * _gum_native_module_make (const gchar * path,
    const GumMemoryRange * range, GumDarwinModuleResolver * resolver);

G_GNUC_INTERNAL void _gum_native_module_detach_resolver (
    GumNativeModule * self);

G_GNUC_INTERNAL gpointer _gum_native_module_get_handle (GumNativeModule * self);
G_GNUC_INTERNAL GumDarwinModule * _gum_native_module_get_darwin_module (
    GumNativeModule * self);

G_END_DECLS

#endif
