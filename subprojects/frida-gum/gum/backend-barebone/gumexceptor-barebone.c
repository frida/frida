/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumexceptorbackend.h"

#include "gumprocess.h"
#include "gum/gumbarebone.h"

struct _GumExceptorBackend
{
  GObject parent;

  GumExceptionHandler handler;
  gpointer handler_data;
};

static void gum_exceptor_backend_finalize (GObject * object);

G_DEFINE_TYPE (GumExceptorBackend, gum_exceptor_backend, G_TYPE_OBJECT)

static GumExceptorBackend * the_backend = NULL;

void
_gum_exceptor_backend_prepare_to_fork (void)
{
}

void
_gum_exceptor_backend_recover_from_fork_in_parent (void)
{
}

void
_gum_exceptor_backend_recover_from_fork_in_child (void)
{
}

static void
gum_exceptor_backend_class_init (GumExceptorBackendClass * klass)
{
  G_OBJECT_CLASS (klass)->finalize = gum_exceptor_backend_finalize;
}

static void
gum_exceptor_backend_init (GumExceptorBackend * self)
{
}

static void
gum_exceptor_backend_finalize (GObject * object)
{
  if (the_backend == GUM_EXCEPTOR_BACKEND (object))
    the_backend = NULL;

  G_OBJECT_CLASS (gum_exceptor_backend_parent_class)->finalize (object);
}

GumExceptorBackend *
gum_exceptor_backend_new (GumExceptionHandler handler,
                          gpointer user_data)
{
  GumExceptorBackend * backend;

  backend = g_object_new (GUM_TYPE_EXCEPTOR_BACKEND, NULL);
  backend->handler = handler;
  backend->handler_data = user_data;

  the_backend = backend;

  return backend;
}

gboolean
gum_barebone_handle_exception (GumExceptionType type,
                               gpointer pc,
                               gpointer accessed_address,
                               GumCpuContext * cpu_context)
{
  GumExceptorBackend * self = the_backend;
  GumExceptionDetails details;
  GumExceptionMemoryDetails * md = &details.memory;

  if (self == NULL)
    return FALSE;

  details.thread_id = gum_process_get_current_thread_id ();
  details.type = type;
  details.address = pc;
  if (details.type == GUM_EXCEPTION_ACCESS_VIOLATION)
  {
    md->operation = GUM_MEMOP_INVALID;
    md->address = accessed_address;
  }
  else
  {
    md->operation = GUM_MEMOP_INVALID;
    md->address = NULL;
  }
  details.context = *cpu_context;
  details.native_context = cpu_context;

  if (!self->handler (&details, self->handler_data))
    return FALSE;

  *cpu_context = details.context;

  return TRUE;
}
