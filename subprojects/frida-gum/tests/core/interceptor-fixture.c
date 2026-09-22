/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor.h"

#include "interceptor-callbacklistener.h"
#include "interceptor-functiondatalistener.h"
#include "lowlevelhelpers.h"
#include "testutil.h"
#include "valgrind.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_WINDOWS
# include "targetfunctions/targetfunctions.c"
#else
# include <dlfcn.h>
# include <unistd.h>
#endif

#define TESTCASE(NAME) \
    void test_interceptor_ ## NAME ( \
        TestInterceptorFixture * fixture, gconstpointer data)
#define TESTENTRY(NAME) \
    TESTENTRY_WITH_FIXTURE ("Core/Interceptor", \
        test_interceptor, NAME, TestInterceptorFixture)

/* TODO: fix this in GLib */
#ifdef HAVE_DARWIN
# undef G_MODULE_SUFFIX
# define G_MODULE_SUFFIX "dylib"
#endif

#if defined (HAVE_WINDOWS)
# define GUM_TEST_SHLIB_OS "windows"
#elif defined (HAVE_MACOS)
# define GUM_TEST_SHLIB_OS "macos"
#elif defined (HAVE_LINUX) && !defined (HAVE_ANDROID)
# define GUM_TEST_SHLIB_OS "linux"
#elif defined (HAVE_IOS)
# define GUM_TEST_SHLIB_OS "ios"
#elif defined (HAVE_WATCHOS)
# define GUM_TEST_SHLIB_OS "watchos"
#elif defined (HAVE_TVOS)
# define GUM_TEST_SHLIB_OS "tvos"
#elif defined (HAVE_ANDROID)
# define GUM_TEST_SHLIB_OS "android"
#elif defined (HAVE_FREEBSD)
# define GUM_TEST_SHLIB_OS "freebsd"
#elif defined (HAVE_QNX)
# define GUM_TEST_SHLIB_OS "qnx"
#else
# error Unknown OS
#endif

#if defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 4
#  define GUM_TEST_SHLIB_ARCH "x86"
# else
#  define GUM_TEST_SHLIB_ARCH "x86_64"
# endif
#elif defined (HAVE_ARM)
# if G_BYTE_ORDER == G_BIG_ENDIAN
#  define GUM_TEST_SHLIB_ARCH "armbe8"
# elif defined (__ARM_PCS_VFP)
#  define GUM_TEST_SHLIB_ARCH "armhf"
# else
#  define GUM_TEST_SHLIB_ARCH "arm"
# endif
#elif defined (HAVE_ARM64)
# if G_BYTE_ORDER == G_BIG_ENDIAN
#  define GUM_TEST_SHLIB_ARCH "arm64be"
# elif defined (HAVE_PTRAUTH)
#  define GUM_TEST_SHLIB_ARCH "arm64e"
# else
#  define GUM_TEST_SHLIB_ARCH "arm64"
# endif
#elif defined (HAVE_MIPS)
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  if GLIB_SIZEOF_VOID_P == 8
#    define GUM_TEST_SHLIB_ARCH "mips64el"
#  else
#    define GUM_TEST_SHLIB_ARCH "mipsel"
#  endif
# else
#  if GLIB_SIZEOF_VOID_P == 8
#    define GUM_TEST_SHLIB_ARCH "mips64"
#  else
#    define GUM_TEST_SHLIB_ARCH "mips"
#  endif
# endif
#else
# error Unknown CPU
#endif

typedef struct _TestInterceptorFixture TestInterceptorFixture;
typedef struct _ListenerContext        ListenerContext;

struct _ListenerContext
{
  TestCallbackListener * listener;

  TestInterceptorFixture * fixture;
  gchar enter_char;
  gchar leave_char;
  GumThreadId last_thread_id;
  gsize last_seen_argument;
  gpointer last_return_value;
  GumCpuContext last_on_enter_cpu_context;
#ifdef GUM_CPU_CONTEXT_HAS_OUT_OF_LINE_VECTORS
  GumX86VectorReg last_on_enter_cpu_context_vectors[GUM_X86_XMM_REG_COUNT];
#endif
};

struct _TestInterceptorFixture
{
  GumInterceptor * interceptor;
  GString * result;
  ListenerContext * listener_context[2];
};

static GumAttachReturn interceptor_fixture_try_attach_with_options (
    TestInterceptorFixture * h, guint listener_index, gpointer test_func,
    gchar enter_char, gchar leave_char, const GumAttachOptions * options);
static void listener_context_free (ListenerContext * ctx);
static void listener_context_on_enter (ListenerContext * self,
    GumInvocationContext * context);
static void listener_context_on_leave (ListenerContext * self,
    GumInvocationContext * context);

gpointer (* target_function) (GString * str) = NULL;
gpointer (* target_nop_function_a) (gpointer data);
gpointer (* target_nop_function_b) (gpointer data);
gpointer (* target_nop_function_c) (gpointer data);

gpointer (* special_function) (GString * str) = NULL;

