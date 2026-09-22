/*
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickcloak.h"

#include "gumquickmacros.h"

#include <gum/gumcloak.h>

GUMJS_DECLARE_FUNCTION (gumjs_cloak_add_thread)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_remove_thread)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_has_thread)

GUMJS_DECLARE_FUNCTION (gumjs_cloak_add_range)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_remove_range)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_has_range_containing)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_clip_range)

GUMJS_DECLARE_FUNCTION (gumjs_cloak_add_file_descriptor)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_remove_file_descriptor)
GUMJS_DECLARE_FUNCTION (gumjs_cloak_has_file_descriptor)

static const JSCFunctionListEntry gumjs_cloak_entries[] =
{
  JS_CFUNC_DEF ("addThread", 0, gumjs_cloak_add_thread),
  JS_CFUNC_DEF ("removeThread", 0, gumjs_cloak_remove_thread),
  JS_CFUNC_DEF ("hasThread", 0, gumjs_cloak_has_thread),

  JS_CFUNC_DEF ("_addRange", 0, gumjs_cloak_add_range),
  JS_CFUNC_DEF ("_removeRange", 0, gumjs_cloak_remove_range),
  JS_CFUNC_DEF ("hasRangeContaining", 0, gumjs_cloak_has_range_containing),
  JS_CFUNC_DEF ("_clipRange", 0, gumjs_cloak_clip_range),

  JS_CFUNC_DEF ("addFileDescriptor", 0, gumjs_cloak_add_file_descriptor),
  JS_CFUNC_DEF ("removeFileDescriptor", 0, gumjs_cloak_remove_file_descriptor),
  JS_CFUNC_DEF ("hasFileDescriptor", 0, gumjs_cloak_has_file_descriptor),
};

void
_gum_quick_cloak_init (GumQuickCloak * self,
                       JSValue ns,
                       GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj;

  self->core = core;

  _gum_quick_core_store_module_data (core, "cloak", self);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_cloak_entries,
      G_N_ELEMENTS (gumjs_cloak_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Cloak", obj, JS_PROP_C_W_E);
}

void
_gum_quick_cloak_dispose (GumQuickCloak * self)
{
}

void
_gum_quick_cloak_finalize (GumQuickCloak * self)
{
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_add_thread)
{
  GumThreadId thread_id;

  if (!_gum_quick_args_parse (args, "Z", &thread_id))
    return JS_EXCEPTION;

  gum_cloak_add_thread (thread_id);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_remove_thread)
{
  GumThreadId thread_id;

  if (!_gum_quick_args_parse (args, "Z", &thread_id))
    return JS_EXCEPTION;

  gum_cloak_remove_thread (thread_id);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_has_thread)
{
  GumThreadId thread_id;

  if (!_gum_quick_args_parse (args, "Z", &thread_id))
    return JS_EXCEPTION;

  return JS_NewBool (ctx, gum_cloak_has_thread (thread_id));
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_add_range)
{
  gpointer address;
  gsize size;
  GumMemoryRange range;

  if (!_gum_quick_args_parse (args, "pZ", &address, &size))
    return JS_EXCEPTION;

  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  gum_cloak_add_range (&range);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_remove_range)
{
  gpointer address;
  gsize size;
  GumMemoryRange range;

  if (!_gum_quick_args_parse (args, "pZ", &address, &size))
    return JS_EXCEPTION;

  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  gum_cloak_remove_range (&range);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_has_range_containing)
{
  gpointer address;

  if (!_gum_quick_args_parse (args, "p", &address))
    return JS_EXCEPTION;

  return JS_NewBool (ctx, gum_cloak_has_range_containing (
        GUM_ADDRESS (address)));
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_clip_range)
{
  JSValue result;
  gpointer address;
  gsize size;
  GumMemoryRange range;
  GArray * visible;
  guint i;

  if (!_gum_quick_args_parse (args, "pZ", &address, &size))
    return JS_EXCEPTION;

  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  visible = gum_cloak_clip_range (&range);
  if (visible == NULL)
    return JS_NULL;

  result = JS_NewArray (ctx);
  for (i = 0; i != visible->len; i++)
  {
    const GumMemoryRange * r = &g_array_index (visible, GumMemoryRange, i);

    JS_DefinePropertyValueUint32 (ctx, result, i,
        _gum_quick_memory_range_new (ctx, r, core),
        JS_PROP_C_W_E);
  }

  g_array_free (visible, TRUE);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_add_file_descriptor)
{
  gint fd;

  if (!_gum_quick_args_parse (args, "i", &fd))
    return JS_EXCEPTION;

  gum_cloak_add_file_descriptor (fd);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_remove_file_descriptor)
{
  gint fd;

  if (!_gum_quick_args_parse (args, "i", &fd))
    return JS_EXCEPTION;

  gum_cloak_remove_file_descriptor (fd);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_has_file_descriptor)
{
  gint fd;

  if (!_gum_quick_args_parse (args, "i", &fd))
    return JS_EXCEPTION;

  return JS_NewBool (ctx, gum_cloak_has_file_descriptor (fd));
}
