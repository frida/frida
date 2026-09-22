/*
 * Copyright (C) 2020-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 DaVinci <nstefanclaudel13@gmail.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickthread.h"

#include "gumquickmacros.h"

enum _GumBacktracerType
{
  GUM_BACKTRACER_ACCURATE = 1,
  GUM_BACKTRACER_FUZZY = 2
};

GUMJS_DECLARE_FUNCTION (gumjs_thread_backtrace)
GUMJS_DECLARE_FUNCTION (gumjs_thread_sleep)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_thread_construct)
GUMJS_DECLARE_FUNCTION (gumjs_thread_set_hardware_breakpoint)
GUMJS_DECLARE_FUNCTION (gumjs_thread_unset_hardware_breakpoint)
GUMJS_DECLARE_FUNCTION (gumjs_thread_set_hardware_watchpoint)
GUMJS_DECLARE_FUNCTION (gumjs_thread_unset_hardware_watchpoint)

static const JSClassDef gumjs_thread_def =
{
  .class_name = "Thread",
};

static const JSCFunctionListEntry gumjs_thread_module_entries[] =
{
  JS_CFUNC_DEF ("_backtrace", 0, gumjs_thread_backtrace),
  JS_CFUNC_DEF ("sleep", 0, gumjs_thread_sleep),
};

static const JSCFunctionListEntry gumjs_thread_entries[] =
{
  JS_CFUNC_DEF ("setHardwareBreakpoint", 0,
      gumjs_thread_set_hardware_breakpoint),
  JS_CFUNC_DEF ("unsetHardwareBreakpoint", 0,
      gumjs_thread_unset_hardware_breakpoint),
  JS_CFUNC_DEF ("setHardwareWatchpoint", 0,
      gumjs_thread_set_hardware_watchpoint),
  JS_CFUNC_DEF ("unsetHardwareWatchpoint", 0,
      gumjs_thread_unset_hardware_watchpoint),
};

static const JSCFunctionListEntry gumjs_backtracer_entries[] =
{
  JS_PROP_INT32_DEF ("ACCURATE", GUM_BACKTRACER_ACCURATE, JS_PROP_C_W_E),
  JS_PROP_INT32_DEF ("FUZZY", GUM_BACKTRACER_FUZZY, JS_PROP_C_W_E),
};

void
_gum_quick_thread_init (GumQuickThread * self,
                        JSValue ns,
                        GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj, proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "thread", self);

  _gum_quick_create_class (ctx, &gumjs_thread_def, core, &self->thread_class,
      &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_thread_construct,
      gumjs_thread_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, ctor, gumjs_thread_module_entries,
      G_N_ELEMENTS (gumjs_thread_module_entries));
  JS_SetPropertyFunctionList (ctx, proto, gumjs_thread_entries,
      G_N_ELEMENTS (gumjs_thread_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_thread_def.class_name, ctor,
      JS_PROP_C_W_E);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_backtracer_entries,
      G_N_ELEMENTS (gumjs_backtracer_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Backtracer", obj, JS_PROP_C_W_E);
}

void
_gum_quick_thread_dispose (GumQuickThread * self)
{
}

void
_gum_quick_thread_finalize (GumQuickThread * self)
{
  g_clear_pointer (&self->accurate_backtracer, g_object_unref);
  g_clear_pointer (&self->fuzzy_backtracer, g_object_unref);
}

static GumQuickThread *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "thread");
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_backtrace)
{
  JSValue result;
  GumQuickThread * self;
  GumCpuContext * cpu_context;
  gint type;
  guint limit;
  GumBacktracer * backtracer;
  GumReturnAddressArray ret_addrs;
  guint i;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "C?iu", &cpu_context, &type, &limit))
    return JS_EXCEPTION;

  if (type != GUM_BACKTRACER_ACCURATE && type != GUM_BACKTRACER_FUZZY)
    goto invalid_type;

  if (type == GUM_BACKTRACER_ACCURATE)
  {
    if (self->accurate_backtracer == NULL)
      self->accurate_backtracer = gum_backtracer_make_accurate ();
    backtracer = self->accurate_backtracer;
  }
  else
  {
    if (self->fuzzy_backtracer == NULL)
      self->fuzzy_backtracer = gum_backtracer_make_fuzzy ();
    backtracer = self->fuzzy_backtracer;
  }
  if (backtracer == NULL)
    goto not_available;

  if (limit != 0)
  {
    gum_backtracer_generate_with_limit (backtracer, cpu_context, &ret_addrs,
        limit);
  }
  else
  {
    gum_backtracer_generate (backtracer, cpu_context, &ret_addrs);
  }

  result = JS_NewArray (ctx);

  for (i = 0; i != ret_addrs.len; i++)
  {
    JS_DefinePropertyValueUint32 (ctx, result, i,
        _gum_quick_native_pointer_new (ctx, ret_addrs.items[i], core),
        JS_PROP_C_W_E);
  }

  return result;

invalid_type:
  {
    return _gum_quick_throw_literal (ctx, "invalid backtracer enum value");
  }
not_available:
  {
    return _gum_quick_throw_literal (ctx, (type == GUM_BACKTRACER_ACCURATE)
        ? "backtracer not yet available for this platform; "
        "please try Thread.backtrace(context, Backtracer.FUZZY)"
        : "backtracer not yet available for this platform; "
        "please try Thread.backtrace(context, Backtracer.ACCURATE)");
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_sleep)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  gdouble delay;

  if (!_gum_quick_args_parse (args, "n", &delay))
    return JS_EXCEPTION;

  if (delay < 0)
    return JS_UNDEFINED;

  _gum_quick_scope_suspend (&scope);

  g_usleep ((gulong) (delay * G_USEC_PER_SEC));

  _gum_quick_scope_resume (&scope);

  return JS_UNDEFINED;
}

JSValue
_gum_quick_thread_new (JSContext * ctx,
                       const GumThreadDetails * details,
                       GumQuickThread * parent)
{
  GumQuickCore * core = parent->core;
  JSValue thread;

  thread = JS_NewObjectClass (ctx, parent->thread_class);

  JS_SetOpaque (thread, GSIZE_TO_POINTER (details->id));

  JS_DefinePropertyValue (ctx, thread,
      GUM_QUICK_CORE_ATOM (core, id),
      JS_NewInt64 (ctx, details->id),
      JS_PROP_C_W_E);

  if ((details->flags & GUM_THREAD_FLAGS_NAME) != 0)
  {
    JS_DefinePropertyValue (ctx, thread,
        GUM_QUICK_CORE_ATOM (core, name),
        JS_NewString (ctx, details->name),
        JS_PROP_C_W_E);
  }

  if ((details->flags & GUM_THREAD_FLAGS_STATE) != 0)
  {
    JS_DefinePropertyValue (ctx, thread,
        GUM_QUICK_CORE_ATOM (core, state),
        _gum_quick_thread_state_new (ctx, details->state),
        JS_PROP_C_W_E);
  }

  if ((details->flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
  {
    JS_DefinePropertyValue (ctx, thread,
        GUM_QUICK_CORE_ATOM (core, context),
        _gum_quick_cpu_context_new (ctx,
          (GumCpuContext *) &details->cpu_context, GUM_CPU_CONTEXT_READONLY,
          core, NULL),
        JS_PROP_C_W_E);
  }

  if ((details->flags & (GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE |
          GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER)) != 0)
  {
    const GumThreadEntrypoint * ep = &details->entrypoint;
    JSValue obj;

    obj = JS_NewObject (ctx);
    if ((details->flags & GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE) != 0)
    {
      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, routine),
          _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (ep->routine),
            core),
          JS_PROP_C_W_E);
    }
    if ((details->flags & GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER) != 0)
    {
      JS_DefinePropertyValue (ctx, obj,
          GUM_QUICK_CORE_ATOM (core, parameter),
          _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (ep->parameter),
            core),
          JS_PROP_C_W_E);
    }

    JS_DefinePropertyValue (ctx, thread, GUM_QUICK_CORE_ATOM (core, entrypoint),
        obj, JS_PROP_C_W_E);
  }

  return thread;
}

static gboolean
gum_thread_get (JSContext * ctx,
                JSValueConst val,
                GumQuickCore * core,
                GumThreadId * thread_id)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->thread_class, core,
      (gpointer *) thread_id);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_thread_construct)
{
  JSValue wrapper = JS_NULL;
  GumThreadId thread_id;
  JSValue proto;

  if (!_gum_quick_args_parse (args, "Z", &thread_id))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto,
      gumjs_get_parent_module (core)->thread_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  JS_SetOpaque (wrapper, GSIZE_TO_POINTER (thread_id));

  return wrapper;
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_set_hardware_breakpoint)
{
  GumThreadId thread_id;
  guint breakpoint_id;
  gpointer address;
  GError * error;

  if (!gum_thread_get (ctx, this_val, core, &thread_id))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "up", &breakpoint_id, &address))
    return JS_EXCEPTION;

  error = NULL;
  gum_thread_set_hardware_breakpoint (thread_id, breakpoint_id,
      GUM_ADDRESS (address), &error);
  if (error != NULL)
    return _gum_quick_throw_error (ctx, &error);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_unset_hardware_breakpoint)
{
  GumThreadId thread_id;
  guint breakpoint_id;
  GError * error;

  if (!gum_thread_get (ctx, this_val, core, &thread_id))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "u", &breakpoint_id))
    return JS_EXCEPTION;

  error = NULL;
  gum_thread_unset_hardware_breakpoint (thread_id, breakpoint_id, &error);
  if (error != NULL)
    return _gum_quick_throw_error (ctx, &error);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_set_hardware_watchpoint)
{
  GumThreadId thread_id;
  guint watchpoint_id;
  gpointer address;
  gsize size;
  const gchar * conditions_str;
  GumWatchConditions conditions;
  const gchar * ch;
  GError * error;

  if (!gum_thread_get (ctx, this_val, core, &thread_id))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "upZs", &watchpoint_id, &address, &size,
        &conditions_str))
    return JS_EXCEPTION;

  conditions = 0;
  for (ch = conditions_str; *ch != '\0'; ch++)
  {
    switch (*ch)
    {
      case 'r':
        conditions |= GUM_WATCH_READ;
        break;
      case 'w':
        conditions |= GUM_WATCH_WRITE;
        break;
      default:
        goto invalid_conditions;
    }
  }
  if (conditions == 0)
    goto invalid_conditions;

  error = NULL;
  gum_thread_set_hardware_watchpoint (thread_id, watchpoint_id,
      GUM_ADDRESS (address), size, conditions, &error);
  if (error != NULL)
    return _gum_quick_throw_error (ctx, &error);

  return JS_UNDEFINED;

invalid_conditions:
  {
    _gum_quick_throw_literal (ctx,
        "expected a string specifying watch conditions, e.g. 'rw'");
    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_unset_hardware_watchpoint)
{
  GumThreadId thread_id;
  guint watchpoint_id;
  GError * error;

  if (!gum_thread_get (ctx, this_val, core, &thread_id))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "u", &watchpoint_id))
    return JS_EXCEPTION;

  error = NULL;
  gum_thread_unset_hardware_watchpoint (thread_id, watchpoint_id, &error);
  if (error != NULL)
    return _gum_quick_throw_error (ctx, &error);

  return JS_UNDEFINED;
}
