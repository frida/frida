/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumscriptbackend.h"

#include "gumquickscriptbackend.h"
#include "gumv8scriptbackend.h"

#include <gum/gum-init.h>
#ifdef HAVE_SQLITE
# include <sqlite3.h>
#endif

#define GUM_SQLITE_BLOCK_ALLOC_SIZE(s) (sizeof (GumSqliteBlock) + (s))
#define GUM_SQLITE_BLOCK_TO_CLIENT(b) (((GumSqliteBlock *) (b)) + 1)
#define GUM_SQLITE_BLOCK_FROM_CLIENT(b) (((GumSqliteBlock *) (b)) - 1)

typedef struct _GumSqliteBlock GumSqliteBlock;

struct _GumSqliteBlock
{
  int size;
  int padding;
};

static void gum_script_backend_deinit_scheduler (void);

static void gum_script_backend_init_internals (void);
static void gum_script_backend_deinit_internals (void);

#ifdef HAVE_SQLITE
static int gum_sqlite_allocator_init (void * data);
static void gum_sqlite_allocator_shutdown (void * data);
static void * gum_sqlite_allocator_malloc (int size);
static void gum_sqlite_allocator_free (void * mem);
static void * gum_sqlite_allocator_realloc (void * mem, int n_bytes);
static int gum_sqlite_allocator_size (void * mem);
static int gum_sqlite_allocator_roundup (int size);
#endif

G_DEFINE_INTERFACE_WITH_CODE (GumScriptBackend, gum_script_backend,
    G_TYPE_OBJECT, gum_script_backend_init_internals ())

static void
gum_script_backend_default_init (GumScriptBackendInterface * iface)
{
}

GumScriptBackend *
gum_script_backend_obtain (void)
{
  GumScriptBackend * backend = NULL;

  backend = gum_script_backend_obtain_qjs ();
  if (backend == NULL)
    backend = gum_script_backend_obtain_v8 ();

  return backend;
}

#ifdef HAVE_QUICKJS

static void gum_script_backend_deinit_qjs (void);

GumScriptBackend *
gum_script_backend_obtain_qjs (void)
{
  static gsize gonce_value;

  if (g_once_init_enter (&gonce_value))
  {
    GumScriptBackend * backend;

    backend = g_object_new (GUM_QUICK_TYPE_SCRIPT_BACKEND, NULL);

    _gum_register_early_destructor (gum_script_backend_deinit_qjs);

    g_once_init_leave (&gonce_value, GPOINTER_TO_SIZE (backend) + 1);
  }

  return GSIZE_TO_POINTER (gonce_value - 1);
}

static void
gum_script_backend_deinit_qjs (void)
{
  g_object_unref (gum_script_backend_obtain_qjs ());
}

#else

GumScriptBackend *
gum_script_backend_obtain_qjs (void)
{
  return NULL;
}

#endif

#ifdef HAVE_V8

static void gum_script_backend_deinit_v8 (void);

GumScriptBackend *
gum_script_backend_obtain_v8 (void)
{
  static gsize gonce_value;

  if (g_once_init_enter (&gonce_value))
  {
    GumScriptBackend * backend;

    backend = g_object_new (GUM_V8_TYPE_SCRIPT_BACKEND, NULL);

    _gum_register_early_destructor (gum_script_backend_deinit_v8);

    g_once_init_leave (&gonce_value, GPOINTER_TO_SIZE (backend) + 1);
  }

  return GSIZE_TO_POINTER (gonce_value - 1);
}

static void
gum_script_backend_deinit_v8 (void)
{
  g_object_unref (gum_script_backend_obtain_v8 ());
}

#else

GumScriptBackend *
gum_script_backend_obtain_v8 (void)
{
  return NULL;
}

#endif

void
gum_script_backend_create (GumScriptBackend * self,
                           const gchar * name,
                           const gchar * source,
                           GBytes * snapshot,
                           GCancellable * cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  GUM_SCRIPT_BACKEND_GET_IFACE (self)->create (self, name, source, snapshot,
      cancellable, callback, user_data);
}

GumScript *
gum_script_backend_create_finish (GumScriptBackend * self,
                                  GAsyncResult * result,
                                  GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->create_finish (self, result,
      error);
}

GumScript *
gum_script_backend_create_sync (GumScriptBackend * self,
                                const gchar * name,
                                const gchar * source,
                                GBytes * snapshot,
                                GCancellable * cancellable,
                                GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->create_sync (self, name, source,
      snapshot, cancellable, error);
}

void
gum_script_backend_create_from_bytes (GumScriptBackend * self,
                                      GBytes * bytes,
                                      GBytes * snapshot,
                                      GCancellable * cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  GUM_SCRIPT_BACKEND_GET_IFACE (self)->create_from_bytes (self, bytes, snapshot,
      cancellable, callback, user_data);
}

GumScript *
gum_script_backend_create_from_bytes_finish (GumScriptBackend * self,
                                             GAsyncResult * result,
                                             GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->create_from_bytes_finish (self,
      result, error);
}

