#pragma once

#if defined _WIN32 || defined __CYGWIN__
  #if defined BUILDING_ADDER
    #define DLL_PUBLIC __declspec(dllexport)
  #else
    #define DLL_PUBLIC __declspec(dllimport)
  #endif
#else
  #if defined __GNUC__
    #if defined BUILDING_ADDER
      #define DLL_PUBLIC __attribute__ ((visibility("default")))
    #else
      #define DLL_PUBLIC
    #endif
  #else
    #pragma message("Compiler does not support symbol visibility.")
    #define DLL_PUBLIC
  #endif
#endif

DLL_PUBLIC void c_func(void);
