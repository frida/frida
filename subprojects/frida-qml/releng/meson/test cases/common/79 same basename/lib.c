#if defined _WIN32 || defined __CYGWIN__
#define DLL_PUBLIC __declspec(dllexport)
#else
  #if defined __GNUC__
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
  #else
    #pragma message ("Compiler does not support symbol visibility.")
    #define DLL_PUBLIC
  #endif
#endif

#if defined SHAR
int DLL_PUBLIC func(void) {
    return 1;
}
#elif defined STAT
int func(void) {
    return 0;
}
#else
#error "Missing type definition."
#endif