static void
test_interceptor_fixture_setup (TestInterceptorFixture * fixture,
                                gconstpointer data)
{
  fixture->interceptor = gum_interceptor_obtain ();
  fixture->result = g_string_sized_new (4096);

  memset (&fixture->listener_context, 0, sizeof (fixture->listener_context));

  if (target_function == NULL)
  {
#ifdef HAVE_WINDOWS
    target_function = gum_test_target_function;
    special_function = gum_test_target_function;
    target_nop_function_a = gum_test_target_nop_function_a;
    target_nop_function_b = gum_test_target_nop_function_b;
    target_nop_function_c = gum_test_target_nop_function_c;
#else
    gchar * testdir, * filename;
    void * lib;

    testdir = test_util_get_data_dir ();

    filename = g_build_filename (testdir,
        "targetfunctions-" GUM_TEST_SHLIB_OS "-" GUM_TEST_SHLIB_ARCH
        "." G_MODULE_SUFFIX, NULL);
    lib = dlopen (filename, RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL)
      g_print ("failed to open '%s'\n", filename);
    g_assert_nonnull (lib);
    g_free (filename);

    target_function = dlsym (lib, "gum_test_target_function");
    g_assert_nonnull (target_function);

    target_nop_function_a = dlsym (lib, "gum_test_target_nop_function_a");
    g_assert_nonnull (target_nop_function_a);

    target_nop_function_b = dlsym (lib, "gum_test_target_nop_function_b");
    g_assert_nonnull (target_nop_function_b);

    target_nop_function_c = dlsym (lib, "gum_test_target_nop_function_c");
    g_assert_nonnull (target_nop_function_c);

    filename = g_build_filename (testdir,
        "specialfunctions-" GUM_TEST_SHLIB_OS "-" GUM_TEST_SHLIB_ARCH
        "." G_MODULE_SUFFIX, NULL);
    lib = dlopen (filename, RTLD_LAZY | RTLD_GLOBAL);
    g_assert_nonnull (lib);
    g_free (filename);

    special_function = dlsym (lib, "gum_test_special_function");
    g_assert_nonnull (special_function);

    g_free (testdir);
#endif
  }
}

static void
test_interceptor_fixture_teardown (TestInterceptorFixture * fixture,
                                   gconstpointer data)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (fixture->listener_context); i++)
  {
    ListenerContext * ctx = fixture->listener_context[i];

    if (ctx != NULL)
    {
      gum_interceptor_detach (fixture->interceptor,
          GUM_INVOCATION_LISTENER (ctx->listener));
      listener_context_free (ctx);
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
  return interceptor_fixture_try_attach_with_options (h, listener_index,
      test_func, enter_char, leave_char, NULL);
}

static GumAttachReturn
interceptor_fixture_try_attach_with_options (TestInterceptorFixture * h,
                                             guint listener_index,
                                             gpointer test_func,
                                             gchar enter_char,
                                             gchar leave_char,
                                             const GumAttachOptions * options)
{
  GumAttachReturn result;
  ListenerContext * ctx;

  ctx = h->listener_context[listener_index];
  if (ctx != NULL)
  {
    listener_context_free (ctx);
    h->listener_context[listener_index] = NULL;
  }

  ctx = g_slice_new0 (ListenerContext);

  ctx->listener = test_callback_listener_new ();
  ctx->listener->on_enter =
      (TestCallbackListenerFunc) listener_context_on_enter;
  ctx->listener->on_leave =
      (TestCallbackListenerFunc) listener_context_on_leave;
  ctx->listener->user_data = ctx;

  ctx->fixture = h;
  ctx->enter_char = enter_char;
  ctx->leave_char = leave_char;

  result = gum_interceptor_attach (h->interceptor, test_func,
      GUM_INVOCATION_LISTENER (ctx->listener), options);
  if (result == GUM_ATTACH_OK)
  {
    h->listener_context[listener_index] = ctx;
  }
  else
  {
    listener_context_free (ctx);
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

static gpointer
interceptor_fixture_get_libc_malloc (void)
{
  return gum_heap_api_list_get_nth (test_util_heap_apis (), 0)->malloc;
}

static gpointer
interceptor_fixture_get_libc_free (void)
{
  return gum_heap_api_list_get_nth (test_util_heap_apis (), 0)->free;
}

static void
listener_context_free (ListenerContext * ctx)
{
  g_object_unref (ctx->listener);
  g_slice_free (ListenerContext, ctx);
}

static void
listener_context_on_enter (ListenerContext * self,
                           GumInvocationContext * context)
{
  g_assert_cmpuint (gum_invocation_context_get_point_cut (context), ==,
      GUM_POINT_ENTER);

  g_string_append_c (self->fixture->result, self->enter_char);

  self->last_seen_argument = (gsize)
      gum_invocation_context_get_nth_argument (context, 0);
  self->last_on_enter_cpu_context = *context->cpu_context;
#ifdef GUM_CPU_CONTEXT_HAS_OUT_OF_LINE_VECTORS
  memcpy (self->last_on_enter_cpu_context_vectors, context->cpu_context->xmm,
      sizeof (self->last_on_enter_cpu_context_vectors));
  self->last_on_enter_cpu_context.xmm =
      self->last_on_enter_cpu_context_vectors;
#endif

  self->last_thread_id = gum_invocation_context_get_thread_id (context);
}

static void
listener_context_on_leave (ListenerContext * self,
                           GumInvocationContext * context)
{
  g_assert_cmpuint (gum_invocation_context_get_point_cut (context), ==,
      GUM_POINT_LEAVE);

  g_string_append_c (self->fixture->result, self->leave_char);

  self->last_return_value = gum_invocation_context_get_return_value (context);
}
