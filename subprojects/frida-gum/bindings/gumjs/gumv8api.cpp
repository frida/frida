/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8api.h"

#include "gumscriptapi-priv.h"
#include "gumv8value.h"

using namespace v8;

struct GumV8ApiEntry
{
  const GumScriptApiFunction * function;
  GumV8Core * core;
};

static void gum_v8_api_expose (GumV8Api * self, GumScriptApi * api,
    Local<ObjectTemplate> scope);
static void gum_v8_api_run_prelude (GumV8Api * self, GumScriptApi * api);
static void gum_v8_api_on_call (const FunctionCallbackInfo<Value> & info);
static gboolean gum_v8_api_read_argument (Local<Value> value,
    GumScriptApiType type, GumV8Core * core, GumScriptApiValue * result,
    GPtrArray * strings);
static void gum_v8_api_return (const FunctionCallbackInfo<Value> & info,
    const GumScriptApiValue * value, GumV8Core * core);

void
_gum_v8_api_init (GumV8Api * self,
                  GumV8Core * core,
                  Local<ObjectTemplate> scope)
{
  GPtrArray * apis;
  guint i;

  self->core = core;
  self->entries = g_ptr_array_new_with_free_func (g_free);

  apis = _gum_script_api_registry_snapshot (gum_script_api_registry_obtain ());
  self->apis = apis;

  for (i = 0; i != apis->len; i++)
  {
    gum_v8_api_expose (self, (GumScriptApi *) g_ptr_array_index (apis, i),
        scope);
  }
}

static void
gum_v8_api_expose (GumV8Api * self,
                   GumScriptApi * api,
                   Local<ObjectTemplate> scope)
{
  auto isolate = self->core->isolate;
  GArray * functions;
  guint i;

  functions = _gum_script_api_peek_functions (api);

  auto ns = _gum_v8_create_module (_gum_script_api_get_name (api), scope,
      isolate);

  for (i = 0; i != functions->len; i++)
  {
    auto function = &g_array_index (functions, GumScriptApiFunction, i);

    auto entry = g_new (GumV8ApiEntry, 1);
    entry->function = function;
    entry->core = self->core;
    g_ptr_array_add (self->entries, entry);

    ns->Set (_gum_v8_string_new_ascii (isolate, function->name),
        FunctionTemplate::New (isolate, gum_v8_api_on_call,
            External::New (isolate, entry)));
  }
}

void
_gum_v8_api_realize (GumV8Api * self)
{
  guint i;

  for (i = 0; i != self->apis->len; i++)
  {
    gum_v8_api_run_prelude (self,
        (GumScriptApi *) g_ptr_array_index (self->apis, i));
  }
}

static void
gum_v8_api_run_prelude (GumV8Api * self,
                        GumScriptApi * api)
{
  auto core = self->core;
  auto isolate = core->isolate;
  const gchar * source;

  source = _gum_script_api_get_prelude (api);
  if (source == NULL)
    return;

  auto context = isolate->GetCurrentContext ();

  TryCatch trycatch (isolate);

  Local<Script> code;
  if (Script::Compile (context,
      String::NewFromUtf8 (isolate, source).ToLocalChecked ()).ToLocal (&code))
  {
    Local<Value> result;
    code->Run (context).ToLocal (&result);
  }

  if (trycatch.HasCaught ())
  {
    String::Utf8Value message (isolate, trycatch.Exception ());
    g_warning ("%s prelude failed: %s", _gum_script_api_get_name (api),
        *message);
  }
}

