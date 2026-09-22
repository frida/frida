#include "frida-core.h"

#include "../windows/icon-helpers.h"

#include <devguid.h>
#include <iphlpapi.h>
#include <setupapi.h>
#include <windns.h>

typedef struct _FridaPairingBrowserBackend FridaPairingBrowserBackend;

typedef struct _FridaMobileDeviceInfo FridaMobileDeviceInfo;
typedef struct _FridaImageDeviceInfo FridaImageDeviceInfo;

typedef struct _FridaFindMobileDeviceContext FridaFindMobileDeviceContext;
typedef struct _FridaFindImageDeviceContext FridaFindImageDeviceContext;

typedef struct _FridaDeviceInfo FridaDeviceInfo;

typedef struct _FridaDnsApi FridaDnsApi;

typedef gboolean (* FridaEnumerateDeviceFunc) (const FridaDeviceInfo * device_info, gpointer user_data);

struct _FridaPairingBrowserBackend
{
  FridaFruityWindowsPairingBrowserResultCallback callback;
  gpointer callback_target;

  DNS_SERVICE_CANCEL browse_handle;
};

struct _FridaMobileDeviceInfo
{
  WCHAR * location;
};

struct _FridaImageDeviceInfo
{
  WCHAR * friendly_name;
  WCHAR * icon_url;
};

struct _FridaFindMobileDeviceContext
{
  const WCHAR * udid;
  FridaMobileDeviceInfo * mobile_device;
};

struct _FridaFindImageDeviceContext
{
  const WCHAR * location;
  FridaImageDeviceInfo * image_device;
};

struct _FridaDeviceInfo
{
  WCHAR * device_path;
  WCHAR * instance_id;
  WCHAR * friendly_name;
  WCHAR * location;

  HDEVINFO device_info_set;
  PSP_DEVINFO_DATA device_info_data;
};

struct _FridaDnsApi
{
  DNS_STATUS (WINAPI * browse) (DNS_SERVICE_BROWSE_REQUEST * request, DNS_SERVICE_CANCEL * cancel);
  DNS_STATUS (WINAPI * browse_cancel) (DNS_SERVICE_CANCEL * cancel_handle);
};

static void WINAPI frida_fruity_windows_pairing_browser_on_browse_results (void * query_context, DNS_QUERY_RESULT * query_results);

static FridaMobileDeviceInfo * find_mobile_device_by_udid (const WCHAR * udid);
static FridaImageDeviceInfo * find_image_device_by_location (const WCHAR * location);

static gboolean compare_udid_and_create_mobile_device_info_if_matching (const FridaDeviceInfo * device_info, gpointer user_data);
static gboolean compare_location_and_create_image_device_info_if_matching (const FridaDeviceInfo * device_info, gpointer user_data);

FridaMobileDeviceInfo * frida_mobile_device_info_new (WCHAR * location);
void frida_mobile_device_info_free (FridaMobileDeviceInfo * mdev);

FridaImageDeviceInfo * frida_image_device_info_new (WCHAR * friendly_name, WCHAR * icon_url);
void frida_image_device_info_free (FridaImageDeviceInfo * idev);

static void frida_foreach_usb_device (const GUID * guid, FridaEnumerateDeviceFunc func, gpointer user_data);

static WCHAR * frida_read_device_registry_string_property (HANDLE info_set, SP_DEVINFO_DATA * info_data, DWORD prop_id);
static WCHAR * frida_read_registry_string (HKEY key, WCHAR * value_name);
static WCHAR * frida_read_registry_multi_string (HKEY key, WCHAR * value_name);
static gpointer frida_read_registry_value (HKEY key, WCHAR * value_name, DWORD expected_type);

static FridaDnsApi * frida_try_get_dns_api (void);

static GUID GUID_APPLE_USB = { 0xF0B32BE3, 0x6678, 0x4879, { 0x92, 0x30, 0x0E4, 0x38, 0x45, 0xD8, 0x05, 0xEE } };

