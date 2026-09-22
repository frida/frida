/*
 * Copyright (C) 2013-2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_FILE_H__
#define __GUM_V8_FILE_H__

#include "gumv8core.h"

struct GumV8File
{
  GumV8Core * core;

  GHashTable * files;
};

G_GNUC_INTERNAL void _gum_v8_file_init (GumV8File * self,
    GumV8Core * core, v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_file_realize (GumV8File * self);
G_GNUC_INTERNAL void _gum_v8_file_dispose (GumV8File * self);
G_GNUC_INTERNAL void _gum_v8_file_finalize (GumV8File * self);

#endif
