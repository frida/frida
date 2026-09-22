/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Matt Oh <oh.jeongwook@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8symbol.h"

#include "gumv8macros.h"

#include <gum/gumsymbolutil.h>

#define GUMJS_MODULE_NAME Symbol

using namespace v8;

struct GumSymbol
{
  Global<Object> * wrapper;
  gboolean resolved;
  GumDebugSymbolDetails details;
  GumV8Symbol * module;
};

GUMJS_DECLARE_FUNCTION (gumjs_symbol_from_address)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_from_name)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_get_function_by_name)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_find_functions_named)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_find_functions_matching)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_load)

static Local<Object> gum_symbol_new (GumV8Symbol * module,
    GumSymbol ** symbol);
static void gum_symbol_free (GumSymbol * self);
GUMJS_DECLARE_GETTER (gumjs_symbol_get_address)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_name)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_module_name)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_file_name)
GUMJS_DECLARE_GETTER (gumjs_symbol_get_line_number)
GUMJS_DECLARE_FUNCTION (gumjs_symbol_to_string)
static void gum_symbol_on_weak_notify (
    const WeakCallbackInfo<GumSymbol> & info);

static const GumV8Function gumjs_symbol_module_functions[] =
{
  { "fromAddress", gumjs_symbol_from_address },
  { "fromName", gumjs_symbol_from_name },
  { "getFunctionByName", gumjs_symbol_get_function_by_name },
  { "findFunctionsNamed", gumjs_symbol_find_functions_named },
  { "findFunctionsMatching", gumjs_symbol_find_functions_matching },
  { "load", gumjs_symbol_load },

  { NULL, NULL }
};

static const GumV8Property gumjs_symbol_values[] =
{
  { "address", gumjs_symbol_get_address, NULL },
  { "name", gumjs_symbol_get_name, NULL },
  { "moduleName", gumjs_symbol_get_module_name, NULL },
  { "fileName", gumjs_symbol_get_file_name, NULL },
  { "lineNumber", gumjs_symbol_get_line_number, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_symbol_functions[] =
{
  { "toString", gumjs_symbol_to_string },

  { NULL, NULL }
};

void
_gum_v8_symbol_init (GumV8Symbol * self,
                     GumV8Core * core,
                     Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto klass = _gum_v8_create_class ("DebugSymbol", nullptr, scope, module,
      isolate);
  _gum_v8_class_add_static (klass, gumjs_symbol_module_functions, module,
      isolate);
  _gum_v8_class_add (klass, gumjs_symbol_values, module, isolate);
  _gum_v8_class_add (klass, gumjs_symbol_functions, module, isolate);
  self->klass = new Global<FunctionTemplate> (isolate, klass);
}

void
_gum_v8_symbol_realize (GumV8Symbol * self)
{
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  self->symbols = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_symbol_free);

  auto klass = Local<FunctionTemplate>::New (isolate, *self->klass);
  auto object = klass->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 0, nullptr).ToLocalChecked ();
  self->template_object = new Global<Object> (isolate, object);
}

void
_gum_v8_symbol_dispose (GumV8Symbol * self)
{
  g_hash_table_unref (self->symbols);
  self->symbols = NULL;

  delete self->template_object;
  self->template_object = nullptr;

  delete self->klass;
  self->klass = nullptr;
}

