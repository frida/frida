/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickstream.h"

#include "gumquickmacros.h"

#ifdef HAVE_WINDOWS
# include <gio/gwin32inputstream.h>
# include <gio/gwin32outputstream.h>

# define GUM_NATIVE_INPUT_STREAM "Win32InputStream"
# define GUM_NATIVE_OUTPUT_STREAM "Win32OutputStream"
typedef gpointer GumStreamHandle;
#else
# include <gio/gunixinputstream.h>
# include <gio/gunixoutputstream.h>

# define GUM_NATIVE_INPUT_STREAM "UnixInputStream"
# define GUM_NATIVE_OUTPUT_STREAM "UnixOutputStream"
typedef gint GumStreamHandle;
#endif

typedef struct _GumQuickCloseIOStreamOperation GumQuickCloseIOStreamOperation;

typedef struct _GumQuickCloseInputOperation GumQuickCloseInputOperation;
typedef struct _GumQuickReadOperation GumQuickReadOperation;
typedef guint GumQuickReadStrategy;

typedef struct _GumQuickCloseOutputOperation GumQuickCloseOutputOperation;
typedef struct _GumQuickWriteOperation GumQuickWriteOperation;
typedef guint GumQuickWriteStrategy;

struct _GumQuickCloseIOStreamOperation
{
  GumQuickObjectOperation operation;
};

struct _GumQuickCloseInputOperation
{
  GumQuickObjectOperation operation;
};

struct _GumQuickReadOperation
{
  GumQuickObjectOperation operation;
  GumQuickReadStrategy strategy;
  gpointer buffer;
  gsize buffer_size;
};

enum _GumQuickReadStrategy
{
  GUM_QUICK_READ_SOME,
  GUM_QUICK_READ_ALL
};

struct _GumQuickCloseOutputOperation
{
  GumQuickObjectOperation operation;
};

struct _GumQuickWriteOperation
{
  GumQuickObjectOperation operation;
  GumQuickWriteStrategy strategy;
  GBytes * bytes;
};

enum _GumQuickWriteStrategy
{
  GUM_QUICK_WRITE_SOME,
  GUM_QUICK_WRITE_ALL
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_io_stream_construct)
GUMJS_DECLARE_GETTER (gumjs_io_stream_get_input)
GUMJS_DECLARE_GETTER (gumjs_io_stream_get_output)
GUMJS_DECLARE_FUNCTION (gumjs_io_stream_close)
static void gum_quick_close_io_stream_operation_start (
    GumQuickCloseIOStreamOperation * self);
static void gum_quick_close_io_stream_operation_finish (GIOStream * stream,
    GAsyncResult * result, GumQuickCloseIOStreamOperation * self);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_input_stream_construct)
GUMJS_DECLARE_FUNCTION (gumjs_input_stream_close)
static void gum_quick_close_input_operation_start (
    GumQuickCloseInputOperation * self);
static void gum_quick_close_input_operation_finish (GInputStream * stream,
    GAsyncResult * result, GumQuickCloseInputOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_input_stream_read)
GUMJS_DECLARE_FUNCTION (gumjs_input_stream_read_all)
static JSValue gumjs_input_stream_read_with_strategy (JSContext * ctx,
    JSValueConst this_val, GumQuickArgs * args, GumQuickReadStrategy strategy,
    GumQuickCore * core);
static void gum_quick_read_operation_dispose (GumQuickReadOperation * self);
static void gum_quick_read_operation_start (GumQuickReadOperation * self);
static void gum_quick_read_operation_finish (GInputStream * stream,
    GAsyncResult * result, GumQuickReadOperation * self);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_output_stream_construct)
GUMJS_DECLARE_FUNCTION (gumjs_output_stream_close)
static void gum_quick_close_output_operation_start (
    GumQuickCloseOutputOperation * self);
static void gum_quick_close_output_operation_finish (GOutputStream * stream,
    GAsyncResult * result, GumQuickCloseOutputOperation * self);
GUMJS_DECLARE_FUNCTION (gumjs_output_stream_write)
GUMJS_DECLARE_FUNCTION (gumjs_output_stream_write_all)
GUMJS_DECLARE_FUNCTION (gumjs_output_stream_write_memory_region)
static JSValue gumjs_output_stream_write_with_strategy (JSContext * ctx,
    JSValueConst this_val, GumQuickArgs * args, GumQuickWriteStrategy strategy,
    GumQuickCore * core);
