/*
 * Copyright (C) 2016-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2018-2019 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickkernel.h"

#include "gumquickmacros.h"

typedef guint GumMemoryValueType;
typedef struct _GumKernelScanContext GumKernelScanContext;
typedef struct _GumMemoryScanSyncContext GumMemoryScanSyncContext;
typedef struct _GumQuickEnumerateContext GumQuickEnumerateContext;

enum _GumMemoryValueType
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

struct _GumKernelScanContext
{
  GumMemoryRange range;
  GumMatchPattern * pattern;
  JSValue on_match;
  JSValue on_error;
  JSValue on_complete;
  GumQuickMatchResult result;

  JSContext * ctx;
  GumQuickCore * core;
};

struct _GumMemoryScanSyncContext
{
  JSValue matches;
  uint32_t index;

  JSContext * ctx;
  GumQuickCore * core;
};

struct _GumQuickEnumerateContext
{
  JSValue elements;
  guint n;

  JSContext * ctx;
  GumQuickCore * core;
};

GUMJS_DECLARE_GETTER (gumjs_kernel_get_available)
GUMJS_DECLARE_GETTER (gumjs_kernel_get_base)
GUMJS_DECLARE_SETTER (gumjs_kernel_set_base)
GUMJS_DECLARE_FUNCTION (gumjs_kernel_enumerate_modules)
static gboolean gum_emit_module (const GumKernelModuleDetails * details,
    GumQuickEnumerateContext * ec);
static JSValue gum_parse_module_details (JSContext * ctx,
    const GumKernelModuleDetails * details, GumQuickCore * core);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_enumerate_ranges)
static gboolean gum_emit_range (const GumRangeDetails * details,
    GumQuickEnumerateContext * ec);
static JSValue gum_parse_range_details (JSContext * ctx,
    const GumRangeDetails * details, GumQuickCore * core);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_enumerate_module_ranges)
static gboolean gum_emit_module_range (
    const GumKernelModuleRangeDetails * details, GumQuickEnumerateContext * ec);
static JSValue gum_parse_module_range_details (JSContext * ctx,
    const GumKernelModuleRangeDetails * details, GumQuickCore * core);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_alloc)
GUMJS_DECLARE_FUNCTION (gumjs_kernel_protect)

static JSValue gum_quick_kernel_read (JSContext * ctx, GumMemoryValueType type,
    GumQuickArgs * args, GumQuickCore * core);
static JSValue gum_quick_kernel_write (JSContext * ctx, GumMemoryValueType type,
    GumQuickArgs * args, GumQuickCore * core);

#define GUMJS_DEFINE_MEMORY_READ(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_kernel_read_##T) \
    { \
      return gum_quick_kernel_read (ctx, GUM_MEMORY_VALUE_##T, args, core); \
    }
#define GUMJS_DEFINE_MEMORY_WRITE(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_kernel_write_##T) \
    { \
      return gum_quick_kernel_write (ctx, GUM_MEMORY_VALUE_##T, args, core); \
    }
#define GUMJS_DEFINE_MEMORY_READ_WRITE(T) \
    GUMJS_DEFINE_MEMORY_READ (T); \
    GUMJS_DEFINE_MEMORY_WRITE (T)

#define GUMJS_EXPORT_MEMORY_READ(N, T) \
    JS_CFUNC_DEF ("read" N, 0, gumjs_kernel_read_##T)
#define GUMJS_EXPORT_MEMORY_WRITE(N, T) \
    JS_CFUNC_DEF ("write" N, 0, gumjs_kernel_write_##T)
#define GUMJS_EXPORT_MEMORY_READ_WRITE(N, T) \
    GUMJS_EXPORT_MEMORY_READ (N, T), \
    GUMJS_EXPORT_MEMORY_WRITE (N, T)

GUMJS_DEFINE_MEMORY_READ_WRITE (S8)
GUMJS_DEFINE_MEMORY_READ_WRITE (U8)
GUMJS_DEFINE_MEMORY_READ_WRITE (S16)
GUMJS_DEFINE_MEMORY_READ_WRITE (U16)
GUMJS_DEFINE_MEMORY_READ_WRITE (S32)
GUMJS_DEFINE_MEMORY_READ_WRITE (U32)
GUMJS_DEFINE_MEMORY_READ_WRITE (S64)
GUMJS_DEFINE_MEMORY_READ_WRITE (U64)
GUMJS_DEFINE_MEMORY_READ_WRITE (LONG)
GUMJS_DEFINE_MEMORY_READ_WRITE (ULONG)
GUMJS_DEFINE_MEMORY_READ_WRITE (FLOAT)
GUMJS_DEFINE_MEMORY_READ_WRITE (DOUBLE)
GUMJS_DEFINE_MEMORY_READ_WRITE (BYTE_ARRAY)
GUMJS_DEFINE_MEMORY_READ (C_STRING)
GUMJS_DEFINE_MEMORY_READ_WRITE (UTF8_STRING)
GUMJS_DEFINE_MEMORY_READ_WRITE (UTF16_STRING)

GUMJS_DECLARE_FUNCTION (gumjs_kernel_scan)
static void gum_kernel_scan_context_free (GumKernelScanContext * ctx);
static void gum_kernel_scan_context_run (GumKernelScanContext * self);
static gboolean gum_kernel_scan_context_emit_match (GumAddress address,
    gsize size, GumKernelScanContext * self);
GUMJS_DECLARE_FUNCTION (gumjs_kernel_scan_sync)
static gboolean gum_append_match (GumAddress address, gsize size,
    GumMemoryScanSyncContext * sc);

static void gum_quick_enumerate_context_begin (GumQuickEnumerateContext * ec,
    GumQuickCore * core);
static JSValue gum_quick_enumerate_context_end (GumQuickEnumerateContext * ec);
static gboolean gum_quick_enumerate_context_collect (
    GumQuickEnumerateContext * ec, JSValue element);

static gboolean gum_quick_kernel_check_api_available (JSContext * ctx);

static const JSCFunctionListEntry gumjs_kernel_entries[] =
{
  JS_CGETSET_DEF ("available", gumjs_kernel_get_available, NULL),
  JS_CGETSET_DEF ("base", gumjs_kernel_get_base, gumjs_kernel_set_base),

  JS_CFUNC_DEF ("enumerateModules", 0, gumjs_kernel_enumerate_modules),
  JS_CFUNC_DEF ("_enumerateRanges", 0, gumjs_kernel_enumerate_ranges),
  JS_CFUNC_DEF ("enumerateModuleRanges", 0,
      gumjs_kernel_enumerate_module_ranges),
  JS_CFUNC_DEF ("alloc", 0, gumjs_kernel_alloc),
  JS_CFUNC_DEF ("protect", 0, gumjs_kernel_protect),

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

  JS_CFUNC_DEF ("_scan", 0, gumjs_kernel_scan),
  JS_CFUNC_DEF ("scanSync", 0, gumjs_kernel_scan_sync),
};

void
_gum_quick_kernel_init (GumQuickKernel * self,
                        JSValue ns,
                        GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj;

  self->core = core;

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_kernel_entries,
      G_N_ELEMENTS (gumjs_kernel_entries));
  JS_DefinePropertyValueStr (ctx, obj, "pageSize",
      JS_NewInt32 (ctx, gum_kernel_query_page_size ()), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr (ctx, ns, "Kernel", obj, JS_PROP_C_W_E);
}

void
_gum_quick_kernel_dispose (GumQuickKernel * self)
{
}

void
_gum_quick_kernel_finalize (GumQuickKernel * self)
{
}

GUMJS_DEFINE_GETTER (gumjs_kernel_get_available)
{
  return JS_NewBool (ctx, gum_kernel_api_is_available ());
}

GUMJS_DEFINE_GETTER (gumjs_kernel_get_base)
{
  GumAddress address;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  address = gum_kernel_find_base_address ();

  return _gum_quick_uint64_new (ctx, address, core);
}

GUMJS_DEFINE_SETTER (gumjs_kernel_set_base)
{
  GumAddress address;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  if (!_gum_quick_uint64_get (ctx, val, core, &address))
    return JS_EXCEPTION;

  gum_kernel_set_base_address (address);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_enumerate_modules)
{
  GumQuickEnumerateContext ec;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  gum_quick_enumerate_context_begin (&ec, core);

  gum_kernel_enumerate_modules ((GumFoundKernelModuleFunc) gum_emit_module,
      &ec);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_module (const GumKernelModuleDetails * details,
                 GumQuickEnumerateContext * ec)
{
  return gum_quick_enumerate_context_collect (ec,
      gum_parse_module_details (ec->ctx, details, ec->core));
}

static JSValue
gum_parse_module_details (JSContext * ctx,
                          const GumKernelModuleDetails * details,
                          GumQuickCore * core)
{
  JSValue m = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, m,
      GUM_QUICK_CORE_ATOM (core, name),
      JS_NewString (ctx, details->name),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, m,
      GUM_QUICK_CORE_ATOM (core, base),
      _gum_quick_uint64_new (ctx, details->range->base_address, core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, m,
      GUM_QUICK_CORE_ATOM (core, size),
      JS_NewInt64 (ctx, details->range->size),
      JS_PROP_C_W_E);

  return m;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_enumerate_ranges)
{
  GumPageProtection prot;
  GumQuickEnumerateContext ec;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "m", &prot))
    return JS_EXCEPTION;

  gum_quick_enumerate_context_begin (&ec, core);

  gum_kernel_enumerate_ranges (prot, (GumFoundRangeFunc) gum_emit_range, &ec);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_range (const GumRangeDetails * details,
                GumQuickEnumerateContext * ec)
{
  return gum_quick_enumerate_context_collect (ec,
      gum_parse_range_details (ec->ctx, details, ec->core));
}

static JSValue
gum_parse_range_details (JSContext * ctx,
                         const GumRangeDetails * details,
                         GumQuickCore * core)
{
  JSValue r = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, base),
      _gum_quick_uint64_new (ctx, details->range->base_address, core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, size),
      JS_NewInt64 (ctx, details->range->size),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, protection),
      _gum_quick_page_protection_new (ctx, details->protection),
      JS_PROP_C_W_E);

  return r;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_enumerate_module_ranges)
{
  const gchar * module_name;
  GumPageProtection prot;
  GumQuickEnumerateContext ec;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "s?m", &module_name, &prot))
    return JS_EXCEPTION;

  gum_quick_enumerate_context_begin (&ec, core);

  gum_kernel_enumerate_module_ranges (
      (module_name == NULL) ? "Kernel" : module_name, prot,
      (GumFoundKernelModuleRangeFunc) gum_emit_module_range, &ec);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_module_range (const GumKernelModuleRangeDetails * details,
                       GumQuickEnumerateContext * ec)
{
  return gum_quick_enumerate_context_collect (ec,
      gum_parse_module_range_details (ec->ctx, details, ec->core));
}

static JSValue
gum_parse_module_range_details (JSContext * ctx,
                                const GumKernelModuleRangeDetails * details,
                                GumQuickCore * core)
{
  JSValue r = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, name),
      JS_NewString (ctx, details->name),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, base),
      _gum_quick_uint64_new (ctx, details->address, core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, size),
      JS_NewInt64 (ctx, details->size),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, r,
      GUM_QUICK_CORE_ATOM (core, protection),
      _gum_quick_page_protection_new (ctx, details->protection),
      JS_PROP_C_W_E);

  return r;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_alloc)
{
  GumAddress address;
  gsize size, page_size;
  guint n_pages;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "Z", &size))
    return JS_EXCEPTION;

  if (size == 0 || size > 0x7fffffff)
    return _gum_quick_throw_literal (ctx, "invalid size");

  page_size = gum_kernel_query_page_size ();
  n_pages = ((size + page_size - 1) & ~(page_size - 1)) / page_size;

  address = gum_kernel_alloc_n_pages (n_pages);

  return _gum_quick_kernel_resource_new (ctx, address, gum_kernel_free_pages,
      core);
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_protect)
{
  GumAddress address;
  gsize size;
  GumPageProtection prot;
  gboolean success;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "QZm", &address, &size, &prot))
    return JS_EXCEPTION;

  if (size > 0x7fffffff)
    return _gum_quick_throw_literal (ctx, "invalid size");

  if (size != 0)
    success = gum_kernel_try_mprotect (address, size, prot);
  else
    success = TRUE;

  return JS_NewBool (ctx, success);
}

static JSValue
gum_quick_kernel_read (JSContext * ctx,
                       GumMemoryValueType type,
                       GumQuickArgs * args,
                       GumQuickCore * core)
{
  JSValue result = JS_NULL;
  GumAddress address;
  gssize length;
  gpointer data = NULL;
  const gchar * end;

  if (!gum_quick_kernel_check_api_available (ctx))
    goto propagate_exception;

  switch (type)
  {
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
    case GUM_MEMORY_VALUE_C_STRING:
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
      if (!_gum_quick_args_parse (args, "QZ", &address, &length))
        goto propagate_exception;
      break;
    default:
      if (!_gum_quick_args_parse (args, "Q", &address))
        goto propagate_exception;
      length = 0;
      break;
  }

  if (address == 0)
    goto beach;

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
        break;
    }
  }

  if (length > 0)
  {
    gsize n_bytes_read;

    data = gum_kernel_read (address, length, &n_bytes_read);
    if (data == NULL)
      goto invalid_address;

    switch (type)
    {
      case GUM_MEMORY_VALUE_S8:
        result = JS_NewInt32 (ctx, *((gint8 *) data));
        break;
      case GUM_MEMORY_VALUE_U8:
        result = JS_NewInt32 (ctx, *((guint8 *) data));
        break;
      case GUM_MEMORY_VALUE_S16:
        result = JS_NewInt32 (ctx, *((gint16 *) data));
        break;
      case GUM_MEMORY_VALUE_U16:
        result = JS_NewInt32 (ctx, *((guint16 *) data));
        break;
      case GUM_MEMORY_VALUE_S32:
        result = JS_NewInt32 (ctx, *((gint32 *) data));
        break;
      case GUM_MEMORY_VALUE_U32:
        result = JS_NewInt32 (ctx, *((guint32 *) data));
        break;
      case GUM_MEMORY_VALUE_S64:
        result = _gum_quick_int64_new (ctx, *((gint64 *) data), core);
        break;
      case GUM_MEMORY_VALUE_U64:
        result = _gum_quick_uint64_new (ctx, *((guint64 *) data), core);
        break;
      case GUM_MEMORY_VALUE_LONG:
        result = _gum_quick_int64_new (ctx, *((glong *) data), core);
        break;
      case GUM_MEMORY_VALUE_ULONG:
        result = _gum_quick_uint64_new (ctx, *((gulong *) data), core);
        break;
      case GUM_MEMORY_VALUE_FLOAT:
        result = JS_NewFloat64 (ctx, *((gfloat *) data));
        break;
      case GUM_MEMORY_VALUE_DOUBLE:
        result = JS_NewFloat64 (ctx, *((gdouble *) data));
        break;
      case GUM_MEMORY_VALUE_BYTE_ARRAY:
      {
        uint8_t * buf = g_steal_pointer (&data);

        result = JS_NewArrayBuffer (ctx, buf, n_bytes_read,
            _gum_quick_array_buffer_free, buf, FALSE);

        break;
      }
      case GUM_MEMORY_VALUE_C_STRING:
      {
        gchar * str;

        str = g_utf8_make_valid (data, n_bytes_read);
        result = JS_NewString (ctx, str);
        g_free (str);

        break;
      }
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        gchar * slice;

        if (!g_utf8_validate (data, n_bytes_read, &end))
          goto invalid_utf8;

        slice = g_strndup (data, n_bytes_read);
        result = JS_NewString (ctx, slice);
        g_free (slice);

        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        gchar * str_utf8;
        glong size;

        str_utf8 = g_utf16_to_utf8 (data, n_bytes_read, NULL, &size, NULL);
        if (str_utf8 == NULL)
          goto invalid_utf16;
        result = JS_NewString (ctx, str_utf8);
        g_free (str_utf8);

        break;
      }
      default:
        g_assert_not_reached ();
    }

  }
  else if (type == GUM_MEMORY_VALUE_BYTE_ARRAY)
  {
    result = JS_NewArrayBufferCopy (ctx, NULL, 0);
  }
  else
  {
    goto invalid_length;
  }

  goto beach;

invalid_address:
  {
    _gum_quick_throw (ctx, "access violation reading 0x%" G_GINT64_MODIFIER "x",
        address);
    goto propagate_exception;
  }
invalid_length:
  {
    _gum_quick_throw_literal (ctx, "expected a length > 0");
    goto propagate_exception;
  }
invalid_utf8:
  {
    _gum_quick_throw (ctx, "can't decode byte 0x%02x in position %u",
        (guint8) *end, (guint) (end - (gchar *) data));
    goto propagate_exception;
  }
invalid_utf16:
  {
    _gum_quick_throw_literal (ctx, "invalid string");
    goto propagate_exception;
  }
propagate_exception:
  {
    result = JS_EXCEPTION;
    goto beach;
  }
beach:
  {
    g_free (data);

    return result;
  }
}

static JSValue
gum_quick_kernel_write (JSContext * ctx,
                        GumMemoryValueType type,
                        GumQuickArgs * args,
                        GumQuickCore * core)
{
  GumAddress address = 0;
  gssize s = 0;
  gsize u = 0;
  gint64 s64 = 0;
  guint64 u64 = 0;
  gdouble number = 0;
  gfloat number32 = 0;
  GBytes * bytes = NULL;
  const gchar * str = NULL;
  gunichar2 * str_utf16 = NULL;
  const guint8 * data = NULL;
  gsize str_length = 0;
  gsize length = 0;
  gboolean success;

  if (!gum_quick_kernel_check_api_available (ctx))
    return JS_EXCEPTION;

  switch (type)
  {
    case GUM_MEMORY_VALUE_S8:
    case GUM_MEMORY_VALUE_S16:
    case GUM_MEMORY_VALUE_S32:
      if (!_gum_quick_args_parse (args, "Qz", &address, &s))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_U8:
    case GUM_MEMORY_VALUE_U16:
    case GUM_MEMORY_VALUE_U32:
      if (!_gum_quick_args_parse (args, "QZ", &address, &u))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_S64:
    case GUM_MEMORY_VALUE_LONG:
      if (!_gum_quick_args_parse (args, "Qq", &address, &s64))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_U64:
    case GUM_MEMORY_VALUE_ULONG:
      if (!_gum_quick_args_parse (args, "QQ", &address, &u64))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_FLOAT:
    case GUM_MEMORY_VALUE_DOUBLE:
      if (!_gum_quick_args_parse (args, "Qn", &address, &number))
        return JS_EXCEPTION;
      number32 = (gfloat) number;
      break;
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
      if (!_gum_quick_args_parse (args, "QB", &address, &bytes))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
      if (!_gum_quick_args_parse (args, "Qs", &address, &str))
        return JS_EXCEPTION;

      str_length = g_utf8_strlen (str, -1);
      if (type == GUM_MEMORY_VALUE_UTF16_STRING)
        str_utf16 = g_utf8_to_utf16 (str, -1, NULL, NULL, NULL);

      break;
    default:
      g_assert_not_reached ();
  }

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
      data = g_bytes_get_data (bytes, &length);
      break;
    case GUM_MEMORY_VALUE_UTF8_STRING:
      data = (guint8 *) str;
      length = g_utf8_offset_to_pointer (str, str_length) - str + 1;
      break;
    case GUM_MEMORY_VALUE_UTF16_STRING:
      data = (guint8 *) str_utf16;
      length = (str_length + 1) * sizeof (gunichar2);
      break;
    default:
      g_assert_not_reached ();
  }

  if (length <= 0)
    goto invalid_length;

  success = gum_kernel_write (address, data, length);

  g_free (str_utf16);

  if (!success)
    goto invalid_address;

  return JS_UNDEFINED;

invalid_address:
  {
    _gum_quick_throw (ctx, "access violation writing to 0x%" G_GINT64_MODIFIER
        "x", address);
    return JS_EXCEPTION;
  }
invalid_length:
  {
    _gum_quick_throw_literal (ctx, "expected a length > 0");
    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_scan)
{
  GumKernelScanContext sc;
  GumAddress address;
  gsize size;

  if (!_gum_quick_args_parse (args, "QZMF{onMatch,onError,onComplete}",
      &address, &size, &sc.pattern, &sc.on_match, &sc.on_error,
      &sc.on_complete))
    return JS_EXCEPTION;

  sc.range.base_address = address;
  sc.range.size = size;

  gum_match_pattern_ref (sc.pattern);

  JS_DupValue (ctx, sc.on_match);
  JS_DupValue (ctx, sc.on_error);
  JS_DupValue (ctx, sc.on_complete);

  sc.result = GUM_QUICK_MATCH_CONTINUE;

  sc.ctx = ctx;
  sc.core = core;

  _gum_quick_core_pin (core);
  _gum_quick_core_push_job (core,
      (GumScriptJobFunc) gum_kernel_scan_context_run,
      g_slice_dup (GumKernelScanContext, &sc),
      (GDestroyNotify) gum_kernel_scan_context_free);

  return JS_UNDEFINED;
}

static void
gum_kernel_scan_context_free (GumKernelScanContext * self)
{
  JSContext * ctx = self->ctx;
  GumQuickCore * core = self->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, core);

  JS_FreeValue (ctx, self->on_match);
  JS_FreeValue (ctx, self->on_error);
  JS_FreeValue (ctx, self->on_complete);

  _gum_quick_core_unpin (core);
  _gum_quick_scope_leave (&scope);

  gum_match_pattern_unref (self->pattern);

  g_slice_free (GumKernelScanContext, self);
}

static void
gum_kernel_scan_context_run (GumKernelScanContext * self)
{
  gum_kernel_scan (&self->range, self->pattern,
      (GumMemoryScanMatchFunc) gum_kernel_scan_context_emit_match, self);

  if (self->result != GUM_QUICK_MATCH_ERROR)
  {
    GumQuickScope script_scope;

    _gum_quick_scope_enter (&script_scope, self->core);

    _gum_quick_scope_call_void (&script_scope, self->on_complete, JS_UNDEFINED,
        0, NULL);

    _gum_quick_scope_leave (&script_scope);
  }
}

static gboolean
gum_kernel_scan_context_emit_match (GumAddress address,
                                    gsize size,
                                    GumKernelScanContext * self)
{
  gboolean proceed;
  JSContext * ctx = self->ctx;
  GumQuickCore * core = self->core;
  GumQuickScope scope;
  JSValue argv[2];
  JSValue result;

  _gum_quick_scope_enter (&scope, core);

  argv[0] = _gum_quick_uint64_new (ctx, address, core);
  argv[1] = JS_NewUint32 (ctx, size);

  result = _gum_quick_scope_call (&scope, self->on_match, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  proceed = _gum_quick_process_match_result (ctx, &result, &self->result);

  _gum_quick_scope_leave (&scope);

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_kernel_scan_sync)
{
  JSValue result;
  GumAddress address;
  gsize size;
  GumMatchPattern * pattern;
  GumMemoryRange range;
  GumMemoryScanSyncContext sc;

  if (!_gum_quick_args_parse (args, "QZM", &address, &size, &pattern))
    return JS_EXCEPTION;

  range.base_address = address;
  range.size = size;

  result = JS_NewArray (ctx);

  sc.matches = result;
  sc.index = 0;

  sc.ctx = ctx;
  sc.core = core;

  gum_kernel_scan (&range, pattern, (GumMemoryScanMatchFunc) gum_append_match,
      &sc);

  return result;
}

static gboolean
gum_append_match (GumAddress address,
                  gsize size,
                  GumMemoryScanSyncContext * sc)
{
  JSContext * ctx = sc->ctx;
  GumQuickCore * core = sc->core;
  JSValue m;

  m = JS_NewObject (ctx);
  JS_DefinePropertyValue (ctx, m, GUM_QUICK_CORE_ATOM (core, address),
      _gum_quick_uint64_new (ctx, address, core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, m, GUM_QUICK_CORE_ATOM (core, size),
      JS_NewUint32 (ctx, size),
      JS_PROP_C_W_E);

  JS_DefinePropertyValueUint32 (ctx, sc->matches, sc->index, m, JS_PROP_C_W_E);
  sc->index++;

  return TRUE;
}

static void
gum_quick_enumerate_context_begin (GumQuickEnumerateContext * ec,
                                   GumQuickCore * core)
{
  ec->elements = JS_NewArray (core->ctx);
  ec->n = 0;

  ec->ctx = core->ctx;
  ec->core = core;
}

static JSValue
gum_quick_enumerate_context_end (GumQuickEnumerateContext * ec)
{
  return ec->elements;
}

static gboolean
gum_quick_enumerate_context_collect (GumQuickEnumerateContext * ec,
                                     JSValue element)
{
  JS_DefinePropertyValueUint32 (ec->ctx, ec->elements, ec->n++, element,
      JS_PROP_C_W_E);
  return TRUE;
}

static gboolean
gum_quick_kernel_check_api_available (JSContext * ctx)
{
  if (!gum_kernel_api_is_available ())
  {
    _gum_quick_throw_literal (ctx,
        "Kernel API is not available on this system");
    return FALSE;
  }

  return TRUE;
}
