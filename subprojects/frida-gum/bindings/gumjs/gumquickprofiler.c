/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickprofiler.h"

#include "gumquickmacros.h"

typedef struct _GumQuickProfilerValue GumQuickProfilerValue;
typedef struct _GumQuickWorstCaseInspector GumQuickWorstCaseInspector;

struct _GumQuickProfilerValue
{
  GumProfiler * handle;
  GHashTable * inspectors;

  GumQuickProfiler * parent;
};

struct _GumQuickWorstCaseInspector
{
  JSValue callback;

  GumQuickProfilerValue * profiler;
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_profiler_construct)
GUMJS_DECLARE_FINALIZER (gumjs_profiler_finalize)
GUMJS_DECLARE_GC_MARKER (gumjs_profiler_gc_mark)
GUMJS_DECLARE_FUNCTION (gumjs_profiler_instrument)
GUMJS_DECLARE_FUNCTION (gumjs_profiler_generate_report)

static void gum_quick_worst_case_inspector_free (
    GumQuickWorstCaseInspector * inspector);
static void gum_quick_worst_case_inspector_inspect (GumInvocationContext * ic,
    gchar * output_buf, guint output_buf_len, gpointer user_data);

static const JSClassDef gumjs_profiler_def =
{
  .class_name = "Profiler",
  .finalizer = gumjs_profiler_finalize,
  .gc_mark = gumjs_profiler_gc_mark,
};

static const JSCFunctionListEntry gumjs_profiler_functions[] =
{
  JS_CFUNC_DEF ("instrument", 0, gumjs_profiler_instrument),
  JS_CFUNC_DEF ("generateReport", 0, gumjs_profiler_generate_report),
};

void
_gum_quick_profiler_init (GumQuickProfiler * self,
                          JSValue ns,
                          GumQuickSampler * sampler,
                          GumQuickInterceptor * interceptor,
                          GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->sampler = sampler;
  self->interceptor = interceptor;
  self->core = core;

  _gum_quick_core_store_module_data (core, "profiler", self);

  _gum_quick_create_class (ctx, &gumjs_profiler_def, core,
      &self->profiler_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_profiler_construct,
      gumjs_profiler_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_profiler_functions,
      G_N_ELEMENTS (gumjs_profiler_functions));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_profiler_def.class_name, ctor,
      JS_PROP_C_W_E);
}

void
_gum_quick_profiler_dispose (GumQuickProfiler * self)
{
}

void
_gum_quick_profiler_finalize (GumQuickProfiler * self)
{
}

static GumQuickProfiler *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "profiler");
}

