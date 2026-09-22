#include "frida-core.h"

#include "icon-helpers.h"

#include <psapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winternl.h>

#define DRIVE_STRINGS_MAX_LENGTH     (512)

typedef struct _FridaEnumerateProcessesOperation FridaEnumerateProcessesOperation;

struct _FridaEnumerateProcessesOperation
{
  FridaScope scope;
  GHashTable * ppid_by_pid;
  guint frontmost_pid;

  GArray * result;
};

static void frida_collect_process_info (guint pid, FridaEnumerateProcessesOperation * op);
static gboolean frida_add_process_metadata (GHashTable * parameters, guint pid, HANDLE process, FridaEnumerateProcessesOperation * op);

static gboolean frida_get_process_filename (HANDLE process, WCHAR * name, DWORD name_capacity);
static GVariant * frida_query_process_argv (guint pid);
static GVariant * frida_get_process_user (HANDLE process);
static GVariant * frida_get_process_start_time (HANDLE process);

static GHashTable * frida_build_ppid_table (void);
static guint frida_get_frontmost_pid (void);

static GDateTime * frida_parse_filetime (const FILETIME * ft);
static gint64 frida_filetime_to_unix (const FILETIME * ft);

void
frida_system_get_frontmost_application (FridaFrontmostQueryOptions * options, FridaHostApplicationInfo * result, GError ** error)
{
  g_set_error (error,
      FRIDA_ERROR,
      FRIDA_ERROR_NOT_SUPPORTED,
      "Not implemented");
}

FridaHostApplicationInfo *
frida_system_enumerate_applications (FridaApplicationQueryOptions * options, int * result_length)
{
  *result_length = 0;

  return NULL;
}

FridaHostProcessInfo *
frida_system_enumerate_processes (FridaProcessQueryOptions * options, int * result_length)
{
  FridaEnumerateProcessesOperation op;

  op.scope = frida_process_query_options_get_scope (options);
  op.ppid_by_pid = NULL;
  op.frontmost_pid = (op.scope != FRIDA_SCOPE_MINIMAL) ? frida_get_frontmost_pid () : 0;

  op.result = g_array_new (FALSE, FALSE, sizeof (FridaHostProcessInfo));

  if (frida_process_query_options_has_selected_pids (options))
  {
    frida_process_query_options_enumerate_selected_pids (options, (GFunc) frida_collect_process_info, &op);
  }
  else
  {
    DWORD * pids = NULL;
    DWORD size = 64 * sizeof (DWORD);
    DWORD bytes_returned;
    guint i;

    do
    {
      size *= 2;
      pids = g_realloc (pids, size);
      if (!EnumProcesses (pids, size, &bytes_returned))
        bytes_returned = 0;
    }
    while (bytes_returned == size);

    for (i = 0; i != bytes_returned / sizeof (DWORD); i++)
      frida_collect_process_info (pids[i], &op);

    g_free (pids);
  }

  g_clear_pointer (&op.ppid_by_pid, g_hash_table_unref);

  *result_length = op.result->len;

  return (FridaHostProcessInfo *) g_array_free (op.result, FALSE);
}

