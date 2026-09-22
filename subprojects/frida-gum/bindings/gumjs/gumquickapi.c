/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickapi.h"

#include "gumquickvalue.h"
#include "gumscriptapi-priv.h"

static void gum_quick_api_expose (GumQuickApi * self, GumScriptApi * api,
    JSValue ns);
static void gum_quick_api_run_prelude (GumQuickApi * self, GumScriptApi * api);
static JSValue gum_quick_api_on_call (JSContext * ctx, JSValueConst this_val,
    int argc, JSValueConst * argv, int magic, JSValue * func_data);
static gboolean gum_quick_api_read_argument (JSContext * ctx,
    JSValueConst value, GumScriptApiType type, GumQuickCore * core,
    GumScriptApiValue * result, GSList ** strings);
static JSValue gum_quick_api_value_new (JSContext * ctx,
    const GumScriptApiValue * value, GumQuickCore * core);

void
_gum_quick_api_init (GumQuickApi * self,
                     JSValue ns,
                     GumQuickCore * core)
{
  GPtrArray * apis;
  guint i;

  self->core = core;
  self->entries = g_ptr_array_new ();
  _gum_quick_core_store_module_data (core, "api", self);

  apis = _gum_script_api_registry_snapshot (gum_script_api_registry_obtain ());
  self->apis = apis;

  for (i = 0; i != apis->len; i++)
    gum_quick_api_expose (self, g_ptr_array_index (apis, i), ns);

  for (i = 0; i != apis->len; i++)
    gum_quick_api_run_prelude (self, g_ptr_array_index (apis, i));
}

static void
gum_quick_api_expose (GumQuickApi * self,
                      GumScriptApi * api,
                      JSValue ns)
{
  JSContext * ctx = self->core->ctx;
  GArray * functions;
  JSValue obj;
  guint i;

  functions = _gum_script_api_peek_functions (api);

  obj = JS_NewObject (ctx);

  for (i = 0; i != functions->len; i++)
  {
    const GumScriptApiFunction * function =
        &g_array_index (functions, GumScriptApiFunction, i);
    JSValue data, wrapper;

    g_ptr_array_add (self->entries, (gpointer) function);

    data = JS_NewInt32 (ctx, (gint32) self->entries->len - 1);
    wrapper = JS_NewCFunctionData (ctx, gum_quick_api_on_call,
        (int) function->arity, 0, 1, &data);
    JS_FreeValue (ctx, data);

    JS_DefinePropertyValueStr (ctx, obj, function->name, wrapper,
        JS_PROP_C_W_E);
  }

  JS_DefinePropertyValueStr (ctx, ns, _gum_script_api_get_name (api), obj,
      JS_PROP_C_W_E);
}

