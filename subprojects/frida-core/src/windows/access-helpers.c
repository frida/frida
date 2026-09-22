#include "access-helpers.h"

static BOOL frida_access_is_windows_8_or_greater (void);
static BOOL frida_access_is_windows_version_or_greater (DWORD major, DWORD minor, DWORD service_pack);

LPCWSTR
frida_access_get_sddl_string_for_temp_directory (void)
{
  #define DACL_START_INHERIT L"D:AI"
  #define DACL_ACE_APPCONTAINER_RWX_WITH_CHILD_INHERIT L"(A;OICI;GRGWGX;;;AC)"
  #define DACL_ACE_SYSTEM_RWX_WITH_CHILD_INHERIT        L"(A;OICI;GA;;;SY)"
  #define DACL_ACE_ADMINS_RWX_WITH_CHILD_INHERIT        L"(A;OICI;GA;;;BA)"
  #define DACL_ACE_LOCAL_SERVICE_RX_WITH_CHILD_INHERIT   L"(A;OICI;GRGX;;;LS)"

  if (frida_access_is_windows_8_or_greater ())
  {
    return DACL_START_INHERIT
        DACL_ACE_APPCONTAINER_RWX_WITH_CHILD_INHERIT
        DACL_ACE_SYSTEM_RWX_WITH_CHILD_INHERIT
        DACL_ACE_ADMINS_RWX_WITH_CHILD_INHERIT
        DACL_ACE_LOCAL_SERVICE_RX_WITH_CHILD_INHERIT;
  }
  else
  {
    return NULL;
  }
}

static BOOL
frida_access_is_windows_8_or_greater (void)
{
  return frida_access_is_windows_version_or_greater (6, 2, 0);
}

static BOOL
frida_access_is_windows_version_or_greater (DWORD major, DWORD minor, DWORD service_pack)
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
