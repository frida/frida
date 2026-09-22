/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2013 Karl Trygve Kalleberg <karltk@boblycat.org>
 * Copyright (C) 2020 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8scriptbackend.h"

#include "gumscripttask.h"
#include "gumv8platform.h"
#include "gumv8script-priv.h"

#include <gum/guminterceptor.h>
#include <string.h>

#if defined (HAVE_DARWIN) && (defined (HAVE_ARM) || defined (HAVE_ARM64))
# define GUM_V8_PLATFORM_FLAGS \
    "--write-protect-code-memory " \
    "--wasm-write-protect-code-memory "
#else
# define GUM_V8_PLATFORM_FLAGS
#endif

#define GUM_V8_FLAGS \
    GUM_V8_PLATFORM_FLAGS \
    "--no-freeze-flags-after-init " \
    "--turbo-instruction-scheduling " \
    "--use-strict " \
    "--expose-gc " \
    "--wasm-staging " \
    "--experimental-wasm-eh " \
    "--experimental-wasm-simd " \
    "--experimental-wasm-return-call"

#define GUM_V8_SCRIPT_BACKEND_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_V8_SCRIPT_BACKEND_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

using namespace v8;
using namespace v8_inspector;

struct _GumV8ScriptBackend
{
  GObject parent;

  volatile gint scope_mutex_trap_depth;

  GMutex mutex;
  GHashTable * live_scripts;
  GumV8Platform * platform;
};

struct GumCreateScriptData
{
  gchar * name;
  gchar * source;
  GBytes * snapshot;
};

struct GumCreateScriptFromBytesData
{
  GBytes * bytes;
  GBytes * snapshot;
};

struct GumCompileScriptData
{
  gchar * name;
  gchar * source;
};

struct GumSnapshotScriptData
{
  gchar * embed_script;
  gchar * warmup_script;
};

static void gum_v8_script_backend_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_v8_script_backend_finalize (GObject * object);

