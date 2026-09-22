/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 * Copyright (C) 2009 Haakon Sporsheim <haakon.sporsheim@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __TEST_UTIL_H__
#define __TEST_UTIL_H__

#include <gum/gum.h>
#include <gum/gum-heap.h>
#include <gum/gum-prof.h>

#define TESTLIST_BEGIN(NAME)                                                \
    void test_ ##NAME## _add_tests (gpointer fixture_data)                  \
    {                                                                       \
      G_GNUC_UNUSED const gchar * group = "/";
#define TESTLIST_END()                                                      \
    }

#define TESTENTRY_SIMPLE(NAME, PREFIX, FUNC)                                \
    G_STMT_START                                                            \
    {                                                                       \
      gchar * path;                                                         \
      extern void PREFIX## _ ##FUNC (void);                                 \
                                                                            \
      path = g_strconcat ("/" NAME, group, #FUNC, NULL);                    \
                                                                            \
      g_test_add_func (path, PREFIX## _ ##FUNC);                            \
                                                                            \
      g_free (path);                                                        \
    }                                                                       \
    G_STMT_END;
#define TESTENTRY_WITH_FIXTURE(NAME, PREFIX, FUNC, STRUCT)                  \
    G_STMT_START                                                            \
    {                                                                       \
      gchar * path;                                                         \
      extern void PREFIX## _ ##FUNC (STRUCT * fixture, gconstpointer data); \
                                                                            \
      path = g_strconcat ("/" NAME, group, #FUNC, NULL);                    \
                                                                            \
      g_test_add (path,                                                     \
          STRUCT,                                                           \
          fixture_data,                                                     \
          PREFIX## _fixture_setup,                                          \
          PREFIX## _ ##FUNC,                                                \
          PREFIX## _fixture_teardown);                                      \
                                                                            \
      g_free (path);                                                        \
    }                                                                       \
    G_STMT_END;

#define TESTGROUP_BEGIN(NAME)                                               \
    group = "/" NAME "/";
#define TESTGROUP_END()                                                     \
    group = "/";

#define TESTLIST_REGISTER(NAME) TESTLIST_REGISTER_WITH_DATA (NAME, NULL)
#define TESTLIST_REGISTER_WITH_DATA(NAME, FIXTURE_DATA)                     \
    G_STMT_START                                                            \
    {                                                                       \
      extern void test_ ##NAME## _add_tests (gpointer fixture_data);        \
      test_ ##NAME## _add_tests (FIXTURE_DATA);                             \
    }                                                                       \
    G_STMT_END

#define GUM_ASSERT_CMPADDR(n1, cmp, n2) \
    g_assert_cmphex (GPOINTER_TO_SIZE (n1), cmp, GPOINTER_TO_SIZE (n2))

#ifdef HAVE_WINDOWS
# define GUM_TESTS_MODULE_NAME "gum-tests.exe"
#else
# define GUM_TESTS_MODULE_NAME "gum-tests"
#endif
#define SYSTEM_MODULE_NAME test_util_get_system_module_name ()
#if defined (HAVE_WINDOWS)
# define SYSTEM_MODULE_EXPORT "NtClose"
#elif defined (HAVE_QNX)
# define SYSTEM_MODULE_EXPORT "bt_get_backtrace"
#else
# define SYSTEM_MODULE_EXPORT "sendto"
#endif
#ifdef HAVE_ANDROID
# define TRICKY_MODULE_NAME test_util_get_android_java_vm_module_name ()
# define TRICKY_MODULE_EXPORT "JNI_GetCreatedJavaVMs"
#else
# define TRICKY_MODULE_NAME SYSTEM_MODULE_NAME
# define TRICKY_MODULE_EXPORT SYSTEM_MODULE_EXPORT
#endif

#ifdef HAVE_DARWIN
# define GUM_HOOK_TARGET GUM_NOINLINE \
    __attribute__ ((section ("__TEXT,__hook_targets"), aligned (16384)))
#else
# define GUM_HOOK_TARGET GUM_NOINLINE
#endif

G_BEGIN_DECLS

G_GNUC_INTERNAL void _test_util_init (void);
G_GNUC_INTERNAL void _test_util_deinit (void);

GumSampler * heap_access_counter_new (void);

void assert_basename_equals (const gchar * expected_filename,
    const gchar * actual_filename);

gchar * test_util_diff_binary (const guint8 * expected_bytes,
    guint expected_length, const guint8 * actual_bytes,
    guint actual_length);
gchar * test_util_diff_text (const gchar * expected_text,
    const gchar * actual_text);
gchar * test_util_diff_xml (const gchar * expected_xml,
    const gchar * actual_xml);

gchar * test_util_get_data_dir (void);
const gchar * test_util_get_system_module_name (void);
#ifdef HAVE_ANDROID
const gchar * test_util_get_android_java_vm_module_name (void);
#endif

const GumHeapApiList * test_util_heap_apis (void);

gboolean gum_is_debugger_present (void);
guint8 gum_try_read_and_write_at (guint8 * a, guint i,
    gboolean * exception_raised_on_read, gboolean * exception_raised_on_write);

void gum_ensure_current_thread_is_named (const gchar * name);

G_END_DECLS

#endif
