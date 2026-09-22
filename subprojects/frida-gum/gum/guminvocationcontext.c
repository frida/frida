/*
 * Copyright (C) 2008-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminvocationcontext.h"

#include "guminterceptor-priv.h"

GumPointCut
gum_invocation_context_get_point_cut (GumInvocationContext * context)
{
  return context->backend->get_point_cut (context);
}

gpointer
gum_invocation_context_get_nth_argument (GumInvocationContext * context,
                                         guint n)
{
  return gum_cpu_context_get_nth_argument (context->cpu_context, n);
}

void
gum_invocation_context_replace_nth_argument (GumInvocationContext * context,
                                             guint n,
                                             gpointer value)
{
  gum_cpu_context_replace_nth_argument (context->cpu_context, n, value);
}

gpointer
gum_invocation_context_get_return_value (GumInvocationContext * context)
{
  return gum_cpu_context_get_return_value (context->cpu_context);
}

void
gum_invocation_context_replace_return_value (GumInvocationContext * context,
                                             gpointer value)
{
  gum_cpu_context_replace_return_value (context->cpu_context, value);
}

gpointer
gum_invocation_context_get_return_address (GumInvocationContext * context)
{
  return _gum_interceptor_peek_top_caller_return_address ();
}

guint
gum_invocation_context_get_thread_id (GumInvocationContext * context)
{
  return context->backend->get_thread_id (context);
}

guint
gum_invocation_context_get_depth (GumInvocationContext * context)
{
  return context->backend->get_depth (context);
}

gpointer
gum_invocation_context_get_listener_thread_data (
    GumInvocationContext * context,
    gsize required_size)
{
  return context->backend->get_listener_thread_data (context, required_size);
}

gpointer
gum_invocation_context_get_listener_function_data (
    GumInvocationContext * context)
{
  return context->backend->get_listener_function_data (context);
}

gpointer
gum_invocation_context_get_listener_invocation_data (
    GumInvocationContext * context,
    gsize required_size)
{
  return context->backend->get_listener_invocation_data (context,
      required_size);
}

gpointer
gum_invocation_context_get_replacement_data (GumInvocationContext * context)
{
  return context->backend->get_replacement_data (context);
}
