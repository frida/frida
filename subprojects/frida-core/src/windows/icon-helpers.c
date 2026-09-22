#include "icon-helpers.h"

#include <psapi.h>
#include <shellapi.h>

typedef struct _FindMainWindowCtx FindMainWindowCtx;

typedef BOOL (WINAPI * Wow64DisableWow64FsRedirectionFunc) (PVOID * OldValue);
typedef BOOL (WINAPI * Wow64RevertWow64FsRedirectionFunc) (PVOID OldValue);

struct _FindMainWindowCtx
{
  DWORD pid;
  HWND main_window;
};

static HWND find_main_window_of_pid (DWORD pid);
static BOOL CALLBACK inspect_window (HWND hwnd, LPARAM lparam);

GVariant *
_frida_icon_from_process_or_file (DWORD pid, WCHAR * filename, FridaIconSize size)
{
  GVariant * icon;

  icon = _frida_icon_from_process (pid, size);
  if (icon == NULL)
    icon = _frida_icon_from_file (filename, size);

  return icon;
}

GVariant *
_frida_icon_from_process (DWORD pid, FridaIconSize size)
{
  GVariant * result = NULL;
  HICON icon = NULL;
  HWND main_window;

  main_window = find_main_window_of_pid (pid);
  if (main_window != NULL)
  {
    UINT flags, timeout;

    flags = SMTO_ABORTIFHUNG | SMTO_BLOCK;
    timeout = 100;

    if (size == FRIDA_ICON_SMALL)
    {
      SendMessageTimeout (main_window, WM_GETICON, ICON_SMALL2, 0,
          flags, timeout, (PDWORD_PTR) &icon);

      if (icon == NULL)
      {
        SendMessageTimeout (main_window, WM_GETICON, ICON_SMALL, 0,
            flags, timeout, (PDWORD_PTR) &icon);
      }

      if (icon == NULL)
        icon = (HICON) GetClassLongPtr (main_window, GCLP_HICONSM);
    }
    else if (size == FRIDA_ICON_LARGE)
    {
      SendMessageTimeout (main_window, WM_GETICON, ICON_BIG, 0,
          flags, timeout, (PDWORD_PTR) &icon);

      if (icon == NULL)
        icon = (HICON) GetClassLongPtr (main_window, GCLP_HICON);

      if (icon == NULL)
      {
        SendMessageTimeout (main_window, WM_QUERYDRAGICON, 0, 0,
            flags, timeout, (PDWORD_PTR) &icon);
      }
    }
    else
    {
      g_assert_not_reached ();
    }
  }

  if (icon != NULL)
    result = _frida_icon_from_native_icon_handle (icon, size);

  return result;
}

GVariant *
_frida_icon_from_file (WCHAR * filename, FridaIconSize size)
{
  GVariant * result = NULL;
  SHFILEINFOW shfi = { 0, };
  UINT flags;

  flags = SHGFI_ICON;
  if (size == FRIDA_ICON_SMALL)
    flags |= SHGFI_SMALLICON;
  else if (size == FRIDA_ICON_LARGE)
    flags |= SHGFI_LARGEICON;
  else
    g_assert_not_reached ();

  SHGetFileInfoW (filename, 0, &shfi, sizeof (shfi), flags);
  if (shfi.hIcon != NULL)
  {
    result = _frida_icon_from_native_icon_handle (shfi.hIcon, size);

    DestroyIcon (shfi.hIcon);
  }

  return result;
}

