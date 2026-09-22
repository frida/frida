/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2025 Kenjiro Ichise <ichise@doranekosystems.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickmemory.h"

#include "gumansi.h"
#include "gumquickmacros.h"

typedef struct _GumMemoryPatchContext GumMemoryPatchContext;
typedef struct _GumMemoryScanContext GumMemoryScanContext;
typedef struct _GumMemoryScanSyncContext GumMemoryScanSyncContext;

struct _GumMemoryPatchContext
{
  JSValue apply;

  JSContext * ctx;
  GumQuickCore * core;
};

struct _GumMemoryScanContext
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

GUMJS_DECLARE_FUNCTION (gumjs_memory_alloc)
GUMJS_DECLARE_FUNCTION (gumjs_memory_copy)
GUMJS_DECLARE_FUNCTION (gumjs_memory_protect)
GUMJS_DECLARE_FUNCTION (gumjs_memory_query_protection)
GUMJS_DECLARE_FUNCTION (gumjs_memory_patch_code)
static void gum_memory_patch_context_apply (gpointer mem,
    GumMemoryPatchContext * self);
GUMJS_DECLARE_FUNCTION (gumjs_memory_check_code_pointer)
GUMJS_DECLARE_FUNCTION (gumjs_memory_alloc_ansi_string)
GUMJS_DECLARE_FUNCTION (gumjs_memory_alloc_utf8_string)
GUMJS_DECLARE_FUNCTION (gumjs_memory_alloc_utf16_string)
GUMJS_DECLARE_FUNCTION (gumjs_memory_scan)
static void gum_memory_scan_context_free (GumMemoryScanContext * ctx);
static void gum_memory_scan_context_run (GumMemoryScanContext * self);
static gboolean gum_memory_scan_context_emit_match (GumAddress address,
    gsize size, GumMemoryScanContext * self);
GUMJS_DECLARE_FUNCTION (gumjs_memory_scan_sync)
static gboolean gum_append_match (GumAddress address, gsize size,
    GumMemoryScanSyncContext * sc);
GUMJS_DECLARE_FUNCTION (gumjs_memory_find_pointers)
static gboolean gum_quick_memory_values_get (JSContext * ctx, JSValueConst val,
    GumQuickCore * core, gsize ** values, guint * n_values);

GUMJS_DECLARE_FUNCTION (gumjs_memory_access_monitor_enable)
GUMJS_DECLARE_FUNCTION (gumjs_memory_access_monitor_disable)
static void gum_quick_memory_clear_monitor (GumQuickMemory * self,
    JSContext * ctx);
static void gum_quick_memory_on_access (GumMemoryAccessMonitor * monitor,
    const GumMemoryAccessDetails * details, GumQuickMemory * self);

GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_thread_id)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_operation)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_from)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_address)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_range_index)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_page_index)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_pages_completed)
GUMJS_DECLARE_GETTER (gumjs_memory_access_details_get_pages_total)

static const JSCFunctionListEntry gumjs_memory_entries[] =
{
  JS_CFUNC_DEF ("_alloc", 0, gumjs_memory_alloc),
  JS_CFUNC_DEF ("copy", 0, gumjs_memory_copy),
  JS_CFUNC_DEF ("protect", 0, gumjs_memory_protect),
  JS_CFUNC_DEF ("queryProtection", 0, gumjs_memory_query_protection),
  JS_CFUNC_DEF ("_patchCode", 0, gumjs_memory_patch_code),
  JS_CFUNC_DEF ("_checkCodePointer", 0, gumjs_memory_check_code_pointer),
  JS_CFUNC_DEF ("allocAnsiString", 0, gumjs_memory_alloc_ansi_string),
  JS_CFUNC_DEF ("allocUtf8String", 0, gumjs_memory_alloc_utf8_string),
  JS_CFUNC_DEF ("allocUtf16String", 0, gumjs_memory_alloc_utf16_string),
  JS_CFUNC_DEF ("_scan", 0, gumjs_memory_scan),
  JS_CFUNC_DEF ("scanSync", 0, gumjs_memory_scan_sync),
  JS_CFUNC_DEF ("findPointers", 0, gumjs_memory_find_pointers),
};

