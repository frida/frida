/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#define DEBUG_HEAP_LEAKS 0

#include "testutil.h"

#include "lowlevelhelpers.h"
#ifdef HAVE_GUMJS
# include "gumscriptbackend.h"
#endif
#include "valgrind.h"

#include <capstone.h>
#include <glib.h>
#ifdef HAVE_GUMJS
# include <gio/gio.h>
#endif
#ifdef HAVE_GIOOPENSSL
# include <gioopenssl.h>
#endif
#include <gum/gum.h>
#include <string.h>

#ifdef HAVE_WINDOWS
# include <windows.h>
# include <conio.h>
# include <crtdbg.h>
# include <stdio.h>
#endif

#if defined (HAVE_DARWIN) || defined (HAVE_QNX)
# include <dlfcn.h>
#endif

#ifdef HAVE_IOS
# include <unistd.h>
#endif

static guint get_number_of_tests_in_suite (GTestSuite * suite);

gint
main (gint argc, gchar * argv[])
{
#if defined (HAVE_FRIDA_GLIB) && !DEBUG_HEAP_LEAKS && !defined (HAVE_ASAN)
  GMemVTable mem_vtable = {
    gum_malloc,
    gum_realloc,
    gum_memalign,
    gum_free,
    gum_calloc,
    gum_malloc,
    gum_realloc
  };
#endif
  gint result;
  GTimer * timer;
  guint num_tests;
  gdouble t;

#if defined (HAVE_WINDOWS) && DEBUG_HEAP_LEAKS
  {
    int tmp_flag;

    /*_CrtSetBreakAlloc (1337);*/

    _CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);

    tmp_flag = _CrtSetDbgFlag (_CRTDBG_REPORT_FLAG);

    tmp_flag |= _CRTDBG_ALLOC_MEM_DF;
    tmp_flag |= _CRTDBG_LEAK_CHECK_DF;
    tmp_flag &= ~_CRTDBG_CHECK_CRT_DF;

    _CrtSetDbgFlag (tmp_flag);
  }
#endif

#ifdef HAVE_WINDOWS
  {
    WORD version_requested = MAKEWORD (2, 2);
    WSADATA wsa_data;
    int err;

    err = WSAStartup (version_requested, &wsa_data);
    g_assert_cmpint (err, ==, 0);
  }
#endif

#ifdef HAVE_DARWIN
  /* Simulate an application where Foundation is available */
  dlopen ("/System/Library/Frameworks/Foundation.framework/Foundation",
      RTLD_LAZY | RTLD_GLOBAL);
#endif

  gum_internal_heap_ref ();
#if !DEBUG_HEAP_LEAKS && !defined (HAVE_ASAN)
  if (RUNNING_ON_VALGRIND)
  {
    g_setenv ("G_SLICE", "always-malloc", TRUE);
  }
  else
  {
#ifdef HAVE_FRIDA_GLIB
    g_mem_set_vtable (&mem_vtable);
#endif
  }
#else
  g_setenv ("G_SLICE", "always-malloc", TRUE);
#endif
  g_setenv ("G_DEBUG", "fatal-warnings:fatal-criticals", TRUE);
#ifdef HAVE_FRIDA_GLIB
  glib_init ();
# ifdef HAVE_GUMJS
  gio_init ();
# endif
#endif
#ifdef HAVE_GIOOPENSSL
  g_io_module_openssl_register ();
#endif
  g_test_init (&argc, &argv, NULL);
  gum_init ();

  _test_util_init ();
  lowlevel_helpers_init ();

#ifdef HAVE_ASAN
  {
    const gchar * asan_options;

    asan_options = g_getenv ("ASAN_OPTIONS");
    if (asan_options == NULL || strstr (asan_options, "handle_segv=0") == NULL)
    {
      g_printerr (
          "\n"
          "You must disable AddressSanitizer's segv-handling. For example:\n"
          "\n"
          "$ export ASAN_OPTIONS=handle_segv=0\n"
          "\n"
          "This is required for testing Gum's exception-handling.\n"
          "\n");
      exit (1);
    }
  }
#endif

