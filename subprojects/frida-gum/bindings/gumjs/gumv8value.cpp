/*
 * Copyright (C) 2016-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8value.h"

#include <string.h>
#include <string>

#define GUM_MAX_SEND_ARRAY_LENGTH (1024 * 1024)

using namespace v8;

struct GumV8ArgsParseScope
{
  GumV8ArgsParseScope ()
    : committed (FALSE),
      strings (NULL),
      arrays (NULL),
      bytes (NULL),
      match_patterns (NULL)
  {
  }

  ~GumV8ArgsParseScope ()
  {
    if (!committed)
    {
      g_slist_foreach (strings, (GFunc) g_free, NULL);
      g_slist_foreach (arrays, (GFunc) g_array_unref, NULL);
      g_slist_foreach (bytes, (GFunc) g_bytes_unref, NULL);
      g_slist_foreach (match_patterns, (GFunc) gum_match_pattern_unref, NULL);
    }

    g_slist_free (strings);
    g_slist_free (arrays);
    g_slist_free (bytes);
    g_slist_free (match_patterns);
  }

  void
  commit ()
  {
    committed = TRUE;
  }

  gchar *
  strdup (const gchar * s)
  {
    auto result = g_strdup (s);
    strings = g_slist_prepend (strings, result);
    return result;
  }

  void
  add (GArray * a)
  {
    arrays = g_slist_prepend (arrays, a);
  }

  void
  add (GBytes * b)
  {
    bytes = g_slist_prepend (bytes, b);
  }

  void
  add (GumMatchPattern * p)
  {
    match_patterns = g_slist_prepend (match_patterns, p);
  }

  gboolean committed;
  GSList * strings;
  GSList * arrays;
  GSList * bytes;
  GSList * match_patterns;
};

struct GumCpuContextWrapper
{
  Global<Object> * instance;
  GumCpuContext * cpu_context;
};

static void gum_delete_heap_allocation (void * data, size_t length,
    void * deleter_data);

static void gum_v8_native_resource_on_weak_notify (
    const WeakCallbackInfo<GumV8NativeResource> & info);
static void gum_v8_kernel_resource_on_weak_notify (
    const WeakCallbackInfo<GumV8KernelResource> & info);

static const gchar * gum_exception_type_to_string (GumExceptionType type);

static void gum_cpu_context_on_weak_notify (
    const WeakCallbackInfo<GumCpuContextWrapper> & info);

gboolean
_gum_v8_args_parse (const GumV8Args * args,
                    const gchar * format,
                    ...)
{
  auto info = args->info;
  auto core = args->core;
  auto isolate = info->GetIsolate ();
  auto context = isolate->GetCurrentContext ();
  GumV8ArgsParseScope scope;
  va_list ap;
  int arg_index, arg_count = info->Length ();
  const gchar * t;
  gboolean is_required;

  va_start (ap, format);

  arg_index = 0;
  is_required = TRUE;
  for (t = format; *t != '\0'; t++)
  {
    if (*t == '|')
    {
      is_required = FALSE;
      continue;
    }

    Local<Value> arg;

    if (arg_index < arg_count)
        arg = (*info)[arg_index];

    if (arg_index >= arg_count || arg->IsUndefined ())
    {
      if (is_required)
      {
        _gum_v8_throw_ascii_literal (isolate, "missing argument");
        return FALSE;
      }
      else
      {
        break;
      }
    }

    switch (*t)
    {
      case 'i':
      {
        gint i;

        if (!_gum_v8_int_get (arg, &i, core))
          return FALSE;

        *va_arg (ap, gint *) = (gint) i;

        break;
      }
      case 'u':
      {
        guint u;

        if (!_gum_v8_uint_get (arg, &u, core))
          return FALSE;

        *va_arg (ap, guint *) = u;

        break;
      }
      case 'q':
      {
        gint64 i;

        gboolean is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        if (is_fuzzy)
        {
          if (!_gum_v8_int64_parse (arg, &i, core))
            return FALSE;
        }
        else
        {
          if (!_gum_v8_int64_get (arg, &i, core))
            return FALSE;
        }

        *va_arg (ap, gint64 *) = i;

        break;
      }
      case 'Q':
      {
        guint64 u;

        gboolean is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        if (is_fuzzy)
        {
          if (!_gum_v8_uint64_parse (arg, &u, core))
            return FALSE;
        }
        else
        {
          if (!_gum_v8_uint64_get (arg, &u, core))
            return FALSE;
        }

        *va_arg (ap, guint64 *) = u;

        break;
      }
      case 'z':
      {
        gssize value;

        if (!_gum_v8_ssize_get (arg, &value, core))
          return FALSE;

        *va_arg (ap, gssize *) = value;

        break;
      }
      case 'Z':
      {
        gsize value;

        if (!_gum_v8_size_get (arg, &value, core))
          return FALSE;

        *va_arg (ap, gsize *) = value;

        break;
      }
      case 'n':
      {
        if (!arg->IsNumber ())
        {
          _gum_v8_throw_ascii_literal (isolate, "expected a number");
          return FALSE;
        }

        *va_arg (ap, gdouble *) = arg.As<Number> ()->Value ();

        break;
      }
      case 't':
      {
        if (!arg->IsBoolean ())
        {
          _gum_v8_throw_ascii_literal (isolate, "expected a boolean");
          return FALSE;
        }

        *va_arg (ap, gboolean *) = arg.As<Boolean> ()->Value ();

        break;
      }
      case 'p':
      {
        gpointer ptr;

        gboolean is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        if (is_fuzzy)
        {
          if (!_gum_v8_native_pointer_parse (arg, &ptr, core))
            return FALSE;
        }
        else
        {
          if (!_gum_v8_native_pointer_get (arg, &ptr, core))
            return FALSE;
        }

        *va_arg (ap, gpointer *) = ptr;

        break;
      }
      case 'X':
      {
        if (!arg->IsExternal ())
        {
          _gum_v8_throw_ascii_literal (isolate, "expected an external pointer");
          return FALSE;
        }

        *va_arg (ap, gpointer *) = arg.As<External> ()->Value ();

        break;
      }
      case 's':
      {
        gchar * str;

        gboolean is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && arg->IsNull ())
        {
          str = NULL;
        }
        else if (arg->IsString ())
        {
          String::Utf8Value arg_utf8 (isolate, arg);
          str = scope.strdup (*arg_utf8);
        }
        else
        {
          _gum_v8_throw_ascii_literal (isolate, "expected a string");
          return FALSE;
        }

        *va_arg (ap, gchar **) = str;

        break;
      }
      case 'S':
      {
        if (!arg->IsString ())
        {
          _gum_v8_throw_ascii_literal (isolate, "expected a string");
          return FALSE;
        }

        String::Utf8Value arg_utf8 (isolate, arg);
        *va_arg (ap, std::string *) = *arg_utf8;

        break;
      }
      case 'r':
      {
        auto range = va_arg (ap, GumMemoryRange *);

        if (!_gum_v8_memory_range_get (arg, range, core))
          return FALSE;

        break;
      }
      case 'R':
      {
        auto ranges = _gum_v8_memory_ranges_get (arg, core);
        if (ranges == NULL)
          return FALSE;

        scope.add (ranges);

        *va_arg (ap, GArray **) = ranges;

        break;
      }
      case 'm':
      {
        GumPageProtection prot;

        if (!_gum_v8_page_protection_get (arg, &prot, core))
          return FALSE;

        *va_arg (ap, GumPageProtection *) = prot;

        break;
      }
      case 'V':
      {
        *va_arg (ap, Local<Value> *) = arg;

        break;
      }
      case 'O':
      {
        gboolean is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && arg->IsNull ())
        {
          *va_arg (ap, Local<Object> *) = Local<Object> ();
        }
        else if (arg->IsObject ())
        {
          *va_arg (ap, Local<Object> *) = arg.As<Object> ();
        }
        else
        {
          _gum_v8_throw_ascii_literal (isolate, "expected an object");
          return FALSE;
        }

        break;
      }
      case 'A':
      {
        gboolean is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (arg->IsArray ())
        {
          *va_arg (ap, Local<Array> *) = arg.As<Array> ();
        }
        else if (is_nullable && arg->IsNull ())
        {
          *va_arg (ap, Local<Array> *) = Local<Array> ();
        }
        else
        {
          _gum_v8_throw_ascii_literal (isolate, "expected an array");
          return FALSE;
        }

        break;
      }
      case 'F':
      {
        gboolean accepts_pointer = t[1] == '*';
        if (accepts_pointer)
          t++;

        gboolean is_expecting_object = t[1] == '{';
        if (is_expecting_object)
          t += 2;

        if (is_expecting_object)
        {
          if (!arg->IsObject ())
          {
            _gum_v8_throw_ascii_literal (isolate,
                "expected an object containing callbacks");
            return FALSE;
          }
          Local<Object> callbacks = arg.As<Object> ();

          const gchar * end, * t_end;

          do
          {
            gchar name[64];

            const gchar * next = strchr (t, ',');
            end = strchr (t, '}');
            t_end = (next != NULL && next < end) ? next : end;
            gsize length = t_end - t;
            strncpy (name, t, length);

            gboolean is_optional = name[length - 1] == '?';
            if (is_optional)
              name[length - 1] = '\0';
            else
              name[length] = '\0';

            Local<Function> func_js;
            gpointer func_c;

            Local<Value> value;
            if (!callbacks->Get (context,
                _gum_v8_string_new_ascii (isolate, name)).ToLocal (&value))
            {
              return FALSE;
            }
            if (value->IsFunction ())
            {
              func_js = value.As<Function> ();
              func_c = NULL;
            }
            else if (is_optional && value->IsUndefined ())
            {
              func_c = NULL;
            }
            else
            {
              auto native_pointer = Local<FunctionTemplate>::New (isolate,
                  *core->native_pointer);
              if (accepts_pointer && native_pointer->HasInstance (value))
              {
                func_c = GUMJS_NATIVE_POINTER_VALUE (value.As<Object> ());
              }
              else
              {
                _gum_v8_throw_ascii_literal (isolate,
                    "expected a callback value");
                return FALSE;
              }
            }

            *va_arg (ap, Local<Function> *) = func_js;
            if (accepts_pointer)
              *va_arg (ap, gpointer *) = func_c;

            t = t_end + 1;
          }
          while (t_end != end);

          t--;
        }
        else
        {
          gboolean is_nullable = t[1] == '?';
          if (is_nullable)
            t++;

          Local<Function> func_js;
          gpointer func_c;

          if (arg->IsFunction ())
          {
            func_js = arg.As<Function> ();
            func_c = NULL;
          }
          else if (is_nullable && arg->IsNull ())
          {
            func_c = NULL;
          }
          else
          {
            auto native_pointer = Local<FunctionTemplate>::New (isolate,
                *core->native_pointer);
            if (accepts_pointer && native_pointer->HasInstance (arg))
            {
              func_c = GUMJS_NATIVE_POINTER_VALUE (arg.As<Object> ());
            }
            else
            {
              _gum_v8_throw_ascii_literal (isolate,
                  "expected a function");
              return FALSE;
            }
          }

          *va_arg (ap, Local<Function> *) = func_js;
          if (accepts_pointer)
            *va_arg (ap, gpointer *) = func_c;
        }

        break;
      }
      case 'B':
      {
        GBytes * bytes;

        gboolean is_fuzzy = t[1] == '~';
        if (is_fuzzy)
          t++;

        gboolean is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && arg->IsNull ())
        {
          bytes = NULL;
        }
        else
        {
          if (is_fuzzy)
            bytes = _gum_v8_bytes_parse (arg, core);
          else
            bytes = _gum_v8_bytes_get (arg, core);
          if (bytes == NULL)
            return FALSE;

          scope.add (bytes);
        }

        *va_arg (ap, GBytes **) = bytes;

        break;
      }
      case 'C':
      {
        GumCpuContext * cpu_context;

        gboolean is_nullable = t[1] == '?';
        if (is_nullable)
          t++;

        if (is_nullable && arg->IsNull ())
        {
          cpu_context = NULL;
        }
        else if (!_gum_v8_cpu_context_get (arg, &cpu_context, core))
        {
          _gum_v8_throw_ascii_literal (isolate, "expected a CpuContext object");
          return FALSE;
        }

        *va_arg (ap, GumCpuContext **) = cpu_context;

        break;
      }
      case 'M':
      {
        GumMatchPattern * pattern;

        if (arg->IsString ())
        {
          String::Utf8Value arg_utf8 (isolate, arg);

          pattern = gum_match_pattern_new_from_string (*arg_utf8);
          if (pattern == NULL)
          {
            _gum_v8_throw_ascii_literal (isolate, "invalid match pattern");
            return FALSE;
          }
        }
        else
        {
          auto match_pattern = Local<FunctionTemplate>::New (core->isolate,
              *core->match_pattern);
          if (!match_pattern->HasInstance (arg))
          {
            _gum_v8_throw_ascii_literal (isolate,
                "expected either a pattern string or a MatchPattern object");
            return FALSE;
          }

          pattern = (GumMatchPattern *) arg.As<Object> ()
              ->GetInternalField (0).As<External> ()->Value ();

          gum_match_pattern_ref (pattern);
        }

        scope.add (pattern);

        *va_arg (ap, GumMatchPattern **) = pattern;

        break;
      }
      default:
        g_assert_not_reached ();
    }

    arg_index++;
  }

  va_end (ap);

  scope.commit ();

  return TRUE;
}

Local<String>
_gum_v8_string_new_ascii (Isolate * isolate,
                          const gchar * str)
{
  return String::NewFromOneByte (isolate, (const uint8_t *) str,
      NewStringType::kNormal).ToLocalChecked ();
}

Local<ArrayBuffer>
_gum_v8_array_buffer_new_take (Isolate * isolate,
                               gpointer data,
                               gsize size)
{
  return ArrayBuffer::New (isolate, ArrayBuffer::NewBackingStore (data, size,
      gum_delete_heap_allocation, data));
}

static void
gum_delete_heap_allocation (void * data,
                            size_t length,
                            void * deleter_data)
{
  g_free (deleter_data);
}

GBytes *
_gum_v8_bytes_get (Local<Value> value,
                   GumV8Core * core)
{
  auto result = _gum_v8_bytes_try_get (value, core);
  if (result == NULL)
  {
    _gum_v8_throw_ascii_literal (core->isolate, "unsupported data value");
    return NULL;
  }

  return result;
}

GBytes *
_gum_v8_bytes_parse (Local<Value> value,
                     GumV8Core * core)
{
  if (value->IsString ())
  {
    String::Utf8Value value_as_utf8 (core->isolate, value);
    auto value_as_string = *value_as_utf8;
    return g_bytes_new (value_as_string, strlen (value_as_string));
  }

  return _gum_v8_bytes_get (value, core);
}

GBytes *
_gum_v8_bytes_try_get (Local<Value> value,
                       GumV8Core * core)
{
  if (value->IsArrayBuffer ())
  {
    auto store = value.As<ArrayBuffer> ()->GetBackingStore ();
    return g_bytes_new (store->Data (), store->ByteLength ());
  }

  if (value->IsArrayBufferView ())
  {
    auto view = value.As<ArrayBufferView> ();

    auto data_length = view->ByteLength ();
    auto data = g_malloc (data_length);
    view->CopyContents (data, data_length);

    return g_bytes_new_take (data, data_length);
  }

  if (value->IsArray ())
  {
    auto array = value.As<Array> ();

    gsize data_length = array->Length ();
    if (data_length > GUM_MAX_SEND_ARRAY_LENGTH)
      return NULL;

    auto context = core->isolate->GetCurrentContext ();

    auto data = (guint8 *) g_malloc (data_length);
    gboolean data_valid = TRUE;

    for (gsize i = 0; i != data_length && data_valid; i++)
    {
      gboolean element_valid = FALSE;

      Local<Value> element_value;
      if (array->Get (context, i).ToLocal (&element_value))
      {
        auto element = element_value->Uint32Value (context);
        if (element.IsJust ())
        {
          data[i] = element.FromJust ();
          element_valid = TRUE;
        }
      }

      if (!element_valid)
        data_valid = FALSE;
    }

    if (!data_valid)
    {
      g_free (data);
      return NULL;
    }

    return g_bytes_new_take (data, data_length);
  }

  return NULL;
}

GumV8NativeResource *
_gum_v8_native_resource_new (gpointer data,
                             gsize size,
                             GDestroyNotify notify,
                             GumV8Core * core)
{
  auto resource = g_slice_new (GumV8NativeResource);
  resource->instance = new Global<Object> (core->isolate,
      _gum_v8_native_pointer_new (data, core));
  resource->instance->SetWeak (resource, gum_v8_native_resource_on_weak_notify,
      WeakCallbackType::kParameter);
  resource->data = data;
  resource->size = size;
  resource->owns_pages = FALSE;
  resource->notify = notify;
  resource->core = core;

  core->isolate->AdjustAmountOfExternalAllocatedMemory (size);

  g_hash_table_add (core->native_resources, resource);

  return resource;
}

GumV8NativeResource *
_gum_v8_native_resource_new_from_pages (gpointer data,
                                        gsize size,
                                        GumV8Core * core)
{
  auto resource = _gum_v8_native_resource_new (data, size, NULL, core);
  resource->owns_pages = TRUE;
  return resource;
}

void
_gum_v8_native_resource_free (GumV8NativeResource * resource)
{
  resource->core->isolate->AdjustAmountOfExternalAllocatedMemory (
      -((gssize) resource->size));

  delete resource->instance;
  if (resource->owns_pages)
    gum_memory_free (resource->data, resource->size);
  else if (resource->notify != NULL)
    resource->notify (resource->data);
  g_slice_free (GumV8NativeResource, resource);
}

static void
gum_v8_native_resource_on_weak_notify (
    const WeakCallbackInfo<GumV8NativeResource> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->core->native_resources, self);
}

GumV8KernelResource *
_gum_v8_kernel_resource_new (GumAddress data,
                             gsize size,
                             GumV8KernelDestroyNotify notify,
                             GumV8Core * core)
{
  auto resource = g_slice_new (GumV8KernelResource);
  resource->instance = new Global<Object> (core->isolate,
      _gum_v8_uint64_new (data, core));
  resource->instance->SetWeak (resource, gum_v8_kernel_resource_on_weak_notify,
      WeakCallbackType::kParameter);
  resource->data = data;
  resource->size = size;
  resource->notify = notify;
  resource->core = core;

  core->isolate->AdjustAmountOfExternalAllocatedMemory (size);

  g_hash_table_add (core->kernel_resources, resource);

  return resource;
}

void
_gum_v8_kernel_resource_free (GumV8KernelResource * resource)
{
  resource->core->isolate->AdjustAmountOfExternalAllocatedMemory (
      -((gssize) resource->size));

  delete resource->instance;
  if (resource->notify != NULL)
    resource->notify (resource->data);
  g_slice_free (GumV8KernelResource, resource);
}

static void
gum_v8_kernel_resource_on_weak_notify (
    const WeakCallbackInfo<GumV8KernelResource> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->core->kernel_resources, self);
}

gboolean
_gum_v8_int_get (Local<Value> value,
                 gint * i,
                 GumV8Core * core)
{
  if (value->IsNumber ())
  {
    double v = value.As<Number> ()->Value ();
    if (v >= G_MININT && v <= G_MAXINT)
    {
      *i = (gint) v;
      return TRUE;
    }
  }
  else if (value->IsBigInt ())
  {
    bool lossless;
    auto v = value.As<BigInt> ()->Int64Value (&lossless);
    if (lossless && v >= G_MININT && v <= G_MAXINT)
    {
      *i = v;
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (core->isolate, "expected an integer");
  return FALSE;
}

gboolean
_gum_v8_uint_get (Local<Value> value,
                  guint * u,
                  GumV8Core * core)
{
  if (value->IsNumber ())
  {
    double v = value.As<Number> ()->Value ();
    if (v >= 0 && v <= G_MAXUINT)
    {
      *u = (guint) v;
      return TRUE;
    }
  }
  else if (value->IsBigInt ())
  {
    bool lossless;
    auto v = value.As<BigInt> ()->Uint64Value (&lossless);
    if (lossless && v <= G_MAXUINT)
    {
      *u = v;
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (core->isolate, "expected an unsigned integer");
  return FALSE;
}

Local<Object>
_gum_v8_int64_new (gint64 value,
                   GumV8Core * core)
{
  auto int64_value (Local<Object>::New (core->isolate, *core->int64_value));
  auto int64_object (int64_value->Clone ());
  _gum_v8_int64_set_value (int64_object, value, core->isolate);
  return int64_object;
}

gboolean
_gum_v8_int64_get (Local<Value> value,
                   gint64 * i,
                   GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsNumber ())
  {
    *i = value->IntegerValue (isolate->GetCurrentContext ()).ToChecked ();
    return TRUE;
  }
  else if (value->IsBigInt ())
  {
    bool lossless;
    *i = value.As<BigInt> ()->Int64Value (&lossless);
    if (lossless)
      return TRUE;
  }
  else
  {
    auto int64 (Local<FunctionTemplate>::New (isolate, *core->int64));
    if (int64->HasInstance (value))
    {
      *i = _gum_v8_int64_get_value (value.As<Object> ());
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "expected an integer");
  return FALSE;
}

gboolean
_gum_v8_int64_parse (Local<Value> value,
                     gint64 * i,
                     GumV8Core * core)
{
  if (value->IsString ())
  {
    auto isolate = core->isolate;

    String::Utf8Value value_as_utf8 (isolate, value);
    auto value_as_string = *value_as_utf8;
    gchar * end;
    if (g_str_has_prefix (value_as_string, "0x"))
    {
      *i = g_ascii_strtoll (value_as_string + 2, &end, 16);
      if (end == value_as_string + 2)
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid hexadecimal string");
        return FALSE;
      }
    }
    else
    {
      *i = g_ascii_strtoll (value_as_string, &end, 10);
      if (end == value_as_string)
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid hexadecimal string");
        return FALSE;
      }
    }

    return TRUE;
  }

  return _gum_v8_int64_get (value, i, core);
}

Local<Object>
_gum_v8_uint64_new (guint64 value,
                    GumV8Core * core)
{
  auto uint64_value (Local<Object>::New (core->isolate, *core->uint64_value));
  auto uint64_object (uint64_value->Clone ());
  _gum_v8_uint64_set_value (uint64_object, value, core->isolate);
  return uint64_object;
}

gint64
_gum_v8_int64_get_value (Local<Object> object)
{
  return object->GetInternalField (0).As<BigInt> ()->Int64Value ();
}

void
_gum_v8_int64_set_value (Local<Object> object,
                         gint64 value,
                         Isolate * isolate)
{
  object->SetInternalField (0, BigInt::New (isolate, value));
}

gboolean
_gum_v8_uint64_get (Local<Value> value,
                    guint64 * u,
                    GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsNumber ())
  {
    double v = value->NumberValue (isolate->GetCurrentContext ()).ToChecked ();
    if (v >= 0)
    {
      *u = (guint64) v;
      return TRUE;
    }
  }
  else if (value->IsBigInt ())
  {
    bool lossless;
    *u = value.As<BigInt> ()->Uint64Value (&lossless);
    if (lossless)
      return TRUE;
  }
  else
  {
    auto uint64 (Local<FunctionTemplate>::New (isolate, *core->uint64));
    if (uint64->HasInstance (value))
    {
      *u = _gum_v8_uint64_get_value (value.As<Object> ());
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "expected an unsigned integer");
  return FALSE;
}

gboolean
_gum_v8_uint64_parse (Local<Value> value,
                      guint64 * u,
                      GumV8Core * core)
{
  if (value->IsString ())
  {
    auto isolate = core->isolate;

    String::Utf8Value value_as_utf8 (isolate, value);
    auto value_as_string = *value_as_utf8;
    gchar * end;
    if (g_str_has_prefix (value_as_string, "0x"))
    {
      *u = g_ascii_strtoull (value_as_string + 2, &end, 16);
      if (end == value_as_string + 2)
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid hexadecimal string");
        return FALSE;
      }
    }
    else
    {
      *u = g_ascii_strtoull (value_as_string, &end, 10);
      if (end == value_as_string)
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid hexadecimal string");
        return FALSE;
      }
    }

    return TRUE;
  }

  return _gum_v8_uint64_get (value, u, core);
}

guint64
_gum_v8_uint64_get_value (Local<Object> object)
{
  return object->GetInternalField (0).As<BigInt> ()->Uint64Value ();
}

void
_gum_v8_uint64_set_value (Local<Object> object,
                          guint64 value,
                          Isolate * isolate)
{
  object->SetInternalField (0, BigInt::NewFromUnsigned (isolate, value));
}

gboolean
_gum_v8_size_get (Local<Value> value,
                  gsize * size,
                  GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsNumber ())
  {
    double v = value->NumberValue (isolate->GetCurrentContext ()).ToChecked ();
    if (v >= 0)
    {
      *size = (gsize) v;
      return TRUE;
    }
  }
  else if (value->IsBigInt ())
  {
    bool lossless;
    auto v = value.As<BigInt> ()->Uint64Value (&lossless);
    if (lossless && v <= G_MAXSIZE)
    {
      *size = v;
      return TRUE;
    }
  }
  else
  {
    auto uint64 (Local<FunctionTemplate>::New (isolate, *core->uint64));
    if (uint64->HasInstance (value))
    {
      *size = (gsize) _gum_v8_uint64_get_value (value.As<Object> ());
      return TRUE;
    }

    auto int64 (Local<FunctionTemplate>::New (isolate, *core->int64));
    if (int64->HasInstance (value))
    {
      auto int64_value = _gum_v8_int64_get_value (value.As<Object> ());
      if (int64_value >= 0)
      {
        *size = (gsize) int64_value;
        return TRUE;
      }
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "expected an unsigned integer");
  return FALSE;
}

gboolean
_gum_v8_ssize_get (Local<Value> value,
                   gssize * size,
                   GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsNumber ())
  {
    *size = (gsize) value->IntegerValue (isolate->GetCurrentContext ())
        .ToChecked ();
    return TRUE;
  }
  else if (value->IsBigInt ())
  {
    bool lossless;
    auto v = value.As<BigInt> ()->Int64Value (&lossless);
    if (lossless && v >= G_MINSSIZE && v <= G_MAXSSIZE)
    {
      *size = v;
      return TRUE;
    }
  }
  else
  {
    Local<FunctionTemplate> int64 (Local<FunctionTemplate>::New (
        isolate, *core->int64));
    if (int64->HasInstance (value))
    {
      *size = (gssize) _gum_v8_int64_get_value (value.As<Object> ());
      return TRUE;
    }

    Local<FunctionTemplate> uint64 (Local<FunctionTemplate>::New (isolate,
        *core->uint64));
    if (uint64->HasInstance (value))
    {
      *size = (gssize) _gum_v8_uint64_get_value (value.As<Object> ());
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "expected an integer");
  return FALSE;
}

Local<String>
_gum_v8_enum_new (Isolate * isolate,
                  gint value,
                  GType type)
{
  auto enum_class = (GEnumClass *) g_type_class_ref (type);

  GEnumValue * enum_value = g_enum_get_value (enum_class, value);
  g_assert (enum_value != NULL);

  auto result = String::NewFromOneByte (isolate,
      (const uint8_t *) enum_value->value_nick,
      NewStringType::kNormal).ToLocalChecked ();

  g_type_class_unref (enum_class);

  return result;
}

gboolean
_gum_v8_enum_get (Local<Value> value,
                  GType type,
                  gint * result,
                  GumV8Core * core)
{
  gboolean success = FALSE;

  String::Utf8Value nick (core->isolate, value);
  if (*nick == nullptr)
    return FALSE;

  auto enum_class = (GEnumClass *) g_type_class_ref (type);

  GEnumValue * enum_value = g_enum_get_value_by_nick (enum_class, *nick);
  if (enum_value != NULL)
  {
    *result = enum_value->value;
    success = TRUE;
  }
  else
  {
    auto options = g_string_new ("");
    for (guint i = 0; i != enum_class->n_values; i++)
    {
      if (i != 0)
        g_string_append (options, ", ");
      g_string_append (options, enum_class->values[i].value_nick);
    }

    _gum_v8_throw (core->isolate, "invalid value: %s (expected one of: %s)",
        *nick, options->str);

    g_string_free (options, TRUE);
  }

  g_type_class_unref (enum_class);

  return success;
}

Local<Object>
_gum_v8_native_pointer_new (gpointer address,
                            GumV8Core * core)
{
  auto native_pointer_value (Local<Object>::New (core->isolate,
      *core->native_pointer_value));
  auto native_pointer_object (native_pointer_value->Clone ());
  native_pointer_object->SetInternalField (0,
      BigInt::NewFromUnsigned (core->isolate, GPOINTER_TO_SIZE (address)));
  return native_pointer_object;
}

gboolean
_gum_v8_native_pointer_get (Local<Value> value,
                            gpointer * ptr,
                            GumV8Core * core)
{
  auto isolate = core->isolate;
  gboolean success = FALSE;

  auto native_pointer = Local<FunctionTemplate>::New (isolate,
      *core->native_pointer);
  if (native_pointer->HasInstance (value))
  {
    *ptr = GUMJS_NATIVE_POINTER_VALUE (value.As<Object> ());
    success = TRUE;
  }
  else
  {
    auto context = isolate->GetCurrentContext ();

    /* Cannot use isObject() here as that returns false for proxies */
    MaybeLocal<Object> maybe_obj;
    {
      TryCatch trycatch (isolate);
      maybe_obj = value->ToObject (context);
      trycatch.Reset ();
    }

    Local<Object> obj;
    if (maybe_obj.ToLocal (&obj))
    {
      auto handle_key (Local<String>::New (isolate, *core->handle_key));
      if (obj->Has (context, handle_key).FromJust ())
      {
        auto handle = obj->Get (context, handle_key).ToLocalChecked ();
        if (native_pointer->HasInstance (handle))
        {
          *ptr = GUMJS_NATIVE_POINTER_VALUE (handle.As<Object> ());
          success = TRUE;
        }
      }
    }
  }

  if (!success)
  {
    _gum_v8_throw_ascii_literal (isolate, "expected a pointer");
    return FALSE;
  }

  return TRUE;
}