void
_frida_fruity_windows_pairing_browser_enumerate_network_interfaces (FridaFruityWindowsPairingBrowserNetifFoundFunc func,
    gpointer func_target)
{
  IP_ADAPTER_ADDRESSES * adapters;
  ULONG buffer_size, result;
  IP_ADAPTER_ADDRESSES_LH * adapter;

  if (frida_try_get_dns_api () == NULL)
    return;

  buffer_size = 32768;
  adapters = g_malloc (buffer_size);

  do
  {
    result = GetAdaptersAddresses (AF_INET6, 0, NULL, adapters, &buffer_size);
    if (result != ERROR_BUFFER_OVERFLOW)
      break;
    adapters = g_realloc (adapters, buffer_size);
  }
  while (result == ERROR_BUFFER_OVERFLOW);
  if (result != ERROR_SUCCESS)
    goto beach;

  for (adapter = adapters; adapter != NULL; adapter = adapter->Next)
  {
    SOCKET_ADDRESS * raw_addr;
    GInetSocketAddress * addr;

    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;

    if (adapter->FirstUnicastAddress == NULL)
      continue;

    raw_addr = &adapter->FirstUnicastAddress->Address;
    addr = G_INET_SOCKET_ADDRESS (g_socket_address_new_from_native (raw_addr->lpSockaddr, raw_addr->iSockaddrLength));

    func (adapter->IfIndex, adapter->AdapterName, addr, func_target);
  }

beach:
  g_free (adapters);
}

void *
_frida_fruity_windows_pairing_browser_monitor_create_backend (gulong interface_index,
    FridaFruityWindowsPairingBrowserResultCallback callback, gpointer callback_target)
{
  FridaPairingBrowserBackend * backend;
  FridaDnsApi * api;
  DNS_SERVICE_BROWSE_REQUEST r = { 0, };

  api = frida_try_get_dns_api ();
  if (api == NULL)
    return NULL;

  backend = g_slice_new0 (FridaPairingBrowserBackend);
  backend->callback = callback;
  backend->callback_target = callback_target;

  r.Version = DNS_QUERY_REQUEST_VERSION2;
  r.InterfaceIndex = interface_index;
  r.QueryName = G_PASTE (L, FRIDA_FRUITY_PAIRING_SERVICE_DNS_SD_NAME);
  r.pBrowseCallbackV2 = frida_fruity_windows_pairing_browser_on_browse_results;
  r.pQueryContext = backend;

  if (api->browse (&r, &backend->browse_handle) != DNS_REQUEST_PENDING)
  {
    g_slice_free (FridaPairingBrowserBackend, backend);
    return NULL;
  }

  return backend;
}

void
_frida_fruity_windows_pairing_browser_monitor_destroy_backend (void * opaque_backend)
{
  FridaPairingBrowserBackend * backend = opaque_backend;

  if (backend == NULL)
    return;

  frida_try_get_dns_api ()->browse_cancel (&backend->browse_handle);

  g_slice_free (FridaPairingBrowserBackend, backend);
}

static void WINAPI
frida_fruity_windows_pairing_browser_on_browse_results (void * query_context, DNS_QUERY_RESULT * query_results)
{
  FridaPairingBrowserBackend * backend = query_context;

  backend->callback (query_results, backend->callback_target);
}

