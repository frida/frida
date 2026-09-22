/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickvalue.h"

#include <gum/gum-init.h>

#include <stdarg.h>
#include <string.h>

#define GUM_MAX_JS_BYTE_ARRAY_LENGTH (100 * 1024 * 1024)

static void gum_quick_args_free_value_later (GumQuickArgs * self, JSValue v);
static void gum_quick_args_free_cstring_later (GumQuickArgs * self,
    const char * s);
static void gum_quick_args_free_array_later (GumQuickArgs * self, GArray * a);
static void gum_quick_args_free_bytes_later (GumQuickArgs * self, GBytes * b);
static void gum_quick_args_free_match_pattern_later (GumQuickArgs * self,
    GumMatchPattern * p);

static JSClassID gum_get_class_id_for_class_def (const JSClassDef * def);
static void gum_deinit_class_ids (void);

static const gchar * gum_exception_type_to_string (GumExceptionType type);
static const gchar * gum_thread_state_to_string (GumThreadState state);
static const gchar * gum_memory_operation_to_string (
    GumMemoryOperation operation);

G_LOCK_DEFINE_STATIC (gum_class_ids);
static GHashTable * gum_class_ids;

void
_gum_quick_args_init (GumQuickArgs * args,
                      JSContext * ctx,
                      int count,
                      JSValueConst * elements,
                      GumQuickCore * core)
{
  args->ctx = ctx;
  args->count = count;
  args->elements = elements;

  args->core = core;

  args->values = NULL;
  args->cstrings = NULL;
  args->arrays = NULL;
  args->bytes = NULL;
  args->match_patterns = NULL;
}

void
_gum_quick_args_destroy (GumQuickArgs * args)
{
  JSContext * ctx = args->ctx;
  GSList * cur, * next;
  GArray * values;

  g_slist_free_full (g_steal_pointer (&args->match_patterns),
      (GDestroyNotify) gum_match_pattern_unref);

  g_slist_free_full (g_steal_pointer (&args->bytes),
      (GDestroyNotify) g_bytes_unref);

  g_slist_free_full (g_steal_pointer (&args->arrays),
      (GDestroyNotify) g_array_unref);

  for (cur = g_steal_pointer (&args->cstrings); cur != NULL; cur = next)
  {
    char * str = cur->data;
    next = cur->next;

    JS_FreeCString (ctx, str);

    g_slist_free_1 (cur);
  }

  values = g_steal_pointer (&args->values);
  if (values != NULL)
  {
    guint i;

    for (i = 0; i != values->len; i++)
    {
      JSValue val = g_array_index (values, JSValue, i);
      JS_FreeValue (ctx, val);
    }

    g_array_free (values, TRUE);
  }
}

