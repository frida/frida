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

#ifndef WORK
# error "Did not get shared only arguments"
#endif

#ifdef BREAK
# error "got static only C args, but shouldn't have"
#endif

int DLL_PUBLIC libfunc(void) {
    return 3;
}
