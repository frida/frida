/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8profiler.h"

#include "gumv8macros.h"

#include <gum/gum-prof.h>

#define GUMJS_MODULE_NAME Profiler

using namespace v8;

struct GumV8WorstCaseInspector
{
  Global<Function> * callback;

  GumV8Profiler * module;
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_profiler_construct)
GUMJS_DECLARE_FUNCTION (gumjs_profiler_instrument)
GUMJS_DECLARE_FUNCTION (gumjs_profiler_generate_report)

static void gum_v8_worst_case_inspector_free (
    GumV8WorstCaseInspector * inspector);
static void gum_v8_worst_case_inspector_inspect (GumInvocationContext * ic,
    gchar * output_buf, guint output_buf_len, gpointer user_data);

static const GumV8Function gumjs_profiler_functions[] =
{
  { "instrument", gumjs_profiler_instrument },
  { "generateReport", gumjs_profiler_generate_report },

  { NULL, NULL }
};

void
_gum_v8_profiler_init (GumV8Profiler * self,
                       GumV8Sampler * sampler,
                       GumV8Interceptor * interceptor,
                       GumV8Core * core,
                       Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->sampler = sampler;
  self->interceptor = interceptor;
  self->core = core;

  auto module = External::New (isolate, self);

  auto profiler = _gum_v8_create_class ("Profiler", gumjs_profiler_construct,
      scope, module, isolate);
  _gum_v8_class_add (profiler, gumjs_profiler_functions, module, isolate);
}

void
_gum_v8_profiler_realize (GumV8Profiler * self)
{
  gum_v8_object_manager_init (&self->objects);
}

void
_gum_v8_profiler_flush (GumV8Profiler * self)
{
  gum_v8_object_manager_flush (&self->objects);
}

void
_gum_v8_profiler_dispose (GumV8Profiler * self)
{
  gum_v8_object_manager_free (&self->objects);
}

void
_gum_v8_profiler_finalize (GumV8Profiler * self)
{
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_profiler_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new Profiler()` to create a new instance");
    return;
  }

  auto profiler = gum_profiler_new ();
  gum_v8_object_manager_add (&module->objects, wrapper, profiler, module);
  wrapper->SetAlignedPointerInInternalField (0, profiler);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_profiler_instrument, GumProfiler)
{
  gpointer function_address;
  Local<Object> sampler_val;
  Local<Function> describe;
  if (!_gum_v8_args_parse (args, "pO|F{describe?}", &function_address,
        &sampler_val, &describe))
    return;

  GumSampler * sampler;
  if (!_gum_v8_sampler_get (sampler_val, &sampler, module->sampler))
    return;

  if (!describe.IsEmpty ())
  {
    auto inspector = g_slice_new (GumV8WorstCaseInspector);
    inspector->callback = new Global<Function> (isolate, describe);
    inspector->module = module;

    gum_profiler_instrument_function_with_inspector (self, function_address,
        sampler, gum_v8_worst_case_inspector_inspect, inspector,
        (GDestroyNotify) gum_v8_worst_case_inspector_free);
  }
  else
  {
    gum_profiler_instrument_function (self, function_address, sampler);
  }
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_profiler_generate_report, GumProfiler)
{
  auto report = gum_profiler_generate_report (self);

  auto xml = gum_profile_report_emit_xml (report);
  info.GetReturnValue ().Set (
      String::NewFromUtf8 (isolate, xml).ToLocalChecked ());
  g_free (xml);

  g_object_unref (report);
}

static void
gum_v8_worst_case_inspector_free (GumV8WorstCaseInspector * inspector)
{
  delete inspector->callback;

  g_slice_free (GumV8WorstCaseInspector, inspector);
}

static void
gum_v8_worst_case_inspector_inspect (GumInvocationContext * ic,
                                     gchar * output_buf,
                                     guint output_buf_len,
                                     gpointer user_data)
{
  auto self = (GumV8WorstCaseInspector *) user_data;
  auto module = self->module;
  auto interceptor = module->interceptor;

  auto core = module->core;
  ScriptScope scope (core->script);
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto callback = Local<Function>::New (isolate, *self->callback);

  auto jic = _gum_v8_interceptor_obtain_invocation_context (interceptor);
  _gum_v8_invocation_context_reset (jic, ic);
  auto recv = Local<Object>::New (isolate, *jic->object);

  auto args = _gum_v8_interceptor_obtain_invocation_args (interceptor);
  _gum_v8_invocation_args_reset (args, ic);
  auto args_object = Local<Object>::New (isolate, *args->object);

  Local<Value> argv[] = { args_object };
  Local<Value> result;
  if (callback->Call (context, recv, G_N_ELEMENTS (argv), argv)
      .ToLocal (&result))
  {
    if (result->IsString ())
    {
      String::Utf8Value str (isolate, result);
      g_strlcpy (output_buf, *str, output_buf_len);
    }
    else
    {
      _gum_v8_throw_ascii_literal (isolate, "describe() must return a string");
    }
  }

  _gum_v8_invocation_args_reset (args, NULL);
  _gum_v8_interceptor_release_invocation_args (interceptor, args);

  _gum_v8_invocation_context_reset (jic, NULL);
  _gum_v8_interceptor_release_invocation_context (interceptor, jic);
}
