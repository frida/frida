#ifndef __FRIDA_WINDOWS_ACCESS_HELPERS_H__
#define __FRIDA_WINDOWS_ACCESS_HELPERS_H__

#define VC_EXTRALEAN
#include <windows.h>
#undef VC_EXTRALEAN

LPCWSTR frida_access_get_sddl_string_for_temp_directory (void);

#endif
