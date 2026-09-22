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

/*
 * This file pretends to be a language runtime that supports extension
 * modules.
 */

int DLL_PUBLIC func_from_language_runtime(void) {
    return 86;
}
