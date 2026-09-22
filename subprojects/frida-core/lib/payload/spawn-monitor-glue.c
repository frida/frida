#include "frida-payload.h"

#ifdef HAVE_WINDOWS

# define VC_EXTRALEAN
# include <windows.h>

static gchar * frida_ansi_string_to_utf8 (const gchar * str_ansi, gint length);

guint32
_frida_spawn_monitor_resume_thread (void * thread)
{
  return ResumeThread (thread);
}

gchar **
_frida_spawn_monitor_get_environment (int * length)
{
  gchar ** result;
  LPWCH strings;

  strings = GetEnvironmentStringsW ();
  result = _frida_spawn_monitor_parse_unicode_environment (strings, length);
  FreeEnvironmentStringsW (strings);

  return result;
}

gchar **
_frida_spawn_monitor_parse_unicode_environment (void * env, int * length)
{
  GPtrArray * result;
  WCHAR * element_data;
  gsize element_length;

  result = g_ptr_array_new ();

  element_data = env;
  while ((element_length = wcslen (element_data)) != 0)
  {
    g_ptr_array_add (result, g_utf16_to_utf8 (element_data, element_length, NULL, NULL, NULL));
    element_data += element_length + 1;
  }

  *length = result->len;

  g_ptr_array_add (result, NULL);

  return (gchar **) g_ptr_array_free (result, FALSE);
}

gchar **
_frida_spawn_monitor_parse_ansi_environment (void * env, int * length)
{
  GPtrArray * result;
  gchar * element_data;
  gsize element_length;

  result = g_ptr_array_new ();

  element_data = env;
  while ((element_length = strlen (element_data)) != 0)
  {
    g_ptr_array_add (result, frida_ansi_string_to_utf8 (element_data, element_length));
    element_data += element_length + 1;
  }

  *length = result->len;

  g_ptr_array_add (result, NULL);

  return (gchar **) g_ptr_array_free (result, FALSE);
}

static gchar *
frida_ansi_string_to_utf8 (const gchar * str_ansi, gint length)
{
  guint str_utf16_size;
  WCHAR * str_utf16;
  gchar * str_utf8;

  if (length < 0)
    length = (gint) strlen (str_ansi);

  str_utf16_size = (guint) (length + 1) * sizeof (WCHAR);
  str_utf16 = (WCHAR *) g_malloc (str_utf16_size);
  MultiByteToWideChar (CP_ACP, 0, str_ansi, length, str_utf16, str_utf16_size);
  str_utf16[length] = L'\0';
  str_utf8 = g_utf16_to_utf8 ((gunichar2 *) str_utf16, -1, NULL, NULL, NULL);
  g_free (str_utf16);

  return str_utf8;
}

#endif