GumScript *
gum_script_backend_create_from_bytes_sync (GumScriptBackend * self,
                                           GBytes * bytes,
                                           GBytes * snapshot,
                                           GCancellable * cancellable,
                                           GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->create_from_bytes_sync (self,
      bytes, snapshot, cancellable, error);
}

void
gum_script_backend_compile (GumScriptBackend * self,
                            const gchar * name,
                            const gchar * source,
                            GCancellable * cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
  GUM_SCRIPT_BACKEND_GET_IFACE (self)->compile (self, name, source, cancellable,
      callback, user_data);
}

GBytes *
gum_script_backend_compile_finish (GumScriptBackend * self,
                                   GAsyncResult * result,
                                   GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->compile_finish (self, result,
      error);
}

GBytes *
gum_script_backend_compile_sync (GumScriptBackend * self,
                                 const gchar * name,
                                 const gchar * source,
                                 GCancellable * cancellable,
                                 GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->compile_sync (self, name, source,
      cancellable, error);
}

void
gum_script_backend_snapshot (GumScriptBackend * self,
                             const gchar * embed_script,
                             const gchar * warmup_script,
                             GCancellable * cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  GUM_SCRIPT_BACKEND_GET_IFACE (self)->snapshot (self, embed_script,
      warmup_script, cancellable, callback, user_data);
}

GBytes *
gum_script_backend_snapshot_finish (GumScriptBackend * self,
                                    GAsyncResult * result,
                                    GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->snapshot_finish (self, result,
      error);
}

GBytes *
gum_script_backend_snapshot_sync (GumScriptBackend * self,
                                  const gchar * embed_script,
                                  const gchar * warmup_script,
                                  GCancellable * cancellable,
                                  GError ** error)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->snapshot_sync (self, embed_script,
      warmup_script, cancellable, error);
}

void
gum_script_backend_with_lock_held (GumScriptBackend * self,
                                   GumScriptBackendLockedFunc func,
                                   gpointer user_data)
{
  GUM_SCRIPT_BACKEND_GET_IFACE (self)->with_lock_held (self, func, user_data);
}

gboolean
gum_script_backend_is_locked (GumScriptBackend * self)
{
  return GUM_SCRIPT_BACKEND_GET_IFACE (self)->is_locked (self);
}

GumScriptScheduler *
gum_script_backend_get_scheduler (void)
{
  static gsize gonce_value;

  if (g_once_init_enter (&gonce_value))
  {
    GumScriptScheduler * scheduler;

    scheduler = gum_script_scheduler_new ();

    _gum_register_early_destructor (gum_script_backend_deinit_scheduler);

    g_once_init_leave (&gonce_value, GPOINTER_TO_SIZE (scheduler) + 1);
  }

  return GSIZE_TO_POINTER (gonce_value - 1);
}

static void
gum_script_backend_deinit_scheduler (void)
{
  g_object_unref (gum_script_backend_get_scheduler ());
}

static void
gum_script_backend_init_internals (void)
{
#ifdef HAVE_SQLITE
  sqlite3_mem_methods gum_mem_methods = {
    gum_sqlite_allocator_malloc,
    gum_sqlite_allocator_free,
    gum_sqlite_allocator_realloc,
    gum_sqlite_allocator_size,
    gum_sqlite_allocator_roundup,
    gum_sqlite_allocator_init,
    gum_sqlite_allocator_shutdown,
    NULL,
  };

  sqlite3_config (SQLITE_CONFIG_MALLOC, &gum_mem_methods);
  sqlite3_config (SQLITE_CONFIG_MULTITHREAD);

  sqlite3_initialize ();
#endif

  _gum_register_early_destructor (gum_script_backend_deinit_internals);
}

static void
gum_script_backend_deinit_internals (void)
{
#ifdef HAVE_SQLITE
  sqlite3_shutdown ();
#endif
}

#ifdef HAVE_SQLITE

static int
gum_sqlite_allocator_init (void * data)
{
  return SQLITE_OK;
}

static void
gum_sqlite_allocator_shutdown (void * data)
{
}

static void *
gum_sqlite_allocator_malloc (int size)
{
  GumSqliteBlock * block;

  block = g_malloc (GUM_SQLITE_BLOCK_ALLOC_SIZE (size));
  block->size = size;

  return GUM_SQLITE_BLOCK_TO_CLIENT (block);
}

static void
gum_sqlite_allocator_free (void * mem)
{
  GumSqliteBlock * block = GUM_SQLITE_BLOCK_FROM_CLIENT (mem);

  g_free (block);
}

static void *
gum_sqlite_allocator_realloc (void * mem,
                              int n_bytes)
{
  GumSqliteBlock * block = GUM_SQLITE_BLOCK_FROM_CLIENT (mem);

  block = g_realloc (block, GUM_SQLITE_BLOCK_ALLOC_SIZE (n_bytes));
  block->size = n_bytes;

  return GUM_SQLITE_BLOCK_TO_CLIENT (block);
}

static int
gum_sqlite_allocator_size (void * mem)
{
  GumSqliteBlock * block = GUM_SQLITE_BLOCK_FROM_CLIENT (mem);

  return block->size;
}

static int
gum_sqlite_allocator_roundup (int size)
{
  return size;
}

#endif
