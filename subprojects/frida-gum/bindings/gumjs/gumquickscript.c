/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickscript.h"

#include "gumquickapi.h"
#include "gumquickapiresolver.h"
#include "gumquickchecksum.h"
#include "gumquickcloak.h"
#include "gumquickcmodule.h"
#include "gumquickcoderelocator.h"
#include "gumquickcodewriter.h"
#include "gumquickcontrolflowgraph.h"
#include "gumquickcore.h"
#include "gumquickfile.h"
#include "gumquickinstruction.h"
#include "gumquickinterceptor.h"
#include "gumquickkernel.h"
#include "gumquickmemory.h"
#include "gumquickmodule.h"
#include "gumquickprocess.h"
#include "gumquickprofiler.h"
#include "gumquicksampler.h"
#include "gumquickscript-priv.h"
#include "gumquickscriptbackend-priv.h"
#include "gumquickscriptbackend.h"
#include "gumquicksocket.h"
#include "gumquickstalker.h"
#include "gumquickstream.h"
#include "gumquicksymbol.h"
#include "gumquickthread.h"
#include "gumscripttask.h"
#ifdef HAVE_SQLITE
# include "gumquickdatabase.h"
#endif

typedef guint GumScriptState;
typedef guint GumInterruptState;
typedef struct _GumUnloadNotifyCallback GumUnloadNotifyCallback;
typedef void (* GumUnloadNotifyFunc) (GumQuickScript * self,
    gpointer user_data);
typedef struct _GumEmitData GumEmitData;
typedef struct _GumPostData GumPostData;
typedef guint GumWorkerState;
typedef struct _GumWorkerMessageDelivery GumWorkerMessageDelivery;

struct _GumQuickScript
{
  GObject parent;

  gchar * name;
  gchar * source;
  GBytes * bytecode;
  GMainContext * main_context;
  GumQuickScriptBackend * backend;

  GumScriptState state;
  GRecMutex interrupt_mutex;
  GumInterruptState interrupt;
  gboolean executing;
  GSList * on_unload;
  JSRuntime * rt;
  JSContext * ctx;
  GumESProgram * program;
  GumQuickCore core;
  GumQuickKernel kernel;
  GumQuickMemory memory;
  GumQuickModule module;
  GumQuickThread thread;
  GumQuickProcess process;
  GumQuickFile file;
  GumQuickChecksum checksum;
#ifndef G_OS_NONE
  GumQuickStream stream;
  GumQuickSocket socket;
#endif
#ifdef HAVE_SQLITE
  GumQuickDatabase database;
#endif
  GumQuickInterceptor interceptor;
  GumQuickApiResolver api_resolver;
  GumQuickSymbol symbol;
  GumQuickCModule cmodule;
  GumQuickInstruction instruction;
  GumQuickControlFlowGraph control_flow_graph;
  GumQuickCodeWriter code_writer;
  GumQuickCodeRelocator code_relocator;
  GumQuickStalker stalker;
  GumQuickCloak cloak;
  GumQuickSampler sampler;
  GumQuickProfiler profiler;
  GumQuickApi api;

  GumScriptMessageHandler message_handler;
  gpointer message_handler_data;
  GDestroyNotify message_handler_data_destroy;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_SOURCE,
  PROP_BYTECODE,
  PROP_MAIN_CONTEXT,
  PROP_BACKEND
};

enum _GumScriptState
{
  GUM_SCRIPT_STATE_CREATED,
  GUM_SCRIPT_STATE_LOADING,
  GUM_SCRIPT_STATE_LOADED,
  GUM_SCRIPT_STATE_UNLOADING,
  GUM_SCRIPT_STATE_UNLOADED
};

enum _GumInterruptState
{
  GUM_INTERRUPT_NONE,
  GUM_INTERRUPT_ONCE,
  GUM_INTERRUPT_FOREVER
};

struct _GumUnloadNotifyCallback
{
  GumUnloadNotifyFunc func;
  gpointer data;
  GDestroyNotify data_destroy;
};

struct _GumEmitData
{
  GumQuickScript * script;
  gchar * message;
  GBytes * data;
};

struct _GumPostData
{
  GumQuickScript * script;
  gchar * message;
  GBytes * data;
};

struct _GumQuickWorker
{
  gint ref_count;

  GumWorkerState state;

  gboolean flushed;
  GMutex flush_mutex;
  GCond flush_cond;

  GumQuickScript * script;
  GumESAsset * asset;
  JSValue on_message;

  GumScriptScheduler * scheduler;

  GRecMutex scope_mutex;

  JSRuntime * rt;
  JSContext * ctx;

  JSValue entrypoint;

  GumQuickCore core;
  GumQuickKernel kernel;
  GumQuickMemory memory;
  GumQuickModule module;
  GumQuickProcess process;
  GumQuickThread thread;
  GumQuickFile file;
  GumQuickChecksum checksum;
#ifndef G_OS_NONE
  GumQuickStream stream;
  GumQuickSocket socket;
#endif
#ifdef HAVE_SQLITE
  GumQuickDatabase database;
#endif
  GumQuickApiResolver api_resolver;
  GumQuickSymbol symbol;
  GumQuickCModule cmodule;
  GumQuickInstruction instruction;
  GumQuickControlFlowGraph control_flow_graph;
  GumQuickCodeWriter code_writer;
  GumQuickCodeRelocator code_relocator;
  GumQuickCloak cloak;
  GumQuickApi api;
};

enum _GumWorkerState
{
  GUM_WORKER_CREATED,
  GUM_WORKER_INITIALIZED,
  GUM_WORKER_LOADED,
  GUM_WORKER_RUNNING,
  GUM_WORKER_TERMINATED,
};

struct _GumWorkerMessageDelivery
{
  GumQuickWorker * worker;
  gchar * message;
  GBytes * data;
};

static void gum_quick_script_iface_init (gpointer g_iface, gpointer iface_data);

static void gum_quick_script_dispose (GObject * object);
static void gum_quick_script_finalize (GObject * object);
static void gum_quick_script_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gum_quick_script_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gum_quick_script_destroy_context (GumQuickScript * self);

