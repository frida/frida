/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8kernel.h"

#include "gumv8enumeratecontext.h"
#include "gumv8macros.h"

#include <gum/gumkernel.h>
#include <string.h>

#define GUMJS_MODULE_NAME Kernel

using namespace v8;

enum GumMemoryValueType
{
  GUM_MEMORY_VALUE_S8,
  GUM_MEMORY_VALUE_U8,
  GUM_MEMORY_VALUE_S16,
  GUM_MEMORY_VALUE_U16,
  GUM_MEMORY_VALUE_S32,
  GUM_MEMORY_VALUE_U32,
  GUM_MEMORY_VALUE_S64,
  GUM_MEMORY_VALUE_U64,
  GUM_MEMORY_VALUE_LONG,
  GUM_MEMORY_VALUE_ULONG,
  GUM_MEMORY_VALUE_FLOAT,
  GUM_MEMORY_VALUE_DOUBLE,
  GUM_MEMORY_VALUE_BYTE_ARRAY,
  GUM_MEMORY_VALUE_C_STRING,
  GUM_MEMORY_VALUE_UTF8_STRING,
  GUM_MEMORY_VALUE_UTF16_STRING
};

struct GumKernelScanContext
{
  GumMemoryRange range;
  GumMatchPattern * pattern;
  Global<Function> * on_match;
  Global<Function> * on_error;
  Global<Function> * on_complete;

  GumV8Core * core;
};

struct GumKernelScanSyncContext
{
  Local<Array> matches;

  GumV8Core * core;
};

GUMJS_DECLARE_GETTER (gumjs_kernel_get_available)
GUMJS_DECLARE_GETTER (gumjs_kernel_get_base)
GUMJS_DECLARE_SETTER (gumjs_kernel_set_base)
GUMJS_DECLARE_FUNCTION (gumjs_kernel_enumerate_modules)
static gboolean gum_emit_module (const GumKernelModuleDetails * details,
    GumV8EnumerateContext<GumV8Kernel> * ec);
static Local<Object> gum_parse_module_details (
    const GumKernelModuleDetails * details, GumV8Core * core);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_enumerate_ranges)
static gboolean gum_emit_range (const GumRangeDetails * details,
    GumV8EnumerateContext<GumV8Kernel> * ec);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_enumerate_module_ranges)
static gboolean gum_emit_module_range (
    const GumKernelModuleRangeDetails * details,
    GumV8EnumerateContext<GumV8Kernel> * ec);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_alloc)
GUMJS_DECLARE_FUNCTION (gumjs_kernel_protect)

static void gum_v8_kernel_read (GumMemoryValueType type,
    const GumV8Args * args, ReturnValue<Value> return_value);
static void gum_v8_kernel_write (GumMemoryValueType type,
    const GumV8Args * args);

