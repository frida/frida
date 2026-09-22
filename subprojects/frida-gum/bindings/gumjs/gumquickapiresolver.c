/*
 * Copyright (C) 2020-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickapiresolver.h"

#include "gumquickmacros.h"

#include <gum/gumapiresolver.h>

typedef struct _GumQuickEnumerateContext GumQuickEnumerateContext;

struct _GumQuickEnumerateContext
{
  JSValue elements;
  guint n;

  JSContext * ctx;
  GumQuickCore * core;
};

GUMJS_DECLARE_CONSTRUCTOR (gumjs_api_resolver_construct)
GUMJS_DECLARE_FUNCTION (gumjs_api_resolver_enumerate_matches)
static gboolean gum_emit_match (const GumApiDetails * details,
    GumQuickEnumerateContext * ec);

static void gum_quick_enumerate_context_begin (GumQuickEnumerateContext * ec,
    GumQuickCore * core);
static JSValue gum_quick_enumerate_context_end (GumQuickEnumerateContext * ec);
static void gum_quick_enumerate_context_cancel (GumQuickEnumerateContext * ec);
static gboolean gum_quick_enumerate_context_collect (
    GumQuickEnumerateContext * ec, JSValue element);

static const JSClassDef gumjs_api_resolver_def =
{
  .class_name = "ApiResolver",
};

static const JSCFunctionListEntry gumjs_api_resolver_entries[] =
{
  JS_CFUNC_DEF ("enumerateMatches", 0, gumjs_api_resolver_enumerate_matches),
};

void
_gum_quick_api_resolver_init (GumQuickApiResolver * self,
                              JSValue ns,
                              GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue proto, ctor;

  self->core = core;

  _gum_quick_core_store_module_data (core, "api-resolver", self);

  _gum_quick_create_class (ctx, &gumjs_api_resolver_def, core,
      &self->api_resolver_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_api_resolver_construct,
      gumjs_api_resolver_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_api_resolver_entries,
      G_N_ELEMENTS (gumjs_api_resolver_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_api_resolver_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_object_manager_init (&self->objects, self, core);
}

void
_gum_quick_api_resolver_dispose (GumQuickApiResolver * self)
{
  _gum_quick_object_manager_free (&self->objects);
}

void
_gum_quick_api_resolver_finalize (GumQuickApiResolver * self)
{
}

static GumQuickApiResolver *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "api-resolver");
}

static gboolean
gum_quick_api_resolver_get (JSContext * ctx,
                            JSValueConst val,
                            GumQuickCore * core,
                            GumQuickObject ** object)
{
  return _gum_quick_unwrap (ctx, val,
      gumjs_get_parent_module (core)->api_resolver_class, core,
      (gpointer *) object);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_api_resolver_construct)
{
  JSValue wrapper = JS_NULL;
  GumQuickApiResolver * parent;
  const gchar * type;
  JSValue proto;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  GumApiResolver * resolver;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "s", &type))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, parent->api_resolver_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  _gum_quick_scope_suspend (&scope);

  resolver = gum_api_resolver_make (type);

  _gum_quick_scope_resume (&scope);

  if (resolver == NULL)
    goto not_available;

  _gum_quick_object_manager_add (&parent->objects, ctx, wrapper, resolver);

  return wrapper;

not_available:
  {
    _gum_quick_throw_literal (ctx,
        "the specified ApiResolver is not available");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_api_resolver_enumerate_matches)
{
  GumQuickObject * self;
  const gchar * query;
  GumQuickEnumerateContext ec;
  GError * error;

  if (!gum_quick_api_resolver_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "s", &query))
    return JS_EXCEPTION;

  gum_quick_enumerate_context_begin (&ec, core);

  error = NULL;
  gum_api_resolver_enumerate_matches (self->handle, query,
      (GumFoundApiFunc) gum_emit_match, &ec, &error);
  if (error != NULL)
  {
    gum_quick_enumerate_context_cancel (&ec);
    return _gum_quick_throw_error (ctx, &error);
  }

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_match (const GumApiDetails * details,
                GumQuickEnumerateContext * ec)
{
  JSContext * ctx = ec->ctx;
  GumQuickCore * core = ec->core;
  JSValue match;

  match = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, match,
      GUM_QUICK_CORE_ATOM (core, name),
      JS_NewString (ctx, details->name),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, match,
      GUM_QUICK_CORE_ATOM (core, address),
      _gum_quick_native_pointer_new (ctx, GSIZE_TO_POINTER (details->address),
          core),
      JS_PROP_C_W_E);
  if (details->size != GUM_API_SIZE_NONE)
  {
    JS_DefinePropertyValue (ctx, match,
        GUM_QUICK_CORE_ATOM (core, size),
        JS_NewUint32 (ctx, details->size),
        JS_PROP_C_W_E);
  }

  return gum_quick_enumerate_context_collect (ec, match);
}

static void
gum_quick_enumerate_context_begin (GumQuickEnumerateContext * ec,
                                   GumQuickCore * core)
{
  ec->elements = JS_NewArray (core->ctx);
  ec->n = 0;

  ec->ctx = core->ctx;
  ec->core = core;
}

static JSValue
gum_quick_enumerate_context_end (GumQuickEnumerateContext * ec)
{
  return ec->elements;
}

static void
gum_quick_enumerate_context_cancel (GumQuickEnumerateContext * ec)
{
  JS_FreeValue (ec->ctx, ec->elements);
}

static gboolean
gum_quick_enumerate_context_collect (GumQuickEnumerateContext * ec,
                                     JSValue element)
{
  JS_DefinePropertyValueUint32 (ec->ctx, ec->elements, ec->n++, element,
      JS_PROP_C_W_E);
  return TRUE;
}