static const JSCFunctionListEntry gumjs_memory_access_monitor_entries[] =
{
  JS_CFUNC_DEF ("enable", 0, gumjs_memory_access_monitor_enable),
  JS_CFUNC_DEF ("disable", 0, gumjs_memory_access_monitor_disable),
};

static const JSClassDef gumjs_memory_access_details_def =
{
  .class_name = "MemoryAccessDetails",
};

static const JSCFunctionListEntry gumjs_memory_access_details_entries[] =
{
  JS_CGETSET_DEF ("threadId", gumjs_memory_access_details_get_thread_id, NULL),
  JS_CGETSET_DEF ("operation", gumjs_memory_access_details_get_operation, NULL),
  JS_CGETSET_DEF ("from", gumjs_memory_access_details_get_from, NULL),
  JS_CGETSET_DEF ("address", gumjs_memory_access_details_get_address, NULL),
  JS_CGETSET_DEF ("rangeIndex", gumjs_memory_access_details_get_range_index,
      NULL),
  JS_CGETSET_DEF ("pageIndex", gumjs_memory_access_details_get_page_index,
      NULL),
  JS_CGETSET_DEF ("pagesCompleted",
      gumjs_memory_access_details_get_pages_completed, NULL),
  JS_CGETSET_DEF ("pagesTotal", gumjs_memory_access_details_get_pages_total,
      NULL),
};

void
_gum_quick_memory_init (GumQuickMemory * self,
                        JSValue ns,
                        GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj, proto;

  self->core = core;
  self->monitor = NULL;
  self->on_access = JS_NULL;

  _gum_quick_core_store_module_data (core, "memory", self);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_memory_entries,
      G_N_ELEMENTS (gumjs_memory_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Memory", obj, JS_PROP_C_W_E);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_memory_access_monitor_entries,
      G_N_ELEMENTS (gumjs_memory_access_monitor_entries));
  JS_DefinePropertyValueStr (ctx, ns, "MemoryAccessMonitor", obj,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_memory_access_details_def, core,
      &self->memory_access_details_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_memory_access_details_entries,
      G_N_ELEMENTS (gumjs_memory_access_details_entries));
}

void
_gum_quick_memory_dispose (GumQuickMemory * self)
{
  gum_quick_memory_clear_monitor (self, self->core->ctx);
}

void
_gum_quick_memory_finalize (GumQuickMemory * self)
{
}

