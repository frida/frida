/*
 * Copyright (C) 2016-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_STREAM_H__
#define __GUM_V8_STREAM_H__

#include "gumv8object.h"

struct GumV8Stream
{
  GumV8Core * core;

  GumV8ObjectManager objects;

  v8::Global<v8::FunctionTemplate> * io_stream;
  v8::Global<v8::FunctionTemplate> * input_stream;
  v8::Global<v8::FunctionTemplate> * output_stream;
};

typedef GumV8Object<GIOStream, GumV8Stream> GumV8IOStream;
typedef GumV8Object<GInputStream, GumV8Stream> GumV8InputStream;
typedef GumV8Object<GOutputStream, GumV8Stream> GumV8OutputStream;

G_GNUC_INTERNAL void _gum_v8_stream_init (GumV8Stream * self, GumV8Core * core,
    v8::Local<v8::ObjectTemplate> scope);
G_GNUC_INTERNAL void _gum_v8_stream_realize (GumV8Stream * self);
G_GNUC_INTERNAL void _gum_v8_stream_flush (GumV8Stream * self);
G_GNUC_INTERNAL void _gum_v8_stream_dispose (GumV8Stream * self);
G_GNUC_INTERNAL void _gum_v8_stream_finalize (GumV8Stream * self);

#endif
