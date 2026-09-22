#include "pipe-sddl.h"

static BOOL frida_pipe_is_windows_vista_or_greater (void);
static BOOL frida_pipe_is_windows_8_or_greater (void);
static BOOL frida_pipe_is_windows_version_or_greater (DWORD major, DWORD minor, DWORD service_pack);

LPCWSTR
frida_pipe_get_sddl_string_for_pipe (void)
{
  #define DACL_START_NOINHERIT L"D:PAI"
  #define DACL_ACE_APPCONTAINER_RW L"(A;;GRGW;;;AC)"
  #define DACL_ACE_EVERYONE_RW L"(A;;GRGW;;;WD)"
  #define SACL_START L"S:"
  #define SACL_ACE_LOWINTEGRITY_NORW L"(ML;;NWNR;;;LW)"
  #define DACL_ACE_SYSTEM_RW L"(A;;GRGW;;;SY)"
  #define DACL_ACE_ADMINS_RW L"(A;;GRGW;;;BA)"

  if (frida_pipe_is_windows_8_or_greater ())
  {
    return DACL_START_NOINHERIT
        DACL_ACE_APPCONTAINER_RW
        DACL_ACE_EVERYONE_RW
        DACL_ACE_SYSTEM_RW
        DACL_ACE_ADMINS_RW
        SACL_START
        SACL_ACE_LOWINTEGRITY_NORW;
  }
  else if (frida_pipe_is_windows_vista_or_greater ())
  {
    return DACL_START_NOINHERIT
        DACL_ACE_EVERYONE_RW
        DACL_ACE_SYSTEM_RW
        DACL_ACE_ADMINS_RW
        SACL_START
        SACL_ACE_LOWINTEGRITY_NORW;
  }
  else
  {
    return DACL_START_NOINHERIT DACL_ACE_EVERYONE_RW;
  }
}

static BOOL
frida_pipe_is_windows_vista_or_greater (void)
{
  return frida_pipe_is_windows_version_or_greater (6, 0, 0);
}

static BOOL
frida_pipe_is_windows_8_or_greater (void)
{
  return frida_pipe_is_windows_version_or_greater (6, 2, 0);
}

static BOOL
frida_pipe_is_windows_version_or_greater (DWORD major, DWORD minor, DWORD service_pack)
{
  OSVERSIONINFOEXW osvi;
  ULONGLONG condition_mask;

  ZeroMemory (&osvi, sizeof (osvi));
  osvi.dwOSVersionInfoSize = sizeof (osvi);

  condition_mask =
      VerSetConditionMask (
          VerSetConditionMask (
              VerSetConditionMask (0, VER_MAJORVERSION, VER_GREATER_EQUAL),
              VER_MINORVERSION, VER_GREATER_EQUAL),
          VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

  osvi.dwMajorVersion = major;
  osvi.dwMinorVersion = minor;
  osvi.wServicePackMajor = service_pack;

  return VerifyVersionInfoW (&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, condition_mask) != FALSE;
}
