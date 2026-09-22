#pragma once

#ifdef _WIN32
  #ifdef STATIC_COMPILATION
    #define DO_IMPORT extern
  #else
    #define DO_IMPORT __declspec(dllimport)
  #endif
  #define DO_EXPORT __declspec(dllexport)
#else
  #define DO_IMPORT extern
  #define DO_EXPORT
#endif
