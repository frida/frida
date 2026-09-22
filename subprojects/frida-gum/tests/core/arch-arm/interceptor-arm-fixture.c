/*
 * Copyright (C) 2018-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor.h"

#include "interceptor-callbacklistener.h"
#include "testutil.h"

#include <string.h>

#define TESTCASE(NAME) \
    void interceptor_ ## NAME ( \
        InterceptorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Interceptor/Arm", \
        interceptor, NAME, InterceptorFixture)

typedef struct _InterceptorFixture InterceptorFixture;
typedef struct _ArmListenerContext ArmListenerContext;

struct _ArmListenerContext
{
  TestCallbackListener * listener;

  InterceptorFixture * fixture;
  gchar enter_char;
  gchar leave_char;
};

struct _InterceptorFixture
{
  GumInterceptor * interceptor;
  GString * result;
  ArmListenerContext * listener_context[2];
};

static void arm_listener_context_free (ArmListenerContext * ctx);
static void arm_listener_context_on_enter (ArmListenerContext * self,
    GumInvocationContext * context);
static void arm_listener_context_on_leave (ArmListenerContext * self,
    GumInvocationContext * context);

static void
interceptor_fixture_setup (InterceptorFixture * fixture,
                           gconstpointer data)
{
  fixture->interceptor = gum_interceptor_obtain ();
  fixture->result = g_string_sized_new (4096);
  memset (&fixture->listener_context, 0, sizeof (fixture->listener_context));
}

static void
interceptor_fixture_teardown (InterceptorFixture * fixture,
                              gconstpointer data)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (fixture->listener_context); i++)
  {
    ArmListenerContext * ctx = fixture->listener_context[i];

    if (ctx != NULL)
    {
      gum_interceptor_detach (fixture->interceptor,
          GUM_INVOCATION_LISTENER (ctx->listener));
      arm_listener_context_free (ctx);
    }
  }

  g_string_free (fixture->result, TRUE);
  g_object_unref (fixture->interceptor);
}

static GumAttachReturn
interceptor_fixture_try_attach (InterceptorFixture * h,
                                guint listener_index,
                                gpointer test_func,
                                gchar enter_char,
                                gchar leave_char)
{
  GumAttachReturn result;
  ArmListenerContext * ctx;

  ctx = h->listener_context[listener_index];
  if (ctx != NULL)
  {
    arm_listener_context_free (ctx);
    h->listener_context[listener_index] = NULL;
  }

  ctx = g_slice_new0 (ArmListenerContext);

  ctx->listener = test_callback_listener_new ();
  ctx->listener->on_enter =
      (TestCallbackListenerFunc) arm_listener_context_on_enter;
  ctx->listener->on_leave =
      (TestCallbackListenerFunc) arm_listener_context_on_leave;
  ctx->listener->user_data = ctx;

  ctx->fixture = h;
  ctx->enter_char = enter_char;
  ctx->leave_char = leave_char;

  result = gum_interceptor_attach (h->interceptor, test_func,
      GUM_INVOCATION_LISTENER (ctx->listener), NULL);
  if (result == GUM_ATTACH_OK)
  {
    h->listener_context[listener_index] = ctx;
  }
  else
  {
    arm_listener_context_free (ctx);
  }

  return result;
}

static void
interceptor_fixture_attach (InterceptorFixture * h,
                            guint listener_index,
                            gpointer test_func,
                            gchar enter_char,
                            gchar leave_char)
{
  g_assert_cmpint (interceptor_fixture_try_attach (h, listener_index, test_func,
      enter_char, leave_char), ==, GUM_ATTACH_OK);
}

static void
interceptor_fixture_detach (InterceptorFixture * h,
                            guint listener_index)
{
  gum_interceptor_detach (h->interceptor,
      GUM_INVOCATION_LISTENER (h->listener_context[listener_index]->listener));
}

static void
arm_listener_context_free (ArmListenerContext * ctx)
{
  g_object_unref (ctx->listener);
  g_slice_free (ArmListenerContext, ctx);
}

static void
arm_listener_context_on_enter (ArmListenerContext * self,
                               GumInvocationContext * context)
{
  g_string_append_c (self->fixture->result, self->enter_char);
}

static void
arm_listener_context_on_leave (ArmListenerContext * self,
                               GumInvocationContext * context)
{
  g_string_append_c (self->fixture->result, self->leave_char);
}
