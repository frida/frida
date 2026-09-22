/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquicksampler.h"

#include "gumquickmacros.h"

static void gum_quick_sampler_register (GumQuickSampler * self, JSValue ns,
    JSValue parent_proto, const JSClassDef * def, JSCFunction * construct,
    JSClassID * id);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_sampler_construct)
GUMJS_DECLARE_FINALIZER (gumjs_sampler_finalize)
GUMJS_DECLARE_FUNCTION (gumjs_sampler_sample)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_cycle_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_busy_cycle_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_wall_clock_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_user_time_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_malloc_count_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_call_count_sampler_construct)

static const JSClassDef gumjs_sampler_def =
{
  .class_name = "Sampler",
  .finalizer = gumjs_sampler_finalize,
};

static const JSCFunctionListEntry gumjs_sampler_functions[] =
{
  JS_CFUNC_DEF ("sample", 0, gumjs_sampler_sample),
};

static const JSClassDef gumjs_cycle_sampler_def =
{
  .class_name = "CycleSampler",
};

static const JSClassDef gumjs_busy_cycle_sampler_def =
{
  .class_name = "BusyCycleSampler",
};

static const JSClassDef gumjs_wall_clock_sampler_def =
{
  .class_name = "WallClockSampler",
};

static const JSClassDef gumjs_user_time_sampler_def =
{
  .class_name = "UserTimeSampler",
};

static const JSClassDef gumjs_malloc_count_sampler_def =
{
  .class_name = "MallocCountSampler",
};

static const JSClassDef gumjs_call_count_sampler_def =
{
  .class_name = "CallCountSampler",
};

void
_gum_quick_sampler_init (GumQuickSampler * self,
                         JSValue ns,
                         GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "sampler", self);

  _gum_quick_create_class (ctx, &gumjs_sampler_def, core, &self->sampler_class,
      &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_sampler_construct,
      gumjs_sampler_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_sampler_functions,
      G_N_ELEMENTS (gumjs_sampler_functions));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_sampler_def.class_name, ctor,
      JS_PROP_C_W_E);

#define GUM_REGISTER_SAMPLER(id) \
    gum_quick_sampler_register (self, ns, proto, \
        &G_PASTE (G_PASTE (gumjs_, id), _sampler_def), \
        G_PASTE (G_PASTE (gumjs_, id), _sampler_construct), \
        &self->G_PASTE (id, _sampler_class))

  GUM_REGISTER_SAMPLER (cycle);
  GUM_REGISTER_SAMPLER (busy_cycle);
  GUM_REGISTER_SAMPLER (wall_clock);
  GUM_REGISTER_SAMPLER (user_time);
  GUM_REGISTER_SAMPLER (malloc_count);
  GUM_REGISTER_SAMPLER (call_count);

#undef GUM_REGISTER_SAMPLER
}

static void
gum_quick_sampler_register (GumQuickSampler * self,
                            JSValue ns,
                            JSValue parent_proto,
                            const JSClassDef * def,
                            JSCFunction * construct,
                            JSClassID * id)
{
  GumQuickCore * core = self->core;
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  _gum_quick_create_subclass (ctx, def, self->sampler_class, parent_proto, core,
      id, &proto);
  ctor = JS_NewCFunction2 (ctx, construct, def->class_name, 0,
      JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_DefinePropertyValueStr (ctx, ns, def->class_name, ctor, JS_PROP_C_W_E);
}

void
_gum_quick_sampler_dispose (GumQuickSampler * self)
{
}

void
_gum_quick_sampler_finalize (GumQuickSampler * self)
{
}

static GumQuickSampler *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "sampler");
}

