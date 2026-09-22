#include "frida-base.h"

#if defined (HAVE_WINDOWS)

#include <windows.h>

gchar *
_frida_query_windows_version (void)
{
  NTSTATUS (WINAPI * rtl_get_version) (PRTL_OSVERSIONINFOW info);
  RTL_OSVERSIONINFOW info = { 0, };

  rtl_get_version = (NTSTATUS (WINAPI *) (PRTL_OSVERSIONINFOW)) GetProcAddress (GetModuleHandleW (L"ntdll.dll"), "RtlGetVersion");

  info.dwOSVersionInfoSize = sizeof (info);
  rtl_get_version (&info);

  return g_strdup_printf ("%lu.%lu.%lu", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
}

gchar *
_frida_query_windows_computer_name (void)
{
  WCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1] = { 0, };
  DWORD buffer_size;

  buffer_size = G_N_ELEMENTS (buffer);
  GetComputerNameW (buffer, &buffer_size);

  return g_utf16_to_utf8 (buffer, -1, NULL, NULL, NULL);
}

#elif defined (HAVE_IOS) || defined (HAVE_TVOS) || defined (HAVE_XROS)

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

GVariant *
_frida_query_mobile_gestalt (const gchar * query)
{
  GVariant * result = NULL;
  static CFTypeRef (* mg_copy_answer) (CFStringRef query) = NULL;
  static CFStringRef (* cf_string_create_with_c_string) (CFAllocatorRef alloc, const char * str, CFStringEncoding encoding) = NULL;
  static Boolean (* cf_string_get_c_string) (CFStringRef str, char * buffer, CFIndex buffer_size, CFStringEncoding encoding) = NULL;
  static const char * (* cf_string_get_c_string_ptr) (CFStringRef str, CFStringEncoding encoding) = NULL;
  static CFIndex (* cf_string_get_length) (CFStringRef str) = NULL;
  static CFIndex (* cf_string_get_maximum_size_for_encoding) (CFIndex length, CFStringEncoding encoding) = NULL;
  static CFTypeID cf_string_type_id = 0;
  static CFTypeID (* cf_get_type_id) (CFTypeRef cf) = NULL;
  static void (* cf_release) (CFTypeRef cf) = NULL;
  CFStringRef query_value = NULL;
  CFTypeRef answer_value = NULL;
  CFTypeID answer_type;

  if (cf_release == NULL)
  {
    void * mg, * cf;
    CFTypeID (* cf_string_get_type_id) (void);

    mg = dlopen ("/usr/lib/libMobileGestalt.dylib", RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD);
    if (mg == NULL)
      goto beach;

    cf = dlopen ("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD);
    g_assert (cf != NULL);

    mg_copy_answer = dlsym (mg, "MGCopyAnswer");

    cf_string_create_with_c_string = dlsym (cf, "CFStringCreateWithCString");
    cf_string_get_c_string = dlsym (cf, "CFStringGetCString");
    cf_string_get_c_string_ptr = dlsym (cf, "CFStringGetCStringPtr");
    cf_string_get_length = dlsym (cf, "CFStringGetLength");
    cf_string_get_maximum_size_for_encoding = dlsym (cf, "CFStringGetMaximumSizeForEncoding");
    cf_string_get_type_id = dlsym (cf, "CFStringGetTypeID");
    cf_string_type_id = cf_string_get_type_id ();
    cf_get_type_id = dlsym (cf, "CFGetTypeID");
    cf_release = dlsym (cf, "CFRelease");

    dlclose (cf);
    dlclose (mg);
  }

  query_value = cf_string_create_with_c_string (NULL, query, kCFStringEncodingUTF8);

  answer_value = mg_copy_answer (query_value);
  if (answer_value == NULL)
    goto beach;

  answer_type = cf_get_type_id (answer_value);

  if (answer_type == cf_string_type_id)
  {
    const gchar * answer;

    answer = cf_string_get_c_string_ptr (answer_value, kCFStringEncodingUTF8);
    if (answer != NULL)
    {
      result = g_variant_new_string (answer);
    }
    else
    {
      gsize buffer_size;
      gchar * buffer;

      buffer_size = cf_string_get_maximum_size_for_encoding (cf_string_get_length (answer_value), kCFStringEncodingUTF8) + 1;
      buffer = g_malloc (buffer_size);

      if (cf_string_get_c_string (answer_value, buffer, buffer_size, kCFStringEncodingUTF8))
        result = g_variant_new_take_string (buffer);
      else
        g_free (buffer);
    }
  }

beach:
  g_clear_pointer (&answer_value, cf_release);
  g_clear_pointer (&query_value, cf_release);

  return (result != NULL) ? g_variant_ref_sink (result) : NULL;
}

#elif defined (HAVE_ANDROID)

#include <sys/system_properties.h>

gchar *
_frida_query_android_system_property (const gchar * name)
{
  gchar buffer[PROP_VALUE_MAX] = { 0, };

  __system_property_get (name, buffer);

  return g_strdup (buffer);
}

#endif