GVariant *
_frida_icon_from_resource_url (WCHAR * resource_url, FridaIconSize size)
{
  static gboolean api_initialized = FALSE;
  static Wow64DisableWow64FsRedirectionFunc Wow64DisableWow64FsRedirectionImpl = NULL;
  static Wow64RevertWow64FsRedirectionFunc Wow64RevertWow64FsRedirectionImpl = NULL;
  GVariant * result = NULL;
  WCHAR * resource_file = NULL;
  DWORD resource_file_length;
  WCHAR * p;
  gint resource_id;
  PVOID old_redirection_value = NULL;
  UINT ret;
  HICON icon = NULL;

  if (!api_initialized)
  {
    HMODULE kmod;

    kmod = GetModuleHandleW (L"kernel32.dll");
    g_assert (kmod != NULL);

    Wow64DisableWow64FsRedirectionImpl = (Wow64DisableWow64FsRedirectionFunc) GetProcAddress (kmod, "Wow64DisableWow64FsRedirection");
    Wow64RevertWow64FsRedirectionImpl = (Wow64RevertWow64FsRedirectionFunc) GetProcAddress (kmod, "Wow64RevertWow64FsRedirection");
    g_assert ((Wow64DisableWow64FsRedirectionImpl != NULL) == (Wow64RevertWow64FsRedirectionImpl != NULL));

    api_initialized = TRUE;
  }

  resource_file_length = ExpandEnvironmentStringsW (resource_url, NULL, 0);
  if (resource_file_length == 0)
    goto beach;
  resource_file = (WCHAR *) g_malloc ((resource_file_length + 1) * sizeof (WCHAR));
  if (ExpandEnvironmentStringsW (resource_url, resource_file, resource_file_length) == 0)
    goto beach;

  p = wcsrchr (resource_file, L',');
  if (p == NULL)
    goto beach;
  *p = L'\0';

  resource_id = wcstol (p + 1, NULL, 10);

  if (Wow64DisableWow64FsRedirectionImpl != NULL)
    Wow64DisableWow64FsRedirectionImpl (&old_redirection_value);

  ret = ExtractIconExW (resource_file, resource_id, (size == FRIDA_ICON_LARGE) ? &icon : NULL, (size == FRIDA_ICON_SMALL) ? &icon : NULL, 1);

  if (Wow64RevertWow64FsRedirectionImpl != NULL)
    Wow64RevertWow64FsRedirectionImpl (old_redirection_value);

  if (ret != 1)
    goto beach;

  result = _frida_icon_from_native_icon_handle (icon, size);

beach:
  if (icon != NULL)
    DestroyIcon (icon);
  g_free (resource_file);

  return result;
}

GVariant *
_frida_icon_from_native_icon_handle (HICON icon, FridaIconSize size)
{
  GVariant * result;
  HDC dc;
  gint width = -1, height = -1;
  BITMAPV5HEADER bi = { 0, };
  guint rowstride;
  guchar * data = NULL;
  HBITMAP bm;
  guint i;
  GVariantBuilder builder;

  dc = CreateCompatibleDC (NULL);

  if (size == FRIDA_ICON_SMALL)
  {
    width = GetSystemMetrics (SM_CXSMICON);
    height = GetSystemMetrics (SM_CYSMICON);
  }
  else if (size == FRIDA_ICON_LARGE)
  {
    width = GetSystemMetrics (SM_CXICON);
    height = GetSystemMetrics (SM_CYICON);
  }
  else
  {
    g_assert_not_reached ();
  }

  bi.bV5Size = sizeof (bi);
  bi.bV5Width = width;
  bi.bV5Height = -height;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask   = 0x00ff0000;
  bi.bV5GreenMask = 0x0000ff00;
  bi.bV5BlueMask  = 0x000000ff;
  bi.bV5AlphaMask = 0xff000000;

  rowstride = width * (bi.bV5BitCount / 8);

  bm = CreateDIBSection (dc, (BITMAPINFO *) &bi, DIB_RGB_COLORS, (void **) &data, NULL, 0);

  SelectObject (dc, bm);
  DrawIconEx (dc, 0, 0, icon, width, height, 0, NULL, DI_NORMAL);
  GdiFlush ();

  for (i = 0; i != rowstride * height; i += 4)
  {
    guchar hold;

    hold = data[i + 0];
    data[i + 0] = data[i + 2];
    data[i + 2] = hold;
  }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "format", g_variant_new_string ("rgba"));
  g_variant_builder_add (&builder, "{sv}", "width", g_variant_new_uint16 (width));
  g_variant_builder_add (&builder, "{sv}", "height", g_variant_new_uint16 (height));
  g_variant_builder_add (&builder, "{sv}", "image",
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, data, rowstride * height, sizeof (guint8)));
  result = g_variant_ref_sink (g_variant_builder_end (&builder));

  DeleteObject (bm);

  DeleteDC (dc);

  return result;
}

static HWND
find_main_window_of_pid (DWORD pid)
{
  FindMainWindowCtx ctx;

  ctx.pid = pid;
  ctx.main_window = NULL;

  EnumWindows (inspect_window, (LPARAM) &ctx);

  return ctx.main_window;
}

static BOOL CALLBACK
inspect_window (HWND hwnd, LPARAM lparam)
{
  if ((GetWindowLong (hwnd, GWL_STYLE) & WS_VISIBLE) != 0)
  {
    FindMainWindowCtx * ctx = (FindMainWindowCtx *) lparam;
    DWORD pid;

    GetWindowThreadProcessId (hwnd, &pid);
    if (pid == ctx->pid)
    {
      ctx->main_window = hwnd;
      return FALSE;
    }
  }

  return TRUE;
}
