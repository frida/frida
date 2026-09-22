/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_MODULE_H__
#define __GUM_V8_MODULE_H__

#include "gumv8core.h"

struct GumV8Module
{
  GumV8Core * core;

  GHashTable * values;
  GHashTable * maps;

  v8::Global<v8::FunctionTemplate> * klass;

  v8::Global<v8::Object> * import_value;
  v8::Global<v8::Object> * export_value;

  v8::Global<v8::String> * type_key;
  v8::Global<v8::String> * name_key;
  v8::Global<v8::String> * module_key;
  v8::Global<v8::String> * address_key;
  v8::Global<v8::String> * slot_key;
  v8::Global<v8::String> * variable_value;
};

G_GNUC_INTERNAL void _gum_v8_module_init (GumV8Module * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_module_realize (GumV8Module * self);
G_GNUC_INTERNAL void _gum_v8_module_dispose (GumV8Module * self);
G_GNUC_INTERNAL void _gum_v8_module_finalize (GumV8Module * self);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_module_new_from_handle (
    GumModule * handle, GumV8Module * parent);
G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_module_new_take_handle (
    GumModule * handle, GumV8Module * parent);

#endif
