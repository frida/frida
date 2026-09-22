/*
 * Copyright (C) 2010-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "interceptor-callbacklistener.h"

static void test_callback_listener_iface_init (gpointer g_iface,
    gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (TestCallbackListener,
                        test_callback_listener,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            test_callback_listener_iface_init))

static void
test_callback_listener_on_enter (GumInvocationListener * listener,
                                 GumInvocationContext * context)
{
  TestCallbackListener * self = TEST_CALLBACK_LISTENER (listener);

  if (self->on_enter != NULL)
    self->on_enter (self->user_data, context);
}

static void
test_callback_listener_on_leave (GumInvocationListener * listener,
                                 GumInvocationContext * context)
{
  TestCallbackListener * self = TEST_CALLBACK_LISTENER (listener);

  if (self->on_leave != NULL)
    self->on_leave (self->user_data, context);
}

static void
test_callback_listener_iface_init (gpointer g_iface,
                                   gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = test_callback_listener_on_enter;
  iface->on_leave = test_callback_listener_on_leave;
}

static void
test_callback_listener_class_init (TestCallbackListenerClass * klass)
{
}

static void
test_callback_listener_init (TestCallbackListener * self)
{
}

TestCallbackListener *
test_callback_listener_new (void)
{
  return g_object_new (TEST_TYPE_CALLBACK_LISTENER, NULL);
}