static void
gum_quick_api_run_prelude (GumQuickApi * self,
                           GumScriptApi * api)
{
  JSContext * ctx = self->core->ctx;
  const gchar * source;
  gchar * name;
  JSValue result;

  source = _gum_script_api_get_prelude (api);
  if (source == NULL)
    return;

  name = g_strconcat ("/_frida_", _gum_script_api_get_name (api), ".js", NULL);

  result = JS_Eval (ctx, source, strlen (source), name,
      JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
  if (JS_IsException (result))
  {
    JSValue exception = JS_GetException (ctx);
    const char * message = JS_ToCString (ctx, exception);

    g_warning ("%s prelude failed: %s", _gum_script_api_get_name (api),
        message);

    JS_FreeCString (ctx, message);
    JS_FreeValue (ctx, exception);
  }

  JS_FreeValue (ctx, result);
  g_free (name);
}

static JSValue
gum_quick_api_on_call (JSContext * ctx,
                       JSValueConst this_val,
                       int argc,
                       JSValueConst * argv,
                       int magic,
                       JSValue * func_data)
{
  JSValue result;
  GumQuickCore * core;
  GumQuickApi * self;
  const GumScriptApiFunction * function;
  GumScriptApiValue * args;
  GumScriptApiValue retval;
  GSList * strings = NULL;
  GError * error = NULL;
  gint32 index;
  guint i;

  core = JS_GetContextOpaque (ctx);
  JS_ToInt32 (ctx, &index, func_data[0]);
  self = _gum_quick_core_load_module_data (core, "api");
  function = g_ptr_array_index (self->entries, index);

  if ((guint) argc < function->arity)
  {
    return _gum_quick_throw (ctx, "%s() expects %u argument(s)",
        function->name, function->arity);
  }

  args = g_newa (GumScriptApiValue, function->arity);
  for (i = 0; i != function->arity; i++)
  {
    if (!gum_quick_api_read_argument (ctx, argv[i], function->arg_types[i],
        core, &args[i], &strings))
    {
      g_slist_free_full (strings, (GDestroyNotify) JS_FreeCString);
      return JS_EXCEPTION;
    }
  }

  retval.type = function->retval_type;

  if (function->func (args, &retval, function->user_data, &error))
    result = gum_quick_api_value_new (ctx, &retval, core);
  else
    result = _gum_quick_throw_error (ctx, &error);

  g_slist_free_full (strings, (GDestroyNotify) JS_FreeCString);

  return result;
}

static gboolean
gum_quick_api_read_argument (JSContext * ctx,
                             JSValueConst value,
                             GumScriptApiType type,
                             GumQuickCore * core,
                             GumScriptApiValue * result,
                             GSList ** strings)
{
  gboolean success;

  result->type = type;

  switch (type)
  {
    case GUM_SCRIPT_API_BOOLEAN:
      success = _gum_quick_boolean_get (ctx, value, &result->b);
      break;
    case GUM_SCRIPT_API_INT:
      success = _gum_quick_int_get (ctx, value, &result->i);
      break;
    case GUM_SCRIPT_API_UINT:
      success = _gum_quick_uint_get (ctx, value, &result->u);
      break;
    case GUM_SCRIPT_API_INT64:
      success = _gum_quick_int64_get (ctx, value, core, &result->i64);
      break;
    case GUM_SCRIPT_API_UINT64:
      success = _gum_quick_uint64_get (ctx, value, core, &result->u64);
      break;
    case GUM_SCRIPT_API_NUMBER:
      success = _gum_quick_float64_get (ctx, value, &result->n);
      break;
    case GUM_SCRIPT_API_STRING:
      success = _gum_quick_string_get (ctx, value, &result->s);
      if (success)
        *strings = g_slist_prepend (*strings, (gpointer) result->s);
      break;
    case GUM_SCRIPT_API_POINTER:
      success = _gum_quick_native_pointer_get (ctx, value, core, &result->p);
      break;
    default:
      g_assert_not_reached ();
  }

  return success;
}

static JSValue
gum_quick_api_value_new (JSContext * ctx,
                         const GumScriptApiValue * value,
                         GumQuickCore * core)
{
  switch (value->type)
  {
    case GUM_SCRIPT_API_VOID:
      return JS_UNDEFINED;
    case GUM_SCRIPT_API_BOOLEAN:
      return JS_NewBool (ctx, value->b);
    case GUM_SCRIPT_API_INT:
      return JS_NewInt32 (ctx, value->i);
    case GUM_SCRIPT_API_UINT:
      return JS_NewUint32 (ctx, value->u);
    case GUM_SCRIPT_API_INT64:
      return _gum_quick_int64_new (ctx, value->i64, core);
    case GUM_SCRIPT_API_UINT64:
      return _gum_quick_uint64_new (ctx, value->u64, core);
    case GUM_SCRIPT_API_NUMBER:
      return JS_NewFloat64 (ctx, value->n);
    case GUM_SCRIPT_API_STRING:
      return (value->s != NULL) ? JS_NewString (ctx, value->s) : JS_NULL;
    case GUM_SCRIPT_API_POINTER:
      return _gum_quick_native_pointer_new (ctx, value->p, core);
    default:
      g_assert_not_reached ();
  }
}

void
_gum_quick_api_finalize (GumQuickApi * self)
{
  g_ptr_array_unref (self->entries);
  g_ptr_array_unref (self->apis);
}
