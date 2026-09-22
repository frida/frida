/*
 * Copyright (C) 2022-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MODULE_ELF_H__
#define __GUM_MODULE_ELF_H__

#include "gumelfmodule.h"
#include "gummodule.h"

G_BEGIN_DECLS

#define GUM_TYPE_NATIVE_MODULE (gum_native_module_get_type ())
G_DECLARE_FINAL_TYPE (GumNativeModule, gum_native_module, GUM, NATIVE_MODULE,
                      GObject)

typedef gpointer (* GumCreateModuleHandleFunc) (GumNativeModule * module,
    gpointer user_data);

struct _GumNativeModule
{
  GObject parent;

  gchar * name;
  gchar * path;
  GumMemoryRange range;
  GumCreateModuleHandleFunc create_handle;
  gpointer create_handle_data;
  GDestroyNotify create_handle_data_destroy;
  GDestroyNotify destroy_handle;

  GRecMutex mutex;

  gpointer cached_handle;
  gboolean attempted_handle_creation;

  GumElfModule * cached_elf_module;
  gboolean attempted_elf_module_creation;
};

G_GNUC_INTERNAL GumNativeModule * _gum_native_module_make (const gchar * path,
    const GumMemoryRange * range, GumCreateModuleHandleFunc create_handle,
    gpointer create_handle_data, GDestroyNotify create_handle_data_destroy,
    GDestroyNotify destroy_handle);
G_GNUC_INTERNAL GumNativeModule * _gum_native_module_make_handleless (
    const gchar * path, const GumMemoryRange * range);

G_GNUC_INTERNAL gpointer _gum_native_module_get_handle (GumNativeModule * self);
G_GNUC_INTERNAL GumElfModule * _gum_native_module_get_elf_module (
    GumNativeModule * self);

G_GNUC_INTERNAL gchar * _gum_native_module_find_path_by_address (
    GumAddress address);

G_END_DECLS

#endif