static void gum_quick_script_load (GumScript * script,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_quick_script_load_finish (GumScript * script,
    GAsyncResult * result);
static void gum_quick_script_load_sync (GumScript * script,
    GCancellable * cancellable);
static void gum_quick_script_do_load (GumScriptTask * task,
    GumQuickScript * self, gpointer task_data, GCancellable * cancellable);
static void gum_quick_script_execute_runtime (GumQuickScript * self,
    GumScriptTask * task);
static void gum_quick_script_on_runtime_loaded (JSValue error,
    gpointer user_data);
static void gum_quick_script_execute_entrypoints (GumQuickScript * self,
    GumScriptTask * task);
static JSValue gum_quick_script_on_entrypoints_executed (JSContext * ctx,
    JSValueConst this_val, int argc, JSValueConst * argv, int magic,
    JSValue * func_data);
static void gum_quick_script_schedule_load_task_completion (
    GumQuickScript * self, GumScriptTask * task);
static gboolean gum_quick_script_complete_load_task (GumScriptTask * task);
static void gum_quick_script_unload (GumScript * script,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_quick_script_unload_finish (GumScript * script,
    GAsyncResult * result);
static void gum_quick_script_unload_sync (GumScript * script,
    GCancellable * cancellable);
static void gum_quick_script_do_unload (GumScriptTask * task,
    GumQuickScript * self, gpointer task_data, GCancellable * cancellable);
static void gum_quick_script_complete_unload_task (GumQuickScript * self,
    GumScriptTask * task);
static void gum_quick_script_try_unload (GumQuickScript * self);
static void gum_quick_script_once_unloaded (GumQuickScript * self,
    GumUnloadNotifyFunc func, gpointer data, GDestroyNotify data_destroy);
static void gum_quick_script_interrupt (GumScript * script);
static void gum_quick_script_terminate (GumScript * script);

static int gum_quick_script_interrupt_handler (JSRuntime * runtime,
    void * opaque);
static void gum_quick_register_interrupt_handler (GumQuickScript * script);
static void gum_quick_remove_interrupt_handler (GumQuickScript * script);

static void gum_quick_script_set_message_handler (GumScript * script,
    GumScriptMessageHandler handler, gpointer data,
    GDestroyNotify data_destroy);
static void gum_quick_script_post (GumScript * script, const gchar * message,
    GBytes * data);
static void gum_quick_script_do_post (GumPostData * d);
static void gum_quick_post_data_free (GumPostData * d);

static void gum_quick_script_set_debug_message_handler (GumScript * backend,
    GumScriptDebugMessageHandler handler, gpointer data,
    GDestroyNotify data_destroy);
static void gum_quick_script_post_debug_message (GumScript * backend,
    const gchar * message);

static GumStalker * gum_quick_script_get_stalker (GumScript * script);

static void gum_quick_script_emit (const gchar * message, GBytes * data,
    GumQuickScript * self);
static gboolean gum_quick_script_do_emit (GumEmitData * d);
static void gum_quick_emit_data_free (GumEmitData * d);

static GumQuickWorker * gum_quick_worker_new (GumQuickScript * script,
    GumESAsset * asset, JSValue on_message);
static void gum_quick_worker_run (GumQuickWorker * self);
static void gum_quick_worker_on_runtime_loaded (JSValue error,
    gpointer user_data);
static void gum_quick_worker_flush (GumQuickWorker * self);
static void gum_quick_worker_do_post (GumWorkerMessageDelivery * d);
static void gum_quick_worker_emit (const gchar * message, GBytes * data,
    GumQuickWorker * self);
static void gum_quick_worker_do_emit (GumWorkerMessageDelivery * d);

static GumWorkerMessageDelivery * gum_worker_message_delivery_new (
    GumQuickWorker * worker, const gchar * message, GBytes * data);
static void gum_worker_message_delivery_free (GumWorkerMessageDelivery * d);

G_DEFINE_TYPE_EXTENDED (GumQuickScript,
                        gum_quick_script,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SCRIPT,
                            gum_quick_script_iface_init))

static void
gum_quick_script_class_init (GumQuickScriptClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_quick_script_dispose;
  object_class->finalize = gum_quick_script_finalize;
  object_class->get_property = gum_quick_script_get_property;
  object_class->set_property = gum_quick_script_set_property;

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Name", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE,
      g_param_spec_string ("source", "Source", "Source code", NULL,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BYTECODE,
      g_param_spec_boxed ("bytecode", "Bytecode", "Bytecode", G_TYPE_BYTES,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MAIN_CONTEXT,
      g_param_spec_boxed ("main-context", "MainContext",
      "MainContext being used", G_TYPE_MAIN_CONTEXT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BACKEND,
      g_param_spec_object ("backend", "Backend", "Backend being used",
      GUM_QUICK_TYPE_SCRIPT_BACKEND,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gum_quick_script_iface_init (gpointer g_iface,
                             gpointer iface_data)
{
  GumScriptInterface * iface = g_iface;

  iface->load = gum_quick_script_load;
  iface->load_finish = gum_quick_script_load_finish;
  iface->load_sync = gum_quick_script_load_sync;
  iface->unload = gum_quick_script_unload;
  iface->unload_finish = gum_quick_script_unload_finish;
  iface->unload_sync = gum_quick_script_unload_sync;
  iface->interrupt = gum_quick_script_interrupt;
  iface->terminate = gum_quick_script_terminate;

  iface->set_message_handler = gum_quick_script_set_message_handler;
  iface->post = gum_quick_script_post;

  iface->set_debug_message_handler = gum_quick_script_set_debug_message_handler;
  iface->post_debug_message = gum_quick_script_post_debug_message;

  iface->get_stalker = gum_quick_script_get_stalker;
}

static void
gum_quick_script_init (GumQuickScript * self)
{
  self->name = g_strdup ("agent");

  self->state = GUM_SCRIPT_STATE_CREATED;
  self->on_unload = NULL;
}

static void
gum_quick_script_dispose (GObject * object)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (object);
  GumScript * script = GUM_SCRIPT (self);

  gum_quick_script_set_message_handler (script, NULL, NULL, NULL);

  g_rec_mutex_lock (&self->interrupt_mutex);
  if (self->state == GUM_SCRIPT_STATE_LOADED)
  {
    /* dispose() will be triggered again at the end of unload() */
    gum_quick_script_unload (script, NULL, NULL, NULL);
  }
  else
  {
    if (self->state == GUM_SCRIPT_STATE_CREATED && self->ctx != NULL)
      gum_quick_script_destroy_context (self);

    g_clear_pointer (&self->main_context, g_main_context_unref);
    g_clear_pointer (&self->backend, g_object_unref);
  }
  g_rec_mutex_unlock (&self->interrupt_mutex);

  G_OBJECT_CLASS (gum_quick_script_parent_class)->dispose (object);
}

static void
gum_quick_script_finalize (GObject * object)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (object);

  g_rec_mutex_clear (&self->interrupt_mutex);

  g_free (self->name);
  g_free (self->source);
  g_bytes_unref (self->bytecode);

  G_OBJECT_CLASS (gum_quick_script_parent_class)->finalize (object);
}

static void
gum_quick_script_get_property (GObject * object,
                               guint property_id,
                               GValue * value,
                               GParamSpec * pspec)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (object);

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
gum_quick_script_set_property (GObject * object,
                               guint property_id,
                               const GValue * value,
                               GParamSpec * pspec)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (object);

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
    case PROP_BYTECODE:
      g_bytes_unref (self->bytecode);
      self->bytecode = g_value_dup_boxed (value);
      break;
    case PROP_MAIN_CONTEXT:
      if (self->main_context != NULL)
        g_main_context_unref (self->main_context);
      self->main_context = g_value_dup_boxed (value);
      break;
    case PROP_BACKEND:
      if (self->backend != NULL)
        g_object_unref (self->backend);
      self->backend = GUM_QUICK_SCRIPT_BACKEND (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

gboolean
gum_quick_script_create_context (GumQuickScript * self,
                                 GError ** error)
{
  GumQuickCore * core = &self->core;
  JSRuntime * rt;
  JSContext * ctx;
  GumESProgram * program;
  JSValue global_obj;
  GumQuickScope scope = { core, NULL, };

  g_assert (self->ctx == NULL);

  rt = gum_quick_script_backend_make_runtime (self->backend);
  JS_SetRuntimeOpaque (rt, core);

  ctx = JS_NewContext (rt);
  JS_SetContextOpaque (ctx, core);

  if (self->bytecode != NULL)
  {
    program = gum_quick_script_backend_read_program (self->backend, ctx,
        self->bytecode, error);
  }
  else
  {
    program = gum_quick_script_backend_compile_program (self->backend, ctx,
        self->name, self->source, error);
  }
  if (program == NULL)
    goto malformed_program;

  self->rt = rt;
  self->ctx = ctx;
  self->program = program;

  global_obj = JS_GetGlobalObject (ctx);

  _gum_quick_core_init (core, self, ctx, global_obj,
      gum_quick_script_backend_get_scope_mutex (self->backend),
      program, &self->interceptor, &self->stalker,
      (GumQuickMessageEmitter) gum_quick_script_emit, self,
      gum_quick_script_backend_get_scheduler (self->backend));

  core->current_scope = &scope;

  _gum_quick_kernel_init (&self->kernel, global_obj, core);
  _gum_quick_memory_init (&self->memory, global_obj, core);
  _gum_quick_module_init (&self->module, global_obj, core);
  _gum_quick_thread_init (&self->thread, global_obj, core);
  _gum_quick_process_init (&self->process, global_obj, &self->module,
      &self->thread, core);
  _gum_quick_file_init (&self->file, global_obj, core);
  _gum_quick_checksum_init (&self->checksum, global_obj, core);
#ifndef G_OS_NONE
  _gum_quick_stream_init (&self->stream, global_obj, core);
  _gum_quick_socket_init (&self->socket, global_obj, &self->stream, core);
#endif
#ifdef HAVE_SQLITE
  _gum_quick_database_init (&self->database, global_obj, core);
#endif
  _gum_quick_interceptor_init (&self->interceptor, global_obj,
      &self->code_writer, core);
  _gum_quick_api_resolver_init (&self->api_resolver, global_obj, core);
  _gum_quick_symbol_init (&self->symbol, global_obj, core);
  _gum_quick_cmodule_init (&self->cmodule, global_obj, core);
  _gum_quick_instruction_init (&self->instruction, global_obj, core);
  _gum_quick_control_flow_graph_init (&self->control_flow_graph, global_obj,
      &self->instruction, core);
  _gum_quick_code_writer_init (&self->code_writer, global_obj, core);
  _gum_quick_code_relocator_init (&self->code_relocator, global_obj,
      &self->code_writer, &self->instruction, core);
  _gum_quick_stalker_init (&self->stalker, global_obj, &self->code_writer,
      &self->instruction, core);
  _gum_quick_cloak_init (&self->cloak, global_obj, core);
  _gum_quick_sampler_init (&self->sampler, global_obj, core);
  _gum_quick_profiler_init (&self->profiler, global_obj, &self->sampler,
      &self->interceptor, core);
  _gum_quick_api_init (&self->api, global_obj, core);

  JS_FreeValue (ctx, global_obj);

  core->current_scope = NULL;

  g_free (self->source);
  self->source = NULL;

  g_bytes_unref (self->bytecode);
  self->bytecode = NULL;

  g_rec_mutex_init (&self->interrupt_mutex);
  self->interrupt = GUM_INTERRUPT_NONE;
  self->executing = FALSE;
  gum_quick_register_interrupt_handler (self);

  return TRUE;

malformed_program:
  {
    JS_FreeContext (ctx);
    JS_FreeRuntime (rt);

    return FALSE;
  }
}

static void
gum_quick_script_destroy_context (GumQuickScript * self)
{
  GumQuickCore * core = &self->core;

  g_assert (self->ctx != NULL);

  {
    GumQuickScope scope;

    _gum_quick_scope_enter (&scope, core);

    _gum_quick_profiler_dispose (&self->profiler);
    _gum_quick_sampler_dispose (&self->sampler);
    _gum_quick_cloak_dispose (&self->cloak);
    _gum_quick_stalker_dispose (&self->stalker);
    _gum_quick_code_relocator_dispose (&self->code_relocator);
    _gum_quick_code_writer_dispose (&self->code_writer);
    _gum_quick_control_flow_graph_dispose (&self->control_flow_graph);
    _gum_quick_instruction_dispose (&self->instruction);
    _gum_quick_cmodule_dispose (&self->cmodule);
    _gum_quick_symbol_dispose (&self->symbol);
    _gum_quick_api_resolver_dispose (&self->api_resolver);
    _gum_quick_interceptor_dispose (&self->interceptor);
#ifdef HAVE_SQLITE
    _gum_quick_database_dispose (&self->database);
#endif
#ifndef G_OS_NONE
    _gum_quick_socket_dispose (&self->socket);
    _gum_quick_stream_dispose (&self->stream);
#endif
    _gum_quick_checksum_dispose (&self->checksum);
    _gum_quick_file_dispose (&self->file);
    _gum_quick_process_dispose (&self->process);
    _gum_quick_thread_dispose (&self->thread);
    _gum_quick_module_dispose (&self->module);
    _gum_quick_memory_dispose (&self->memory);
    _gum_quick_kernel_dispose (&self->kernel);
    _gum_quick_core_dispose (core);

    _gum_quick_scope_leave (&scope);
  }

  {
    GumQuickScope scope = { core, NULL, };

    core->current_scope = &scope;

    gum_es_program_free (self->program, self->ctx);
    self->program = NULL;

    JS_FreeContext (self->ctx);
    self->ctx = NULL;

    gum_quick_remove_interrupt_handler (self);
    self->interrupt = GUM_INTERRUPT_NONE;
    /*
     * NOTE: interrupt_mutex is NOT cleared here. dispose() locks it after
     * destroy_context runs (via unload), so it must remain valid until
     * finalize().
     */

    JS_FreeRuntime (self->rt);
    self->rt = NULL;

    core->current_scope = NULL;
  }

  _gum_quick_api_finalize (&self->api);
  _gum_quick_profiler_finalize (&self->profiler);
  _gum_quick_sampler_finalize (&self->sampler);
  _gum_quick_cloak_finalize (&self->cloak);
  _gum_quick_stalker_finalize (&self->stalker);
  _gum_quick_code_relocator_finalize (&self->code_relocator);
  _gum_quick_code_writer_finalize (&self->code_writer);
  _gum_quick_control_flow_graph_finalize (&self->control_flow_graph);
  _gum_quick_instruction_finalize (&self->instruction);
  _gum_quick_cmodule_finalize (&self->cmodule);
  _gum_quick_symbol_finalize (&self->symbol);
  _gum_quick_api_resolver_finalize (&self->api_resolver);
  _gum_quick_interceptor_finalize (&self->interceptor);
#ifdef HAVE_SQLITE
  _gum_quick_database_finalize (&self->database);
#endif
#ifndef G_OS_NONE
  _gum_quick_socket_finalize (&self->socket);
  _gum_quick_stream_finalize (&self->stream);
#endif
  _gum_quick_checksum_finalize (&self->checksum);
  _gum_quick_file_finalize (&self->file);
  _gum_quick_process_finalize (&self->process);
  _gum_quick_thread_finalize (&self->thread);
  _gum_quick_module_finalize (&self->module);
  _gum_quick_memory_finalize (&self->memory);
  _gum_quick_kernel_finalize (&self->kernel);
  _gum_quick_core_finalize (core);
}

static void
gum_quick_script_load (GumScript * script,
                       GCancellable * cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);
  GumScriptTask * task;

  task = gum_script_task_new ((GumScriptTaskFunc) gum_quick_script_do_load,
      self, cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_quick_script_backend_get_scheduler (self->backend));
  g_object_unref (task);
}

static void
gum_quick_script_load_finish (GumScript * script,
                              GAsyncResult * result)
{
  gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), NULL);
}

static void
gum_quick_script_load_sync (GumScript * script,
                            GCancellable * cancellable)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);
  GumScriptTask * task;

  task = gum_script_task_new ((GumScriptTaskFunc) gum_quick_script_do_load,
      self, cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_quick_script_backend_get_scheduler (self->backend));
  gum_script_task_propagate_pointer (task, NULL);
  g_object_unref (task);
}

