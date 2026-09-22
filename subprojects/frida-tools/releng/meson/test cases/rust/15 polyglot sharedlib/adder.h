#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
    #pragma message ("Compiler does not support symbol visibility.")
    #define DLL_PUBLIC
  #endif
#endif

typedef struct _Adder adder;

DLL_PUBLIC extern adder*  adder_create(int number);
DLL_PUBLIC extern int  adder_add(adder *a, int number);
DLL_PUBLIC extern void  adder_destroy(adder*);

#ifdef __cplusplus
}
#endif
