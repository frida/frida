#include "frida-tests.h"

#include "frida-tvos.h"

#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#ifdef HAVE_DARWIN
# include <fcntl.h>
# include <mach-o/dyld.h>
# include <signal.h>
# include <spawn.h>
# include <sys/types.h>
#endif
#ifdef HAVE_FREEBSD
# include <gum/gumfreebsd.h>
#endif

#ifdef HAVE_QNX
# include <spawn.h>
# include <gum/gumqnx.h>
#endif

#if !(defined (HAVE_DARWIN) || defined (HAVE_QNX))

typedef struct _FridaTestWaitContext FridaTestWaitContext;

struct _FridaTestWaitContext
{
  gint ref_count;
  gpointer process;
  GMainLoop * loop;
  gboolean timed_out;
};

# ifdef HAVE_ANDROID

typedef struct _FridaTestSuperSUSpawnContext FridaTestSuperSUSpawnContext;

struct _FridaTestSuperSUSpawnContext
{
  GMainLoop * loop;
  FridaSuperSUProcess * process;
  GDataInputStream * output;
  guint pid;
  GError ** error;
};

static void frida_test_process_backend_on_super_su_spawn_ready (GObject * source_object, GAsyncResult * res, gpointer user_data);
static void frida_test_process_backend_on_super_su_read_line_ready (GObject * source_object, GAsyncResult * res, gpointer user_data);
# endif

static void frida_test_process_backend_on_wait_ready (GObject * source_object, GAsyncResult * res, gpointer user_data);
static gboolean frida_test_process_backend_on_wait_timeout (gpointer user_data);

static FridaTestWaitContext * frida_test_wait_context_new (gpointer process);
static FridaTestWaitContext * frida_test_wait_context_ref (FridaTestWaitContext * context);
static void frida_test_wait_context_unref (FridaTestWaitContext * context);

#endif

static int frida_magic_self_handle = -1;

char *
frida_test_process_backend_filename_of (void * handle)
{
#if defined (HAVE_DARWIN)
  guint image_count, image_idx;

  g_assert_true (handle == &frida_magic_self_handle);

  image_count = _dyld_image_count ();
  for (image_idx = 0; image_idx != image_count; image_idx++)
  {
    if (_dyld_get_image_header (image_idx)->filetype == MH_EXECUTE)
      return g_strdup (_dyld_get_image_name (image_idx));
  }

  g_assert_not_reached ();
  return NULL;
#elif defined (HAVE_LINUX)
  g_assert_true (handle == &frida_magic_self_handle);

  return g_file_read_link ("/proc/self/exe", NULL);
#elif defined (HAVE_FREEBSD)
  g_assert_true (handle == &frida_magic_self_handle);

  return gum_freebsd_query_program_path_for_self (NULL);
#elif defined (HAVE_QNX)
  return gum_qnx_query_program_path_for_self (NULL);
#endif
}

void *
frida_test_process_backend_self_handle (void)
{
  return &frida_magic_self_handle;
}

guint
frida_test_process_backend_self_id (void)
{
  return getpid ();
}

