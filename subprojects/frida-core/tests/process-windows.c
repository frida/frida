#include "frida-tests.h"

#include <windows.h>
#include <psapi.h>

static WCHAR * frida_command_line_from_argv (gchar ** argv, gint argv_length);
static WCHAR * frida_environment_block_from_envp (gchar ** envp, gint envp_length);
static void frida_append_n_backslashes (GString * str, guint n);

char *
frida_test_process_backend_filename_of (void * handle)
{
  WCHAR filename_utf16[MAX_PATH + 1];

  GetModuleFileNameExW (handle, NULL, filename_utf16, sizeof (filename_utf16));

  return g_utf16_to_utf8 (filename_utf16, -1, NULL, NULL, NULL);
}

void *
frida_test_process_backend_self_handle (void)
{
  return GetCurrentProcess ();
}

guint
frida_test_process_backend_self_id (void)
{
  return GetCurrentProcessId ();
}

void
frida_test_process_backend_create (const char * path, gchar ** argv,
    int argv_length, gchar ** envp, int envp_length, FridaTestArch arch,
    gboolean suspended, void ** handle, guint * id, GError ** error)
{
  WCHAR * application_name, * command_line, * environment;
  STARTUPINFOW startup_info = { 0, };
  PROCESS_INFORMATION process_info = { 0, };
  BOOL success;

  (void) arch;
  (void) suspended;

  application_name = (WCHAR *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
  command_line = frida_command_line_from_argv (argv, argv_length);
  environment = frida_environment_block_from_envp (envp, envp_length);

  startup_info.cb = sizeof (startup_info);

  success = CreateProcessW (
      application_name,
      command_line,
      NULL,
      NULL,
      FALSE,
      CREATE_UNICODE_ENVIRONMENT,
      environment,
      NULL,
      &startup_info,
      &process_info);

  if (success)
  {
    CloseHandle (process_info.hThread);

    *handle = process_info.hProcess;
    *id = process_info.dwProcessId;
  }
  else
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unable to spawn executable at '%s': 0x%08lx\n",
        path, GetLastError ());
  }

  g_free (environment);
  g_free (command_line);
  g_free (application_name);
}

int
frida_test_process_backend_join (void * handle, guint timeout_msec, GError ** error)
{
  DWORD exit_code;

  if (WaitForSingleObject (handle,
      (timeout_msec != 0) ? timeout_msec : INFINITE) == WAIT_TIMEOUT)
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_TIMED_OUT,
        "Timed out while waiting for process to exit");
    return -1;
  }

  GetExitCodeProcess (handle, &exit_code);
  CloseHandle (handle);

  return exit_code;
}

void
frida_test_process_backend_resume (void * handle, GError ** error)
{
  (void) handle;

  g_set_error (error,
      FRIDA_ERROR,
      FRIDA_ERROR_NOT_SUPPORTED,
      "Not implemented on this OS");
}

void
frida_test_process_backend_kill (void * handle)
{
  TerminateProcess (handle, 1);
  CloseHandle (handle);
}

static WCHAR *
frida_command_line_from_argv (gchar ** argv, gint argv_length)
{
  GString * line;
  WCHAR * line_utf16;
  gint i;

  line = g_string_new ("");

  for (i = 0; i != argv_length; i++)
  {
    const gchar * arg = argv[i];
    gboolean no_quotes_needed;

    if (i > 0)
      g_string_append_c (line, ' ');

    no_quotes_needed = arg[0] != '\0' &&
        g_utf8_strchr (arg, -1, ' ') == NULL &&
        g_utf8_strchr (arg, -1, '\t') == NULL &&
        g_utf8_strchr (arg, -1, '\n') == NULL &&
        g_utf8_strchr (arg, -1, '\v') == NULL &&
        g_utf8_strchr (arg, -1, '"') == NULL;
    if (no_quotes_needed)
    {
      g_string_append (line, arg);
    }
    else
    {
      const gchar * c;

      g_string_append_c (line, '"');

      for (c = arg; *c != '\0'; c = g_utf8_next_char (c))
      {
        guint num_backslashes = 0;

        while (*c != '\0' && *c == '\\')
        {
          num_backslashes++;
          c++;
        }

        if (*c == '\0')
        {
          frida_append_n_backslashes (line, num_backslashes * 2);
          break;
        }
        else if (*c == '"')
        {
          frida_append_n_backslashes (line, (num_backslashes * 2) + 1);
          g_string_append_c (line, *c);
        }
        else
        {
          frida_append_n_backslashes (line, num_backslashes);
          g_string_append_unichar (line, g_utf8_get_char (c));
        }
      }

      g_string_append_c (line, '"');
    }
  }

  line_utf16 = (WCHAR *) g_utf8_to_utf16 (line->str, -1, NULL, NULL, NULL);

  g_string_free (line, TRUE);

  return line_utf16;
}

static WCHAR *
frida_environment_block_from_envp (gchar ** envp, gint envp_length)
{
  GString * block;
  gint i;

  block = g_string_new ("");

  if (envp_length > 0)
  {
    for (i = 0; i != envp_length; i++)
    {
      gunichar2 * var;
      glong items_written;

      var = g_utf8_to_utf16 (envp[i], -1, NULL, &items_written, NULL);
      g_string_append_len (block, (gchar *) var, (items_written + 1) * sizeof (gunichar2));
      g_free (var);
    }
  }
  else
  {
    g_string_append_c (block, '\0');
    g_string_append_c (block, '\0');
  }
  g_string_append_c (block, '\0');

  return (WCHAR *) g_string_free (block, FALSE);
}

static void
frida_append_n_backslashes (GString * str, guint n)
{
  guint i;

  for (i = 0; i != n; i++)
    g_string_append_c (str, '\\');
}
