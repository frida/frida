/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020-2021 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_CORE_H__
#define __GUM_V8_CORE_H__

#include "gumscriptscheduler.h"
#include "gumv8scope.h"
#include "gumv8script.h"
#include "gumv8scriptbackend.h"

#include <gum/gumexceptor.h>
#include <gum/gumprocess.h>
#include <v8.h>

#define GUMJS_NATIVE_POINTER_VALUE(o) \
    GSIZE_TO_POINTER ((o)->GetInternalField (0).As<BigInt> ()->Uint64Value ())
#define GUMJS_CPU_CONTEXT_VALUE(o) \
    ((GumCpuContext *) (o)->GetAlignedPointerFromInternalField (0))

#ifdef HAVE_WINDOWS
# define GUMJS_SYSTEM_ERROR_FIELD "lastError"
#else
# define GUMJS_SYSTEM_ERROR_FIELD "errno"
#endif

struct GumV8ExceptionSink;
struct GumV8MessageSink;

typedef void (* GumV8FlushNotify) (GumV8Script * script);
typedef void (* GumV8MessageEmitter) (GumV8Script * script,
    const gchar * message, GBytes * data);
typedef void (* GumV8KernelDestroyNotify) (guint64 data);

struct GumV8Core
{
  GumV8Script * script;
  GumV8ScriptBackend * backend;
  GumV8Core * core;
  GumV8MessageEmitter message_emitter;
  GumScriptScheduler * scheduler;
  GumExceptor * exceptor;
  v8::Isolate * isolate;

  ScriptScope * current_scope;
  GumThreadId current_owner;
  GumThreadId transaction_released_owner;
  volatile guint usage_count;
  volatile GumV8FlushNotify flush_notify;

  GMainLoop * event_loop;
  GMutex event_mutex;
  GCond event_cond;
  volatile guint event_count;
  volatile gboolean event_source_available;

  GumV8ExceptionSink * unhandled_exception_sink;
  GumV8MessageSink * incoming_message_sink;

  v8::Global<v8::Function> * on_global_get;
  v8::Global<v8::Object> * global_receiver;

  GHashTable * weak_refs;
  guint last_weak_ref_id;
  GQueue pending_weak_callbacks;
  GSource * pending_weak_source;

  GHashTable * scheduled_callbacks;
  guint next_callback_id;

  GHashTable * native_functions;

  GHashTable * native_callbacks;

  GHashTable * native_resources;
  GHashTable * kernel_resources;

  GHashTable * match_patterns;

  GHashTable * source_maps;

  v8::Global<v8::FunctionTemplate> * int64;
  v8::Global<v8::Object> * int64_value;

  v8::Global<v8::FunctionTemplate> * uint64;
  v8::Global<v8::Object> * uint64_value;

  v8::Global<v8::FunctionTemplate> * native_pointer;
  v8::Global<v8::Object> * native_pointer_value;
  v8::Global<v8::String> * handle_key;

  v8::Global<v8::FunctionTemplate> * native_function;
  v8::Global<v8::String> * abi_key;
  v8::Global<v8::String> * scheduling_key;
  v8::Global<v8::String> * exceptions_key;
  v8::Global<v8::String> * traps_key;
  v8::Global<v8::Object> * native_return_value;
  v8::Global<v8::String> * value_key;
  v8::Global<v8::String> * system_error_key;

  v8::Global<v8::FunctionTemplate> * callback_context;
  v8::Global<v8::Object> * callback_context_value;

  v8::Global<v8::FunctionTemplate> * cpu_context;
  v8::Global<v8::Object> * cpu_context_value;

  v8::Global<v8::FunctionTemplate> * match_pattern;

  v8::Global<v8::FunctionTemplate> * source_map;
};

struct GumV8NativeResource
{
  v8::Global<v8::Object> * instance;
  gpointer data;
  gsize size;
  gboolean owns_pages;
  GDestroyNotify notify;
  GumV8Core * core;
};

struct GumV8KernelResource
{
  v8::Global<v8::Object> * instance;
  GumAddress data;
  gsize size;
  GumV8KernelDestroyNotify notify;
  GumV8Core * core;
};

struct GumV8ByteArray
{
  v8::Global<v8::Object> * instance;
  gpointer data;
  gsize size;
  GumV8Core * core;
};

class GumV8SystemErrorPreservationScope
{
public:
  GumV8SystemErrorPreservationScope ()
    : saved_error (gum_thread_get_system_error ())
  {
  }

  ~GumV8SystemErrorPreservationScope ()
  {
    gum_thread_set_system_error (saved_error);
  }

  gint saved_error;
};

G_GNUC_INTERNAL void _gum_v8_core_init (GumV8Core * self,
    GumV8Script * script, GumV8MessageEmitter message_emitter,
    GumScriptScheduler * scheduler, v8::Isolate * isolate,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_core_realize (GumV8Core * self);
G_GNUC_INTERNAL gboolean _gum_v8_core_flush (GumV8Core * self,
    GumV8FlushNotify flush_notify);
G_GNUC_INTERNAL void _gum_v8_core_notify_flushed (GumV8Core * self,
    GumV8FlushNotify func);
G_GNUC_INTERNAL void _gum_v8_core_dispose (GumV8Core * self);
G_GNUC_INTERNAL void _gum_v8_core_finalize (GumV8Core * self);

G_GNUC_INTERNAL void _gum_v8_core_pin (GumV8Core * self);
G_GNUC_INTERNAL void _gum_v8_core_unpin (GumV8Core * self);

G_GNUC_INTERNAL void _gum_v8_core_on_unhandled_exception (
    GumV8Core * self, v8::Local<v8::Value> exception);

G_GNUC_INTERNAL void _gum_v8_core_post (GumV8Core * self, const gchar * message,
    GBytes * data);

G_GNUC_INTERNAL void _gum_v8_core_push_job (GumV8Core * self,
    GumScriptJobFunc job_func, gpointer data, GDestroyNotify data_destroy);

#endif
