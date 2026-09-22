#pragma once

/* Both funcs here for simplicity. */

#if defined _WIN32 || defined __CYGWIN__
#if defined BUILDING_DLL
  #define DLL_PUBLIC __declspec(dllexport)
#else
  #define DLL_PUBLIC __declspec(dllimport)
#endif
#else
  #if defined __GNUC__
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
  #else
    #pragma message ("Compiler does not support symbol visibility.")
    #define DLL_PUBLIC
  #endif
#endif

int DLL_PUBLIC func1(void);
int DLL_PUBLIC func2(void);
