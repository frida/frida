/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Matt Oh <oh.jeongwook@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquicksymbol.h"

#include "gumquickmacros.h"

#include <gum/gumsymbolutil.h>

typedef struct _GumSymbol GumSymbol;

struct _GumSymbol
{
  gboolean resolved;
  GumDebugSymbolDetails details;
};

GUMJS_DECLARE_FUNCTION (gumjs_symbol_from_address)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_from_name)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_get_function_by_name)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_find_functions_named)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_find_functions_matching)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_load)

static JSValue gum_symbol_new (JSContext * ctx, GumQuickSymbol * parent,
    GumSymbol ** symbol);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_symbol_construct)
GUMJS_DECLARE_FINALIZER (gumjs_symbol_finalize)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_address)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_name)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_module_name)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_file_name)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_line_number)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_column)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_to_string)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_to_json)

static JSValue gum_quick_pointer_array_new (JSContext * ctx, GArray * pointers,
    GumQuickCore * core);

static const JSClassDef gumjs_symbol_def =
{
  .class_name = "DebugSymbol",
  .finalizer = gumjs_symbol_finalize,
};

static const JSCFunctionListEntry gumjs_symbol_module_entries[] =
{
  JS_CFUNC_DEF ("fromAddress", 0, gumjs_symbol_from_address),
  JS_CFUNC_DEF ("fromName", 0, gumjs_symbol_from_name),
  JS_CFUNC_DEF ("getFunctionByName", 0, gumjs_symbol_get_function_by_name),
  JS_CFUNC_DEF ("findFunctionsNamed", 0, gumjs_symbol_find_functions_named),
  JS_CFUNC_DEF ("findFunctionsMatching", 0,
      gumjs_symbol_find_functions_matching),
  JS_CFUNC_DEF ("load", 0, gumjs_symbol_load),
};

static const JSCFunctionListEntry gumjs_symbol_entries[] =
{
  JS_CGETSET_DEF ("address", gumjs_symbol_get_address, NULL),
  JS_CGETSET_DEF ("name", gumjs_symbol_get_name, NULL),
  JS_CGETSET_DEF ("moduleName", gumjs_symbol_get_module_name, NULL),
  JS_CGETSET_DEF ("fileName", gumjs_symbol_get_file_name, NULL),
  JS_CGETSET_DEF ("lineNumber", gumjs_symbol_get_line_number, NULL),
  JS_CGETSET_DEF ("column", gumjs_symbol_get_column, NULL),
  JS_CFUNC_DEF ("toString", 0, gumjs_symbol_to_string),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_symbol_to_json),
};

void
_gum_quick_symbol_init (GumQuickSymbol * self,
                        JSValue ns,
                        GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "debug-symbol", self);

  _gum_quick_create_class (ctx, &gumjs_symbol_def, core, &self->symbol_class,
      &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_symbol_construct,
      gumjs_symbol_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, ctor, gumjs_symbol_module_entries,
      G_N_ELEMENTS (gumjs_symbol_module_entries));
  JS_SetPropertyFunctionList (ctx, proto, gumjs_symbol_entries,
      G_N_ELEMENTS (gumjs_symbol_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_symbol_def.class_name, ctor,
      JS_PROP_C_W_E);
}

void
_gum_quick_symbol_dispose (GumQuickSymbol * self)
{
}

void
_gum_quick_symbol_finalize (GumQuickSymbol * self)
{
}

static GumQuickSymbol *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "debug-symbol");
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_from_address)
{
  JSValue wrapper;
  gpointer address;
  GumSymbol * sym;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);

  if (!_gum_quick_args_parse (args, "p", &address))
    return JS_EXCEPTION;

  wrapper = gum_symbol_new (ctx, gumjs_get_parent_module (core), &sym);

  sym->details.address = GPOINTER_TO_SIZE (address);

  _gum_quick_scope_suspend (&scope);

  sym->resolved = gum_symbol_details_from_address (address, &sym->details);

  _gum_quick_scope_resume (&scope);

  return wrapper;
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_from_name)
{
  JSValue wrapper;
  const gchar * name;
  GumSymbol * sym;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  gpointer address;

  if (!_gum_quick_args_parse (args, "s", &name))
    return JS_EXCEPTION;

  wrapper = gum_symbol_new (ctx, gumjs_get_parent_module (core), &sym);

  _gum_quick_scope_suspend (&scope);

  address = gum_find_function (name);
  if (address != NULL)
    sym->resolved = gum_symbol_details_from_address (address, &sym->details);

  _gum_quick_scope_resume (&scope);

  return wrapper;
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_get_function_by_name)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  const gchar * name;
  gpointer address;

  if (!_gum_quick_args_parse (args, "s", &name))
    return JS_EXCEPTION;

  _gum_quick_scope_suspend (&scope);

  address = gum_find_function (name);

  _gum_quick_scope_resume (&scope);

  if (address == NULL)
  {
    return _gum_quick_throw (ctx,
        "unable to find function with name '%s'",
        name);
  }

  return _gum_quick_native_pointer_new (ctx, address, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_find_functions_named)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  gchar * name;
  GArray * functions;

  if (!_gum_quick_args_parse (args, "s", &name))
    return JS_EXCEPTION;

  _gum_quick_scope_suspend (&scope);

  functions = gum_find_functions_named (name);

  _gum_quick_scope_resume (&scope);

  return gum_quick_pointer_array_new (ctx, functions, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_find_functions_matching)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  const gchar * str;
  GArray * functions;

  if (!_gum_quick_args_parse (args, "s", &str))
    return JS_EXCEPTION;

  _gum_quick_scope_suspend (&scope);

  functions = gum_find_functions_matching (str);

  _gum_quick_scope_resume (&scope);

  return gum_quick_pointer_array_new (ctx, functions, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_load)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  const gchar * path;
  gboolean success;

  if (!_gum_quick_args_parse (args, "s", &path))
    return JS_EXCEPTION;

  _gum_quick_scope_suspend (&scope);

  success = gum_load_symbols (path);

  _gum_quick_scope_resume (&scope);

  if (!success)
    return _gum_quick_throw_literal (ctx, "unable to load symbols");

  return JS_UNDEFINED;
}