void
_frida_fruity_usbmux_backend_extract_details_for_device (gint product_id, const char * udid, char ** name, GVariant ** icon,
    GError ** error)
{
  gboolean result = FALSE;
  GString * udid_plain;
  const gchar * cursor;
  WCHAR * udid_utf16 = NULL;
  FridaMobileDeviceInfo * mdev = NULL;
  FridaImageDeviceInfo * idev = NULL;
  GVariant * idev_icon = NULL;

  udid_plain = g_string_sized_new (40);
  for (cursor = udid; *cursor != '\0'; cursor++)
  {
    gchar ch = *cursor;
    if (ch != '-')
      g_string_append_c (udid_plain, ch);
  }

  udid_utf16 = (WCHAR *) g_utf8_to_utf16 (udid_plain->str, udid_plain->len, NULL, NULL, NULL);

  mdev = find_mobile_device_by_udid (udid_utf16);
  if (mdev == NULL)
    goto beach;

  idev = find_image_device_by_location (mdev->location);
  if (idev != NULL)
  {
    idev_icon = _frida_icon_from_resource_url (idev->icon_url, FRIDA_ICON_SMALL);
  }

  if (idev_icon != NULL)
  {
    *name = g_utf16_to_utf8 ((gunichar2 *) idev->friendly_name, -1, NULL, NULL, NULL);
    *icon = idev_icon;
  }
  else
  {
    /* TODO: grab metadata from iTunes instead of relying on having an image device */
    *name = g_strdup ("iOS Device");
    *icon = NULL;
  }
  result = TRUE;

beach:
  if (!result)
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unable to extract details for device by UDID '%s'", udid);
  }

  frida_image_device_info_free (idev);
  frida_mobile_device_info_free (mdev);
  g_free (udid_utf16);
  g_string_free (udid_plain, TRUE);
}

static FridaMobileDeviceInfo *
find_mobile_device_by_udid (const WCHAR * udid)
{
  FridaFindMobileDeviceContext ctx;

  ctx.udid = udid;
  ctx.mobile_device = NULL;

  frida_foreach_usb_device (&GUID_APPLE_USB, compare_udid_and_create_mobile_device_info_if_matching, &ctx);

  return ctx.mobile_device;
}

static FridaImageDeviceInfo *
find_image_device_by_location (const WCHAR * location)
{
  FridaFindImageDeviceContext ctx;

  ctx.location = location;
  ctx.image_device = NULL;

  frida_foreach_usb_device (&GUID_DEVCLASS_IMAGE, compare_location_and_create_image_device_info_if_matching, &ctx);

  return ctx.image_device;
}

static gboolean
compare_udid_and_create_mobile_device_info_if_matching (const FridaDeviceInfo * device_info, gpointer user_data)
{
  FridaFindMobileDeviceContext * ctx = (FridaFindMobileDeviceContext *) user_data;
  WCHAR * udid, * location;
  size_t udid_len;

  udid = wcsrchr (device_info->instance_id, L'\\');
  if (udid == NULL)
    goto try_device_path;
  udid++;

  if (_wcsicmp (udid, ctx->udid) == 0)
    goto match;

try_device_path:
  udid = device_info->device_path;
  if (udid == NULL)
    goto keep_looking;

  udid_len = wcslen (ctx->udid);
  while (*udid != L'\0')
  {
    if (_wcsnicmp (udid, ctx->udid, udid_len) == 0)
      goto match;
    udid++;
  }

  goto keep_looking;

match:
  location = (WCHAR *) g_memdup2 (device_info->location, ((guint) wcslen (device_info->location) + 1) * sizeof (WCHAR));
  ctx->mobile_device = frida_mobile_device_info_new (location);

  return FALSE;

keep_looking:
  return TRUE;
}

