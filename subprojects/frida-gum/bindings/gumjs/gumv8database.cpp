/*
 * Copyright (C) 2017-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8database.h"

#include "gumv8macros.h"
#include "gumv8scope.h"

#define GUMJS_MODULE_NAME Database

using namespace v8;

struct GumDatabase
{
  Global<Object> * wrapper;
  sqlite3 * handle;
  gchar * path;
  gboolean is_virtual;
  GumV8Database * module;
};

struct GumStatement
{
  Global<Object> * wrapper;
  sqlite3_stmt * handle;
  GumV8Database * module;
};

GUMJS_DECLARE_FUNCTION (gumjs_database_open)
GUMJS_DECLARE_FUNCTION (gumjs_database_open_inline)

GUMJS_DECLARE_FUNCTION (gumjs_database_close)
GUMJS_DECLARE_FUNCTION (gumjs_database_exec)
GUMJS_DECLARE_FUNCTION (gumjs_database_prepare)
GUMJS_DECLARE_FUNCTION (gumjs_database_dump)

static Local<Object> gum_database_new (sqlite3 * handle, const gchar * path,
    gboolean is_virtual, GumV8Database * module);
static void gum_database_free (GumDatabase * self);
static void gum_database_close (GumDatabase * self);
static gboolean gum_database_check_open (GumDatabase * self, Isolate * isolate);
static void gum_database_on_weak_notify (
    const WeakCallbackInfo<GumDatabase> & info);

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

static Local<Object> gum_statement_new (sqlite3_stmt * handle,
    GumV8Database * module);
static void gum_statement_free (GumStatement * self);
static void gum_statement_on_weak_notify (
    const WeakCallbackInfo<GumStatement> & info);

static Local<Array> gum_parse_row (Isolate * isolate, sqlite3_stmt * statement);
static Local<Value> gum_parse_column (Isolate * isolate,
    sqlite3_stmt * statement, guint index);
static const char * gum_column_type_to_string (int type);

static const GumV8Function gumjs_database_module_functions[] =
{
  { "_open", gumjs_database_open },
  { "openInline", gumjs_database_open_inline },

  { NULL, NULL }
};

static const GumV8Function gumjs_database_functions[] =
{
  { "close", gumjs_database_close },
  { "exec", gumjs_database_exec },
  { "prepare", gumjs_database_prepare },
  { "dump", gumjs_database_dump },

  { NULL, NULL }
};

static const GumV8Property gumjs_statement_values[] =
{
  { "columnNames", gumjs_statement_get_column_names, NULL },
  { "columnTypes", gumjs_statement_get_column_types, NULL },
  { "declaredTypes", gumjs_statement_get_declared_types, NULL },
  { "paramsCount", gumjs_statement_get_params_count, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_statement_functions[] =
{
  { "bindInteger", gumjs_statement_bind_integer },
  { "bindFloat", gumjs_statement_bind_float },
  { "bindText", gumjs_statement_bind_text },
  { "bindBlob", gumjs_statement_bind_blob },
  { "bindNull", gumjs_statement_bind_null },
  { "step", gumjs_statement_step },
  { "reset", gumjs_statement_reset },

  { NULL, NULL }
};

void
_gum_v8_database_init (GumV8Database * self,
                       GumV8Core * core,
                       Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto database = _gum_v8_create_class ("SqliteDatabase", nullptr, scope,
      module, isolate);
  _gum_v8_class_add_static (database, gumjs_database_module_functions, module,
      isolate);
  _gum_v8_class_add (database, gumjs_database_functions, module, isolate);
  self->database = new Global<FunctionTemplate> (isolate, database);

  auto statement = _gum_v8_create_class ("SqliteStatement", nullptr, scope,
      module, isolate);
  _gum_v8_class_add (statement, gumjs_statement_values, module, isolate);
  _gum_v8_class_add (statement, gumjs_statement_functions, module, isolate);
  self->statement = new Global<FunctionTemplate> (isolate, statement);

  self->memory_vfs = gum_memory_vfs_new ();
  sqlite3_vfs_register (&self->memory_vfs->vfs, FALSE);
}

void
_gum_v8_database_realize (GumV8Database * self)
{
  self->databases = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_database_free);
  self->statements = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_statement_free);
}

void
_gum_v8_database_dispose (GumV8Database * self)
{
  g_hash_table_unref (self->statements);
  self->statements = NULL;

  g_hash_table_unref (self->databases);
  self->databases = NULL;

  delete self->statement;
  self->statement = nullptr;

  delete self->database;
  self->database = nullptr;
}

void
_gum_v8_database_finalize (GumV8Database * self)
{
  sqlite3_vfs_unregister (&self->memory_vfs->vfs);
  gum_memory_vfs_free (self->memory_vfs);
}

GUMJS_DEFINE_FUNCTION (gumjs_database_open)
{
  gchar * path;
  gint flags;
  sqlite3 * handle;
  gint status;
  Local<Object> object;

  if (!_gum_v8_args_parse (args, "si", &path, &flags))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  handle = NULL;
  status = sqlite3_open_v2 (path, &handle, flags, NULL);
  if (status != SQLITE_OK)
    goto invalid_database;

  object = gum_database_new (handle, path, FALSE, module);

  info.GetReturnValue ().Set (object);

  g_free (path);

  return;

invalid_database:
  {
    sqlite3_close_v2 (handle);
    g_free (path);
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
    return;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_database_open_inline)
{
  gchar * encoded_contents;
  gpointer contents;
  gsize size;
  gboolean valid;
  const gchar * path;
  sqlite3 * handle;
  gint status;
  Local<Object> object;

  if (!_gum_v8_args_parse (args, "s", &encoded_contents))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  valid =
      gum_memory_vfs_contents_from_string (encoded_contents, &contents, &size);
  g_free (encoded_contents);
  if (!valid)
    goto invalid_data;

  path = gum_memory_vfs_add_file (module->memory_vfs, contents, size);

  handle = NULL;
  status = sqlite3_open_v2 (path, &handle, SQLITE_OPEN_READWRITE,
      module->memory_vfs->name);
  if (status != SQLITE_OK)
    goto invalid_database;

  object = gum_database_new (handle, path, TRUE, module);

  info.GetReturnValue ().Set (object);

  return;

invalid_data:
  {
    _gum_v8_throw (isolate, "invalid data");
    return;
  }
invalid_database:
  {
    sqlite3_close_v2 (handle);
    gum_memory_vfs_remove_file (module->memory_vfs, path);
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
    return;
  }
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_database_close, GumDatabase)
{
  gum_database_close (self);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_database_exec, GumDatabase)
{
  gchar * sql, * error_message;
  gint status;

  if (!gum_database_check_open (self, isolate))
    return;

  if (!_gum_v8_args_parse (args, "s", &sql))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  status = sqlite3_exec (self->handle, sql, NULL, NULL, &error_message);
  g_free (sql);
  if (status != SQLITE_OK)
    goto error;

  return;

error:
  {
    _gum_v8_throw (isolate, "%s", error_message);
    sqlite3_free (error_message);
    return;
  }
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_database_prepare, GumDatabase)
{
  gchar * sql;
  sqlite3_stmt * statement;
  gint status;
  Local<Object> object;

  if (!gum_database_check_open (self, isolate))
    return;

  if (!_gum_v8_args_parse (args, "s", &sql))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  statement = NULL;
  status = sqlite3_prepare_v2 (self->handle, sql, -1, &statement, NULL);
  g_free (sql);
  if (statement == NULL)
    goto invalid_sql;

  object = gum_statement_new (statement, module);

  info.GetReturnValue ().Set (object);

  return;

invalid_sql:
  {
    if (status == SQLITE_OK)
      _gum_v8_throw (isolate, "invalid statement");
    else
      _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
    return;
  }
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_database_dump, GumDatabase)
{
  gpointer data, malloc_data;
  gsize size;
  GError * error;
  gchar * data_str;

  if (!gum_database_check_open (self, isolate))
    return;

  if (self->is_virtual)
  {
    gboolean found;

    found = gum_memory_vfs_get_file_contents (module->memory_vfs, self->path,
        &data, &size);
    g_assert (found);

    malloc_data = NULL;
  }
  else
  {
    error = NULL;
    g_file_get_contents (self->path, (gchar **) &data, &size, &error);
    if (_gum_v8_maybe_throw (isolate, &error))
      return;

    malloc_data = data;
  }

  data_str = gum_memory_vfs_contents_to_string (data, size);

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, data_str));

  g_free (data_str);
  g_free (malloc_data);
}

static Local<Object>
gum_database_new (sqlite3 * handle,
                  const gchar * path,
                  gboolean is_virtual,
                  GumV8Database * module)
{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto constructor = Local<FunctionTemplate>::New (isolate,
      *module->database);
  auto object = constructor->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 0, nullptr).ToLocalChecked ();

  auto database = g_slice_new (GumDatabase);
  database->wrapper = new Global<Object> (isolate, object);
  database->wrapper->SetWeak (database, gum_database_on_weak_notify,
      WeakCallbackType::kParameter);
  database->handle = handle;
  database->path = g_strdup (path);
  database->is_virtual = is_virtual;
  database->module = module;

  object->SetAlignedPointerInInternalField (0, database);

  g_hash_table_add (module->databases, database);

  return object;
}

static void
gum_database_free (GumDatabase * self)
{
  gum_database_close (self);

  delete self->wrapper;

  g_slice_free (GumDatabase, self);
}

static void
gum_database_close (GumDatabase * self)
{
  if (self->handle == NULL)
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  sqlite3_close_v2 (self->handle);
  self->handle = NULL;

  if (self->is_virtual)
    gum_memory_vfs_remove_file (self->module->memory_vfs, self->path);

  g_free (self->path);
  self->path = NULL;
}

static gboolean
gum_database_check_open (GumDatabase * self,
                         Isolate * isolate)
{
  if (self->handle == NULL)
  {
    _gum_v8_throw (isolate, "database is closed");
    return FALSE;
  }

  return TRUE;
}

static void
gum_database_on_weak_notify (const WeakCallbackInfo<GumDatabase> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->databases, self);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_statement_get_column_names, GumStatement)
{
  auto context = isolate->GetCurrentContext ();
  auto num_columns = sqlite3_column_count (self->handle);
  auto result = Array::New (isolate, num_columns);

  for (gint index = 0; index != num_columns; index++)
  {
    auto name = String::NewFromUtf8 (isolate,
        sqlite3_column_name (self->handle, index),
        NewStringType::kNormal).ToLocalChecked ();
    result->Set (context, index, name).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_statement_get_column_types, GumStatement)
{
  auto context = isolate->GetCurrentContext ();
  auto num_columns = sqlite3_column_count (self->handle);
  auto result = Array::New (isolate, num_columns);

  for (gint index = 0; index != num_columns; index++)
  {
    auto type_name = String::NewFromUtf8 (isolate,
        gum_column_type_to_string (sqlite3_column_type (self->handle, index)),
        NewStringType::kNormal).ToLocalChecked ();
    result->Set (context, index, type_name).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_statement_get_declared_types, GumStatement)
{
  auto context = isolate->GetCurrentContext ();
  auto num_columns = sqlite3_column_count (self->handle);
  auto result = Array::New (isolate, num_columns);

  for (gint index = 0; index != num_columns; index++)
  {
    auto decl_type = sqlite3_column_decltype (self->handle, index);
    Local<Value> value;
    if (decl_type != NULL)
    {
      value = String::NewFromUtf8 (isolate, decl_type,
          NewStringType::kNormal).ToLocalChecked ();
    }
    else
    {
      value = Null (isolate);
    }
    result->Set (context, index, value).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_statement_get_params_count, GumStatement)
{
  info.GetReturnValue ().Set (
      sqlite3_bind_parameter_count (self->handle));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_bind_integer, GumStatement)
{
  gint index, value;
  if (!_gum_v8_args_parse (args, "ii", &index, &value))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_bind_int64 (self->handle, index, value);
  if (status != SQLITE_OK)
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_bind_float, GumStatement)
{
  gint index;
  gdouble value;
  if (!_gum_v8_args_parse (args, "in", &index, &value))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_bind_double (self->handle, index, value);
  if (status != SQLITE_OK)
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_bind_text, GumStatement)
{
  gint index;
  gchar * value;
  if (!_gum_v8_args_parse (args, "is", &index, &value))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_bind_text (self->handle, index, value, -1, g_free);
  if (status != SQLITE_OK)
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_bind_blob, GumStatement)
{
  gint index;
  GBytes * bytes;
  if (!_gum_v8_args_parse (args, "iB~", &index, &bytes))
    return;

  gsize size;
  auto data = g_bytes_unref_to_data (bytes, &size);

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_bind_blob64 (self->handle, index, data, size, g_free);
  if (status != SQLITE_OK)
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_bind_null, GumStatement)
{
  gint index;
  if (!_gum_v8_args_parse (args, "i", &index))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_bind_null (self->handle, index);
  if (status != SQLITE_OK)
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_step, GumStatement)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_step (self->handle);
  switch (status)
  {
    case SQLITE_ROW:
      info.GetReturnValue ().Set (gum_parse_row (isolate, self->handle));
      break;
    case SQLITE_DONE:
      info.GetReturnValue ().SetNull ();
      break;
    default:
      _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
      break;
  }
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_statement_reset, GumStatement)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto status = sqlite3_reset (self->handle);
  if (status != SQLITE_OK)
    _gum_v8_throw (isolate, "%s", sqlite3_errstr (status));
}

static Local<Object>
gum_statement_new (sqlite3_stmt * handle,
                   GumV8Database * module)
{
  auto isolate = module->core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto constructor = Local<FunctionTemplate>::New (isolate,
      *module->statement);
  auto object = constructor->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 0, nullptr).ToLocalChecked ();

  auto statement = g_slice_new (GumStatement);
  statement->wrapper = new Global<Object> (isolate, object);
  statement->wrapper->SetWeak (statement, gum_statement_on_weak_notify,
      WeakCallbackType::kParameter);
  statement->handle = handle;
  statement->module = module;

  object->SetAlignedPointerInInternalField (0, statement);

  g_hash_table_add (module->statements, statement);

  return object;
}

static void
gum_statement_free (GumStatement * self)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  delete self->wrapper;

  sqlite3_finalize (self->handle);

  g_slice_free (GumStatement, self);
}

static void
gum_statement_on_weak_notify (const WeakCallbackInfo<GumStatement> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->statements, self);
}

static Local<Array>
gum_parse_row (Isolate * isolate,
               sqlite3_stmt * statement)
{
  auto context = isolate->GetCurrentContext ();

  auto num_columns = sqlite3_column_count (statement);
  auto row = Array::New (isolate, num_columns);

  for (gint index = 0; index != num_columns; index++)
  {
    auto column = gum_parse_column (isolate, statement, index);
    row->Set (context, index, column).Check ();
  }

  return row;
}

static Local<Value>
gum_parse_column (Isolate * isolate,
                  sqlite3_stmt * statement,
                  guint index)
{
  switch (sqlite3_column_type (statement, index))
  {
    case SQLITE_INTEGER:
      return Number::New (isolate, sqlite3_column_int64 (statement, index));
    case SQLITE_FLOAT:
      return Number::New (isolate, sqlite3_column_int64 (statement, index));
    case SQLITE_TEXT:
      return String::NewFromUtf8 (isolate,
          (const char *) sqlite3_column_text (statement, index),
          NewStringType::kNormal).ToLocalChecked ();
    case SQLITE_BLOB:
    {
      auto size = sqlite3_column_bytes (statement, index);
      auto data = g_memdup2 (sqlite3_column_blob (statement, index), size);
      return _gum_v8_array_buffer_new_take (isolate, data, size);
    }
    case SQLITE_NULL:
      return Null (isolate);
    default:
      g_assert_not_reached ();
  }

  return Local<Value> ();
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
