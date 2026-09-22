/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2013 Karl Trygve Kalleberg <karltk@boblycat.org>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8script.h"

#include "gumscripttask.h"
#include "gumsourcemap.h"
#include "gumv8script-priv.h"
#include "gumv8script-runtime.h"
#include "gumv8value.h"

#include <cstring>

#define GUM_V8_INSPECTOR_LOCK(o) g_mutex_lock (&(o)->inspector_mutex)
#define GUM_V8_INSPECTOR_UNLOCK(o) g_mutex_unlock (&(o)->inspector_mutex)

using namespace v8;
using namespace v8_inspector;

typedef void (* GumUnloadNotifyFunc) (GumV8Script * self, gpointer user_data);
typedef void (* GumRuntimeLoadedFunc) (Local<Value> error, gpointer user_data);

enum
{
  CONTEXT_CREATED,
  CONTEXT_DESTROYED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_SOURCE,
  PROP_SNAPSHOT,
  PROP_MAIN_CONTEXT,
  PROP_BACKEND
};

struct GumImportOperation
{
  GumV8Script * self;
  Global<Promise::Resolver> * resolver;
  Global<Module> * module;
};

struct GumUnloadNotifyCallback
{
  GumUnloadNotifyFunc func;
  gpointer data;
  GDestroyNotify data_destroy;
};

struct GumPostData
{
  GumV8Script * script;
  gchar * message;
  GBytes * data;
};

struct GumEmitData
{
  GumV8Script * script;
  gchar * message;
  GBytes * data;
};

struct GumEmitDebugMessageData
{
  GumV8Script * script;
  gchar * message;
};

class GumInspectorClient : public V8InspectorClient
{
public:
  GumInspectorClient (GumV8Script * script);

  void runMessageLoopOnPause (int context_group_id) override;
  void quitMessageLoopOnPause () override;

  Local<Context> ensureDefaultContextInGroup (int contextGroupId) override;

  double currentTimeMS () override;

private:
  void startSkippingAllPauses ();

  GumV8Script * script;
};

class GumInspectorChannel : public V8Inspector::Channel
{
public:
  GumInspectorChannel (GumV8Script * script, guint id);

  void takeSession (std::unique_ptr<V8InspectorSession> session);
  void dispatchStanza (const char * stanza);
  void startSkippingAllPauses ();

  void sendResponse (int call_id,
      std::unique_ptr<StringBuffer> message) override;
  void sendNotification (std::unique_ptr<StringBuffer> message) override;
  void flushProtocolNotifications () override;

private:
  void emitStanza (std::unique_ptr<StringBuffer> stanza);

  GumV8Script * script;
  guint id;
  std::unique_ptr<V8InspectorSession> inspector_session;
};

struct GumLoadRuntimeData
{
  GumRuntimeLoadedFunc func;
  gpointer data;
  GDestroyNotify data_destroy;
};

static void gum_v8_script_iface_init (gpointer g_iface, gpointer iface_data);

static void gum_v8_script_constructed (GObject * object);
static void gum_v8_script_dispose (GObject * object);
static void gum_v8_script_finalize (GObject * object);
static void gum_v8_script_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gum_v8_script_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static GumESProgram * gum_v8_script_compile (GumV8Script * self,
    Isolate * isolate, Local<Context> context, GError ** error);
static MaybeLocal<Promise> gum_import_module (Local<Context> context,
    Local<Data> host_defined_options, Local<Value> resource_name,
    Local<String> specifier, Local<FixedArray> import_assertions);
static void gum_on_import_success (const FunctionCallbackInfo<Value> & info);
static void gum_on_import_failure (const FunctionCallbackInfo<Value> & info);
static void gum_import_operation_free (GumImportOperation * op);
static MaybeLocal<Module> gum_resolve_module (Local<Context> context,
    Local<String> specifier, Local<FixedArray> import_assertions,
    Local<Module> referrer);
static gchar * gum_normalize_module_name (const gchar * base_name,
    const gchar * name, GumESProgram * program);
static MaybeLocal<Module> gum_ensure_module_defined (Isolate * isolate,
    Local<Context> context, GumESAsset * asset, GumESProgram * program);
static void gum_v8_script_destroy_context (GumV8Script * self);

