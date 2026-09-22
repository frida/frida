#define VC_EXTRALEAN

#include <stdio.h>
#include <string.h>
#include <windows.h>

static int spawn_child (const wchar_t * program, const wchar_t * method);

int
wmain (int argc, wchar_t * argv[])
{
  const wchar_t * operation;
  int result;

  if (argc < 3)
    goto missing_argument;

  operation = argv[1];

  if (wcscmp (operation, L"spawn") == 0)
  {
    const wchar_t * good_path = argv[0];
    const wchar_t * method = argv[2];

    result = spawn_child (good_path, method);
  }
  else if (wcscmp (operation, L"spawn-bad-path") == 0)
  {
    size_t argv0_length, bad_path_size;
    wchar_t * bad_path;
    const wchar_t * method;

    argv0_length = wcslen (argv[0]);

    bad_path_size = (argv0_length + 15 + 1) * sizeof (wchar_t);
    bad_path = malloc (bad_path_size);
    swprintf_s (bad_path, bad_path_size, L"%.*s-does-not-exist.exe", (int) (argv0_length - 4), argv[0]);

    method = argv[2];

    result = spawn_child (bad_path, method);

    free (bad_path);
  }
  else if (wcscmp (operation, L"say") == 0)
  {
    const wchar_t * message = argv[2];

    OutputDebugStringW (message);

    result = 0;
  }
  else
  {
    goto missing_argument;
  }

  return result;

missing_argument:
  {
    fputws (L"Missing argument", stderr);
    return 1;
  }
}

static int
spawn_child (const wchar_t * path, const wchar_t * method)
{
  size_t command_line_size;
  wchar_t * command_line;

  command_line_size = (1 + wcslen (path) + 1 + 1 + 3 + 1 + 1 + wcslen (method) + 1 + 1) * sizeof (wchar_t);
  command_line = malloc (command_line_size);
  swprintf_s (command_line, command_line_size, L"\"%s\" say \"%s\"", path, method);

  if (wcscmp (method, L"CreateProcess") == 0)
  {
    STARTUPINFO startup_info = { 0, };
    PROCESS_INFORMATION process_info;
    BOOL success;

    startup_info.cb = sizeof (startup_info);

    success = CreateProcessW (path, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &process_info);

    if (!success)
      goto create_process_failed;

    WaitForSingleObject (process_info.hProcess, INFINITE);

    CloseHandle (process_info.hProcess);
    CloseHandle (process_info.hThread);
  }
  else
  {
    goto missing_argument;
  }

  free (command_line);

  return 0;

missing_argument:
  {
    fputws (L"Missing argument", stderr);
    goto error_epilogue;
  }
create_process_failed:
  {
    wchar_t * reason;

    FormatMessageW (
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError (),
        MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &reason,
        0,
        NULL);
    fwprintf (stderr, L"CreateProcess(\n"
        L"\tpath='%s',\n"
        L"\tcommand_line='%s'\n"
        L") => %s",
        path, command_line, reason);
    LocalFree (reason);

    goto error_epilogue;
  }
error_epilogue:
  {
    free (command_line);

    return 1;
  }
}
