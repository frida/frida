/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickchecksum.h"

#include "gumquickmacros.h"

#include <string.h>

typedef struct _GumChecksum GumChecksum;

struct _GumChecksum
{
  GChecksum * handle;
  GChecksumType type;
  gboolean closed;
};

GUMJS_DECLARE_FUNCTION (gumjs_checksum_compute)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_checksum_construct)
GUMJS_DECLARE_FINALIZER (gumjs_checksum_finalize)
GUMJS_DECLARE_FUNCTION (gumjs_checksum_update)
GUMJS_DECLARE_FUNCTION (gumjs_checksum_copy)
GUMJS_DECLARE_FUNCTION (gumjs_checksum_get_string)
GUMJS_DECLARE_FUNCTION (gumjs_checksum_peek_string)
GUMJS_DECLARE_FUNCTION (gumjs_checksum_get_digest)
GUMJS_DECLARE_FUNCTION (gumjs_checksum_peek_digest)

static GumChecksum * gum_checksum_new (GChecksumType type);
static void gum_checksum_free (GumChecksum * self);

static GumChecksum * gum_checksum_copy (GumChecksum * self);

static gboolean gum_quick_checksum_type_get (JSContext * ctx,
    const gchar * name, GChecksumType * type);

static const JSClassDef gumjs_checksum_def =
{
  .class_name = "Checksum",
  .finalizer = gumjs_checksum_finalize,
};

static const JSCFunctionListEntry gumjs_checksum_module_entries[] =
{
  JS_CFUNC_DEF ("compute", 2, gumjs_checksum_compute),
};

static const JSCFunctionListEntry gumjs_checksum_entries[] =
{
  JS_CFUNC_DEF ("update", 1, gumjs_checksum_update),
  JS_CFUNC_DEF ("copy", 0, gumjs_checksum_copy),
  JS_CFUNC_DEF ("getString", 0, gumjs_checksum_get_string),
  JS_CFUNC_DEF ("peekString", 0, gumjs_checksum_peek_string),
  JS_CFUNC_DEF ("getDigest", 0, gumjs_checksum_get_digest),
  JS_CFUNC_DEF ("peekDigest", 0, gumjs_checksum_peek_digest),
};

void
_gum_quick_checksum_init (GumQuickChecksum * self,
                          JSValue ns,
                          GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "checksum", self);

  _gum_quick_create_class (ctx, &gumjs_checksum_def, core,
      &self->checksum_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_checksum_construct,
      gumjs_checksum_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, ctor, gumjs_checksum_module_entries,
      G_N_ELEMENTS (gumjs_checksum_module_entries));
  JS_SetPropertyFunctionList (ctx, proto, gumjs_checksum_entries,
      G_N_ELEMENTS (gumjs_checksum_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_checksum_def.class_name, ctor,
      JS_PROP_C_W_E);
}

void
_gum_quick_checksum_dispose (GumQuickChecksum * self)
{
}

void
_gum_quick_checksum_finalize (GumQuickChecksum * self)
{
}

static GumQuickChecksum *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "checksum");
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_compute)
{
  JSValue result;
  JSValue data_val = args->elements[1];
  const gchar * type_str, * str;
  GBytes * bytes;
  GChecksumType type;
  gchar * result_str;

  if (JS_IsString (data_val))
  {
    if (!_gum_quick_args_parse (args, "ss", &type_str, &str))
      return JS_EXCEPTION;
    bytes = NULL;
  }
  else
  {
    if (!_gum_quick_args_parse (args, "sB", &type_str, &bytes))
      return JS_EXCEPTION;
    str = NULL;
  }

  if (!gum_quick_checksum_type_get (ctx, type_str, &type))
    return JS_EXCEPTION;

  if (str != NULL)
    result_str = g_compute_checksum_for_string (type, str, -1);
  else
    result_str = g_compute_checksum_for_bytes (type, bytes);
  result = JS_NewString (ctx, result_str);
  g_free (result_str);

  return result;
}

static gboolean
gum_checksum_get (JSContext * ctx,
                  JSValueConst val,
                  GumQuickCore * core,
                  GumChecksum ** checksum)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->checksum_class, core,
      (gpointer *) checksum);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_checksum_construct)
{
  JSValue wrapper = JS_NULL;
  const gchar * type_str;
  GChecksumType type;
  JSValue proto;

  if (!_gum_quick_args_parse (args, "s", &type_str))
    return JS_EXCEPTION;

  if (!gum_quick_checksum_type_get (ctx, type_str, &type))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto,
      gumjs_get_parent_module (core)->checksum_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  JS_SetOpaque (wrapper, gum_checksum_new (type));

  return wrapper;
}

