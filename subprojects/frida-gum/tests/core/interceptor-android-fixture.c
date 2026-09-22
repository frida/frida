/*
 * Copyright (C) 2017-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor.h"

#include "interceptor-callbacklistener.h"
#include "testutil.h"

#include <dlfcn.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/system_properties.h>

#define TESTCASE(NAME) \
    void test_interceptor_ ## NAME ( \
        TestInterceptorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Interceptor/Android", \
        test_interceptor, NAME, TestInterceptorFixture)

typedef struct _TestInterceptorFixture TestInterceptorFixture;
typedef struct _AndroidListenerContext AndroidListenerContext;

struct _AndroidListenerContext
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
  AndroidListenerContext * listener_context[2];
};

static void interceptor_fixture_detach (TestInterceptorFixture * h,
    guint listener_index);

static void android_listener_context_free (AndroidListenerContext * ctx);
static void android_listener_context_on_enter (AndroidListenerContext * self,
    GumInvocationContext * context);
static void android_listener_context_on_leave (AndroidListenerContext * self,
    GumInvocationContext * context);

static void init_java_vm (JavaVM ** vm, JNIEnv ** env);
static guint get_system_api_level (void);

static JavaVM * java_vm = NULL;
static JNIEnv * java_env = NULL;

static void
test_interceptor_fixture_setup (TestInterceptorFixture * fixture,
                                gconstpointer data)
{
  fixture->interceptor = gum_interceptor_obtain ();
  fixture->result = g_string_sized_new (4096);
  memset (&fixture->listener_context, 0, sizeof (fixture->listener_context));

  if (java_vm == NULL)
  {
    init_java_vm (&java_vm, &java_env);
  }

  (void) interceptor_fixture_detach;
}

static void
test_interceptor_fixture_teardown (TestInterceptorFixture * fixture,
                                   gconstpointer data)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (fixture->listener_context); i++)
  {
    AndroidListenerContext * ctx = fixture->listener_context[i];

    if (ctx != NULL)
    {
      gum_interceptor_detach (fixture->interceptor,
          GUM_INVOCATION_LISTENER (ctx->listener));
      android_listener_context_free (ctx);
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
  AndroidListenerContext * ctx;

  ctx = h->listener_context[listener_index];
  if (ctx != NULL)
  {
    android_listener_context_free (ctx);
    h->listener_context[listener_index] = NULL;
  }

  ctx = g_slice_new0 (AndroidListenerContext);

  ctx->listener = test_callback_listener_new ();
  ctx->listener->on_enter =
      (TestCallbackListenerFunc) android_listener_context_on_enter;
  ctx->listener->on_leave =
      (TestCallbackListenerFunc) android_listener_context_on_leave;
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
    android_listener_context_free (ctx);
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
android_listener_context_free (AndroidListenerContext * ctx)
{
  g_object_unref (ctx->listener);
  g_slice_free (AndroidListenerContext, ctx);
}

static void
android_listener_context_on_enter (AndroidListenerContext * self,
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
android_listener_context_on_leave (AndroidListenerContext * self,
                                   GumInvocationContext * context)
{
  g_assert_cmpuint (gum_invocation_context_get_point_cut (context), ==,
      GUM_POINT_LEAVE);

  g_string_append_c (self->fixture->result, self->leave_char);

  self->last_return_value = gum_invocation_context_get_return_value (context);
}

static void
init_java_vm (JavaVM ** vm,
              JNIEnv ** env)
{
  void * vm_module, * runtime_module;
  jint (* create_java_vm) (JavaVM ** vm, JNIEnv ** env, void * vm_args);
  JavaVMOption options[4];
  JavaVMInitArgs args;
  jint (* register_natives) (JNIEnv * env);
  jint (* register_natives_legacy) (JNIEnv * env, jclass clazz);
  jint result;

  vm_module = dlopen ((get_system_api_level () >= 21)
      ? "libart.so"
      : "libdvm.so",
      RTLD_LAZY | RTLD_GLOBAL);
  g_assert_nonnull (vm_module);

  runtime_module = dlopen ("libandroid_runtime.so", RTLD_LAZY | RTLD_GLOBAL);
  g_assert_nonnull (runtime_module);

  create_java_vm = dlsym (vm_module, "JNI_CreateJavaVM");
  g_assert_nonnull (create_java_vm);

  options[0].optionString = "-verbose:jni";
  options[1].optionString = "-verbose:gc";
  options[2].optionString = "-Xcheck:jni";
  options[3].optionString = "-Xdebug";

  args.version = JNI_VERSION_1_6;
  args.nOptions = G_N_ELEMENTS (options);
  args.options = options;
  args.ignoreUnrecognized = JNI_TRUE;

  result = create_java_vm (vm, env, &args);
  g_assert_cmpint (result, ==, JNI_OK);

  register_natives = dlsym (runtime_module, "registerFrameworkNatives");
  if (register_natives != NULL)
  {
    result = register_natives (*env);
    g_assert_cmpint (result, ==, JNI_OK);
  }
  else
  {
    register_natives_legacy = dlsym (runtime_module,
        "Java_com_android_internal_util_WithFramework_registerNatives");
    g_assert_nonnull (register_natives_legacy);

    result = register_natives_legacy (*env, NULL);
    g_assert_cmpint (result, ==, JNI_OK);
  }
}

static guint
get_system_api_level (void)
{
  gchar sdk_version[PROP_VALUE_MAX];

  __system_property_get ("ro.build.version.sdk", sdk_version);

  return atoi (sdk_version);
}
