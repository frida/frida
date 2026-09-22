/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"

#include "gum/gumbarebone.h"

typedef struct _GumFindThreadContext GumFindThreadContext;

struct _GumFindThreadContext
{
  GumThreadId id;
  GumThreadDetails * thread;
};

static gboolean gum_store_matching_thread (const GumThreadDetails * thread,
    gpointer user_data);
static void gum_throw_not_supported (GError ** error);

G_GNUC_WEAK const gchar *
gum_barebone_query_platform (void)
{
  return "barebone";
}

G_GNUC_WEAK gsize
gum_barebone_query_stack_size (void)
{
  return 0;
}

GumModule *
gum_process_get_libc_module (void)
{
  return NULL;
}

gboolean
gum_process_is_debugger_attached (void)
{
  return FALSE;
}

G_GNUC_WEAK GumProcessId
gum_process_get_id (void)
{
  return 0;
}

G_GNUC_WEAK GumThreadId
gum_process_get_current_thread_id (void)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return 1;
}

gboolean
gum_process_has_thread (GumThreadId thread_id)
{
  GumThreadDetails * thread;

  thread = gum_process_find_thread_by_id (thread_id, GUM_THREAD_FLAGS_NONE);
  if (thread == NULL)
    return FALSE;

  gum_thread_details_free (thread);

  return TRUE;
}

GumThreadDetails *
gum_process_find_thread_by_id (GumThreadId thread_id,
                               GumThreadFlags flags)
{
  return gum_barebone_find_thread_by_id (thread_id, flags);
}

G_GNUC_WEAK GumThreadDetails *
gum_barebone_find_thread_by_id (GumThreadId thread_id,
                                GumThreadFlags flags)
{
  GumFindThreadContext ctx;

  ctx.id = thread_id;
  ctx.thread = NULL;

  gum_barebone_enumerate_threads (gum_store_matching_thread, &ctx, flags);

  return ctx.thread;
}

gboolean
gum_process_modify_thread (GumThreadId thread_id,
                           GumModifyThreadFunc func,
                           gpointer user_data,
                           GumModifyThreadFlags flags)
{
  return gum_barebone_modify_thread (thread_id, func, user_data, flags);
}

G_GNUC_WEAK gboolean
gum_barebone_modify_thread (GumThreadId thread_id,
                            GumModifyThreadFunc func,
                            gpointer user_data,
                            GumModifyThreadFlags flags)
{
  return FALSE;
}

void
_gum_process_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data,
                                GumThreadFlags flags)
{
  gum_barebone_enumerate_threads (func, user_data, flags);
}

G_GNUC_WEAK void
gum_barebone_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data,
                                GumThreadFlags flags)
{
}

gboolean
_gum_process_collect_main_module (GumModule * module,
                                  gpointer user_data)
{
  GumModule ** out = user_data;

  *out = g_object_ref (module);

  return FALSE;
}

G_GNUC_WEAK void
_gum_process_enumerate_ranges (GumPageProtection prot,
                               GumFoundRangeFunc func,
                               gpointer user_data)
{
}

void
gum_process_enumerate_malloc_ranges (GumFoundMallocRangeFunc func,
                                     gpointer user_data)
{
}

guint
gum_thread_try_get_ranges (GumMemoryRange * ranges,
                           guint max_length)
{
  return 0;
}

gint
gum_thread_get_system_error (void)
{
  return 0;
}

void
gum_thread_set_system_error (gint value)
{
}

gboolean
gum_thread_suspend (GumThreadId thread_id,
                    GError ** error)
{
  gum_throw_not_supported (error);
  return FALSE;
}

gboolean
gum_thread_resume (GumThreadId thread_id,
                   GError ** error)
{
  gum_throw_not_supported (error);
  return FALSE;
}

gboolean
gum_thread_set_hardware_breakpoint (GumThreadId thread_id,
                                    guint breakpoint_id,
                                    GumAddress address,
                                    GError ** error)
{
  gum_throw_not_supported (error);
  return FALSE;
}

gboolean
gum_thread_unset_hardware_breakpoint (GumThreadId thread_id,
                                      guint breakpoint_id,
                                      GError ** error)
{
  gum_throw_not_supported (error);
  return FALSE;
}

gboolean
gum_thread_set_hardware_watchpoint (GumThreadId thread_id,
                                    guint watchpoint_id,
                                    GumAddress address,
                                    gsize size,
                                    GumWatchConditions wc,
                                    GError ** error)
{
  gum_throw_not_supported (error);
  return FALSE;
}

gboolean
gum_thread_unset_hardware_watchpoint (GumThreadId thread_id,
                                      guint watchpoint_id,
                                      GError ** error)
{
  gum_throw_not_supported (error);
  return FALSE;
}

static gboolean
gum_store_matching_thread (const GumThreadDetails * thread,
                           gpointer user_data)
{
  GumFindThreadContext * ctx = user_data;

  if (thread->id != ctx->id)
    return TRUE;

  ctx->thread = gum_thread_details_copy (thread);

  return FALSE;
}

static void
gum_throw_not_supported (GError ** error)
{
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "Not supported by the Barebone backend");
}
