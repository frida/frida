/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8scope.h"

#include "gumv8interceptor.h"
#include "gumv8script-priv.h"

using namespace v8;

ScriptScope::ScriptScope (GumV8Script * parent)
  : parent (parent),
    stalker_scope (parent),
    interceptor_scope (parent),
    locker (parent->isolate),
    isolate_scope (parent->isolate),
    handle_scope (parent->isolate),
    context (Local<Context>::New (parent->isolate, *parent->context)),
    context_scope (context),
    trycatch (parent->isolate)
{
  auto core = &parent->core;

  _gum_v8_core_pin (core);

  next_scope = core->current_scope;
  next_owner = core->current_owner;
  core->current_scope = this;
  core->current_owner = gum_process_get_current_thread_id ();

  root_scope = this;
  while (root_scope->next_scope != nullptr)
    root_scope = root_scope->next_scope;

  tick_callbacks = &root_scope->tick_callbacks_storage;
  scheduled_sources = &root_scope->scheduled_sources_storage;

  if (this == root_scope)
  {
    g_queue_init (&tick_callbacks_storage);
    g_queue_init (&scheduled_sources_storage);

    g_rec_mutex_lock (&parent->interrupt_mutex);
    parent->executing = TRUE;
    g_rec_mutex_unlock (&parent->interrupt_mutex);
  }

  parent->inspector->idleFinished ();
}

ScriptScope::~ScriptScope ()
{
  auto core = &parent->core;

  ProcessAnyPendingException ();

  if (this == root_scope)
  {
    PerformPendingIO ();

    g_rec_mutex_lock (&parent->interrupt_mutex);
    parent->executing = FALSE;
    if (parent->interrupt == GUM_INTERRUPT_ONCE)
    {
      parent->isolate->CancelTerminateExecution ();
      parent->interrupt = GUM_INTERRUPT_NONE;
    }
    g_rec_mutex_unlock (&parent->interrupt_mutex);
  }

  parent->inspector->idleStarted ();

  core->current_scope = next_scope;
  core->current_owner = next_owner;

  _gum_v8_core_unpin (core);

  auto pending_flush_notify = core->flush_notify;
  if (pending_flush_notify != NULL && core->usage_count == 0)
  {
    core->flush_notify = NULL;

    {
      ScriptUnlocker unlocker (core);

      _gum_v8_core_notify_flushed (core, pending_flush_notify);
    }
  }
}

void
ScriptScope::ProcessAnyPendingException ()
{
  if (trycatch.HasTerminated ())
  {
    trycatch.Reset ();
    _gum_v8_script_handle_termination (parent);
  }
  else if (trycatch.HasCaught ())
  {
    auto exception = trycatch.Exception ();
    trycatch.Reset ();
    _gum_v8_core_on_unhandled_exception (&parent->core, exception);
    trycatch.Reset ();
  }
}

void
ScriptScope::PerformPendingIO ()
{
  auto core = &parent->core;
  auto isolate = parent->isolate;

  bool io_performed;
  do
  {
    io_performed = false;

    isolate->PerformMicrotaskCheckpoint ();

    if (!g_queue_is_empty (tick_callbacks))
    {
      Global<Function> * tick_callback;
      auto receiver = Undefined (isolate);
      while ((tick_callback = (Global<Function> *)
          g_queue_pop_head (tick_callbacks)) != nullptr)
      {
        auto callback = Local<Function>::New (isolate, *tick_callback);

        auto result = callback->Call (context, receiver, 0, nullptr);
        if (result.IsEmpty ())
          ProcessAnyPendingException ();

        delete tick_callback;
      }

      io_performed = true;
    }

    GSource * source;
    while ((source = (GSource *) g_queue_pop_head (scheduled_sources)) != NULL)
    {
      if (!g_source_is_destroyed (source))
      {
        g_source_attach (source,
            gum_script_scheduler_get_js_context (core->scheduler));
      }

      g_source_unref (source);

      io_performed = true;
    }
  }
  while (io_performed);
}

void
ScriptScope::AddTickCallback (Local<Function> callback)
{
  g_queue_push_tail (tick_callbacks,
      new Global<Function> (parent->isolate, callback));
}

void
ScriptScope::AddScheduledSource (GSource * source)
{
  g_queue_push_tail (scheduled_sources, source);
}

ScriptInterceptorScope::ScriptInterceptorScope (GumV8Script * parent)
  : parent (parent)
{
  gum_interceptor_begin_transaction (parent->interceptor.interceptor);
}

ScriptInterceptorScope::~ScriptInterceptorScope ()
{
  GumV8Core * core = &parent->core;

  if (core->transaction_released_owner == gum_process_get_current_thread_id ())
  {
    core->transaction_released_owner = GUM_THREAD_ID_INVALID;
    gum_v8_script_backend_unmark_scope_mutex_trapped (core->backend);
  }
  else
  {
    gum_interceptor_end_transaction (parent->interceptor.interceptor);
  }
}

ScriptStalkerScope::ScriptStalkerScope (GumV8Script * parent)
  : pending_level (0),
    transformer (NULL),
    sink (NULL),
    parent (parent)
{
}

ScriptStalkerScope::~ScriptStalkerScope ()
{
  _gum_v8_stalker_process_pending (&parent->stalker, this);
}

ScriptUnlocker::ScriptUnlocker (GumV8Core * core)
  : exit_current_scope (core),
    exit_isolate_scope (core->isolate),
    unlocker (core->isolate),
    exit_interceptor_scope (core)
{
}

ScriptUnlocker::ExitCurrentScope::ExitCurrentScope (GumV8Core * core)
  : core (core),
    scope (core->current_scope),
    owner (core->current_owner)
{
  core->script->inspector->idleStarted ();

  core->current_scope = nullptr;
  core->current_owner = GUM_THREAD_ID_INVALID;
}

ScriptUnlocker::ExitCurrentScope::~ExitCurrentScope ()
{
  core->current_scope = scope;
  core->current_owner = owner;

  core->script->inspector->idleFinished ();
}

ScriptUnlocker::ExitIsolateScope::ExitIsolateScope (Isolate * isolate)
  : isolate (isolate)
{
  isolate->Exit ();
}

ScriptUnlocker::ExitIsolateScope::~ExitIsolateScope ()
{
  isolate->Enter ();
}

ScriptUnlocker::ExitInterceptorScope::ExitInterceptorScope (
    GumV8Core * core)
  : interceptor (core->script->interceptor.interceptor)
{
  gum_interceptor_end_transaction (interceptor);
}

ScriptUnlocker::ExitInterceptorScope::~ExitInterceptorScope ()
{
  gum_interceptor_begin_transaction (interceptor);
}

GumV8InterceptorIgnoreScope::GumV8InterceptorIgnoreScope ()
{
  interceptor = gum_interceptor_obtain ();
  gum_interceptor_ignore_current_thread (interceptor);
}

GumV8InterceptorIgnoreScope::~GumV8InterceptorIgnoreScope ()
{
  gum_interceptor_unignore_current_thread (interceptor);
  g_object_unref (interceptor);
}