static void
gum_v8_api_on_call (const FunctionCallbackInfo<Value> & info)
{
  auto entry = (GumV8ApiEntry *) info.Data ().As<External> ()->Value ();
  auto function = entry->function;
  auto core = entry->core;
  auto isolate = core->isolate;
  GumScriptApiValue * args;
  GumScriptApiValue retval;
  GPtrArray * strings;
  GError * error = NULL;
  guint i;

  if ((guint) info.Length () < function->arity)
  {
    _gum_v8_throw (isolate, "%s() expects %u argument(s)", function->name,
        function->arity);
    return;
  }

  strings = g_ptr_array_new_with_free_func (g_free);

  args = g_newa (GumScriptApiValue, function->arity);
  for (i = 0; i != function->arity; i++)
  {
    if (!gum_v8_api_read_argument (info[i], function->arg_types[i], core,
        &args[i], strings))
    {
      g_ptr_array_unref (strings);
      return;
    }
  }

  retval.type = function->retval_type;

  if (function->func (args, &retval, function->user_data, &error))
    gum_v8_api_return (info, &retval, core);
  else
    _gum_v8_throw_literal (isolate, error->message);

  g_clear_error (&error);
  g_ptr_array_unref (strings);
}

static gboolean
gum_v8_api_read_argument (Local<Value> value,
                          GumScriptApiType type,
                          GumV8Core * core,
                          GumScriptApiValue * result,
                          GPtrArray * strings)
{
  auto isolate = core->isolate;

  result->type = type;

  switch (type)
  {
    case GUM_SCRIPT_API_BOOLEAN:
      result->b = value->BooleanValue (isolate);
      return TRUE;
    case GUM_SCRIPT_API_INT:
      return _gum_v8_int_get (value, &result->i, core);
    case GUM_SCRIPT_API_UINT:
      return _gum_v8_uint_get (value, &result->u, core);
    case GUM_SCRIPT_API_INT64:
      return _gum_v8_int64_get (value, &result->i64, core);
    case GUM_SCRIPT_API_UINT64:
      return _gum_v8_uint64_get (value, &result->u64, core);
    case GUM_SCRIPT_API_NUMBER:
    {
      Local<Number> n;
      if (!value->ToNumber (isolate->GetCurrentContext ()).ToLocal (&n))
        return FALSE;
      result->n = n->Value ();
      return TRUE;
    }
    case GUM_SCRIPT_API_STRING:
    {
      String::Utf8Value str (isolate, value);
      if (*str == NULL)
      {
        _gum_v8_throw_ascii_literal (isolate, "expected a string");
        return FALSE;
      }
      result->s = g_strdup (*str);
      g_ptr_array_add (strings, (gpointer) result->s);
      return TRUE;
    }
    case GUM_SCRIPT_API_POINTER:
      return _gum_v8_native_pointer_get (value, &result->p, core);
    default:
      g_assert_not_reached ();
  }
}

static void
gum_v8_api_return (const FunctionCallbackInfo<Value> & info,
                   const GumScriptApiValue * value,
                   GumV8Core * core)
{
  auto isolate = core->isolate;
  auto retval = info.GetReturnValue ();

  switch (value->type)
  {
    case GUM_SCRIPT_API_VOID:
      break;
    case GUM_SCRIPT_API_BOOLEAN:
      retval.Set (!!value->b);
      break;
    case GUM_SCRIPT_API_INT:
      retval.Set (value->i);
      break;
    case GUM_SCRIPT_API_UINT:
      retval.Set (value->u);
      break;
    case GUM_SCRIPT_API_INT64:
      retval.Set (_gum_v8_int64_new (value->i64, core));
      break;
    case GUM_SCRIPT_API_UINT64:
      retval.Set (_gum_v8_uint64_new (value->u64, core));
      break;
    case GUM_SCRIPT_API_NUMBER:
      retval.Set (value->n);
      break;
    case GUM_SCRIPT_API_STRING:
      if (value->s != NULL)
        retval.Set (String::NewFromUtf8 (isolate, value->s).ToLocalChecked ());
      else
        retval.SetNull ();
      break;
    case GUM_SCRIPT_API_POINTER:
      retval.Set (_gum_v8_native_pointer_new (value->p, core));
      break;
    default:
      g_assert_not_reached ();
  }
}

void
_gum_v8_api_finalize (GumV8Api * self)
{
  g_ptr_array_unref (self->entries);
  g_ptr_array_unref (self->apis);
}
