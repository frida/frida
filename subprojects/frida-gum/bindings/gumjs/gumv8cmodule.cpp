/*
 * Copyright (C) 2019-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8cmodule.h"

#include "gumcmodule.h"
#include "gumv8macros.h"

#define GUMJS_MODULE_NAME CModule

using namespace v8;

struct GumCModuleEntry
{
  Global<Object> * wrapper;
  Global<Object> * symbols;
  GumCModule * handle;
  GumV8CModule * module;
};

struct GumGetBuiltinsOperation
{
  Local<Object> container;
  GumV8Core * core;
};

struct GumAddCSymbolsOperation
{
  Local<Object> wrapper;
  GumV8Core * core;
};

GUMJS_DECLARE_GETTER (gumjs_cmodule_get_builtins)
static void gum_store_builtin_define (const GumCDefineDetails * details,
    GumGetBuiltinsOperation * op);
static void gum_store_builtin_header (const GumCHeaderDetails * details,
    GumGetBuiltinsOperation * op);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_cmodule_construct)
static gboolean gum_parse_cmodule_options (Local<Object> options_val,
    GumCModuleOptions * options, Local<Context> context, GumV8CModule * parent);
static gboolean gum_parse_cmodule_toolchain (Local<Value> val,
    GumCModuleToolchain * toolchain, Isolate * isolate);
static gboolean gum_add_csymbol (const GumCSymbolDetails * details,
    GumAddCSymbolsOperation * op);
GUMJS_DECLARE_FUNCTION (gumjs_cmodule_dispose)

static GumCModuleEntry * gum_cmodule_entry_new (Local<Object> wrapper,
    Local<Object> symbols, GumCModule * handle, GumV8CModule * module);
static void gum_cmodule_entry_free (GumCModuleEntry * self);
static void gum_cmodule_entry_on_weak_notify (
    const WeakCallbackInfo<GumCModuleEntry> & info);

static const GumV8Property gumjs_cmodule_module_values[] =
{
  { "builtins", gumjs_cmodule_get_builtins, NULL },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_cmodule_functions[] =
{
  { "dispose", gumjs_cmodule_dispose },

  { NULL, NULL }
};

void
_gum_v8_cmodule_init (GumV8CModule * self,
                      GumV8Core * core,
                      Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto cmodule = _gum_v8_create_class ("CModule", gumjs_cmodule_construct,
      scope, module, isolate);
  _gum_v8_class_add_static (cmodule, gumjs_cmodule_module_values, module,
      isolate);
  _gum_v8_class_add (cmodule, gumjs_cmodule_functions, module, isolate);
}

void
_gum_v8_cmodule_realize (GumV8CModule * self)
{
  auto isolate = self->core->isolate;

  self->cmodules = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_cmodule_entry_free);

  self->toolchain_key = new Global<String> (isolate,
      _gum_v8_string_new_ascii (isolate, "toolchain"));
}

void
_gum_v8_cmodule_dispose (GumV8CModule * self)
{
  g_hash_table_remove_all (self->cmodules);

  delete self->toolchain_key;
  self->toolchain_key = nullptr;
}

void
_gum_v8_cmodule_finalize (GumV8CModule * self)
{
  g_clear_pointer (&self->cmodules, g_hash_table_unref);
}

GUMJS_DEFINE_GETTER (gumjs_cmodule_get_builtins)
{
  auto result = Object::New (isolate);

  GumGetBuiltinsOperation op;
  op.core = core;

  op.container = Object::New (isolate);
  gum_cmodule_enumerate_builtin_defines (
      (GumFoundCDefineFunc) gum_store_builtin_define, &op);
  _gum_v8_object_set (result, "defines", op.container, core);

  op.container = Object::New (isolate);
  gum_cmodule_enumerate_builtin_headers (
      (GumFoundCHeaderFunc) gum_store_builtin_header, &op);
  _gum_v8_object_set (result, "headers", op.container, core);

  info.GetReturnValue ().Set (result);
}

static void
gum_store_builtin_define (const GumCDefineDetails * details,
                          GumGetBuiltinsOperation * op)
{
  auto core = op->core;

  if (details->value != NULL)
  {
    _gum_v8_object_set_utf8 (op->container, details->name, details->value,
        core);
  }
  else
  {
    _gum_v8_object_set (op->container, details->name, True (core->isolate),
        core);
  }
}

static void
gum_store_builtin_header (const GumCHeaderDetails * details,
                          GumGetBuiltinsOperation * op)
{
  if (details->kind != GUM_CHEADER_FRIDA)
    return;

  _gum_v8_object_set_utf8 (op->container, details->name, details->data,
      op->core);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_cmodule_construct)
{
  Local<Context> context = isolate->GetCurrentContext ();

  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new CModule()` to create a new instance");
    return;
  }

  if (info.Length () == 0)
  {
    _gum_v8_throw_ascii_literal (isolate, "missing argument");
    return;
  }

  gchar * source = NULL;
  GBytes * binary = NULL;
  Local<Object> symbols;
  Local<Object> options_val;
  if (!info[0]->IsObject ())
  {
    if (!_gum_v8_args_parse (args, "s|O?O?", &source, &symbols, &options_val))
      return;
  }
  else
  {
    if (!_gum_v8_args_parse (args, "B|O?O?", &binary, &symbols, &options_val))
      return;
  }

  GumCModuleOptions options;
  if (!gum_parse_cmodule_options (options_val, &options, context, module))
  {
    g_free (source);
    g_bytes_unref (binary);
    return;
  }

  GError * error = NULL;
  auto handle = gum_cmodule_new (source, binary, &options, &error);

  g_free (source);
  g_bytes_unref (binary);

  if (error == NULL && !symbols.IsEmpty ())
  {
    gboolean valid = TRUE;

    Local<Array> names;
    if (symbols->GetOwnPropertyNames (context).ToLocal (&names))
    {
      guint count = names->Length ();
      for (guint i = 0; i != count; i++)
      {
        Local<Value> name_val;
        if (!names->Get (context, i).ToLocal (&name_val))
        {
          valid = FALSE;
          break;
        }

        Local<String> name_str;
        if (!name_val->ToString (context).ToLocal (&name_str))
        {
          valid = FALSE;
          break;
        }

        String::Utf8Value name_utf8 (isolate, name_str);

        Local<Value> value_val;
        if (!symbols->Get (context, name_val).ToLocal (&value_val))
        {
          valid = FALSE;
          break;
        }

        gpointer value;
        if (!_gum_v8_native_pointer_get (value_val, &value, core))
        {
          valid = FALSE;
          break;
        }

        gum_cmodule_add_symbol (handle, *name_utf8, value);
      }
    }
    else
    {
      valid = FALSE;
    }

    if (!valid)
    {
      g_object_unref (handle);
      return;
    }
  }

  if (error == NULL)
    gum_cmodule_link (handle, &error);

  if (_gum_v8_maybe_throw (isolate, &error))
  {
    g_clear_object (&handle);
    return;
  }

  GumAddCSymbolsOperation op;
  op.wrapper = wrapper;
  op.core = core;

  gum_cmodule_enumerate_symbols (handle, (GumFoundCSymbolFunc) gum_add_csymbol,
      &op);

  gum_cmodule_drop_metadata (handle);

  auto entry = gum_cmodule_entry_new (wrapper, symbols, handle, module);
  wrapper->SetAlignedPointerInInternalField (0, entry);
}

static gboolean
gum_parse_cmodule_options (Local<Object> options_val,
                           GumCModuleOptions * options,
                           Local<Context> context,
                           GumV8CModule * parent)
{
  auto isolate = parent->core->isolate;
  Local<Value> v;

  options->toolchain = GUM_CMODULE_TOOLCHAIN_ANY;

  if (options_val.IsEmpty ())
    return TRUE;

  if (!options_val->Get (context, Local<String>::New (isolate,
      *parent->toolchain_key)).ToLocal (&v))
    return FALSE;
  if (!v->IsUndefined ())
  {
    if (!gum_parse_cmodule_toolchain (v, &options->toolchain, isolate))
      return FALSE;
  }

  return TRUE;
}

static gboolean
gum_parse_cmodule_toolchain (Local<Value> val,
                             GumCModuleToolchain * toolchain,
                             Isolate * isolate)
{
  if (val->IsString ())
  {
    String::Utf8Value str_val (isolate, val);
    auto str = *str_val;

    if (strcmp (str, "any") == 0)
    {
      *toolchain = GUM_CMODULE_TOOLCHAIN_ANY;
      return TRUE;
    }

    if (strcmp (str, "internal") == 0)
    {
      *toolchain = GUM_CMODULE_TOOLCHAIN_INTERNAL;
      return TRUE;
    }

    if (strcmp (str, "external") == 0)
    {
      *toolchain = GUM_CMODULE_TOOLCHAIN_EXTERNAL;
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid toolchain value");
  return FALSE;
}

static gboolean
gum_add_csymbol (const GumCSymbolDetails * details,
                 GumAddCSymbolsOperation * op)
{
  _gum_v8_object_set_pointer (op->wrapper, details->name,
      details->address, op->core);

  return TRUE;
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_cmodule_dispose, GumCModuleEntry)
{
  if (self != NULL)
  {
    wrapper->SetAlignedPointerInInternalField (0, NULL);

    g_hash_table_remove (module->cmodules, self);
  }
}

static GumCModuleEntry *
gum_cmodule_entry_new (Local<Object> wrapper,
                       Local<Object> symbols,
                       GumCModule * handle,
                       GumV8CModule * module)
{
  auto isolate = module->core->isolate;
  const GumMemoryRange * range;

  auto entry = g_slice_new (GumCModuleEntry);
  entry->wrapper = new Global<Object> (isolate, wrapper);
  entry->wrapper->SetWeak (entry, gum_cmodule_entry_on_weak_notify,
      WeakCallbackType::kParameter);
  entry->symbols = new Global<Object> (isolate, symbols);
  entry->handle = handle;
  entry->module = module;

  range = gum_cmodule_get_range (handle);
  module->core->isolate->AdjustAmountOfExternalAllocatedMemory (range->size);

  g_hash_table_add (module->cmodules, entry);

  return entry;
}

static void
gum_cmodule_entry_free (GumCModuleEntry * self)
{
  const GumMemoryRange * range;

  range = gum_cmodule_get_range (self->handle);
  self->module->core->isolate->AdjustAmountOfExternalAllocatedMemory (
      -((gssize) range->size));

  g_object_unref (self->handle);

  delete self->symbols;
  delete self->wrapper;

  g_slice_free (GumCModuleEntry, self);
}

static void
gum_cmodule_entry_on_weak_notify (
    const WeakCallbackInfo<GumCModuleEntry> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->cmodules, self);
}