void
frida_test_process_backend_create (const char * path, gchar ** argv,
    int argv_length, gchar ** envp, int envp_length, FridaTestArch arch,
    gboolean suspended, void ** handle, guint * id, GError ** error)
{
  const gchar * override = g_getenv ("FRIDA_TARGET_PID");
  if (override != NULL)
  {
    *id = atoi (override);
    *handle = GSIZE_TO_POINTER (*id);
  }
  else
  {
#if defined (HAVE_DARWIN)
    posix_spawn_file_actions_t actions;
    const gchar * stdio_output_path;
    posix_spawnattr_t attr;
    sigset_t signal_mask_set;
    int result;
    cpu_type_t pref;
    gchar * special_path;
    size_t ocount;
    pid_t pid;

    posix_spawn_file_actions_init (&actions);
    posix_spawn_file_actions_addinherit_np (&actions, 0);

    stdio_output_path = g_getenv ("FRIDA_STDIO_OUTPUT");
    if (stdio_output_path != NULL)
    {
      posix_spawn_file_actions_addopen (&actions, 1, stdio_output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      posix_spawn_file_actions_adddup2 (&actions, 1, 2);
    }
    else
    {
      posix_spawn_file_actions_addinherit_np (&actions, 1);
      posix_spawn_file_actions_addinherit_np (&actions, 2);
    }

    posix_spawnattr_init (&attr);
    sigemptyset (&signal_mask_set);
    posix_spawnattr_setsigmask (&attr, &signal_mask_set);
    posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_CLOEXEC_DEFAULT |
        (suspended ? POSIX_SPAWN_START_SUSPENDED : 0));

    special_path = NULL;

# if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
    pref = (arch == FRIDA_TEST_ARCH_CURRENT) ? CPU_TYPE_X86 : CPU_TYPE_X86_64;
# elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
    pref = (arch == FRIDA_TEST_ARCH_CURRENT) ? CPU_TYPE_X86_64 : CPU_TYPE_X86;
# elif defined (HAVE_ARM)
    pref = (arch == FRIDA_TEST_ARCH_CURRENT) ? CPU_TYPE_ARM : CPU_TYPE_ARM64;
# elif defined (HAVE_ARM64)
    pref = CPU_TYPE_ARM64;
#  if __has_feature (ptrauth_calls)
    if (arch == FRIDA_TEST_ARCH_CURRENT)
#  else
    if (arch == FRIDA_TEST_ARCH_OTHER)
#  endif
    {
      special_path = g_strconcat (path, "-arm64e", NULL);
      path = special_path;
    }
# endif
    posix_spawnattr_setbinpref_np (&attr, 1, &pref, &ocount);

    result = posix_spawn (&pid, path, &actions, &attr, argv, envp);

    posix_spawnattr_destroy (&attr);
    posix_spawn_file_actions_destroy (&actions);

    if (result == 0)
    {
      g_free (special_path);
    }
    else
    {
      g_set_error (error,
          FRIDA_ERROR,
          FRIDA_ERROR_INVALID_ARGUMENT,
          "Unable to spawn executable at '%s': %s",
          path, g_strerror (errno));
      g_free (special_path);
      return;
    }

    *handle = GSIZE_TO_POINTER (pid);
    *id = pid;
#elif defined (HAVE_QNX)
    int result;
    pid_t pid;

    result = posix_spawn (&pid, path, NULL, NULL, argv, envp);
    if (result != 0)
    {
      g_set_error (error,
          FRIDA_ERROR,
          FRIDA_ERROR_INVALID_ARGUMENT,
          "Unable to spawn executable at '%s': %s",
          path, g_strerror (errno));
      return;
    }

    *handle = GSIZE_TO_POINTER (pid);
    *id = pid;
#else
    GSubprocessLauncher * launcher;
    GSubprocess * subprocess;
    GError * spawn_error = NULL;

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_INHERIT);
    g_subprocess_launcher_set_environ (launcher, envp);
    subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *) argv, &spawn_error);
    g_object_unref (launcher);

    if (subprocess != NULL)
    {
      *handle = subprocess;
      *id = atoi (g_subprocess_get_identifier (subprocess));
    }
    else
    {
# ifdef HAVE_ANDROID
      if (spawn_error->domain == G_SPAWN_ERROR && spawn_error->code == G_SPAWN_ERROR_ACCES)
      {
        FridaTestSuperSUSpawnContext ctx;
        gchar * args, * wrapper_argv[] = { "su", "-c", NULL, NULL };

        args = g_strjoinv (" ", argv);

        wrapper_argv[0] = "su";
        wrapper_argv[1] = "-c";
        wrapper_argv[2] = g_strconcat ("echo $BASHPID; exec ", args, NULL);

        g_free (args);

        ctx.loop = g_main_loop_new (NULL, FALSE);
        ctx.process = NULL;
        ctx.output = NULL;
        ctx.pid = 0;
        ctx.error = error;

        frida_super_su_spawn ("/", wrapper_argv, 3, envp, envp_length, TRUE, NULL, frida_test_process_backend_on_super_su_spawn_ready, &ctx);

        g_free (wrapper_argv[2]);

        g_main_loop_run (ctx.loop);

        *handle = ctx.process;
        *id = ctx.pid;

        if (ctx.output != NULL)
          g_object_unref (ctx.output);
        g_main_loop_unref (ctx.loop);
      }
      else
# endif
      {
        g_set_error_literal (error,
            FRIDA_ERROR,
            FRIDA_ERROR_INVALID_ARGUMENT,
            spawn_error->message);
      }

      g_error_free (spawn_error);
    }
#endif
  }
}