static GumQuickMemory *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "memory");
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_alloc)
{
  gsize size, page_size;
  GumPageProtection prot;
  GumAddressSpec spec;

  if (!_gum_quick_args_parse (args, "ZmpZ", &size, &prot, &spec.near_address,
      &spec.max_distance))
    return JS_EXCEPTION;

  if (size == 0 || size > 0x7fffffff)
    return _gum_quick_throw_literal (ctx, "invalid size");

  page_size = gum_query_page_size ();

  if (spec.near_address != NULL)
  {
    gpointer result;

    if ((size % page_size) != 0)
    {
      return _gum_quick_throw_literal (ctx,
          "size must be a multiple of page size");
    }

    result = gum_memory_allocate_near (&spec, size, page_size, prot);
    if (result == NULL)
    {
      return _gum_quick_throw_literal (ctx,
          "unable to allocate free page(s) near address");
    }
    if (prot == GUM_PAGE_NO_ACCESS)
      gum_memory_recommit (result, size, prot);

    return _gum_quick_native_resource_new_from_pages (ctx, result, size, core);
  }
  else
  {
    if ((size % page_size) != 0)
    {
      if ((prot & GUM_PAGE_EXECUTE) != 0)
      {
        return _gum_quick_throw_literal (ctx,
            "size must be a multiple of page size");
      }

      return _gum_quick_native_resource_new (ctx, g_malloc0 (size), g_free,
          core);
    }
    else
    {
      gpointer result;

      result = gum_memory_allocate (NULL, size, page_size, prot);
      if (prot == GUM_PAGE_NO_ACCESS)
        gum_memory_recommit (result, size, prot);

      return _gum_quick_native_resource_new_from_pages (ctx, result, size,
          core);
    }
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_copy)
{
  GumExceptor * exceptor = core->exceptor;
  gpointer destination, source;
  gsize size;
  GumExceptorScope scope;

  if (!_gum_quick_args_parse (args, "ppZ", &destination, &source, &size))
    return JS_EXCEPTION;

  if (size == 0)
    return JS_UNDEFINED;
  else if (size > 0x7fffffff)
    return _gum_quick_throw_literal (ctx, "invalid size");

  if (gum_exceptor_try (exceptor, &scope))
  {
    memmove (destination, source, size);
  }

  if (gum_exceptor_catch (exceptor, &scope))
  {
    return _gum_quick_throw_native (ctx, &scope.exception, core);
  }

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_protect)
{
  gpointer address;
  gsize size;
  GumPageProtection prot;
  gboolean success;

  if (!_gum_quick_args_parse (args, "pZm", &address, &size, &prot))
    return JS_EXCEPTION;

  if (size > 0x7fffffff)
    return _gum_quick_throw_literal (ctx, "invalid size");

  if (size != 0)
    success = gum_try_mprotect (address, size, prot);
  else
    success = TRUE;

  return JS_NewBool (ctx, success);
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_query_protection)
{
  gpointer address;
  GumPageProtection prot;

  if (!_gum_quick_args_parse (args, "p", &address))
    goto propagate_exception;

  if (!gum_memory_query_protection (address, &prot))
    goto query_failed;

  return _gum_quick_page_protection_new (ctx, prot);

query_failed:
  _gum_quick_throw_literal (ctx, "failed to query address");

propagate_exception:
  return JS_EXCEPTION;
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_patch_code)
{
  gpointer address;
  gsize size;
  GumMemoryPatchContext pc;
  gboolean success;

  if (!_gum_quick_args_parse (args, "pZF", &address, &size, &pc.apply))
    return JS_EXCEPTION;
  pc.ctx = ctx;
  pc.core = core;

  success = gum_memory_patch_code (address, size,
      (GumMemoryPatchApplyFunc) gum_memory_patch_context_apply, &pc);
  if (!success)
    return _gum_quick_throw_literal (ctx, "invalid address");

  return JS_UNDEFINED;
}

static void
gum_memory_patch_context_apply (gpointer mem,
                                GumMemoryPatchContext * self)
{
  JSContext * ctx = self->ctx;
  GumQuickCore * core = self->core;
  JSValue mem_val;

  mem_val = _gum_quick_native_pointer_new (ctx, mem, core);

  _gum_quick_scope_call_void (self->core->current_scope, self->apply,
      JS_UNDEFINED, 1, &mem_val);

  JS_FreeValue (ctx, mem_val);
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_check_code_pointer)
{
  JSValue result = JS_NULL;
  const guint8 * ptr;
  GumExceptor * exceptor = core->exceptor;
  GumExceptorScope scope;

  if (!_gum_quick_args_parse (args, "p", &ptr))
    return JS_EXCEPTION;

  ptr = gum_strip_code_pointer ((gpointer) ptr);

#ifdef HAVE_ARM
  ptr = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (ptr) & ~1);
#endif

  gum_ensure_code_readable (ptr, 1);

  if (gum_exceptor_try (exceptor, &scope))
  {
    result = JS_NewUint32 (ctx, *ptr);
  }

  if (gum_exceptor_catch (exceptor, &scope))
  {
    return _gum_quick_throw_native (ctx, &scope.exception, core);
  }

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_alloc_ansi_string)
{
#ifdef HAVE_WINDOWS
  const gchar * str;
  gchar * str_ansi;

  if (!_gum_quick_args_parse (args, "s", &str))
    return JS_EXCEPTION;

  str_ansi = _gum_ansi_string_from_utf8 (str);

  return _gum_quick_native_resource_new (ctx, str_ansi, g_free, core);
#else
  return _gum_quick_throw_literal (ctx,
      "ANSI API is only applicable on Windows");
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_alloc_utf8_string)
{
  const gchar * str;

  if (!_gum_quick_args_parse (args, "s", &str))
    return JS_EXCEPTION;

  return _gum_quick_native_resource_new (ctx, g_strdup (str), g_free, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_alloc_utf16_string)
{
  const gchar * str;
  gunichar2 * str_utf16;

  if (!_gum_quick_args_parse (args, "s", &str))
    return JS_EXCEPTION;

  str_utf16 = g_utf8_to_utf16 (str, -1, NULL, NULL, NULL);

  return _gum_quick_native_resource_new (ctx, str_utf16, g_free, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_scan)
{
  gpointer address;
  gsize size;
  GumMemoryScanContext sc;

  if (!_gum_quick_args_parse (args, "pZMF{onMatch,onError,onComplete}",
      &address, &size, &sc.pattern, &sc.on_match, &sc.on_error,
      &sc.on_complete))
    return JS_EXCEPTION;

  sc.range.base_address = GUM_ADDRESS (address);
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
      (GumScriptJobFunc) gum_memory_scan_context_run,
      g_slice_dup (GumMemoryScanContext, &sc),
      (GDestroyNotify) gum_memory_scan_context_free);

  return JS_UNDEFINED;
}

static void
gum_memory_scan_context_free (GumMemoryScanContext * self)
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

  g_slice_free (GumMemoryScanContext, self);
}

