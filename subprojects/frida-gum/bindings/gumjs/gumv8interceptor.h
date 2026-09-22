/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_INTERCEPTOR_H__
#define __GUM_V8_INTERCEPTOR_H__

#include "gumv8codewriter.h"
#include "gumv8core.h"

#include <capstone.h>
#include <gum/guminterceptor.h>

struct GumV8InvocationContext;
struct GumV8InvocationArgs;
struct GumV8InvocationReturnValue;

struct GumV8Interceptor
{
  GumV8Core * core;

  GumInterceptor * interceptor;

  GHashTable * invocation_listeners;
  GHashTable * invocation_context_values;
  GHashTable * invocation_args_values;
  GHashTable * invocation_return_values;
  GHashTable * replacement_by_address;
  GSource * flush_timer;

  v8::Global<v8::FunctionTemplate> * invocation_listener;
  v8::Global<v8::FunctionTemplate> * invocation_context;
  v8::Global<v8::FunctionTemplate> * invocation_args;
  v8::Global<v8::FunctionTemplate> * invocation_return;

  v8::Global<v8::Object> * invocation_listener_value;
  v8::Global<v8::Object> * invocation_context_value;
  v8::Global<v8::Object> * invocation_args_value;
  v8::Global<v8::Object> * invocation_return_value;

  GumV8InvocationContext * cached_invocation_context;
  gboolean cached_invocation_context_in_use;

  GumV8InvocationArgs * cached_invocation_args;
  gboolean cached_invocation_args_in_use;

  GumV8InvocationReturnValue * cached_invocation_return_value;
  gboolean cached_invocation_return_value_in_use;

  GumV8CodeWriter * writer;
  v8::Global<v8::Object> * defaults_value;
  gpointer default_redirect_closure;
  csh capstone;
};

struct GumV8InvocationContext
{
  v8::Global<v8::Object> * object;
  GumInvocationContext * handle;
  v8::Global<v8::Object> * cpu_context;
  gboolean dirty;

  GumV8Interceptor * module;
};

struct GumV8InvocationArgs
{
  v8::Global<v8::Object> * object;
  GumInvocationContext * ic;

  GumV8Interceptor * module;
};

G_GNUC_INTERNAL void _gum_v8_interceptor_init (GumV8Interceptor * self,
    GumV8Core * core, GumV8CodeWriter * writer,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_interceptor_realize (GumV8Interceptor * self);
G_GNUC_INTERNAL void _gum_v8_interceptor_flush (GumV8Interceptor * self);
G_GNUC_INTERNAL void _gum_v8_interceptor_dispose (GumV8Interceptor * self);
G_GNUC_INTERNAL void _gum_v8_interceptor_finalize (GumV8Interceptor * self);

G_GNUC_INTERNAL GumV8InvocationContext *
    _gum_v8_interceptor_obtain_invocation_context (GumV8Interceptor * self);
G_GNUC_INTERNAL void _gum_v8_interceptor_release_invocation_context (
    GumV8Interceptor * self, GumV8InvocationContext * jic);
G_GNUC_INTERNAL void _gum_v8_invocation_context_reset (
    GumV8InvocationContext * self, GumInvocationContext * handle);

G_GNUC_INTERNAL GumV8InvocationArgs *
    _gum_v8_interceptor_obtain_invocation_args (GumV8Interceptor * self);
G_GNUC_INTERNAL void _gum_v8_interceptor_release_invocation_args (
    GumV8Interceptor * self, GumV8InvocationArgs * args);
G_GNUC_INTERNAL void _gum_v8_invocation_args_reset (GumV8InvocationArgs * self,
    GumInvocationContext * ic);

#endif