gboolean
_gum_quick_args_parse (GumQuickArgs * self,
                       const gchar * format,
                       ...)
{
  JSContext * ctx = self->ctx;
  GumQuickCore * core = self->core;
  va_list ap;
  int arg_index;
  const gchar * t;
  gboolean is_required;
  const gchar * error_message = NULL;

  va_start (ap, format);

  arg_index = 0;
  is_required = TRUE;
  for (t = format; *t != '\0'; t++)
  {
    JSValue arg;

    if (*t == '|')
    {
      is_required = FALSE;
      continue;
    }

    arg = (arg_index < self->count) ? self->elements[arg_index] : JS_UNDEFINED;

    if (JS_IsUndefined (arg))
    {
      if (is_required)
        goto missing_argument;
      else
        break;
    }

    switch (*t)
    {
      case 'i':
      {
        gint i;

        if (!_gum_quick_int_get (ctx, arg, &i))
          goto propagate_exception;

        *va_arg (ap, gint *) = i;

        break;
      }
      case 'u':
      {
        guint u;

        if (!_gum_quick_uint_get (ctx, arg, &u))
          goto propagate_exception;

        *va_arg (ap, guint *) = (guint) u;

        break;
      }
      case 'q':
      {
        gint64 i;
        gboolean is_fuzzy;

        is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        if (is_fuzzy)
        {
          if (!_gum_quick_int64_parse (ctx, arg, core, &i))
            goto propagate_exception;
        }
        else
        {
          if (!_gum_quick_int64_get (ctx, arg, core, &i))
            goto propagate_exception;
        }

        *va_arg (ap, gint64 *) = i;

        break;
      }
      case 'Q':
      {
        guint64 u;
        gboolean is_fuzzy;

        is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        if (is_fuzzy)
        {
          if (!_gum_quick_uint64_parse (ctx, arg, core, &u))
            goto propagate_exception;
        }
        else
        {
          if (!_gum_quick_uint64_get (ctx, arg, core, &u))
            goto propagate_exception;
        }

        *va_arg (ap, guint64 *) = u;

        break;
      }
      case 'z':
      {
        gssize value;

        if (!_gum_quick_ssize_get (ctx, arg, core, &value))
          goto propagate_exception;

        *va_arg (ap, gssize *) = value;

        break;
      }
      case 'Z':
      {
        gsize value;

        if (!_gum_quick_size_get (ctx, arg, core, &value))
          goto propagate_exception;

        *va_arg (ap, gsize *) = value;

        break;
      }
      case 'n':
      {
        gdouble d;

        if (!_gum_quick_float64_get (ctx, arg, &d))
          goto propagate_exception;

        *va_arg (ap, gdouble *) = d;

        break;
      }
      case 't':
      {
        gboolean b;

        if (!_gum_quick_boolean_get (ctx, arg, &b))
          goto propagate_exception;

        *va_arg (ap, gboolean *) = b;

        break;
      }
      case 'p':
      {
        gpointer ptr;
        gboolean is_fuzzy;

        is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        if (is_fuzzy)
        {
          if (!_gum_quick_native_pointer_parse (ctx, arg, core, &ptr))
            goto propagate_exception;
        }
        else
        {
          if (!_gum_quick_native_pointer_get (ctx, arg, core, &ptr))
            goto propagate_exception;
        }

        *va_arg (ap, gpointer *) = ptr;

        break;
      }
      case 's':
      {
        const gchar * str;
        gboolean is_nullable;

        is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && JS_IsNull (arg))
          str = NULL;
        else if (!_gum_quick_string_get (ctx, arg, &str))
          goto propagate_exception;

        gum_quick_args_free_cstring_later (self, str);

        *va_arg (ap, const char **) = str;

        break;
      }
      case 'R':
      {
        GArray * ranges;

        if (!_gum_quick_memory_ranges_get (ctx, arg, core, &ranges))
          goto propagate_exception;

        gum_quick_args_free_array_later (self, ranges);

        *va_arg (ap, GArray **) = ranges;

        break;
      }
      case 'm':
      {
        GumPageProtection prot;

        if (!_gum_quick_page_protection_get (ctx, arg, &prot))
          goto propagate_exception;

        *va_arg (ap, GumPageProtection *) = prot;

        break;
      }
      case 'V':
      {
        gboolean is_nullable;

        is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (JS_IsNull (arg))
        {
          if (!is_nullable)
            goto expected_object_or_string;
        }
        else if (!JS_IsObject (arg) && !JS_IsString (arg))
        {
          goto expected_object_or_string;
        }

        *va_arg (ap, JSValue *) = arg;

        break;
      }
      case 'O':
      {
        gboolean is_nullable;

        is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (JS_IsNull (arg))
        {
          if (!is_nullable)
            goto expected_object;
        }
        else if (!JS_IsObject (arg))
        {
          goto expected_object;
        }

        *va_arg (ap, JSValue *) = arg;

        break;
      }
      case 'A':
      {
        gboolean is_nullable;

        is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (JS_IsNull (arg))
        {
          if (!is_nullable)
            goto expected_array;
        }
        else if (!JS_IsArray (ctx, arg))
        {
          goto expected_array;
        }

        *va_arg (ap, JSValue *) = arg;

        break;
      }
      case 'F':
      {
        JSValue func_js;
        gpointer func_c;
        gboolean accepts_pointer, is_expecting_object;

        accepts_pointer = t[1] == '*';
        if (accepts_pointer)
          t++;

        is_expecting_object = t[1] == '{';
        if (is_expecting_object)
          t += 2;

        if (is_expecting_object)
        {
          const gchar * next, * end, * t_end;

          if (!JS_IsObject (arg))
            goto expected_callback_object;

          do
          {
            gchar name[64];
            gsize length;
            gboolean is_optional;
            JSValue val;

            next = strchr (t, ',');
            end = strchr (t, '}');
            t_end = (next != NULL && next < end) ? next : end;
            length = t_end - t;
            strncpy (name, t, length);

            is_optional = name[length - 1] == '?';
            if (is_optional)
              name[length - 1] = '\0';
            else
              name[length] = '\0';

            val = JS_GetPropertyStr (ctx, arg, name);
            gum_quick_args_free_value_later (self, val);

            if (JS_IsFunction (ctx, val))
            {
              func_js = val;
              func_c = NULL;
            }
            else if (is_optional && JS_IsUndefined (val))
            {
              func_js = JS_NULL;
              func_c = NULL;
            }
            else if (accepts_pointer)
            {
              func_js = JS_NULL;
              if (!_gum_quick_native_pointer_get (ctx, val, core, &func_c))
                goto expected_callback_value;
            }
            else
            {
              goto expected_callback_value;
            }

            *va_arg (ap, JSValue *) = func_js;
            if (accepts_pointer)
              *va_arg (ap, gpointer *) = func_c;

            t = t_end + 1;
          }
          while (t_end != end);

          t--;
        }
        else
        {
          gboolean is_nullable;

          is_nullable = t[1] == '?';
          if (is_nullable)
            t++;

          if (JS_IsFunction (ctx, arg))
          {
            func_js = arg;
            func_c = NULL;
          }
          else if (is_nullable && JS_IsNull (arg))
          {
            func_js = arg;
            func_c = NULL;
          }
          else if (accepts_pointer)
          {
            func_js = JS_NULL;
            if (!_gum_quick_native_pointer_get (ctx, arg, core, &func_c))
              goto expected_function;
          }
          else
          {
            goto expected_function;
          }

          *va_arg (ap, JSValue *) = func_js;
          if (accepts_pointer)
            *va_arg (ap, gpointer *) = func_c;
        }

        break;
      }
      case 'B':
      {
        GBytes * bytes;
        gboolean is_fuzzy, is_nullable;

        is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;
        is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && JS_IsNull (arg))
        {
          bytes = NULL;
        }
        else
        {
          gboolean success;

          if (is_fuzzy)
            success = _gum_quick_bytes_parse (ctx, arg, core, &bytes);
          else
            success = _gum_quick_bytes_get (ctx, arg, core, &bytes);

          if (!success)
            goto propagate_exception;
        }

        gum_quick_args_free_bytes_later (self, bytes);

        *va_arg (ap, GBytes **) = bytes;

        break;
      }
      case 'C':
      {
        GumCpuContext * cpu_context;
        gboolean is_nullable;

        is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && JS_IsNull (arg))
          cpu_context = NULL;
        else if (!_gum_quick_cpu_context_get (ctx, arg, core, &cpu_context))
          goto propagate_exception;

        *va_arg (ap, GumCpuContext **) = cpu_context;

        break;
      }
      case 'M':
      {
        GumMatchPattern * pattern;

        if (JS_IsString (arg))
        {
          const char * str;

          str = JS_ToCString (ctx, arg);
          if (str == NULL)
            goto propagate_exception;

          pattern = gum_match_pattern_new_from_string (str);

          JS_FreeCString (ctx, str);

          if (pattern == NULL)
            goto invalid_pattern;
        }
        else if (JS_IsObject (arg))
        {
          pattern = JS_GetOpaque (arg, core->match_pattern_class);
          if (pattern == NULL)
            goto expected_pattern;

          gum_match_pattern_ref (pattern);
        }
        else
        {
          goto expected_pattern;
        }

        *va_arg (ap, GumMatchPattern **) = pattern;

        gum_quick_args_free_match_pattern_later (self, pattern);

        break;
      }
      default:
        g_assert_not_reached ();
    }

    arg_index++;
  }

  va_end (ap);

  return TRUE;

missing_argument:
  {
    error_message = "missing argument";
    goto propagate_exception;
  }
expected_object_or_string:
  {
    error_message = "expected an object or string";
    goto propagate_exception;
  }
expected_object:
  {
    error_message = "expected an object";
    goto propagate_exception;
  }
expected_array:
  {
    error_message = "expected an array";
    goto propagate_exception;
  }
expected_callback_object:
  {
    error_message = "expected an object containing callbacks";
    goto propagate_exception;
  }
expected_callback_value:
  {
    error_message = "expected a callback value";
    goto propagate_exception;
  }
expected_function:
  {
    error_message = "expected a function";
    goto propagate_exception;
  }
invalid_pattern:
  {
    error_message = "invalid match pattern";
    goto propagate_exception;
  }
expected_pattern:
  {
    error_message = "expected either a pattern string or a MatchPattern object";
    goto propagate_exception;
  }
propagate_exception:
  {
    va_end (ap);

    if (error_message != NULL)
      _gum_quick_throw_literal (ctx, error_message);

    return FALSE;
  }
}

GBytes *
_gum_quick_args_steal_bytes (GumQuickArgs * self,
                             GBytes * bytes)
{
  self->bytes = g_slist_remove (self->bytes, bytes);
  return bytes;
}

