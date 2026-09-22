#if defined _WIN32 || defined __CYGWIN__
  #define DLL_IMPORT __declspec(dllimport)
#else
  #define DLL_IMPORT
#endif

int DLL_IMPORT cppfunc(void);

int otherfunc(void) {
    return cppfunc() != 42;
}
