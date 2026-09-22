/*
 * Copyright (C) 2012-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_LINUX_H__
#define __GUM_LINUX_H__

#include "gumprocess.h"

#include <ucontext.h>

G_BEGIN_DECLS

typedef struct _GumLinuxPThreadIter GumLinuxPThreadIter;
typedef struct _GumLinuxPThreadSpec GumLinuxPThreadSpec;
typedef struct _GumLinuxNamedRange GumLinuxNamedRange;

struct _GumLinuxPThreadIter
{
#ifdef HAVE_GLIBC
  GList * list;
#else
  gpointer list;
  gpointer node;
  const GumLinuxPThreadSpec * spec;
#endif
};

struct _GumLinuxNamedRange
{
  const gchar * name;
  gpointer base;
  gsize size;
};

GUM_API gboolean gum_linux_check_kernel_version (guint major, guint minor,
    guint micro);
GUM_API GumCpuType gum_linux_cpu_type_from_file (const gchar * path,
    GError ** error);
GUM_API GumCpuType gum_linux_cpu_type_from_pid (pid_t pid, GError ** error);
GUM_API GumCpuType gum_linux_cpu_type_from_auxv (gconstpointer auxv,
    gsize auxv_size);
GUM_API void gum_linux_pthread_iter_init (GumLinuxPThreadIter * iter,
    const GumLinuxPThreadSpec * spec);
GUM_API gboolean gum_linux_pthread_iter_next (GumLinuxPThreadIter * self,
    pthread_t * thread);
GUM_API void gum_linux_enumerate_ranges (pid_t pid, GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);
GUM_API GHashTable * gum_linux_collect_named_ranges (void);
GUM_API gchar * gum_linux_query_thread_name (GumThreadId id);
GUM_API gboolean gum_linux_query_thread_state (GumThreadId tid,
    GumThreadState * state);
GUM_API gboolean gum_linux_query_thread_cpu_context (GumThreadId tid,
    GumCpuContext * ctx);
GUM_API GumThreadId gum_linux_query_pthread_tid (pthread_t thread,
    const GumLinuxPThreadSpec * spec);
GUM_API gpointer gum_linux_query_pthread_start_routine (pthread_t thread,
    const GumLinuxPThreadSpec * spec);
GUM_API gpointer gum_linux_query_pthread_start_parameter (pthread_t thread,
    const GumLinuxPThreadSpec * spec);
GUM_API void gum_linux_lock_pthread_list (const GumLinuxPThreadSpec * spec);
GUM_API void gum_linux_unlock_pthread_list (const GumLinuxPThreadSpec * spec);
GUM_API const GumLinuxPThreadSpec * gum_linux_query_pthread_spec (void);

GUM_API gboolean gum_linux_module_path_matches (const gchar * path,
    const gchar * name_or_path);

GUM_API void gum_linux_parse_ucontext (const ucontext_t * uc,
    GumCpuContext * ctx);
GUM_API void gum_linux_unparse_ucontext (const GumCpuContext * ctx,
    ucontext_t * uc);

G_END_DECLS

#endif