static void
gum_quick_script_do_load (GumScriptTask * task,
                          GumQuickScript * self,
                          gpointer task_data,
                          GCancellable * cancellable)
{
  if (self->state != GUM_SCRIPT_STATE_CREATED)
    goto invalid_operation;

  self->state = GUM_SCRIPT_STATE_LOADING;

  gum_quick_script_execute_runtime (self, task);

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
gum_quick_script_execute_runtime (GumQuickScript * self,
                                  GumScriptTask * task)
{
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, &self->core);

  gum_es_program_load_runtime (self->program, self->ctx,
      gum_quick_script_on_runtime_loaded, g_object_ref (task),
      g_object_unref);

  _gum_quick_scope_leave (&scope);
}

static void
gum_quick_script_on_runtime_loaded (JSValue error,
                                    gpointer user_data)
{
  GumScriptTask * task = user_data;
  GumQuickScript * self;

  self = GUM_QUICK_SCRIPT (
      g_async_result_get_source_object (G_ASYNC_RESULT (task)));

  if (JS_IsNull (error))
  {
    gum_quick_script_execute_entrypoints (self, task);
  }
  else
  {
    _gum_quick_core_on_unhandled_exception (&self->core, error);
    gum_quick_script_schedule_load_task_completion (self, task);
  }

  g_object_unref (self);
}