#define GUM_DEFINE_MEMORY_READ(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_kernel_read_##T) \
    { \
      gum_v8_kernel_read (GUM_MEMORY_VALUE_##T, args, info.GetReturnValue ()); \
    }
#define GUM_DEFINE_MEMORY_WRITE(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_kernel_write_##T) \
    { \
      gum_v8_kernel_write (GUM_MEMORY_VALUE_##T, args); \
    }
#define GUM_DEFINE_MEMORY_READ_WRITE(T) \
    GUM_DEFINE_MEMORY_READ (T); \
    GUM_DEFINE_MEMORY_WRITE (T)

#define GUMJS_EXPORT_MEMORY_READ(N, T) \
    { "read" N, gumjs_kernel_read_##T }
#define GUMJS_EXPORT_MEMORY_WRITE(N, T) \
    { "write" N, gumjs_kernel_write_##T }
#define GUMJS_EXPORT_MEMORY_READ_WRITE(N, T) \
    GUMJS_EXPORT_MEMORY_READ (N, T), \
    GUMJS_EXPORT_MEMORY_WRITE (N, T)

GUM_DEFINE_MEMORY_READ_WRITE (S8)
GUM_DEFINE_MEMORY_READ_WRITE (U8)
GUM_DEFINE_MEMORY_READ_WRITE (S16)
GUM_DEFINE_MEMORY_READ_WRITE (U16)
GUM_DEFINE_MEMORY_READ_WRITE (S32)
GUM_DEFINE_MEMORY_READ_WRITE (U32)
GUM_DEFINE_MEMORY_READ_WRITE (S64)
GUM_DEFINE_MEMORY_READ_WRITE (U64)
GUM_DEFINE_MEMORY_READ_WRITE (LONG)
GUM_DEFINE_MEMORY_READ_WRITE (ULONG)
GUM_DEFINE_MEMORY_READ_WRITE (FLOAT)
GUM_DEFINE_MEMORY_READ_WRITE (DOUBLE)
GUM_DEFINE_MEMORY_READ_WRITE (BYTE_ARRAY)
GUM_DEFINE_MEMORY_READ (C_STRING)
GUM_DEFINE_MEMORY_READ_WRITE (UTF8_STRING)
GUM_DEFINE_MEMORY_READ_WRITE (UTF16_STRING)

GUMJS_DECLARE_FUNCTION (gumjs_kernel_scan)
static void gum_kernel_scan_context_free (GumKernelScanContext * self);
static void gum_kernel_scan_context_run (GumKernelScanContext * self);
static gboolean gum_kernel_scan_context_emit_match (GumAddress address,
    gsize size, GumKernelScanContext * self);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_scan_sync)
static gboolean gum_append_match (GumAddress address, gsize size,
    GumKernelScanSyncContext * ctx);

static gboolean gum_v8_kernel_check_api_available (Isolate * isolate);

static const GumV8Property gumjs_kernel_values[] =
{
  { "available", gumjs_kernel_get_available, NULL },
  { "base", gumjs_kernel_get_base, gumjs_kernel_set_base },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_kernel_functions[] =
{
  { "enumerateModules", gumjs_kernel_enumerate_modules },
  { "_enumerateRanges", gumjs_kernel_enumerate_ranges },
  { "enumerateModuleRanges", gumjs_kernel_enumerate_module_ranges },
  { "alloc", gumjs_kernel_alloc },
  { "protect", gumjs_kernel_protect },

  GUMJS_EXPORT_MEMORY_READ_WRITE ("S8", S8),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U8", U8),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S16", S16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U16", U16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S32", S32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U32", U32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S64", S64),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U64", U64),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Short", S16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("UShort", U16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Int", S32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("UInt", U32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Long", LONG),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("ULong", ULONG),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Float", FLOAT),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Double", DOUBLE),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("ByteArray", BYTE_ARRAY),
  GUMJS_EXPORT_MEMORY_READ ("CString", C_STRING),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Utf8String", UTF8_STRING),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Utf16String", UTF16_STRING),

  { "_scan", gumjs_kernel_scan },
  { "scanSync", gumjs_kernel_scan_sync },

  { NULL, NULL }
};

void
_gum_v8_kernel_init (GumV8Kernel * self,
                     GumV8Core * core,
                     Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto kernel = _gum_v8_create_module ("Kernel", scope, isolate);
  kernel->Set (_gum_v8_string_new_ascii (isolate, "pageSize"),
      Number::New (isolate, gum_kernel_query_page_size ()), ReadOnly);
  _gum_v8_module_add (module, kernel, gumjs_kernel_values, isolate);
  _gum_v8_module_add (module, kernel, gumjs_kernel_functions, isolate);
}

void
_gum_v8_kernel_realize (GumV8Kernel * self)
{
}

void
_gum_v8_kernel_dispose (GumV8Kernel * self)
{
}

void
_gum_v8_kernel_finalize (GumV8Kernel * self)
{
}

GUMJS_DEFINE_GETTER (gumjs_kernel_get_available)
{
  info.GetReturnValue ().Set (!!gum_kernel_api_is_available ());
}

GUMJS_DEFINE_GETTER (gumjs_kernel_get_base)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumAddress address = gum_kernel_find_base_address ();
  info.GetReturnValue ().Set (_gum_v8_uint64_new (address, core));
}

GUMJS_DEFINE_SETTER (gumjs_kernel_set_base)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumAddress address;
  if (!_gum_v8_uint64_get (value, &address, core))
    return;

  gum_kernel_set_base_address (address);
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_enumerate_modules)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumV8EnumerateContext<GumV8Kernel> ec (isolate, module);

  gum_kernel_enumerate_modules ((GumFoundKernelModuleFunc) gum_emit_module,
      &ec);

  info.GetReturnValue ().Set (ec.End ());
}

static gboolean
gum_emit_module (const GumKernelModuleDetails * details,
                 GumV8EnumerateContext<GumV8Kernel> * ec)
{
  return ec->Collect (gum_parse_module_details (details, ec->parent->core));
}