static void gum_v8_script_load (GumScript * script, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
static void gum_v8_script_load_finish (GumScript * script,
    GAsyncResult * result);
static void gum_v8_script_load_sync (GumScript * script,
    GCancellable * cancellable);
static void gum_v8_script_do_load (GumScriptTask * task, GumV8Script * self,
    gpointer task_data, GCancellable * cancellable);
static void gum_v8_script_execute_runtime (GumV8Script * self,
    GumScriptTask * task);
static void gum_v8_script_on_runtime_loaded (Local<Value> error,
    gpointer user_data);
static void gum_v8_script_execute_entrypoints (GumV8Script * self,
    GumScriptTask * task);
static void gum_v8_script_on_entrypoints_executed (
    const FunctionCallbackInfo<Value> & info);
static void gum_v8_script_schedule_load_task_completion (GumV8Script * self,
    GumScriptTask * task);
static gboolean gum_v8_script_complete_load_task (GumScriptTask * task);
static void gum_v8_script_unload (GumScript * script,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_v8_script_unload_finish (GumScript * script,
    GAsyncResult * result);
static void gum_v8_script_unload_sync (GumScript * script,
    GCancellable * cancellable);
static void gum_v8_script_do_unload (GumScriptTask * task, GumV8Script * self,
    gpointer task_data, GCancellable * cancellable);
static void gum_v8_script_complete_unload_task (GumV8Script * self,
    GumScriptTask * task);
static void gum_v8_script_try_unload (GumV8Script * self);
static void gum_v8_script_once_unloaded (GumV8Script * self,
    GumUnloadNotifyFunc func, gpointer data, GDestroyNotify data_destroy);
static void gum_v8_script_interrupt (GumScript * script);
static void gum_v8_script_terminate (GumScript * script);

static void gum_v8_script_set_message_handler (GumScript * script,
    GumScriptMessageHandler handler, gpointer data,
    GDestroyNotify data_destroy);
static void gum_v8_script_post (GumScript * script, const gchar * message,
    GBytes * data);
static void gum_v8_script_do_post (GumPostData * d);
static void gum_v8_post_data_free (GumPostData * d);

static void gum_v8_script_emit (GumV8Script * self, const gchar * message,
    GBytes * data);
static gboolean gum_v8_script_do_emit (GumEmitData * d);
static void gum_v8_emit_data_free (GumEmitData * d);

static void gum_v8_script_set_debug_message_handler (GumScript * backend,
    GumScriptDebugMessageHandler handler, gpointer data,
    GDestroyNotify data_destroy);
static void gum_v8_script_post_debug_message (GumScript * backend,
    const gchar * message);
static void gum_v8_script_process_queued_debug_messages (GumV8Script * self);
static void gum_v8_script_process_queued_debug_messages_unlocked (
    GumV8Script * self);
static void gum_v8_script_drop_queued_debug_messages_unlocked (
    GumV8Script * self);
static void gum_v8_script_process_debug_message (GumV8Script * self,
    const gchar * message);
static gboolean gum_v8_script_do_emit_debug_message (
    GumEmitDebugMessageData * d);
static void gum_emit_debug_message_data_free (GumEmitDebugMessageData * d);
static void gum_v8_script_clear_inspector_channels (GumV8Script * self);
static void gum_v8_script_connect_inspector_channel (GumV8Script * self,
    guint id);
static void gum_v8_script_disconnect_inspector_channel (GumV8Script * self,
    guint id);
static void gum_v8_script_dispatch_inspector_stanza (GumV8Script * self,
    guint channel_id, const gchar * stanza);

static GumStalker * gum_v8_script_get_stalker (GumScript * script);

static void gum_v8_script_on_fatal_error (const char * location,
    const char * message);

static GumESProgram * gum_es_program_new (void);
static void gum_es_program_free (GumESProgram * program);
static void gum_es_program_load_runtime (GumESProgram * self, Isolate * isolate,
    Local<Context> context, GumRuntimeLoadedFunc func, gpointer data,
    GDestroyNotify data_destroy);
static void gum_es_program_on_runtime_load_success (
    const FunctionCallbackInfo<Value> & info);
static void gum_es_program_on_runtime_load_failure (
    const FunctionCallbackInfo<Value> & info);
static void gum_es_program_on_runtime_loaded (
    const FunctionCallbackInfo<Value> & info, Local<Value> error);

static GumESAsset * gum_es_asset_new (const gchar * name, gconstpointer data,
    gsize data_size, GDestroyNotify data_destroy);
static GumESAsset * gum_es_asset_ref (GumESAsset * asset);
static void gum_es_asset_unref (GumESAsset * asset);
static void gum_es_asset_release_data (GumESAsset * asset);

static std::unique_ptr<StringBuffer> gum_string_buffer_from_utf8 (
    const gchar * str);
static gchar * gum_string_view_to_utf8 (const StringView & view);

G_DEFINE_TYPE_EXTENDED (GumV8Script,
                        gum_v8_script,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SCRIPT,
                            gum_v8_script_iface_init))

static guint gum_v8_script_signals[LAST_SIGNAL] = { 0, };

static void
gum_v8_script_class_init (GumV8ScriptClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gum_v8_script_constructed;
  object_class->dispose = gum_v8_script_dispose;
  object_class->finalize = gum_v8_script_finalize;
  object_class->get_property = gum_v8_script_get_property;
  object_class->set_property = gum_v8_script_set_property;

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Name", NULL,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_SOURCE,
      g_param_spec_string ("source", "Source", "Source code", NULL,
      (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_SNAPSHOT,
      g_param_spec_boxed ("snapshot", "Snapshot", "Snapshot", G_TYPE_BYTES,
      (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MAIN_CONTEXT,
      g_param_spec_boxed ("main-context", "MainContext",
      "MainContext being used", G_TYPE_MAIN_CONTEXT,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_BACKEND,
      g_param_spec_object ("backend", "Backend", "Backend being used",
      GUM_V8_TYPE_SCRIPT_BACKEND,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS)));

  gum_v8_script_signals[CONTEXT_CREATED] = g_signal_new ("context-created",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
  gum_v8_script_signals[CONTEXT_DESTROYED] = g_signal_new ("context-destroyed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
gum_v8_script_iface_init (gpointer g_iface,
                          gpointer iface_data)
{
  auto iface = (GumScriptInterface *) g_iface;

  iface->load = gum_v8_script_load;
  iface->load_finish = gum_v8_script_load_finish;
  iface->load_sync = gum_v8_script_load_sync;
  iface->unload = gum_v8_script_unload;
  iface->unload_finish = gum_v8_script_unload_finish;
  iface->unload_sync = gum_v8_script_unload_sync;
  iface->interrupt = gum_v8_script_interrupt;
  iface->terminate = gum_v8_script_terminate;

  iface->set_message_handler = gum_v8_script_set_message_handler;
  iface->post = gum_v8_script_post;

  iface->set_debug_message_handler = gum_v8_script_set_debug_message_handler;
  iface->post_debug_message = gum_v8_script_post_debug_message;

  iface->get_stalker = gum_v8_script_get_stalker;
}

static void
gum_v8_script_init (GumV8Script * self)
{
  self->state = GUM_SCRIPT_STATE_CREATED;
  self->on_unload = NULL;

  g_mutex_init (&self->inspector_mutex);
  g_cond_init (&self->inspector_cond);
  self->inspector_state = GUM_V8_RUNNING;
  self->context_group_id = 1;

  g_queue_init (&self->debug_messages);
  self->flush_scheduled = false;

  self->channels = new GumInspectorChannelMap ();
}

static void
gum_v8_script_constructed (GObject * object)
{
  auto self = GUM_V8_SCRIPT (object);

  G_OBJECT_CLASS (gum_v8_script_parent_class)->constructed (object);

  Isolate::CreateParams params;
  params.snapshot_blob = self->snapshot_blob;
  params.array_buffer_allocator =
      ((GumV8Platform *) gum_v8_script_backend_get_platform (self->backend))
      ->GetArrayBufferAllocator ();

  Isolate * isolate = Isolate::New (params);
  isolate->SetData (0, self);
  isolate->SetFatalErrorHandler (gum_v8_script_on_fatal_error);
  isolate->SetMicrotasksPolicy (MicrotasksPolicy::kExplicit);
  isolate->SetHostImportModuleDynamicallyCallback (gum_import_module);
  self->isolate = isolate;

  {
    Locker locker (isolate);
    Isolate::Scope isolate_scope (isolate);
    HandleScope handle_scope (isolate);

    auto client = new GumInspectorClient (self);
    self->inspector_client = client;

    auto inspector = V8Inspector::create (isolate, client);
    self->inspector = inspector.release ();
  }
}

static void
gum_v8_script_dispose (GObject * object)
{
  auto self = GUM_V8_SCRIPT (object);
  auto script = GUM_SCRIPT (self);

  gum_v8_script_set_message_handler (script, NULL, NULL, NULL);

  g_rec_mutex_lock (&self->interrupt_mutex);
  if (self->state == GUM_SCRIPT_STATE_LOADED)
  {
    /* dispose() will be triggered again at the end of unload() */
    gum_v8_script_unload (script, NULL, NULL, NULL);
  }
  else
  {
    if (self->state == GUM_SCRIPT_STATE_CREATED && self->context != nullptr)
      gum_v8_script_destroy_context (self);

    g_clear_pointer (&self->debug_handler_context, g_main_context_unref);
    if (self->debug_handler_data_destroy != NULL)
      self->debug_handler_data_destroy (self->debug_handler_data);
    self->debug_handler = NULL;
    self->debug_handler_data = NULL;
    self->debug_handler_data_destroy = NULL;

    GUM_V8_INSPECTOR_LOCK (self);
    self->inspector_state = GUM_V8_RUNNING;
    g_cond_signal (&self->inspector_cond);
    GUM_V8_INSPECTOR_UNLOCK (self);

    gum_v8_script_clear_inspector_channels (self);

    GUM_V8_INSPECTOR_LOCK (self);
    gum_v8_script_drop_queued_debug_messages_unlocked (self);
    GUM_V8_INSPECTOR_UNLOCK (self);

    delete self->channels;
    self->channels = nullptr;

    {
      auto isolate = self->isolate;
      Locker locker (isolate);
      Isolate::Scope isolate_scope (isolate);
      HandleScope handle_scope (isolate);

      delete self->inspector;
      self->inspector = nullptr;

      delete self->inspector_client;
      self->inspector_client = nullptr;
    }

    auto platform =
        (GumV8Platform *) gum_v8_script_backend_get_platform (self->backend);
    platform->DisposeIsolate (&self->isolate);

    g_clear_pointer (&self->main_context, g_main_context_unref);
    g_clear_pointer (&self->backend, g_object_unref);
  }
  g_rec_mutex_unlock (&self->interrupt_mutex);

  G_OBJECT_CLASS (gum_v8_script_parent_class)->dispose (object);
}

static void
gum_v8_script_finalize (GObject * object)
{
  auto self = GUM_V8_SCRIPT (object);

  g_rec_mutex_clear (&self->interrupt_mutex);

  g_cond_clear (&self->inspector_cond);
  g_mutex_clear (&self->inspector_mutex);

  g_free (self->name);
  g_free (self->source);
  g_bytes_unref (self->snapshot);

  G_OBJECT_CLASS (gum_v8_script_parent_class)->finalize (object);
}

static void
gum_v8_script_get_property (GObject * object,
                            guint property_id,
                            GValue * value,
                            GParamSpec * pspec)
{
  auto self = GUM_V8_SCRIPT (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_MAIN_CONTEXT:
      g_value_set_boxed (value, self->main_context);
      break;
    case PROP_BACKEND:
      g_value_set_object (value, self->backend);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_v8_script_set_property (GObject * object,
                            guint property_id,
                            const GValue * value,
                            GParamSpec * pspec)
{
  auto self = GUM_V8_SCRIPT (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;
    case PROP_SOURCE:
      g_free (self->source);
      self->source = g_value_dup_string (value);
      break;
    case PROP_SNAPSHOT:
      g_bytes_unref (self->snapshot);
      self->snapshot = (GBytes *) g_value_dup_boxed (value);

      if (self->snapshot != NULL)
      {
        gsize size;
        gconstpointer data = g_bytes_get_data (self->snapshot, &size);

        self->snapshot_blob_storage = { (const char *) data, (int) size };
        self->snapshot_blob = &self->snapshot_blob_storage;
      }
      else
      {
        self->snapshot_blob = NULL;
      }

      break;
    case PROP_MAIN_CONTEXT:
      if (self->main_context != NULL)
        g_main_context_unref (self->main_context);
      self->main_context = (GMainContext *) g_value_dup_boxed (value);
      break;
    case PROP_BACKEND:
      if (self->backend != NULL)
        g_object_unref (self->backend);
      self->backend = GUM_V8_SCRIPT_BACKEND (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

gboolean
gum_v8_script_create_context (GumV8Script * self,
                              GError ** error)
{
  g_assert (self->context == NULL);

  {
    Isolate * isolate = self->isolate;
    Locker locker (isolate);
    Isolate::Scope isolate_scope (isolate);
    HandleScope handle_scope (isolate);

    self->inspector->idleStarted ();

    auto global_templ = ObjectTemplate::New (isolate);
    _gum_v8_core_init (&self->core, self, gum_v8_script_emit,
        gum_v8_script_backend_get_scheduler (self->backend), isolate,
        global_templ);
    _gum_v8_kernel_init (&self->kernel, &self->core, global_templ);
    _gum_v8_memory_init (&self->memory, &self->core, global_templ);
    _gum_v8_module_init (&self->module, &self->core, global_templ);
    _gum_v8_thread_init (&self->thread, &self->core, global_templ);
    _gum_v8_process_init (&self->process, &self->module, &self->thread,
        &self->core, global_templ);
    _gum_v8_file_init (&self->file, &self->core, global_templ);
    _gum_v8_checksum_init (&self->checksum, &self->core, global_templ);
#ifndef G_OS_NONE
    _gum_v8_stream_init (&self->stream, &self->core, global_templ);
    _gum_v8_socket_init (&self->socket, &self->core, global_templ);
#endif
#ifdef HAVE_SQLITE
    _gum_v8_database_init (&self->database, &self->core, global_templ);
#endif
    _gum_v8_interceptor_init (&self->interceptor, &self->core,
        &self->code_writer, global_templ);
    _gum_v8_api_resolver_init (&self->api_resolver, &self->core, global_templ);
    _gum_v8_symbol_init (&self->symbol, &self->core, global_templ);
    _gum_v8_cmodule_init (&self->cmodule, &self->core, global_templ);
    _gum_v8_instruction_init (&self->instruction, &self->core, global_templ);
    _gum_v8_control_flow_graph_init (&self->control_flow_graph,
        &self->instruction, &self->core, global_templ);
    _gum_v8_code_writer_init (&self->code_writer, &self->core, global_templ);
    _gum_v8_code_relocator_init (&self->code_relocator, &self->code_writer,
        &self->instruction, &self->core, global_templ);
    _gum_v8_stalker_init (&self->stalker, &self->code_writer,
        &self->instruction, &self->core, global_templ);
    _gum_v8_cloak_init (&self->cloak, &self->core, global_templ);
    _gum_v8_sampler_init (&self->sampler, &self->core, global_templ);
    _gum_v8_profiler_init (&self->profiler, &self->sampler, &self->interceptor,
        &self->core, global_templ);
    _gum_v8_api_init (&self->api, &self->core, global_templ);

    Local<Context> context (Context::New (isolate, NULL, global_templ));
    {
      auto name_buffer = gum_string_buffer_from_utf8 (self->name);
      V8ContextInfo info (context, self->context_group_id,
          name_buffer->string ());
      self->inspector->contextCreated (info);
    }
    g_signal_emit (self, gum_v8_script_signals[CONTEXT_CREATED], 0, &context);
    self->context = new Global<Context> (isolate, context);
    Context::Scope context_scope (context);
    _gum_v8_core_realize (&self->core);
    _gum_v8_kernel_realize (&self->kernel);
    _gum_v8_memory_realize (&self->memory);
    _gum_v8_module_realize (&self->module);
    _gum_v8_thread_realize (&self->thread);
    _gum_v8_process_realize (&self->process);
    _gum_v8_file_realize (&self->file);
    _gum_v8_checksum_realize (&self->checksum);
#ifndef G_OS_NONE
    _gum_v8_stream_realize (&self->stream);
    _gum_v8_socket_realize (&self->socket);
#endif
#ifdef HAVE_SQLITE
    _gum_v8_database_realize (&self->database);
#endif
    _gum_v8_interceptor_realize (&self->interceptor);
    _gum_v8_api_resolver_realize (&self->api_resolver);
    _gum_v8_symbol_realize (&self->symbol);
    _gum_v8_cmodule_realize (&self->cmodule);
    _gum_v8_instruction_realize (&self->instruction);
    _gum_v8_control_flow_graph_realize (&self->control_flow_graph);
    _gum_v8_code_writer_realize (&self->code_writer);
    _gum_v8_code_relocator_realize (&self->code_relocator);
    _gum_v8_stalker_realize (&self->stalker);
    _gum_v8_cloak_realize (&self->cloak);
    _gum_v8_sampler_realize (&self->sampler);
    _gum_v8_profiler_realize (&self->profiler);
    _gum_v8_api_realize (&self->api);

    self->program = gum_v8_script_compile (self, isolate, context, error);
  }

  if (self->program == NULL)
  {
    gum_v8_script_destroy_context (self);
    return FALSE;
  }

  g_free (self->source);
  self->source = NULL;

  g_rec_mutex_init (&self->interrupt_mutex);
  self->interrupt = GUM_INTERRUPT_NONE;
  self->executing = FALSE;

  return TRUE;
}

static GumESProgram *
gum_v8_script_compile (GumV8Script * self,
                       Isolate * isolate,
                       Local<Context> context,
                       GError ** error)
{
  GumESProgram * program = gum_es_program_new ();
  context->SetAlignedPointerInEmbedderData (0, program);

  const gchar * source = self->source;
  const gchar * package_marker = "📦\n";
  const gchar * delimiter_marker = "\n✄\n";
  const gchar * alias_marker = "\n↻ ";

  if (g_str_has_prefix (source, package_marker))
  {
    program->entrypoints = g_ptr_array_new ();

    const gchar * source_end = source + std::strlen (source);
    const gchar * header_cursor = source + std::strlen (package_marker);

    do
    {
      GumESAsset * entrypoint = NULL;

      const gchar * asset_cursor = strstr (header_cursor, delimiter_marker);
      if (asset_cursor == NULL)
        goto malformed_package;

      const gchar * header_end = asset_cursor;

      for (guint i = 0; header_cursor != header_end; i++)
      {
        if (i != 0 && !g_str_has_prefix (asset_cursor, delimiter_marker))
          goto malformed_package;
        asset_cursor += std::strlen (delimiter_marker);

        const gchar * size_end;
        guint64 asset_size =
            g_ascii_strtoull (header_cursor, (gchar **) &size_end, 10);
        if (asset_size == 0 || asset_size > GUM_MAX_ASSET_SIZE)
          goto malformed_package;
        if (asset_cursor + asset_size > source_end)
          goto malformed_package;

        const gchar * rest_start = size_end + 1;
        const gchar * rest_end = std::strchr (rest_start, '\n');

        gchar * asset_name = g_strndup (rest_start, rest_end - rest_start);
        if (g_hash_table_contains (program->es_assets, asset_name))
        {
          g_free (asset_name);
          goto malformed_package;
        }

        gchar * asset_data = g_strndup (asset_cursor, asset_size);

        auto asset =
            gum_es_asset_new (asset_name, asset_data, asset_size, g_free);
        g_hash_table_insert (program->es_assets, asset_name, asset);

        while (g_str_has_prefix (rest_end, alias_marker))
        {
          const gchar * alias_start = rest_end + std::strlen (alias_marker);
          const gchar * alias_end = std::strchr (alias_start, '\n');

          gchar * asset_alias =
              g_strndup (alias_start, alias_end - alias_start);
          if (g_hash_table_contains (program->es_assets, asset_alias))
          {
            g_free (asset_alias);
            goto malformed_package;
          }
          g_hash_table_insert (program->es_assets, asset_alias,
              gum_es_asset_ref (asset));

          rest_end = alias_end;
        }

        if (entrypoint == NULL && g_str_has_suffix (asset_name, ".js"))
          entrypoint = asset;

        header_cursor = rest_end;
        asset_cursor += asset_size;
      }

      if (entrypoint == NULL)
        goto malformed_package;

      Local<Module> module;
      TryCatch trycatch (isolate);
      auto result =
          gum_ensure_module_defined (isolate, context, entrypoint, program);
      bool success = result.ToLocal (&module);
      if (success)
      {
        auto instantiate_result =
            module->InstantiateModule (context, gum_resolve_module);
        if (!instantiate_result.To (&success))
          success = false;
      }

      if (!success)
      {
        gchar * message =
            _gum_v8_error_get_message (isolate, trycatch.Exception ());
        g_set_error_literal (error, GUM_ERROR, GUM_ERROR_FAILED, message);
        g_free (message);
        goto propagate_error;
      }

      g_ptr_array_add (program->entrypoints, entrypoint);

      if (g_str_has_prefix (asset_cursor, delimiter_marker))
        header_cursor = asset_cursor + std::strlen (delimiter_marker);
      else
        header_cursor = NULL;
    }
    while (header_cursor != NULL);
  }
  else
  {
    program->global_filename = g_strconcat ("/", self->name, ".js",
        (const gchar *) NULL);

    auto resource_name = String::NewFromUtf8 (isolate, program->global_filename)
        .ToLocalChecked ();
    ScriptOrigin origin (isolate, resource_name);

    auto source_str = String::NewFromUtf8 (isolate, source).ToLocalChecked ();

    Local<Script> code;
    TryCatch trycatch (isolate);
    auto maybe_code = Script::Compile (context, source_str, &origin);
    if (maybe_code.ToLocal (&code))
    {
      program->global_code = new Global<Script> (isolate, code);
    }
    else
    {
      Local<Message> message = trycatch.Message ();
      Local<Value> exception = trycatch.Exception ();
      String::Utf8Value exception_str (isolate, exception);
      g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "Script(line %d): %s",
          message->GetLineNumber (context).FromMaybe (-1), *exception_str);
      goto propagate_error;
    }
  }

  goto beach;

malformed_package:
  {
    g_set_error (error,
        GUM_ERROR,
        GUM_ERROR_INVALID_DATA,
        "Malformed package");

    goto propagate_error;
  }
propagate_error:
  {
    context->SetAlignedPointerInEmbedderData (0, nullptr);
    gum_es_program_free (program);
    program = NULL;

    goto beach;
  }
beach:
  {
    return program;
  }
}

MaybeLocal<Module>
_gum_v8_script_load_module (GumV8Script * self,
                            const gchar * name,
                            const gchar * source)
{
  auto isolate = self->isolate;

  GumESProgram * program = self->program;
  if (g_hash_table_contains (program->es_assets, name))
  {
    _gum_v8_throw (isolate, "module '%s' already exists", name);
    return MaybeLocal<Module> ();
  }

  gchar * name_copy = g_strdup (name);
  GumESAsset * asset = gum_es_asset_new (name_copy, g_strdup (source),
      strlen (source), g_free);

  auto context = Local<Context>::New (isolate, *self->context);

  MaybeLocal<Module> maybe_module =
      gum_ensure_module_defined (isolate, context, asset, program);

  bool success = false;
  Local<Module> m;
  if (maybe_module.ToLocal (&m))
  {
    success = m->InstantiateModule (context, gum_resolve_module).IsJust ();
  }

  if (success)
  {
    g_hash_table_insert (program->es_assets, name_copy, asset);

    gchar * source_map = gum_source_map_try_extract_inline (source);
    if (source_map != NULL)
    {
      gchar * map_name = g_strconcat (name, ".map", (const gchar *) NULL);
      auto asset = gum_es_asset_new (map_name, source_map, strlen (source_map),
          g_free);
      g_hash_table_insert (program->es_assets, map_name, asset);
    }
  }
  else
  {
    gum_es_asset_unref (asset);
    g_free (name_copy);
  }

  return maybe_module;
}

void
_gum_v8_script_register_source_map (GumV8Script * self,
                                    const gchar * name,
                                    gchar * source_map)
{
  gchar * map_name = g_strconcat (name, ".map", (const gchar *) NULL);
  g_hash_table_insert (self->program->es_assets, map_name,
      gum_es_asset_new (map_name, source_map, strlen (source_map), g_free));
}

static MaybeLocal<Promise>
gum_import_module (Local<Context> context,
                   Local<Data> host_defined_options,
                   Local<Value> resource_name,
                   Local<String> specifier,
                   Local<FixedArray> import_assertions)
{
  Local<Promise::Resolver> resolver =
      Promise::Resolver::New (context).ToLocalChecked ();

  auto isolate = context->GetIsolate ();
  auto self = (GumV8Script *) isolate->GetData (0);
  auto program =
      (GumESProgram *) context->GetAlignedPointerFromEmbedderData (0);

  String::Utf8Value specifier_str (isolate, specifier);
  String::Utf8Value resource_name_str (isolate, resource_name);

  gchar * name =
      gum_normalize_module_name (*resource_name_str, *specifier_str, program);

  GumESAsset * target_module = (GumESAsset *) g_hash_table_lookup (
      program->es_assets, name);

  g_free (name);

  if (target_module == NULL)
  {
    resolver->Reject (context,
        Exception::Error (_gum_v8_string_new_ascii (isolate, "not found")))
          .ToChecked ();
    return MaybeLocal<Promise> (resolver->GetPromise ());
  }

  Local<Module> module;
  {
    TryCatch trycatch (isolate);
    if (!gum_ensure_module_defined (isolate, context, target_module, program)
        .ToLocal (&module))
    {
      resolver->Reject (context, trycatch.Exception ()).ToChecked ();
      return MaybeLocal<Promise> (resolver->GetPromise ());
    }
  }

  auto operation = g_slice_new (GumImportOperation);
  operation->self = self;
  operation->resolver = new Global<Promise::Resolver> (isolate, resolver);
  operation->module = new Global<Module> (isolate, module);
  _gum_v8_core_pin (&self->core);

  auto evaluate_request = module->Evaluate (context)
      .ToLocalChecked ().As<Promise> ();
  auto data = External::New (isolate, operation);
  evaluate_request->Then (context,
      Function::New (context, gum_on_import_success,
        data, 1, ConstructorBehavior::kThrow).ToLocalChecked (),
      Function::New (context, gum_on_import_failure,
        data, 1, ConstructorBehavior::kThrow).ToLocalChecked ())
      .ToLocalChecked ();

  return MaybeLocal<Promise> (resolver->GetPromise ());
}

static void
gum_on_import_success (const FunctionCallbackInfo<Value> & info)
{
  auto op = (GumImportOperation *) info.Data ().As<External> ()->Value ();
  auto isolate = info.GetIsolate ();
  auto context = isolate->GetCurrentContext ();

  auto resolver = Local<Promise::Resolver>::New (isolate, *op->resolver);
  auto module = Local<Module>::New (isolate, *op->module);
  resolver->Resolve (context, module->GetModuleNamespace ()).ToChecked ();

  gum_import_operation_free (op);
}

static void
gum_on_import_failure (const FunctionCallbackInfo<Value> & info)
{
  auto op = (GumImportOperation *) info.Data ().As<External> ()->Value ();
  auto isolate = info.GetIsolate ();
  auto context = isolate->GetCurrentContext ();

  auto resolver = Local<Promise::Resolver>::New (isolate, *op->resolver);
  resolver->Reject (context, info[0]).ToChecked ();

  gum_import_operation_free (op);
}

static void
gum_import_operation_free (GumImportOperation * op)
{
  delete op->module;
  delete op->resolver;
  _gum_v8_core_unpin (&op->self->core);
  g_slice_free (GumImportOperation, op);
}

static MaybeLocal<Module>
gum_resolve_module (Local<Context> context,
                    Local<String> specifier,
                    Local<FixedArray> import_assertions,
                    Local<Module> referrer)
{
  auto isolate = context->GetIsolate ();
  auto program =
      (GumESProgram *) context->GetAlignedPointerFromEmbedderData (0);

  auto referrer_module = (GumESAsset *) g_hash_table_lookup (
      program->es_modules, GINT_TO_POINTER (referrer->ScriptId ()));

  String::Utf8Value specifier_str (isolate, specifier);
  gchar * name = gum_normalize_module_name (referrer_module->name,
      *specifier_str, program);

  GumESAsset * target_module = (GumESAsset *) g_hash_table_lookup (
      program->es_assets, name);

  if (target_module == NULL)
    goto not_found;

  g_free (name);

  return gum_ensure_module_defined (isolate, context, target_module, program);

not_found:
  {
    _gum_v8_throw (isolate, "could not load module '%s'", name);
    g_free (name);
    return MaybeLocal<Module> ();
  }
}

static gchar *
gum_normalize_module_name (const gchar * base_name,
                           const gchar * name,
                           GumESProgram * program)
{
  if (name[0] != '.')
  {
    auto asset = (GumESAsset *) g_hash_table_lookup (program->es_assets, name);
    if (asset != NULL)
      return g_strdup (asset->name);

    return g_strdup (name);
  }

  /* The following is exactly like QuickJS' default implementation: */

  guint base_dir_length;
  auto base_dir_end = strrchr (base_name, '/');
  if (base_dir_end != NULL)
    base_dir_length = base_dir_end - base_name;
  else
    base_dir_length = 0;

  auto result = (gchar *) g_malloc (base_dir_length + 1 + strlen (name) + 1);
  memcpy (result, base_name, base_dir_length);
  result[base_dir_length] = '\0';

  auto cursor = name;
  while (TRUE)
  {
    if (g_str_has_prefix (cursor, "./"))
    {
      cursor += 2;
    }
    else if (g_str_has_prefix (cursor, "../"))
    {
      if (result[0] == '\0')
        break;

      gchar * new_end = strrchr (result, '/');
      if (new_end != NULL)
        new_end++;
      else
        new_end = result;

      if (strcmp (new_end, ".") == 0 || strcmp (new_end, "..") == 0)
        break;

      if (new_end > result)
        new_end--;

      *new_end = '\0';

      cursor += 3;
    }
    else
    {
      break;
    }
  }

  strcat (result, "/");
  strcat (result, cursor);

  return result;
}

static MaybeLocal<Module>
gum_ensure_module_defined (Isolate * isolate,
                           Local<Context> context,
                           GumESAsset * asset,
                           GumESProgram * program)
{
  if (asset->module != nullptr)
    return Local<Module>::New (isolate, *asset->module);

  auto source_str = String::NewFromUtf8 (isolate, (const char *) asset->data)
      .ToLocalChecked ();

  auto resource_name = String::NewFromUtf8 (isolate, asset->name)
      .ToLocalChecked ();
  int resource_line_offset = 0;
  int resource_column_offset = 0;
  bool resource_is_shared_cross_origin = false;
  int script_id = -1;
  auto source_map_url = Local<Value> ();
  bool resource_is_opaque = false;
  bool is_wasm = false;
  bool is_module = true;
  ScriptOrigin origin (
      isolate,
      resource_name,
      resource_line_offset,
      resource_column_offset,
      resource_is_shared_cross_origin,
      script_id,
      source_map_url,
      resource_is_opaque,
      is_wasm,
      is_module);

  ScriptCompiler::Source source (source_str, origin);

  Local<Module> module;
  gchar * error_description = NULL;
  int line = -1;
  {
    TryCatch trycatch (isolate);
    auto compile_result = ScriptCompiler::CompileModule (isolate, &source);
    if (!compile_result.ToLocal (&module))
    {
      error_description =
          _gum_v8_error_get_message (isolate, trycatch.Exception ());
      line = trycatch.Message ()->GetLineNumber (context).FromMaybe (-1);
    }
  }
  if (error_description != NULL)
  {
    _gum_v8_throw (isolate,
        "could not parse '%s' line %d: %s",
        asset->name,
        line,
        error_description);
    g_free (error_description);
    return MaybeLocal<Module> ();
  }

  asset->module = new Global<Module> (isolate, module);

  g_hash_table_insert (program->es_modules,
      GINT_TO_POINTER (module->ScriptId ()), asset);

  gum_es_asset_release_data (asset);

  return module;
}

static void
gum_v8_script_destroy_context (GumV8Script * self)
{
  g_assert (self->context != NULL);

  {
    ScriptScope scope (self);

    _gum_v8_profiler_dispose (&self->profiler);
    _gum_v8_sampler_dispose (&self->sampler);
    _gum_v8_cloak_dispose (&self->cloak);
    _gum_v8_stalker_dispose (&self->stalker);
    _gum_v8_code_relocator_dispose (&self->code_relocator);
    _gum_v8_code_writer_dispose (&self->code_writer);
    _gum_v8_control_flow_graph_dispose (&self->control_flow_graph);
    _gum_v8_instruction_dispose (&self->instruction);
    _gum_v8_cmodule_dispose (&self->cmodule);
    _gum_v8_symbol_dispose (&self->symbol);
    _gum_v8_api_resolver_dispose (&self->api_resolver);
    _gum_v8_interceptor_dispose (&self->interceptor);
#ifdef HAVE_SQLITE
    _gum_v8_database_dispose (&self->database);
#endif
#ifndef G_OS_NONE
    _gum_v8_socket_dispose (&self->socket);
    _gum_v8_stream_dispose (&self->stream);
#endif
    _gum_v8_checksum_dispose (&self->checksum);
    _gum_v8_file_dispose (&self->file);
    _gum_v8_process_dispose (&self->process);
    _gum_v8_thread_dispose (&self->thread);
    _gum_v8_module_dispose (&self->module);
    _gum_v8_memory_dispose (&self->memory);
    _gum_v8_kernel_dispose (&self->kernel);
    _gum_v8_core_dispose (&self->core);

    auto context = Local<Context>::New (self->isolate, *self->context);
    self->inspector->contextDestroyed (context);
    g_signal_emit (self, gum_v8_script_signals[CONTEXT_DESTROYED], 0, &context);
  }

  gum_es_program_free (self->program);
  self->program = NULL;
  delete self->context;
  self->context = nullptr;

  self->interrupt = GUM_INTERRUPT_NONE;
  /*
   * NOTE: interrupt_mutex is NOT cleared here. dispose() locks it after
   * destroy_context runs (via unload), so it must remain valid until
   * finalize().
   */

  _gum_v8_api_finalize (&self->api);
  _gum_v8_profiler_finalize (&self->profiler);
  _gum_v8_sampler_finalize (&self->sampler);
  _gum_v8_cloak_finalize (&self->cloak);
  _gum_v8_stalker_finalize (&self->stalker);
  _gum_v8_code_relocator_finalize (&self->code_relocator);
  _gum_v8_code_writer_finalize (&self->code_writer);
  _gum_v8_control_flow_graph_finalize (&self->control_flow_graph);
  _gum_v8_instruction_finalize (&self->instruction);
  _gum_v8_cmodule_finalize (&self->cmodule);
  _gum_v8_symbol_finalize (&self->symbol);
  _gum_v8_api_resolver_finalize (&self->api_resolver);
  _gum_v8_interceptor_finalize (&self->interceptor);
#ifdef HAVE_SQLITE
  _gum_v8_database_finalize (&self->database);
#endif
#ifndef G_OS_NONE
  _gum_v8_socket_finalize (&self->socket);
  _gum_v8_stream_finalize (&self->stream);
#endif
  _gum_v8_checksum_finalize (&self->checksum);
  _gum_v8_file_finalize (&self->file);
  _gum_v8_process_finalize (&self->process);
  _gum_v8_thread_finalize (&self->thread);
  _gum_v8_module_finalize (&self->module);
  _gum_v8_memory_finalize (&self->memory);
  _gum_v8_kernel_finalize (&self->kernel);
  _gum_v8_core_finalize (&self->core);
}

static void
gum_v8_script_load (GumScript * script,
                    GCancellable * cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  auto self = GUM_V8_SCRIPT (script);

  auto task = gum_script_task_new ((GumScriptTaskFunc) gum_v8_script_do_load,
      self, cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_v8_script_backend_get_scheduler (self->backend));
  g_object_unref (task);
}

static void
gum_v8_script_load_finish (GumScript * script,
                           GAsyncResult * result)
{
  gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), NULL);
}

static void
gum_v8_script_load_sync (GumScript * script,
                         GCancellable * cancellable)
{
  auto self = GUM_V8_SCRIPT (script);

  auto task = gum_script_task_new ((GumScriptTaskFunc) gum_v8_script_do_load,
      self, cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_v8_script_backend_get_scheduler (self->backend));
  gum_script_task_propagate_pointer (task, NULL);
  g_object_unref (task);
}

static void
gum_v8_script_do_load (GumScriptTask * task,
                       GumV8Script * self,
                       gpointer task_data,
                       GCancellable * cancellable)
{
  if (self->state != GUM_SCRIPT_STATE_CREATED)
    goto invalid_operation;

  self->state = GUM_SCRIPT_STATE_LOADING;

  gum_v8_script_execute_runtime (self, task);

  return;

invalid_operation:
  {
    gum_script_task_return_error (task,
        g_error_new_literal (
          GUM_ERROR,
          GUM_ERROR_NOT_SUPPORTED,
          "Invalid operation"));
  }
}

static void
gum_v8_script_execute_runtime (GumV8Script * self,
                               GumScriptTask * task)
{
  ScriptScope scope (self);
  auto isolate = self->isolate;
  auto context = isolate->GetCurrentContext ();
  gum_es_program_load_runtime (self->program, isolate, context,
      gum_v8_script_on_runtime_loaded, g_object_ref (task),
      g_object_unref);
}

static void
gum_v8_script_on_runtime_loaded (Local<Value> error,
                                 gpointer user_data)
{
  auto task = (GumScriptTask *) user_data;
  auto self = (GumV8Script *)
      g_async_result_get_source_object (G_ASYNC_RESULT (task));

  if (error->IsNull ())
  {
    gum_v8_script_execute_entrypoints (self, task);
  }
  else
  {
    _gum_v8_core_on_unhandled_exception (&self->core, error);
    gum_v8_script_schedule_load_task_completion (self, task);
  }

  g_object_unref (self);
}

static void
gum_v8_script_execute_entrypoints (GumV8Script * self,
                                   GumScriptTask * task)
{
  {
    auto isolate = self->isolate;
    auto context = isolate->GetCurrentContext ();

    auto program = self->program;
    if (program->entrypoints != NULL)
    {
      auto entrypoints = program->entrypoints;

      auto pending = Array::New (isolate, entrypoints->len);
      for (guint i = 0; i != entrypoints->len; i++)
      {
        auto entrypoint = (GumESAsset *) g_ptr_array_index (entrypoints, i);
        auto module = Local<Module>::New (isolate, *entrypoint->module);
        auto promise = module->Evaluate (context);
        pending->Set (context, i, promise.ToLocalChecked ()).Check ();
      }

      auto promise_class = context->Global ()
          ->Get (context, _gum_v8_string_new_ascii (isolate, "Promise"))
          .ToLocalChecked ().As<Object> ();
      auto all_settled = promise_class
          ->Get (context, _gum_v8_string_new_ascii (isolate, "allSettled"))
          .ToLocalChecked ().As<Function> ();

      Local<Value> argv[] = { pending };
      auto load_request = all_settled
          ->Call (context, promise_class, G_N_ELEMENTS (argv), argv)
          .ToLocalChecked ().As<Promise> ();

      load_request->Then (context,
          Function::New (context, gum_v8_script_on_entrypoints_executed,
            External::New (isolate, g_object_ref (task)), 1,
            ConstructorBehavior::kThrow)
          .ToLocalChecked ())
          .ToLocalChecked ();
    }
    else
    {
      auto code = Local<Script>::New (isolate, *program->global_code);

      {
        TryCatch trycatch (isolate);

        auto result = code->Run (context);
        _gum_v8_ignore_result (result);

        if (trycatch.HasTerminated ())
        {
          trycatch.Reset ();
          _gum_v8_script_handle_termination (self);
        }
        else if (trycatch.HasCaught ())
        {
          auto exception = trycatch.Exception ();
          trycatch.Reset ();
          _gum_v8_core_on_unhandled_exception (&self->core, exception);
          trycatch.Reset ();
        }
      }

      gum_v8_script_schedule_load_task_completion (self, task);
    }
  }
}

static void
gum_v8_script_on_entrypoints_executed (const FunctionCallbackInfo<Value> & info)
{
  auto task = (GumScriptTask *) info.Data ().As<External> ()->Value ();
  auto self = (GumV8Script *)
      g_async_result_get_source_object (G_ASYNC_RESULT (task));
  auto core = &self->core;
  auto isolate = info.GetIsolate ();
  auto context = isolate->GetCurrentContext ();

  auto values = info[0].As<Array> ();
  uint32_t n = values->Length ();
  auto reason_str = _gum_v8_string_new_ascii (isolate, "reason");
  for (uint32_t i = 0; i != n; i++)
  {
    auto value = values->Get (context, i).ToLocalChecked ().As<Object> ();
    auto reason = value->Get (context, reason_str).ToLocalChecked ();
    if (!reason->IsUndefined ())
      _gum_v8_core_on_unhandled_exception (core, reason);
  }

  gum_v8_script_schedule_load_task_completion (self, task);

  g_object_unref (self);
  g_object_unref (task);
}

static void
gum_v8_script_schedule_load_task_completion (GumV8Script * self,
                                             GumScriptTask * task)
{
  auto core = &self->core;

  auto source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) gum_v8_script_complete_load_task,
      g_object_ref (task), g_object_unref);
  g_source_attach (source,
      gum_script_scheduler_get_js_context (core->scheduler));
  g_source_unref (source);

  _gum_v8_core_pin (core);
}

static gboolean
gum_v8_script_complete_load_task (GumScriptTask * task)
{
  auto self = GUM_V8_SCRIPT (
      g_async_result_get_source_object (G_ASYNC_RESULT (task)));

  {
    ScriptScope scope (self);

    _gum_v8_core_unpin (&self->core);
  }

  g_rec_mutex_lock (&self->interrupt_mutex);
  self->state = GUM_SCRIPT_STATE_LOADED;
  gboolean terminating = self->interrupt == GUM_INTERRUPT_FOREVER;
  g_rec_mutex_unlock (&self->interrupt_mutex);

  gum_script_task_return_pointer (task, NULL, NULL);

  if (terminating)
    gum_v8_script_unload (GUM_SCRIPT (self), NULL, NULL, NULL);

  g_object_unref (self);

  return G_SOURCE_REMOVE;
}

static void
gum_v8_script_unload (GumScript * script,
                      GCancellable * cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
  auto self = GUM_V8_SCRIPT (script);

  auto task = gum_script_task_new ((GumScriptTaskFunc) gum_v8_script_do_unload,
      self, cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_v8_script_backend_get_scheduler (self->backend));
  g_object_unref (task);
}

static void
gum_v8_script_unload_finish (GumScript * script,
                             GAsyncResult * result)
{
  gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), NULL);
}

static void
gum_v8_script_unload_sync (GumScript * script,
                           GCancellable * cancellable)
{
  auto self = GUM_V8_SCRIPT (script);

  auto task = gum_script_task_new ((GumScriptTaskFunc) gum_v8_script_do_unload,
      self, cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_v8_script_backend_get_scheduler (self->backend));
  gum_script_task_propagate_pointer (task, NULL);
  g_object_unref (task);
}

static void
gum_v8_script_do_unload (GumScriptTask * task,
                         GumV8Script * self,
                         gpointer task_data,
                         GCancellable * cancellable)
{
  g_rec_mutex_lock (&self->interrupt_mutex);
  if (self->state != GUM_SCRIPT_STATE_LOADED)
  {
    g_rec_mutex_unlock (&self->interrupt_mutex);
    goto invalid_operation;
  }
  self->state = GUM_SCRIPT_STATE_UNLOADING;
  g_rec_mutex_unlock (&self->interrupt_mutex);

  gum_v8_script_once_unloaded (self,
      (GumUnloadNotifyFunc) gum_v8_script_complete_unload_task,
      g_object_ref (task), g_object_unref);

  gum_v8_script_try_unload (self);

  return;

invalid_operation:
  {
    gum_script_task_return_error (task,
        g_error_new_literal (
          GUM_ERROR,
          GUM_ERROR_NOT_SUPPORTED,
          "Invalid operation"));
  }
}

static void
gum_v8_script_complete_unload_task (GumV8Script * self,
                                    GumScriptTask * task)
{
  gum_script_task_return_pointer (task, NULL, NULL);
}

static void
gum_v8_script_try_unload (GumV8Script * self)
{
  g_assert (self->state == GUM_SCRIPT_STATE_UNLOADING);

  gboolean success;

  {
    ScriptScope scope (self);

    _gum_v8_profiler_flush (&self->profiler);
    _gum_v8_sampler_flush (&self->sampler);
    _gum_v8_stalker_flush (&self->stalker);
    _gum_v8_interceptor_flush (&self->interceptor);
#ifndef G_OS_NONE
    _gum_v8_socket_flush (&self->socket);
    _gum_v8_stream_flush (&self->stream);
#endif
    _gum_v8_process_flush (&self->process);
    success = _gum_v8_core_flush (&self->core, gum_v8_script_try_unload);
  }

  if (success)
  {
    gum_v8_script_destroy_context (self);

    self->state = GUM_SCRIPT_STATE_UNLOADED;

    while (self->on_unload != NULL)
    {
      auto link = self->on_unload;
      auto callback = (GumUnloadNotifyCallback *) link->data;

      callback->func (self, callback->data);
      if (callback->data_destroy != NULL)
        callback->data_destroy (callback->data);
      g_slice_free (GumUnloadNotifyCallback, callback);

      self->on_unload = g_slist_delete_link (self->on_unload, link);
    }
  }
}

static void
gum_v8_script_once_unloaded (GumV8Script * self,
                             GumUnloadNotifyFunc func,
                             gpointer data,
                             GDestroyNotify data_destroy)
{
  auto callback = g_slice_new (GumUnloadNotifyCallback);
  callback->func = func;
  callback->data = data;
  callback->data_destroy = data_destroy;

  self->on_unload = g_slist_append (self->on_unload, callback);
}

static void
gum_v8_script_interrupt (GumScript * script)
{
  auto self = GUM_V8_SCRIPT (script);
  gboolean executing;

  g_rec_mutex_lock (&self->interrupt_mutex);
  executing = self->executing;
  if (executing && self->interrupt == GUM_INTERRUPT_NONE)
    self->interrupt = GUM_INTERRUPT_ONCE;
  g_rec_mutex_unlock (&self->interrupt_mutex);

  if (executing)
    self->isolate->TerminateExecution ();
}

static void
gum_v8_script_terminate (GumScript * script)
{
  auto self = GUM_V8_SCRIPT (script);

  g_rec_mutex_lock (&self->interrupt_mutex);
  self->interrupt = GUM_INTERRUPT_FOREVER;
  if (self->state == GUM_SCRIPT_STATE_LOADED)
    gum_v8_script_unload (script, NULL, NULL, NULL);
  g_rec_mutex_unlock (&self->interrupt_mutex);

  if (self->isolate != NULL)
    self->isolate->TerminateExecution ();
}

void
_gum_v8_script_handle_termination (GumV8Script * self)
{
  self->isolate->CancelTerminateExecution ();
}

static void
gum_v8_script_set_message_handler (GumScript * script,
                                   GumScriptMessageHandler handler,
                                   gpointer data,
                                   GDestroyNotify data_destroy)
{
  auto self = GUM_V8_SCRIPT (script);

  if (self->message_handler_data_destroy != NULL)
    self->message_handler_data_destroy (self->message_handler_data);
  self->message_handler = handler;
  self->message_handler_data = data;
  self->message_handler_data_destroy = data_destroy;
}

static void
gum_v8_script_post (GumScript * script,
                    const gchar * message,
                    GBytes * data)
{
  auto self = GUM_V8_SCRIPT (script);

  auto d = g_slice_new (GumPostData);
  d->script = self;
  g_object_ref (self);
  d->message = g_strdup (message);
  d->data = (data != NULL) ? g_bytes_ref (data) : NULL;

  gum_script_scheduler_push_job_on_js_thread (
      gum_v8_script_backend_get_scheduler (self->backend),
      G_PRIORITY_DEFAULT, (GumScriptJobFunc) gum_v8_script_do_post, d,
      (GDestroyNotify) gum_v8_post_data_free);
}

static void
gum_v8_script_do_post (GumPostData * d)
{
  GBytes * data = d->data;
  d->data = NULL;

  _gum_v8_core_post (&d->script->core, d->message, data);
}

static void
gum_v8_post_data_free (GumPostData * d)
{
  g_free (d->message);
  g_object_unref (d->script);

  g_slice_free (GumPostData, d);
}

static void
gum_v8_script_emit (GumV8Script * self,
                    const gchar * message,
                    GBytes * data)
{
  auto d = g_slice_new (GumEmitData);
  d->script = self;
  g_object_ref (self);
  d->message = g_strdup (message);
  d->data = (data != NULL) ? g_bytes_ref (data) : NULL;

  auto source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) gum_v8_script_do_emit, d,
      (GDestroyNotify) gum_v8_emit_data_free);
  g_source_attach (source, self->main_context);
  g_source_unref (source);
}

static gboolean
gum_v8_script_do_emit (GumEmitData * d)
{
  auto self = d->script;

  if (self->message_handler != NULL)
    self->message_handler (d->message, d->data, self->message_handler_data);

  return FALSE;
}

static void
gum_v8_emit_data_free (GumEmitData * d)
{
  g_bytes_unref (d->data);
  g_free (d->message);
  g_object_unref (d->script);

  g_slice_free (GumEmitData, d);
}

static void
gum_v8_script_set_debug_message_handler (GumScript * backend,
                                         GumScriptDebugMessageHandler handler,
                                         gpointer data,
                                         GDestroyNotify data_destroy)
{
  auto self = GUM_V8_SCRIPT (backend);

  if (self->debug_handler_data_destroy != NULL)
    self->debug_handler_data_destroy (self->debug_handler_data);

  self->debug_handler = handler;
  self->debug_handler_data = data;
  self->debug_handler_data_destroy = data_destroy;

  auto new_context = (handler != NULL)
      ? g_main_context_ref_thread_default ()
      : NULL;

  GUM_V8_INSPECTOR_LOCK (self);

  auto old_context = self->debug_handler_context;
  self->debug_handler_context = new_context;

  if (handler != NULL)
  {
    if (self->inspector_state == GUM_V8_RUNNING)
      self->inspector_state = GUM_V8_DEBUGGING;
  }
  else
  {
    gum_v8_script_drop_queued_debug_messages_unlocked (self);

    self->inspector_state = GUM_V8_RUNNING;
    g_cond_signal (&self->inspector_cond);
  }

  GUM_V8_INSPECTOR_UNLOCK (self);

  if (old_context != NULL)
    g_main_context_unref (old_context);

  if (handler == NULL)
  {
    gum_script_scheduler_push_job_on_js_thread (
        gum_v8_script_backend_get_scheduler (self->backend), G_PRIORITY_DEFAULT,
        (GumScriptJobFunc) gum_v8_script_clear_inspector_channels,
        g_object_ref (self), g_object_unref);
  }
}

static void
gum_v8_script_post_debug_message (GumScript * backend,
                                  const gchar * message)
{
  auto self = GUM_V8_SCRIPT (backend);

  if (self->debug_handler == NULL)
    return;

  gchar * message_copy = g_strdup (message);

  GUM_V8_INSPECTOR_LOCK (self);

  g_queue_push_tail (&self->debug_messages, message_copy);
  g_cond_signal (&self->inspector_cond);

  bool flush_not_already_scheduled = !self->flush_scheduled;
  self->flush_scheduled = true;

  GUM_V8_INSPECTOR_UNLOCK (self);

  if (flush_not_already_scheduled)
  {
    gum_script_scheduler_push_job_on_js_thread (
        gum_v8_script_backend_get_scheduler (self->backend), G_PRIORITY_DEFAULT,
        (GumScriptJobFunc) gum_v8_script_process_queued_debug_messages,
        self, NULL);
  }
}

static void
gum_v8_script_process_queued_debug_messages (GumV8Script * self)
{
  auto isolate = self->isolate;
  Locker locker (isolate);
  Isolate::Scope isolate_scope (isolate);
  HandleScope handle_scope (isolate);

  GUM_V8_INSPECTOR_LOCK (self);
  gum_v8_script_process_queued_debug_messages_unlocked (self);
  GUM_V8_INSPECTOR_UNLOCK (self);

  isolate->PerformMicrotaskCheckpoint ();
}

static void
gum_v8_script_process_queued_debug_messages_unlocked (GumV8Script * self)
{
  gchar * message;
  while ((message = (gchar *) g_queue_pop_head (&self->debug_messages)) != NULL)
  {
    GUM_V8_INSPECTOR_UNLOCK (self);
    gum_v8_script_process_debug_message (self, message);
    GUM_V8_INSPECTOR_LOCK (self);

    g_free (message);
  }

  self->flush_scheduled = false;
}

static void
gum_v8_script_drop_queued_debug_messages_unlocked (GumV8Script * self)
{
  gchar * message;
  while ((message = (gchar *) g_queue_pop_head (&self->debug_messages)) != NULL)
    g_free (message);
}

static void
gum_v8_script_process_debug_message (GumV8Script * self,
                                     const gchar * message)
{
  guint id;
  const char * id_start, * id_end;
  id_start = strchr (message, ' ');
  if (id_start == NULL)
    return;
  id_start++;
  id = (guint) g_ascii_strtoull (id_start, (gchar **) &id_end, 10);
  if (id_end == id_start)
    return;

  if (g_str_has_prefix (message, "CONNECT "))
  {
    gum_v8_script_connect_inspector_channel (self, id);
  }
  else if (g_str_has_prefix (message, "DISCONNECT "))
  {
    gum_v8_script_disconnect_inspector_channel (self, id);
  }
  else if (g_str_has_prefix (message, "DISPATCH "))
  {
    if (*id_end != ' ')
      return;
    const char * stanza = id_end + 1;
    gum_v8_script_dispatch_inspector_stanza (self, id, stanza);
  }
}

static void
gum_v8_script_emit_debug_message (GumV8Script * self,
                                  const gchar * format,
                                  ...)
{
  GUM_V8_INSPECTOR_LOCK (self);
  auto context = (self->debug_handler_context != NULL)
      ? g_main_context_ref (self->debug_handler_context)
      : NULL;
  GUM_V8_INSPECTOR_UNLOCK (self);

  if (context == NULL)
    return;

  auto d = g_slice_new (GumEmitDebugMessageData);

  d->script = self;
  g_object_ref (self);

  va_list args;
  va_start (args, format);
  d->message = g_strdup_vprintf (format, args);
  va_end (args);

  auto source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gum_v8_script_do_emit_debug_message, d,
      (GDestroyNotify) gum_emit_debug_message_data_free);
  g_source_attach (source, context);
  g_source_unref (source);

  g_main_context_unref (context);
}

static gboolean
gum_v8_script_do_emit_debug_message (GumEmitDebugMessageData * d)
{
  auto self = d->script;

  if (self->debug_handler != NULL)
    self->debug_handler (d->message, self->debug_handler_data);

  return FALSE;
}

static void
gum_emit_debug_message_data_free (GumEmitDebugMessageData * d)
{
  g_free (d->message);
  g_object_unref (d->script);

  g_slice_free (GumEmitDebugMessageData, d);
}

static void
gum_v8_script_clear_inspector_channels (GumV8Script * self)
{
  auto isolate = self->isolate;
  Locker locker (isolate);
  Isolate::Scope isolate_scope (isolate);
  HandleScope handle_scope (isolate);

  GUM_V8_INSPECTOR_LOCK (self);
  bool debugger_still_disabled = (self->inspector_state == GUM_V8_RUNNING);
  GUM_V8_INSPECTOR_UNLOCK (self);

  if (debugger_still_disabled)
    self->channels->clear ();
}

static void
gum_v8_script_connect_inspector_channel (GumV8Script * self,
                                         guint id)
{
  auto channel = new GumInspectorChannel (self, id);
  (*self->channels)[id] = std::unique_ptr<GumInspectorChannel> (channel);

  auto session = self->inspector->connect (self->context_group_id, channel,
      StringView (), V8Inspector::ClientTrustLevel::kFullyTrusted);
  channel->takeSession (std::move (session));
}

static void
gum_v8_script_disconnect_inspector_channel (GumV8Script * self,
                                            guint id)
{
  self->channels->erase (id);
}

static void
gum_v8_script_dispatch_inspector_stanza (GumV8Script * self,
                                         guint channel_id,
                                         const gchar * stanza)
{
  auto channel = (*self->channels)[channel_id].get ();
  if (channel == nullptr)
    return;

  channel->dispatchStanza (stanza);
}

static void
gum_v8_script_emit_inspector_stanza (GumV8Script * self,
                                     guint channel_id,
                                     const gchar * stanza)
{
  gum_v8_script_emit_debug_message (self, "DISPATCH %u %s",
      channel_id, stanza);
}

GumInspectorClient::GumInspectorClient (GumV8Script * script)
  : script (script)
{
}

void
GumInspectorClient::runMessageLoopOnPause (int context_group_id)
{
  GUM_V8_INSPECTOR_LOCK (script);

  if (script->inspector_state == GUM_V8_RUNNING)
  {
    startSkippingAllPauses ();
    GUM_V8_INSPECTOR_UNLOCK (script);
    return;
  }

  script->inspector_state = GUM_V8_PAUSED;
  while (script->inspector_state == GUM_V8_PAUSED)
  {
    gum_v8_script_process_queued_debug_messages_unlocked (script);

    if (script->inspector_state == GUM_V8_PAUSED)
      g_cond_wait (&script->inspector_cond, &script->inspector_mutex);
  }

  gum_v8_script_process_queued_debug_messages_unlocked (script);

  if (script->inspector_state == GUM_V8_RUNNING)
  {
    startSkippingAllPauses ();
  }

  GUM_V8_INSPECTOR_UNLOCK (script);
}

void
GumInspectorClient::quitMessageLoopOnPause ()
{
  GUM_V8_INSPECTOR_LOCK (script);

  if (script->inspector_state == GUM_V8_PAUSED)
  {
    script->inspector_state = GUM_V8_DEBUGGING;
    g_cond_signal (&script->inspector_cond);
  }

  GUM_V8_INSPECTOR_UNLOCK (script);
}

Local<Context>
GumInspectorClient::ensureDefaultContextInGroup (int contextGroupId)
{
  return Local<Context>::New (script->isolate, *script->context);
}

double
GumInspectorClient::currentTimeMS ()
{
  auto platform =
      (GumV8Platform *) gum_v8_script_backend_get_platform (script->backend);

  return platform->CurrentClockTimeMillis ();
}

void
GumInspectorClient::startSkippingAllPauses ()
{
  for (const auto & pair : *script->channels)
  {
    pair.second->startSkippingAllPauses ();
  }
}

GumInspectorChannel::GumInspectorChannel (GumV8Script * script,
                                          guint id)
  : script (script),
    id (id)
{
}

void
GumInspectorChannel::takeSession (std::unique_ptr<V8InspectorSession> session)
{
  inspector_session = std::move (session);
}

void
GumInspectorChannel::dispatchStanza (const char * stanza)
{
  auto buffer = gum_string_buffer_from_utf8 (stanza);

  inspector_session->dispatchProtocolMessage (buffer->string ());
}

void
GumInspectorChannel::startSkippingAllPauses ()
{
  inspector_session->setSkipAllPauses (true);
}

void
GumInspectorChannel::emitStanza (std::unique_ptr<StringBuffer> stanza)
{
  gchar * stanza_utf8 = gum_string_view_to_utf8 (stanza->string ());

  gum_v8_script_emit_inspector_stanza (script, id, stanza_utf8);

  g_free (stanza_utf8);
}

void
GumInspectorChannel::sendResponse (int call_id,
                                   std::unique_ptr<StringBuffer> message)
{
  emitStanza (std::move (message));
}

void
GumInspectorChannel::sendNotification (std::unique_ptr<StringBuffer> message)
{
  emitStanza (std::move (message));
}

void
GumInspectorChannel::flushProtocolNotifications ()
{
}

static GumStalker *
gum_v8_script_get_stalker (GumScript * script)
{
  auto self = GUM_V8_SCRIPT (script);

  return _gum_v8_stalker_get (&self->stalker);
}

static void
gum_v8_script_on_fatal_error (const char * location,
                              const char * message)
{
  g_log ("V8", G_LOG_LEVEL_ERROR, "%s: %s", location, message);
}

static GumESProgram *
gum_es_program_new (void)
{
  auto program = g_slice_new0 (GumESProgram);

  program->es_assets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) gum_es_asset_unref);
  program->es_modules = g_hash_table_new (NULL, NULL);

  for (auto cur = gumjs_runtime_modules; cur->name != NULL; cur++)
  {
    auto name = g_strconcat ("/frida/runtime/", cur->name,
        (const gchar *) NULL);
    auto asset = gum_es_asset_new (name, cur->source_code,
        strlen (cur->source_code), NULL);

    if (program->runtime == NULL)
      program->runtime = asset;

    g_hash_table_insert (program->es_assets, name, asset);
  }

  return program;
}

