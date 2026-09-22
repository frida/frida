#include "frida-core.h"

#include "access-helpers.h"

#define VC_EXTRALEAN
#include <aclapi.h>
#include <sddl.h>
#include <windows.h>

#define CHECK_WINAPI_RESULT(n1, cmp, n2, op) \
  if (!(n1 cmp n2)) \
  { \
    failed_operation = op; \
    goto winapi_failure; \
  }

void
frida_winjector_set_acls_as_needed (const gchar * path, GError ** error)
{
  const gchar * failed_operation;
  LPWSTR path_utf16;
  LPCWSTR sddl;
  SECURITY_DESCRIPTOR * sd = NULL;
  BOOL dacl_present;
  BOOL dacl_defaulted;
  PACL dacl;

  path_utf16 = (WCHAR *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
  sddl = frida_access_get_sddl_string_for_temp_directory ();

  if (sddl != NULL)
  {
    DWORD success = ConvertStringSecurityDescriptorToSecurityDescriptorW (sddl, SDDL_REVISION_1, (PSECURITY_DESCRIPTOR *) &sd, NULL);
    CHECK_WINAPI_RESULT (success, !=, FALSE, "ConvertStringSecurityDescriptorToSecurityDescriptor");

    dacl_present = FALSE;
    dacl_defaulted = FALSE;
    success = GetSecurityDescriptorDacl (sd, &dacl_present, &dacl, &dacl_defaulted);
    CHECK_WINAPI_RESULT (success, !=, FALSE, "GetSecurityDescriptorDacl");

    success = SetNamedSecurityInfoW (path_utf16, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, dacl, NULL);
    CHECK_WINAPI_RESULT (success, ==, ERROR_SUCCESS, "SetNamedSecurityInfo");
  }

  goto beach;

winapi_failure:
  {
    DWORD last_error = GetLastError ();
    g_set_error (error,
        G_IO_ERROR,
        g_io_error_from_win32_error (last_error),
        "Error setting ACLs (%s returned 0x%08lx)",
        failed_operation, last_error);
    goto beach;
  }

beach:
  {
    if (sd != NULL)
      LocalFree (sd);

    g_free (path_utf16);
  }
}
