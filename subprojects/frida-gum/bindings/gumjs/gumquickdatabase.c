/*
 * Copyright (C) 2020-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickdatabase.h"

#include "gumquickinterceptor.h"
#include "gumquickmacros.h"

typedef struct _GumDatabase GumDatabase;
typedef guint GumStorage;

struct _GumDatabase
{
  sqlite3 * handle;
  gchar * path;
  GumStorage storage;
  GumQuickDatabase * parent;
};

enum _GumStorage
{
  GUM_STORAGE_FILESYSTEM,
  GUM_STORAGE_MEMORY
};

GUMJS_DECLARE_FUNCTION (gumjs_database_open)
GUMJS_DECLARE_FUNCTION (gumjs_database_open_inline)

static gboolean gum_database_get_unchecked (JSContext * ctx, JSValueConst val,
    GumQuickCore * core, GumDatabase ** database);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_database_construct)
GUMJS_DECLARE_FINALIZER (gumjs_database_finalize)
GUMJS_DECLARE_FUNCTION (gumjs_database_close)
GUMJS_DECLARE_FUNCTION (gumjs_database_exec)
GUMJS_DECLARE_FUNCTION (gumjs_database_prepare)
GUMJS_DECLARE_FUNCTION (gumjs_database_dump)

static JSValue gum_database_new (JSContext * ctx, sqlite3 * handle,
    const gchar * path, GumStorage storage, GumQuickDatabase * parent);
static void gum_database_free (GumDatabase * self);
static void gum_database_close (GumDatabase * self);

GUMJS_DECLARE_FINALIZER (gumjs_statement_finalize)
GUMJS_DECLARE_GETTER (gumjs_statement_get_column_names)
GUMJS_DECLARE_GETTER (gumjs_statement_get_column_types)
GUMJS_DECLARE_GETTER (gumjs_statement_get_declared_types)
GUMJS_DECLARE_GETTER (gumjs_statement_get_params_count)
GUMJS_DECLARE_FUNCTION (gumjs_statement_bind_integer)
GUMJS_DECLARE_FUNCTION (gumjs_statement_bind_float)
GUMJS_DECLARE_FUNCTION (gumjs_statement_bind_text)
GUMJS_DECLARE_FUNCTION (gumjs_statement_bind_blob)
GUMJS_DECLARE_FUNCTION (gumjs_statement_bind_null)
GUMJS_DECLARE_FUNCTION (gumjs_statement_step)
GUMJS_DECLARE_FUNCTION (gumjs_statement_reset)

static JSValue gum_statement_new (JSContext * ctx, sqlite3_stmt * handle,
    GumQuickDatabase * parent);

static JSValue gum_parse_row (JSContext * ctx, sqlite3_stmt * statement);
static JSValue gum_parse_column (JSContext * ctx, sqlite3_stmt * statement,
    guint index);
static const char * gum_column_type_to_string (int type);

static const JSClassDef gumjs_database_def =
{
  .class_name = "SqliteDatabase",
  .finalizer = gumjs_database_finalize,
};

static const JSCFunctionListEntry gumjs_database_module_entries[] =
{
  JS_CFUNC_DEF ("_open", 0, gumjs_database_open),
  JS_CFUNC_DEF ("openInline", 0, gumjs_database_open_inline),
};

static const JSCFunctionListEntry gumjs_database_entries[] =
{
  JS_CFUNC_DEF ("close", 0, gumjs_database_close),
  JS_CFUNC_DEF ("exec", 0, gumjs_database_exec),
  JS_CFUNC_DEF ("prepare", 0, gumjs_database_prepare),
  JS_CFUNC_DEF ("dump", 0, gumjs_database_dump),
};

static const JSClassDef gumjs_statement_def =
{
  .class_name = "SqliteStatement",
  .finalizer = gumjs_statement_finalize,
};

static const JSCFunctionListEntry gumjs_statement_entries[] =
{
  JS_CGETSET_DEF ("columnNames", gumjs_statement_get_column_names, NULL),
  JS_CGETSET_DEF ("columnTypes", gumjs_statement_get_column_types, NULL),
  JS_CGETSET_DEF ("declaredTypes", gumjs_statement_get_declared_types, NULL),
  JS_CGETSET_DEF ("paramsCount", gumjs_statement_get_params_count, NULL),
  JS_CFUNC_DEF ("bindInteger", 0, gumjs_statement_bind_integer),
  JS_CFUNC_DEF ("bindFloat", 0, gumjs_statement_bind_float),
  JS_CFUNC_DEF ("bindText", 0, gumjs_statement_bind_text),
  JS_CFUNC_DEF ("bindBlob", 0, gumjs_statement_bind_blob),
  JS_CFUNC_DEF ("bindNull", 0, gumjs_statement_bind_null),
  JS_CFUNC_DEF ("step", 0, gumjs_statement_step),
  JS_CFUNC_DEF ("reset", 0, gumjs_statement_reset),
};

void
_gum_quick_database_init (GumQuickDatabase * self,
                          JSValue ns,
                          GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "database", self);

  _gum_quick_create_class (ctx, &gumjs_database_def, core,
      &self->database_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_database_construct,
      gumjs_database_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, ctor, gumjs_database_module_entries,
      G_N_ELEMENTS (gumjs_database_module_entries));
  JS_SetPropertyFunctionList (ctx, proto, gumjs_database_entries,
      G_N_ELEMENTS (gumjs_database_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_database_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_statement_def, core,
      &self->statement_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_statement_entries,
      G_N_ELEMENTS (gumjs_statement_entries));

  self->memory_vfs = gum_memory_vfs_new ();
  sqlite3_vfs_register (&self->memory_vfs->vfs, FALSE);
}

void
_gum_quick_database_dispose (GumQuickDatabase * self)
{
}

void
_gum_quick_database_finalize (GumQuickDatabase * self)
{
  sqlite3_vfs_unregister (&self->memory_vfs->vfs);
  gum_memory_vfs_free (self->memory_vfs);
}

static GumQuickDatabase *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "database");
}

GUMJS_DEFINE_FUNCTION (gumjs_database_open)
{
  GumQuickDatabase * self;
  const gchar * path;
  gint flags;
  sqlite3 * handle;
  gint status;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "si", &path, &flags))
    return JS_EXCEPTION;

  handle = NULL;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_open_v2 (path, &handle, flags, NULL);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    goto invalid_database;

  return gum_database_new (ctx, handle, path, GUM_STORAGE_FILESYSTEM, self);

invalid_database:
  {
    GUMJS_INTERCEPTOR_IGNORE ();

    sqlite3_close_v2 (handle);

    GUMJS_INTERCEPTOR_UNIGNORE ();

    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_database_open_inline)
{
  GumQuickDatabase * self;
  const gchar * encoded_contents;
  gpointer contents;
  gsize size;
  gboolean valid;
  const gchar * path;
  sqlite3 * handle;
  gint status;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "s", &encoded_contents))
    return JS_EXCEPTION;

  valid =
      gum_memory_vfs_contents_from_string (encoded_contents, &contents, &size);
  if (!valid)
    goto invalid_data;

  path = gum_memory_vfs_add_file (self->memory_vfs, contents, size);

  handle = NULL;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_open_v2 (path, &handle, SQLITE_OPEN_READWRITE,
      self->memory_vfs->name);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    goto invalid_database;

  return gum_database_new (ctx, handle, path, GUM_STORAGE_MEMORY, self);

invalid_data:
  {
    return _gum_quick_throw_literal (ctx, "invalid data");
  }
invalid_database:
  {
    GUMJS_INTERCEPTOR_IGNORE ();

    sqlite3_close_v2 (handle);

    GUMJS_INTERCEPTOR_UNIGNORE ();

    gum_memory_vfs_remove_file (self->memory_vfs, path);

    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));
  }
}

static gboolean
gum_database_get (JSContext * ctx,
                  JSValueConst val,
                  GumQuickCore * core,
                  GumDatabase ** database)
{
  GumDatabase * db;

  if (!gum_database_get_unchecked (ctx, val, core, &db))
    return FALSE;

  if (db->handle == NULL)
  {
    _gum_quick_throw_literal (ctx, "database is closed");
    return FALSE;
  }

  *database = db;
  return TRUE;
}

static gboolean
gum_database_get_unchecked (JSContext * ctx,
                            JSValueConst val,
                            GumQuickCore * core,
                            GumDatabase ** database)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->database_class, core,
      (gpointer *) database);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_database_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FINALIZER (gumjs_database_finalize)
{
  GumDatabase * db;

  db = JS_GetOpaque (val, gumjs_get_parent_module (core)->database_class);
  if (db == NULL)
    return;

  GUMJS_INTERCEPTOR_IGNORE ();

  gum_database_free (db);

  GUMJS_INTERCEPTOR_UNIGNORE ();
}

GUMJS_DEFINE_FUNCTION (gumjs_database_close)
{
  GumDatabase * self;

  if (!gum_database_get_unchecked (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  gum_database_close (self);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_database_exec)
{
  GumDatabase * self;
  const gchar * sql;
  gchar * error_message;
  gint status;

  if (!gum_database_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "s", &sql))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_exec (self->handle, sql, NULL, NULL, &error_message);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    goto error;

  return JS_UNDEFINED;

error:
  {
    _gum_quick_throw_literal (ctx, error_message);

    GUMJS_INTERCEPTOR_IGNORE ();

    sqlite3_free (error_message);

    GUMJS_INTERCEPTOR_UNIGNORE ();

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_database_prepare)
{
  GumDatabase * self;
  const gchar * sql;
  sqlite3_stmt * statement;
  gint status;

  if (!gum_database_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "s", &sql))
    return JS_EXCEPTION;

  statement = NULL;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_prepare_v2 (self->handle, sql, -1, &statement, NULL);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (statement == NULL)
    goto invalid_sql;

  return gum_statement_new (ctx, statement, gumjs_get_parent_module (core));

invalid_sql:
  {
    if (status == SQLITE_OK)
      _gum_quick_throw_literal (ctx, "invalid statement");
    else
      _gum_quick_throw_literal (ctx, sqlite3_errstr (status));
    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_database_dump)
{
  JSValue result;
  GumDatabase * self;
  gpointer data, malloc_data;
  gsize size;
  GError * error;
  gchar * data_str;

  if (!gum_database_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (self->storage == GUM_STORAGE_MEMORY)
  {
    gum_memory_vfs_get_file_contents (self->parent->memory_vfs, self->path,
        &data, &size);

    malloc_data = NULL;
  }
  else
  {
    error = NULL;
    if (!g_file_get_contents (self->path, (gchar **) &data, &size, &error))
      return _gum_quick_throw_error (ctx, &error);

    malloc_data = data;
  }

  data_str = gum_memory_vfs_contents_to_string (data, size);

  result = JS_NewString (ctx, data_str);

  g_free (data_str);
  g_free (malloc_data);

  return result;
}

static JSValue
gum_database_new (JSContext * ctx,
                  sqlite3 * handle,
                  const gchar * path,
                  GumStorage storage,
                  GumQuickDatabase * parent)
{
  JSValue wrapper;
  GumDatabase * db;

  wrapper = JS_NewObjectClass (ctx, parent->database_class);

  db = g_slice_new (GumDatabase);
  db->handle = handle;
  db->path = g_strdup (path);
  db->storage = storage;
  db->parent = parent;

  JS_SetOpaque (wrapper, db);

  return wrapper;
}

static void
gum_database_free (GumDatabase * self)
{
  gum_database_close (self);

  g_free (self->path);

  g_slice_free (GumDatabase, self);
}

static void
gum_database_close (GumDatabase * self)
{
  if (self->handle == NULL)
    return;

  sqlite3_close_v2 (self->handle);
  self->handle = NULL;

  if (self->storage == GUM_STORAGE_MEMORY)
    gum_memory_vfs_remove_file (self->parent->memory_vfs, self->path);
}

static gboolean
gum_statement_get (JSContext * ctx,
                   JSValueConst val,
                   GumQuickCore * core,
                   sqlite3_stmt ** statement)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->statement_class, core,
      (gpointer *) statement);
}

GUMJS_DEFINE_FINALIZER (gumjs_statement_finalize)
{
  sqlite3_stmt * s;

  s = JS_GetOpaque (val, gumjs_get_parent_module (core)->statement_class);
  if (s == NULL)
    return;

  GUMJS_INTERCEPTOR_IGNORE ();

  sqlite3_finalize (s);

  GUMJS_INTERCEPTOR_UNIGNORE ();
}

GUMJS_DEFINE_GETTER (gumjs_statement_get_column_names)
{
  sqlite3_stmt * self;
  JSValue result;
  gint num_columns, i;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  num_columns = sqlite3_column_count (self);

  result = JS_NewArray (ctx);

  for (i = 0; i != num_columns; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, result, i,
        JS_NewString (ctx, sqlite3_column_name (self, i)),
        JS_PROP_C_W_E);
  }

  return result;
}

GUMJS_DEFINE_GETTER (gumjs_statement_get_column_types)
{
  sqlite3_stmt * self;
  JSValue result;
  gint num_columns, i;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  num_columns = sqlite3_column_count (self);

  result = JS_NewArray (ctx);

  for (i = 0; i != num_columns; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, result, i,
        JS_NewString (ctx,
            gum_column_type_to_string (sqlite3_column_type (self, i))),
        JS_PROP_C_W_E);
  }

  return result;
}

GUMJS_DEFINE_GETTER (gumjs_statement_get_declared_types)
{
  sqlite3_stmt * self;
  JSValue result;
  gint num_columns, i;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  num_columns = sqlite3_column_count (self);

  result = JS_NewArray (ctx);

  for (i = 0; i != num_columns; i++)
  {
    const char * decl_type = sqlite3_column_decltype (self, i);
    JS_DefinePropertyValueUint32 (ctx, result, i,
        (decl_type != NULL) ? JS_NewString (ctx, decl_type) : JS_NULL,
        JS_PROP_C_W_E);
  }

  return result;
}

GUMJS_DEFINE_GETTER (gumjs_statement_get_params_count)
{
  sqlite3_stmt * self;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewInt32 (ctx, sqlite3_bind_parameter_count (self));
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_bind_integer)
{
  sqlite3_stmt * self;
  gint index, value, status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "ii", &index, &value))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_bind_int64 (self, index, value);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_bind_float)
{
  sqlite3_stmt * self;
  gint index;
  gdouble value;
  gint status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "in", &index, &value))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_bind_double (self, index, value);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_bind_text)
{
  sqlite3_stmt * self;
  gint index;
  const gchar * value;
  gint status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "is", &index, &value))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_bind_text (self, index, value, -1, SQLITE_TRANSIENT);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_bind_blob)
{
  sqlite3_stmt * self;
  gint index;
  GBytes * bytes;
  gpointer data;
  gsize size;
  gint status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "iB~", &index, &bytes))
    return JS_EXCEPTION;

  data = g_bytes_unref_to_data (_gum_quick_args_steal_bytes (args, bytes),
      &size);

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_bind_blob64 (self, index, data, size, g_free);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_bind_null)
{
  sqlite3_stmt * self;
  gint index, status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "i", &index))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_bind_null (self, index);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_step)
{
  sqlite3_stmt * self;
  gint status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_step (self);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  switch (status)
  {
    case SQLITE_ROW:
      return gum_parse_row (ctx, self);
    case SQLITE_DONE:
      return JS_NULL;
    default:
      return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_statement_reset)
{
  sqlite3_stmt * self;
  gint status;

  if (!gum_statement_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  GUMJS_INTERCEPTOR_IGNORE ();

  status = sqlite3_reset (self);

  GUMJS_INTERCEPTOR_UNIGNORE ();

  if (status != SQLITE_OK)
    return _gum_quick_throw_literal (ctx, sqlite3_errstr (status));

  return JS_UNDEFINED;
}

static JSValue
gum_statement_new (JSContext * ctx,
                   sqlite3_stmt * handle,
                   GumQuickDatabase * parent)
{
  JSValue wrapper = JS_NewObjectClass (ctx, parent->statement_class);

  JS_SetOpaque (wrapper, handle);

  return wrapper;
}

static JSValue
gum_parse_row (JSContext * ctx,
               sqlite3_stmt * statement)
{
  JSValue row;
  gint num_columns, i;

  row = JS_NewArray (ctx);

  num_columns = sqlite3_column_count (statement);

  for (i = 0; i != num_columns; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, row, i,
        gum_parse_column (ctx, statement, i),
        JS_PROP_C_W_E);
  }

  return row;
}

static JSValue
gum_parse_column (JSContext * ctx,
                  sqlite3_stmt * statement,
                  guint index)
{
  switch (sqlite3_column_type (statement, index))
  {
    case SQLITE_INTEGER:
      return JS_NewInt64 (ctx, sqlite3_column_int64 (statement, index));
    case SQLITE_FLOAT:
      return JS_NewFloat64 (ctx, sqlite3_column_double (statement, index));
    case SQLITE_TEXT:
      return JS_NewString (ctx,
          (const char *) sqlite3_column_text (statement, index));
    case SQLITE_BLOB:
      return JS_NewArrayBufferCopy (ctx, sqlite3_column_blob (statement, index),
          sqlite3_column_bytes (statement, index));
    case SQLITE_NULL:
      return JS_NULL;
    default:
      g_assert_not_reached ();
  }
}

static const char *
gum_column_type_to_string (int type)
{
  switch (type)
  {
    case SQLITE_INTEGER: return "integer";
    case SQLITE_FLOAT:   return "float";
    case SQLITE_TEXT:    return "text";
    case SQLITE_BLOB:    return "blob";
    case SQLITE_NULL:    return "null";
    default:
      g_assert_not_reached ();
  }
}