static void
gum_quick_script_execute_entrypoints (GumQuickScript * self,
                                      GumScriptTask * task)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (&self->core);
  JSContext * ctx = self->ctx;
  GArray * entrypoints;
  guint i;

  entrypoints = self->program->entrypoints;

  if (gum_es_program_is_esm (self->program))
  {
    JSValue pending;
    guint num_results;
    JSValue global_obj, promise_class, all_settled, loaded_promise, then;
    JSValue task_obj, on_loaded, val;

    pending = JS_NewArray (ctx);
    num_results = 0;
    for (i = 0; i != entrypoints->len; i++)
    {
      JSValue result;

      result = JS_EvalFunction (ctx, g_array_index (entrypoints, JSValue, i));
      if (JS_IsException (result))
      {
        _gum_quick_scope_catch_and_emit (&scope);
      }
      else
      {
        JS_DefinePropertyValueUint32 (ctx, pending, num_results++, result,
            JS_PROP_C_W_E);
      }
    }

    global_obj = JS_GetGlobalObject (ctx);
    promise_class = JS_GetPropertyStr (ctx, global_obj, "Promise");
    all_settled = JS_GetPropertyStr (ctx, promise_class, "allSettled");

    loaded_promise = JS_Call (ctx, all_settled, promise_class, 1, &pending);

    then = JS_GetPropertyStr (ctx, loaded_promise, "then");

    task_obj = JS_NewObject (ctx);
    JS_SetOpaque (task_obj, g_object_ref (task));

    on_loaded = JS_NewCFunctionData (ctx,
        gum_quick_script_on_entrypoints_executed, 1, 0, 1, &task_obj);

    val = JS_Call (ctx, then, loaded_promise, 1, &on_loaded);

    JS_FreeValue (ctx, val);
    JS_FreeValue (ctx, on_loaded);
    JS_FreeValue (ctx, task_obj);
    JS_FreeValue (ctx, then);
    JS_FreeValue (ctx, loaded_promise);
    JS_FreeValue (ctx, all_settled);
    JS_FreeValue (ctx, promise_class);
    JS_FreeValue (ctx, global_obj);
    JS_FreeValue (ctx, pending);
  }
  else
  {
    for (i = 0; i != entrypoints->len; i++)
    {
      JSValue result;

      result = JS_EvalFunction (ctx, g_array_index (entrypoints, JSValue, i));
      if (JS_IsException (result))
        _gum_quick_scope_catch_and_emit (&scope);

      JS_FreeValue (ctx, result);
    }

    gum_quick_script_schedule_load_task_completion (self, task);
  }

  g_array_set_size (entrypoints, 0);
}