gboolean
_gum_v8_native_pointer_parse (Local<Value> value,
                              gpointer * ptr,
                              GumV8Core * core)
{
  auto isolate = core->isolate;

  if (value->IsString ())
  {
    String::Utf8Value ptr_as_utf8 (isolate, value);
    auto ptr_as_string = *ptr_as_utf8;
    gchar * endptr;
    if (g_str_has_prefix (ptr_as_string, "0x"))
    {
      *ptr = GSIZE_TO_POINTER (
          g_ascii_strtoull (ptr_as_string + 2, &endptr, 16));
      if (endptr == ptr_as_string + 2)
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid hexadecimal string");
        return FALSE;
      }
    }
    else
    {
      *ptr = GSIZE_TO_POINTER (g_ascii_strtoull (ptr_as_string, &endptr, 10));
      if (endptr == ptr_as_string)
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid decimal string");
        return FALSE;
      }
    }

    return TRUE;
  }
  else if (value->IsNumber ())
  {
    double number = value.As<Number> ()->Value ();

    if (number < 0)
    {
      union
      {
        gpointer p;
        gint64 i;
      } v;

      v.i = (gint64) number;

      *ptr = v.p;
      return TRUE;
    }

    *ptr = GSIZE_TO_POINTER ((guint64) number);
    return TRUE;
  }
  else if (value->IsBigInt ())
  {
    *ptr = GSIZE_TO_POINTER (value.As<BigInt> ()->Uint64Value ());
    return TRUE;
  }
  else
  {
    auto uint64 (Local<FunctionTemplate>::New (isolate, *core->uint64));
    if (uint64->HasInstance (value))
    {
      *ptr = GSIZE_TO_POINTER (_gum_v8_uint64_get_value (value.As<Object> ()));
      return TRUE;
    }

    auto int64 (Local<FunctionTemplate>::New (isolate, *core->int64));
    if (int64->HasInstance (value))
    {
      *ptr = GSIZE_TO_POINTER (_gum_v8_int64_get_value (value.As<Object> ()));
      return TRUE;
    }
  }

  return _gum_v8_native_pointer_get (value, ptr, core);
}