static void
gum_es_program_free (GumESProgram * program)
{
  if (program == NULL)
    return;

  delete program->global_code;
  g_free (program->global_filename);

  g_clear_pointer (&program->es_modules, g_hash_table_unref);
  g_clear_pointer (&program->es_assets, g_hash_table_unref);
  g_clear_pointer (&program->entrypoints, g_ptr_array_unref);

  g_slice_free (GumESProgram, program);
}

static void
gum_es_program_load_runtime (GumESProgram * self,
                             Isolate * isolate,
                             Local<Context> context,
                             GumRuntimeLoadedFunc func,
                             gpointer data,
                             GDestroyNotify data_destroy)
{
  Local<Module> module;
  bool success;
  {
    TryCatch trycatch (isolate);
    auto result =
        gum_ensure_module_defined (isolate, context, self->runtime, self);
    success = result.ToLocal (&module);
    if (success)
    {
      auto instantiate_result =
          module->InstantiateModule (context, gum_resolve_module);
      if (!instantiate_result.To (&success))
        success = false;
    }

    if (!success)
    {
      func (trycatch.Exception (), data);
      if (data_destroy != NULL)
        data_destroy (data);
      return;
    }
  }

  auto promise = module->Evaluate (context).ToLocalChecked ().As<Promise> ();

  auto d = g_slice_new (GumLoadRuntimeData);
  d->func = func;
  d->data = data;
  d->data_destroy = data_destroy;

  auto data_val = External::New (isolate, d);

  auto on_success = Function::New (context,
      gum_es_program_on_runtime_load_success, data_val, 1,
      ConstructorBehavior::kThrow).ToLocalChecked ();
  auto on_failure = Function::New (context,
      gum_es_program_on_runtime_load_failure, data_val, 1,
      ConstructorBehavior::kThrow).ToLocalChecked ();

  promise->Then (context, on_success, on_failure).ToLocalChecked ();
}