static void gum_v8_script_backend_create (GumScriptBackend * backend,
    const gchar * name, const gchar * source, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static GumScript * gum_v8_script_backend_create_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GumScript * gum_v8_script_backend_create_sync (
    GumScriptBackend * backend, const gchar * name, const gchar * source,
    GBytes * snapshot, GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_create_script_task_new (GumV8ScriptBackend * backend,
    const gchar * name, const gchar * source, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_create_script_task_run (GumScriptTask * task,
    GumV8ScriptBackend * self, GumCreateScriptData * d,
    GCancellable * cancellable);
static void gum_create_script_data_free (GumCreateScriptData * d);
static void gum_v8_script_backend_create_from_bytes (GumScriptBackend * backend,
    GBytes * bytes, GBytes * snapshot, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
static GumScript * gum_v8_script_backend_create_from_bytes_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GumScript * gum_v8_script_backend_create_from_bytes_sync (
    GumScriptBackend * backend, GBytes * bytes, GBytes * snapshot,
    GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_create_script_from_bytes_task_new (
    GumV8ScriptBackend * backend, GBytes * bytes, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_create_script_from_bytes_task_run (GumScriptTask * task,
    GumV8ScriptBackend * self, GumCreateScriptFromBytesData * d,
    GCancellable * cancellable);
static void gum_create_script_from_bytes_data_free (
    GumCreateScriptFromBytesData * d);

static void gum_v8_script_backend_compile (GumScriptBackend * backend,
    const gchar * name, const gchar * source, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
static GBytes * gum_v8_script_backend_compile_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GBytes * gum_v8_script_backend_compile_sync (GumScriptBackend * backend,
    const gchar * name, const gchar * source, GCancellable * cancellable,
    GError ** error);
static GumScriptTask * gum_compile_script_task_new (
    GumV8ScriptBackend * backend, const gchar * name, const gchar * source,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static void gum_compile_script_task_run (GumScriptTask * task,
    GumV8ScriptBackend * self, GumCompileScriptData * d,
    GCancellable * cancellable);
static void gum_compile_script_data_free (GumCompileScriptData * d);
static void gum_v8_script_backend_snapshot (GumScriptBackend * backend,
    const gchar * embed_script, const gchar * warmup_script,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
static GBytes * gum_v8_script_backend_snapshot_finish (
    GumScriptBackend * backend, GAsyncResult * result, GError ** error);
static GBytes * gum_v8_script_backend_snapshot_sync (GumScriptBackend * backend,
    const gchar * embed_script, const gchar * warmup_script,
    GCancellable * cancellable, GError ** error);
static GumScriptTask * gum_snapshot_script_task_new (
    GumV8ScriptBackend * backend, const gchar * embed_script,
    const gchar * warmup_script, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
static void gum_snapshot_script_task_run (GumScriptTask * task,
    GumV8ScriptBackend * self, GumSnapshotScriptData * d,
    GCancellable * cancellable);
static StartupData gum_create_snapshot (const gchar * embed_script,
    GumV8Platform * platform, GError ** error);
static StartupData gum_warm_up_snapshot (StartupData cold,
    const gchar * warmup_script, GumV8Platform * platform, GError ** error);
static bool gum_run_code (Isolate * isolate, Local<Context> context,
    const gchar * source, const gchar * name, GError ** error);
static void gum_snapshot_script_data_free (GumSnapshotScriptData * d);
static void gum_snapshot_script_blob_free (char * blob);

static void gum_v8_script_backend_with_lock_held (GumScriptBackend * backend,
    GumScriptBackendLockedFunc func, gpointer user_data);
static gboolean gum_v8_script_backend_is_locked (GumScriptBackend * backend);

static void gum_v8_script_backend_on_context_created (GumV8ScriptBackend * self,
    Local<Context> * context, GumV8Script * script);
static void gum_v8_script_backend_on_context_destroyed (
    GumV8ScriptBackend * self, Local<Context> * context, GumV8Script * script);

G_DEFINE_TYPE_EXTENDED (GumV8ScriptBackend,
                        gum_v8_script_backend,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SCRIPT_BACKEND,
                            gum_v8_script_backend_iface_init))

static void
gum_v8_script_backend_class_init (GumV8ScriptBackendClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_v8_script_backend_finalize;
}

static void
gum_v8_script_backend_iface_init (gpointer g_iface,
                                  gpointer iface_data)
{
  auto iface = (GumScriptBackendInterface *) g_iface;

  iface->create = gum_v8_script_backend_create;
  iface->create_finish = gum_v8_script_backend_create_finish;
  iface->create_sync = gum_v8_script_backend_create_sync;
  iface->create_from_bytes = gum_v8_script_backend_create_from_bytes;
  iface->create_from_bytes_finish =
      gum_v8_script_backend_create_from_bytes_finish;
  iface->create_from_bytes_sync = gum_v8_script_backend_create_from_bytes_sync;

  iface->compile = gum_v8_script_backend_compile;
  iface->compile_finish = gum_v8_script_backend_compile_finish;
  iface->compile_sync = gum_v8_script_backend_compile_sync;
  iface->snapshot = gum_v8_script_backend_snapshot;
  iface->snapshot_finish = gum_v8_script_backend_snapshot_finish;
  iface->snapshot_sync = gum_v8_script_backend_snapshot_sync;

  iface->with_lock_held = gum_v8_script_backend_with_lock_held;
  iface->is_locked = gum_v8_script_backend_is_locked;
}

static void
gum_v8_script_backend_init (GumV8ScriptBackend * self)
{
  self->scope_mutex_trap_depth = 0;

  g_mutex_init (&self->mutex);
  self->live_scripts = g_hash_table_new (NULL, NULL);
  self->platform = NULL;
}

static void
gum_v8_script_backend_finalize (GObject * object)
{
  auto self = GUM_V8_SCRIPT_BACKEND (object);

  delete self->platform;
  g_hash_table_unref (self->live_scripts);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_v8_script_backend_parent_class)->finalize (object);
}

gpointer
gum_v8_script_backend_get_platform (GumV8ScriptBackend * self)
{
  if (self->platform == NULL)
  {
    GString * flags;

    flags = g_string_new (GUM_V8_FLAGS);

    if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_REQUIRED)
    {
      g_string_append (flags, " --jitless");
    }

    const gchar * extra_flags = g_getenv ("FRIDA_V8_EXTRA_FLAGS");
    if (extra_flags != NULL)
    {
      g_string_append_c (flags, ' ');
      g_string_append (flags, extra_flags);
    }

    V8::SetFlagsFromString (flags->str, (size_t) flags->len);

    g_string_free (flags, TRUE);

    self->platform = new GumV8Platform ();
  }

  return self->platform;
}

GumScriptScheduler *
gum_v8_script_backend_get_scheduler (GumV8ScriptBackend * self)
{
  auto platform = (GumV8Platform *) gum_v8_script_backend_get_platform (self);

  return platform->GetScheduler ();
}

static void
gum_v8_script_backend_create (GumScriptBackend * backend,
                              const gchar * name,
                              const gchar * source,
                              GBytes * snapshot,
                              GCancellable * cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_create_script_task_new (self, name, source, snapshot,
      cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_v8_script_backend_get_scheduler (self));
  g_object_unref (task);
}

static GumScript *
gum_v8_script_backend_create_finish (GumScriptBackend * backend,
                                     GAsyncResult * result,
                                     GError ** error)
{
  return GUM_SCRIPT (gum_script_task_propagate_pointer (
      GUM_SCRIPT_TASK (result), error));
}

static GumScript *
gum_v8_script_backend_create_sync (GumScriptBackend * backend,
                                   const gchar * name,
                                   const gchar * source,
                                   GBytes * snapshot,
                                   GCancellable * cancellable,
                                   GError ** error)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_create_script_task_new (self, name, source, snapshot,
      cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_v8_script_backend_get_scheduler (self));
  auto script = GUM_SCRIPT (gum_script_task_propagate_pointer (task, error));
  g_object_unref (task);

  return script;
}

static GumScriptTask *
gum_create_script_task_new (GumV8ScriptBackend * backend,
                            const gchar * name,
                            const gchar * source,
                            GBytes * snapshot,
                            GCancellable * cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
  auto d = g_slice_new (GumCreateScriptData);
  d->name = g_strdup (name);
  d->source = g_strdup (source);
  d->snapshot = (snapshot != NULL) ? g_bytes_ref (snapshot) : NULL;

  auto task = gum_script_task_new (
      (GumScriptTaskFunc) gum_create_script_task_run, backend, cancellable,
      callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_create_script_data_free);
  return task;
}

static void
gum_create_script_task_run (GumScriptTask * task,
                            GumV8ScriptBackend * self,
                            GumCreateScriptData * d,
                            GCancellable * cancellable)
{
  auto script = GUM_V8_SCRIPT (g_object_new (GUM_V8_TYPE_SCRIPT,
      "name", d->name,
      "source", d->source,
      "snapshot", d->snapshot,
      "main-context", gum_script_task_get_context (task),
      "backend", self,
      NULL));
  g_signal_connect_swapped (script, "context-created",
      G_CALLBACK (gum_v8_script_backend_on_context_created), self);
  g_signal_connect_swapped (script, "context-destroyed",
      G_CALLBACK (gum_v8_script_backend_on_context_destroyed), self);

  GError * error = NULL;
  gum_v8_script_create_context (script, &error);

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
gum_v8_script_backend_create_from_bytes (GumScriptBackend * backend,
                                         GBytes * bytes,
                                         GBytes * snapshot,
                                         GCancellable * cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_create_script_from_bytes_task_new (self, bytes, snapshot,
      cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_v8_script_backend_get_scheduler (self));
  g_object_unref (task);
}

static GumScript *
gum_v8_script_backend_create_from_bytes_finish (GumScriptBackend * backend,
                                                GAsyncResult * result,
                                                GError ** error)
{
  return GUM_SCRIPT (gum_script_task_propagate_pointer (
      GUM_SCRIPT_TASK (result), error));
}

static GumScript *
gum_v8_script_backend_create_from_bytes_sync (GumScriptBackend * backend,
                                              GBytes * bytes,
                                              GBytes * snapshot,
                                              GCancellable * cancellable,
                                              GError ** error)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_create_script_from_bytes_task_new (self, bytes, snapshot,
      cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_v8_script_backend_get_scheduler (self));
  auto script = GUM_SCRIPT (gum_script_task_propagate_pointer (task, error));
  g_object_unref (task);

  return script;
}

static GumScriptTask *
gum_create_script_from_bytes_task_new (GumV8ScriptBackend * backend,
                                       GBytes * bytes,
                                       GBytes * snapshot,
                                       GCancellable * cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
  auto d = g_slice_new (GumCreateScriptFromBytesData);
  d->bytes = g_bytes_ref (bytes);
  d->snapshot = (snapshot != NULL) ? g_bytes_ref (snapshot) : NULL;

  auto task = gum_script_task_new (
      (GumScriptTaskFunc) gum_create_script_from_bytes_task_run, backend,
      cancellable, callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_create_script_from_bytes_data_free);
  return task;
}

static void
gum_create_script_from_bytes_task_run (GumScriptTask * task,
                                       GumV8ScriptBackend * self,
                                       GumCreateScriptFromBytesData * d,
                                       GCancellable * cancellable)
{
  auto error = g_error_new (GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "script creation from bytecode is not supported by the V8 runtime");
  gum_script_task_return_error (task, error);
}

static void
gum_create_script_from_bytes_data_free (GumCreateScriptFromBytesData * d)
{
  g_bytes_unref (d->bytes);
  g_bytes_unref (d->snapshot);

  g_slice_free (GumCreateScriptFromBytesData, d);
}

static void
gum_v8_script_backend_compile (GumScriptBackend * backend,
                               const gchar * name,
                               const gchar * source,
                               GCancellable * cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_compile_script_task_new (self, name, source, cancellable,
      callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_v8_script_backend_get_scheduler (self));
  g_object_unref (task);
}

static GBytes *
gum_v8_script_backend_compile_finish (GumScriptBackend * backend,
                                      GAsyncResult * result,
                                      GError ** error)
{
  return (GBytes *) gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result),
      error);
}

static GBytes *
gum_v8_script_backend_compile_sync (GumScriptBackend * backend,
                                    const gchar * name,
                                    const gchar * source,
                                    GCancellable * cancellable,
                                    GError ** error)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task =
      gum_compile_script_task_new (self, name, source, cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_v8_script_backend_get_scheduler (self));
  auto bytes = (GBytes *) gum_script_task_propagate_pointer (task, error);
  g_object_unref (task);

  return bytes;
}

static GumScriptTask *
gum_compile_script_task_new (GumV8ScriptBackend * backend,
                             const gchar * name,
                             const gchar * source,
                             GCancellable * cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  auto d = g_slice_new (GumCompileScriptData);
  d->name = g_strdup (name);
  d->source = g_strdup (source);

  auto task = gum_script_task_new (
      (GumScriptTaskFunc) gum_compile_script_task_run, backend, cancellable,
      callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_compile_script_data_free);
  return task;
}

static void
gum_compile_script_task_run (GumScriptTask * task,
                             GumV8ScriptBackend * self,
                             GumCompileScriptData * d,
                             GCancellable * cancellable)
{
  auto error = g_error_new (GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
      "compilation to bytecode is not supported by the V8 runtime");
  gum_script_task_return_error (task, error);
}

static void
gum_compile_script_data_free (GumCompileScriptData * d)
{
  g_free (d->name);
  g_free (d->source);

  g_slice_free (GumCompileScriptData, d);
}

static void
gum_v8_script_backend_snapshot (GumScriptBackend * backend,
                                const gchar * embed_script,
                                const gchar * warmup_script,
                                GCancellable * cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_snapshot_script_task_new (self, embed_script, warmup_script,
      cancellable, callback, user_data);
  gum_script_task_run_in_js_thread (task,
      gum_v8_script_backend_get_scheduler (self));
  g_object_unref (task);
}

static GBytes *
gum_v8_script_backend_snapshot_finish (GumScriptBackend * backend,
                                       GAsyncResult * result,
                                       GError ** error)
{
  return (GBytes *) gum_script_task_propagate_pointer (GUM_SCRIPT_TASK (result),
      error);
}

static GBytes *
gum_v8_script_backend_snapshot_sync (GumScriptBackend * backend,
                                     const gchar * embed_script,
                                     const gchar * warmup_script,
                                     GCancellable * cancellable,
                                     GError ** error)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  auto task = gum_snapshot_script_task_new (self, embed_script, warmup_script,
      cancellable, NULL, NULL);
  gum_script_task_run_in_js_thread_sync (task,
      gum_v8_script_backend_get_scheduler (self));
  auto bytes = (GBytes *) gum_script_task_propagate_pointer (task, error);
  g_object_unref (task);

  return bytes;
}

static GumScriptTask *
gum_snapshot_script_task_new (GumV8ScriptBackend * backend,
                              const gchar * embed_script,
                              const gchar * warmup_script,
                              GCancellable * cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
  auto d = g_slice_new (GumSnapshotScriptData);
  d->embed_script = g_strdup (embed_script);
  d->warmup_script = g_strdup (warmup_script);

  auto task = gum_script_task_new (
      (GumScriptTaskFunc) gum_snapshot_script_task_run, backend, cancellable,
      callback, user_data);
  gum_script_task_set_task_data (task, d,
      (GDestroyNotify) gum_snapshot_script_data_free);
  return task;
}

static void
gum_snapshot_script_task_run (GumScriptTask * task,
                              GumV8ScriptBackend * self,
                              GumSnapshotScriptData * d,
                              GCancellable * cancellable)
{
  auto platform = (GumV8Platform *) gum_v8_script_backend_get_platform (self);

  GError * error = NULL;
  StartupData blob = gum_create_snapshot (d->embed_script, platform, &error);

  if (error == NULL && d->warmup_script != NULL)
  {
    StartupData cold = blob;
    blob = gum_warm_up_snapshot (cold, d->warmup_script, platform, &error);
    delete[] cold.data;
  }

  if (error == NULL)
  {
    gum_script_task_return_pointer (task,
        g_bytes_new_with_free_func (blob.data, blob.raw_size,
          (GDestroyNotify) gum_snapshot_script_blob_free,
          (gpointer) blob.data),
        (GDestroyNotify) g_bytes_unref);
  }
  else
  {
    gum_script_task_return_error (task, error);
  }
}

static StartupData
gum_create_snapshot (const gchar * embed_script,
                     GumV8Platform * platform,
                     GError ** error)
{
  SnapshotCreator creator;

  StartupData blob = {};
  auto isolate = creator.GetIsolate ();
  {
    Isolate::Scope isolate_scope (isolate);
    Locker locker (isolate);

    bool success = false;
    {
      HandleScope handle_scope (isolate);
      auto context = Context::New (isolate);

      if (gum_run_code (isolate, context, embed_script, "embedded", error))
      {
        creator.SetDefaultContext (context);
        success = true;
      }
    }

    if (success)
      blob = creator.CreateBlob (SnapshotCreator::FunctionCodeHandling::kKeep);
  }

  platform->ForgetIsolate (isolate);

  return blob;
}

static StartupData
gum_warm_up_snapshot (StartupData cold,
                      const gchar * warmup_script,
                      GumV8Platform * platform,
                      GError ** error)
{
  SnapshotCreator creator (nullptr, &cold);

  StartupData blob = {};
  auto isolate = creator.GetIsolate ();
  {
    Isolate::Scope isolate_scope (isolate);
    Locker locker (isolate);

    bool success = false;
    {
      HandleScope handle_scope (isolate);
      auto context = Context::New (isolate);

      success = gum_run_code (isolate, context, warmup_script, "warmup", error);
    }

    if (success)
    {
      {
        HandleScope handle_scope (isolate);
        isolate->ContextDisposedNotification (false);
        auto context = Context::New (isolate);
        creator.SetDefaultContext (context);
      }

      blob = creator.CreateBlob (SnapshotCreator::FunctionCodeHandling::kKeep);
    }
  }

  platform->ForgetIsolate (isolate);

  return blob;
}

static bool
gum_run_code (Isolate * isolate,
              Local<Context> context,
              const gchar * source,
              const gchar * name,
              GError ** error)
{
  Context::Scope context_scope (context);

  bool success = false;
  Local<Script> code;
  TryCatch trycatch (isolate);
  if (Script::Compile (context, String::NewFromUtf8 (isolate, source)
        .ToLocalChecked ()).ToLocal (&code))
  {
    success = !code->Run (context).IsEmpty ();
  }

  if (!success)
  {
    Local<Message> message = trycatch.Message ();
    Local<Value> exception = trycatch.Exception ();
    String::Utf8Value exception_str (isolate, exception);
    *error = g_error_new (
        GUM_ERROR,
        GUM_ERROR_FAILED,
        "%s script line %d: %s",
        name,
        message->GetLineNumber (context).FromMaybe (-1),
        *exception_str);
  }

  return success;
}

static void
gum_snapshot_script_data_free (GumSnapshotScriptData * d)
{
  g_free (d->embed_script);
  g_free (d->warmup_script);

  g_slice_free (GumSnapshotScriptData, d);
}

static void
gum_snapshot_script_blob_free (char * blob)
{
  delete[] blob;
}

static void
gum_v8_script_backend_with_lock_held (GumScriptBackend * backend,
                                      GumScriptBackendLockedFunc func,
                                      gpointer user_data)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  if (g_atomic_int_get (&self->scope_mutex_trap_depth) != 0)
  {
    func (user_data);
    return;
  }

  GUM_V8_SCRIPT_BACKEND_LOCK (self);

  gint n = g_hash_table_size (self->live_scripts);
  auto lockers = g_newa (Locker, n);

  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->live_scripts);

  GumV8Script * script;
  for (gint i = 0;
      g_hash_table_iter_next (&iter, (gpointer *) &script, NULL);
      i++)
  {
    new (&lockers[i]) Locker (script->isolate);
  }

  GUM_V8_SCRIPT_BACKEND_UNLOCK (self);

  func (user_data);

  for (gint i = n - 1; i != -1; i--)
    lockers[i].~Locker ();
}