static Local<Object>
gum_parse_module_details (const GumKernelModuleDetails * details,
                          GumV8Core * core)
{
  auto module = Object::New (core->isolate);
  _gum_v8_object_set_utf8 (module, "name", details->name, core);
  _gum_v8_object_set_uint64 (module, "base", details->range->base_address,
      core);
  _gum_v8_object_set_uint (module, "size", details->range->size, core);
  return module;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_enumerate_ranges)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumPageProtection prot;
  if (!_gum_v8_args_parse (args, "m", &prot))
    return;

  GumV8EnumerateContext<GumV8Kernel> ec (isolate, module);

  gum_kernel_enumerate_ranges (prot, (GumFoundRangeFunc) gum_emit_range, &ec);

  info.GetReturnValue ().Set (ec.End ());
}

static gboolean
gum_emit_range (const GumRangeDetails * details,
                GumV8EnumerateContext<GumV8Kernel> * ec)
{
  auto core = ec->parent->core;

  auto range = Object::New (ec->isolate);
  _gum_v8_object_set_uint64 (range, "base", details->range->base_address,
      core);
  _gum_v8_object_set_uint (range, "size", details->range->size, core);
  _gum_v8_object_set_page_protection (range, "protection", details->protection,
      core);

  return ec->Collect (range);
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_enumerate_module_ranges)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  gchar * module_name;
  GumPageProtection prot;
  if (!_gum_v8_args_parse (args, "s?m", &module_name, &prot))
    return;

  GumV8EnumerateContext<GumV8Kernel> ec (isolate, module);

  gum_kernel_enumerate_module_ranges (
    (module_name == NULL) ? "Kernel" : module_name, prot,
    (GumFoundKernelModuleRangeFunc) gum_emit_module_range, &ec);

  info.GetReturnValue ().Set (ec.End ());

  g_free (module_name);
}

static gboolean
gum_emit_module_range (const GumKernelModuleRangeDetails * details,
                       GumV8EnumerateContext<GumV8Kernel> * ec)
{
  auto core = ec->parent->core;

  auto range = Object::New (ec->isolate);
  _gum_v8_object_set_utf8 (range, "name", details->name, core);
  _gum_v8_object_set_uint64 (range, "base", details->address, core);
  _gum_v8_object_set_uint (range, "size", details->size, core);
  _gum_v8_object_set_page_protection (range, "protection", details->protection,
      core);

  return ec->Collect (range);
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_alloc)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  gsize size;
  if (!_gum_v8_args_parse (args, "Z", &size))
    return;

  if (size == 0 || size > 0x7fffffff)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid size");
    return;
  }

  gsize page_size = gum_kernel_query_page_size ();
  guint n_pages = ((size + page_size - 1) & ~(page_size - 1)) / page_size;

  GumAddress address = gum_kernel_alloc_n_pages (n_pages);

  GumV8KernelResource * res = _gum_v8_kernel_resource_new (address,
      n_pages * page_size, gum_kernel_free_pages, core);

  info.GetReturnValue ().Set (Local<Object>::New (isolate, *res->instance));
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_protect)
{
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumAddress address;
  gsize size;
  GumPageProtection prot;
  if (!_gum_v8_args_parse (args, "QZm", &address, &size, &prot))
    return;

  if (size > 0x7fffffff)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid size");
    return;
  }

  bool success;
  if (size != 0)
    success = !!gum_kernel_try_mprotect (address, size, prot);
  else
    success = true;

  info.GetReturnValue ().Set (success);
}

