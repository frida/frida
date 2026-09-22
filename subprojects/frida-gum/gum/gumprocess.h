/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020-2024 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2023 Grant Douglas <me@hexplo.it>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PROCESS_H__
#define __GUM_PROCESS_H__

#include <gum/gummemory.h>
#include <gum/gummodule.h>

#define GUM_THREAD_ID_INVALID ((GumThreadId) -1)
#define GUM_TYPE_THREAD_DETAILS (gum_thread_details_get_type ())

G_BEGIN_DECLS

typedef guint GumProcessId;
typedef gsize GumThreadId;
typedef struct _GumThreadDetails GumThreadDetails;
typedef struct _GumThreadEntrypoint GumThreadEntrypoint;
typedef struct _GumMallocRangeDetails GumMallocRangeDetails;

typedef enum {
  GUM_TEARDOWN_REQUIREMENT_FULL,
  GUM_TEARDOWN_REQUIREMENT_MINIMAL
} GumTeardownRequirement;

typedef enum {
  GUM_CODE_SIGNING_OPTIONAL,
  GUM_CODE_SIGNING_REQUIRED
} GumCodeSigningPolicy;

typedef enum {
  GUM_MODIFY_THREAD_FLAGS_NONE         = 0,
  GUM_MODIFY_THREAD_FLAGS_ABORT_SAFELY = (1 << 0),
} GumModifyThreadFlags;

typedef enum {
  GUM_THREAD_FLAGS_NAME                 = (1 << 0),
  GUM_THREAD_FLAGS_STATE                = (1 << 1),
  GUM_THREAD_FLAGS_CPU_CONTEXT          = (1 << 2),
  GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE   = (1 << 3),
  GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER = (1 << 4),

  GUM_THREAD_FLAGS_NONE                 = 0,
  GUM_THREAD_FLAGS_ALL                  = GUM_THREAD_FLAGS_NAME |
                                          GUM_THREAD_FLAGS_STATE |
                                          GUM_THREAD_FLAGS_CPU_CONTEXT |
                                          GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE |
                                          GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER,
} GumThreadFlags;

typedef enum {
  GUM_THREAD_RUNNING = 1,
  GUM_THREAD_STOPPED,
  GUM_THREAD_WAITING,
  GUM_THREAD_UNINTERRUPTIBLE,
  GUM_THREAD_HALTED
} GumThreadState;

struct _GumThreadEntrypoint
{
  GumAddress routine;
  GumAddress parameter;
};

struct _GumThreadDetails
{
  GumThreadFlags flags;
  GumThreadId id;
  const gchar * name;
  GumThreadState state;
  GumCpuContext cpu_context;
  GumThreadEntrypoint entrypoint;
};

typedef enum {
  GUM_WATCH_READ  = (1 << 0),
  GUM_WATCH_WRITE = (1 << 1),
} GumWatchConditions;

struct _GumMallocRangeDetails
{
  const GumMemoryRange * range;
};

typedef void (* GumModifyThreadFunc) (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
typedef gboolean (* GumFoundThreadFunc) (const GumThreadDetails * details,
    gpointer user_data);
typedef gboolean (* GumFoundModuleFunc) (GumModule * module,
    gpointer user_data);
typedef gboolean (* GumFoundMallocRangeFunc) (
    const GumMallocRangeDetails * details, gpointer user_data);

GUM_API GumOS gum_process_get_native_os (void);
GUM_API GumTeardownRequirement gum_process_get_teardown_requirement (void);
GUM_API void gum_process_set_teardown_requirement (
    GumTeardownRequirement requirement);
GUM_API GumCodeSigningPolicy gum_process_get_code_signing_policy (void);
GUM_API void gum_process_set_code_signing_policy (GumCodeSigningPolicy policy);
GUM_API gboolean gum_process_is_debugger_attached (void);
GUM_API GumProcessId gum_process_get_id (void);
GUM_API GumThreadId gum_process_get_current_thread_id (void);
GUM_API gboolean gum_process_has_thread (GumThreadId thread_id);
GUM_API GumThreadDetails * gum_process_find_thread_by_id (GumThreadId thread_id,
    GumThreadFlags flags);
GUM_API gboolean gum_process_modify_thread (GumThreadId thread_id,
    GumModifyThreadFunc func, gpointer user_data, GumModifyThreadFlags flags);
GUM_API void gum_process_enumerate_threads (GumFoundThreadFunc func,
    gpointer user_data, GumThreadFlags flags);
GUM_API GumModule * gum_process_get_main_module (void);
GUM_API GumModule * gum_process_get_libc_module (void);
GUM_API GumModule * gum_process_find_module_by_name (const gchar * name);
GUM_API GumModule * gum_process_find_module_by_address (GumAddress address);
GUM_API gboolean gum_process_find_function_range (gconstpointer address,
    GumMemoryRange * range);
GUM_API void gum_process_enumerate_modules (GumFoundModuleFunc func,
    gpointer user_data);
GUM_API void gum_process_enumerate_ranges (GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);
GUM_API void gum_process_enumerate_malloc_ranges (
    GumFoundMallocRangeFunc func, gpointer user_data);
GUM_API guint gum_thread_try_get_ranges (GumMemoryRange * ranges,
    guint max_length);
GUM_API gint gum_thread_get_system_error (void);
GUM_API void gum_thread_set_system_error (gint value);
GUM_API gboolean gum_thread_suspend (GumThreadId thread_id, GError ** error);
GUM_API gboolean gum_thread_resume (GumThreadId thread_id, GError ** error);
GUM_API gboolean gum_thread_set_hardware_breakpoint (GumThreadId thread_id,
    guint breakpoint_id, GumAddress address, GError ** error);
GUM_API gboolean gum_thread_unset_hardware_breakpoint (GumThreadId thread_id,
    guint breakpoint_id, GError ** error);
GUM_API gboolean gum_thread_set_hardware_watchpoint (GumThreadId thread_id,
    guint watchpoint_id, GumAddress address, gsize size, GumWatchConditions wc,
    GError ** error);
GUM_API gboolean gum_thread_unset_hardware_watchpoint (GumThreadId thread_id,
    guint watchpoint_id, GError ** error);

GUM_API const gchar * gum_code_signing_policy_to_string (
    GumCodeSigningPolicy policy);

GUM_API GType gum_thread_details_get_type (void) G_GNUC_CONST;
GUM_API GumThreadDetails * gum_thread_details_copy (
    const GumThreadDetails * details);
GUM_API void gum_thread_details_free (GumThreadDetails * details);

G_END_DECLS

#endif