void
_gum_v8_throw (Isolate * isolate,
               const gchar * format,
               ...)
{
  va_list args;
  va_start (args, format);

  auto message = g_strdup_vprintf (format, args);
  _gum_v8_throw_literal (isolate, message);
  g_free (message);

  va_end (args);
}

void
_gum_v8_throw_literal (Isolate * isolate,
                       const gchar * message)
{
  isolate->ThrowException (Exception::Error (
      String::NewFromUtf8 (isolate, message).ToLocalChecked ()));
}

void
_gum_v8_throw_ascii (Isolate * isolate,
                     const gchar * format,
                     ...)
{
  va_list args;
  va_start (args, format);

  auto message = g_strdup_vprintf (format, args);
  _gum_v8_throw_ascii_literal (isolate, message);
  g_free (message);

  va_end (args);
}

void
_gum_v8_throw_ascii_literal (Isolate * isolate,
                             const gchar * message)
{
  isolate->ThrowException (Exception::Error (
      _gum_v8_string_new_ascii (isolate, message)));
}

void
_gum_v8_throw_native (GumExceptionDetails * details,
                      GumV8Core * core)
{
  Local<Object> ex, context;
  _gum_v8_parse_exception_details (details, ex, context, core);
  _gum_v8_cpu_context_free_later (new Global<Object> (core->isolate, context),
      core);
  core->isolate->ThrowException (ex);
}