static void
frida_collect_process_info (guint pid, FridaEnumerateProcessesOperation * op)
{
  FridaHostProcessInfo info = { 0, };
  gboolean still_alive = TRUE;
  HANDLE handle;
  WCHAR program_path_utf16[MAX_PATH];
  gchar * program_path = NULL;

  handle = OpenProcess (PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (handle == NULL)
    return;

  if (!frida_get_process_filename (handle, program_path_utf16, G_N_ELEMENTS (program_path_utf16)))
    goto beach;

  program_path = g_utf16_to_utf8 (program_path_utf16, -1, NULL, NULL, NULL);

  info.pid = pid;
  info.name = g_path_get_basename (program_path);

  info.parameters = frida_make_parameters_dict ();

  if (op->scope != FRIDA_SCOPE_MINIMAL)
  {
    GVariant * argv;

    g_hash_table_insert (info.parameters, g_strdup ("path"),
        g_variant_ref_sink (g_variant_new_take_string (g_steal_pointer (&program_path))));

    still_alive = frida_add_process_metadata (info.parameters, pid, handle, op);

    if (pid == op->frontmost_pid)
      g_hash_table_insert (info.parameters, g_strdup ("frontmost"), g_variant_ref_sink (g_variant_new_boolean (TRUE)));

    argv = frida_query_process_argv (pid);
    if (argv != NULL)
      g_hash_table_insert (info.parameters, g_strdup ("argv"), g_variant_ref_sink (argv));
  }

  if (op->scope == FRIDA_SCOPE_FULL)
  {
    GVariantBuilder builder;
    GVariant * small_icon, * large_icon;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    small_icon = _frida_icon_from_process_or_file (pid, program_path_utf16, FRIDA_ICON_SMALL);
    if (small_icon != NULL)
    {
      g_variant_builder_add_value (&builder, small_icon);
      g_variant_unref (small_icon);
    }

    large_icon = _frida_icon_from_process_or_file (pid, program_path_utf16, FRIDA_ICON_LARGE);
    if (large_icon != NULL)
    {
      g_variant_builder_add_value (&builder, large_icon);
      g_variant_unref (large_icon);
    }

    g_hash_table_insert (info.parameters, g_strdup ("icons"), g_variant_ref_sink (g_variant_builder_end (&builder)));

    still_alive = small_icon != NULL && large_icon != NULL;
  }

  if (still_alive)
    g_array_append_val (op->result, info);
  else
    frida_host_process_info_destroy (&info);

beach:
  g_free (program_path);
  CloseHandle (handle);
}

void
frida_system_kill (guint pid)
{
  HANDLE process;

  process = OpenProcess (PROCESS_TERMINATE, FALSE, pid);
  if (process != NULL)
  {
    TerminateProcess (process, 0xdeadbeef);
    CloseHandle (process);
  }
}

gchar *
frida_temporary_directory_get_system_tmp (void)
{
  return g_strdup (g_get_tmp_dir ());
}

static gboolean
frida_add_process_metadata (GHashTable * parameters, guint pid, HANDLE process, FridaEnumerateProcessesOperation * op)
{
  GVariant * user;
  guint ppid;
  GVariant * started;

  user = frida_get_process_user (process);
  if (user == NULL)
    return FALSE;
  g_hash_table_insert (parameters, g_strdup ("user"), g_variant_ref_sink (user));

  if (op->ppid_by_pid == NULL)
    op->ppid_by_pid = frida_build_ppid_table ();
  ppid = GPOINTER_TO_UINT (g_hash_table_lookup (op->ppid_by_pid, GUINT_TO_POINTER (pid)));
  if (ppid == 0)
    return FALSE;
  g_hash_table_insert (parameters, g_strdup ("ppid"), g_variant_ref_sink (g_variant_new_int64 (ppid)));

  started = frida_get_process_start_time (process);
  if (started == NULL)
    return FALSE;
  g_hash_table_insert (parameters, g_strdup ("started"), g_variant_ref_sink (started));

  return TRUE;
}

static gboolean
frida_get_process_filename (HANDLE process, WCHAR * name, DWORD name_capacity)
{
  gsize name_length;
  WCHAR drive_strings[DRIVE_STRINGS_MAX_LENGTH];
  WCHAR *drive;

  if (GetProcessImageFileNameW (process, name, name_capacity) == 0)
    return FALSE;
  name_length = wcslen (name);

  drive_strings[0] = L'\0';
  drive_strings[DRIVE_STRINGS_MAX_LENGTH - 1] = L'\0';
  GetLogicalDriveStringsW (DRIVE_STRINGS_MAX_LENGTH - 1, drive_strings);
  for (drive = drive_strings; *drive != '\0'; drive += wcslen (drive) + 1)
  {
    WCHAR device_name[3];
    WCHAR mapping_strings[MAX_PATH];
    WCHAR * mapping;
    gsize mapping_length;

    wcsncpy (device_name, drive, 2);
    device_name[2] = L'\0';

    mapping_strings[0] = '\0';
    mapping_strings[MAX_PATH - 1] = '\0';
    QueryDosDeviceW (device_name, mapping_strings, MAX_PATH - 1);
    for (mapping = mapping_strings; *mapping != '\0'; mapping += mapping_length + 1)
    {
      mapping_length = wcslen (mapping);

      if (mapping_length > name_length)
        continue;

      if (wcsncmp (name, mapping, mapping_length) == 0)
      {
        wcsncpy (name, device_name, 2);
        memmove (name + 2, name + mapping_length, (name_length - mapping_length + 1) * sizeof (WCHAR));
        return TRUE;
      }
    }
  }

  return FALSE;
}

static GVariant *
frida_query_process_argv (guint pid)
{
  GVariant * result = NULL;
  HANDLE process;
  HMODULE ntdll;
  NTSTATUS (NTAPI * query_information_process) (HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
  PROCESS_BASIC_INFORMATION basic_info;
  PEB peb;
  RTL_USER_PROCESS_PARAMETERS parameters;
  WCHAR * command_line = NULL;
  WCHAR ** argv = NULL;
  int argc, i;
  GVariantBuilder builder;

  process = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (process == NULL)
    goto beach;

  ntdll = GetModuleHandleW (L"ntdll.dll");
  query_information_process = (gpointer) GetProcAddress (ntdll, "NtQueryInformationProcess");

  if (query_information_process (process, ProcessBasicInformation, &basic_info, sizeof (basic_info), NULL) != 0)
    goto beach;

  if (!ReadProcessMemory (process, basic_info.PebBaseAddress, &peb, sizeof (peb), NULL))
    goto beach;

  if (!ReadProcessMemory (process, peb.ProcessParameters, &parameters, sizeof (parameters), NULL))
    goto beach;

  command_line = g_malloc (parameters.CommandLine.Length + sizeof (WCHAR));
  if (!ReadProcessMemory (process, parameters.CommandLine.Buffer, command_line, parameters.CommandLine.Length, NULL))
    goto beach;
  command_line[parameters.CommandLine.Length / sizeof (WCHAR)] = L'\0';

  argv = CommandLineToArgvW (command_line, &argc);
  if (argv == NULL)
    goto beach;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  for (i = 0; i != argc; i++)
    g_variant_builder_add_value (&builder, g_variant_new_take_string (g_utf16_to_utf8 (argv[i], -1, NULL, NULL, NULL)));
  result = g_variant_builder_end (&builder);

beach:
  if (argv != NULL)
    LocalFree (argv);
  g_free (command_line);
  if (process != NULL)
    CloseHandle (process);

  return result;
}

static GVariant *
frida_get_process_user (HANDLE process)
{
  GVariant * result = NULL;
  HANDLE token;
  TOKEN_USER * user = NULL;
  DWORD user_size;
  WCHAR * name = NULL;
  DWORD name_length;
  WCHAR * domain_name = NULL;
  DWORD domain_name_length;
  SID_NAME_USE name_use;

  if (!OpenProcessToken (process, TOKEN_QUERY, &token))
    return NULL;

  user_size = 64;
  user = g_malloc (user_size);

  if (!GetTokenInformation (token, TokenUser, user, user_size, &user_size))
  {
    if (GetLastError () != ERROR_INSUFFICIENT_BUFFER)
      goto beach;

    user = g_realloc (user, user_size);

    if (!GetTokenInformation (token, TokenUser, user, user_size, &user_size))
      goto beach;
  }

  name_length = 64;
  name = g_malloc (name_length * sizeof (WCHAR));

  domain_name_length = 64;
  domain_name = g_malloc (domain_name_length * sizeof (WCHAR));

  if (!LookupAccountSidW (NULL, user->User.Sid, name, &name_length, domain_name, &domain_name_length, &name_use))
  {
    if (GetLastError () != ERROR_INSUFFICIENT_BUFFER)
      goto beach;

    name = g_realloc (name, name_length * sizeof (WCHAR));
    domain_name = g_realloc (domain_name, domain_name_length * sizeof (WCHAR));

    if (!LookupAccountSidW (NULL, user->User.Sid, name, &name_length, domain_name, &domain_name_length, &name_use))
      goto beach;
  }

  result = g_variant_new_take_string (g_utf16_to_utf8 (name, -1, NULL, NULL, NULL));

beach:
  g_free (domain_name);
  g_free (name);
  g_free (user);
  CloseHandle (token);

  return result;
}

static GVariant *
frida_get_process_start_time (HANDLE process)
{
  GVariant * result;
  FILETIME creation_time, exit_time, kernel_time, user_time;
  GDateTime * creation_dt;

  if (!GetProcessTimes (process, &creation_time, &exit_time, &kernel_time, &user_time))
    return NULL;

  creation_dt = frida_parse_filetime (&creation_time);
  result = g_variant_new_take_string (g_date_time_format_iso8601 (creation_dt));
  g_date_time_unref (creation_dt);

  return result;
}

static GHashTable *
frida_build_ppid_table (void)
{
  GHashTable * result = NULL;
  HANDLE snapshot;
  PROCESSENTRY32 entry;

  snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    goto beach;

  entry.dwSize = sizeof (entry);

  if (!Process32First (snapshot, &entry))
    goto beach;

  result = g_hash_table_new (NULL, NULL);

  do
  {
    g_hash_table_insert (result, GUINT_TO_POINTER (entry.th32ProcessID), GUINT_TO_POINTER (entry.th32ParentProcessID));
  }
  while (Process32Next (snapshot, &entry));

beach:
  if (snapshot != INVALID_HANDLE_VALUE)
    CloseHandle (snapshot);

  return result;
}

static guint
frida_get_frontmost_pid (void)
{
  DWORD pid;
  HWND window;

  window = GetForegroundWindow ();
  if (window == NULL)
    return 0;

  pid = 0;
  GetWindowThreadProcessId (window, &pid);

  return pid;
}

static GDateTime *
frida_parse_filetime (const FILETIME * ft)
{
  GDateTime * result;
  gint64 unix_time, unix_sec, unix_usec;
  GDateTime * dt;

  unix_time = frida_filetime_to_unix (ft);

  unix_sec = unix_time / G_USEC_PER_SEC;
  unix_usec = unix_time % G_USEC_PER_SEC;

  dt = g_date_time_new_from_unix_utc (unix_sec);
  result = g_date_time_add (dt, unix_usec);
  g_date_time_unref (dt);

  return result;
}

static gint64
frida_filetime_to_unix (const FILETIME * ft)
{
  ULARGE_INTEGER u;

  u.LowPart = ft->dwLowDateTime;
  u.HighPart = ft->dwHighDateTime;

  return (u.QuadPart - G_GUINT64_CONSTANT (116444736000000000)) / 10;
}
