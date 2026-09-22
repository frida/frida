#include "server-ios-tvos.h"

#include <dlfcn.h>
#include <gio/gio.h>
#include <gum/gumdarwin.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <unistd.h>

#define FRIDA_TRUST_CACHE_INJECT_PATH "/usr/bin/inject"

#ifndef MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT
# define MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT 6
#endif
#ifndef RENAME_SWAP
# define RENAME_SWAP 0x00000002
#endif

extern int memorystatus_control (uint32_t command, int32_t pid, uint32_t flags, void * buffer, size_t buffer_size);

static gboolean frida_is_platformized (void);
static gboolean frida_try_platformize (const gchar * path);

static gchar * frida_get_executable_path (void);
static gboolean frida_refresh_inode (const gchar * path);
static gboolean frida_add_to_trust_cache (const gchar * path);

void
_frida_server_ios_tvos_configure (void)
{
  memorystatus_control (MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT, getpid (), 256, NULL, 0);

  if (!frida_is_platformized ())
  {
    gchar * server_path;

    server_path = frida_get_executable_path ();

    if (frida_try_platformize (server_path))
    {
      g_print (
          "***\n"
          "*** The %s executable is now in the kernel's trust cache; please restart it.\n"
          "*** This is normally handled by launchd and you should not see this message.\n"
          "***\n",
          server_path);
      exit (0);
    }

    g_free (server_path);
  }
}

static gboolean
frida_is_platformized (void)
{
  gboolean result;
  gboolean system_has_guarded_ports;
  mach_port_t self_task, launchd_task, launchd_rx;
  const gint launchd_pid = 1;
  kern_return_t kr;

  system_has_guarded_ports = gum_darwin_check_xnu_version (7938, 0, 0);
  if (system_has_guarded_ports)
    return TRUE;

  self_task = mach_task_self ();

  kr = task_for_pid (self_task, launchd_pid, &launchd_task);
  if (kr != KERN_SUCCESS)
    return TRUE;

  kr = mach_port_allocate (launchd_task, MACH_PORT_RIGHT_RECEIVE, &launchd_rx);
  if (kr == KERN_SUCCESS)
  {
    mach_port_deallocate (launchd_task, launchd_rx);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }

  mach_port_deallocate (self_task, launchd_task);

  return result;
}

static gboolean
frida_try_platformize (const gchar * path)
{
  if (!frida_add_to_trust_cache (path))
    return FALSE;

  if (!frida_refresh_inode (path))
    return FALSE;

  return TRUE;
}

static gchar *
frida_get_executable_path (void)
{
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
}

static gboolean
frida_refresh_inode (const gchar * path)
{
  gboolean success = FALSE;
  int (* clonefile) (const char * src, const char * dst, int flags);
  int (* renamex_np) (const char * from, const char * to, unsigned int flags);
  gchar * temp_path;

  clonefile = dlsym (RTLD_DEFAULT, "clonefile");
  if (clonefile == NULL)
    return FALSE;

  renamex_np = dlsym (RTLD_DEFAULT, "renamex_np");
  if (renamex_np == NULL)
    return FALSE;

  temp_path = g_strconcat (path, ".tmp", NULL);

  unlink (temp_path);
  if (clonefile (path, temp_path, 0) != 0)
    goto beach;

  success = renamex_np (temp_path, path, RENAME_SWAP) == 0;

  unlink (temp_path);

beach:
  g_free (temp_path);

  return success;
}

static gboolean
frida_add_to_trust_cache (const gchar * path)
{
  GSubprocess * process;
  GError * error;

  if (!g_file_test (FRIDA_TRUST_CACHE_INJECT_PATH, G_FILE_TEST_EXISTS))
    return FALSE;

  error = NULL;
  process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
      &error, FRIDA_TRUST_CACHE_INJECT_PATH, path, NULL);

  if (error != NULL)
    goto inject_failed;

  if (!g_subprocess_wait_check (process, NULL, &error))
    goto inject_failed;

  g_object_unref (process);

  return TRUE;

inject_failed:
  {
    g_error_free (error);
    g_clear_object (&process);
    return FALSE;
  }
}