gboolean
_gum_v8_maybe_throw (Isolate * isolate,
                     GError ** error)
{
  auto value = _gum_v8_error_new_take_error (isolate, error);
  if (value->IsNull ())
    return FALSE;
  isolate->ThrowException (value);
  return TRUE;
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
    default:
      break;
  }

  g_assert_not_reached ();
}

Local<Object>
_gum_v8_cpu_context_new_immutable (const GumCpuContext * cpu_context,
                                   GumV8Core * core)
{
  auto isolate = core->isolate;
  auto cpu_context_value (Local<Object>::New (isolate,
      *core->cpu_context_value));
  auto cpu_context_object (cpu_context_value->Clone ());
  cpu_context_object->SetAlignedPointerInInternalField (0,
      (void *) cpu_context);
  const bool is_mutable = false;
  cpu_context_object->SetInternalField (1, Boolean::New (isolate, is_mutable));
  return cpu_context_object;
}

Local<Object>
_gum_v8_cpu_context_new_mutable (GumCpuContext * cpu_context,
                                 GumV8Core * core)
{
  auto isolate = core->isolate;
  auto cpu_context_value (Local<Object>::New (isolate,
      *core->cpu_context_value));
  auto cpu_context_object (cpu_context_value->Clone ());
  cpu_context_object->SetAlignedPointerInInternalField (0, cpu_context);
  const bool is_mutable = true;
  cpu_context_object->SetInternalField (1, Boolean::New (isolate, is_mutable));
  return cpu_context_object;
}