static JSValue
gum_quick_script_on_entrypoints_executed (JSContext * ctx,
                                          JSValueConst this_val,
                                          int argc,
                                          JSValueConst * argv,
                                          int magic,
                                          JSValue * func_data)
{
  JSValueConst results = argv[0];
  GumScriptTask * task;
  JSClassID class_id;
  GumQuickScript * self;
  GumQuickCore * core;
  guint n, i;

  task = JS_GetAnyOpaque (func_data[0], &class_id);
  self = GUM_QUICK_SCRIPT (
      g_async_result_get_source_object (G_ASYNC_RESULT (task)));
  core = &self->core;

  _gum_quick_array_get_length (ctx, results, core, &n);
  for (i = 0; i != n; i++)
  {
    JSValue result, reason;

    result = JS_GetPropertyUint32 (ctx, results, i);

    reason = JS_GetPropertyStr (ctx, result, "reason");
    if (!JS_IsUndefined (reason))
      _gum_quick_core_on_unhandled_exception (core, reason);

    JS_FreeValue (ctx, reason);
    JS_FreeValue (ctx, result);
  }

  gum_quick_script_schedule_load_task_completion (self, task);

  g_object_unref (self);
  g_object_unref (task);

  return JS_UNDEFINED;
}

static void
gum_quick_script_schedule_load_task_completion (GumQuickScript * self,
                                                GumScriptTask * task)
{
  GumQuickCore * core = &self->core;
  GSource * source;

  source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gum_quick_script_complete_load_task,
      g_object_ref (task), g_object_unref);
  g_source_attach (source,
      gum_script_scheduler_get_js_context (core->scheduler));
  g_source_unref (source);

  _gum_quick_core_pin (core);
}

static gboolean
gum_quick_script_complete_load_task (GumScriptTask * task)
{
  GumQuickScript * self;
  GumQuickCore * core;
  GumQuickScope scope;
  gboolean terminating;

  self = GUM_QUICK_SCRIPT (
      g_async_result_get_source_object (G_ASYNC_RESULT (task)));
  core = &self->core;

  _gum_quick_scope_enter (&scope, core);
  _gum_quick_core_unpin (core);
  _gum_quick_scope_leave (&scope);

  g_rec_mutex_lock (&self->interrupt_mutex);
  self->state = GUM_SCRIPT_STATE_LOADED;
  terminating = self->interrupt == GUM_INTERRUPT_FOREVER;
  g_rec_mutex_unlock (&self->interrupt_mutex);

  gum_script_task_return_pointer (task, NULL, NULL);

  if (terminating)
    gum_quick_script_unload (GUM_SCRIPT (self), NULL, NULL, NULL);

  g_object_unref (self);

  return G_SOURCE_REMOVE;
}

static void
gum_quick_script_unload (GumScript * script,
                         GCancellable * cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);
  GumScriptTask * task;

  task = gum_script_task_new ((GumScriptTaskFunc) gum_quick_script_do_unload,
      self, cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_quick_script_backend_get_scheduler (self->backend));
  g_object_unref (task);
}

static void
gum_quick_script_unload_finish (GumScript * script,
                                GAsyncResult * result)
{
  gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), NULL);
}

static void
gum_quick_script_unload_sync (GumScript * script,
                              GCancellable * cancellable)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);
  GumScriptTask * task;

  task = gum_script_task_new ((GumScriptTaskFunc) gum_quick_script_do_unload,
      self, cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_quick_script_backend_get_scheduler (self->backend));
  gum_script_task_propagate_pointer (task, NULL);
  g_object_unref (task);
}

static void
gum_quick_script_do_unload (GumScriptTask * task,
                            GumQuickScript * self,
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
  self->interrupt = GUM_INTERRUPT_NONE;
  g_rec_mutex_unlock (&self->interrupt_mutex);

  gum_quick_script_once_unloaded (self,
      (GumUnloadNotifyFunc) gum_quick_script_complete_unload_task,
      g_object_ref (task), g_object_unref);

  gum_quick_script_try_unload (self);

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
gum_quick_script_complete_unload_task (GumQuickScript * self,
                                       GumScriptTask * task)
{
  gum_script_task_return_pointer (task, NULL, NULL);
}

static void
gum_quick_script_try_unload (GumQuickScript * self)
{
  GumQuickScope scope;
  gboolean success;

  g_assert (self->state == GUM_SCRIPT_STATE_UNLOADING);

  _gum_quick_scope_enter (&scope, &self->core);

  _gum_quick_stalker_flush (&self->stalker);
  _gum_quick_interceptor_flush (&self->interceptor);
#ifndef G_OS_NONE
  _gum_quick_socket_flush (&self->socket);
  _gum_quick_stream_flush (&self->stream);
#endif
  _gum_quick_process_flush (&self->process);
  success = _gum_quick_core_flush (&self->core,
      (GumQuickFlushNotify) gum_quick_script_try_unload,
      g_object_ref (self), g_object_unref);

  _gum_quick_scope_leave (&scope);

  if (success)
  {
    gum_quick_script_destroy_context (self);

    self->state = GUM_SCRIPT_STATE_UNLOADED;

    while (self->on_unload != NULL)
    {
      GSList * link = self->on_unload;
      GumUnloadNotifyCallback * callback = link->data;

      callback->func (self, callback->data);
      if (callback->data_destroy != NULL)
        callback->data_destroy (callback->data);
      g_slice_free (GumUnloadNotifyCallback, callback);

      self->on_unload = g_slist_delete_link (self->on_unload, link);
    }
  }
}

static void
gum_quick_script_once_unloaded (GumQuickScript * self,
                                GumUnloadNotifyFunc func,
                                gpointer data,
                                GDestroyNotify data_destroy)
{
  GumUnloadNotifyCallback * callback;

  callback = g_slice_new (GumUnloadNotifyCallback);
  callback->func = func;
  callback->data = data;
  callback->data_destroy = data_destroy;

  self->on_unload = g_slist_append (self->on_unload, callback);
}

static void
gum_quick_script_interrupt (GumScript * script)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);

  g_rec_mutex_lock (&self->interrupt_mutex);
  if (self->executing && self->interrupt == GUM_INTERRUPT_NONE)
    self->interrupt = GUM_INTERRUPT_ONCE;
  g_rec_mutex_unlock (&self->interrupt_mutex);
}

static void
gum_quick_script_terminate (GumScript * script)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);

  g_rec_mutex_lock (&self->interrupt_mutex);
  self->interrupt = GUM_INTERRUPT_FOREVER;
  if (self->state == GUM_SCRIPT_STATE_LOADED)
    gum_quick_script_unload (script, NULL, NULL, NULL);
  g_rec_mutex_unlock (&self->interrupt_mutex);
}

void
_gum_quick_script_on_scope_entered (GumQuickCore * core)
{
  GumQuickScript * self = core->script;

  if (core != &self->core)
    return;

  g_rec_mutex_lock (&self->interrupt_mutex);
  self->executing = TRUE;
  g_rec_mutex_unlock (&self->interrupt_mutex);
}

void
_gum_quick_script_on_scope_left (GumQuickCore * core)
{
  GumQuickScript * self = core->script;

  if (core != &self->core)
    return;

  g_rec_mutex_lock (&self->interrupt_mutex);
  self->executing = FALSE;
  if (self->interrupt == GUM_INTERRUPT_ONCE)
    self->interrupt = GUM_INTERRUPT_NONE;
  g_rec_mutex_unlock (&self->interrupt_mutex);
}

