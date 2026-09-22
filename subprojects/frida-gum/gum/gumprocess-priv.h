/*
 * Copyright (C) 2017-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PROCESS_PRIV_H__
#define __GUM_PROCESS_PRIV_H__

#include "gumprocess.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void _gum_process_enumerate_threads (GumFoundThreadFunc func,
    gpointer user_data, GumThreadFlags flags);
G_GNUC_INTERNAL gboolean _gum_process_collect_main_module (GumModule * module,
    gpointer user_data);
G_GNUC_INTERNAL void _gum_process_enumerate_ranges (GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);

#ifdef HAVE_WINDOWS
G_GNUC_INTERNAL gboolean _gum_windows_is_win7_wow64 (void);
#else
# define _gum_windows_is_win7_wow64() FALSE
#endif

#if defined (HAVE_I386)
G_GNUC_INTERNAL void _gum_x86_set_breakpoint (gsize * dr7, gsize * dr0,
    guint breakpoint_id, GumAddress address);
G_GNUC_INTERNAL void _gum_x86_unset_breakpoint (gsize * dr7, gsize * dr0,
    guint breakpoint_id);
G_GNUC_INTERNAL void _gum_x86_set_watchpoint (gsize * dr7, gsize * dr0,
    guint watchpoint_id, GumAddress address, gsize size,
    GumWatchConditions conditions);
G_GNUC_INTERNAL void _gum_x86_unset_watchpoint (gsize * dr7, gsize * dr0,
    guint watchpoint_id);
#elif defined (HAVE_ARM)
G_GNUC_INTERNAL void _gum_arm_set_breakpoint (guint32 * bcr, guint32 * bvr,
    guint breakpoint_id, GumAddress address);
G_GNUC_INTERNAL void _gum_arm_unset_breakpoint (guint32 * bcr, guint32 * bvr,
    guint breakpoint_id);
G_GNUC_INTERNAL void _gum_arm_set_watchpoint (guint32 * wcr, guint32 * wvr,
    guint watchpoint_id, GumAddress address, gsize size,
    GumWatchConditions conditions);
G_GNUC_INTERNAL void _gum_arm_unset_watchpoint (guint32 * wcr, guint32 * wvr,
    guint watchpoint_id);
#elif defined (HAVE_ARM64)
# if defined (HAVE_WINDOWS)
typedef guint32 GumArm64CtrlReg;
# else
typedef guint64 GumArm64CtrlReg;
# endif
G_GNUC_INTERNAL void _gum_arm64_set_breakpoint (GumArm64CtrlReg * bcr,
    guint64 * bvr, guint breakpoint_id, GumAddress address);
G_GNUC_INTERNAL void _gum_arm64_unset_breakpoint (GumArm64CtrlReg * bcr,
    guint64 * bvr, guint breakpoint_id);
G_GNUC_INTERNAL void _gum_arm64_set_watchpoint (GumArm64CtrlReg * wcr,
    guint64 * wvr, guint watchpoint_id, GumAddress address, gsize size,
    GumWatchConditions conditions);
G_GNUC_INTERNAL void _gum_arm64_unset_watchpoint (GumArm64CtrlReg * wcr,
    guint64 * wvr, guint watchpoint_id);
#elif defined (HAVE_MIPS)
G_GNUC_INTERNAL void _gum_mips_set_breakpoint (gsize * watch_lo,
    guint16 * watch_hi, guint breakpoint_id, GumAddress address);
G_GNUC_INTERNAL void _gum_mips_unset_breakpoint (gsize * watch_lo,
    guint16 * watch_hi, guint breakpoint_id);
G_GNUC_INTERNAL void _gum_mips_set_watchpoint (gsize * watch_lo,
    guint16 * watch_hi, guint watchpoint_id, GumAddress address, gsize size,
    GumWatchConditions conditions);
G_GNUC_INTERNAL void _gum_mips_unset_watchpoint (gsize * watch_lo,
    guint16 * watch_hi, guint watchpoint_id);
#endif

G_END_DECLS

#endif