void
_gum_v8_cpu_context_free_later (Global<Object> * cpu_context,
                                GumV8Core * core)
{
  auto isolate = core->isolate;

  auto instance (Local<Object>::New (isolate, *cpu_context));
  auto original =
      (GumCpuContext *) instance->GetAlignedPointerFromInternalField (0);
  auto copy = g_slice_dup (GumCpuContext, original);
  instance->SetAlignedPointerInInternalField (0, copy);
  const bool is_mutable = false;
  instance->SetInternalField (1, Boolean::New (isolate, is_mutable));

  auto wrapper = g_slice_new (GumCpuContextWrapper);
  wrapper->instance = cpu_context;
  wrapper->cpu_context = copy;

  cpu_context->SetWeak (wrapper, gum_cpu_context_on_weak_notify,
      WeakCallbackType::kParameter);
}

static void
gum_cpu_context_on_weak_notify (
    const WeakCallbackInfo<GumCpuContextWrapper> & info)
{
  auto wrapper = info.GetParameter ();

  delete wrapper->instance;

  g_slice_free (GumCpuContext, wrapper->cpu_context);

  g_slice_free (GumCpuContextWrapper, wrapper);
}

gboolean
_gum_v8_cpu_context_get (Local<Value> value,
                         GumCpuContext ** context,
                         GumV8Core * core)
{
  auto cpu_context (Local<FunctionTemplate>::New (core->isolate,
      *core->cpu_context));
  if (!cpu_context->HasInstance (value))
  {
    _gum_v8_throw_ascii_literal (core->isolate, "expected a CpuContext object");
    return FALSE;
  }
  *context = GUMJS_CPU_CONTEXT_VALUE (value.As<Object> ());

  return TRUE;
}