static void
gum_quick_args_free_value_later (GumQuickArgs * self,
                                 JSValue v)
{
  if (!JS_VALUE_HAS_REF_COUNT (v))
    return;

  if (self->values == NULL)
    self->values = g_array_sized_new (FALSE, FALSE, sizeof (JSValue), 4);

  g_array_append_val (self->values, v);
}

static void
gum_quick_args_free_cstring_later (GumQuickArgs * self,
                                   const char * s)
{
  if (s == NULL)
    return;

  self->cstrings = g_slist_prepend (self->cstrings, (gpointer) s);
}

static void
gum_quick_args_free_array_later (GumQuickArgs * self,
                                 GArray * a)
{
  if (a == NULL)
    return;

  self->arrays = g_slist_prepend (self->arrays, a);
}

static void
gum_quick_args_free_bytes_later (GumQuickArgs * self,
                                 GBytes * b)
{
  if (b == NULL)
    return;

  self->bytes = g_slist_prepend (self->bytes, b);
}

static void
gum_quick_args_free_match_pattern_later (GumQuickArgs * self,
                                         GumMatchPattern * p)
{
  if (p == NULL)
    return;

  self->match_patterns = g_slist_prepend (self->match_patterns, p);
}

gboolean
_gum_quick_string_get (JSContext * ctx,
                       JSValueConst val,
                       const char ** str)
{
  if (!JS_IsString (val))
    goto expected_string;

  *str = JS_ToCString (ctx, val);
  return *str != NULL;

expected_string:
  {
    _gum_quick_throw_literal (ctx, "expected a string");
    return FALSE;
  }
}

gboolean
_gum_quick_bytes_get (JSContext * ctx,
                      JSValueConst val,
                      GumQuickCore * core,
                      GBytes ** bytes)
{
  uint8_t * data;
  size_t size;
  JSValue exception;
  gboolean buffer_is_empty;
  gboolean is_array_buffer;
  JSValue element = JS_NULL;
  guint8 * tmp_array = NULL;

  data = JS_GetArrayBuffer (ctx, &size, val);

  exception = JS_GetException (ctx);
  buffer_is_empty = data == NULL && JS_IsNull (exception);
  JS_FreeValue (ctx, exception);

  is_array_buffer = data != NULL || buffer_is_empty;

  if (!is_array_buffer)
  {
    JSValue buf;
    size_t byte_offset, byte_length;

    buf = JS_GetTypedArrayBuffer (ctx, val, &byte_offset, &byte_length, NULL);
    if (!JS_IsException (buf))
    {
      *bytes = g_bytes_new (JS_GetArrayBuffer (ctx, &size, buf) + byte_offset,
          byte_length);

      JS_FreeValue (ctx, buf);

      return TRUE;
    }
    else
    {
      JS_FreeValue (ctx, JS_GetException (ctx));
    }
  }

  if (is_array_buffer)
  {
    *bytes = g_bytes_new (data, size);
  }
  else if (JS_IsArray (ctx, val))
  {
    guint n, i;

    if (!_gum_quick_array_get_length (ctx, val, core, &n))
      return FALSE;

    if (n >= GUM_MAX_JS_BYTE_ARRAY_LENGTH)
      goto array_too_large;

    tmp_array = g_malloc (n);

    for (i = 0; i != n; i++)
    {
      uint32_t u;

      element = JS_GetPropertyUint32 (ctx, val, i);
      if (JS_IsException (element))
        goto propagate_exception;

      if (JS_ToUint32 (ctx, &u, element) != 0)
        goto propagate_exception;

      tmp_array[i] = u;

      JS_FreeValue (ctx, element);
      element = JS_NULL;
    }

    *bytes = g_bytes_new_take (tmp_array, n);
  }
  else
  {
    goto expected_bytes;
  }

  return TRUE;

expected_bytes:
  {
    _gum_quick_throw_literal (ctx, "expected a buffer-like object");
    goto propagate_exception;
  }
array_too_large:
  {
    _gum_quick_throw_literal (ctx, "array too large, use ArrayBuffer instead");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, element);
    g_free (tmp_array);

    return FALSE;
  }
}

gboolean
_gum_quick_bytes_parse (JSContext * ctx,
                        JSValueConst val,
                        GumQuickCore * core,
                        GBytes ** bytes)
{
  if (JS_IsString (val))
  {
    const char * str;

    str = JS_ToCString (ctx, val);

    *bytes = g_bytes_new (str, strlen (str));

    JS_FreeCString (ctx, str);

    return TRUE;
  }

  return _gum_quick_bytes_get (ctx, val, core, bytes);
}

gboolean
_gum_quick_boolean_get (JSContext * ctx,
                        JSValueConst val,
                        gboolean * b)
{
  if (!JS_IsBool (val))
    goto expected_boolean;

  *b = JS_VALUE_GET_BOOL (val);
  return TRUE;

expected_boolean:
  {
    _gum_quick_throw_literal (ctx, "expected a boolean");
    return FALSE;
  }
}

gboolean
_gum_quick_int_get (JSContext * ctx,
                    JSValueConst val,
                    gint * i)
{
  if (JS_IsNumber (val))
  {
    int32_t v;

    if (JS_ToInt32 (ctx, &v, val) == 0)
    {
      *i = v;
      return TRUE;
    }
  }
  else if (JS_IsBigInt (ctx, val))
  {
    int64_t v;

    if (JS_ToInt64Ext (ctx, &v, val) == 0 &&
        v >= G_MININT &&
        v <= G_MAXINT)
    {
      *i = v;
      return TRUE;
    }
  }

  _gum_quick_throw_literal (ctx, "expected an integer");
  return FALSE;
}

gboolean
_gum_quick_uint_get (JSContext * ctx,
                     JSValueConst val,
                     guint * u)
{
  if (JS_IsNumber (val))
  {
    uint32_t v;

    if (JS_ToUint32 (ctx, &v, val) == 0)
    {
      *u = v;
      return TRUE;
    }
  }
  else if (JS_IsBigInt (ctx, val))
  {
    int64_t v;

    if (JS_ToInt64Ext (ctx, &v, val) == 0 &&
        v >= 0 &&
        v <= G_MAXUINT)
    {
      *u = v;
      return TRUE;
    }
  }

  _gum_quick_throw_literal (ctx, "expected an unsigned integer");
  return FALSE;
}

JSValue
_gum_quick_int64_new (JSContext * ctx,
                      gint64 i,
                      GumQuickCore * core)
{
  JSValue wrapper;
  GumQuickInt64 * i64;

  wrapper = JS_NewObjectClass (ctx, core->int64_class);

  i64 = g_slice_new (GumQuickInt64);
  i64->value = i;

  JS_SetOpaque (wrapper, i64);

  return wrapper;
}

gboolean
_gum_quick_int64_unwrap (JSContext * ctx,
                         JSValueConst val,
                         GumQuickCore * core,
                         GumQuickInt64 ** instance)
{
  return _gum_quick_unwrap (ctx, val, core->int64_class, core,
      (gpointer *) instance);
}