static void
gum_memory_scan_context_run (GumMemoryScanContext * self)
{
  JSContext * ctx = self->ctx;
  GumQuickCore * core = self->core;
  GumExceptor * exceptor = core->exceptor;
  GumExceptorScope exceptor_scope;
  GumQuickScope script_scope;

  if (gum_exceptor_try (exceptor, &exceptor_scope))
  {
    gum_memory_scan (&self->range, self->pattern,
        (GumMemoryScanMatchFunc) gum_memory_scan_context_emit_match, self);
  }

  _gum_quick_scope_enter (&script_scope, core);

  if (gum_exceptor_catch (exceptor, &exceptor_scope))
  {
    if (!JS_IsNull (self->on_error))
    {
      gchar * message;
      JSValue message_val;

      message = gum_exception_details_to_string (&exceptor_scope.exception);
      message_val = JS_NewString (ctx, message);
      g_free (message);

      _gum_quick_scope_call_void (&script_scope, self->on_error, JS_UNDEFINED,
          1, &message_val);
    }
  }

  if (self->result != GUM_QUICK_MATCH_ERROR)
  {
    _gum_quick_scope_call_void (&script_scope, self->on_complete, JS_UNDEFINED,
        0, NULL);
  }

  _gum_quick_scope_leave (&script_scope);
}