static int
gum_quick_script_interrupt_handler (JSRuntime * runtime,
                                    void * opaque)
{
  GumQuickScript * script = opaque;
  int rc;

  g_rec_mutex_lock (&script->interrupt_mutex);
  rc = script->interrupt != GUM_INTERRUPT_NONE;
  if (script->interrupt == GUM_INTERRUPT_ONCE)
    script->interrupt = GUM_INTERRUPT_NONE;
  g_rec_mutex_unlock (&script->interrupt_mutex);

  return rc;
}

static void
gum_quick_register_interrupt_handler (GumQuickScript * script)
{
  g_rec_mutex_lock (&script->interrupt_mutex);
  g_assert (script->interrupt == GUM_INTERRUPT_NONE);
  g_rec_mutex_unlock (&script->interrupt_mutex);

  JS_SetInterruptHandler (script->rt, gum_quick_script_interrupt_handler,
      script);
}

static void
gum_quick_remove_interrupt_handler (GumQuickScript * script)
{
  JS_SetInterruptHandler (script->rt, NULL, NULL);
}

static void
gum_quick_script_set_message_handler (GumScript * script,
                                      GumScriptMessageHandler handler,
                                      gpointer data,
                                      GDestroyNotify data_destroy)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);

  if (self->message_handler_data_destroy != NULL)
    self->message_handler_data_destroy (self->message_handler_data);
  self->message_handler = handler;
  self->message_handler_data = data;
  self->message_handler_data_destroy = data_destroy;
}

static void
gum_quick_script_post (GumScript * script,
                       const gchar * message,
                       GBytes * data)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);
  GumPostData * d;

  d = g_slice_new (GumPostData);
  d->script = self;
  g_object_ref (self);
  d->message = g_strdup (message);
  d->data = (data != NULL) ? g_bytes_ref (data) : NULL;

  gum_script_scheduler_push_job_on_js_thread (
      gum_quick_script_backend_get_scheduler (self->backend),
      G_PRIORITY_DEFAULT, (GumScriptJobFunc) gum_quick_script_do_post, d,
      (GDestroyNotify) gum_quick_post_data_free);
}

static void
gum_quick_script_do_post (GumPostData * d)
{
  GBytes * data;

  data = d->data;
  d->data = NULL;

  _gum_quick_core_post (&d->script->core, d->message, data);
}

static void
gum_quick_post_data_free (GumPostData * d)
{
  g_free (d->message);
  g_object_unref (d->script);

  g_slice_free (GumPostData, d);
}

static void
gum_quick_script_set_debug_message_handler (
    GumScript * backend,
    GumScriptDebugMessageHandler handler,
    gpointer data,
    GDestroyNotify data_destroy)
{
  if (data_destroy != NULL)
    data_destroy (data);
}

static void
gum_quick_script_post_debug_message (GumScript * backend,
                                     const gchar * message)
{
}

static GumStalker *
gum_quick_script_get_stalker (GumScript * script)
{
  GumQuickScript * self = GUM_QUICK_SCRIPT (script);

  return _gum_quick_stalker_get (&self->stalker);
}

static void
gum_quick_script_emit (const gchar * message,
                       GBytes * data,
                       GumQuickScript * self)
{
  GumEmitData * d;
  GSource * source;

  d = g_slice_new (GumEmitData);
  d->script = self;
  g_object_ref (self);
  d->message = g_strdup (message);
  d->data = (data != NULL) ? g_bytes_ref (data) : NULL;

  source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gum_quick_script_do_emit,
      d,
      (GDestroyNotify) gum_quick_emit_data_free);
  g_source_attach (source, self->main_context);
  g_source_unref (source);
}

static gboolean
gum_quick_script_do_emit (GumEmitData * d)
{
  GumQuickScript * self = d->script;

  if (self->message_handler != NULL)
    self->message_handler (d->message, d->data, self->message_handler_data);

  return FALSE;
}

static void
gum_quick_emit_data_free (GumEmitData * d)
{
  g_bytes_unref (d->data);
  g_free (d->message);
  g_object_unref (d->script);

  g_slice_free (GumEmitData, d);
}

GumQuickWorker *
_gum_quick_script_make_worker (GumQuickScript * self,
                               const gchar * url,
                               JSValue on_message)
{
  GumQuickWorker * worker;
  GumESAsset * asset;
  JSContext * ctx;
  JSValue mod;
  JSValue global_obj;
  GumQuickCore * core;

  if (!g_str_has_prefix (url, "file://"))
    goto invalid_url;

  asset = g_hash_table_lookup (self->program->es_assets,
      url + strlen ("file://"));
  if (asset == NULL)
    goto invalid_url;

  worker = gum_quick_worker_new (self, asset, on_message);
  ctx = worker->ctx;

  mod = gum_es_program_compile_worker (self->program, ctx, asset);
  if (JS_IsException (mod))
    goto malformed_module;
  worker->entrypoint = mod;

  global_obj = JS_GetGlobalObject (ctx);

  core = &worker->core;

  {
    GumQuickScope scope = { core, NULL, };

    _gum_quick_core_init (core, self, ctx, global_obj, &worker->scope_mutex,
        self->program, NULL, NULL,
        (GumQuickMessageEmitter) gum_quick_worker_emit, worker,
        worker->scheduler);

    core->current_scope = &scope;

    _gum_quick_kernel_init (&worker->kernel, global_obj, core);
    _gum_quick_memory_init (&worker->memory, global_obj, core);
    _gum_quick_module_init (&worker->module, global_obj, core);
    _gum_quick_thread_init (&worker->thread, global_obj, core);
    _gum_quick_process_init (&worker->process, global_obj, &worker->module,
        &worker->thread, core);
    _gum_quick_file_init (&worker->file, global_obj, core);
    _gum_quick_checksum_init (&worker->checksum, global_obj, core);
#ifndef G_OS_NONE
    _gum_quick_stream_init (&worker->stream, global_obj, core);
    _gum_quick_socket_init (&worker->socket, global_obj, &worker->stream, core);
#endif
#ifdef HAVE_SQLITE
    _gum_quick_database_init (&worker->database, global_obj, core);
#endif
    _gum_quick_api_resolver_init (&worker->api_resolver, global_obj, core);
    _gum_quick_symbol_init (&worker->symbol, global_obj, core);
    _gum_quick_cmodule_init (&worker->cmodule, global_obj, core);
    _gum_quick_instruction_init (&worker->instruction, global_obj, core);
    _gum_quick_control_flow_graph_init (&worker->control_flow_graph, global_obj,
        &worker->instruction, core);
    _gum_quick_code_writer_init (&worker->code_writer, global_obj, core);
    _gum_quick_code_relocator_init (&worker->code_relocator, global_obj,
        &worker->code_writer, &worker->instruction, core);
    _gum_quick_cloak_init (&worker->cloak, global_obj, core);
    _gum_quick_api_init (&worker->api, global_obj, core);

    core->current_scope = NULL;
  }

  JS_FreeValue (ctx, global_obj);

  worker->state = GUM_WORKER_INITIALIZED;

  gum_script_scheduler_push_job_on_js_thread (worker->scheduler,
      G_PRIORITY_DEFAULT, (GumScriptJobFunc) gum_quick_worker_run,
      _gum_quick_worker_ref (worker),
      (GDestroyNotify) _gum_quick_worker_unref);

  return worker;

invalid_url:
  {
    _gum_quick_throw_literal (self->ctx, "invalid URL");

    return NULL;
  }
malformed_module:
  {
    _gum_quick_script_rethrow_parse_error_with_decorations (self, ctx,
        asset->name);

    _gum_quick_worker_unref (worker);

    return NULL;
  }
}