gboolean
_gum_quick_int64_get (JSContext * ctx,
                      JSValueConst val,
                      GumQuickCore * core,
                      gint64 * i)
{
  if (JS_IsNumber (val))
  {
    int64_t v;

    if (JS_ToInt64 (ctx, &v, val) != 0)
      return FALSE;

    *i = v;
  }
  else if (JS_IsBigInt (ctx, val))
  {
    if (JS_ToInt64Ext (ctx, i, val) != 0)
      return FALSE;
  }
  else
  {
    GumQuickInt64 * i64;

    if (!_gum_quick_int64_unwrap (ctx, val, core, &i64))
      return FALSE;

    *i = i64->value;
  }

  return TRUE;
}

gboolean
_gum_quick_int64_parse (JSContext * ctx,
                        JSValueConst val,
                        GumQuickCore * core,
                        gint64 * i)
{
  if (JS_IsString (val))
  {
    const gchar * value_as_string, * end;
    gboolean valid;

    value_as_string = JS_ToCString (ctx, val);

    if (g_str_has_prefix (value_as_string, "0x"))
    {
      *i = g_ascii_strtoll (value_as_string + 2, (gchar **) &end, 16);
      valid = end != value_as_string + 2;
    }
    else
    {
      *i = g_ascii_strtoll (value_as_string, (gchar **) &end, 10);
      valid = end != value_as_string;
    }

    JS_FreeCString (ctx, value_as_string);

    if (!valid)
      _gum_quick_throw_literal (ctx, "expected an integer");

    return valid;
  }

  return _gum_quick_int64_get (ctx, val, core, i);
}

JSValue
_gum_quick_uint64_new (JSContext * ctx,
                       guint64 u,
                       GumQuickCore * core)
{
  JSValue wrapper;
  GumQuickUInt64 * u64;

  wrapper = JS_NewObjectClass (ctx, core->uint64_class);

  u64 = g_slice_new (GumQuickUInt64);
  u64->value = u;

  JS_SetOpaque (wrapper, u64);

  return wrapper;
}

gboolean
_gum_quick_uint64_unwrap (JSContext * ctx,
                          JSValueConst val,
                          GumQuickCore * core,
                          GumQuickUInt64 ** instance)
{
  return _gum_quick_unwrap (ctx, val, core->uint64_class, core,
      (gpointer *) instance);
}

gboolean
_gum_quick_uint64_get (JSContext * ctx,
                       JSValueConst val,
                       GumQuickCore * core,
                       guint64 * u)
{
  if (JS_IsNumber (val))
  {
    double v;

    if (JS_ToFloat64 (ctx, &v, val) != 0)
      return FALSE;

    if (v < 0)
      goto expected_uint;

    *u = (guint64) v;
  }
  else if (JS_IsBigInt (ctx, val))
  {
    const gchar * str = JS_ToCString (ctx, val);

    *u = g_ascii_strtoull (str, NULL, 10);

    JS_FreeCString (ctx, str);
  }
  else
  {
    GumQuickUInt64 * u64;

    if (!_gum_quick_uint64_unwrap (ctx, val, core, &u64))
      return FALSE;

    *u = u64->value;
  }

  return TRUE;

expected_uint:
  {
    _gum_quick_throw_literal (ctx, "expected an unsigned integer");
    return FALSE;
  }
}

gboolean
_gum_quick_uint64_parse (JSContext * ctx,
                         JSValueConst val,
                         GumQuickCore * core,
                         guint64 * u)
{
  if (JS_IsString (val))
  {
    const gchar * value_as_string, * end;
    gboolean valid;

    value_as_string = JS_ToCString (ctx, val);

    if (g_str_has_prefix (value_as_string, "0x"))
      *u = g_ascii_strtoull (value_as_string + 2, (gchar **) &end, 16);
    else
      *u = g_ascii_strtoull (value_as_string, (gchar **) &end, 10);

    valid = end == value_as_string + strlen (value_as_string);

    JS_FreeCString (ctx, value_as_string);

    if (!valid)
      _gum_quick_throw_literal (ctx, "expected an unsigned integer");

    return valid;
  }

  return _gum_quick_uint64_get (ctx, val, core, u);
}

gboolean
_gum_quick_size_get (JSContext * ctx,
                     JSValueConst val,
                     GumQuickCore * core,
                     gsize * size)
{
  GumQuickUInt64 * u64;
  GumQuickInt64 * i64;

  if (JS_IsNumber (val))
  {
    double v;

    if (JS_ToFloat64 (ctx, &v, val) != 0)
      return FALSE;

    if (v < 0)
      goto expected_uint;

    *size = (gsize) v;
  }
  else if (JS_IsBigInt (ctx, val))
  {
    if (sizeof (gsize) == 4)
    {
      int64_t v;

      if (JS_ToInt64Ext (ctx, &v, val) != 0)
        return FALSE;

      *size = v;
    }
    else
    {
      const gchar * str = JS_ToCString (ctx, val);

      *size = g_ascii_strtoull (str, NULL, 10);

      JS_FreeCString (ctx, str);
    }
  }
  else if (_gum_quick_try_unwrap (val, core->uint64_class, core,
      (gpointer *) &u64))
  {
    *size = u64->value;
  }
  else if (_gum_quick_try_unwrap (val, core->int64_class, core,
      (gpointer *) &i64))
  {
    if (i64->value < 0)
      goto expected_uint;

    *size = i64->value;
  }
  else
  {
    goto expected_uint;
  }

  return TRUE;

expected_uint:
  {
    _gum_quick_throw_literal (ctx, "expected an unsigned integer");
    return FALSE;
  }
}

gboolean
_gum_quick_ssize_get (JSContext * ctx,
                      JSValueConst val,
                      GumQuickCore * core,
                      gssize * size)
{
  GumQuickInt64 * i64;
  GumQuickUInt64 * u64;

  if (JS_IsNumber (val) || JS_IsBigInt (ctx, val))
  {
    int64_t v;

    if (JS_ToInt64Ext (ctx, &v, val) != 0)
      goto expected_int;

    *size = v;
  }
  else if (_gum_quick_try_unwrap (val, core->int64_class, core,
      (gpointer *) &i64))
  {
    *size = i64->value;
  }
  else if (_gum_quick_try_unwrap (val, core->uint64_class, core,
      (gpointer *) &u64))
  {
    *size = u64->value;
  }
  else
  {
    goto expected_int;
  }

  return TRUE;

expected_int:
  {
    _gum_quick_throw_literal (ctx, "expected an integer");
    return FALSE;
  }
}