static gboolean
gum_memory_scan_context_emit_match (GumAddress address,
                                    gsize size,
                                    GumMemoryScanContext * self)
{
  gboolean proceed;
  JSContext * ctx = self->ctx;
  GumQuickCore * core = self->core;
  GumQuickScope scope;
  JSValue argv[2];
  JSValue result;

  _gum_quick_scope_enter (&scope, core);

  argv[0] = _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (address),
      core);
  argv[1] = JS_NewUint32 (ctx, size);

  result = _gum_quick_scope_call (&scope, self->on_match, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[0]);

  proceed = _gum_quick_process_match_result (ctx, &result, &self->result);

  _gum_quick_scope_leave (&scope);

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_scan_sync)
{
  JSValue result;
  gpointer address;
  gsize size;
  GumMatchPattern * pattern;
  GumMemoryRange range;
  GumExceptorScope scope;

  if (!_gum_quick_args_parse (args, "pZM", &address, &size, &pattern))
    return JS_EXCEPTION;

  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  result = JS_NewArray (ctx);

  if (gum_exceptor_try (core->exceptor, &scope))
  {
    GumMemoryScanSyncContext sc;

    sc.matches = result;
    sc.index = 0;

    sc.ctx = ctx;
    sc.core = core;

    gum_memory_scan (&range, pattern, (GumMemoryScanMatchFunc) gum_append_match,
        &sc);
  }

  if (gum_exceptor_catch (core->exceptor, &scope))
  {
    JS_FreeValue (ctx, result);
    result = _gum_quick_throw_native (ctx, &scope.exception, core);
  }

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
      _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (address), core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, m, GUM_QUICK_CORE_ATOM (core, size),
      JS_NewUint32 (ctx, size),
      JS_PROP_C_W_E);

  JS_DefinePropertyValueUint32 (ctx, sc->matches, sc->index, m, JS_PROP_C_W_E);
  sc->index++;

  return TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_find_pointers)
{
  JSValue result;
  GArray * ranges;
  JSValue values_value;
  JSValue options = JS_NULL;
  gsize mask = G_MAXSIZE;
  gsize * values;
  guint n_values, i;
  GArray * matches;

  if (!_gum_quick_args_parse (args, "RA|O?", &ranges, &values_value, &options))
    return JS_EXCEPTION;

  if (!JS_IsNull (options))
  {
    JSValue mask_value = JS_GetPropertyStr (ctx, options, "mask");
    gpointer mask_ptr;

    if (JS_IsException (mask_value))
      return JS_EXCEPTION;

    if (!JS_IsUndefined (mask_value))
    {
      if (!_gum_quick_native_pointer_get (ctx, mask_value, core, &mask_ptr))
      {
        JS_FreeValue (ctx, mask_value);
        return JS_EXCEPTION;
      }

      mask = GPOINTER_TO_SIZE (mask_ptr);
    }

    JS_FreeValue (ctx, mask_value);
  }

  if (!gum_quick_memory_values_get (ctx, values_value, core, &values,
      &n_values))
    return JS_EXCEPTION;

  {
    GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);

    _gum_quick_scope_suspend (&scope);

    matches = gum_memory_find_pointers ((GumMemoryRange *) ranges->data,
        ranges->len, values, n_values, GPOINTER_TO_SIZE (mask));

    _gum_quick_scope_resume (&scope);
  }

  result = JS_NewArray (ctx);

  for (i = 0; i != matches->len; i++)
  {
    GumPointerMatch * match = &g_array_index (matches, GumPointerMatch, i);
    JSValue m = JS_NewObject (ctx);

    JS_DefinePropertyValue (ctx, m, GUM_QUICK_CORE_ATOM (core, address),
        _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (match->address),
            core),
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, m, GUM_QUICK_CORE_ATOM (core, value),
        _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (match->value),
            core),
        JS_PROP_C_W_E);

    JS_DefinePropertyValueUint32 (ctx, result, i, m, JS_PROP_C_W_E);
  }

  g_array_free (matches, TRUE);
  g_free (values);

  return result;
}