static void
gum_es_program_on_runtime_load_success (
    const FunctionCallbackInfo<Value> & info)
{
  gum_es_program_on_runtime_loaded (info, Null (info.GetIsolate ()));
}

static void
gum_es_program_on_runtime_load_failure (
    const FunctionCallbackInfo<Value> & info)
{
  gum_es_program_on_runtime_loaded (info, info[0]);
}

static void
gum_es_program_on_runtime_loaded (const FunctionCallbackInfo<Value> & info,
                                  Local<Value> error)
{
  auto d = (GumLoadRuntimeData *) info.Data ().As<External> ()->Value ();

  d->func (error, d->data);

  if (d->data_destroy != NULL)
    d->data_destroy (d->data);
  g_slice_free (GumLoadRuntimeData, d);
}

static GumESAsset *
gum_es_asset_new (const gchar * name,
                  gconstpointer data,
                  gsize data_size,
                  GDestroyNotify data_destroy)
{
  auto asset = g_slice_new (GumESAsset);

  asset->ref_count = 1;

  asset->name = name;

  asset->data = data;
  asset->data_size = data_size;
  asset->data_destroy = data_destroy;

  asset->module = nullptr;

  return asset;
}

static GumESAsset *
gum_es_asset_ref (GumESAsset * asset)
{
  g_atomic_int_inc (&asset->ref_count);

  return asset;
}

static void
gum_es_asset_unref (GumESAsset * asset)
{
  if (asset == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&asset->ref_count))
    return;

  delete asset->module;

  gum_es_asset_release_data (asset);

  g_slice_free (GumESAsset, asset);
}

static void
gum_es_asset_release_data (GumESAsset * asset)
{
  if (asset->data == NULL)
    return;

  if (asset->data_destroy != NULL)
    asset->data_destroy ((gpointer) asset->data);
  asset->data = NULL;
}

static std::unique_ptr<StringBuffer>
gum_string_buffer_from_utf8 (const gchar * str)
{
  glong length;
  auto str_utf16 = g_utf8_to_utf16 (str, -1, NULL, &length, NULL);
  g_assert (str_utf16 != NULL);

  auto buffer = StringBuffer::create (StringView (str_utf16, length));

  g_free (str_utf16);

  return buffer;
}

static gchar *
gum_string_view_to_utf8 (const StringView & view)
{
  if (view.is8Bit ())
    return g_strndup ((const gchar *) view.characters8 (), view.length ());

  return g_utf16_to_utf8 (view.characters16 (), (glong) view.length (), NULL,
      NULL, NULL);
}
