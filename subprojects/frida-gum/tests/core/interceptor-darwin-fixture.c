/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor.h"

#include "interceptor-callbacklistener.h"
#include "testutil.h"

#include <dlfcn.h>
#include <string.h>

#define TESTCASE(NAME) \
    void test_interceptor_ ## NAME ( \
        TestInterceptorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Interceptor/Darwin", \
        test_interceptor, NAME, TestInterceptorFixture)

typedef struct _TestInterceptorFixture TestInterceptorFixture;
typedef struct _DarwinListenerContext  DarwinListenerContext;

struct _DarwinListenerContext
{
  TestCallbackListener * listener;

  TestInterceptorFixture * fixture;
  gchar enter_char;
  gchar leave_char;
  GumThreadId last_thread_id;
  gsize last_seen_argument;
  gpointer last_return_value;
  GumCpuContext last_on_enter_cpu_context;
};

struct _TestInterceptorFixture
{
  GumInterceptor * interceptor;
  GString * result;
  DarwinListenerContext * listener_context[2];
};

static void darwin_listener_context_free (DarwinListenerContext * ctx);
static void darwin_listener_context_on_enter (DarwinListenerContext * self,
    GumInvocationContext * context);
static void darwin_listener_context_on_leave (DarwinListenerContext * self,
    GumInvocationContext * context);

static gpointer sqlite_module = NULL;

static void
test_interceptor_fixture_setup (TestInterceptorFixture * fixture,
                                gconstpointer data)
{
  fixture->interceptor = gum_interceptor_obtain ();
  fixture->result = g_string_sized_new (4096);
  memset (&fixture->listener_context, 0, sizeof (fixture->listener_context));

  if (sqlite_module == NULL)
  {
    sqlite_module = dlopen ("/usr/lib/libsqlite3.0.dylib",
        RTLD_LAZY | RTLD_GLOBAL);
    g_assert_nonnull (sqlite_module);
  }
}

static void
test_interceptor_fixture_teardown (TestInterceptorFixture * fixture,
                                   gconstpointer data)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (fixture->listener_context); i++)
  {
    DarwinListenerContext * ctx = fixture->listener_context[i];

    if (ctx != NULL)
    {
      gum_interceptor_detach (fixture->interceptor,
          GUM_INVOCATION_LISTENER (ctx->listener));
      darwin_listener_context_free (ctx);
    }
  }

  g_string_free (fixture->result, TRUE);
  g_object_unref (fixture->interceptor);
}

static GumAttachReturn
interceptor_fixture_try_attach (TestInterceptorFixture * h,
                                guint listener_index,
                                gpointer test_func,
                                gchar enter_char,
                                gchar leave_char)
{
  GumAttachReturn result;
  DarwinListenerContext * ctx;

  ctx = h->listener_context[listener_index];
  if (ctx != NULL)
  {
    darwin_listener_context_free (ctx);
    h->listener_context[listener_index] = NULL;
  }

  ctx = g_slice_new0 (DarwinListenerContext);

  ctx->listener = test_callback_listener_new ();
  ctx->listener->on_enter =
      (TestCallbackListenerFunc) darwin_listener_context_on_enter;
  ctx->listener->on_leave =
      (TestCallbackListenerFunc) darwin_listener_context_on_leave;
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
    darwin_listener_context_free (ctx);
  }

  return result;
}

static void
interceptor_fixture_attach (TestInterceptorFixture * h,
                            guint listener_index,
                            gpointer test_func,
                            gchar enter_char,
                            gchar leave_char)
{
  g_assert_cmpint (interceptor_fixture_try_attach (h, listener_index, test_func,
      enter_char, leave_char), ==, GUM_ATTACH_OK);
}

static void
interceptor_fixture_detach (TestInterceptorFixture * h,
                            guint listener_index)
{
  gum_interceptor_detach (h->interceptor,
      GUM_INVOCATION_LISTENER (h->listener_context[listener_index]->listener));
}

static void
darwin_listener_context_free (DarwinListenerContext * ctx)
{
  g_object_unref (ctx->listener);
  g_slice_free (DarwinListenerContext, ctx);
}

static void
darwin_listener_context_on_enter (DarwinListenerContext * self,
                                  GumInvocationContext * context)
{
  g_assert_cmpuint (gum_invocation_context_get_point_cut (context), ==,
      GUM_POINT_ENTER);

  g_string_append_c (self->fixture->result, self->enter_char);

  self->last_seen_argument = (gsize)
      gum_invocation_context_get_nth_argument (context, 0);
  self->last_on_enter_cpu_context = *context->cpu_context;

  self->last_thread_id = gum_invocation_context_get_thread_id (context);
}

static void
darwin_listener_context_on_leave (DarwinListenerContext * self,
                                  GumInvocationContext * context)
{
  g_assert_cmpuint (gum_invocation_context_get_point_cut (context), ==,
      GUM_POINT_LEAVE);

  g_string_append_c (self->fixture->result, self->leave_char);

  self->last_return_value = gum_invocation_context_get_return_value (context);
}
