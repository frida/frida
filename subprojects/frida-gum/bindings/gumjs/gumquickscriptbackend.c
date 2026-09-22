/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickscriptbackend.h"

#include "gumquickscript.h"
#include "gumquickscript-runtime.h"
#include "gumquickscriptbackend-priv.h"
#include "gumscripttask.h"
#include "gumsourcemap.h"

#include <stdlib.h>
#include <string.h>

#if G_BYTE_ORDER == G_BIG_ENDIAN
# define GUM_QUICKJS_BYTECODE_MAGIC 0x42
#else
# define GUM_QUICKJS_BYTECODE_MAGIC 0x02
#endif

typedef struct _GumCompileProgramOperation GumCompileProgramOperation;
typedef struct _GumCreateScriptData GumCreateScriptData;
typedef struct _GumCreateScriptFromBytesData GumCreateScriptFromBytesData;
typedef struct _GumCompileScriptData GumCompileScriptData;
typedef struct _GumSnapshotScriptData GumSnapshotScriptData;
typedef struct _GumLoadRuntimeData GumLoadRuntimeData;

struct _GumQuickScriptBackend
{
  GObject parent;

  GMutex mutex;
  GRecMutex scope_mutex;
  volatile gint scope_mutex_trap_depth;

  GumScriptScheduler * scheduler;
};

struct _GumCompileProgramOperation
{
  GumESProgram * program;
  GError * error;
};

struct _GumCreateScriptData
{
  gchar * name;
  gchar * source;
  GBytes * snapshot;
};

struct _GumCreateScriptFromBytesData
{
  GBytes * bytes;
  GBytes * snapshot;
};

struct _GumCompileScriptData
{
  gchar * name;
  gchar * source;
};

struct _GumSnapshotScriptData
{
  gchar * embed_script;
  gchar * warmup_script;
};

struct _GumLoadRuntimeData
{
  GumRuntimeLoadedFunc func;
  gpointer data;
  GDestroyNotify data_destroy;
};

enum _GumLoadRuntimeMagic
{
  GUM_LOAD_RUNTIME_MAGIC_ON_SUCCESS,
  GUM_LOAD_RUNTIME_MAGIC_ON_FAILURE,
};

static void gum_quick_script_backend_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_quick_script_backend_dispose (GObject * object);
static void gum_quick_script_backend_finalize (GObject * object);

static char * gum_normalize_module_name_during_compilation (JSContext * ctx,
    const char * base_name, const char * name, void * opaque);
static char * gum_normalize_module_name_during_runtime (JSContext * ctx,
    const char * base_name, const char * name, void * opaque);
static JSModuleDef * gum_load_module_during_compilation (JSContext * ctx,
    const char * module_name, void * opaque);
static JSModuleDef * gum_load_module_during_runtime (JSContext * ctx,
    const char * module_name, void * opaque);
static JSValue gum_compile_module (JSContext * ctx, const GumESAsset * asset);