static void gum_quick_write_operation_dispose (GumQuickWriteOperation * self);
static void gum_quick_write_operation_start (GumQuickWriteOperation * self);
static void gum_quick_write_operation_finish (GOutputStream * stream,
    GAsyncResult * result, GumQuickWriteOperation * self);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_input_stream_construct)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_output_stream_construct)

static gboolean gum_quick_native_stream_ctor_args_parse (GumQuickArgs * args,
    GumStreamHandle * handle, gboolean * auto_close);

static const JSClassDef gumjs_io_stream_def =
{
  .class_name = "IOStream",
};

static const JSCFunctionListEntry gumjs_io_stream_entries[] =
{
  JS_CGETSET_DEF ("input", gumjs_io_stream_get_input, NULL),
  JS_CGETSET_DEF ("output", gumjs_io_stream_get_output, NULL),
  JS_CFUNC_DEF ("_close", 0, gumjs_io_stream_close),
};

static const JSClassDef gumjs_input_stream_def =
{
  .class_name = "InputStream",
};

static const JSCFunctionListEntry gumjs_input_stream_entries[] =
{
  JS_CFUNC_DEF ("_close", 0, gumjs_input_stream_close),
  JS_CFUNC_DEF ("_read", 0, gumjs_input_stream_read),
  JS_CFUNC_DEF ("_readAll", 0, gumjs_input_stream_read_all),
};

static const JSClassDef gumjs_output_stream_def =
{
  .class_name = "OutputStream",
};

static const JSCFunctionListEntry gumjs_output_stream_entries[] =
{
  JS_CFUNC_DEF ("_close", 0, gumjs_output_stream_close),
  JS_CFUNC_DEF ("_write", 0, gumjs_output_stream_write),
  JS_CFUNC_DEF ("_writeAll", 0, gumjs_output_stream_write_all),
  JS_CFUNC_DEF ("_writeMemoryRegion", 0,
      gumjs_output_stream_write_memory_region),
};

static const JSClassDef gumjs_native_input_stream_def =
{
  .class_name = GUM_NATIVE_INPUT_STREAM,
};

static const JSClassDef gumjs_native_output_stream_def =
{
  .class_name = GUM_NATIVE_OUTPUT_STREAM,
};