#ifdef HAVE_IOS
  if (g_file_test ("/usr/lib/libjailbreak.dylib", G_FILE_TEST_EXISTS))
  {
    void * module;
    void (* entitle_now) (pid_t pid);

    module = dlopen ("/usr/lib/libjailbreak.dylib", RTLD_LAZY | RTLD_GLOBAL);
    g_assert_nonnull (module);

    entitle_now = dlsym (module, "jb_oneshot_entitle_now");
    g_assert_nonnull (entitle_now);

    entitle_now (getpid ());

    dlclose (module);
  }
#endif

#ifdef HAVE_QNX
  dlopen (SYSTEM_MODULE_NAME, RTLD_LAZY | RTLD_GLOBAL);
#endif

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4210)
#endif

  /* Core */
  TESTLIST_REGISTER (testutil);
  TESTLIST_REGISTER (tls);
  TESTLIST_REGISTER (cfg);
  TESTLIST_REGISTER (cloak);
  TESTLIST_REGISTER (memory);
  TESTLIST_REGISTER (process);
  TESTLIST_REGISTER (thread_registry);
  TESTLIST_REGISTER (module_registry);
#if !defined (HAVE_QNX) && !(defined (HAVE_ANDROID) && defined (HAVE_ARM64))
  TESTLIST_REGISTER (symbolutil);
#endif
  TESTLIST_REGISTER (x86writer);
  if (cs_support (CS_ARCH_X86))
    TESTLIST_REGISTER (x86relocator);
  TESTLIST_REGISTER (armwriter);
  if (cs_support (CS_ARCH_ARM))
    TESTLIST_REGISTER (armrelocator);
  TESTLIST_REGISTER (thumbwriter);
  if (cs_support (CS_ARCH_ARM))
    TESTLIST_REGISTER (thumbrelocator);
  TESTLIST_REGISTER (arm64writer);
  if (cs_support (CS_ARCH_ARM64))
    TESTLIST_REGISTER (arm64relocator);
  TESTLIST_REGISTER (interceptor);
#ifdef HAVE_DARWIN
  TESTLIST_REGISTER (interceptor_darwin);
  TESTLIST_REGISTER (unwind_broker);
#endif
#ifdef HAVE_ANDROID
  TESTLIST_REGISTER (interceptor_android);
#endif
#ifdef HAVE_ARM
  TESTLIST_REGISTER (interceptor_arm);
#endif
#ifdef HAVE_ARM64
  TESTLIST_REGISTER (interceptor_arm64);
#endif
#if !(defined (HAVE_FREEBSD) && defined (HAVE_ARM64))
  TESTLIST_REGISTER (memoryaccessmonitor);
#endif

  if (gum_stalker_is_supported ())
  {
#if defined (HAVE_I386) || defined (HAVE_ARM) || defined (HAVE_ARM64)
    TESTLIST_REGISTER (stalker);
#endif
#ifdef HAVE_MACOS
    TESTLIST_REGISTER (stalker_macos);
#endif
#if defined (HAVE_ARM64) && defined (HAVE_DARWIN)
    TESTLIST_REGISTER (stalker_darwin);
#endif
  }

  TESTLIST_REGISTER (api_resolver);
#if !defined (HAVE_QNX) && \
    !(defined (HAVE_MIPS))
  TESTLIST_REGISTER (backtracer);
#endif

  /* Heap */
  TESTLIST_REGISTER (allocation_tracker);
#ifdef HAVE_WINDOWS
  TESTLIST_REGISTER (allocator_probe);
  TESTLIST_REGISTER (allocator_probe_cxx);
  TESTLIST_REGISTER (cobjecttracker);
  TESTLIST_REGISTER (instancetracker);
#endif
  TESTLIST_REGISTER (pagepool);
#ifndef HAVE_WINDOWS
  if (gum_is_debugger_present ())
  {
    g_print (
        "\n"
        "***\n"
        "NOTE: Skipping BoundsChecker tests because debugger is attached\n"
        "***\n"
        "\n");
  }
  else
#endif
  {
#ifdef HAVE_WINDOWS
    TESTLIST_REGISTER (boundschecker);
#endif
  }
#ifdef HAVE_WINDOWS
  TESTLIST_REGISTER (sanitychecker);
#endif

  /* Prof */
#if !defined (HAVE_IOS) && !(defined (HAVE_ANDROID) && defined (HAVE_ARM64))
  TESTLIST_REGISTER (sampler);
  TESTLIST_REGISTER (profiler);