static void
gum_v8_kernel_read (GumMemoryValueType type,
                    const GumV8Args * args,
                    ReturnValue<Value> return_value)
{
  auto core = args->core;
  auto isolate = core->isolate;
  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumAddress address;
  gssize length = 0;

  switch (type)
  {
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
    case GUM_MEMORY_VALUE_C_STRING:
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
      if (!_gum_v8_args_parse (args, "QZ", &address, &length))
        return;
      break;
    default:
      if (!_gum_v8_args_parse (args, "Q", &address))
        return;
      break;
  }

  if (address == 0)
  {
    return_value.Set (Null (isolate));
    return;
  }

  if (length == 0)
  {
    switch (type)
    {
      case GUM_MEMORY_VALUE_S8:
      case GUM_MEMORY_VALUE_U8:
        length = 1;
        break;
      case GUM_MEMORY_VALUE_S16:
      case GUM_MEMORY_VALUE_U16:
        length = 2;
        break;
      case GUM_MEMORY_VALUE_S32:
      case GUM_MEMORY_VALUE_U32:
      case GUM_MEMORY_VALUE_FLOAT:
        length = 4;
        break;
      case GUM_MEMORY_VALUE_S64:
      case GUM_MEMORY_VALUE_U64:
      case GUM_MEMORY_VALUE_LONG:
      case GUM_MEMORY_VALUE_ULONG:
      case GUM_MEMORY_VALUE_DOUBLE:
        length = 8;
        break;
      default:
        g_assert_not_reached ();
    }
  }

  Local<Value> result;
  if (length > 0)
  {
    gsize n_bytes_read;
    auto data = gum_kernel_read (address, length, &n_bytes_read);
    if (data == NULL)
    {
      _gum_v8_throw_ascii (isolate,
          "access violation reading 0x%" G_GINT64_MODIFIER "x",
          address);
      return;
    }

    switch (type)
    {
      case GUM_MEMORY_VALUE_S8:
        result = Integer::New (isolate, *((gint8 *) data));
        break;
      case GUM_MEMORY_VALUE_U8:
        result = Integer::NewFromUnsigned (isolate, *((guint8 *) data));
        break;
      case GUM_MEMORY_VALUE_S16:
        result = Integer::New (isolate, *((gint16 *) data));
        break;
      case GUM_MEMORY_VALUE_U16:
        result = Integer::NewFromUnsigned (isolate, *((guint16 *) data));
        break;
      case GUM_MEMORY_VALUE_S32:
        result = Integer::New (isolate, *((gint32 *) data));
        break;
      case GUM_MEMORY_VALUE_U32:
        result = Integer::NewFromUnsigned (isolate, *((guint32 *) data));
        break;
      case GUM_MEMORY_VALUE_S64:
        result = _gum_v8_int64_new (*((gint64 *) data), core);
        break;
      case GUM_MEMORY_VALUE_U64:
        result = _gum_v8_uint64_new (*((guint64 *) data), core);
        break;
      case GUM_MEMORY_VALUE_LONG:
        result = _gum_v8_int64_new (*((glong *) data), core);
        break;
      case GUM_MEMORY_VALUE_ULONG:
        result = _gum_v8_uint64_new (*((gulong *) data), core);
        break;
      case GUM_MEMORY_VALUE_FLOAT:
        result = Number::New (isolate, *((gfloat *) data));
        break;
      case GUM_MEMORY_VALUE_DOUBLE:
        result = Number::New (isolate, *((gdouble *) data));
        break;
      case GUM_MEMORY_VALUE_BYTE_ARRAY:
        result = _gum_v8_array_buffer_new_take (isolate,
            g_steal_pointer (&data), n_bytes_read);
        break;
      case GUM_MEMORY_VALUE_C_STRING:
      {
        gchar * str = g_utf8_make_valid ((gchar *) data, length);
        result = String::NewFromUtf8 (isolate, str).ToLocalChecked ();
        g_free (str);

        break;
      }
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        const gchar * end;
        if (!g_utf8_validate ((gchar *) data, length, &end))
        {
          _gum_v8_throw_ascii (isolate,
              "can't decode byte 0x%02x in position %u",
              (guint8) *end, (guint) (end - (gchar *) data));
          break;
        }

        result = String::NewFromUtf8 (isolate, (gchar *) data,
            NewStringType::kNormal, length).ToLocalChecked ();

        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        auto str_utf16 = (gunichar2 *) data;

        glong size;
        auto str_utf8 = g_utf16_to_utf8 (str_utf16, length, NULL, &size, NULL);
        if (str_utf8 == NULL)
        {
          _gum_v8_throw_ascii_literal (isolate, "invalid string");
          break;
        }

        if (size != 0)
        {
          result = String::NewFromUtf8 (isolate, str_utf8,
              NewStringType::kNormal, size).ToLocalChecked ();
        }
        else
        {
          result = String::Empty (isolate);
        }

        g_free (str_utf8);

        break;
      }
    }

    g_free (data);
  }
  else
  {
    switch (type)
    {
      case GUM_MEMORY_VALUE_C_STRING:
      case GUM_MEMORY_VALUE_UTF8_STRING:
      case GUM_MEMORY_VALUE_UTF16_STRING:
        result = String::Empty (isolate);
        break;
      case GUM_MEMORY_VALUE_BYTE_ARRAY:
        result = ArrayBuffer::New (isolate, 0);
        break;
      default:
        _gum_v8_throw_ascii (isolate, "please provide a length > 0");
        return;
    }
  }

  if (!result.IsEmpty ())
    return_value.Set (result);
}