void
_gum_v8_symbol_finalize (GumV8Symbol * self)
{
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_from_address)
{
  gpointer address;
  if (!_gum_v8_args_parse (args, "p", &address))
    return;

  GumSymbol * symbol;
  auto object = gum_symbol_new (module, &symbol);

  symbol->details.address = GPOINTER_TO_SIZE (address);

  {
    ScriptUnlocker unlocker (core);

    symbol->resolved =
        gum_symbol_details_from_address (address, &symbol->details);
  }

  info.GetReturnValue ().Set (object);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_from_name)
{
  gchar * name;
  if (!_gum_v8_args_parse (args, "s", &name))
    return;

  GumSymbol * symbol;
  auto object = gum_symbol_new (module, &symbol);

  {
    ScriptUnlocker unlocker (core);

    auto address = gum_find_function (name);
    if (address != NULL)
    {
      symbol->resolved =
          gum_symbol_details_from_address (address, &symbol->details);
    }
    else
    {
      symbol->resolved = FALSE;
      symbol->details.address = 0;
    }
  }

  g_free (name);

  info.GetReturnValue ().Set (object);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_get_function_by_name)
{
  gchar * name;
  if (!_gum_v8_args_parse (args, "s", &name))
    return;

  gpointer address;

  {
    ScriptUnlocker unlocker (core);

    address = gum_find_function (name);
  }

  if (address != NULL)
  {
    info.GetReturnValue ().Set (_gum_v8_native_pointer_new (address, core));
  }
  else
  {
    _gum_v8_throw (isolate, "unable to find function with name '%s'", name);
  }

  g_free (name);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_find_functions_named)
{
  auto context = isolate->GetCurrentContext ();

  gchar * name;
  if (!_gum_v8_args_parse (args, "s", &name))
    return;

  GArray * functions;

  {
    ScriptUnlocker unlocker (core);

    functions = gum_find_functions_named (name);
  }

  auto result = Array::New (isolate, functions->len);
  for (guint i = 0; i != functions->len; i++)
  {
    auto address = g_array_index (functions, gpointer, i);
    result->Set (context, i, _gum_v8_native_pointer_new (address, core))
        .Check ();
  }

  info.GetReturnValue ().Set (result);

  g_array_free (functions, TRUE);

  g_free (name);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_find_functions_matching)
{
  auto context = isolate->GetCurrentContext ();

  gchar * str;
  if (!_gum_v8_args_parse (args, "s", &str))
    return;

  GArray * functions;

  {
    ScriptUnlocker unlocker (core);

    functions = gum_find_functions_matching (str);
  }

  auto result = Array::New (isolate, functions->len);
  for (guint i = 0; i != functions->len; i++)
  {
    auto address = g_array_index (functions, gpointer, i);
    result->Set (context, i, _gum_v8_native_pointer_new (address, core))
        .Check ();
  }

  info.GetReturnValue ().Set (result);

  g_array_free (functions, TRUE);

  g_free (str);
}

GUMJS_DEFINE_FUNCTION (gumjs_symbol_load)
{
  gchar * path;
  if (!_gum_v8_args_parse (args, "s", &path))
    return;

  gboolean success;
  {
    ScriptUnlocker unlocker (core);

    success = gum_load_symbols (path);
  }

  if (!success)
    _gum_v8_throw (isolate, "unable to load symbols");

  g_free (path);
}

static Local<Object>
gum_symbol_new (GumV8Symbol * module,
                GumSymbol ** symbol)
{
  auto isolate = module->core->isolate;

  auto template_object = Local<Object>::New (isolate, *module->template_object);
  auto object = template_object->Clone ();

  auto s = g_slice_new (GumSymbol);
  s->wrapper = new Global<Object> (isolate, object);
  s->wrapper->SetWeak (s, gum_symbol_on_weak_notify,
      WeakCallbackType::kParameter);
  s->module = module;

  object->SetAlignedPointerInInternalField (0, s);

  isolate->AdjustAmountOfExternalAllocatedMemory (sizeof (GumSymbol));

  g_hash_table_add (module->symbols, s);

  *symbol = s;

  return object;
}

static void
gum_symbol_free (GumSymbol * self)
{
  self->module->core->isolate->AdjustAmountOfExternalAllocatedMemory (
      -((gssize) sizeof (GumSymbol)));

  delete self->wrapper;

  g_slice_free (GumSymbol, self);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_symbol_get_address, GumSymbol)
{
  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (self->details.address),
          core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_symbol_get_name, GumSymbol)
{
  if (self->resolved)
  {
    info.GetReturnValue ().Set (
        String::NewFromUtf8 (isolate, self->details.symbol_name)
        .ToLocalChecked ());
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_symbol_get_module_name, GumSymbol)
{
  if (self->resolved)
  {
    info.GetReturnValue ().Set (
        String::NewFromUtf8 (isolate, self->details.module_name)
        .ToLocalChecked ());
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_symbol_get_file_name, GumSymbol)
{
  if (self->resolved)
  {
    info.GetReturnValue ().Set (
        String::NewFromUtf8 (isolate, self->details.file_name)
        .ToLocalChecked ());
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_symbol_get_line_number, GumSymbol)
{
  if (self->resolved)
  {
    info.GetReturnValue ().Set ((uint32_t) self->details.line_number);
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_symbol_to_string, GumSymbol)
{
  auto * d = &self->details;

  auto s = g_string_new ("0");

  if (self->resolved)
  {
    g_string_append_printf (s, "x%" G_GINT64_MODIFIER "x %s!%s", d->address,
        d->module_name, d->symbol_name);
    if (d->file_name[0] != '\0')
    {
      g_string_append_printf (s, " %s:%u", d->file_name, d->line_number);
    }
  }
  else if (d->address != 0)
  {
    g_string_append_printf (s, "x%" G_GINT64_MODIFIER "x", d->address);
  }

  info.GetReturnValue ().Set (String::NewFromUtf8 (isolate, s->str)
      .ToLocalChecked ());

  g_string_free (s, TRUE);
}

static void
gum_symbol_on_weak_notify (const WeakCallbackInfo<GumSymbol> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->symbols, self);
}