gboolean
_gum_quick_profiler_get (JSContext * ctx,
                         JSValue val,
                         GumQuickProfiler * parent,
                         GumQuickProfilerValue ** profiler)
{
  return _gum_quick_unwrap (ctx, val, parent->profiler_class, parent->core,
      (gpointer *) profiler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_profiler_construct)
{
  JSValue wrapper = JS_NULL;
  GumQuickProfiler * parent;
  JSValue proto;
  GumQuickProfilerValue * profiler;

  parent = gumjs_get_parent_module (core);

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, parent->profiler_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  profiler = g_slice_new (GumQuickProfilerValue);
  profiler->handle = gum_profiler_new ();
  profiler->inspectors = g_hash_table_new (NULL, NULL);
  profiler->parent = parent;

  JS_SetOpaque (wrapper, profiler);

  return wrapper;

propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FINALIZER (gumjs_profiler_finalize)
{
  GumQuickProfilerValue * profiler;

  profiler = JS_GetOpaque (val, gumjs_get_parent_module (core)->profiler_class);

  g_object_unref (profiler->handle);
  g_hash_table_unref (profiler->inspectors);

  g_slice_free (GumQuickProfilerValue, profiler);
}

GUMJS_DEFINE_GC_MARKER (gumjs_profiler_gc_mark)
{
  GumQuickProfilerValue * self;
  GHashTableIter iter;
  GumQuickWorstCaseInspector * inspector;

  self = JS_GetOpaque (val, gumjs_get_parent_module (core)->profiler_class);

  g_hash_table_iter_init (&iter, self->inspectors);
  while (g_hash_table_iter_next (&iter, (gpointer *) &inspector, NULL))
    JS_MarkValue (rt, inspector->callback, mark_func);
}

GUMJS_DEFINE_FUNCTION (gumjs_profiler_instrument)
{
  GumQuickProfiler * parent;
  GumQuickProfilerValue * self;
  gpointer function_address;
  JSValue sampler_val, describe;
  GumSampler * sampler;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_profiler_get (ctx, this_val, parent, &self))
    return JS_EXCEPTION;

  describe = JS_NULL;
  if (!_gum_quick_args_parse (args, "pO|F{describe?}", &function_address,
        &sampler_val, &describe))
    return JS_EXCEPTION;

  if (!_gum_quick_sampler_get (ctx, sampler_val, parent->sampler, &sampler))
    return JS_EXCEPTION;

  if (!JS_IsNull (describe))
  {
    GumQuickWorstCaseInspector * inspector;

    inspector = g_slice_new (GumQuickWorstCaseInspector);
    inspector->callback = JS_DupValue (ctx, describe);
    inspector->profiler = self;

    g_hash_table_add (self->inspectors, inspector);

    gum_profiler_instrument_function_with_inspector (self->handle,
        function_address, sampler, gum_quick_worst_case_inspector_inspect,
        inspector, (GDestroyNotify) gum_quick_worst_case_inspector_free);
  }
  else
  {
    gum_profiler_instrument_function (self->handle, function_address, sampler);
  }

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_profiler_generate_report)
{
  JSValue result;
  GumQuickProfilerValue * self;
  GumProfileReport * report;
  gchar * xml;

  if (!_gum_quick_profiler_get (ctx, this_val, gumjs_get_parent_module (core),
        &self))
    return JS_EXCEPTION;

  report = gum_profiler_generate_report (self->handle);

  xml = gum_profile_report_emit_xml (report);
  result = JS_NewString (ctx, xml);
  g_free (xml);

  g_object_unref (report);

  return result;
}

static void
gum_quick_worst_case_inspector_free (GumQuickWorstCaseInspector * inspector)
{
  GumQuickProfilerValue * profiler = inspector->profiler;

  g_hash_table_remove (profiler->inspectors, inspector);

  JS_FreeValue (profiler->parent->core->ctx, inspector->callback);

  g_slice_free (GumQuickWorstCaseInspector, inspector);
}

static void
gum_quick_worst_case_inspector_inspect (GumInvocationContext * ic,
                                        gchar * output_buf,
                                        guint output_buf_len,
                                        gpointer user_data)
{
  GumQuickWorstCaseInspector * self = user_data;
  GumQuickProfiler * parent = self->profiler->parent;
  GumQuickInterceptor * interceptor = parent->interceptor;
  GumQuickCore * core = parent->core;
  JSContext * ctx = core->ctx;
  GumQuickScope scope;
  GumQuickInvocationContext * jic;
  GumQuickInvocationArgs * args;
  JSValue result;

  _gum_quick_scope_enter (&scope, core);

  jic = _gum_quick_interceptor_obtain_invocation_context (interceptor);
  _gum_quick_invocation_context_reset (jic, ic);

  args = _gum_quick_interceptor_obtain_invocation_args (interceptor);
  _gum_quick_invocation_args_reset (args, ic);

  result = _gum_quick_scope_call (&scope, self->callback, jic->wrapper, 1,
      &args->wrapper);
  if (!JS_IsException (result))
  {
    if (JS_IsString (result))
    {
      const char * str = JS_ToCString (ctx, result);
      g_strlcpy (output_buf, str, output_buf_len);
      JS_FreeCString (ctx, str);
    }
    else
    {
      _gum_quick_throw_literal (ctx, "describe() must return a string");
      _gum_quick_scope_catch_and_emit (&scope);
    }

    JS_FreeValue (ctx, result);
  }

  _gum_quick_invocation_args_reset (args, NULL);
  _gum_quick_interceptor_release_invocation_args (interceptor, args);

  _gum_quick_invocation_context_reset (jic, NULL);
  _gum_quick_interceptor_release_invocation_context (interceptor, jic);

  _gum_quick_scope_leave (&scope);
}