static void
gum_v8_kernel_write (GumMemoryValueType type,
                     const GumV8Args * args)
{
  auto core = args->core;
  auto isolate = core->isolate;
  gssize s = 0;
  gsize u = 0;
  gint64 s64 = 0;
  guint64 u64 = 0;
  gdouble number = 0;
  gfloat number32 = 0;
  GBytes * bytes = NULL;
  gchar * str = NULL;
  gsize str_length = 0;
  gunichar2 * str_utf16 = NULL;

  if (!gum_v8_kernel_check_api_available (isolate))
    return;

  GumAddress address = 0;
  guint8 * data = NULL;

  switch (type)
  {
    case GUM_MEMORY_VALUE_S8:
    case GUM_MEMORY_VALUE_S16:
    case GUM_MEMORY_VALUE_S32:
      if (!_gum_v8_args_parse (args, "Qz", &address, &s))
        return;
      break;
    case GUM_MEMORY_VALUE_U8:
    case GUM_MEMORY_VALUE_U16:
    case GUM_MEMORY_VALUE_U32:
      if (!_gum_v8_args_parse (args, "QZ", &address, &u))
        return;
      break;
    case GUM_MEMORY_VALUE_S64:
    case GUM_MEMORY_VALUE_LONG:
      if (!_gum_v8_args_parse (args, "Qq", &address, &s64))
        return;
      break;
    case GUM_MEMORY_VALUE_U64:
    case GUM_MEMORY_VALUE_ULONG:
      if (!_gum_v8_args_parse (args, "QQ", &address, &u64))
        return;
      break;
    case GUM_MEMORY_VALUE_FLOAT:
    case GUM_MEMORY_VALUE_DOUBLE:
      if (!_gum_v8_args_parse (args, "Qn", &address, &number))
        return;
      number32 = (gfloat) number;
      break;
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
      if (!_gum_v8_args_parse (args, "QB", &address, &bytes))
        return;
      break;
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
      if (!_gum_v8_args_parse (args, "Qs", &address, &str))
        return;

      str_length = g_utf8_strlen (str, -1);

      if (type == GUM_MEMORY_VALUE_UTF16_STRING)
        str_utf16 = g_utf8_to_utf16 (str, -1, NULL, NULL, NULL);

      break;
    default:
      g_assert_not_reached ();
  }

  gsize length = 0;

  switch (type)
  {
    case GUM_MEMORY_VALUE_S8:
      data = (guint8 *) &s;
      length = 1;
      break;
    case GUM_MEMORY_VALUE_U8:
      data = (guint8 *) &u;
      length = 1;
      break;
    case GUM_MEMORY_VALUE_S16:
      data = (guint8 *) &s;
      length = 2;
      break;
    case GUM_MEMORY_VALUE_U16:
      data = (guint8 *) &u;
      length = 2;
      break;
    case GUM_MEMORY_VALUE_S32:
      data = (guint8 *) &s;
      length = 4;
      break;
    case GUM_MEMORY_VALUE_U32:
      data = (guint8 *) &u;
      length = 4;
      break;
    case GUM_MEMORY_VALUE_LONG:
    case GUM_MEMORY_VALUE_S64:
      data = (guint8 *) &s64;
      length = 8;
      break;
    case GUM_MEMORY_VALUE_ULONG:
    case GUM_MEMORY_VALUE_U64:
      data = (guint8 *) &u64;
      length = 8;
      break;
    case GUM_MEMORY_VALUE_FLOAT:
      data = (guint8 *) &number32;
      length = 4;
      break;
    case GUM_MEMORY_VALUE_DOUBLE:
      data = (guint8 *) &number;
      length = 8;
      break;
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
    {
      data = (guint8 *) g_bytes_get_data (bytes, &length);
      break;
    }
    case GUM_MEMORY_VALUE_UTF8_STRING:
    {
      data = (guint8 *) str;
      length = g_utf8_offset_to_pointer (str, str_length) - str + 1;
      break;
    }
    case GUM_MEMORY_VALUE_UTF16_STRING:
    {
      data = (guint8 *) str_utf16;
      length = (str_length + 1) * sizeof (gunichar2);
      break;
    }
    default:
      g_assert_not_reached ();
  }

  if (length > 0)
  {
    if (!gum_kernel_write (address, data, length))
    {
      _gum_v8_throw_ascii (isolate,
          "access violation writing to 0x%" G_GINT64_MODIFIER "x",
          address);
    }
  }
  else
  {
    _gum_v8_throw_ascii (isolate, "please provide a length > 0");
  }

  g_bytes_unref (bytes);
  g_free (str);
  g_free (str_utf16);
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_scan)
{
  GumAddress address;
  gsize size;
  GumMatchPattern * pattern;
  Local<Function> on_match, on_error, on_complete;
  if (!_gum_v8_args_parse (args, "QZMF{onMatch,onError,onComplete}", &address,
      &size, &pattern, &on_match, &on_error, &on_complete))
    return;

  GumMemoryRange range;
  range.base_address = address;
  range.size = size;

  auto ctx = g_slice_new0 (GumKernelScanContext);
  ctx->range = range;
  ctx->pattern = pattern;
  ctx->on_match = new Global<Function> (isolate, on_match);
  ctx->on_error = new Global<Function> (isolate, on_error);
  ctx->on_complete = new Global<Function> (isolate, on_complete);
  ctx->core = core;

  _gum_v8_core_pin (core);
  _gum_v8_core_push_job (core, (GumScriptJobFunc) gum_kernel_scan_context_run,
      ctx, (GDestroyNotify) gum_kernel_scan_context_free);
}