void
_gum_v8_parse_exception_details (GumExceptionDetails * details,
                                 Local<Object> & exception,
                                 Local<Object> & cpu_context,
                                 GumV8Core * core)
{
  auto message = gum_exception_details_to_string (details);
  auto ex = Exception::Error (
      String::NewFromUtf8 (core->isolate, message).ToLocalChecked ())
      .As<Object> ();
  g_free (message);

  _gum_v8_object_set_ascii (ex, "type",
      gum_exception_type_to_string (details->type), core);
  _gum_v8_object_set_pointer (ex, "address", details->address, core);

  const GumExceptionMemoryDetails * md = &details->memory;
  if (md->operation != GUM_MEMOP_INVALID)
  {
    auto memory (Object::New (core->isolate));
    _gum_v8_object_set_ascii (memory, "operation",
        _gum_v8_memory_operation_to_string (md->operation), core);
    _gum_v8_object_set_pointer (memory, "address", md->address, core);
    _gum_v8_object_set (ex, "memory", memory, core);
  }

  auto context = _gum_v8_cpu_context_new_mutable (&details->context, core);
  _gum_v8_object_set (ex, "context", context, core);
  _gum_v8_object_set_pointer (ex, "nativeContext", details->native_context,
      core);

  exception = ex;
  cpu_context = context;
}

