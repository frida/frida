/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_STREAM_H__
#define __GUM_QUICK_STREAM_H__

#include "gumquickobject.h"

G_BEGIN_DECLS

typedef struct _GumQuickStream GumQuickStream;

struct _GumQuickStream
{
  GumQuickCore * core;

  GumQuickObjectManager objects;

  JSClassID io_stream_class;
  JSValue io_stream_proto;
  JSClassID input_stream_class;
  JSClassID output_stream_class;
  JSClassID native_input_stream_class;
  JSClassID native_output_stream_class;
};

G_GNUC_INTERNAL void _gum_quick_stream_init (GumQuickStream * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_stream_flush (GumQuickStream * self);
G_GNUC_INTERNAL void _gum_quick_stream_dispose (GumQuickStream * self);
G_GNUC_INTERNAL void _gum_quick_stream_finalize (GumQuickStream * self);

G_END_DECLS

#endif
