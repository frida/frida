#pragma once

#if defined _WIN32 || defined __CYGWIN__
  #define DLL_PUBLIC __declspec(dllimport)
#else
  #define DLL_PUBLIC
#endif

DLL_PUBLIC
int somefunc(void);