Local<Value>
_gum_v8_error_new_take_error (Isolate * isolate,
                              GError ** error)
{
  Local<Value> result;

  auto e = (GError *) g_steal_pointer (error);
  if (e != NULL)
  {
    const gchar * m = e->message;
    GString * message = g_string_sized_new (strlen (m));

    bool probably_starts_with_acronym =
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

    result = Exception::Error (
        String::NewFromUtf8 (isolate, message->str).ToLocalChecked ());

    g_string_free (message, TRUE);
    g_error_free (e);
  }
  else
  {
    result = Null (isolate);
  }

  return result;
}

gchar *
_gum_v8_error_get_message (Isolate * isolate,
                           Local<Value> error)
{
  auto obj = error.As<Object> ();
  auto message = obj->Get (isolate->GetCurrentContext (),
        _gum_v8_string_new_ascii (isolate, "message"))
      .ToLocalChecked ()
      .As<String> ();
  String::Utf8Value message_str (isolate, message);
  const char * m = *message_str;
  auto length = strlen (m);

  auto result = g_string_sized_new (length);
  if (length >= 1)
    g_string_append_unichar (result, g_unichar_toupper (g_utf8_get_char (m)));
  if (length >= 2)
    g_string_append (result, g_utf8_offset_to_pointer (m, 1));
  return g_string_free (result, FALSE);
}

const gchar *
_gum_v8_thread_state_to_string (GumThreadState state)
{
  switch (state)
  {
    case GUM_THREAD_RUNNING: return "running";
    case GUM_THREAD_STOPPED: return "stopped";
    case GUM_THREAD_WAITING: return "waiting";
    case GUM_THREAD_UNINTERRUPTIBLE: return "uninterruptible";
    case GUM_THREAD_HALTED: return "halted";
    default:
      break;
  }

  g_assert_not_reached ();
}

const gchar *
_gum_v8_memory_operation_to_string (GumMemoryOperation operation)
{
  switch (operation)
  {
    case GUM_MEMOP_INVALID: return "invalid";
    case GUM_MEMOP_READ: return "read";
    case GUM_MEMOP_WRITE: return "write";
    case GUM_MEMOP_EXECUTE: return "execute";
    default:
      g_assert_not_reached ();
  }
}

gboolean
_gum_v8_object_set (Local<Object> object,
                    const gchar * key,
                    Local<Value> value,
                    GumV8Core * core)
{
  auto success = object->Set (core->isolate->GetCurrentContext (),
      _gum_v8_string_new_ascii (core->isolate, key), value);
  return success.IsJust ();
}

gboolean
_gum_v8_object_set_int (Local<Object> object,
                        const gchar * key,
                        gint value,
                        GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      Integer::New (core->isolate, value),
      core);
}

gboolean
_gum_v8_object_set_uint (Local<Object> object,
                         const gchar * key,
                         guint value,
                         GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      Integer::NewFromUnsigned (core->isolate, value),
      core);
}

gboolean
_gum_v8_object_set_pointer (Local<Object> object,
                            const gchar * key,
                            gpointer value,
                            GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      _gum_v8_native_pointer_new (value, core),
      core);
}

gboolean
_gum_v8_object_set_pointer (Local<Object> object,
                            const gchar * key,
                            GumAddress value,
                            GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (value), core),
      core);
}

gboolean
_gum_v8_object_set_uint64 (Local<Object> object,
                            const gchar * key,
                            GumAddress value,
                            GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      _gum_v8_uint64_new (value, core),
      core);
}

gboolean
_gum_v8_object_set_enum (Local<Object> object,
                         const gchar * key,
                         gint value,
                         GType type,
                         GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      _gum_v8_enum_new (core->isolate, value, type),
      core);
}

gboolean
_gum_v8_object_set_ascii (Local<Object> object,
                          const gchar * key,
                          const gchar * value,
                          GumV8Core * core)
{
  return _gum_v8_object_set (object, key,
      _gum_v8_string_new_ascii (core->isolate, value), core);
}

gboolean
_gum_v8_object_set_utf8 (Local<Object> object,
                         const gchar * key,
                         const gchar * value,
                         GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      String::NewFromUtf8 (core->isolate, value).ToLocalChecked (),
      core);
}

gboolean
_gum_v8_object_set_page_protection (Local<Object> object,
                                    const gchar * key,
                                    GumPageProtection prot,
                                    GumV8Core * core)
{
  return _gum_v8_object_set (object,
      key,
      _gum_v8_page_protection_new (core->isolate, prot),
      core);
}

Local<Object>
_gum_v8_range_details_new (const GumRangeDetails * details,
                           GumV8Core * core)
{
  auto isolate = core->isolate;

  auto range = Object::New (isolate);
  _gum_v8_object_set_pointer (range, "base", details->range->base_address,
      core);
  _gum_v8_object_set_uint (range, "size", details->range->size, core);
  _gum_v8_object_set_page_protection (range, "protection", details->protection,
      core);

  auto f = details->file;
  if (f != NULL)
  {
    auto file = Object::New (isolate);
    _gum_v8_object_set_utf8 (file, "path", f->path, core);
    _gum_v8_object_set_uint (file, "offset", f->offset, core);
    _gum_v8_object_set_uint (file, "size", f->size, core);
    _gum_v8_object_set (range, "file", file, core);
  }

  return range;
}

GArray *
_gum_v8_memory_ranges_get (Local<Value> value,
                           GumV8Core * core)
{
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  if (value->IsArray ())
  {
    auto range_values = value.As<Array> ();

    uint32_t length = range_values->Length ();
    auto ranges =
        g_array_sized_new (FALSE, FALSE, sizeof (GumMemoryRange), length);
    for (uint32_t i = 0; i != length; i++)
    {
      Local<Value> range_value;
      GumMemoryRange range;
      if (!range_values->Get (context, i).ToLocal (&range_value) ||
          !_gum_v8_memory_range_get (range_value, &range, core))
      {
        g_array_free (ranges, TRUE);
        return NULL;
      }
      g_array_append_val (ranges, range);
    }
    return ranges;
  }
  else if (value->IsObject ())
  {
    GumMemoryRange range;
    if (!_gum_v8_memory_range_get (value.As<Object> (), &range, core))
      return NULL;

    auto ranges = g_array_sized_new (FALSE, FALSE, sizeof (GumMemoryRange), 1);
    g_array_append_val (ranges, range);
    return ranges;
  }
  else
  {
    _gum_v8_throw_ascii_literal (isolate,
        "expected a range object or an array of range objects");
    return NULL;
  }
}