void
_gum_quick_stream_init (GumQuickStream * self,
                        JSValue ns,
                        GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor, input_stream_proto, output_stream_proto;

  self->core = core;

  _gum_quick_core_store_module_data (core, "stream", self);

  _gum_quick_create_class (ctx, &gumjs_io_stream_def, core,
      &self->io_stream_class, &proto);
  self->io_stream_proto = JS_DupValue (ctx, proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_io_stream_construct,
      gumjs_io_stream_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_io_stream_entries,
      G_N_ELEMENTS (gumjs_io_stream_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_io_stream_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_input_stream_def, core,
      &self->input_stream_class, &proto);
  input_stream_proto = proto;
  ctor = JS_NewCFunction2 (ctx, gumjs_input_stream_construct,
      gumjs_input_stream_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_input_stream_entries,
      G_N_ELEMENTS (gumjs_input_stream_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_input_stream_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_output_stream_def, core,
      &self->output_stream_class, &proto);
  output_stream_proto = proto;
  ctor = JS_NewCFunction2 (ctx, gumjs_output_stream_construct,
      gumjs_output_stream_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_output_stream_entries,
      G_N_ELEMENTS (gumjs_output_stream_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_output_stream_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_subclass (ctx, &gumjs_native_input_stream_def,
      self->input_stream_class, input_stream_proto, core,
      &self->native_input_stream_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_native_input_stream_construct,
      gumjs_native_input_stream_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_DefinePropertyValueStr (ctx, ns, gumjs_native_input_stream_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_create_subclass (ctx, &gumjs_native_output_stream_def,
      self->output_stream_class, output_stream_proto, core,
      &self->native_output_stream_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_native_output_stream_construct,
      gumjs_native_output_stream_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_DefinePropertyValueStr (ctx, ns, gumjs_native_output_stream_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_object_manager_init (&self->objects, self, core);
}

void
_gum_quick_stream_flush (GumQuickStream * self)
{
  _gum_quick_object_manager_flush (&self->objects);
}

void
_gum_quick_stream_dispose (GumQuickStream * self)
{
  JSContext * ctx = self->core->ctx;

  _gum_quick_object_manager_free (&self->objects);

  JS_FreeValue (ctx, self->io_stream_proto);
}

void
_gum_quick_stream_finalize (GumQuickStream * self)
{
}

static GumQuickStream *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "stream");
}

static gboolean
gum_quick_io_stream_get (JSContext * ctx,
                         JSValueConst val,
                         GumQuickCore * core,
                         GumQuickObject ** object)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->io_stream_class, core,
      (gpointer *) object);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_io_stream_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_GETTER (gumjs_io_stream_get_input)
{
  JSValue wrapper;
  GumQuickStream * parent;
  GumQuickObject * self;
  GInputStream * handle;
  GumQuickObject * input;

  wrapper = JS_GetProperty (ctx, this_val,
      GUM_QUICK_CORE_ATOM (core, cachedInput));
  if (!JS_IsUndefined (wrapper))
    return wrapper;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_io_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  handle = g_io_stream_get_input_stream (self->handle);

  input = _gum_quick_object_manager_lookup (&parent->objects, handle);
  if (input != NULL)
    return JS_DupValue (ctx, input->wrapper);

  wrapper = JS_NewObjectClass (ctx, parent->input_stream_class);

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper,
      g_object_ref (handle));

  JS_DefinePropertyValue (ctx, this_val,
      GUM_QUICK_CORE_ATOM (core, cachedInput),
      JS_DupValue (ctx, wrapper),
      0);

  return wrapper;
}

GUMJS_DEFINE_GETTER (gumjs_io_stream_get_output)
{
  GumQuickStream * parent;
  GumQuickObject * self;
  GOutputStream * handle;
  GumQuickObject * output;
  JSValue wrapper;

  wrapper = JS_GetProperty (ctx, this_val,
      GUM_QUICK_CORE_ATOM (core, cachedOutput));
  if (!JS_IsUndefined (wrapper))
    return wrapper;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_io_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  handle = g_io_stream_get_output_stream (self->handle);

  output = _gum_quick_object_manager_lookup (&parent->objects, handle);
  if (output != NULL)
    return JS_DupValue (ctx, output->wrapper);

  wrapper = JS_NewObjectClass (ctx, parent->output_stream_class);

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper,
      g_object_ref (handle));

  JS_DefinePropertyValue (ctx, this_val,
      GUM_QUICK_CORE_ATOM (core, cachedOutput),
      JS_DupValue (ctx, wrapper),
      0);

  return wrapper;
}

GUMJS_DEFINE_FUNCTION (gumjs_io_stream_close)
{
  GumQuickStream * parent;
  GumQuickObject * self;
  JSValue callback;
  GumQuickCloseIOStreamOperation * op;
  GPtrArray * dependencies;
  GumQuickObject * input, * output;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_io_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "F", &callback))
    return JS_EXCEPTION;

  op = _gum_quick_object_operation_new (GumQuickCloseIOStreamOperation, self,
      callback, gum_quick_close_io_stream_operation_start, NULL);

  dependencies = g_ptr_array_sized_new (2);

  input = _gum_quick_object_manager_lookup (&parent->objects,
      g_io_stream_get_input_stream (self->handle));
  if (input != NULL)
  {
    g_cancellable_cancel (input->cancellable);
    g_ptr_array_add (dependencies, input);
  }

  output = _gum_quick_object_manager_lookup (&parent->objects,
      g_io_stream_get_output_stream (self->handle));
  if (output != NULL)
  {
    g_cancellable_cancel (output->cancellable);
    g_ptr_array_add (dependencies, output);
  }

  g_cancellable_cancel (self->cancellable);

  _gum_quick_object_operation_schedule_when_idle (op, dependencies);

  g_ptr_array_unref (dependencies);

  return JS_UNDEFINED;
}

static void
gum_quick_close_io_stream_operation_start (
    GumQuickCloseIOStreamOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);

  g_io_stream_close_async (op->object->handle, G_PRIORITY_DEFAULT, NULL,
      (GAsyncReadyCallback) gum_quick_close_io_stream_operation_finish, self);
}

