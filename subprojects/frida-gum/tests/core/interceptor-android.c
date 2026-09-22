/*
 * Copyright (C) 2017-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "interceptor-android-fixture.c"

TESTLIST_BEGIN (interceptor_android)
  TESTENTRY (can_attach_to_close_with_two_unrelated_interceptors)
  TESTENTRY (can_attach_to_dlopen)
  TESTENTRY (can_attach_to_fork)
  TESTENTRY (can_attach_to_set_argv0)
TESTLIST_END ()

TESTCASE (can_attach_to_close_with_two_unrelated_interceptors)
{
  GumInterceptor * other_interceptor;
  int (* close_impl) (int fd);
  int fd;

  other_interceptor = g_object_new (GUM_TYPE_INTERCEPTOR, NULL);

  close_impl = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (gum_process_get_libc_module (), "close"));

  fd = eventfd (FALSE, EFD_CLOEXEC);
  g_assert_true (fd != -1);

  interceptor_fixture_attach (fixture, 0, close_impl, '>', '<');

  gum_interceptor_attach (other_interceptor, close_impl,
      GUM_INVOCATION_LISTENER (fixture->listener_context[0]->listener),
      NULL);

  close_impl (fd);

  g_assert_cmpstr (fixture->result->str, ==, ">><<");

  g_object_unref (other_interceptor);
}

TESTCASE (can_attach_to_dlopen)
{
  void * (* dlopen_impl) (const char * filename, int flags);
  void * libc;

  dlopen_impl = GSIZE_TO_POINTER (
      gum_module_find_global_export_by_name ("dlopen"));

  interceptor_fixture_attach (fixture, 0, dlopen_impl, '>', '<');

  libc = dlopen ("libc.so", RTLD_LAZY | RTLD_GLOBAL);
  g_assert_nonnull (libc);

  dlclose (libc);

  g_assert_cmpstr (fixture->result->str, ==, "><");
}

TESTCASE (can_attach_to_fork)
{
  pid_t (* fork_impl) (void);
  pid_t pid;

  fork_impl = GSIZE_TO_POINTER (
      gum_module_find_export_by_name (gum_process_get_libc_module (), "fork"));

  interceptor_fixture_attach (fixture, 0, fork_impl, '>', '<');

  pid = fork_impl ();
  if (pid == 0)
  {
    exit (0);
  }
  g_assert_cmpint (pid, !=, -1);
  g_assert_cmpstr (fixture->result->str, ==, "><");
}

typedef struct _GumRuntimeBounds GumRuntimeBounds;

struct _GumRuntimeBounds
{
  gpointer start;
  gpointer end;
};

static gboolean gum_store_runtime_bounds (GumModule * module,
    GumRuntimeBounds * bounds);

TESTCASE (can_attach_to_set_argv0)
{
  JNIEnv * env = java_env;
  jclass process;
  jmethodID set_argv0;
  GumRuntimeBounds runtime_bounds;
  guint offset;
  gpointer set_argv0_impl = NULL;

  process = (*env)->FindClass (env, "android/os/Process");
  g_assert_nonnull (process);

  set_argv0 = (*env)->GetStaticMethodID (env, process, "setArgV0",
      "(Ljava/lang/String;)V");
  g_assert_nonnull (set_argv0);

  runtime_bounds.start = NULL;
  runtime_bounds.end = NULL;
  gum_process_enumerate_modules ((GumFoundModuleFunc) gum_store_runtime_bounds,
      &runtime_bounds);
  g_assert_true (runtime_bounds.end != runtime_bounds.start);

  for (offset = 0; offset != 64; offset += 4)
  {
    gpointer address = *((gpointer *) (GPOINTER_TO_SIZE (set_argv0) + offset));

    if (address >= runtime_bounds.start && address < runtime_bounds.end)
    {
      set_argv0_impl = address;
      break;
    }
  }

  interceptor_fixture_attach (fixture, 0, set_argv0_impl, '>', '<');
}

static gboolean
gum_store_runtime_bounds (GumModule * module,
                          GumRuntimeBounds * bounds)
{
  const GumMemoryRange * range;

  if (strcmp (gum_module_get_name (module), "libandroid_runtime.so") != 0)
    return TRUE;

  range = gum_module_get_range (module);
  bounds->start = GSIZE_TO_POINTER (range->base_address);
  bounds->end = GSIZE_TO_POINTER (range->base_address + range->size);

  return FALSE;
}