static GumQuickWorker *
gum_quick_worker_new (GumQuickScript * script,
                      GumESAsset * asset,
                      JSValue on_message)
{
  GumQuickWorker * worker;

  worker = g_slice_new0 (GumQuickWorker);
  worker->ref_count = 1;

  worker->state = GUM_WORKER_CREATED;

  worker->flushed = FALSE;
  g_mutex_init (&worker->flush_mutex);
  g_cond_init (&worker->flush_cond);

  worker->script = script;
  worker->asset = gum_es_asset_ref (asset);
  worker->on_message = JS_DupValue (script->ctx, on_message);

  worker->scheduler = gum_script_scheduler_new ();

  g_rec_mutex_init (&worker->scope_mutex);

  worker->rt = gum_quick_script_backend_make_runtime (script->backend);
  JS_SetRuntimeOpaque (worker->rt, &worker->core);

  worker->ctx = JS_NewContext (worker->rt);
  JS_SetContextOpaque (worker->ctx, &worker->core);

  worker->entrypoint = JS_NULL;

  return worker;
}

GumQuickWorker *
_gum_quick_worker_ref (GumQuickWorker * worker)
{
  g_atomic_int_inc (&worker->ref_count);

  return worker;
}

void
_gum_quick_worker_unref (GumQuickWorker * worker)
{
  GumQuickCore * core;

  if (worker == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&worker->ref_count))
    return;

  g_assert (worker->state == GUM_WORKER_CREATED ||
      worker->state == GUM_WORKER_TERMINATED);

  g_object_unref (worker->scheduler);

  core = &worker->core;

  if (worker->state != GUM_WORKER_CREATED)
  {
    GumQuickScope scope;

    _gum_quick_scope_enter (&scope, core);

    _gum_quick_code_relocator_dispose (&worker->code_relocator);
    _gum_quick_code_writer_dispose (&worker->code_writer);
    _gum_quick_control_flow_graph_dispose (&worker->control_flow_graph);
    _gum_quick_instruction_dispose (&worker->instruction);
    _gum_quick_cmodule_dispose (&worker->cmodule);
    _gum_quick_symbol_dispose (&worker->symbol);
    _gum_quick_api_resolver_dispose (&worker->api_resolver);
#ifdef HAVE_SQLITE
    _gum_quick_database_dispose (&worker->database);
#endif
#ifndef G_OS_NONE
    _gum_quick_socket_dispose (&worker->socket);
    _gum_quick_stream_dispose (&worker->stream);
#endif
    _gum_quick_checksum_dispose (&worker->checksum);
    _gum_quick_file_dispose (&worker->file);
    _gum_quick_thread_dispose (&worker->thread);
    _gum_quick_process_dispose (&worker->process);
    _gum_quick_module_dispose (&worker->module);
    _gum_quick_memory_dispose (&worker->memory);
    _gum_quick_kernel_dispose (&worker->kernel);
    _gum_quick_core_dispose (core);

    _gum_quick_scope_leave (&scope);
  }

  {
    GumQuickScope scope = { core, NULL, };

    core->current_scope = &scope;

    JS_FreeContext (worker->ctx);
    JS_FreeRuntime (worker->rt);

    core->current_scope = NULL;
  }

  if (worker->state != GUM_WORKER_CREATED)
  {
    _gum_quick_api_finalize (&worker->api);
    _gum_quick_code_relocator_finalize (&worker->code_relocator);
    _gum_quick_code_writer_finalize (&worker->code_writer);
    _gum_quick_control_flow_graph_finalize (&worker->control_flow_graph);
    _gum_quick_instruction_finalize (&worker->instruction);
    _gum_quick_cmodule_finalize (&worker->cmodule);
    _gum_quick_symbol_finalize (&worker->symbol);
    _gum_quick_api_resolver_finalize (&worker->api_resolver);
#ifdef HAVE_SQLITE
    _gum_quick_database_finalize (&worker->database);
#endif
#ifndef G_OS_NONE
    _gum_quick_socket_finalize (&worker->socket);
    _gum_quick_stream_finalize (&worker->stream);
#endif
    _gum_quick_checksum_finalize (&worker->checksum);
    _gum_quick_file_finalize (&worker->file);
    _gum_quick_thread_finalize (&worker->thread);
    _gum_quick_process_finalize (&worker->process);
    _gum_quick_module_finalize (&worker->module);
    _gum_quick_memory_finalize (&worker->memory);
    _gum_quick_kernel_finalize (&worker->kernel);
    _gum_quick_core_finalize (core);
  }

  g_rec_mutex_clear (&worker->scope_mutex);

  JS_FreeValue (worker->script->ctx, worker->on_message);

  gum_es_asset_unref (worker->asset);

  g_cond_clear (&worker->flush_cond);
  g_mutex_clear (&worker->flush_mutex);

  g_slice_free (GumQuickWorker, worker);
}

static void
gum_quick_worker_run (GumQuickWorker * self)
{
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, &self->core);

  gum_es_program_load_runtime (self->script->program, self->ctx,
      gum_quick_worker_on_runtime_loaded, _gum_quick_worker_ref (self),
      (GDestroyNotify) _gum_quick_worker_unref);

  _gum_quick_scope_leave (&scope);
}

