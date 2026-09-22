/*
 * Copyright (C) 2010-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_SCOPE_H__
#define __GUM_V8_SCOPE_H__

#include "gumv8script.h"

#include <v8.h>

struct GumV8Core;

class ScriptInterceptorScope
{
public:
  ScriptInterceptorScope (GumV8Script * parent);
  ~ScriptInterceptorScope ();

private:
  GumV8Script * parent;
};

class ScriptStalkerScope
{
public:
  ScriptStalkerScope (GumV8Script * parent);
  ~ScriptStalkerScope ();

  gint pending_level;
  GumStalkerTransformer * transformer;
  GumEventSink * sink;

private:
  GumV8Script * parent;
};

class ScriptScope
{
public:
  ScriptScope (GumV8Script * parent);
  ~ScriptScope ();

  bool HasPendingException () const { return trycatch.HasCaught (); }
  void ProcessAnyPendingException ();
  void PerformPendingIO ();

  void AddTickCallback (v8::Local<v8::Function> callback);
  void AddScheduledSource (GSource * source);

  GumV8Script * parent;
  ScriptStalkerScope stalker_scope;

private:
  ScriptInterceptorScope interceptor_scope;
  v8::Locker locker;
  v8::Isolate::Scope isolate_scope;
  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context;
  v8::Context::Scope context_scope;
  v8::TryCatch trycatch;
  ScriptScope * root_scope;
  ScriptScope * next_scope;
  GumThreadId next_owner;
  GQueue * tick_callbacks;
  GQueue * scheduled_sources;
  GQueue tick_callbacks_storage;
  GQueue scheduled_sources_storage;
};

class ScriptUnlocker
{
public:
  ScriptUnlocker (GumV8Core * core);

private:
  class ExitCurrentScope
  {
  public:
    ExitCurrentScope (GumV8Core * core);
    ~ExitCurrentScope ();

  private:
    GumV8Core * core;
    ScriptScope * scope;
    GumThreadId owner;
  };

  class ExitIsolateScope
  {
  public:
    ExitIsolateScope (v8::Isolate * isolate);
    ~ExitIsolateScope ();

  private:
    v8::Isolate * isolate;
  };

  class ExitInterceptorScope
  {
  public:
    ExitInterceptorScope (GumV8Core * core);
    ~ExitInterceptorScope ();

  private:
    GumInterceptor * interceptor;
  };

  ExitCurrentScope exit_current_scope;
  ExitIsolateScope exit_isolate_scope;
  v8::Unlocker unlocker;
  ExitInterceptorScope exit_interceptor_scope;
};

class GumV8InterceptorIgnoreScope
{
public:
  GumV8InterceptorIgnoreScope ();
  ~GumV8InterceptorIgnoreScope ();

private:
  GumInterceptor * interceptor;
};

#endif
