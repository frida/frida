/*
 * Copyright (C) 2018-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_SYMBOLICATOR_H__
#define __GUM_DARWIN_SYMBOLICATOR_H__

#include "gumdarwin.h"

G_BEGIN_DECLS

#define GUM_DARWIN_TYPE_SYMBOLICATOR (gum_darwin_symbolicator_get_type ())
G_DECLARE_FINAL_TYPE (GumDarwinSymbolicator, gum_darwin_symbolicator,
                      GUM_DARWIN, SYMBOLICATOR, GObject)

GUM_API GumDarwinSymbolicator * gum_darwin_symbolicator_new_with_path (
    const gchar * path, GumCpuType cpu_type, GError ** error);
GUM_API GumDarwinSymbolicator * gum_darwin_symbolicator_new_with_task (
    mach_port_t task, GError ** error);

GUM_API gboolean gum_darwin_symbolicator_load (GumDarwinSymbolicator * self,
    GError ** error);

GUM_API gboolean gum_darwin_symbolicator_details_from_address (
    GumDarwinSymbolicator * self, GumAddress address,
    GumDebugSymbolDetails * details);
GUM_API gchar * gum_darwin_symbolicator_name_from_address (
    GumDarwinSymbolicator * self, GumAddress address);

GUM_API GumAddress gum_darwin_symbolicator_find_function (
    GumDarwinSymbolicator * self, const gchar * name);
GUM_API GumAddress * gum_darwin_symbolicator_find_functions_named (
    GumDarwinSymbolicator * self, const gchar * name, gsize * len);
GUM_API GumAddress * gum_darwin_symbolicator_find_functions_matching (
    GumDarwinSymbolicator * self, const gchar * str, gsize * len);

G_END_DECLS

#endif
