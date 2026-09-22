/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthreadregistry-priv.h"

#include "guminterceptor.h"
#include "gum/gumwindows.h"

#include <tlhelp32.h>
#include <winternl.h>

#if GLIB_SIZEOF_VOID_P == 4
# define GUM_THREAD_PARAM_DETECT_MAGIC 0x11223344U
#else
# define GUM_THREAD_PARAM_DETECT_MAGIC G_GUINT64_CONSTANT (0x1122334455667788)
#endif

static gboolean gum_add_existing_thread (const GumThreadDetails * thread,
    gpointer user_data);
static void gum_thread_registry_on_thread_start (GumInvocationContext * ic,
    gpointer user_data);
static void gum_thread_registry_on_thread_terminate (GumInvocationContext * ic,
    gpointer user_data);
static void gum_thread_registry_on_set_thread_description (
    GumInvocationContext * ic, gpointer user_data);

static void gum_enumerate_threads (GumFoundThreadFunc func, gpointer user_data);

static GumInterceptor * gum_thread_interceptor;
static GumInvocationListener * gum_start_handler;
static GumInvocationListener * gum_terminate_handler;
static GumInvocationListener * gum_rename_handler = NULL;

void
_gum_thread_registry_activate (GumThreadRegistry * self)
{
  GumModule * ntdll, * kernel32;
  gpointer set_description_impl;
  GumAttachOptions options = { .ignorability = GUM_INVOCATION_UNIGNORABLE };

  gum_thread_interceptor = gum_interceptor_obtain ();

  ntdll = gum_process_find_module_by_name ("ntdll.dll");
  kernel32 = gum_process_find_module_by_name ("kernel32.dll");

  set_description_impl = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (kernel32, "SetThreadDescription"));

  gum_start_handler = gum_make_probe_listener (
      gum_thread_registry_on_thread_start, self, NULL);
  gum_terminate_handler = gum_make_probe_listener (
      gum_thread_registry_on_thread_terminate, self, NULL);
  if (set_description_impl != NULL)
  {
    gum_rename_handler = gum_make_probe_listener (
        gum_thread_registry_on_set_thread_description, self, NULL);
  }

  gum_interceptor_begin_transaction (gum_thread_interceptor);

  gum_interceptor_attach (gum_thread_interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (kernel32,
          "BaseThreadInitThunk")),
      gum_start_handler, &options);
  gum_interceptor_attach (gum_thread_interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (ntdll,
          "RtlExitUserThread")),
      gum_terminate_handler, &options);
  if (set_description_impl != NULL)
  {
    gum_interceptor_attach (gum_thread_interceptor, set_description_impl,
        gum_rename_handler, &options);
  }

  gum_interceptor_end_transaction (gum_thread_interceptor);

  gum_enumerate_threads (gum_add_existing_thread, self);

  g_object_unref (kernel32);
  g_object_unref (ntdll);
}

void
_gum_thread_registry_deactivate (GumThreadRegistry * self)
{
  GumInvocationListener ** handlers[] = {
    &gum_start_handler,
    &gum_terminate_handler,
    &gum_rename_handler,
  };
  guint i;

  for (i = 0; i != G_N_ELEMENTS (handlers); i++)
  {
    GumInvocationListener ** handler = handlers[i];

    if (*handler != NULL)
    {
      gum_interceptor_detach (gum_thread_interceptor, *handler);

      g_object_unref (*handler);
      *handler = NULL;
    }
  }

  g_clear_object (&gum_thread_interceptor);
}

static gboolean
gum_add_existing_thread (const GumThreadDetails * thread,
                         gpointer user_data)
{
  GumThreadRegistry * registry = user_data;

  _gum_thread_registry_register (registry, thread);

  return TRUE;
}

static void
gum_thread_registry_on_thread_start (GumInvocationContext * ic,
                                     gpointer user_data)
{
  GumThreadRegistry * registry = user_data;
  GumThreadDetails thread = { 0, };

  thread.id = gum_process_get_current_thread_id ();

  thread.entrypoint.routine =
      GUM_ADDRESS (gum_invocation_context_get_nth_argument (ic, 1));
  thread.entrypoint.parameter =
      GUM_ADDRESS (gum_invocation_context_get_nth_argument (ic, 2));
  thread.flags |=
      GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE |
      GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER;

  _gum_thread_registry_register (registry, &thread);
}

static void
gum_thread_registry_on_thread_terminate (GumInvocationContext * ic,
                                         gpointer user_data)
{
  GumThreadRegistry * registry = user_data;

  _gum_thread_registry_unregister (registry,
      gum_process_get_current_thread_id ());
}

static void
gum_thread_registry_on_set_thread_description (GumInvocationContext * ic,
                                               gpointer user_data)
{
  GumThreadRegistry * registry = user_data;
  HANDLE thread;
  gunichar2 * name_utf16;
  gchar * name;

  thread = gum_invocation_context_get_nth_argument (ic, 0);

  name_utf16 = gum_invocation_context_get_nth_argument (ic, 1);
  name = g_utf16_to_utf8 (name_utf16, -1, NULL, NULL, NULL);

  _gum_thread_registry_rename (registry, GetThreadId (thread), name);

  g_free (name);
}

static void
gum_enumerate_threads (GumFoundThreadFunc func,
                       gpointer user_data)
{
  DWORD this_process_id;
  HANDLE snapshot;
  THREADENTRY32 entry;
  gboolean carry_on;

  this_process_id = GetCurrentProcessId ();

  snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    goto beach;

  entry.dwSize = sizeof (entry);
  if (!Thread32First (snapshot, &entry))
    goto beach;

  carry_on = TRUE;
  do
  {
    if (RTL_CONTAINS_FIELD (&entry, entry.dwSize, th32OwnerProcessID) &&
        entry.th32OwnerProcessID == this_process_id)
    {
      HANDLE handle;

      handle = OpenThread (THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
      if (handle != NULL)
      {
        GumThreadDetails thread = { 0, };

        thread.id = entry.th32ThreadID;

        thread.name = gum_windows_query_thread_name (handle);
        if (thread.name != NULL)
          thread.flags |= GUM_THREAD_FLAGS_NAME;

        thread.entrypoint.routine =
            gum_windows_query_thread_entrypoint_routine (handle);
        thread.flags |= GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE;

        carry_on = func (&thread, user_data);

        g_free ((gpointer) thread.name);
        CloseHandle (handle);
      }
    }

    entry.dwSize = sizeof (entry);
  }
  while (carry_on && Thread32Next (snapshot, &entry));

beach:
  if (snapshot != INVALID_HANDLE_VALUE)
    CloseHandle (snapshot);
}