static void
gum_kernel_scan_context_free (GumKernelScanContext * self)
{
  auto core = self->core;

  {
    ScriptScope script_scope (core->script);

    delete self->on_match;
    delete self->on_error;
    delete self->on_complete;

    _gum_v8_core_unpin (core);
  }

  gum_match_pattern_unref (self->pattern);

  g_slice_free (GumKernelScanContext, self);
}

static void
gum_kernel_scan_context_run (GumKernelScanContext * self)
{
  auto core = self->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  gum_kernel_scan (&self->range, self->pattern,
      (GumMemoryScanMatchFunc) gum_kernel_scan_context_emit_match, self);

  ScriptScope script_scope (core->script);

  auto on_complete (Local<Function>::New (isolate, *self->on_complete));
  auto recv = Undefined (isolate);
  auto result = on_complete->Call (context, recv, 0, nullptr);
  _gum_v8_ignore_result (result);
}

static gboolean
gum_kernel_scan_context_emit_match (GumAddress address,
                                    gsize size,
                                    GumKernelScanContext * self)
{
  ScriptScope scope (self->core->script);
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  gboolean proceed = TRUE;

  auto on_match = Local<Function>::New (isolate, *self->on_match);
  auto recv = Undefined (isolate);
  Local<Value> argv[] = {
    _gum_v8_uint64_new (address, self->core),
    Integer::NewFromUnsigned (isolate, size)
  };
  Local<Value> result;
  if (on_match->Call (context, recv, G_N_ELEMENTS (argv), argv)
      .ToLocal (&result) && result->IsString ())
  {
    v8::String::Utf8Value str (isolate, result);
    proceed = strcmp (*str, "stop") != 0;
  }

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_scan_sync)
{
  GumAddress address;
  gsize size;
  GumMatchPattern * pattern;
  if (!_gum_v8_args_parse (args, "QZM", &address, &size, &pattern))
    return;

  GumMemoryRange range;
  range.base_address = address;
  range.size = size;

  GumKernelScanSyncContext ctx;
  ctx.matches = Array::New (isolate);
  ctx.core = core;

  gum_kernel_scan (&range, pattern, (GumMemoryScanMatchFunc) gum_append_match,
      &ctx);

  info.GetReturnValue ().Set (ctx.matches);

  gum_match_pattern_unref (pattern);
}

static gboolean
gum_append_match (GumAddress address,
                  gsize size,
                  GumKernelScanSyncContext * ctx)
{
  GumV8Core * core = ctx->core;

  auto match = Object::New (core->isolate);
  _gum_v8_object_set_uint64 (match, "address", address, core);
  _gum_v8_object_set_uint (match, "size", size, core);
  ctx->matches->Set (core->isolate->GetCurrentContext (),
      ctx->matches->Length (), match).ToChecked ();

  return TRUE;
}

static gboolean
gum_v8_kernel_check_api_available (Isolate * isolate)
{
  if (!gum_kernel_api_is_available ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "Kernel API is not available on this system");
    return FALSE;
  }

  return TRUE;
}
