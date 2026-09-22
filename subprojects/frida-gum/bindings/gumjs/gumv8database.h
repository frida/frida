/*
 * Copyright (C) 2017-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_DATABASE_H__
#define __GUM_V8_DATABASE_H__

#include "gumv8core.h"
#include "gummemoryvfs.h"

struct GumV8Database
{
  GumV8Core * core;

  GHashTable * databases;
  GHashTable * statements;
  v8::Global<v8::FunctionTemplate> * database;
  v8::Global<v8::FunctionTemplate> * statement;

  GumMemoryVfs * memory_vfs;
};

G_GNUC_INTERNAL void _gum_v8_database_init (GumV8Database * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_database_realize (GumV8Database * self);
G_GNUC_INTERNAL void _gum_v8_database_dispose (GumV8Database * self);
G_GNUC_INTERNAL void _gum_v8_database_finalize (GumV8Database * self);

#endif