gboolean
_gum_quick_float64_get (JSContext * ctx,
                        JSValueConst val,
                        gdouble * d)
{
  if (JS_IsNumber (val))
  {
    double v;

    if (JS_ToFloat64 (ctx, &v, val) != 0)
      goto expected_number;

    *d = v;
  }
  else if (JS_IsBigInt (ctx, val))
  {
    int64_t v;

    if (JS_ToInt64Ext (ctx, &v, val) != 0)
      goto expected_number;

    *d = v;
  }
  else
  {
    goto expected_number;
  }

  return TRUE;

expected_number:
  {
    _gum_quick_throw_literal (ctx, "expected a number");
    return FALSE;
  }
}

JSValue
_gum_quick_enum_new (JSContext * ctx,
                     gint value,
                     GType type)
{
  JSValue result;
  GEnumClass * enum_class;
  GEnumValue * enum_value;

  enum_class = g_type_class_ref (type);

  enum_value = g_enum_get_value (enum_class, value);
  g_assert (enum_value != NULL);

  result = JS_NewString (ctx, enum_value->value_nick);

  g_type_class_unref (enum_class);

  return result;
}

gboolean
_gum_quick_enum_get (JSContext * ctx,
                     JSValueConst val,
                     GType type,
                     gint * value)
{
  gboolean success = FALSE;
  const char * str;
  GEnumClass * enum_class;
  GEnumValue * enum_value;

  str = JS_ToCString (ctx, val);
  if (str == NULL)
    return FALSE;

  enum_class = g_type_class_ref (type);

  enum_value = g_enum_get_value_by_nick (enum_class, str);
  if (enum_value != NULL)
  {
    *value = enum_value->value;
    success = TRUE;
  }
  else
  {
    GString * options;
    guint i;

    options = g_string_new ("");
    for (i = 0; i != enum_class->n_values; i++)
    {
      if (i != 0)
        g_string_append (options, ", ");
      g_string_append (options, enum_class->values[i].value_nick);
    }

    _gum_quick_throw (ctx, "invalid value: %s (expected one of: %s)", str,
        options->str);

    g_string_free (options, TRUE);
  }

  g_type_class_unref (enum_class);
  JS_FreeCString (ctx, str);

  return success;
}

JSValue
_gum_quick_native_pointer_new (JSContext * ctx,
                               gpointer ptr,
                               GumQuickCore * core)
{
  JSValue wrapper;
  GumQuickNativePointer * np;

  wrapper = JS_NewObjectClass (ctx, core->native_pointer_class);

  np = g_slice_new (GumQuickNativePointer);
  np->value = ptr;

  JS_SetOpaque (wrapper, np);

  return wrapper;
}

gboolean
_gum_quick_native_pointer_unwrap (JSContext * ctx,
                                  JSValueConst val,
                                  GumQuickCore * core,
                                  GumQuickNativePointer ** instance)
{
  return _gum_quick_unwrap (ctx, val, core->native_pointer_class, core,
      (gpointer *) instance);
}

gboolean
_gum_quick_native_pointer_get (JSContext * ctx,
                               JSValueConst val,
                               GumQuickCore * core,
                               gpointer * ptr)
{
  if (!_gum_quick_native_pointer_try_get (ctx, val, core, ptr))
  {
    _gum_quick_throw_literal (ctx, "expected a pointer");
    return FALSE;
  }

  return TRUE;
}

gboolean
_gum_quick_native_pointer_try_get (JSContext * ctx,
                                   JSValueConst val,
                                   GumQuickCore * core,
                                   gpointer * ptr)
{
  gboolean success = FALSE;
  GumQuickNativePointer * p;

  if (_gum_quick_try_unwrap (val, core->native_pointer_class, core,
      (gpointer *) &p))
  {
    *ptr = p->value;
    success = TRUE;
  }
  else if (JS_IsObject (val))
  {
    JSValue handle;

    handle = JS_GetProperty (ctx, val, GUM_QUICK_CORE_ATOM (core, handle));
    if (!JS_IsException (val))
    {
      if (_gum_quick_try_unwrap (handle, core->native_pointer_class, core,
          (gpointer *) &p))
      {
        *ptr = p->value;
        success = TRUE;
      }

      JS_FreeValue (ctx, handle);
    }
    else
    {
      JS_FreeValue (ctx, JS_GetException (ctx));
    }
  }

  return success;
}

gboolean
_gum_quick_native_pointer_parse (JSContext * ctx,
                                 JSValueConst val,
                                 GumQuickCore * core,
                                 gpointer * ptr)
{
  GumQuickUInt64 * u64;
  GumQuickInt64 * i64;

  if (_gum_quick_native_pointer_try_get (ctx, val, core, ptr))
    return TRUE;

  if (JS_IsString (val))
  {
    const gchar * ptr_as_string, * end;
    gboolean valid;

    ptr_as_string = JS_ToCString (ctx, val);

    if (g_str_has_prefix (ptr_as_string, "0x"))
    {
      *ptr = GSIZE_TO_POINTER (
          g_ascii_strtoull (ptr_as_string + 2, (gchar **) &end, 16));
    }
    else
    {
      *ptr = GSIZE_TO_POINTER (
          g_ascii_strtoull (ptr_as_string, (gchar **) &end, 10));
    }

    valid = end == ptr_as_string + strlen (ptr_as_string);

    JS_FreeCString (ctx, ptr_as_string);

    if (!valid)
      goto expected_pointer;
  }
  else if (JS_IsNumber (val))
  {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN || GLIB_SIZEOF_VOID_P == 8
    union
    {
      gpointer p;
      int64_t i;
    } v;
#else
    union
    {
      struct
      {
        gpointer _pad;
        gpointer p;
      };
      int64_t i;
    } v;
#endif

    JS_ToInt64 (ctx, &v.i, val);

    *ptr = v.p;
  }
  else if (_gum_quick_try_unwrap (val, core->uint64_class, core,
      (gpointer *) &u64))
  {
    *ptr = GSIZE_TO_POINTER (u64->value);
  }
  else if (_gum_quick_try_unwrap (val, core->int64_class, core,
      (gpointer *) &i64))
  {
    *ptr = GSIZE_TO_POINTER (i64->value);
  }
  else
  {
    goto expected_pointer;
  }

  return TRUE;

expected_pointer:
  {
    _gum_quick_throw_literal (ctx, "expected a pointer");
    return FALSE;
  }
}

JSValue
_gum_quick_native_resource_new (JSContext * ctx,
                                gpointer data,
                                GDestroyNotify notify,
                                GumQuickCore * core)
{
  JSValue wrapper;
  GumQuickNativeResource * res;
  GumQuickNativePointer * ptr;

  wrapper = JS_NewObjectClass (ctx, core->native_resource_class);

  res = g_slice_new (GumQuickNativeResource);
  ptr = &res->native_pointer;
  ptr->value = data;
  res->size = 0;
  res->owns_pages = FALSE;
  res->notify = notify;

  JS_SetOpaque (wrapper, res);

  return wrapper;
}

