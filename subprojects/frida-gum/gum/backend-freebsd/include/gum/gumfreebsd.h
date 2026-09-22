/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_FREEBSD_H__
#define __GUM_FREEBSD_H__

#include "gumprocess.h"

#include <ucontext.h>
#include <machine/reg.h>

G_BEGIN_DECLS

GUM_API gchar * gum_freebsd_query_program_path_for_self (GError ** error);
GUM_API gchar * gum_freebsd_query_program_path_for_pid (pid_t pid,
    GError ** error);
GUM_API void gum_freebsd_enumerate_ranges (pid_t pid, GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);

GUM_API void gum_freebsd_parse_ucontext (const ucontext_t * uc,
    GumCpuContext * ctx);
GUM_API void gum_freebsd_unparse_ucontext (const GumCpuContext * ctx,
    ucontext_t * uc);
GUM_API void gum_freebsd_parse_regs (const struct reg * regs,
    GumCpuContext * ctx);
GUM_API void gum_freebsd_unparse_regs (const GumCpuContext * ctx,
    struct reg * regs);

G_END_DECLS

#endif
