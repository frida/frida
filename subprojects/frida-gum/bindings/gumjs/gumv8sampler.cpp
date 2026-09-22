/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8sampler.h"

#include "gumv8macros.h"

#define GUMJS_MODULE_NAME Sampler

using namespace v8;

GUMJS_DECLARE_CONSTRUCTOR (gumjs_sampler_construct)
GUMJS_DECLARE_FUNCTION (gumjs_sampler_sample)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_cycle_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_busy_cycle_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_wall_clock_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_user_time_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_malloc_count_sampler_construct)
GUMJS_DECLARE_CONSTRUCTOR (gumjs_call_count_sampler_construct)

static const GumV8Function gumjs_sampler_functions[] =
{
  { "sample", gumjs_sampler_sample },

  { NULL, NULL }
};

void
_gum_v8_sampler_init (GumV8Sampler * self,
                      GumV8Core * core,
                      Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto sampler = _gum_v8_create_class ("Sampler", gumjs_sampler_construct,
      scope, module, isolate);
  _gum_v8_class_add (sampler, gumjs_sampler_functions, module, isolate);
  self->klass = new Global<FunctionTemplate> (isolate, sampler);

#define GUM_REGISTER_SAMPLER(id, name) \
  auto G_PASTE (id, _sampler) = _gum_v8_create_class (name "Sampler", \
      G_PASTE (G_PASTE (gumjs_, id), _sampler_construct), scope, module, \
      isolate); \
  G_PASTE (id, _sampler)->Inherit (sampler)

  GUM_REGISTER_SAMPLER (cycle, "Cycle");
  GUM_REGISTER_SAMPLER (busy_cycle, "BusyCycle");
  GUM_REGISTER_SAMPLER (wall_clock, "WallClock");
  GUM_REGISTER_SAMPLER (user_time, "UserTime");
  GUM_REGISTER_SAMPLER (malloc_count, "MallocCount");
  GUM_REGISTER_SAMPLER (call_count, "CallCount");

#undef GUM_REGISTER_SAMPLER
}

void
_gum_v8_sampler_realize (GumV8Sampler * self)
{
  gum_v8_object_manager_init (&self->objects);
}

void
_gum_v8_sampler_flush (GumV8Sampler * self)
{
  gum_v8_object_manager_flush (&self->objects);
}

void
_gum_v8_sampler_dispose (GumV8Sampler * self)
{
  gum_v8_object_manager_free (&self->objects);

  delete self->klass;
  self->klass = nullptr;
}

void
_gum_v8_sampler_finalize (GumV8Sampler * self)
{
}

gboolean
_gum_v8_sampler_get (Local<Value> value,
                     GumSampler ** sampler,
                     GumV8Sampler * module)
{
  auto isolate = module->core->isolate;

  Local<FunctionTemplate> klass (Local<FunctionTemplate>::New (isolate,
        *module->klass));
  if (!klass->HasInstance (value))
  {
    _gum_v8_throw_ascii_literal (isolate, "expected a Sampler object");
    return FALSE;
  }

  *sampler = (GumSampler *)
      value.As<Object> ()->GetAlignedPointerFromInternalField (0);
  return TRUE;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_sampler_construct)
{
  _gum_v8_throw_ascii_literal (isolate, "not user-instantiable");
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_sampler_sample, GumSampler)
{
  auto sample = gum_sampler_sample (self);

  info.GetReturnValue ().Set (BigInt::NewFromUnsigned (isolate, sample));
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_cycle_sampler_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new CycleSampler()` to create a new instance");
    return;
  }

  auto sampler = gum_cycle_sampler_new ();
  if (!gum_cycle_sampler_is_available (GUM_CYCLE_SAMPLER (sampler)))
  {
    g_object_unref (sampler);

    _gum_v8_throw_ascii_literal (isolate, "not available on the current OS");
    return;
  }

  gum_v8_object_manager_add (&module->objects, wrapper, sampler, module);
  wrapper->SetAlignedPointerInInternalField (0, sampler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_busy_cycle_sampler_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new BusyCycleSampler()` to create a new instance");
    return;
  }

  auto sampler = gum_busy_cycle_sampler_new ();
  if (!gum_busy_cycle_sampler_is_available (GUM_BUSY_CYCLE_SAMPLER (sampler)))
  {
    g_object_unref (sampler);

    _gum_v8_throw_ascii_literal (isolate, "not available on the current OS");
    return;
  }

  gum_v8_object_manager_add (&module->objects, wrapper, sampler, module);
  wrapper->SetAlignedPointerInInternalField (0, sampler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_wall_clock_sampler_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new WallClockSampler()` to create a new instance");
    return;
  }

  auto sampler = gum_wall_clock_sampler_new ();
  gum_v8_object_manager_add (&module->objects, wrapper, sampler, module);
  wrapper->SetAlignedPointerInInternalField (0, sampler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_user_time_sampler_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new UserTimeSampler()` to create a new instance");
    return;
  }

  auto thread_id = gum_process_get_current_thread_id ();
  if (!_gum_v8_args_parse (args, "|Z", &thread_id))
    return;

  auto sampler = gum_user_time_sampler_new_with_thread_id (thread_id);
  if (!gum_user_time_sampler_is_available (GUM_USER_TIME_SAMPLER (sampler)))
  {
    g_object_unref (sampler);

    _gum_v8_throw_ascii_literal (isolate, "not available on the current OS");
    return;
  }

  gum_v8_object_manager_add (&module->objects, wrapper, sampler, module);
  wrapper->SetAlignedPointerInInternalField (0, sampler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_malloc_count_sampler_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new MallocCountSampler()` to create a new instance");
    return;
  }

  auto sampler = gum_malloc_count_sampler_new ();
  gum_v8_object_manager_add (&module->objects, wrapper, sampler, module);
  wrapper->SetAlignedPointerInInternalField (0, sampler);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_call_count_sampler_construct)
{
  auto context = isolate->GetCurrentContext ();

  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new CallCountSampler()` to create a new instance");
    return;
  }

  Local<Array> functions_val;
  if (!_gum_v8_args_parse (args, "A", &functions_val))
    return;

  uint32_t n = functions_val->Length ();
  gpointer * functions = g_new (gpointer, n);

  for (uint32_t i = 0; i != n; i++)
  {
    Local<Value> element;
    if (!functions_val->Get (context, i).ToLocal (&element) ||
        !_gum_v8_native_pointer_get (element, &functions[i], core))
    {
      g_free (functions);
      return;
    }
  }

  auto sampler = gum_call_count_sampler_newv (functions, n);

  gum_v8_object_manager_add (&module->objects, wrapper, sampler, module);
  wrapper->SetAlignedPointerInInternalField (0, sampler);

  g_free (functions);
}
