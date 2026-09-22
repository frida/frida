/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumthreadregistry-priv.h"

#include "guminterceptor.h"
#include "gum/gumdarwin.h"

#include <strings.h>
#include <pthread/introspection.h>

static void gum_thread_registry_on_thread_event (unsigned int event,
    pthread_t thread, void * addr, size_t size);
static void gum_thread_registry_on_setname (GumInvocationContext * ic,
    gpointer user_data);

static void gum_compute_thread_details_from_pthread (pthread_t thread,
    const GumDarwinPThreadSpec * spec, GumThreadDetails * details);

static GumThreadRegistry * gum_registry;
static const GumDarwinPThreadSpec * gum_pthread;

static gboolean gum_hook_installed = FALSE;
static pthread_introspection_hook_t gum_previous_hook;

static GumInterceptor * gum_thread_interceptor;
static GumInvocationListener * gum_rename_handler;

void
_gum_thread_registry_activate (GumThreadRegistry * self)
{
  GumDarwinPThreadIter iter;
  pthread_t thread;
  GumAttachOptions options = { .ignorability = GUM_INVOCATION_UNIGNORABLE };

  gum_registry = self;

  gum_pthread = gum_darwin_query_pthread_spec ();

  gum_rename_handler = gum_make_call_listener (NULL,
      gum_thread_registry_on_setname, gum_registry, NULL);

  gum_thread_interceptor = gum_interceptor_obtain ();

  gum_darwin_lock_pthread_list (gum_pthread);

  gum_previous_hook =
      pthread_introspection_hook_install (gum_thread_registry_on_thread_event);
  gum_hook_installed = TRUE;

  gum_interceptor_attach (gum_thread_interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (
          gum_process_get_libc_module (), "pthread_setname_np")),
      gum_rename_handler, &options);

  gum_darwin_pthread_iter_init (&iter, gum_pthread);
  while (gum_darwin_pthread_iter_next (&iter, &thread))
  {
    GumThreadDetails t;

    gum_compute_thread_details_from_pthread (thread, gum_pthread, &t);

    _gum_thread_registry_register (self, &t);
  }

  gum_darwin_unlock_pthread_list (gum_pthread);
}

void
_gum_thread_registry_deactivate (GumThreadRegistry * self)
{
  if (gum_rename_handler != NULL)
  {
    gum_interceptor_detach (gum_thread_interceptor, gum_rename_handler);

    g_object_unref (gum_rename_handler);
    gum_rename_handler = NULL;

    g_object_unref (gum_thread_interceptor);
    gum_thread_interceptor = NULL;
  }

  if (gum_hook_installed)
  {
    (void) pthread_introspection_hook_install (gum_previous_hook);
    gum_previous_hook = NULL;

    gum_hook_installed = FALSE;
  }
}

static void
gum_thread_registry_on_thread_event (unsigned int event,
                                     pthread_t thread,
                                     void * addr,
                                     size_t size)
{
  switch (event)
  {
    case PTHREAD_INTROSPECTION_THREAD_START:
    {
      GumThreadDetails t;

      gum_compute_thread_details_from_pthread (thread, gum_pthread, &t);

      _gum_thread_registry_register (gum_registry, &t);

      break;
    }
    case PTHREAD_INTROSPECTION_THREAD_TERMINATE:
    {
      _gum_thread_registry_unregister (gum_registry,
          pthread_mach_thread_np (thread));
      break;
    }
    default:
      break;
  }

  if (gum_previous_hook != NULL)
    gum_previous_hook (event, thread, addr, size);
}

static void
gum_thread_registry_on_setname (GumInvocationContext * ic,
                                gpointer user_data)
{
  GumThreadRegistry * registry = user_data;
  pthread_t thread;
  GumThreadId id;
  const char * name;

  thread = pthread_self ();

  id = pthread_mach_thread_np (thread);

  name = (char *) pthread_self () + gum_pthread->name_offset;
  if (name[0] == '\0')
    name = NULL;

  _gum_thread_registry_rename (registry, id, name);
}

static void
gum_compute_thread_details_from_pthread (pthread_t thread,
                                         const GumDarwinPThreadSpec * spec,
                                         GumThreadDetails * details)
{
  gpointer start_routine;

  bzero (details, sizeof (GumThreadDetails));

  details->id = gum_darwin_query_pthread_port (thread, spec);

  details->name = gum_darwin_query_pthread_name (thread, spec);
  if (details->name != NULL)
    details->flags |= GUM_THREAD_FLAGS_NAME;

  start_routine = gum_darwin_query_pthread_start_routine (thread, spec);
  if (start_routine != NULL)
  {
    details->entrypoint.routine = GUM_ADDRESS (start_routine);
    details->entrypoint.parameter = GUM_ADDRESS (
        gum_darwin_query_pthread_start_parameter (thread, spec));
    details->flags |=
        GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE |
        GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER;
  }
}