static void
gum_quick_close_io_stream_operation_finish (
    GIOStream * stream,
    GAsyncResult * result,
    GumQuickCloseIOStreamOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GError * error = NULL;
  gboolean success;
  GumQuickScope scope;
  JSValue argv[2];

  success = g_io_stream_close_finish (stream, result, &error);

  _gum_quick_scope_enter (&scope, core);

  argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
  argv[1] = JS_NewBool (ctx, success);

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

static gboolean
gum_quick_input_stream_get (JSContext * ctx,
                            JSValueConst val,
                            GumQuickCore * core,
                            GumQuickObject ** object)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->input_stream_class, core,
      (gpointer *) object);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_input_stream_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FUNCTION (gumjs_input_stream_close)
{
  GumQuickObject * self;
  JSValue callback;
  GumQuickCloseInputOperation * op;

  if (!gum_quick_input_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "F", &callback))
    return JS_EXCEPTION;

  g_cancellable_cancel (self->cancellable);

  op = _gum_quick_object_operation_new (GumQuickCloseInputOperation, self,
      callback, gum_quick_close_input_operation_start, NULL);
  _gum_quick_object_operation_schedule_when_idle (op, NULL);

  return JS_UNDEFINED;
}

static void
gum_quick_close_input_operation_start (GumQuickCloseInputOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);

  g_input_stream_close_async (op->object->handle, G_PRIORITY_DEFAULT,
      NULL, (GAsyncReadyCallback) gum_quick_close_input_operation_finish, self);
}

static void
gum_quick_close_input_operation_finish (GInputStream * stream,
                                        GAsyncResult * result,
                                        GumQuickCloseInputOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GError * error = NULL;
  gboolean success;
  GumQuickScope scope;
  JSValue argv[2];

  success = g_input_stream_close_finish (stream, result, &error);

  _gum_quick_scope_enter (&scope, core);

  argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
  argv[1] = JS_NewBool (ctx, success);

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

GUMJS_DEFINE_FUNCTION (gumjs_input_stream_read)
{
  return gumjs_input_stream_read_with_strategy (ctx, this_val, args,
      GUM_QUICK_READ_SOME, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_input_stream_read_all)
{
  return gumjs_input_stream_read_with_strategy (ctx, this_val, args,
      GUM_QUICK_READ_ALL, core);
}

static JSValue
gumjs_input_stream_read_with_strategy (JSContext * ctx,
                                       JSValueConst this_val,
                                       GumQuickArgs * args,
                                       GumQuickReadStrategy strategy,
                                       GumQuickCore * core)
{
  GumQuickObject * self;
  guint64 size;
  JSValue callback;
  GumQuickReadOperation * op;

  if (!gum_quick_input_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "QF", &size, &callback))
    return JS_EXCEPTION;

  op = _gum_quick_object_operation_new (GumQuickReadOperation, self, callback,
      gum_quick_read_operation_start, gum_quick_read_operation_dispose);
  op->strategy = strategy;
  op->buffer = g_malloc (size);
  op->buffer_size = size;
  _gum_quick_object_operation_schedule (op);

  return JS_UNDEFINED;
}

static void
gum_quick_read_operation_dispose (GumQuickReadOperation * self)
{
  g_free (self->buffer);
}

static void
gum_quick_read_operation_start (GumQuickReadOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickObject * stream = op->object;

  if (self->strategy == GUM_QUICK_READ_SOME)
  {
    g_input_stream_read_async (stream->handle, self->buffer, self->buffer_size,
        G_PRIORITY_DEFAULT, stream->cancellable,
        (GAsyncReadyCallback) gum_quick_read_operation_finish, self);
  }
  else
  {
    g_assert (self->strategy == GUM_QUICK_READ_ALL);

    g_input_stream_read_all_async (stream->handle, self->buffer,
        self->buffer_size, G_PRIORITY_DEFAULT, stream->cancellable,
        (GAsyncReadyCallback) gum_quick_read_operation_finish, self);
  }
}

static void
gum_quick_read_operation_finish (GInputStream * stream,
                                 GAsyncResult * result,
                                 GumQuickReadOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  gsize bytes_read = 0;
  GError * error = NULL;
  GumQuickScope scope;
  JSValue argv[2];
  gboolean emit_data;

  if (self->strategy == GUM_QUICK_READ_SOME)
  {
    gsize n;

    n = g_input_stream_read_finish (stream, result, &error);
    if (n > 0)
      bytes_read = n;
  }
  else
  {
    g_assert (self->strategy == GUM_QUICK_READ_ALL);

    g_input_stream_read_all_finish (stream, result, &bytes_read, &error);
  }

  _gum_quick_scope_enter (&scope, op->core);

  if (self->strategy == GUM_QUICK_READ_ALL && bytes_read != self->buffer_size)
  {
    argv[0] = (error != NULL)
        ? _gum_quick_error_new_take_error (ctx, &error, core)
        : _gum_quick_error_new (ctx, "short read", core);
    emit_data = TRUE;
  }
  else if (error == NULL)
  {
    argv[0] = JS_NULL;
    emit_data = TRUE;
  }
  else
  {
    argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
    emit_data = FALSE;
  }

  if (emit_data)
    argv[1] = JS_NewArrayBufferCopy (ctx, self->buffer, bytes_read);
  else
    argv[1] = JS_NULL;

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);
  JS_FreeValue (ctx, argv[1]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

static gboolean
gum_quick_output_stream_get (JSContext * ctx,
                             JSValueConst val,
                             GumQuickCore * core,
                             GumQuickObject ** object)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->output_stream_class, core,
      (gpointer *) object);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_output_stream_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FUNCTION (gumjs_output_stream_close)
{
  GumQuickObject * self;
  JSValue callback;
  GumQuickCloseOutputOperation * op;

  if (!gum_quick_output_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "F", &callback))
    return JS_EXCEPTION;

  g_cancellable_cancel (self->cancellable);

  op = _gum_quick_object_operation_new (GumQuickCloseOutputOperation, self,
      callback, gum_quick_close_output_operation_start, NULL);
  _gum_quick_object_operation_schedule_when_idle (op, NULL);

  return JS_UNDEFINED;
}

