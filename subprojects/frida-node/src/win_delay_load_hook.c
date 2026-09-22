/*
 * When this file is linked to a DLL, it sets up a delay-load hook that
 * intervenes when the DLL is trying to load the host executable
 * dynamically. Instead of trying to locate the .exe file it'll just
 * return a handle to the process image.
 *
 * This allows compiled addons to work when the host executable is renamed.
 */

#ifdef _MSC_VER

#pragma managed(push, off)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <delayimp.h>
#include <string.h>

static FARPROC WINAPI
load_exe_hook (unsigned int event,
               DelayLoadInfo * info)
{
  static HMODULE node_dll = NULL;

  switch (event)
  {
    case dliNoteStartProcessing:
      if (node_dll == NULL)
      {
        HMODULE m = GetModuleHandle (NULL);
        if (GetProcAddress (m, "napi_define_class") != NULL)
          node_dll = m;
        else
          node_dll = GetModuleHandle ("node.dll");
      }
      return NULL;
    case dliNotePreLoadLibrary:
      if (_stricmp (info->szDll, "node.exe") == 0)
        return (FARPROC) node_dll;
      return NULL;
    case dliNotePreGetProcAddress:
      return GetProcAddress (node_dll, info->dlp.szProcName);
    default:
      return NULL;
  }
}

const PfnDliHook __pfnDliNotifyHook2 = load_exe_hook;

#pragma managed(pop)

#endif