JSValue
_gum_quick_native_resource_new_from_pages (JSContext * ctx,
                                           gpointer data,
                                           gsize size,
                                           GumQuickCore * core)
{
  JSValue wrapper;
  GumQuickNativeResource * res;

  wrapper = _gum_quick_native_resource_new (ctx, data, NULL, core);

  res = JS_GetOpaque (wrapper, core->native_resource_class);
  res->size = size;
  res->owns_pages = TRUE;

  return wrapper;
}

JSValue
_gum_quick_kernel_resource_new (JSContext * ctx,
                                GumAddress data,
                                GumQuickKernelDestroyNotify notify,
                                GumQuickCore * core)
{
  JSValue wrapper;
  GumQuickKernelResource * res;
  GumQuickUInt64 * u64;

  wrapper = JS_NewObjectClass (ctx, core->kernel_resource_class);

  res = g_slice_new (GumQuickKernelResource);
  u64 = &res->u64;
  u64->value = data;
  res->notify = notify;

  JS_SetOpaque (wrapper, res);

  return wrapper;
}

JSValue
_gum_quick_cpu_context_new (JSContext * ctx,
                            GumCpuContext * handle,
                            GumQuickCpuContextAccess access,
                            GumQuickCore * core,
                            GumQuickCpuContext ** cpu_context)
{
  GumQuickCpuContext * cc;
  JSValue wrapper;

  wrapper = JS_NewObjectClass (ctx, core->cpu_context_class);

  cc = g_slice_new (GumQuickCpuContext);
  cc->wrapper = wrapper;
  cc->core = core;

  JS_SetOpaque (wrapper, cc);

  _gum_quick_cpu_context_reset (cc, handle, access);

  if (cpu_context != NULL)
    *cpu_context = cc;

  return wrapper;
}

void
_gum_quick_cpu_context_reset (GumQuickCpuContext * self,
                              GumCpuContext * handle,
                              GumQuickCpuContextAccess access)
{
  if (handle != NULL)
  {
    if (access == GUM_CPU_CONTEXT_READWRITE)
    {
      self->handle = handle;
    }
    else
    {
      memcpy (&self->storage, handle, sizeof (GumCpuContext));
      self->handle = &self->storage;
    }
  }
  else
  {
    self->handle = NULL;
  }

  self->access = access;
}

void
_gum_quick_cpu_context_make_read_only (GumQuickCpuContext * self)
{
  if (self->access == GUM_CPU_CONTEXT_READWRITE)
  {
    memcpy (&self->storage, self->handle, sizeof (GumCpuContext));
    self->handle = &self->storage;
    self->access = GUM_CPU_CONTEXT_READONLY;
  }
}

gboolean
_gum_quick_cpu_context_unwrap (JSContext * ctx,
                               JSValueConst val,
                               GumQuickCore * core,
                               GumQuickCpuContext ** instance)
{
  return _gum_quick_unwrap (ctx, val, core->cpu_context_class, core,
      (gpointer *) instance);
}

gboolean
_gum_quick_cpu_context_get (JSContext * ctx,
                            JSValueConst val,
                            GumQuickCore * core,
                            GumCpuContext ** cpu_context)
{
  GumQuickCpuContext * instance;

  if (!_gum_quick_cpu_context_unwrap (ctx, val, core, &instance))
    return FALSE;

  *cpu_context = instance->handle;
  return TRUE;
}

JSValue
_gum_quick_thread_state_new (JSContext * ctx,
                             GumThreadState state)
{
  return JS_NewString (ctx, gum_thread_state_to_string (state));
}

JSValue
_gum_quick_range_details_new (JSContext * ctx,
                              const GumRangeDetails * details,
                              GumQuickCore * core)
{
  const GumFileMapping * f = details->file;
  JSValue d;

  d = _gum_quick_memory_range_new (ctx, details->range, core);

  JS_DefinePropertyValue (ctx, d,
      GUM_QUICK_CORE_ATOM (core, protection),
      _gum_quick_page_protection_new (ctx, details->protection),
      JS_PROP_C_W_E);

  if (f != NULL)
  {
    JSValue file = JS_NewObject (ctx);

    JS_DefinePropertyValue (ctx, file,
        GUM_QUICK_CORE_ATOM (core, path),
        JS_NewString (ctx, f->path),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, file,
        GUM_QUICK_CORE_ATOM (core, offset),
        JS_NewInt64 (ctx, f->offset),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, file,
        GUM_QUICK_CORE_ATOM (core, size),
        JS_NewInt64 (ctx, f->size),
        JS_PROP_C_W_E);

    JS_DefinePropertyValue (ctx, d,
        GUM_QUICK_CORE_ATOM (core, file),
        file,
        JS_PROP_C_W_E);
  }

  return d;
}

JSValue
_gum_quick_memory_range_new (JSContext * ctx,
                             const GumMemoryRange * range,
                             GumQuickCore * core)
{
  JSValue r = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, base),
      _gum_quick_native_pointer_new (ctx,
          GSIZE_TO_POINTER (range->base_address), core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, size),
      JS_NewInt64 (ctx, range->size),
      JS_PROP_C_W_E);

  return r;
}

gboolean
_gum_quick_memory_ranges_get (JSContext * ctx,
                              JSValueConst val,
                              GumQuickCore * core,
                              GArray ** ranges)
{
  GArray * result = NULL;
  JSValue element = JS_NULL;
  GumMemoryRange range;

  if (JS_IsArray (ctx, val))
  {
    guint n, i;

    if (!_gum_quick_array_get_length (ctx, val, core, &n))
      return FALSE;

    result = g_array_sized_new (FALSE, FALSE, sizeof (GumMemoryRange), n);

    for (i = 0; i != n; i++)
    {
      element = JS_GetPropertyUint32 (ctx, val, i);
      if (JS_IsException (element))
        goto propagate_exception;

      if (!_gum_quick_memory_range_get (ctx, element, core, &range))
        goto propagate_exception;

      g_array_append_val (result, range);

      JS_FreeValue (ctx, element);
      element = JS_NULL;
    }
  }
  else if (_gum_quick_memory_range_get (ctx, val, core, &range))
  {
    result = g_array_sized_new (FALSE, FALSE, sizeof (GumMemoryRange), 1);
    g_array_append_val (result, range);
  }
  else
  {
    goto expected_array_of_ranges_or_range;
  }

  *ranges = result;
  return TRUE;

expected_array_of_ranges_or_range:
  {
    _gum_quick_throw_literal (ctx,
        "expected a range object or an array of range objects");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, element);
    if (result != NULL)
      g_array_free (result, TRUE);

    return FALSE;
  }
}