static void gum_quick_script_backend_create (GumScriptBackend * backend,
    const gchar * name, const gchar * source, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static GumScript * gum_quick_script_backend_create_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GumScript * gum_quick_script_backend_create_sync (
    GumScriptBackend * backend, const gchar * name, const gchar * source,
    GBytes * snapshot, GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_create_script_task_new (
    GumQuickScriptBackend * backend, const gchar * name, const gchar * source,
    GBytes * snapshot, GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_create_script_task_run (GumScriptTask * task,
    GumQuickScriptBackend * self, GumCreateScriptData * d,
    GCancellable * cancellable);
static void gum_create_script_data_free (GumCreateScriptData * d);
static void gum_quick_script_backend_create_from_bytes (
    GumScriptBackend * backend, GBytes * bytes, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static GumScript * gum_quick_script_backend_create_from_bytes_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GumScript * gum_quick_script_backend_create_from_bytes_sync (
    GumScriptBackend * backend, GBytes * bytes, GBytes * snapshot,
    GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_create_script_from_bytes_task_new (
    GumQuickScriptBackend * backend, GBytes * bytes, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_create_script_from_bytes_task_run (GumScriptTask * task,
    GumQuickScriptBackend * self, GumCreateScriptFromBytesData * d,
    GCancellable * cancellable);
static void gum_create_script_from_bytes_data_free (
    GumCreateScriptFromBytesData * d);

static void gum_quick_script_backend_compile (GumScriptBackend * backend,
    const gchar * name, const gchar * source, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
static GBytes * gum_quick_script_backend_compile_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GBytes * gum_quick_script_backend_compile_sync (
    GumScriptBackend * backend, const gchar * name, const gchar * source,
    GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_compile_script_task_new (
    GumQuickScriptBackend * backend, const gchar * name, const gchar * source,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_compile_script_task_run (GumScriptTask * task,
    GumQuickScriptBackend * self, GumCompileScriptData * d,
    GCancellable * cancellable);
static void gum_compile_script_data_free (GumCompileScriptData * d);
static void gum_quick_script_backend_snapshot (GumScriptBackend * backend,
    const gchar * embed_script, const gchar * warmup_script,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static GBytes * gum_quick_script_backend_snapshot_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GBytes * gum_quick_script_backend_snapshot_sync (
    GumScriptBackend * backend, const gchar * embed_script,
    const gchar * warmup_script, GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_snapshot_script_task_new (
    GumQuickScriptBackend * backend, const gchar * embed_script,
    const gchar * warmup_script, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
static void gum_snapshot_script_task_run (GumScriptTask * task,
    GumQuickScriptBackend * self, GumSnapshotScriptData * d,
    GCancellable * cancellable);
static void gum_snapshot_script_data_free (GumSnapshotScriptData * d);

static void gum_quick_script_backend_with_lock_held (GumScriptBackend * backend,
    GumScriptBackendLockedFunc func, gpointer user_data);
static gboolean gum_quick_script_backend_is_locked (GumScriptBackend * backend);

static GumESProgram * gum_es_program_new (void);
static JSValue gum_es_program_on_runtime_loaded (JSContext * ctx,
    JSValueConst this_val, int argc, JSValueConst * argv, int magic,
    JSValue * func_data);
static char * gum_es_program_normalize_module_name (GumESProgram * self,
    JSContext * ctx, const char * base_name, const char * name);

static GError * gum_capture_parse_error (JSContext * ctx,
    const gchar * filename);

#ifndef HAVE_ASAN
static void * gum_quick_malloc (JSMallocState * state, size_t size);
static void gum_quick_free (JSMallocState * state, void * ptr);
static void * gum_quick_realloc (JSMallocState * state, void * ptr,
    size_t size);
static size_t gum_quick_malloc_usable_size (const void * ptr);
#endif

G_DEFINE_TYPE_EXTENDED (GumQuickScriptBackend,
                        gum_quick_script_backend,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SCRIPT_BACKEND,
                            gum_quick_script_backend_iface_init))

static void
gum_quick_script_backend_class_init (GumQuickScriptBackendClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_quick_script_backend_dispose;
  object_class->finalize = gum_quick_script_backend_finalize;
}

static void
gum_quick_script_backend_iface_init (gpointer g_iface,
                                     gpointer iface_data)
{
  GumScriptBackendInterface * iface = g_iface;

  iface->create = gum_quick_script_backend_create;
  iface->create_finish = gum_quick_script_backend_create_finish;
  iface->create_sync = gum_quick_script_backend_create_sync;
  iface->create_from_bytes = gum_quick_script_backend_create_from_bytes;
  iface->create_from_bytes_finish =
      gum_quick_script_backend_create_from_bytes_finish;
  iface->create_from_bytes_sync =
      gum_quick_script_backend_create_from_bytes_sync;

  iface->compile = gum_quick_script_backend_compile;
  iface->compile_finish = gum_quick_script_backend_compile_finish;
  iface->compile_sync = gum_quick_script_backend_compile_sync;
  iface->snapshot = gum_quick_script_backend_snapshot;
  iface->snapshot_finish = gum_quick_script_backend_snapshot_finish;
  iface->snapshot_sync = gum_quick_script_backend_snapshot_sync;

  iface->with_lock_held = gum_quick_script_backend_with_lock_held;
  iface->is_locked = gum_quick_script_backend_is_locked;
}

static void
gum_quick_script_backend_init (GumQuickScriptBackend * self)
{
  g_mutex_init (&self->mutex);
  g_rec_mutex_init (&self->scope_mutex);
  self->scope_mutex_trap_depth = 0;

  self->scheduler = g_object_ref (gum_script_backend_get_scheduler ());
}

static void
gum_quick_script_backend_dispose (GObject * object)
{
  GumQuickScriptBackend * self = GUM_QUICK_SCRIPT_BACKEND (object);

  g_clear_object (&self->scheduler);

  G_OBJECT_CLASS (gum_quick_script_backend_parent_class)->dispose (object);
}

static void
gum_quick_script_backend_finalize (GObject * object)
{
  GumQuickScriptBackend * self = GUM_QUICK_SCRIPT_BACKEND (object);

  g_mutex_clear (&self->mutex);
  g_rec_mutex_clear (&self->scope_mutex);

  G_OBJECT_CLASS (gum_quick_script_backend_parent_class)->finalize (object);
}

JSRuntime *
gum_quick_script_backend_make_runtime (GumQuickScriptBackend * self)
{
  JSRuntime * rt;

#ifndef HAVE_ASAN
  const JSMallocFunctions mf = {
    gum_quick_malloc,
    gum_quick_free,
    gum_quick_realloc,
    gum_quick_malloc_usable_size
  };

  rt = JS_NewRuntime2 (&mf, self);
#else
  rt = JS_NewRuntime ();
#endif

#ifdef HAVE_TINY_STACK
  JS_SetMaxStackSize (rt, GUM_QUICK_TINY_STACK_SIZE);
#endif

  return rt;
}

GumESProgram *
gum_quick_script_backend_compile_program (GumQuickScriptBackend * self,
                                          JSContext * ctx,
                                          const gchar * name,
                                          const gchar * source,
                                          GError ** error)
{
  GumESProgram * program;
  GumCompileProgramOperation op;
  JSRuntime * rt;
  const gchar * package_marker = "📦\n";
  const gchar * delimiter_marker = "\n✄\n";
  const gchar * alias_marker = "\n↻ ";
  GumESAsset * entrypoint = NULL;

  program = gum_es_program_new ();

  op.program = program;
  op.error = NULL;

  rt = JS_GetRuntime (ctx);

  if (g_str_has_prefix (source, package_marker))
  {
    const gchar * source_end, * header_cursor;

    JS_SetModuleLoaderFunc (rt,
        gum_normalize_module_name_during_compilation,
        gum_load_module_during_compilation,
        &op);

    source_end = source + strlen (source);
    header_cursor = source + strlen (package_marker);

    do
    {
      const gchar * asset_cursor, * header_end;
      guint i;
      JSValue val;

      entrypoint = NULL;

      asset_cursor = strstr (header_cursor, delimiter_marker);
      if (asset_cursor == NULL)
        goto malformed_package;

      header_end = asset_cursor;

      for (i = 0; header_cursor != header_end; i++)
      {
        guint64 asset_size;
        const gchar * size_end, * rest_start, * rest_end;
        gchar * asset_name, * asset_data;
        GumESAsset * asset;

        if (i != 0 && !g_str_has_prefix (asset_cursor, delimiter_marker))
          goto malformed_package;
        asset_cursor += strlen (delimiter_marker);

        asset_size = g_ascii_strtoull (header_cursor, (gchar **) &size_end, 10);
        if (asset_size == 0 || asset_size > GUM_MAX_ASSET_SIZE)
          goto malformed_package;
        if (asset_cursor + asset_size > source_end)
          goto malformed_package;

        rest_start = size_end + 1;
        rest_end = strchr (rest_start, '\n');

        asset_name = g_strndup (rest_start, rest_end - rest_start);
        if (g_hash_table_contains (program->es_assets, asset_name))
        {
          g_free (asset_name);
          goto malformed_package;
        }

        asset_data = g_strndup (asset_cursor, asset_size);

        asset = gum_es_asset_new (asset_name, asset_data, asset_size, g_free);
        g_hash_table_insert (program->es_assets, asset_name, asset);

        while (g_str_has_prefix (rest_end, alias_marker))
        {
          const gchar * alias_start, * alias_end;
          gchar * asset_alias;

          alias_start = rest_end + strlen (alias_marker);
          alias_end = strchr (alias_start, '\n');

          asset_alias = g_strndup (alias_start, alias_end - alias_start);
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

      val = gum_compile_module (ctx, entrypoint);
      if (JS_IsException (val))
        goto malformed_entrypoint;

      g_array_append_val (program->entrypoints, val);

      if (g_str_has_prefix (asset_cursor, delimiter_marker))
        header_cursor = asset_cursor + strlen (delimiter_marker);
      else
        header_cursor = NULL;
    }
    while (header_cursor != NULL);
  }
  else
  {
    JSValue val;

    program->global_filename = g_strconcat ("/", name, ".js", NULL);

    val = JS_Eval (ctx, source, strlen (source), program->global_filename,
        JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException (val))
      goto malformed_code;

    g_array_append_val (program->entrypoints, val);

    program->global_source_map = gum_source_map_try_extract_inline (source);
  }

  goto beach;

malformed_package:
  {
    op.error = g_error_new (
        GUM_ERROR,
        GUM_ERROR_INVALID_DATA,
        "Malformed package");

    goto propagate_error;
  }
malformed_entrypoint:
  {
    if (op.error == NULL)
      op.error = gum_capture_parse_error (ctx, entrypoint->name);

    goto propagate_error;
  }
malformed_code:
  {
    JSValue exception_val, line_val;
    const char * message;
    uint32_t line;

    exception_val = JS_GetException (ctx);

    message = JS_ToCString (ctx, exception_val);

    line_val = JS_GetPropertyStr (ctx, exception_val, "lineNumber");
    JS_ToUint32 (ctx, &line, line_val);

    op.error = g_error_new (
        GUM_ERROR,
        GUM_ERROR_FAILED,
        "Script(line %u): %s",
        line,
        message);

    JS_FreeValue (ctx, line_val);
    JS_FreeCString (ctx, message);
    JS_FreeValue (ctx, exception_val);

    goto propagate_error;
  }
propagate_error:
  {
    g_propagate_error (error, op.error);

    gum_es_program_free (program, ctx);
    program = NULL;

    goto beach;
  }
beach:
  {
    if (program != NULL)
    {
      JS_SetModuleLoaderFunc (rt,
          gum_normalize_module_name_during_runtime,
          gum_load_module_during_runtime,
          program);
    }
    else
    {
      JS_SetModuleLoaderFunc (rt, NULL, NULL, NULL);
    }

    return program;
  }
}

static char *
gum_normalize_module_name_during_compilation (JSContext * ctx,
                                              const char * base_name,
                                              const char * name,
                                              void * opaque)
{
  GumCompileProgramOperation * op = opaque;

  return gum_es_program_normalize_module_name (op->program, ctx, base_name,
      name);
}

static char *
gum_normalize_module_name_during_runtime (JSContext * ctx,
                                          const char * base_name,
                                          const char * name,
                                          void * opaque)
{
  GumESProgram * program = opaque;

  return gum_es_program_normalize_module_name (program, ctx, base_name, name);
}

static JSModuleDef *
gum_load_module_during_compilation (JSContext * ctx,
                                    const char * module_name,
                                    void * opaque)
{
  GumCompileProgramOperation * op = opaque;
  GumESAsset * asset;
  JSValue val;

  asset = g_hash_table_lookup (op->program->es_assets, module_name);
  if (asset == NULL)
    goto not_found;

  val = gum_compile_module (ctx, asset);
  if (JS_IsException (val))
    goto malformed_module;

  JS_FreeValue (ctx, val);

  return JS_VALUE_GET_PTR (val);

not_found:
  {
    if (op->error == NULL)
    {
      op->error = g_error_new (
          GUM_ERROR,
          GUM_ERROR_FAILED,
          "Could not load module '%s'",
          module_name);
    }

    return NULL;
  }
malformed_module:
  {
    if (op->error == NULL)
      op->error = gum_capture_parse_error (ctx, asset->name);

    return NULL;
  }
}

static JSModuleDef *
gum_load_module_during_runtime (JSContext * ctx,
                                const char * module_name,
                                void * opaque)
{
  GumESProgram * program = opaque;
  GumESAsset * asset;
  JSValue val;

  asset = g_hash_table_lookup (program->es_assets, module_name);
  if (asset == NULL)
    goto not_found;

  val = gum_compile_module (ctx, asset);
  if (JS_IsException (val))
    return NULL;

  JS_FreeValue (ctx, val);

  return JS_VALUE_GET_PTR (val);

not_found:
  {
    gchar * message;
    JSValue error;

    message = g_strdup_printf ("could not load module '%s'", module_name);

    error = JS_NewError (ctx);
    JS_SetPropertyStr (ctx, error, "message", JS_NewString (ctx, message));

    g_free (message);

    JS_Throw (ctx, error);

    return NULL;
  }
}

static JSValue
gum_compile_module (JSContext * ctx,
                    const GumESAsset * asset)
{
  JSValue val;
  JSModuleDef * mod;
  JSValue meta;
  gchar * url;

  if (((guint8 *) asset->data)[0] == GUM_QUICKJS_BYTECODE_MAGIC)
  {
    val = JS_ReadObject (ctx, asset->data, asset->data_size,
        JS_READ_OBJ_BYTECODE);
  }
  else
  {
    val = JS_Eval (ctx, asset->data, asset->data_size, asset->name,
        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY);
  }
  if (JS_IsException (val))
    return JS_EXCEPTION;

  mod = JS_VALUE_GET_PTR (val);

  meta = JS_GetImportMeta (ctx, mod);

  url = g_strconcat ("file://", asset->name, NULL);
  JS_DefinePropertyValueStr (ctx, meta, "url", JS_NewString (ctx, url),
      JS_PROP_C_W_E);
  g_free (url);

  JS_FreeValue (ctx, meta);

  return val;
}

GumESProgram *
gum_quick_script_backend_read_program (GumQuickScriptBackend * self,
                                       JSContext * ctx,
                                       GBytes * bytecode,
                                       GError ** error)
{
  GumESProgram * program;
  JSValue val;
  gconstpointer code;
  gsize size;

  program = gum_es_program_new ();

  code = g_bytes_get_data (bytecode, &size);

  val = JS_ReadObject (ctx, code, size, JS_READ_OBJ_BYTECODE);

  if (JS_IsException (val))
    goto malformed_code;

  g_array_append_val (program->entrypoints, val);

  JS_SetModuleLoaderFunc (JS_GetRuntime (ctx),
      gum_normalize_module_name_during_runtime,
      gum_load_module_during_runtime,
      program);

  return program;

malformed_code:
  {
    JSValue exception_val;
    const char * message_str;

    gum_es_program_free (program, ctx);

    exception_val = JS_GetException (ctx);
    message_str = JS_ToCString (ctx, exception_val);

    g_set_error_literal (error, GUM_ERROR, GUM_ERROR_FAILED, message_str);

    JS_FreeCString (ctx, message_str);
    JS_FreeValue (ctx, exception_val);

    return NULL;
  }
}

GRecMutex *
gum_quick_script_backend_get_scope_mutex (GumQuickScriptBackend * self)
{
  return &self->scope_mutex;
}

GumScriptScheduler *
gum_quick_script_backend_get_scheduler (GumQuickScriptBackend * self)
{
  return self->scheduler;
}

static void
gum_quick_script_backend_create (GumScriptBackend * backend,
                                 const gchar * name,
                                 const gchar * source,
                                 GBytes * snapshot,
                                 GCancellable * cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
  GumQuickScriptBackend * self;
  GumScriptTask * task;

  self = GUM_QUICK_SCRIPT_BACKEND (backend);

  task = gum_create_script_task_new (self, name, source, snapshot, cancellable,
      callback, user_data);
  gum_script_task_run_in_js_thread (task, self->scheduler);
  g_object_unref (task);
}

static GumScript *
gum_quick_script_backend_create_finish (GumScriptBackend * backend,
                                        GAsyncResult * result,
                                        GError ** error)
{
  return gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), error);
}

static GumScript *
gum_quick_script_backend_create_sync (GumScriptBackend * backend,
                                      const gchar * name,
                                      const gchar * source,
                                      GBytes * snapshot,
                                      GCancellable * cancellable,
                                      GError ** error)
{
  GumScript * script;
  GumQuickScriptBackend * self;
  GumScriptTask * task;

  self = GUM_QUICK_SCRIPT_BACKEND (backend);

  task = gum_create_script_task_new (self, name, source, snapshot, cancellable,
      NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task, self->scheduler);
  script = gum_script_task_propagate_pointer (task, error);
  g_object_unref (task);

  return script;
}

static GumScriptTask *
gum_create_script_task_new (GumQuickScriptBackend * backend,
                            const gchar * name,
                            const gchar * source,
                            GBytes * snapshot,
                            GCancellable * cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
  GumScriptTask * task;
  GumCreateScriptData * d;

  d = g_slice_new (GumCreateScriptData);
  d->name = g_strdup (name);
  d->source = g_strdup (source);
  d->snapshot = (snapshot != NULL) ? g_bytes_ref (snapshot) : NULL;

  task = gum_script_task_new ((GumScriptTaskFunc) gum_create_script_task_run,
      backend, cancellable, callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_create_script_data_free);

  return task;
}

static void
gum_create_script_task_run (GumScriptTask * task,
                            GumQuickScriptBackend * self,
                            GumCreateScriptData * d,
                            GCancellable * cancellable)
{
  GumQuickScript * script;
  GError * error = NULL;

  if (d->snapshot != NULL)
  {
    gum_script_task_return_error (task,
        g_error_new (GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
          "snapshots are not supported by the QuickJS runtime"));
    return;
  }

  script = g_object_new (GUM_QUICK_TYPE_SCRIPT,
      "name", d->name,
      "source", d->source,
      "main-context", gum_script_task_get_context (task),
      "backend", self,
      NULL);

  gum_quick_script_create_context (script, &error);

  if (error == NULL)
  {
    gum_script_task_return_pointer (task, script, g_object_unref);
  }
  else
  {
    gum_script_task_return_error (task, error);
    g_object_unref (script);
  }
}

static void
gum_create_script_data_free (GumCreateScriptData * d)
{
  g_free (d->name);
  g_free (d->source);
  g_bytes_unref (d->snapshot);

  g_slice_free (GumCreateScriptData, d);
}

static void
gum_quick_script_backend_create_from_bytes (GumScriptBackend * backend,
                                            GBytes * bytes,
                                            GBytes * snapshot,
                                            GCancellable * cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
  GumQuickScriptBackend * self = GUM_QUICK_SCRIPT_BACKEND (backend);
  GumScriptTask * task;

  task = gum_create_script_from_bytes_task_new (self, bytes, snapshot,
      cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task, self->scheduler);
  g_object_unref (task);
}

static GumScript *
gum_quick_script_backend_create_from_bytes_finish (GumScriptBackend * backend,
                                                   GAsyncResult * result,
                                                   GError ** error)
{
  return gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), error);
}

static GumScript *
gum_quick_script_backend_create_from_bytes_sync (GumScriptBackend * backend,
                                                 GBytes * bytes,
                                                 GBytes * snapshot,
                                                 GCancellable * cancellable,
                                                 GError ** error)
{
  GumQuickScriptBackend * self = GUM_QUICK_SCRIPT_BACKEND (backend);
  GumScript * script;
  GumScriptTask * task;

  task = gum_create_script_from_bytes_task_new (self, bytes, snapshot,
      cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task, self->scheduler);
  script = GUM_SCRIPT (gum_script_task_propagate_pointer (task, error));
  g_object_unref (task);

  return script;
}

static GumScriptTask *
gum_create_script_from_bytes_task_new (GumQuickScriptBackend * backend,
                                       GBytes * bytes,
                                       GBytes * snapshot,
                                       GCancellable * cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
  GumScriptTask * task;
  GumCreateScriptFromBytesData * d;

  d = g_slice_new (GumCreateScriptFromBytesData);
  d->bytes = g_bytes_ref (bytes);
  d->snapshot = (snapshot != NULL) ? g_bytes_ref (snapshot) : NULL;

  task = gum_script_task_new (
      (GumScriptTaskFunc) gum_create_script_from_bytes_task_run, backend,
      cancellable, callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_create_script_from_bytes_data_free);

  return task;
}

static void
gum_create_script_from_bytes_task_run (GumScriptTask * task,
                                       GumQuickScriptBackend * self,
                                       GumCreateScriptFromBytesData * d,
                                       GCancellable * cancellable)
{
  GumQuickScript * script;
  GError * error = NULL;

  if (d->snapshot != NULL)
  {
    gum_script_task_return_error (task,
        g_error_new (GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
          "snapshots are not supported by the QuickJS runtime"));
    return;
  }

  script = g_object_new (GUM_QUICK_TYPE_SCRIPT,
      "bytecode", d->bytes,
      "main-context", gum_script_task_get_context (task),
      "backend", self,
      NULL);

  gum_quick_script_create_context (script, &error);

  if (error == NULL)
  {
    gum_script_task_return_pointer (task, script, g_object_unref);
  }
  else
  {
    gum_script_task_return_error (task, error);
    g_object_unref (script);
  }
}

static void
gum_create_script_from_bytes_data_free (GumCreateScriptFromBytesData * d)
{
  g_bytes_unref (d->bytes);
  g_bytes_unref (d->snapshot);

  g_slice_free (GumCreateScriptFromBytesData, d);
}

static void
gum_quick_script_backend_compile (GumScriptBackend * backend,
                                  const gchar * name,
                                  const gchar * source,
                                  GCancellable * cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  GumQuickScriptBackend * self;
  GumScriptTask * task;

  self = GUM_QUICK_SCRIPT_BACKEND (backend);

  task = gum_compile_script_task_new (self, name, source, cancellable, callback,
      user_data);
  gum_script_task_run_in_js_thread (task, self->scheduler);
  g_object_unref (task);
}

static GBytes *
gum_quick_script_backend_compile_finish (GumScriptBackend * backend,
                                         GAsyncResult * result,
                                         GError ** error)
{
  return gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), error);
}

static GBytes *
gum_quick_script_backend_compile_sync (GumScriptBackend * backend,
                                       const gchar * name,
                                       const gchar * source,
                                       GCancellable * cancellable,
                                       GError ** error)
{
  GBytes * bytes;
  GumQuickScriptBackend * self;
  GumScriptTask * task;

  self = GUM_QUICK_SCRIPT_BACKEND (backend);

  task = gum_compile_script_task_new (self, name, source, cancellable, NULL,
      NULL);
  gum_script_task_run_in_js_thread_sync (task, self->scheduler);
  bytes = gum_script_task_propagate_pointer (task, error);
  g_object_unref (task);

  return bytes;
}

static GumScriptTask *
gum_compile_script_task_new (GumQuickScriptBackend * backend,
                             const gchar * name,
                             const gchar * source,
                             GCancellable * cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  GumScriptTask * task;
  GumCompileScriptData * d;

  d = g_slice_new (GumCompileScriptData);
  d->name = g_strdup (name);
  d->source = g_strdup (source);

  task = gum_script_task_new ((GumScriptTaskFunc) gum_compile_script_task_run,
      backend, cancellable, callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_compile_script_data_free);

  return task;
}

static void
gum_compile_script_task_run (GumScriptTask * task,
                             GumQuickScriptBackend * self,
                             GumCompileScriptData * d,
                             GCancellable * cancellable)
{
  JSRuntime * rt;
  JSContext * ctx;
  GumESProgram * program;
  GError * error = NULL;

  rt = gum_quick_script_backend_make_runtime (self);
  ctx = JS_NewContext (rt);

  program = gum_quick_script_backend_compile_program (self, ctx, d->name,
      d->source, &error);

  if (error == NULL)
  {
    JSValue val;
    uint8_t * code;
    size_t size;
    GBytes * bytes;
    GDestroyNotify free_impl;

    /* TODO: Add support for compiling ESM-flavored scripts to bytecode. */
    val = g_array_index (program->entrypoints, JSValue, 0);

#ifndef HAVE_ASAN
    free_impl = gum_free;
#else
    free_impl = free;
#endif

    code = JS_WriteObject (ctx, &size, val, JS_WRITE_OBJ_BYTECODE);

    bytes = g_bytes_new_with_free_func (code, size, free_impl, code);

    gum_script_task_return_pointer (task, bytes,
        (GDestroyNotify) g_bytes_unref);

    gum_es_program_free (program, ctx);
  }
  else
  {
    gum_script_task_return_error (task, error);
  }

  JS_FreeContext (ctx);
  JS_FreeRuntime (rt);
}

static void
gum_compile_script_data_free (GumCompileScriptData * d)
{
  g_free (d->name);
  g_free (d->source);

  g_slice_free (GumCompileScriptData, d);
}

static void
gum_quick_script_backend_snapshot (GumScriptBackend * backend,
                                   const gchar * embed_script,
                                   const gchar * warmup_script,
                                   GCancellable * cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
  GumQuickScriptBackend * self;
  GumScriptTask * task;

  self = GUM_QUICK_SCRIPT_BACKEND (backend);

  task = gum_snapshot_script_task_new (self, embed_script, warmup_script,
      cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task, self->scheduler);
  g_object_unref (task);
}

static GBytes *
gum_quick_script_backend_snapshot_finish (GumScriptBackend * backend,
                                          GAsyncResult * result,
                                          GError ** error)
{
  return gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result), error);
}

static GBytes *
gum_quick_script_backend_snapshot_sync (GumScriptBackend * backend,
                                        const gchar * embed_script,
                                        const gchar * warmup_script,
                                        GCancellable * cancellable,
                                        GError ** error)
{
  GBytes * bytes;
  GumQuickScriptBackend * self;
  GumScriptTask * task;

  self = GUM_QUICK_SCRIPT_BACKEND (backend);

  task = gum_snapshot_script_task_new (self, embed_script, warmup_script,
      cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task, self->scheduler);
  bytes = gum_script_task_propagate_pointer (task, error);
  g_object_unref (task);

  return bytes;
}

static GumScriptTask *
gum_snapshot_script_task_new (GumQuickScriptBackend * backend,
                              const gchar * embed_script,
                              const gchar * warmup_script,
                              GCancellable * cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
  GumScriptTask * task;
  GumSnapshotScriptData * d;

  d = g_slice_new (GumSnapshotScriptData);
  d->embed_script = g_strdup (embed_script);
  d->warmup_script = g_strdup (warmup_script);

  task = gum_script_task_new ((GumScriptTaskFunc) gum_snapshot_script_task_run,
      backend, cancellable, callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_snapshot_script_data_free);

  return task;
}

static void
gum_snapshot_script_task_run (GumScriptTask * task,
                              GumQuickScriptBackend * self,
                              GumSnapshotScriptData * d,
                              GCancellable * cancellable)
{
  gum_script_task_return_error (task,
      g_error_new (GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
        "not supported by the QuickJS runtime"));
}

static void
gum_snapshot_script_data_free (GumSnapshotScriptData * d)
{
  g_free (d->embed_script);
  g_free (d->warmup_script);

  g_slice_free (GumSnapshotScriptData, d);
}

static void
gum_quick_script_backend_with_lock_held (GumScriptBackend * backend,
                                         GumScriptBackendLockedFunc func,
                                         gpointer user_data)
{
  GumQuickScriptBackend * self = GUM_QUICK_SCRIPT_BACKEND (backend);

  if (g_atomic_int_get (&self->scope_mutex_trap_depth) != 0)
  {
    func (user_data);
    return;
  }

  g_rec_mutex_lock (&self->scope_mutex);
  func (user_data);
  g_rec_mutex_unlock (&self->scope_mutex);
}

static gboolean
gum_quick_script_backend_is_locked (GumScriptBackend * backend)
{
  GumQuickScriptBackend * self = GUM_QUICK_SCRIPT_BACKEND (backend);

  if (g_atomic_int_get (&self->scope_mutex_trap_depth) != 0)
    return FALSE;

  if (!g_rec_mutex_trylock (&self->scope_mutex))
    return TRUE;

  g_rec_mutex_unlock (&self->scope_mutex);

  return FALSE;
}

gboolean
gum_quick_script_backend_is_scope_mutex_trapped (GumQuickScriptBackend * self)
{
  return g_atomic_int_get (&self->scope_mutex_trap_depth) != 0;
}

void
gum_quick_script_backend_mark_scope_mutex_trapped (GumQuickScriptBackend * self)
{
  g_atomic_int_inc (&self->scope_mutex_trap_depth);
}

void
gum_quick_script_backend_unmark_scope_mutex_trapped (
    GumQuickScriptBackend * self)
{
  g_atomic_int_add (&self->scope_mutex_trap_depth, -1);
}

static GumESProgram *
gum_es_program_new (void)
{
  GumESProgram * program;
  const GumQuickRuntimeModule * cur;

  program = g_slice_new0 (GumESProgram);
  program->entrypoints = g_array_new (FALSE, FALSE, sizeof (JSValue));
  program->es_assets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) gum_es_asset_unref);

  for (cur = gumjs_runtime_modules; cur->name != NULL; cur++)
  {
    gchar * name;
    GumESAsset * asset;

    name = g_strconcat ("/frida/runtime/", cur->name, NULL);
    asset = gum_es_asset_new (name, cur->bytecode, cur->bytecode_size, NULL);

    if (program->runtime == NULL)
      program->runtime = asset;

    g_hash_table_insert (program->es_assets, name, asset);
  }

  return program;
}

void
gum_es_program_free (GumESProgram * program,
                     JSContext * ctx)
{
  GArray * entrypoints;
  guint i;

  if (program == NULL)
    return;

  g_free (program->global_source_map);
  g_free (program->global_filename);

  g_clear_pointer (&program->es_assets, g_hash_table_unref);

  entrypoints = program->entrypoints;
  for (i = 0; i != entrypoints->len; i++)
    JS_FreeValue (ctx, g_array_index (entrypoints, JSValue, i));
  g_array_free (entrypoints, TRUE);

  g_slice_free (GumESProgram, program);
}

gboolean
gum_es_program_is_esm (GumESProgram * self)
{
  return self->global_filename == NULL;
}

void
gum_es_program_load_runtime (GumESProgram * self,
                             JSContext * ctx,
                             GumRuntimeLoadedFunc func,
                             gpointer data,
                             GDestroyNotify data_destroy)
{
  JSValue entrypoint, promise, data_obj, on_success, on_failure, then, catch, v;
  G_GNUC_UNUSED int res;
  GumLoadRuntimeData * d;

  entrypoint = gum_compile_module (ctx, self->runtime);
  g_assert (!JS_IsException (entrypoint));

  res = JS_ResolveModule (ctx, entrypoint);
  g_assert (res == 0);

  promise = JS_EvalFunction (ctx, entrypoint);
  g_assert (!JS_IsException (promise));

  d = g_slice_new (GumLoadRuntimeData);
  d->func = func;
  d->data = data;
  d->data_destroy = data_destroy;

  data_obj = JS_NewObject (ctx);
  JS_SetOpaque (data_obj, d);

  on_success = JS_NewCFunctionData (ctx, gum_es_program_on_runtime_loaded, 1,
      GUM_LOAD_RUNTIME_MAGIC_ON_SUCCESS, 1, &data_obj);
  on_failure = JS_NewCFunctionData (ctx, gum_es_program_on_runtime_loaded, 1,
      GUM_LOAD_RUNTIME_MAGIC_ON_FAILURE, 1, &data_obj);

  then = JS_GetPropertyStr (ctx, promise, "then");
  catch = JS_GetPropertyStr (ctx, promise, "catch");

  v = JS_Call (ctx, then, promise, 1, &on_success);
  JS_FreeValue (ctx, v);

  v = JS_Call (ctx, catch, promise, 1, &on_failure);
  JS_FreeValue (ctx, v);

  JS_FreeValue (ctx, catch);
  JS_FreeValue (ctx, then);
  JS_FreeValue (ctx, on_failure);
  JS_FreeValue (ctx, on_success);
  JS_FreeValue (ctx, data_obj);
  JS_FreeValue (ctx, promise);
}

static JSValue
gum_es_program_on_runtime_loaded (JSContext * ctx,
                                  JSValueConst this_val,
                                  int argc,
                                  JSValueConst * argv,
                                  int magic,
                                  JSValue * func_data)
{
  GumLoadRuntimeData * d;
  JSClassID class_id;
  JSValueConst error;

  d = JS_GetAnyOpaque (func_data[0], &class_id);

  error = (magic == GUM_LOAD_RUNTIME_MAGIC_ON_SUCCESS)
      ? JS_NULL
      : argv[0];
  d->func (error, d->data);

  if (d->data_destroy != NULL)
    d->data_destroy (d->data);
  g_slice_free (GumLoadRuntimeData, d);

  return JS_UNDEFINED;
}

JSValue
gum_es_program_compile_worker (GumESProgram * self,
                               JSContext * ctx,
                               const GumESAsset * asset)
{
  JS_SetModuleLoaderFunc (JS_GetRuntime (ctx),
      gum_normalize_module_name_during_runtime,
      gum_load_module_during_runtime,
      self);

  return gum_compile_module (ctx, asset);
}

static char *
gum_es_program_normalize_module_name (GumESProgram * self,
                                      JSContext * ctx,
                                      const char * base_name,
                                      const char * name)
{
  char * result;
  const char * base_dir_end;
  guint base_dir_length;
  const char * cursor;

  if (name[0] != '.')
  {
    GumESAsset * asset;

    asset = g_hash_table_lookup (self->es_assets, name);
    if (asset != NULL)
      return js_strdup (ctx, asset->name);

    return js_strdup (ctx, name);
  }

  /* The following mimics QuickJS' default implementation: */

  base_dir_end = strrchr (base_name, '/');
  if (base_dir_end != NULL)
    base_dir_length = base_dir_end - base_name;
  else
    base_dir_length = 0;

  result = js_malloc (ctx, base_dir_length + 1 + strlen (name) + 1);
  memcpy (result, base_name, base_dir_length);
  result[base_dir_length] = '\0';

  cursor = name;
  while (TRUE)
  {
    if (g_str_has_prefix (cursor, "./"))
    {
      cursor += 2;
    }
    else if (g_str_has_prefix (cursor, "../"))
    {
      char * new_end;

      if (result[0] == '\0')
        break;

      new_end = strrchr (result, '/');
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

GumESAsset *
gum_es_asset_new (const gchar * name,
                  gconstpointer data,
                  gsize data_size,
                  GDestroyNotify data_destroy)
{
  GumESAsset * asset;

  asset = g_slice_new (GumESAsset);

  asset->ref_count = 1;

  asset->name = name;

  asset->data = data;
  asset->data_size = data_size;
  asset->data_destroy = data_destroy;

  return asset;
}

GumESAsset *
gum_es_asset_ref (GumESAsset * asset)
{
  g_atomic_int_inc (&asset->ref_count);

  return asset;
}

void
gum_es_asset_unref (GumESAsset * asset)
{
  if (asset == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&asset->ref_count))
    return;

  if (asset->data_destroy != NULL)
    asset->data_destroy ((gpointer) asset->data);

  g_slice_free (GumESAsset, asset);
}

static GError *
gum_capture_parse_error (JSContext * ctx,
                         const gchar * filename)
{
  GError * error;
  JSValue exception_val, message_val, line_val;
  const char * message;
  uint32_t line;

  exception_val = JS_GetException (ctx);
  message_val = JS_GetPropertyStr (ctx, exception_val, "message");
  line_val = JS_GetPropertyStr (ctx, exception_val, "lineNumber");

  message = JS_ToCString (ctx, message_val);
  JS_ToUint32 (ctx, &line, line_val);

  error = g_error_new (
      GUM_ERROR,
      GUM_ERROR_FAILED,
      "Could not parse '%s' line %u: %s",
      filename,
      line,
      message);

  JS_FreeCString (ctx, message);
  JS_FreeValue (ctx, line_val);
  JS_FreeValue (ctx, message_val);

  JS_Throw (ctx, exception_val);

  return error;
}

#ifndef HAVE_ASAN

static void *
gum_quick_malloc (JSMallocState * state,
                  size_t size)
{
  return gum_malloc (size);
}

static void
gum_quick_free (JSMallocState * state,
                void * ptr)
{
  gum_free (ptr);
}

static void *
gum_quick_realloc (JSMallocState * state,
                   void * ptr,
                   size_t size)
{
  return gum_realloc (ptr, size);
}

static size_t
gum_quick_malloc_usable_size (const void * ptr)
{
  return gum_malloc_usable_size (ptr);
}

#endif
