/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ANDROID_H__
#define __GUM_ANDROID_H__

#include "gumelfmodule.h"
#include "gumprocess.h"

G_BEGIN_DECLS

typedef gint GumAndroidLinkerFlavor;
typedef struct _GumAndroidUnrestrictedLinkerApi GumAndroidUnrestrictedLinkerApi;

typedef void * (* GumGenericDlopenImpl) (const char * filename, int flags);
typedef void * (* GumGenericDlsymImpl) (void * handle, const char * symbol);

typedef void * (* GumAndroidDlopenImpl) (const char * filename, int flags,
    void * caller_addr);
typedef void * (* GumAndroidDlsymImpl) (void * handle, const char * symbol,
    const char * version, const void * caller_addr);

enum _GumAndroidLinkerFlavor
{
  GUM_ANDROID_LINKER_NATIVE,
  GUM_ANDROID_LINKER_EMULATED
};

struct _GumAndroidUnrestrictedLinkerApi
{
  GumAndroidDlopenImpl dlopen;
  GumAndroidDlsymImpl dlsym;
};

GUM_API GumAndroidLinkerFlavor gum_android_get_linker_flavor (void);

GUM_API guint gum_android_get_api_level (void);

GUM_API gboolean gum_android_is_linker_module_name (const gchar * name);
GUM_API GumModule * gum_android_get_linker_module (void);
GUM_API const gchar ** gum_android_get_magic_linker_export_names (void);
GUM_API gboolean gum_android_try_resolve_magic_export (
    const gchar * module_name, const gchar * symbol_name, GumAddress * result);

GUM_API void gum_android_enumerate_modules (GumFoundModuleFunc func,
    gpointer user_data);

GUM_API gboolean gum_android_find_unrestricted_dlopen (
    GumGenericDlopenImpl * generic_dlopen);
GUM_API gboolean gum_android_find_unrestricted_dlsym (
    GumGenericDlsymImpl * generic_dlsym);
GUM_API gboolean gum_android_find_unrestricted_linker_api (
    GumAndroidUnrestrictedLinkerApi * api);

G_END_DECLS

#endif