gboolean
_gum_quick_memory_range_get (JSContext * ctx,
                             JSValueConst val,
                             GumQuickCore * core,
                             GumMemoryRange * range)
{
  gboolean success = FALSE;
  JSValue v = JS_NULL;
  gpointer base;
  gsize size;

  v = JS_GetProperty (ctx, val, GUM_QUICK_CORE_ATOM (core, base));
  if (JS_IsException (v))
    goto expected_range;
  if (!_gum_quick_native_pointer_get (ctx, v, core, &base))
    goto expected_range;
  JS_FreeValue (ctx, v);

  v = JS_GetProperty (ctx, val, GUM_QUICK_CORE_ATOM (core, size));
  if (JS_IsException (v))
    goto expected_range;
  if (!_gum_quick_size_get (ctx, v, core, &size))
    goto expected_range;
  JS_FreeValue (ctx, v);

  v = JS_NULL;

  range->base_address = GUM_ADDRESS (base);
  range->size = size;

  success = TRUE;
  goto beach;

expected_range:
  {
    _gum_quick_throw_literal (ctx, "expected a range object");
    goto beach;
  }
beach:
  {
    JS_FreeValue (ctx, v);

    return success;
  }
}

JSValue
_gum_quick_page_protection_new (JSContext * ctx,
                                GumPageProtection prot)
{
  gchar str[4] = "---";

  if ((prot & GUM_PAGE_READ) != 0)
    str[0] = 'r';
  if ((prot & GUM_PAGE_WRITE) != 0)
    str[1] = 'w';
  if ((prot & GUM_PAGE_EXECUTE) != 0)
    str[2] = 'x';

  return JS_NewString (ctx, str);
}

gboolean
_gum_quick_page_protection_get (JSContext * ctx,
                                JSValueConst val,
                                GumPageProtection * prot)
{
  GumPageProtection p;
  const char * str = NULL;
  const char * ch;

  if (!JS_IsString (val))
    goto expected_protection;

  str = JS_ToCString (ctx, val);

  p = GUM_PAGE_NO_ACCESS;
  for (ch = str; *ch != '\0'; ch++)
  {
    switch (*ch)
    {
      case 'r':
        p |= GUM_PAGE_READ;
        break;
      case 'w':
        p |= GUM_PAGE_WRITE;
        break;
      case 'x':
        p |= GUM_PAGE_EXECUTE;
        break;
      case '-':
        break;
      default:
        goto expected_protection;
    }
  }

  JS_FreeCString (ctx, str);

  *prot = p;
  return TRUE;

expected_protection:
  {
    JS_FreeCString (ctx, str);

    _gum_quick_throw_literal (ctx,
        "expected a string specifying memory protection");
    return FALSE;
  }
}

JSValue
_gum_quick_memory_operation_new (JSContext * ctx,
                                 GumMemoryOperation operation)
{
  return JS_NewString (ctx, gum_memory_operation_to_string (operation));
}

gboolean
_gum_quick_array_get_length (JSContext * ctx,
                             JSValueConst array,
                             GumQuickCore * core,
                             guint * length)
{
  JSValue val;
  int res;
  uint32_t v;

  val = JS_GetProperty (ctx, array, GUM_QUICK_CORE_ATOM (core, length));
  if (JS_IsException (val))
    return FALSE;

  res = JS_ToUint32 (ctx, &v, val);

  JS_FreeValue (ctx, val);

  if (res != 0)
    return FALSE;

  *length = v;
  return TRUE;
}

void
_gum_quick_array_buffer_free (JSRuntime * rt,
                              void * opaque,
                              void * ptr)
{
  g_free (opaque);
}

gboolean
_gum_quick_process_match_result (JSContext * ctx,
                                 JSValue * val,
                                 GumQuickMatchResult * result)
{
  GumQuickMatchResult r = GUM_QUICK_MATCH_CONTINUE;
  JSValue v = *val;

  if (JS_IsString (v))
  {
    const gchar * str = JS_ToCString (ctx, v);
    if (strcmp (str, "stop") == 0)
      r = GUM_QUICK_MATCH_STOP;
    JS_FreeCString (ctx, str);
  }
  else if (JS_IsException (v))
  {
    r = GUM_QUICK_MATCH_ERROR;
  }

  JS_FreeValue (ctx, v);

  *val = JS_NULL;
  *result = r;

  return r == GUM_QUICK_MATCH_CONTINUE;
}

JSValue
_gum_quick_maybe_call_on_complete (JSContext * ctx,
                                   GumQuickMatchResult match_result,
                                   JSValue on_complete)
{
  JSValue val;

  if (match_result == GUM_QUICK_MATCH_ERROR)
    return JS_EXCEPTION;

  val = JS_Call (ctx, on_complete, JS_UNDEFINED, 0, NULL);
  if (JS_IsException (val))
    return JS_EXCEPTION;

  JS_FreeValue (ctx, val);

  return JS_UNDEFINED;
}

JSValue
_gum_quick_exception_details_new (JSContext * ctx,
                                  GumExceptionDetails * details,
                                  GumQuickCore * core,
                                  GumQuickCpuContext ** cpu_context)
{
  const GumExceptionMemoryDetails * md = &details->memory;
  JSValue d;
  gchar * message;

  message = gum_exception_details_to_string (details);
  d = _gum_quick_error_new (ctx, message, core);
  g_free (message);

  JS_DefinePropertyValue (ctx, d, GUM_QUICK_CORE_ATOM (core, type),
      JS_NewString (ctx, gum_exception_type_to_string (details->type)),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, d, GUM_QUICK_CORE_ATOM (core, address),
      _gum_quick_native_pointer_new (ctx, details->address, core),
      JS_PROP_C_W_E);

  if (md->operation != GUM_MEMOP_INVALID)
  {
    JSValue op = JS_NewError (ctx);

    JS_DefinePropertyValue (ctx, op, GUM_QUICK_CORE_ATOM (core, operation),
        _gum_quick_memory_operation_new (ctx, md->operation),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, op, GUM_QUICK_CORE_ATOM (core, address),
        _gum_quick_native_pointer_new (ctx, md->address, core),
        JS_PROP_C_W_E);

    JS_DefinePropertyValue (ctx, d, GUM_QUICK_CORE_ATOM (core, memory), op,
        JS_PROP_C_W_E);
  }

  JS_DefinePropertyValue (ctx, d, GUM_QUICK_CORE_ATOM (core, context),
      _gum_quick_cpu_context_new (ctx, &details->context,
          GUM_CPU_CONTEXT_READWRITE, core, cpu_context),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, d, GUM_QUICK_CORE_ATOM (core, nativeContext),
      _gum_quick_native_pointer_new (ctx, details->native_context, core),
      JS_PROP_C_W_E);

  return d;
}

JSValue
_gum_quick_error_new (JSContext * ctx,
                      const gchar * message,
                      GumQuickCore * core)
{
  JSValue error;

  error = JS_NewError (ctx);
  JS_SetProperty (ctx, error, GUM_QUICK_CORE_ATOM (core, message),
      JS_NewString (ctx, message));

  return error;
}