static gboolean
gum_quick_memory_values_get (JSContext * ctx,
                             JSValueConst val,
                             GumQuickCore * core,
                             gsize ** values,
                             guint * n_values)
{
  gsize * result;
  guint n, i;
  JSValue element = JS_NULL;

  if (!_gum_quick_array_get_length (ctx, val, core, &n))
    return FALSE;

  result = g_new (gsize, n);

  for (i = 0; i != n; i++)
  {
    gpointer ptr;

    element = JS_GetPropertyUint32 (ctx, val, i);
    if (JS_IsException (element))
      goto propagate_exception;

    if (!_gum_quick_native_pointer_get (ctx, element, core, &ptr))
      goto propagate_exception;

    result[i] = GPOINTER_TO_SIZE (ptr);

    JS_FreeValue (ctx, element);
    element = JS_NULL;
  }

  *values = result;
  *n_values = n;

  return TRUE;

propagate_exception:
  {
    JS_FreeValue (ctx, element);
    g_free (result);

    return FALSE;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_access_monitor_enable)
{
  GumQuickMemory * self;
  GArray * ranges;
  JSValue on_access;
  GError * error;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "RF{onAccess}", &ranges, &on_access))
    return JS_EXCEPTION;

  if (ranges->len == 0)
    return _gum_quick_throw_literal (ctx, "expected one or more ranges");

  gum_quick_memory_clear_monitor (self, ctx);

  self->on_access = JS_DupValue (ctx, on_access);
  self->monitor = gum_memory_access_monitor_new (
      (GumMemoryRange *) ranges->data, ranges->len, GUM_PAGE_RWX, TRUE,
      (GumMemoryAccessNotify) gum_quick_memory_on_access, self, NULL);

  if (!gum_memory_access_monitor_enable (self->monitor, &error))
  {
    _gum_quick_throw_error (ctx, &error);

    gum_quick_memory_clear_monitor (self, ctx);

    return JS_EXCEPTION;
  }

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_memory_access_monitor_disable)
{
  GumQuickMemory * self = gumjs_get_parent_module (core);

  gum_quick_memory_clear_monitor (self, ctx);

  return JS_UNDEFINED;
}

static void
gum_quick_memory_clear_monitor (GumQuickMemory * self,
                                JSContext * ctx)
{
  if (self->monitor != NULL)
  {
    gum_memory_access_monitor_disable (self->monitor);
    g_object_unref (self->monitor);
    self->monitor = NULL;
  }

  if (!JS_IsNull (self->on_access))
  {
    JS_FreeValue (ctx, self->on_access);
    self->on_access = JS_NULL;
  }
}

static void
gum_quick_memory_on_access (GumMemoryAccessMonitor * monitor,
                            const GumMemoryAccessDetails * details,
                            GumQuickMemory * self)
{
  GumQuickCore * core = self->core;
  JSContext * ctx = core->ctx;
  GumQuickScope scope;
  JSValue d;
  GumQuickCpuContext * cpu_context;

  _gum_quick_scope_enter (&scope, core);

  d = JS_NewObjectClass (ctx, self->memory_access_details_class);
  JS_SetOpaque (d, (void *) details);

  JS_DefinePropertyValue (ctx, d, GUM_QUICK_CORE_ATOM (core, context),
      _gum_quick_cpu_context_new (ctx, details->context,
          GUM_CPU_CONTEXT_READWRITE, core, &cpu_context),
      JS_PROP_C_W_E);

  _gum_quick_scope_call_void (&scope, self->on_access, JS_UNDEFINED, 1, &d);

  _gum_quick_cpu_context_make_read_only (cpu_context);

  JS_SetOpaque (d, NULL);
  JS_FreeValue (ctx, d);

  _gum_quick_scope_leave (&scope);
}

static gboolean
gum_quick_memory_access_details_get (JSContext * ctx,
                                     JSValueConst val,
                                     GumQuickCore * core,
                                     const GumMemoryAccessDetails ** details)
{
  const GumMemoryAccessDetails * d;

  if (!_gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->memory_access_details_class, core,
      (gpointer *) &d))
    return FALSE;

  if (d == NULL)
  {
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }

  *details = d;
  return TRUE;
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_thread_id)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return JS_NewInt64 (ctx, details->thread_id);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_operation)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return _gum_quick_memory_operation_new (ctx, details->operation);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_from)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, details->from, core);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_address)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, details->address, core);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_range_index)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return JS_NewUint32 (ctx, details->range_index);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_page_index)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return JS_NewUint32 (ctx, details->page_index);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_pages_completed)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return JS_NewUint32 (ctx, details->pages_completed);
}

GUMJS_DEFINE_GETTER (gumjs_memory_access_details_get_pages_total)
{
  const GumMemoryAccessDetails * details;

  if (!gum_quick_memory_access_details_get (ctx, this_val, core, &details))
    return JS_EXCEPTION;

  return JS_NewUint32 (ctx, details->pages_total);
}
