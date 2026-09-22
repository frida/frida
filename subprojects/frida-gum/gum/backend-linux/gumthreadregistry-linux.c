/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthreadregistry-priv.h"

#include "guminterceptor.h"
#include "gumlinux-priv.h"
#include "gum/gumlinux.h"

#ifdef HAVE_MUSL
# define GUM_PTHREAD_FROM_PTR(p) (p)
#else
# define GUM_PTHREAD_FROM_PTR(p) GPOINTER_TO_SIZE (p)
#endif

static void gum_thread_registry_on_pthread_start (GumInvocationContext * ic,
    gpointer user_data);
static void gum_thread_registry_on_pthread_terminate (GumInvocationContext * ic,
    gpointer user_data);
static void gum_thread_registry_on_pthread_setname (GumInvocationContext * ic,
    gpointer user_data);

static void gum_compute_thread_details_from_pthread (pthread_t thread,
    const GumLinuxPThreadSpec * spec, GumThreadDetails * details,
    gpointer * storage);

static GumThreadRegistry * gum_registry;
static const GumLinuxPThreadSpec * gum_pthread;

static GumInterceptor * gum_thread_interceptor;
static GumInvocationListener * gum_start_handler;
static GumInvocationListener * gum_terminate_handler;
static GumInvocationListener * gum_rename_handler = NULL;

void
_gum_thread_registry_activate (GumThreadRegistry * self)
{
  GumLinuxPThreadIter iter;
  pthread_t thread;
  GumAttachOptions options = { .ignorability = GUM_INVOCATION_UNIGNORABLE };

  gum_registry = self;
  gum_pthread = gum_linux_query_pthread_spec ();

  gum_start_handler = gum_make_probe_listener (
      gum_thread_registry_on_pthread_start, gum_registry, NULL);
  gum_terminate_handler = gum_make_probe_listener (
      gum_thread_registry_on_pthread_terminate, gum_registry, NULL);
  if (gum_pthread->set_name != NULL)
  {
    gum_rename_handler = gum_make_probe_listener (
        gum_thread_registry_on_pthread_setname, gum_registry, NULL);
  }

  gum_thread_interceptor = gum_interceptor_obtain ();
  gum_interceptor_begin_transaction (gum_thread_interceptor);

  gum_linux_lock_pthread_list (gum_pthread);

  gum_interceptor_attach (gum_thread_interceptor, gum_pthread->start_impl,
      gum_start_handler, &options);
  if (gum_pthread->start_c11_impl != NULL)
  {
    gum_interceptor_attach (gum_thread_interceptor, gum_pthread->start_c11_impl,
        gum_start_handler, &options);
  }
  gum_interceptor_attach (gum_thread_interceptor, gum_pthread->terminate_impl,
      gum_terminate_handler, &options);
  if (gum_pthread->set_name != NULL)
  {
    gum_interceptor_attach (gum_thread_interceptor, gum_pthread->set_name,
        gum_rename_handler, &options);
  }

  gum_interceptor_end_transaction (gum_thread_interceptor);

  gum_linux_pthread_iter_init (&iter, gum_pthread);
  while (gum_linux_pthread_iter_next (&iter, &thread))
  {
    GumThreadDetails t;
    gpointer storage;

    gum_compute_thread_details_from_pthread (thread, gum_pthread, &t, &storage);

    _gum_thread_registry_register (self, &t);

    g_free (storage);
  }

  gum_linux_unlock_pthread_list (gum_pthread);
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
    if (*handlers[i] != NULL)
      gum_interceptor_detach (gum_thread_interceptor, *handlers[i]);
  }

  for (i = 0; i != G_N_ELEMENTS (handlers); i++)
  {
    if (*handlers[i] == NULL)
      continue;

    while (!gum_interceptor_flush_listener (gum_thread_interceptor,
          *handlers[i]))
      g_thread_yield ();
  }

  for (i = 0; i != G_N_ELEMENTS (handlers); i++)
  {
    GumInvocationListener ** handler = handlers[i];

    if (*handler != NULL)
    {
      g_object_unref (*handler);
      *handler = NULL;
    }
  }

  g_clear_object (&gum_thread_interceptor);
}

static void
gum_thread_registry_on_pthread_start (GumInvocationContext * ic,
                                      gpointer user_data)
{
  GumThreadRegistry * registry = user_data;
  GumThreadDetails thread;
  gpointer storage;

  gum_compute_thread_details_from_pthread (pthread_self (), gum_pthread,
      &thread, &storage);

  _gum_thread_registry_register (registry, &thread);

  g_free (storage);
}

static void
gum_thread_registry_on_pthread_terminate (GumInvocationContext * ic,
                                          gpointer user_data)
{
  GumThreadRegistry * registry = user_data;

  _gum_thread_registry_unregister (registry,
      gum_process_get_current_thread_id ());
}

static void
gum_thread_registry_on_pthread_setname (GumInvocationContext * ic,
                                        gpointer user_data)
{
  GumThreadRegistry * registry = user_data;
  pthread_t pthread;
  const char * name;

  pthread =
      GUM_PTHREAD_FROM_PTR (gum_invocation_context_get_nth_argument (ic, 0));
  name = gum_invocation_context_get_nth_argument (ic, 1);

  _gum_thread_registry_rename (registry,
      gum_linux_query_pthread_tid (pthread, gum_pthread), name);
}

static void
gum_compute_thread_details_from_pthread (pthread_t thread,
                                         const GumLinuxPThreadSpec * spec,
                                         GumThreadDetails * details,
                                         gpointer * storage)
{
  gchar * name;
  gpointer start_routine;

  bzero (details, sizeof (GumThreadDetails));
  *storage = NULL;

  details->id = gum_linux_query_pthread_tid (thread, spec);

  name = gum_linux_query_thread_name (details->id);
  if (name != NULL)
  {
    details->name = name;
    details->flags |= GUM_THREAD_FLAGS_NAME;

    *storage = g_steal_pointer (&name);
  }

  start_routine = gum_linux_query_pthread_start_routine (thread, spec);
  if (start_routine != NULL)
  {
    details->entrypoint.routine = GUM_ADDRESS (start_routine);
    details->entrypoint.parameter = GUM_ADDRESS (
        gum_linux_query_pthread_start_parameter (thread, spec));
    details->flags |=
        GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE |
        GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER;
  }
}