gboolean
_gum_v8_memory_range_get (Local<Value> value,
                          GumMemoryRange * range,
                          GumV8Core * core)
{
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  if (!value->IsObject ())
  {
    _gum_v8_throw_ascii_literal (isolate, "expected a range object");
    return FALSE;
  }
  auto object = value.As<Object> ();

  Local<Value> base_value;
  if (!object->Get (context, _gum_v8_string_new_ascii (isolate, "base"))
      .ToLocal (&base_value))
    return FALSE;

  gpointer base;
  if (!_gum_v8_native_pointer_get (base_value, &base, core))
    return FALSE;

  Local<Value> size_value;
  if (!object->Get (context, _gum_v8_string_new_ascii (isolate, "size"))
      .ToLocal (&size_value))
    return FALSE;
  if (!size_value->IsNumber ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "range object has an invalid or missing size property");
    return FALSE;
  }

  range->base_address = GUM_ADDRESS (base);
  range->size = size_value.As<Number> ()->Uint32Value (context).ToChecked ();
  return TRUE;
}

v8::Local<v8::String>
_gum_v8_page_protection_new (v8::Isolate * isolate,
                             GumPageProtection prot)
{
  gchar prot_str[4] = "---";

  if ((prot & GUM_PAGE_READ) != 0)
    prot_str[0] = 'r';
  if ((prot & GUM_PAGE_WRITE) != 0)
    prot_str[1] = 'w';
  if ((prot & GUM_PAGE_EXECUTE) != 0)
    prot_str[2] = 'x';

  return _gum_v8_string_new_ascii (isolate, prot_str);
}

gboolean
_gum_v8_page_protection_get (Local<Value> prot_val,
                             GumPageProtection * prot,
                             GumV8Core * core)
{
  auto isolate = core->isolate;

  if (!prot_val->IsString ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "expected a string specifying memory protection");
    return FALSE;
  }
  String::Utf8Value prot_str (isolate, prot_val);

  *prot = GUM_PAGE_NO_ACCESS;
  for (const gchar * ch = *prot_str; *ch != '\0'; ch++)
  {
    switch (*ch)
    {
      case 'r':
        *prot |= GUM_PAGE_READ;
        break;
      case 'w':
        *prot |= GUM_PAGE_WRITE;
        break;
      case 'x':
        *prot |= GUM_PAGE_EXECUTE;
        break;
      case '-':
        break;
      default:
        _gum_v8_throw_ascii_literal (isolate, "invalid character in memory "
            "protection specifier string");
        return FALSE;
    }
  }

  return TRUE;
}

Local<ObjectTemplate>
_gum_v8_create_module (const gchar * name,
                       Local<ObjectTemplate> scope,
                       Isolate * isolate)
{
  auto module = ObjectTemplate::New (isolate);
  scope->Set (_gum_v8_string_new_ascii (isolate, name), module);
  return module;
}

void
_gum_v8_module_add (Local<External> module,
                    Local<ObjectTemplate> object,
                    const GumV8Property * properties,
                    Isolate * isolate)
{
  auto prop = properties;
  while (prop->name != NULL)
  {
    object->SetAccessor (_gum_v8_string_new_ascii (isolate, prop->name),
        prop->getter, prop->setter, module);
    prop++;
  }
}

void
_gum_v8_module_add (Local<External> module,
                    Local<ObjectTemplate> object,
                    const GumV8Function * functions,
                    Isolate * isolate)
{
  auto func = functions;
  while (func->name != NULL)
  {
    object->Set (_gum_v8_string_new_ascii (isolate, func->name),
        FunctionTemplate::New (isolate, func->callback, module));
    func++;
  }
}

Local<FunctionTemplate>
_gum_v8_create_class (const gchar * name,
                      FunctionCallback ctor,
                      Local<ObjectTemplate> scope,
                      Local<External> module,
                      Isolate * isolate)
{
  auto klass = FunctionTemplate::New (isolate, ctor, module);
  auto name_value = _gum_v8_string_new_ascii (isolate, name);
  klass->SetClassName (name_value);
  klass->InstanceTemplate ()->SetInternalFieldCount (1);
  scope->Set (name_value, klass);
  return klass;
}

void
_gum_v8_class_add_static (Local<FunctionTemplate> klass,
                          const GumV8Property * properties,
                          Local<External> module,
                          Isolate * isolate)
{
  auto prop = properties;
  while (prop->name != NULL)
  {
    klass->SetNativeDataProperty (
        _gum_v8_string_new_ascii (isolate, prop->name),
        prop->getter, prop->setter, module);
    prop++;
  }
}

void
_gum_v8_class_add_static (Local<FunctionTemplate> klass,
                          const GumV8Function * functions,
                          Local<External> module,
                          Isolate * isolate)
{
  auto func = functions;
  while (func->name != NULL)
  {
    klass->Set (_gum_v8_string_new_ascii (isolate, func->name),
        FunctionTemplate::New (isolate, func->callback, module));
    func++;
  }
}

void
_gum_v8_class_add (Local<FunctionTemplate> klass,
                   const GumV8Property * properties,
                   Local<External> module,
                   Isolate * isolate)
{
  auto object = klass->InstanceTemplate ();

  auto prop = properties;
  while (prop->name != NULL)
  {
    object->SetAccessor (_gum_v8_string_new_ascii (isolate, prop->name),
        prop->getter, prop->setter, module);
    prop++;
  }
}

void
_gum_v8_class_add (Local<FunctionTemplate> klass,
                   const GumV8Function * functions,
                   Local<External> module,
                   Isolate * isolate)
{
  auto proto = klass->PrototypeTemplate ();

  auto func = functions;
  while (func->name != NULL)
  {
    proto->Set (_gum_v8_string_new_ascii (isolate, func->name),
        FunctionTemplate::New (isolate, func->callback, module));
    func++;
  }
}
