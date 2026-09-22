/*
 * Copyright (C) 2020-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_INTERCEPTOR_H__
#define __GUM_QUICK_INTERCEPTOR_H__

#include "gumquickcodewriter.h"
#include "gumquickcore.h"

#include <capstone.h>
#include <gum/guminterceptor.h>

G_BEGIN_DECLS

typedef struct _GumQuickInvocationContext GumQuickInvocationContext;
typedef struct _GumQuickInvocationArgs GumQuickInvocationArgs;
typedef struct _GumQuickInvocationRetval GumQuickInvocationRetval;

struct _GumQuickInterceptor
{
  GumQuickCore * core;

  GumInterceptor * interceptor;

  GHashTable * invocation_listeners;
  GHashTable * replacement_by_address;
  GSource * flush_timer;

  JSClassID invocation_listener_class;
  JSClassID invocation_context_class;
  JSClassID invocation_args_class;
  JSClassID invocation_retval_class;

  GumQuickInvocationContext * cached_invocation_context;
  gboolean cached_invocation_context_in_use;

  GumQuickInvocationArgs * cached_invocation_args;
  gboolean cached_invocation_args_in_use;

  GumQuickInvocationRetval * cached_invocation_retval;
  gboolean cached_invocation_retval_in_use;

  GumQuickCodeWriter * writer;
  JSValue defaults_value;
  gpointer default_redirect_closure;
  csh capstone;
};

struct _GumQuickInvocationContext
{
  JSValue wrapper;
  GumInvocationContext * handle;
  GumQuickCpuContext * cpu_context;
  int initial_property_count;

  GumQuickInterceptor * interceptor;
};

struct _GumQuickInvocationArgs
{
  JSValue wrapper;
  GumInvocationContext * ic;
  JSContext * ctx;
};

G_GNUC_INTERNAL void _gum_quick_interceptor_init (GumQuickInterceptor * self,
    JSValue ns, GumQuickCodeWriter * writer, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_interceptor_flush (GumQuickInterceptor * self);
G_GNUC_INTERNAL void _gum_quick_interceptor_dispose (
    GumQuickInterceptor * self);
G_GNUC_INTERNAL void _gum_quick_interceptor_finalize (
    GumQuickInterceptor * self);

G_GNUC_INTERNAL GumQuickInvocationContext *
    _gum_quick_interceptor_obtain_invocation_context (
        GumQuickInterceptor * self);
G_GNUC_INTERNAL void _gum_quick_interceptor_release_invocation_context (
    GumQuickInterceptor * self, GumQuickInvocationContext * jic);
G_GNUC_INTERNAL void _gum_quick_invocation_context_reset (
    GumQuickInvocationContext * self, GumInvocationContext * handle);

G_GNUC_INTERNAL GumQuickInvocationArgs *
    _gum_quick_interceptor_obtain_invocation_args (GumQuickInterceptor * self);
G_GNUC_INTERNAL void _gum_quick_interceptor_release_invocation_args (
    GumQuickInterceptor * self, GumQuickInvocationArgs * args);
G_GNUC_INTERNAL void _gum_quick_invocation_args_reset (
    GumQuickInvocationArgs * self, GumInvocationContext * ic);

G_END_DECLS

#endif