static void
gum_quick_close_output_operation_start (GumQuickCloseOutputOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);

  g_output_stream_close_async (op->object->handle, G_PRIORITY_DEFAULT, NULL,
      (GAsyncReadyCallback) gum_quick_close_output_operation_finish, self);
}

static void
gum_quick_close_output_operation_finish (GOutputStream * stream,
                                         GAsyncResult * result,
                                         GumQuickCloseOutputOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  GError * error = NULL;
  gboolean success;
  GumQuickScope scope;
  JSValue argv[2];

  success = g_output_stream_close_finish (stream, result, &error);

  _gum_quick_scope_enter (&scope, op->core);

  argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
  argv[1] = JS_NewBool (ctx, success);

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

GUMJS_DEFINE_FUNCTION (gumjs_output_stream_write)
{
  return gumjs_output_stream_write_with_strategy (ctx, this_val, args,
      GUM_QUICK_WRITE_SOME, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_output_stream_write_all)
{
  return gumjs_output_stream_write_with_strategy (ctx, this_val, args,
      GUM_QUICK_WRITE_ALL, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_output_stream_write_memory_region)
{
  GumQuickObject * self;
  gconstpointer address;
  gsize length;
  JSValue callback;
  GumQuickWriteOperation * op;

  if (!gum_quick_output_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "pZF", &address, &length, &callback))
    return JS_EXCEPTION;

  op = _gum_quick_object_operation_new (GumQuickWriteOperation, self, callback,
      gum_quick_write_operation_start, gum_quick_write_operation_dispose);
  op->strategy = GUM_QUICK_WRITE_ALL;
  op->bytes = g_bytes_new_static (address, length);
  _gum_quick_object_operation_schedule (op);

  return JS_UNDEFINED;
}

static JSValue
gumjs_output_stream_write_with_strategy (JSContext * ctx,
                                         JSValueConst this_val,
                                         GumQuickArgs * args,
                                         GumQuickWriteStrategy strategy,
                                         GumQuickCore * core)
{
  GumQuickObject * self;
  GBytes * bytes;
  JSValue callback;
  GumQuickWriteOperation * op;

  if (!gum_quick_output_stream_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "BF", &bytes, &callback))
    return JS_EXCEPTION;

  op = _gum_quick_object_operation_new (GumQuickWriteOperation, self, callback,
      gum_quick_write_operation_start, gum_quick_write_operation_dispose);
  op->strategy = strategy;
  op->bytes = g_bytes_ref (bytes);
  _gum_quick_object_operation_schedule (op);

  return JS_UNDEFINED;
}

static void
gum_quick_write_operation_dispose (GumQuickWriteOperation * self)
{
  g_bytes_unref (self->bytes);
}

static void
gum_quick_write_operation_start (GumQuickWriteOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickObject * stream = op->object;

  if (self->strategy == GUM_QUICK_WRITE_SOME)
  {
    g_output_stream_write_bytes_async (stream->handle, self->bytes,
        G_PRIORITY_DEFAULT, stream->cancellable,
        (GAsyncReadyCallback) gum_quick_write_operation_finish, self);
  }
  else
  {
    gsize size;
    gconstpointer data;

    g_assert (self->strategy == GUM_QUICK_WRITE_ALL);

    data = g_bytes_get_data (self->bytes, &size);

    g_output_stream_write_all_async (stream->handle, data, size,
        G_PRIORITY_DEFAULT, stream->cancellable,
        (GAsyncReadyCallback) gum_quick_write_operation_finish, self);
  }
}

static void
gum_quick_write_operation_finish (GOutputStream * stream,
                                  GAsyncResult * result,
                                  GumQuickWriteOperation * self)
{
  GumQuickObjectOperation * op = GUM_QUICK_OBJECT_OPERATION (self);
  GumQuickCore * core = op->core;
  JSContext * ctx = core->ctx;
  gsize bytes_written = 0;
  GError * error = NULL;
  GumQuickScope scope;
  JSValue argv[2];

  if (self->strategy == GUM_QUICK_WRITE_SOME)
  {
    gssize n;

    n = g_output_stream_write_bytes_finish (stream, result, &error);
    if (n > 0)
      bytes_written = n;
  }
  else
  {
    g_assert (self->strategy == GUM_QUICK_WRITE_ALL);

    g_output_stream_write_all_finish (stream, result, &bytes_written, &error);
  }

  _gum_quick_scope_enter (&scope, op->core);

  if (self->strategy == GUM_QUICK_WRITE_ALL &&
      bytes_written != g_bytes_get_size (self->bytes))
  {
    argv[0] = (error != NULL)
        ? _gum_quick_error_new_take_error (ctx, &error, core)
        : _gum_quick_error_new (ctx, "short write", core);
  }
  else if (error == NULL)
  {
    argv[0] = JS_NULL;
  }
  else
  {
    argv[0] = _gum_quick_error_new_take_error (ctx, &error, core);
  }

  argv[1] = JS_NewInt64 (ctx, bytes_written);

  _gum_quick_scope_call_void (&scope, op->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  _gum_quick_scope_leave (&scope);

  _gum_quick_object_operation_finish (op);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_input_stream_construct)
{
  GumQuickStream * parent;
  JSValue wrapper;
  GumStreamHandle handle;
  gboolean auto_close;
  JSValue proto;
  GInputStream * stream;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_native_stream_ctor_args_parse (args, &handle, &auto_close))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto,
      parent->native_input_stream_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

#ifdef HAVE_WINDOWS
  stream = g_win32_input_stream_new (handle, auto_close);
#else
  stream = g_unix_input_stream_new (handle, auto_close);
#endif

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper, stream);

  return wrapper;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_output_stream_construct)
{
  GumQuickStream * parent;
  JSValue wrapper;
  GumStreamHandle handle;
  gboolean auto_close;
  JSValue proto;
  GOutputStream * stream;

  parent = gumjs_get_parent_module (core);

  if (!gum_quick_native_stream_ctor_args_parse (args, &handle, &auto_close))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto,
      parent->native_output_stream_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

#ifdef HAVE_WINDOWS
  stream = g_win32_output_stream_new (handle, auto_close);
#else
  stream = g_unix_output_stream_new (handle, auto_close);
#endif

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper, stream);

  return wrapper;
}

static gboolean
gum_quick_native_stream_ctor_args_parse (GumQuickArgs * args,
                                         GumStreamHandle * handle,
                                         gboolean * auto_close)
{
  JSValue options = JS_NULL;

#ifdef HAVE_WINDOWS
  if (!_gum_quick_args_parse (args, "p|O", handle, &options))
#else
  if (!_gum_quick_args_parse (args, "i|O", handle, &options))
#endif
    return FALSE;

  *auto_close = FALSE;
  if (!JS_IsNull (options))
  {
    JSContext * ctx = args->ctx;
    GumQuickCore * core = args->core;
    JSValue val;
    gboolean valid;

    val = JS_GetProperty (ctx, options, GUM_QUICK_CORE_ATOM (core, autoClose));
    if (JS_IsException (val))
      return FALSE;
    valid = _gum_quick_boolean_get (ctx, val, auto_close);
    JS_FreeValue (ctx, val);

    if (!valid)
      return FALSE;
  }

  return TRUE;
}