static void
gum_quick_worker_on_runtime_loaded (JSValue error,
                                    gpointer user_data)
{
  GumQuickWorker * self = user_data;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (&self->core);
  JSContext * ctx = self->ctx;
  JSValue val;

  if (!JS_IsNull (error))
  {
    _gum_quick_core_on_unhandled_exception (&self->core, error);
    return;
  }

  val = JS_EvalFunction (ctx, self->entrypoint);
  if (!JS_IsException (val))
    self->state = GUM_WORKER_LOADED;
  else
    _gum_quick_scope_catch_and_emit (&scope);

  JS_FreeValue (ctx, val);

  if (self->state == GUM_WORKER_LOADED)
  {
    gchar * init_code;

    init_code = g_strdup_printf (
        "(async () => {\n"
        "  try {\n"
        "    const w = await import('%s');\n"
        "    await w.run();\n"
        "  } catch (e) {\n"
        "    Script.nextTick(() => { throw e; });\n"
        "  }\n"
        "})();\n",
        self->asset->name);

    val = JS_Eval (ctx, init_code, strlen (init_code),
        "/_frida_worker_runtime.js",
        JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
    if (!JS_IsException (val))
      self->state = GUM_WORKER_RUNNING;
    else
      _gum_quick_scope_catch_and_emit (&scope);

    JS_FreeValue (ctx, val);
    g_free (init_code);
  }
}

void
_gum_quick_worker_terminate (GumQuickWorker * self)
{
  if (self->state == GUM_WORKER_TERMINATED)
    return;

  gum_script_scheduler_push_job_on_js_thread (self->scheduler,
      G_PRIORITY_DEFAULT, (GumScriptJobFunc) gum_quick_worker_flush,
      self, NULL);

  g_mutex_lock (&self->flush_mutex);
  while (!self->flushed)
    g_cond_wait (&self->flush_cond, &self->flush_mutex);
  g_mutex_unlock (&self->flush_mutex);

  gum_script_scheduler_stop (self->scheduler);

  self->state = GUM_WORKER_TERMINATED;
}

static void
gum_quick_worker_flush (GumQuickWorker * self)
{
  GumQuickScope scope;
  gboolean success;

  _gum_quick_scope_enter (&scope, &self->core);

#ifndef G_OS_NONE
  _gum_quick_socket_flush (&self->socket);
  _gum_quick_stream_flush (&self->stream);
#endif
  _gum_quick_process_flush (&self->process);
  success = _gum_quick_core_flush (&self->core,
      (GumQuickFlushNotify) gum_quick_worker_flush,
      _gum_quick_worker_ref (self),
      (GDestroyNotify) _gum_quick_worker_unref);

  _gum_quick_scope_leave (&scope);

  if (success)
  {
    g_mutex_lock (&self->flush_mutex);
    self->flushed = TRUE;
    g_cond_signal (&self->flush_cond);
    g_mutex_unlock (&self->flush_mutex);
  }
}

void
_gum_quick_worker_post (GumQuickWorker * self,
                        const gchar * message,
                        GBytes * data)
{
  gum_script_scheduler_push_job_on_js_thread (self->scheduler,
      G_PRIORITY_DEFAULT, (GumScriptJobFunc) gum_quick_worker_do_post,
      gum_worker_message_delivery_new (self, message, data),
      (GDestroyNotify) gum_worker_message_delivery_free);
}

static void
gum_quick_worker_do_post (GumWorkerMessageDelivery * d)
{
  _gum_quick_core_post (&d->worker->core, d->message,
      g_steal_pointer (&d->data));
}

static void
gum_quick_worker_emit (const gchar * message,
                       GBytes * data,
                       GumQuickWorker * self)
{
  gum_script_scheduler_push_job_on_js_thread (
      gum_quick_script_backend_get_scheduler (self->core.backend),
      G_PRIORITY_DEFAULT, (GumScriptJobFunc) gum_quick_worker_do_emit,
      gum_worker_message_delivery_new (self, message, data),
      (GDestroyNotify) gum_worker_message_delivery_free);
}

static void
gum_quick_worker_do_emit (GumWorkerMessageDelivery * d)
{
  GumQuickWorker * self = d->worker;
  GumQuickScript * script = self->script;
  JSContext * ctx = script->ctx;
  GumQuickScope scope;
  JSValue argv[2];

  _gum_quick_scope_enter (&scope, &script->core);

  argv[0] = JS_NewString (ctx, d->message);

  if (d->data != NULL)
  {
    gpointer data_buffer;
    gsize data_size;

    data_buffer =
        g_bytes_unref_to_data (g_steal_pointer (&d->data), &data_size);

    argv[1] = JS_NewArrayBuffer (ctx, data_buffer, data_size,
        _gum_quick_array_buffer_free, data_buffer, FALSE);
  }
  else
  {
    argv[1] = JS_NULL;
  }

  _gum_quick_scope_call_void (&scope, self->on_message, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[1]);
  JS_FreeValue (ctx, argv[0]);

  _gum_quick_scope_leave (&scope);
}

static GumWorkerMessageDelivery *
gum_worker_message_delivery_new (GumQuickWorker * worker,
                                 const gchar * message,
                                 GBytes * data)
{
  GumWorkerMessageDelivery * d;

  d = g_slice_new (GumWorkerMessageDelivery);
  d->worker = _gum_quick_worker_ref (worker);
  d->message = g_strdup (message);
  d->data = (data != NULL) ? g_bytes_ref (data) : NULL;

  return d;
}

static void
gum_worker_message_delivery_free (GumWorkerMessageDelivery * d)
{
  g_bytes_unref (d->data);
  g_free (d->message);
  _gum_quick_worker_unref (d->worker);

  g_slice_free (GumWorkerMessageDelivery, d);
}

JSValue
_gum_quick_script_rethrow_parse_error_with_decorations (GumQuickScript * self,
                                                        JSContext * ctx,
                                                        const gchar * name)
{
  JSValue exception_val, message_val, line_val;
  const char * message;
  uint32_t line;

  exception_val = JS_GetException (ctx);
  message_val = JS_GetPropertyStr (ctx, exception_val, "message");
  line_val = JS_GetPropertyStr (ctx, exception_val, "lineNumber");

  message = JS_ToCString (ctx, message_val);
  JS_ToUint32 (ctx, &line, line_val);

  _gum_quick_throw (self->ctx, "could not parse '%s' line %u: %s",
      name, line, message);

  JS_FreeCString (ctx, message);
  JS_FreeValue (ctx, line_val);
  JS_FreeValue (ctx, message_val);
  JS_FreeValue (ctx, exception_val);

  return JS_EXCEPTION;
}

void
_gum_quick_panic (JSContext * ctx,
                  const gchar * prefix)
{
  JSValue exception_val, stack_val;
  const char * message, * stack;

  exception_val = JS_GetException (ctx);

  message = JS_ToCString (ctx, exception_val);

  stack_val = JS_GetPropertyStr (ctx, exception_val, "stack");
  stack = JS_ToCString (ctx, stack_val);

  if (stack[0] != '\0')
    gum_panic ("%s: %s [stack: %s]", prefix, message, stack);
  else
    gum_panic ("%s: %s", prefix, message);
}
