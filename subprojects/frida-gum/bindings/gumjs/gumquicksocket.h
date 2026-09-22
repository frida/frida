/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_SOCKET_H__
#define __GUM_QUICK_SOCKET_H__

#include "gumquickstream.h"

G_BEGIN_DECLS

typedef struct _GumQuickSocket GumQuickSocket;

struct _GumQuickSocket
{
  GumQuickStream * stream;
  GumQuickCore * core;

  GumQuickObjectManager objects;

  JSClassID socket_listener_class;
  JSClassID socket_connection_class;
};

G_GNUC_INTERNAL void _gum_quick_socket_init (GumQuickSocket * self,
    JSValue ns, GumQuickStream * stream, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_socket_flush (GumQuickSocket * self);
G_GNUC_INTERNAL void _gum_quick_socket_dispose (GumQuickSocket * self);
G_GNUC_INTERNAL void _gum_quick_socket_finalize (GumQuickSocket * self);

G_END_DECLS

#endif
