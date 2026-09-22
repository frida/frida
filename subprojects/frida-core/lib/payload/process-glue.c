#include "frida-payload.h"

#ifdef HAVE_WINDOWS
# define VC_EXTRALEAN
# include <windows.h>
#else
# include <pthread.h>
# include <signal.h>
# include <unistd.h>
#endif
#ifdef HAVE_DARWIN
# include <limits.h>
# include <mach-o/dyld.h>
#endif

guint
frida_get_process_id (void)
{
#ifdef HAVE_WINDOWS
  return GetCurrentProcessId ();
#else
  return getpid ();
#endif
}

gpointer
frida_get_current_pthread (void)
{
#ifndef HAVE_WINDOWS
  return (gpointer) pthread_self ();
#else
  return NULL;
#endif
}

void
frida_join_pthread (gpointer pthread)
{
#ifndef HAVE_WINDOWS
  pthread_join ((pthread_t) pthread, NULL);
#endif
}

void
frida_kill_process (guint pid)
{
#ifdef HAVE_WINDOWS
  HANDLE process;

  process = OpenProcess (PROCESS_TERMINATE, FALSE, pid);
  if (process == NULL)
    return;

  TerminateProcess (process, 1);

  CloseHandle (process);
#else
  kill (pid, SIGKILL);
#endif
}

gchar *
frida_try_get_executable_path (void)
{
#ifdef HAVE_DARWIN
  uint32_t buf_size;
  gchar * buf;

  buf_size = PATH_MAX;

  do
  {
    buf = g_malloc (buf_size);
    if (_NSGetExecutablePath (buf, &buf_size) == 0)
      return buf;

    g_free (buf);
  }
  while (TRUE);
#elif HAVE_LINUX
  return g_file_read_link ("/proc/self/exe", NULL);
#else
  return NULL;
#endif
}
