/*
 * Copyright (C) 2010-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __INTERCEPTOR_CALLBACKLISTENER_H__
#define __INTERCEPTOR_CALLBACKLISTENER_H__

#include <glib-object.h>
#include <gum/guminvocationlistener.h>

G_BEGIN_DECLS

#define TEST_TYPE_CALLBACK_LISTENER (test_callback_listener_get_type ())
G_DECLARE_FINAL_TYPE (TestCallbackListener, test_callback_listener, TEST,
    CALLBACK_LISTENER, GObject)

typedef void (* TestCallbackListenerFunc) (gpointer user_data,
    GumInvocationContext * context);

struct _TestCallbackListener
{
  GObject parent;

  TestCallbackListenerFunc on_enter;
  TestCallbackListenerFunc on_leave;
  gpointer user_data;
};

TestCallbackListener * test_callback_listener_new (void);

G_END_DECLS

#endif
