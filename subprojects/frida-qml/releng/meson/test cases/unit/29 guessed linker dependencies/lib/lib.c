#if defined _WIN32
  #define DLL_PUBLIC __declspec(dllexport)
#else
  #if defined __GNUC__
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
  #else
    #pragma message ("Compiler does not support symbol visibility.")
    #define DLL_PUBLIC
  #endif
#endif

void DLL_PUBLIC liba_func() {
}

#ifdef MORE_EXPORTS

void DLL_PUBLIC libb_func() {
}

#endif
