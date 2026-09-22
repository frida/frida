/*
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8cloak.h"

#include "gumv8macros.h"

#include <gum/gumcloak.h>

#define GUMJS_MODULE_NAME Cloak

using namespace v8;

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

static const GumV8Function gumjs_cloak_functions[] =
{
  { "addThread", gumjs_cloak_add_thread },
  { "removeThread", gumjs_cloak_remove_thread },
  { "hasThread", gumjs_cloak_has_thread },

  { "_addRange", gumjs_cloak_add_range },
  { "_removeRange", gumjs_cloak_remove_range },
  { "hasRangeContaining", gumjs_cloak_has_range_containing },
  { "_clipRange", gumjs_cloak_clip_range },

  { "addFileDescriptor", gumjs_cloak_add_file_descriptor },
  { "removeFileDescriptor", gumjs_cloak_remove_file_descriptor },
  { "hasFileDescriptor", gumjs_cloak_has_file_descriptor },

  { NULL, NULL }
};

void
_gum_v8_cloak_init (GumV8Cloak * self,
                    GumV8Core * core,
                    Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto cloak = _gum_v8_create_module ("Cloak", scope, isolate);
  _gum_v8_module_add (module, cloak, gumjs_cloak_functions, isolate);
}

void
_gum_v8_cloak_realize (GumV8Cloak * self)
{
}

void
_gum_v8_cloak_dispose (GumV8Cloak * self)
{
}

void
_gum_v8_cloak_finalize (GumV8Cloak * self)
{
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_add_thread)
{
  GumThreadId thread_id;
  if (!_gum_v8_args_parse (args, "Z", &thread_id))
    return;

  gum_cloak_add_thread (thread_id);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_remove_thread)
{
  GumThreadId thread_id;
  if (!_gum_v8_args_parse (args, "Z", &thread_id))
    return;

  gum_cloak_remove_thread (thread_id);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_has_thread)
{
  GumThreadId thread_id;
  if (!_gum_v8_args_parse (args, "Z", &thread_id))
    return;

  info.GetReturnValue ().Set ((bool) gum_cloak_has_thread (thread_id));
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_add_range)
{
  gpointer address;
  gsize size;
  if (!_gum_v8_args_parse (args, "pZ", &address, &size))
    return;

  GumMemoryRange range;
  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  gum_cloak_add_range (&range);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_remove_range)
{
  gpointer address;
  gsize size;
  if (!_gum_v8_args_parse (args, "pZ", &address, &size))
    return;

  GumMemoryRange range;
  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  gum_cloak_remove_range (&range);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_has_range_containing)
{
  gpointer address;
  if (!_gum_v8_args_parse (args, "p", &address))
    return;

  info.GetReturnValue ().Set ((bool) gum_cloak_has_range_containing (
      GUM_ADDRESS (address)));
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_clip_range)
{
  gpointer address;
  gsize size;
  if (!_gum_v8_args_parse (args, "pZ", &address, &size))
    return;

  GumMemoryRange range;
  range.base_address = GUM_ADDRESS (address);
  range.size = size;

  auto context = isolate->GetCurrentContext ();

  GArray * visible = gum_cloak_clip_range (&range);
  if (visible == NULL)
  {
    info.GetReturnValue ().SetNull ();
    return;
  }

  auto result = Array::New (isolate, visible->len);
  for (guint i = 0; i != visible->len; i++)
  {
    auto r = &g_array_index (visible, GumMemoryRange, i);
    auto obj = Object::New (isolate);
    _gum_v8_object_set_pointer (obj, "base", r->base_address, core);
    _gum_v8_object_set_uint (obj, "size", r->size, core);
    result->Set (context, i, obj).Check ();
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_add_file_descriptor)
{
  gint fd;
  if (!_gum_v8_args_parse (args, "i", &fd))
    return;

  gum_cloak_add_file_descriptor (fd);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_remove_file_descriptor)
{
  gint fd;
  if (!_gum_v8_args_parse (args, "i", &fd))
    return;

  gum_cloak_remove_file_descriptor (fd);
}

GUMJS_DEFINE_FUNCTION (gumjs_cloak_has_file_descriptor)
{
  gint fd;
  if (!_gum_v8_args_parse (args, "i", &fd))
    return;

  info.GetReturnValue ().Set ((bool) gum_cloak_has_file_descriptor (fd));
}
