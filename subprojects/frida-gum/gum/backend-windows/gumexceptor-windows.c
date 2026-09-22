/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumexceptorbackend.h"

#include "gum/gumwindows.h"

struct _GumExceptorBackend
{
  GObject parent;

  GumExceptionHandler handler;
  gpointer handler_data;

  void * vectored_handler;
};

static void gum_exceptor_backend_finalize (GObject * object);

static LONG NTAPI gum_exceptor_backend_dispatch (
    EXCEPTION_POINTERS * exception_info);

G_DEFINE_TYPE (GumExceptorBackend, gum_exceptor_backend, G_TYPE_OBJECT)

static GumExceptorBackend * the_backend = NULL;
static GPrivate gum_active_context_key;

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
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_exceptor_backend_finalize;
}

static void
gum_exceptor_backend_init (GumExceptorBackend * self)
{
  the_backend = self;

  self->vectored_handler =
      AddVectoredExceptionHandler (TRUE, gum_exceptor_backend_dispatch);
}

static void
gum_exceptor_backend_finalize (GObject * object)
{
  GumExceptorBackend * self = GUM_EXCEPTOR_BACKEND (object);

  RemoveVectoredExceptionHandler (self->vectored_handler);

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

  return backend;
}

static LONG NTAPI
gum_exceptor_backend_dispatch (EXCEPTION_POINTERS * exception_info)
{
  EXCEPTION_RECORD * exception_record = exception_info->ExceptionRecord;
  CONTEXT * context = exception_info->ContextRecord;
  GumExceptorBackend * self = the_backend;
  GumExceptionDetails ed;
  GumExceptionMemoryDetails * md = &ed.memory;
  GumCpuContext * cpu_context = &ed.context;
  gboolean handled;

  ed.thread_id = gum_process_get_current_thread_id ();

  switch (exception_record->ExceptionCode)
  {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      ed.type = GUM_EXCEPTION_ACCESS_VIOLATION;
      break;
    case EXCEPTION_GUARD_PAGE:
      ed.type = GUM_EXCEPTION_GUARD_PAGE;
      break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
      ed.type = GUM_EXCEPTION_ILLEGAL_INSTRUCTION;
      break;
    case EXCEPTION_STACK_OVERFLOW:
      ed.type = GUM_EXCEPTION_STACK_OVERFLOW;
      break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
      ed.type = GUM_EXCEPTION_ARITHMETIC;
      break;
    case EXCEPTION_BREAKPOINT:
      ed.type = GUM_EXCEPTION_BREAKPOINT;
      break;
    case EXCEPTION_SINGLE_STEP:
      ed.type = GUM_EXCEPTION_SINGLE_STEP;
      break;
    default:
      ed.type = GUM_EXCEPTION_SYSTEM;
      break;
  }

  ed.address = exception_record->ExceptionAddress;

  switch (exception_record->ExceptionCode)
  {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_IN_PAGE_ERROR:
      switch (exception_record->ExceptionInformation[0])
      {
        case 0:
          md->operation = GUM_MEMOP_READ;
          break;
        case 1:
          md->operation = GUM_MEMOP_WRITE;
          break;
        case 8:
          md->operation = GUM_MEMOP_EXECUTE;
          break;
        default:
          md->operation = GUM_MEMOP_INVALID;
          break;
      }
      md->address =
          GSIZE_TO_POINTER (exception_record->ExceptionInformation[1]);
      break;
    default:
      md->operation = GUM_MEMOP_INVALID;
      md->address = 0;
      break;
  }

  gum_windows_parse_context (context, cpu_context);
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  cpu_context->xmm = (GumX86VectorReg *) &context->Xmm0;
#endif
  ed.native_context = context;

  g_private_set (&gum_active_context_key, context);
  handled = self->handler (&ed, self->handler_data);
  g_private_set (&gum_active_context_key, NULL);

  if (handled)
  {
    gum_windows_unparse_context (cpu_context, context);
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

CONTEXT *
gum_windows_get_active_exceptor_context (void)
{
  return g_private_get (&gum_active_context_key);
}
