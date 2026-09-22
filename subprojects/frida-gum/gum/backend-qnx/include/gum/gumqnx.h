/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QNX_H__
#define __GUM_QNX_H__

#include "gumprocess.h"

#include <sys/debug.h>

G_BEGIN_DECLS

GUM_API GumCpuType gum_qnx_cpu_type_from_file (const gchar * path,
    GError ** error);
GUM_API GumCpuType gum_qnx_cpu_type_from_pid (pid_t pid, GError ** error);
GUM_API gchar * gum_qnx_query_program_path_for_self (GError ** error);
GUM_API void gum_qnx_enumerate_ranges (pid_t pid, GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);

GUM_API void gum_qnx_parse_ucontext (const ucontext_t * uc,
    GumCpuContext * ctx);
GUM_API void gum_qnx_unparse_ucontext (const GumCpuContext * ctx,
    ucontext_t * uc);

G_END_DECLS

#endif
