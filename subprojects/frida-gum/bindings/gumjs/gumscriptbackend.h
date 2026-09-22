/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SCRIPT_BACKEND_H__
#define __GUM_SCRIPT_BACKEND_H__

#include <gumjs/gumscript.h>
#include <gumjs/gumscriptapi.h>
#include <gumjs/gumscriptscheduler.h>

G_BEGIN_DECLS

#define GUM_MAX_ASSET_SIZE (100 * 1024 * 1024)

#define GUM_TYPE_SCRIPT_BACKEND (gum_script_backend_get_type ())
G_DECLARE_INTERFACE (GumScriptBackend, gum_script_backend, GUM, SCRIPT_BACKEND,
    GObject)

typedef void (* GumScriptBackendLockedFunc) (gpointer user_data);

struct _GumScriptBackendInterface
{
  GTypeInterface parent;

  void (* create) (GumScriptBackend * self, const gchar * name,
      const gchar * source, GBytes * snapshot, GCancellable * cancellable,
      GAsyncReadyCallback callback, gpointer user_data);
  GumScript * (* create_finish) (GumScriptBackend * self, GAsyncResult * result,
      GError ** error);
  GumScript * (* create_sync) (GumScriptBackend * self, const gchar * name,
      const gchar * source, GBytes * snapshot, GCancellable * cancellable,
      GError ** error);
  void (* create_from_bytes) (GumScriptBackend * self, GBytes * bytes,
      GBytes * snapshot, GCancellable * cancellable,
      GAsyncReadyCallback callback, gpointer user_data);
  GumScript * (* create_from_bytes_finish) (GumScriptBackend * self,
      GAsyncResult * result, GError ** error);
  GumScript * (* create_from_bytes_sync) (GumScriptBackend * self,
      GBytes * bytes, GBytes * snapshot, GCancellable * cancellable,
      GError ** error);

  void (* compile) (GumScriptBackend * self, const gchar * name,
      const gchar * source, GCancellable * cancellable,
      GAsyncReadyCallback callback, gpointer user_data);
  GBytes * (* compile_finish) (GumScriptBackend * self, GAsyncResult * result,
      GError ** error);
  GBytes * (* compile_sync) (GumScriptBackend * self, const gchar * name,
      const gchar * source, GCancellable * cancellable, GError ** error);
  void (* snapshot) (GumScriptBackend * self, const gchar * embed_script,
      const gchar * warmup_script, GCancellable * cancellable,
      GAsyncReadyCallback callback, gpointer user_data);
  GBytes * (* snapshot_finish) (GumScriptBackend * self, GAsyncResult * result,
      GError ** error);
  GBytes * (* snapshot_sync) (GumScriptBackend * self,
      const gchar * embed_script, const gchar * warmup_script,
      GCancellable * cancellable, GError ** error);

  void (* with_lock_held) (GumScriptBackend * self,
      GumScriptBackendLockedFunc func, gpointer user_data);
  gboolean (* is_locked) (GumScriptBackend * self);
};

GUM_API GumScriptBackend * gum_script_backend_obtain (void);
GUM_API GumScriptBackend * gum_script_backend_obtain_qjs (void);
GUM_API GumScriptBackend * gum_script_backend_obtain_v8 (void);

GUM_API void gum_script_backend_create (GumScriptBackend * self,
    const gchar * name, const gchar * source, GBytes * snapshot,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
GUM_API GumScript * gum_script_backend_create_finish (GumScriptBackend * self,
    GAsyncResult * result, GError ** error);
GUM_API GumScript * gum_script_backend_create_sync (GumScriptBackend * self,
    const gchar * name, const gchar * source, GBytes * snapshot,
    GCancellable * cancellable, GError ** error);
GUM_API void gum_script_backend_create_from_bytes (GumScriptBackend * self,
    GBytes * bytes, GBytes * snapshot, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
GUM_API GumScript * gum_script_backend_create_from_bytes_finish (
    GumScriptBackend * self, GAsyncResult * result, GError ** error);
GUM_API GumScript * gum_script_backend_create_from_bytes_sync (
    GumScriptBackend * self, GBytes * bytes, GBytes * snapshot,
    GCancellable * cancellable, GError ** error);

GUM_API void gum_script_backend_compile (GumScriptBackend * self,
    const gchar * name, const gchar * source, GCancellable * cancellable,
    GAsyncReadyCallback callback, gpointer user_data);
GUM_API GBytes * gum_script_backend_compile_finish (GumScriptBackend * self,
    GAsyncResult * result, GError ** error);
GUM_API GBytes * gum_script_backend_compile_sync (GumScriptBackend * self,
    const gchar * name, const gchar * source, GCancellable * cancellable,
    GError ** error);
GUM_API void gum_script_backend_snapshot (GumScriptBackend * self,
    const gchar * embed_script, const gchar * warmup_script,
    GCancellable * cancellable, GAsyncReadyCallback callback,
    gpointer user_data);
GUM_API GBytes * gum_script_backend_snapshot_finish (GumScriptBackend * self,
    GAsyncResult * result, GError ** error);
GUM_API GBytes * gum_script_backend_snapshot_sync (GumScriptBackend * self,
    const gchar * embed_script, const gchar * warmup_script,
    GCancellable * cancellable, GError ** error);

GUM_API void gum_script_backend_with_lock_held (GumScriptBackend * self,
    GumScriptBackendLockedFunc func, gpointer user_data);
GUM_API gboolean gum_script_backend_is_locked (GumScriptBackend * self);

GUM_API GumScriptScheduler * gum_script_backend_get_scheduler (void);

G_END_DECLS

#endif