static gboolean
compare_location_and_create_image_device_info_if_matching (const FridaDeviceInfo * device_info, gpointer user_data)
{
  FridaFindImageDeviceContext * ctx = (FridaFindImageDeviceContext *) user_data;
  HKEY devkey = (HKEY) INVALID_HANDLE_VALUE;
  WCHAR * friendly_name = NULL;
  WCHAR * icon_url = NULL;

  if (_wcsicmp (device_info->location, ctx->location) != 0)
    goto keep_looking;

  devkey = SetupDiOpenDevRegKey (device_info->device_info_set, device_info->device_info_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
  if (devkey == INVALID_HANDLE_VALUE)
    goto keep_looking;

  friendly_name = frida_read_registry_string (devkey, L"FriendlyName");
  if (friendly_name == NULL)
  {
    friendly_name = frida_read_registry_string (devkey, L"Label");
    if (friendly_name == NULL)
      goto keep_looking;
  }

  icon_url = frida_read_registry_multi_string (devkey, L"Icons");
  if (icon_url == NULL)
    goto keep_looking;

  ctx->image_device = frida_image_device_info_new (friendly_name, icon_url);

  RegCloseKey (devkey);
  return FALSE;

keep_looking:
  g_free (icon_url);
  g_free (friendly_name);
  if (devkey != INVALID_HANDLE_VALUE)
    RegCloseKey (devkey);
  return TRUE;
}

FridaMobileDeviceInfo *
frida_mobile_device_info_new (WCHAR * location)
{
  FridaMobileDeviceInfo * mdev;

  mdev = g_new (FridaMobileDeviceInfo, 1);
  mdev->location = location;

  return mdev;
}

void
frida_mobile_device_info_free (FridaMobileDeviceInfo * mdev)
{
  if (mdev == NULL)
    return;

  g_free (mdev->location);
  g_free (mdev);
}

FridaImageDeviceInfo *
frida_image_device_info_new (WCHAR * friendly_name, WCHAR * icon_url)
{
  FridaImageDeviceInfo * idev;

  idev = g_new (FridaImageDeviceInfo, 1);
  idev->friendly_name = friendly_name;
  idev->icon_url = icon_url;

  return idev;
}

void
frida_image_device_info_free (FridaImageDeviceInfo * idev)
{
  if (idev == NULL)
    return;

  g_free (idev->icon_url);
  g_free (idev->friendly_name);
  g_free (idev);
}

static void
frida_foreach_usb_device (const GUID * guid, FridaEnumerateDeviceFunc func, gpointer user_data)
{
  HANDLE info_set;
  gboolean carry_on = TRUE;
  guint member_index;

  info_set = SetupDiGetClassDevsW (guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  if (info_set == INVALID_HANDLE_VALUE)
    goto beach;

  for (member_index = 0; carry_on; member_index++)
  {
    SP_DEVICE_INTERFACE_DATA iface_data = { 0, };
    SP_DEVINFO_DATA info_data = { 0, };
    DWORD detail_size;
    SP_DEVICE_INTERFACE_DETAIL_DATA_W * detail_data = NULL;
    BOOL success;
    FridaDeviceInfo device_info = { 0, };
    DWORD instance_id_size;

    iface_data.cbSize = sizeof (iface_data);
    if (!SetupDiEnumDeviceInterfaces (info_set, NULL, guid, member_index, &iface_data))
      break;

    info_data.cbSize = sizeof (info_data);
    success = SetupDiGetDeviceInterfaceDetailW (info_set, &iface_data, NULL, 0, &detail_size, &info_data);
    if (!success && GetLastError () != ERROR_INSUFFICIENT_BUFFER)
      goto skip_device;

    detail_data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *) g_malloc (detail_size);
    detail_data->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    success = SetupDiGetDeviceInterfaceDetailW (info_set, &iface_data, detail_data, detail_size, NULL, &info_data);
    if (!success)
      goto skip_device;

    device_info.device_path = detail_data->DevicePath;

    success = SetupDiGetDeviceInstanceIdW (info_set, &info_data, NULL, 0, &instance_id_size);
    if (!success && GetLastError () != ERROR_INSUFFICIENT_BUFFER)
      goto skip_device;

    device_info.instance_id = (WCHAR *) g_malloc (instance_id_size * sizeof (WCHAR));
    success = SetupDiGetDeviceInstanceIdW (info_set, &info_data, device_info.instance_id, instance_id_size, NULL);
    if (!success)
      goto skip_device;

    device_info.friendly_name = frida_read_device_registry_string_property (info_set, &info_data, SPDRP_FRIENDLYNAME);

    device_info.location = frida_read_device_registry_string_property (info_set, &info_data, SPDRP_LOCATION_INFORMATION);
    if (device_info.location == NULL)
      goto skip_device;

    device_info.device_info_set = info_set;
    device_info.device_info_data = &info_data;

    carry_on = func (&device_info, user_data);

skip_device:
    g_free (device_info.location);
    g_free (device_info.friendly_name);
    g_free (device_info.instance_id);

    g_free (detail_data);
  }

beach:
  if (info_set != INVALID_HANDLE_VALUE)
    SetupDiDestroyDeviceInfoList (info_set);
}

static WCHAR *
frida_read_device_registry_string_property (HANDLE info_set, SP_DEVINFO_DATA * info_data, DWORD prop_id)
{
  gboolean success = FALSE;
  WCHAR * value_buffer = NULL;
  DWORD value_size;
  BOOL ret;

  ret = SetupDiGetDeviceRegistryPropertyW (info_set, info_data, prop_id, NULL, NULL, 0, &value_size);
  if (!ret && GetLastError () != ERROR_INSUFFICIENT_BUFFER)
    goto beach;

  value_buffer = (WCHAR *) g_malloc (value_size);
  if (!SetupDiGetDeviceRegistryPropertyW (info_set, info_data, prop_id, NULL, (PBYTE) value_buffer, value_size, NULL))
    goto beach;

  success = TRUE;

beach:
  if (!success)
  {
    g_free (value_buffer);
    value_buffer = NULL;
  }

  return value_buffer;
}

static WCHAR *
frida_read_registry_string (HKEY key, WCHAR * value_name)
{
  return (WCHAR *) frida_read_registry_value (key, value_name, REG_SZ);
}

static WCHAR *
frida_read_registry_multi_string (HKEY key, WCHAR * value_name)
{
  return (WCHAR *) frida_read_registry_value (key, value_name, REG_MULTI_SZ);
}

static gpointer
frida_read_registry_value (HKEY key, WCHAR * value_name, DWORD expected_type)
{
  gboolean success = FALSE;
  DWORD type;
  WCHAR * buffer = NULL;
  DWORD base_size = 0, real_size;
  LONG ret;

  ret = RegQueryValueExW (key, value_name, NULL, &type, NULL, &base_size);
  if (ret != ERROR_SUCCESS || type != expected_type)
    goto beach;

  if (type == REG_SZ)
    real_size = base_size + sizeof (WCHAR);
  else if (type == REG_MULTI_SZ)
    real_size = base_size + 2 * sizeof (WCHAR);
  else
    real_size = base_size;
  buffer = (WCHAR *) g_malloc0 (real_size);
  ret = RegQueryValueExW (key, value_name, NULL, &type, (LPBYTE) buffer, &base_size);
  if (ret != ERROR_SUCCESS || type != expected_type)
    goto beach;

  success = TRUE;

beach:
  if (!success)
  {
    g_free (buffer);
    buffer = NULL;
  }

  return buffer;
}

static FridaDnsApi *
frida_try_get_dns_api (void)
{
  static gsize api_value = 0;

  if (g_once_init_enter (&api_value))
  {
    HMODULE mod;
    FARPROC browse;
    FridaDnsApi * api = NULL;

    mod = GetModuleHandleW (L"dnsapi.dll");

    browse = GetProcAddress (mod, "DnsServiceBrowse");
    if (browse != NULL)
    {
      api = g_slice_new (FridaDnsApi);
      api->browse = (gpointer) browse;
      api->browse_cancel = (gpointer) GetProcAddress (mod, "DnsServiceBrowseCancel");
    }

    g_once_init_leave (&api_value, GPOINTER_TO_SIZE (api) + 1);
  }

  return GSIZE_TO_POINTER (api_value - 1);
}