JSValue
_gum_quick_error_new_take_error (JSContext * ctx,
                                 GError ** error,
                                 GumQuickCore * core)
{
  JSValue result;
  GError * e;

  e = g_steal_pointer (error);
  if (e != NULL)
  {
    const gchar * m = e->message;
    GString * message;
    gboolean probably_starts_with_acronym;

    message = g_string_sized_new (strlen (m));

    probably_starts_with_acronym =
        g_unichar_isupper (g_utf8_get_char (m)) &&
        g_utf8_strlen (m, -1) >= 2 &&
        g_unichar_isupper (g_utf8_get_char (g_utf8_offset_to_pointer (m, 1)));

    if (probably_starts_with_acronym)
    {
      g_string_append (message, m);
    }
    else
    {
      g_string_append_unichar (message,
          g_unichar_tolower (g_utf8_get_char (m)));
      g_string_append (message, g_utf8_offset_to_pointer (m, 1));
    }

    result = _gum_quick_error_new (ctx, message->str, core);

    g_string_free (message, TRUE);
    g_error_free (e);
  }
  else
  {
    result = JS_NULL;
  }

  return result;
}

gboolean
_gum_quick_unwrap (JSContext * ctx,
                   JSValue val,
                   JSClassID klass,
                   GumQuickCore * core,
                   gpointer * instance)
{
  if (!_gum_quick_try_unwrap (val, klass, core, instance))
  {
    JS_ThrowTypeErrorInvalidClass (ctx, klass);
    return FALSE;
  }

  return TRUE;
}

gboolean
_gum_quick_try_unwrap (JSValue val,
                       JSClassID klass,
                       GumQuickCore * core,
                       gpointer * instance)
{
  gpointer result;
  JSClassID concrete_class;

  result = JS_GetAnyOpaque (val, &concrete_class);
  if (concrete_class == 0)
    return FALSE;

  if (concrete_class != klass)
  {
    JSClassID base_class = GPOINTER_TO_SIZE (g_hash_table_lookup (
        core->subclasses, GSIZE_TO_POINTER (concrete_class)));
    if (base_class != klass)
      return FALSE;
  }

  *instance = result;
  return TRUE;
}

void
_gum_quick_create_class (JSContext * ctx,
                         const JSClassDef * def,
                         GumQuickCore * core,
                         JSClassID * klass,
                         JSValue * prototype)
{
  JSClassID id;
  JSValue proto;

  id = gum_get_class_id_for_class_def (def);

  JS_NewClass (core->rt, id, def);

  proto = JS_NewObject (ctx);
  JS_SetClassProto (ctx, id, proto);

  *klass = id;
  *prototype = proto;
}

void
_gum_quick_create_subclass (JSContext * ctx,
                            const JSClassDef * def,
                            JSClassID parent_class,
                            JSValue parent_prototype,
                            GumQuickCore * core,
                            JSClassID * klass,
                            JSValue * prototype)
{
  JSClassID id;
  JSValue proto;

  id = gum_get_class_id_for_class_def (def);

  JS_NewClass (core->rt, id, def);

  proto = JS_NewObjectProto (ctx, parent_prototype);
  JS_SetClassProto (ctx, id, proto);

  g_hash_table_insert (core->subclasses, GSIZE_TO_POINTER (id),
      GSIZE_TO_POINTER (parent_class));

  *klass = id;
  *prototype = proto;
}

static JSClassID
gum_get_class_id_for_class_def (const JSClassDef * def)
{
  JSClassID id;

  G_LOCK (gum_class_ids);

  if (gum_class_ids == NULL)
  {
    gum_class_ids = g_hash_table_new (NULL, NULL);
    _gum_register_destructor (gum_deinit_class_ids);
  }

  id = GPOINTER_TO_UINT (g_hash_table_lookup (gum_class_ids, def));
  if (id == 0)
  {
    JS_NewClassID (&id);
    g_hash_table_insert (gum_class_ids, (gpointer) def, GUINT_TO_POINTER (id));
  }

  G_UNLOCK (gum_class_ids);

  return id;
}

static void
gum_deinit_class_ids (void)
{
  g_hash_table_unref (gum_class_ids);
}

JSValue
_gum_quick_throw (JSContext * ctx,
                  const gchar * format,
                  ...)
{
  JSValue result;
  va_list args;
  gchar * message;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  result = _gum_quick_throw_literal (ctx, message);
  g_free (message);
  va_end (args);

  return result;
}

JSValue
_gum_quick_throw_literal (JSContext * ctx,
                          const gchar * message)
{
  return JS_Throw (ctx,
      _gum_quick_error_new (ctx, message, JS_GetContextOpaque (ctx)));
}

JSValue
_gum_quick_throw_error (JSContext * ctx,
                        GError ** error)
{
  return JS_Throw (ctx,
      _gum_quick_error_new_take_error (ctx, error, JS_GetContextOpaque (ctx)));
}

JSValue
_gum_quick_throw_native (JSContext * ctx,
                         GumExceptionDetails * details,
                         GumQuickCore * core)
{
  JSValue d;
  GumQuickCpuContext * cc;

  d = _gum_quick_exception_details_new (ctx, details, core, &cc);
  _gum_quick_cpu_context_make_read_only (cc);

  return JS_Throw (ctx, d);
}

static const gchar *
gum_exception_type_to_string (GumExceptionType type)
{
  switch (type)
  {
    case GUM_EXCEPTION_ABORT: return "abort";
    case GUM_EXCEPTION_ACCESS_VIOLATION: return "access-violation";
    case GUM_EXCEPTION_GUARD_PAGE: return "guard-page";
    case GUM_EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal-instruction";
    case GUM_EXCEPTION_STACK_OVERFLOW: return "stack-overflow";
    case GUM_EXCEPTION_ARITHMETIC: return "arithmetic";
    case GUM_EXCEPTION_BREAKPOINT: return "breakpoint";
    case GUM_EXCEPTION_SINGLE_STEP: return "single-step";
    case GUM_EXCEPTION_SYSTEM: return "system";
    default: break;
  }

  return NULL;
}

static const gchar *
gum_thread_state_to_string (GumThreadState state)
{
  switch (state)
  {
    case GUM_THREAD_RUNNING: return "running";
    case GUM_THREAD_STOPPED: return "stopped";
    case GUM_THREAD_WAITING: return "waiting";
    case GUM_THREAD_UNINTERRUPTIBLE: return "uninterruptible";
    case GUM_THREAD_HALTED: return "halted";
    default: break;
  }

  return NULL;
}

static const gchar *
gum_memory_operation_to_string (GumMemoryOperation operation)
{
  switch (operation)
  {
    case GUM_MEMOP_INVALID: return "invalid";
    case GUM_MEMOP_READ: return "read";
    case GUM_MEMOP_WRITE: return "write";
    case GUM_MEMOP_EXECUTE: return "execute";
    default: break;
  }

  return NULL;
}