gboolean
_gum_quick_sampler_get (JSContext * ctx,
                        JSValue val,
                        GumQuickSampler * parent,
                        GumSampler ** sampler)
{
  return _gum_quick_unwrap (ctx, val, parent->sampler_class, parent->core,
      (gpointer *) sampler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_sampler_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FINALIZER (gumjs_sampler_finalize)
{
  GumSampler * sampler;

  _gum_quick_try_unwrap (val, gumjs_get_parent_module (core)->sampler_class,
      core, (gpointer *) &sampler);

  g_object_unref (sampler);
}

GUMJS_DEFINE_FUNCTION (gumjs_sampler_sample)
{
  GumSampler * self;
  GumSample sample;

  if (!_gum_quick_unwrap (ctx, this_val,
        gumjs_get_parent_module (core)->sampler_class, core,
        (gpointer *) &self))
  {
    return JS_EXCEPTION;
  }

  sample = gum_sampler_sample (self);

  return JS_NewBigUint64 (ctx, sample);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_cycle_sampler_construct)
{
  GumSampler * sampler;
  JSValue wrapper;

  sampler = gum_cycle_sampler_new ();
  if (!gum_cycle_sampler_is_available (GUM_CYCLE_SAMPLER (sampler)))
    goto not_available;

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->cycle_sampler_class);
  JS_SetOpaque (wrapper, sampler);

  return wrapper;

not_available:
  {
    g_object_unref (sampler);

    return _gum_quick_throw_literal (ctx, "not available on the current OS");
  }
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_busy_cycle_sampler_construct)
{
  GumSampler * sampler;
  JSValue wrapper;

  sampler = gum_busy_cycle_sampler_new ();
  if (!gum_busy_cycle_sampler_is_available (GUM_BUSY_CYCLE_SAMPLER (sampler)))
    goto not_available;

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->busy_cycle_sampler_class);
  JS_SetOpaque (wrapper, sampler);

  return wrapper;

not_available:
  {
    g_object_unref (sampler);

    return _gum_quick_throw_literal (ctx, "not available on the current OS");
  }
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_wall_clock_sampler_construct)
{
  JSValue wrapper;
  GumSampler * sampler;

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->wall_clock_sampler_class);
  sampler = gum_wall_clock_sampler_new ();
  JS_SetOpaque (wrapper, sampler);

  return wrapper;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_user_time_sampler_construct)
{
  GumThreadId thread_id;
  GumSampler * sampler;
  JSValue wrapper;

  thread_id = gum_process_get_current_thread_id ();
  if (!_gum_quick_args_parse (args, "|Z", &thread_id))
    return JS_EXCEPTION;

  sampler = gum_user_time_sampler_new_with_thread_id (thread_id);
  if (!gum_user_time_sampler_is_available (GUM_USER_TIME_SAMPLER (sampler)))
    goto not_available;

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->user_time_sampler_class);
  JS_SetOpaque (wrapper, sampler);

  return wrapper;

not_available:
  {
    g_object_unref (sampler);

    return _gum_quick_throw_literal (ctx, "not available on the current OS");
  }
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_malloc_count_sampler_construct)
{
  JSValue wrapper;
  GumHeapApiList * apis;
  GumSampler * sampler;

  apis = gum_process_find_heap_apis ();
  sampler = gum_malloc_count_sampler_new_with_heap_apis (apis);
  gum_heap_api_list_free (apis);

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->malloc_count_sampler_class);
  JS_SetOpaque (wrapper, sampler);

  return wrapper;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_call_count_sampler_construct)
{
  JSValue result = JS_EXCEPTION;
  JSValue functions_val;
  guint n, i;
  gpointer * functions = NULL;
  JSValue element = JS_NULL;
  JSValue wrapper;
  GumSampler * sampler;

  if (!_gum_quick_args_parse (args, "A", &functions_val))
    return JS_EXCEPTION;

  if (!_gum_quick_array_get_length (ctx, functions_val, core, &n))
    return JS_EXCEPTION;

  functions = g_new (gpointer, n);

  for (i = 0; i != n; i++)
  {
    element = JS_GetPropertyUint32 (ctx, functions_val, i);
    if (JS_IsException (element))
      goto beach;

    if (!_gum_quick_native_pointer_get (ctx, element, core, &functions[i]))
      goto expected_array_of_pointers;

    JS_FreeValue (ctx, element);
    element = JS_NULL;
  }

  wrapper = JS_NewObjectClass (ctx,
      gumjs_get_parent_module (core)->call_count_sampler_class);
  sampler = gum_call_count_sampler_newv (functions, n);
  JS_SetOpaque (wrapper, sampler);

  result = wrapper;
  goto beach;

expected_array_of_pointers:
  {
    _gum_quick_throw_literal (ctx, "expected an array of NativePointer values");
    goto beach;
  }
beach:
  {
    JS_FreeValue (ctx, element);
    g_free (functions);

    return result;
  }
}
