/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_ENUMERATE_CONTEXT_H__
#define __GUM_V8_ENUMERATE_CONTEXT_H__

#include "gumv8script.h"

#include <v8.h>

template <typename T>
class GumV8EnumerateContext
{
public:
  GumV8EnumerateContext (v8::Isolate * isolate, T * parent)
    : isolate (isolate),
      context (isolate->GetCurrentContext ()),
      parent (parent),
      elements (v8::Array::New (isolate)),
      n (0)
  {
  }

  v8::Local<v8::Array>
  End ()
  {
    return elements;
  }

  gboolean
  Collect (v8::Local<v8::Value> element)
  {
    elements->Set (context, n++, element).ToChecked ();
    return TRUE;
  }

  v8::Isolate * isolate;
  v8::Local<v8::Context> context;
  T * parent;

private:
  v8::Local<v8::Array> elements;
  guint n;
};

#endif