#endif

#if defined (HAVE_GUMJS) && defined (HAVE_FRIDA_GLIB)
  /* GumJS */
  {
    GumScriptBackend * qjs_backend, * v8_backend;

    qjs_backend = gum_script_backend_obtain_qjs ();
    if (qjs_backend != NULL)
      TESTLIST_REGISTER_WITH_DATA (script, qjs_backend);

    v8_backend = gum_script_backend_obtain_v8 ();
    if (v8_backend != NULL)
      TESTLIST_REGISTER_WITH_DATA (script, v8_backend);

# ifndef HAVE_ASAN
    if (g_test_slow () && gum_kernel_api_is_available ())
      TESTLIST_REGISTER (kscript);
# endif
  }
#endif

#if defined (HAVE_GUMPP) && defined (HAVE_WINDOWS)
  /* Gum++ */
  TESTLIST_REGISTER (gumpp_backtracer);
#endif

#ifdef _MSC_VER
#pragma warning (pop)
#endif

  num_tests = get_number_of_tests_in_suite (g_test_get_root ());

  timer = g_timer_new ();
  result = g_test_run ();
  t = g_timer_elapsed (timer, NULL);
  g_timer_destroy (timer);

  g_print ("\nRan %d tests in %.2f seconds\n", num_tests, t);

#if DEBUG_HEAP_LEAKS || defined (HAVE_ASAN)
  {
    GMainContext * context;

    context = g_main_context_get_thread_default ();
    while (g_main_context_pending (context))
      g_main_context_iteration (context, FALSE);
  }

  gum_shutdown ();
# ifdef HAVE_GUMJS
  gio_shutdown ();
# endif
  glib_shutdown ();

  _test_util_deinit ();

# ifdef HAVE_I386
  lowlevel_helpers_deinit ();
# endif

  gum_deinit ();
# ifdef HAVE_GUMJS
  gio_deinit ();
# endif
  glib_deinit ();
  gum_internal_heap_unref ();

# ifdef HAVE_WINDOWS
  WSACleanup ();
# endif
#endif

#if defined (HAVE_WINDOWS) && !DEBUG_HEAP_LEAKS
  if (IsDebuggerPresent ())
  {
    printf ("\nPress a key to exit.\n");
    _getch ();
  }
#endif

  return result;
}

/* HACK */
struct GTestSuite
{
  gchar * name;
  GSList * suites;
  GSList * cases;
};

static guint
get_number_of_tests_in_suite (GTestSuite * suite)
{
  guint total;
  GSList * cur;

  total = g_slist_length (suite->cases);
  for (cur = suite->suites; cur != NULL; cur = cur->next)
    total += get_number_of_tests_in_suite (cur->data);

  return total;
}

#ifdef HAVE_ANDROID

void
ClaimSignalChain (int signal,
                  struct sigaction * oldaction)
{
  /* g_print ("ClaimSignalChain(signal=%d)\n", signal); */
}

void
UnclaimSignalChain (int signal)
{
  /* g_print ("UnclaimSignalChain(signal=%d)\n", signal); */
}

void
InvokeUserSignalHandler (int signal,
                         siginfo_t * info,
                         void * context)
{
  /* g_print ("InvokeUserSignalHandler(signal=%d)\n", signal); */
}

void
InitializeSignalChain (void)
{
  /* g_print ("InitializeSignalChain()\n"); */
}

void
EnsureFrontOfChain (int signal,
                    struct sigaction * expected_action)
{
  /* g_print ("EnsureFrontOfChain(signal=%d)\n", signal); */
}

void
SetSpecialSignalHandlerFn (int signal,
                           gpointer fn)
{
  /* g_print ("SetSpecialSignalHandlerFn(signal=%d)\n", signal); */
}

void
AddSpecialSignalHandlerFn (int signal,
                           gpointer sa)
{
  /* g_print ("AddSpecialSignalHandlerFn(signal=%d)\n", signal); */
}

void
RemoveSpecialSignalHandlerFn (int signal,
                              bool (* fn) (int, siginfo_t *, void *))
{
  /* g_print ("RemoveSpecialSignalHandlerFn(signal=%d)\n", signal); */
}

#endif
