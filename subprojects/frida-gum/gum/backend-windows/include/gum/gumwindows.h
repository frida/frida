/*
 * Copyright (C) 2010-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_WINDOWS_H__
#define __GUM_WINDOWS_H__

#include <gum/gummemory.h>

#include <glib.h>
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

G_BEGIN_DECLS

GUM_API GumCpuType gum_windows_query_native_cpu_type (void);
GUM_API GumCpuType gum_windows_cpu_type_from_pid (guint pid, GError ** error);

GUM_API gchar * gum_windows_query_thread_name (HANDLE thread);
GUM_API GumAddress gum_windows_query_thread_entrypoint_routine (HANDLE thread);
GUM_API void gum_windows_parse_context (const CONTEXT * context,
    GumCpuContext * cpu_context);
GUM_API void gum_windows_unparse_context (const GumCpuContext * cpu_context,
    CONTEXT * context);
GUM_API CONTEXT * gum_windows_get_active_exceptor_context (void);

GUM_API GumPageProtection gum_page_protection_from_windows (DWORD native_prot);
GUM_API DWORD gum_page_protection_to_windows (GumPageProtection prot);

G_END_DECLS

#endif
