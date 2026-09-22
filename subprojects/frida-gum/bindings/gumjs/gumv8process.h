/*
 * Copyright (C) 2010-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_PROCESS_H__
#define __GUM_V8_PROCESS_H__

#include "gumv8core.h"
#include "gumv8module.h"
#include "gumv8thread.h"

struct GumV8ExceptionHandler;

struct GumV8Process
{
  GumV8Module * module;
  GumV8Thread * thread;
  GumV8Core * core;

  GHashTable * thread_observers;
  GHashTable * module_observers;

  v8::Global<v8::FunctionTemplate> * thread_observer;
  v8::Global<v8::FunctionTemplate> * module_observer;

  v8::Global<v8::Object> * thread_observer_value;
  v8::Global<v8::Object> * module_observer_value;
  v8::Global<v8::Object> * main_module_value;

  GumStalker * stalker;
  GSource * stalker_gc_timer;

  GumV8ExceptionHandler * exception_handler;
};

G_GNUC_INTERNAL void _gum_v8_process_init (GumV8Process * self,
    GumV8Module * module, GumV8Thread * thread, GumV8Core * core,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_process_realize (GumV8Process * self);
G_GNUC_INTERNAL void _gum_v8_process_flush (GumV8Process * self);
G_GNUC_INTERNAL void _gum_v8_process_dispose (GumV8Process * self);
G_GNUC_INTERNAL void _gum_v8_process_finalize (GumV8Process * self);

#endif