int
frida_test_process_backend_join (void * handle, guint timeout_msec,
    GError ** error)
{
  int status = -1;

#if defined (HAVE_DARWIN) || defined (HAVE_QNX)
  GTimer * timer;

  timer = g_timer_new ();

  while (TRUE)
  {
    int ret;

    ret = waitpid (GPOINTER_TO_SIZE (handle), &status, WNOHANG);
    if (ret > 0)
    {
      if (WIFEXITED (status))
      {
        status = WEXITSTATUS (status);
      }
      else
      {
        g_set_error (error,
            FRIDA_ERROR,
            FRIDA_ERROR_NOT_SUPPORTED,
            "Unexpected error while waiting for process to exit (child process crashed)");
        status = -1;
      }

      break;
    }
    else if (ret < 0 && errno != ETIMEDOUT)
    {
      g_set_error (error,
          FRIDA_ERROR,
          FRIDA_ERROR_NOT_SUPPORTED,
          "Unexpected error while waiting for process to exit (waitpid returned '%s')",
          g_strerror (errno));
      break;
    }
    else if (g_timer_elapsed (timer, NULL) * 1000.0 >= timeout_msec)
    {
      g_set_error (error,
          FRIDA_ERROR,
          FRIDA_ERROR_TIMED_OUT,
          "Timed out while waiting for process to exit");
      break;
    }

    g_usleep (G_USEC_PER_SEC / 50);
  }

  g_timer_destroy (timer);
#else
  FridaTestWaitContext * context;

  context = frida_test_wait_context_new (handle);

# ifdef HAVE_ANDROID
  if (FRIDA_SUPER_SU_IS_PROCESS (handle))
  {
    FridaSuperSUProcess * process = handle;
    guint timeout;

    frida_super_su_process_wait (process, NULL, frida_test_process_backend_on_wait_ready, frida_test_wait_context_ref (context));
    timeout = g_timeout_add (timeout_msec, frida_test_process_backend_on_wait_timeout, frida_test_wait_context_ref (context));

    g_main_loop_run (context->loop);

    if (!context->timed_out)
    {
      g_source_remove (timeout);

      status = frida_super_su_process_get_exit_status (process);
    }
  }
  else
# endif
  {
    GSubprocess * subprocess = handle;
    guint timeout;

    g_subprocess_wait_async (subprocess, NULL, frida_test_process_backend_on_wait_ready, frida_test_wait_context_ref (context));
    timeout = g_timeout_add (timeout_msec, frida_test_process_backend_on_wait_timeout, frida_test_wait_context_ref (context));

    g_main_loop_run (context->loop);

    if (!context->timed_out)
    {
      g_source_remove (timeout);

      if (g_subprocess_get_if_exited (subprocess))
        status = g_subprocess_get_exit_status (subprocess);
    }
  }

  if (context->timed_out)
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_TIMED_OUT,
        "Timed out while waiting for process to exit");
  }

  frida_test_wait_context_unref (context);
#endif

  return status;
}

void
frida_test_process_backend_resume (void * handle, GError ** error)
{
#if defined (HAVE_DARWIN) || defined (HAVE_QNX)
  kill (GPOINTER_TO_SIZE (handle), SIGCONT);
#else
  (void) handle;

  g_set_error (error,
      FRIDA_ERROR,
      FRIDA_ERROR_NOT_SUPPORTED,
      "Not implemented on this OS");
#endif
}

void
frida_test_process_backend_kill (void * handle)
{
#if defined (HAVE_DARWIN) || defined (HAVE_QNX)
  kill (GPOINTER_TO_SIZE (handle), SIGKILL);
#else
  g_subprocess_force_exit (handle);
  g_object_unref (handle);
#endif
}

#if !(defined (HAVE_DARWIN) || defined (HAVE_QNX))

# ifdef HAVE_ANDROID

static void
frida_test_process_backend_on_super_su_spawn_ready (GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  FridaTestSuperSUSpawnContext * ctx = user_data;

  ctx->process = frida_super_su_spawn_finish (res, ctx->error);
  if (ctx->process == NULL)
  {
    g_main_loop_quit (ctx->loop);
    return;
  }

  ctx->output = g_data_input_stream_new (frida_super_su_process_get_output (ctx->process));
  g_data_input_stream_read_line_async (ctx->output, G_PRIORITY_DEFAULT, NULL, frida_test_process_backend_on_super_su_read_line_ready, ctx);
}

static void
frida_test_process_backend_on_super_su_read_line_ready (GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  FridaTestSuperSUSpawnContext * ctx = user_data;
  gsize length;
  gchar * line;

  line = g_data_input_stream_read_line_finish_utf8 (ctx->output, res, &length, ctx->error);
  if (line != NULL)
  {
    ctx->pid = atoi (line);
    g_free (line);
  }

  g_main_loop_quit (ctx->loop);
}

# endif

static void
frida_test_process_backend_on_wait_ready (GObject * source_object, GAsyncResult * res, gpointer user_data)
{
  FridaTestWaitContext * ctx = user_data;

  g_main_loop_quit (ctx->loop);

  frida_test_wait_context_unref (ctx);
}

static gboolean
frida_test_process_backend_on_wait_timeout (gpointer user_data)
{
  FridaTestWaitContext * ctx = user_data;

  ctx->timed_out = TRUE;
  g_main_loop_quit (ctx->loop);

  frida_test_wait_context_unref (ctx);

  return FALSE;
}

static FridaTestWaitContext *
frida_test_wait_context_new (gpointer process)
{
  FridaTestWaitContext * context;

  context = g_slice_new (FridaTestWaitContext);
  context->ref_count = 1;
  context->process = process;
  context->loop = g_main_loop_new (NULL, FALSE);
  context->timed_out = FALSE;

  return context;
}

static FridaTestWaitContext *
frida_test_wait_context_ref (FridaTestWaitContext * context)
{
  context->ref_count++;
  return context;
}

static void
frida_test_wait_context_unref (FridaTestWaitContext * context)
{
  if (--context->ref_count == 0)
  {
    g_main_loop_unref (context->loop);
    g_object_unref (context->process);

    g_slice_free (FridaTestWaitContext, context);
  }
}

#endif