static JSValue
gum_symbol_new (JSContext * ctx,
                GumQuickSymbol * parent,
                GumSymbol ** symbol)
{
  JSValue wrapper;
  GumSymbol * sym;

  wrapper = JS_NewObjectClass (ctx, parent->symbol_class);

  sym = g_slice_new0 (GumSymbol);

  JS_SetOpaque (wrapper, sym);

  *symbol = sym;
  return wrapper;
}

static gboolean
gum_symbol_get (JSContext * ctx,
                JSValueConst val,
                GumQuickCore * core,
                GumSymbol ** symbol)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->symbol_class, core, (gpointer *) symbol);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_symbol_construct)
{
  return _gum_quick_throw_literal (ctx, "not user-instantiable");
}

GUMJS_DEFINE_FINALIZER (gumjs_symbol_finalize)
{
  GumSymbol * s;

  s = JS_GetOpaque (val, gumjs_get_parent_module (core)->symbol_class);
  if (s == NULL)
    return;

  g_slice_free (GumSymbol, s);
}

GUMJS_DEFINE_GETTER (gumjs_symbol_get_address)
{
  GumSymbol * self;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx,
      GSIZE_TO_POINTER (self->details.address), core);
}

GUMJS_DEFINE_GETTER (gumjs_symbol_get_name)
{
  GumSymbol * self;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!self->resolved)
    return JS_NULL;

  return JS_NewString (ctx, self->details.symbol_name);
}

GUMJS_DEFINE_GETTER (gumjs_symbol_get_module_name)
{
  GumSymbol * self;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!self->resolved)
    return JS_NULL;

  return JS_NewString (ctx, self->details.module_name);
}

GUMJS_DEFINE_GETTER (gumjs_symbol_get_file_name)
{
  GumSymbol * self;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!self->resolved)
    return JS_NULL;

  return JS_NewString (ctx, self->details.file_name);
}

GUMJS_DEFINE_GETTER (gumjs_symbol_get_line_number)
{
  GumSymbol * self;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!self->resolved)
    return JS_NULL;

  return JS_NewInt32 (ctx, self->details.line_number);
}

GUMJS_DEFINE_GETTER (gumjs_symbol_get_column)
{
  GumSymbol * self;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!self->resolved)
    return JS_NULL;

  return JS_NewInt32 (ctx, self->details.column);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_to_string)
{
  JSValue result;
  GumSymbol * self;
  const GumDebugSymbolDetails * d;
  GString * s;

  if (!gum_symbol_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  d = &self->details;

  s = g_string_new ("0");

  if (self->resolved)
  {
    g_string_append_printf (s, "x%" G_GINT64_MODIFIER "x %s!%s", d->address,
        d->module_name, d->symbol_name);
    if (d->file_name[0] != '\0')
    {
      if (d->column != 0)
      {
        g_string_append_printf (s, " %s:%u:%u", d->file_name, d->line_number,
            d->column);
      }
      else
      {
        g_string_append_printf (s, " %s:%u", d->file_name, d->line_number);
      }
    }
  }
  else if (d->address != 0)
  {
    g_string_append_printf (s, "x%" G_GINT64_MODIFIER "x", d->address);
  }

  result = JS_NewString (ctx, s->str);

  g_string_free (s, TRUE);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_to_json)
{
  JSValue result;
  guint i;

  result = JS_NewObject (ctx);

  for (i = 0; i != G_N_ELEMENTS (gumjs_symbol_entries); i++)
  {
    const JSCFunctionListEntry * e = &gumjs_symbol_entries[i];
    JSValue val;

    if (e->def_type != JS_DEF_CGETSET)
      continue;

    val = JS_GetPropertyStr (ctx, this_val, e->name);
    if (JS_IsException (val))
      goto propagate_exception;
    JS_SetPropertyStr (ctx, result, e->name, val);
  }

  return result;

propagate_exception:
  {
    JS_FreeValue (ctx, result);

    return JS_EXCEPTION;
  }
}

static JSValue
gum_quick_pointer_array_new (JSContext * ctx,
                             GArray * pointers,
                             GumQuickCore * core)
{
  JSValue result;
  guint i;

  result = JS_NewArray (ctx);

  for (i = 0; i != pointers->len; i++)
  {
    gpointer address = g_array_index (pointers, gpointer, i);

    JS_DefinePropertyValueUint32 (ctx, result, i,
        _gum_quick_native_pointer_new (ctx, address, core),
        JS_PROP_C_W_E);
  }

  g_array_free (pointers, TRUE);

  return result;
}