static gboolean
gum_v8_script_backend_is_locked (GumScriptBackend * backend)
{
  auto self = GUM_V8_SCRIPT_BACKEND (backend);

  if (g_atomic_int_get (&self->scope_mutex_trap_depth) != 0)
    return FALSE;

  GUM_V8_SCRIPT_BACKEND_LOCK (self);

  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->live_scripts);

  gboolean is_locked = FALSE;
  GumV8Script * script;
  while (g_hash_table_iter_next (&iter, (gpointer *) &script, NULL))
  {
    auto isolate = script->isolate;

    if (Locker::IsLocked (isolate))
      continue;

    if (Locker::IsLockedByAnyThread (isolate))
    {
      is_locked = TRUE;
      break;
    }
  }

  GUM_V8_SCRIPT_BACKEND_UNLOCK (self);

  return is_locked;
}

gboolean
gum_v8_script_backend_is_scope_mutex_trapped (GumV8ScriptBackend * self)
{
  return g_atomic_int_get (&self->scope_mutex_trap_depth) != 0;
}

void
gum_v8_script_backend_mark_scope_mutex_trapped (GumV8ScriptBackend * self)
{
  g_atomic_int_inc (&self->scope_mutex_trap_depth);
}

void
gum_v8_script_backend_unmark_scope_mutex_trapped (GumV8ScriptBackend * self)
{
  g_atomic_int_add (&self->scope_mutex_trap_depth, -1);
}

static void
gum_v8_script_backend_on_context_created (GumV8ScriptBackend * self,
                                          Local<Context> * context,
                                          GumV8Script * script)
{
  GUM_V8_SCRIPT_BACKEND_LOCK (self);
  g_hash_table_add (self->live_scripts, script);
  GUM_V8_SCRIPT_BACKEND_UNLOCK (self);
}

static void
gum_v8_script_backend_on_context_destroyed (GumV8ScriptBackend * self,
                                            Local<Context> * context,
                                            GumV8Script * script)
{
  GUM_V8_SCRIPT_BACKEND_LOCK (self);
  g_hash_table_remove (self->live_scripts, script);
  GUM_V8_SCRIPT_BACKEND_UNLOCK (self);
}
