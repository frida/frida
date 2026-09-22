#ifndef __GUM_INTERCEPTOR_H__
#define __GUM_INTERCEPTOR_H__

#include "gumdefs.h"

#define GUM_IC_GET_THREAD_DATA(ic, data_type) ((data_type *) \
    gum_invocation_context_get_listener_thread_data (ic, sizeof (data_type)))
#define GUM_IC_GET_FUNC_DATA(ic, data_type) ((data_type) \
    gum_invocation_context_get_listener_function_data (ic))
#define GUM_IC_GET_INVOCATION_DATA(ic, data_type) ((data_type *) \
    gum_invocation_context_get_listener_invocation_data (ic, \
        sizeof (data_type)))

#define GUM_IC_GET_REPLACEMENT_DATA(ctx, data_type) \
    ((data_type) gum_invocation_context_get_replacement_data (ctx))

typedef struct _GumInvocationContext GumInvocationContext;
typedef struct _GumInvocationBackend GumInvocationBackend;

struct _GumInvocationContext
{
  GCallback function;
  GumCpuContext * cpu_context;
  gint system_error;

  GumInvocationBackend * backend;
};

GumInvocationContext * gum_interceptor_get_current_invocation (void);

gpointer gum_invocation_context_get_nth_argument (GumInvocationContext * ic,
    guint n);
void gum_invocation_context_replace_nth_argument (GumInvocationContext * ic,
    guint n, gpointer value);
gpointer gum_invocation_context_get_return_value (GumInvocationContext * ic);
void gum_invocation_context_replace_return_value (GumInvocationContext * ic,
    gpointer value);

gpointer gum_invocation_context_get_return_address (GumInvocationContext * ic);

guint gum_invocation_context_get_thread_id (GumInvocationContext * ic);
guint gum_invocation_context_get_depth (GumInvocationContext * ic);

gpointer gum_invocation_context_get_listener_thread_data (
    GumInvocationContext * ic, gsize required_size);
gpointer gum_invocation_context_get_listener_function_data (
    GumInvocationContext * ic);
gpointer gum_invocation_context_get_listener_invocation_data (
    GumInvocationContext * ic, gsize required_size);

gpointer gum_invocation_context_get_replacement_data (
    GumInvocationContext * ic);

#endif
