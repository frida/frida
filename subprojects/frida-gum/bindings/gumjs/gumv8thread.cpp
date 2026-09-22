/*
 * Copyright (C) 2010-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 DaVinci <nstefanclaudel13@gmail.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8thread.h"

#include "gumv8macros.h"

#define GUMJS_MODULE_NAME Thread

#define GUMJS_THREAD_ID(o) \
    (o)->GetInternalField (0).As<BigInt> ()->Uint64Value ()

using namespace v8;

GUMJS_DECLARE_FUNCTION (gumjs_thread_backtrace)
GUMJS_DECLARE_FUNCTION (gumjs_thread_sleep)

GUMJS_DECLARE_FUNCTION (gumjs_thread_set_hardware_breakpoint)
GUMJS_DECLARE_FUNCTION (gumjs_thread_unset_hardware_breakpoint)
GUMJS_DECLARE_FUNCTION (gumjs_thread_set_hardware_watchpoint)
GUMJS_DECLARE_FUNCTION (gumjs_thread_unset_hardware_watchpoint)

static const GumV8Function gumjs_thread_module_functions[] =
{
  { "_backtrace", gumjs_thread_backtrace },
  { "sleep", gumjs_thread_sleep },

  { NULL, NULL }
};

static const GumV8Function gumjs_thread_functions[] =
{
  { "setHardwareBreakpoint", gumjs_thread_set_hardware_breakpoint },
  { "unsetHardwareBreakpoint", gumjs_thread_unset_hardware_breakpoint },
  { "setHardwareWatchpoint", gumjs_thread_set_hardware_watchpoint },
  { "unsetHardwareWatchpoint", gumjs_thread_unset_hardware_watchpoint },

  { NULL, NULL }
};

void
_gum_v8_thread_init (GumV8Thread * self,
                     GumV8Core * core,
                     Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto klass = _gum_v8_create_class ("Thread", nullptr, scope, module, isolate);
  _gum_v8_class_add_static (klass, gumjs_thread_module_functions, module,
      isolate);
  _gum_v8_class_add (klass, gumjs_thread_functions, module, isolate);
  self->klass = new Global<FunctionTemplate> (isolate, klass);

  _gum_v8_create_module ("Backtracer", scope, isolate);
}

void
_gum_v8_thread_realize (GumV8Thread * self)
{
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto backtracer = context->Global ()->Get (context,
      _gum_v8_string_new_ascii (isolate, "Backtracer")).ToLocalChecked ()
      .As<Object> ();

  auto accurate = Symbol::ForApi (isolate,
      _gum_v8_string_new_ascii (isolate, "Backtracer.ACCURATE"));
  backtracer->DefineOwnProperty (context,
      _gum_v8_string_new_ascii (isolate, "ACCURATE"), accurate,
      (PropertyAttribute) (ReadOnly | DontDelete)).ToChecked ();
  self->accurate_enum_value = new Global<Symbol> (isolate, accurate);

  auto fuzzy = Symbol::ForApi (isolate,
      _gum_v8_string_new_ascii (isolate, "Backtracer.FUZZY"));
  backtracer->DefineOwnProperty (context,
      _gum_v8_string_new_ascii (isolate, "FUZZY"), fuzzy,
      (PropertyAttribute) (ReadOnly | DontDelete)).ToChecked ();
  self->fuzzy_enum_value = new Global<Symbol> (isolate, fuzzy);
}

void
_gum_v8_thread_dispose (GumV8Thread * self)
{
  delete self->fuzzy_enum_value;
  self->fuzzy_enum_value = nullptr;

  delete self->accurate_enum_value;
  self->accurate_enum_value = nullptr;

  delete self->klass;
  self->klass = nullptr;
}

void
_gum_v8_thread_finalize (GumV8Thread * self)
{
  g_clear_object (&self->accurate_backtracer);
  g_clear_object (&self->fuzzy_backtracer);
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_backtrace)
{
  auto context = isolate->GetCurrentContext ();

  GumCpuContext * cpu_context = NULL;
  Local<Value> raw_type;
  guint limit;
  if (!_gum_v8_args_parse (args, "C?Vu", &cpu_context, &raw_type, &limit))
    return;

  if (!raw_type->IsSymbol ())
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid backtracer value");
    return;
  }
  Local<Symbol> type = raw_type.As<Symbol> ();
  gboolean accurate = TRUE;
  if (type->StrictEquals (
        Local<Symbol>::New (isolate, *module->fuzzy_enum_value)))
  {
    accurate = FALSE;
  }
  else if (!type->StrictEquals (
        Local<Symbol>::New (isolate, *module->accurate_enum_value)))
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid backtracer enum value");
    return;
  }

  GumBacktracer * backtracer;
  if (accurate)
  {
    if (module->accurate_backtracer == NULL)
      module->accurate_backtracer = gum_backtracer_make_accurate ();
    backtracer = module->accurate_backtracer;
  }
  else
  {
    if (module->fuzzy_backtracer == NULL)
      module->fuzzy_backtracer = gum_backtracer_make_fuzzy ();
    backtracer = module->fuzzy_backtracer;
  }
  if (backtracer == NULL)
  {
    _gum_v8_throw_ascii_literal (isolate, accurate
            ? "backtracer not yet available for this platform; "
            "please try Thread.backtrace(context, Backtracer.FUZZY)"
            : "backtracer not yet available for this platform; "
            "please try Thread.backtrace(context, Backtracer.ACCURATE)");
    return;
  }

  GumReturnAddressArray ret_addrs;
  if (limit != 0)
  {
    gum_backtracer_generate_with_limit (backtracer, cpu_context, &ret_addrs,
        limit);
  }
  else
  {
    gum_backtracer_generate (backtracer, cpu_context, &ret_addrs);
  }

  auto result = Array::New (isolate, ret_addrs.len);
  for (guint i = 0; i != ret_addrs.len; i++)
  {
    result->Set (context, i,
        _gum_v8_native_pointer_new (ret_addrs.items[i], core)).Check ();
  }
  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_sleep)
{
  gdouble delay;

  if (!_gum_v8_args_parse (args, "n", &delay))
    return;

  if (delay < 0)
    return;

  {
    ScriptUnlocker unlocker (core);

    g_usleep (delay * G_USEC_PER_SEC);
  }
}

Local<Object>
_gum_v8_thread_new (const GumThreadDetails * details,
                    GumV8Thread * module)
{
  auto core = module->core;
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto klass = Local<FunctionTemplate>::New (isolate, *module->klass);
  auto thread = klass->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 0, nullptr).ToLocalChecked ();
  thread->SetInternalField (0, BigInt::NewFromUnsigned (isolate, details->id));

  _gum_v8_object_set (thread, "id", Number::New (isolate, details->id), core);

  if ((details->flags & GUM_THREAD_FLAGS_NAME) != 0)
  {
    _gum_v8_object_set_utf8 (thread, "name", details->name, core);
  }

  if ((details->flags & GUM_THREAD_FLAGS_STATE) != 0)
  {
    _gum_v8_object_set (thread, "state", _gum_v8_string_new_ascii (isolate,
        _gum_v8_thread_state_to_string (details->state)), core);
  }

  if ((details->flags & GUM_THREAD_FLAGS_CPU_CONTEXT) != 0)
  {
    auto cpu_context =
        _gum_v8_cpu_context_new_immutable (&details->cpu_context, core);
    _gum_v8_object_set (thread, "context", cpu_context, core);
    _gum_v8_cpu_context_free_later (new Global<Object> (isolate, cpu_context),
        core);
  }

  if ((details->flags & (GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE |
          GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER)) != 0)
  {
    const GumThreadEntrypoint * ep = &details->entrypoint;

    auto obj = Object::New (isolate);
    if ((details->flags & GUM_THREAD_FLAGS_ENTRYPOINT_ROUTINE) != 0)
      _gum_v8_object_set_pointer (obj, "routine", ep->routine, core);
    if ((details->flags & GUM_THREAD_FLAGS_ENTRYPOINT_PARAMETER) != 0)
      _gum_v8_object_set_pointer (obj, "parameter", ep->parameter, core);

    _gum_v8_object_set (thread, "entrypoint", obj, core);
  }

  return thread;
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_set_hardware_breakpoint)
{
  GumThreadId thread_id = GUMJS_THREAD_ID (info.Holder ());

  guint breakpoint_id;
  gpointer address;
  if (!_gum_v8_args_parse (args, "up", &breakpoint_id, &address))
    return;

  GError * error = NULL;
  gum_thread_set_hardware_breakpoint (thread_id, breakpoint_id,
      GUM_ADDRESS (address), &error);
  _gum_v8_maybe_throw (isolate, &error);
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_unset_hardware_breakpoint)
{
  GumThreadId thread_id = GUMJS_THREAD_ID (info.Holder ());

  guint breakpoint_id;
  if (!_gum_v8_args_parse (args, "u", &breakpoint_id))
    return;

  GError * error = NULL;
  gum_thread_unset_hardware_breakpoint (thread_id, breakpoint_id, &error);
  _gum_v8_maybe_throw (isolate, &error);
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_set_hardware_watchpoint)
{
  GumThreadId thread_id = GUMJS_THREAD_ID (info.Holder ());

  guint watchpoint_id;
  gpointer address;
  gsize size;
  gchar * conditions_str;
  if (!_gum_v8_args_parse (args, "upZs", &watchpoint_id, &address, &size,
        &conditions_str))
  {
    return;
  }

  auto conditions = (GumWatchConditions) 0;
  bool conditions_valid = true;
  for (const gchar * ch = conditions_str; *ch != '\0'; ch++)
  {
    switch (*ch)
    {
      case 'r':
        conditions = (GumWatchConditions) (conditions | GUM_WATCH_READ);
        break;
      case 'w':
        conditions = (GumWatchConditions) (conditions | GUM_WATCH_WRITE);
        break;
      default:
        conditions_valid = false;
        break;
    }
  }

  g_free (conditions_str);

  if (conditions == 0 || !conditions_valid)
  {
    _gum_v8_throw_ascii_literal (isolate,
        "expected a string specifying watch conditions, e.g. 'rw'");
    return;
  }

  GError * error = NULL;
  gum_thread_set_hardware_watchpoint (thread_id, watchpoint_id,
      GUM_ADDRESS (address), size, conditions, &error);
  _gum_v8_maybe_throw (isolate, &error);
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_unset_hardware_watchpoint)
{
  GumThreadId thread_id = GUMJS_THREAD_ID (info.Holder ());

  guint watchpoint_id;
  if (!_gum_v8_args_parse (args, "u", &watchpoint_id))
    return;

  GError * error = NULL;
  gum_thread_unset_hardware_watchpoint (thread_id, watchpoint_id, &error);
  _gum_v8_maybe_throw (isolate, &error);
}