GUMJS_DEFINE_FINALIZER (gumjs_checksum_finalize)
{
  GumChecksum * checksum;

  checksum = JS_GetOpaque (val, gumjs_get_parent_module (core)->checksum_class);
  if (checksum == NULL)
    return;

  gum_checksum_free (checksum);
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_update)
{
  GumChecksum * self;
  JSValue data_val = args->elements[0];
  const gchar * str;
  GBytes * bytes;

  if (!gum_checksum_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (self->closed)
    goto invalid_operation;

  if (JS_IsString (data_val))
  {
    if (!_gum_quick_args_parse (args, "s", &str))
      return JS_EXCEPTION;
    bytes = NULL;
  }
  else
  {
    if (!_gum_quick_args_parse (args, "B", &bytes))
      return JS_EXCEPTION;
    str = NULL;
  }

  if (str != NULL)
  {
    g_checksum_update (self->handle, (const guchar *) str, -1);
  }
  else
  {
    gconstpointer data;
    gsize size;

    data = g_bytes_get_data (bytes, &size);

    g_checksum_update (self->handle, data, size);
  }

  return JS_DupValue (ctx, this_val);

invalid_operation:
  {
    _gum_quick_throw_literal (ctx, "checksum is closed");
    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_copy)
{
  GumChecksum * self;
  JSValue wrapper;

  if (!gum_checksum_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->checksum_class);
  JS_SetOpaque (wrapper, gum_checksum_copy (self));

  return wrapper;
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_get_string)
{
  GumChecksum * self;

  if (!gum_checksum_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  self->closed = TRUE;

  return JS_NewString (ctx, g_checksum_get_string (self->handle));
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_peek_string)
{
  JSValue result;
  GumChecksum * self;
  GChecksum * clone;

  if (!gum_checksum_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  clone = g_checksum_copy (self->handle);
  result = JS_NewString (ctx, g_checksum_get_string (clone));
  g_checksum_free (clone);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_get_digest)
{
  JSValue result;
  GumChecksum * self;
  gsize length;
  guint8 * data;

  if (!gum_checksum_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  self->closed = TRUE;

  length = g_checksum_type_get_length (self->type);
  data = g_malloc (length);
  result = JS_NewArrayBuffer (ctx, data, length, _gum_quick_array_buffer_free,
      data, FALSE);

  g_checksum_get_digest (self->handle, data, &length);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_checksum_peek_digest)
{
  JSValue result;
  GumChecksum * self;
  GChecksum * clone;
  gsize length;
  guint8 * data;

  if (!gum_checksum_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  clone = g_checksum_copy (self->handle);

  length = g_checksum_type_get_length (self->type);
  data = g_malloc (length);
  result = JS_NewArrayBuffer (ctx, data, length, _gum_quick_array_buffer_free,
      data, FALSE);

  g_checksum_get_digest (clone, data, &length);
  g_checksum_free (clone);

  return result;
}

static GumChecksum *
gum_checksum_new (GChecksumType type)
{
  GumChecksum * cs;

  cs = g_slice_new (GumChecksum);
  cs->handle = g_checksum_new (type);
  cs->type = type;
  cs->closed = FALSE;

  return cs;
}

static GumChecksum *
gum_checksum_copy (GumChecksum * self)
{
  GumChecksum * cs;

  cs = g_slice_new (GumChecksum);
  cs->handle = g_checksum_copy (self->handle);
  cs->type = self->type;
  cs->closed = self->closed;

  return cs;
}

static void
gum_checksum_free (GumChecksum * self)
{
  g_checksum_free (self->handle);

  g_slice_free (GumChecksum, self);
}

static gboolean
gum_quick_checksum_type_get (JSContext * ctx,
                             const gchar * name,
                             GChecksumType * type)
{
  if (strcmp (name, "sha256") == 0)
    *type = G_CHECKSUM_SHA256;
  else if (strcmp (name, "sha384") == 0)
    *type = G_CHECKSUM_SHA384;
  else if (strcmp (name, "sha512") == 0)
    *type = G_CHECKSUM_SHA512;
  else if (strcmp (name, "sha1") == 0)
    *type = G_CHECKSUM_SHA1;
  else if (strcmp (name, "md5") == 0)
    *type = G_CHECKSUM_MD5;
  else
    goto invalid_type;

  return TRUE;

invalid_type:
  _gum_quick_throw_literal (ctx, "unsupported checksum type");
  return FALSE;
}
