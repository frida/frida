/* See http://gcc.gnu.org/wiki/Visibility#How_to_use_the_new_C.2B-.2B-_visibility_support */
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef BUILDING_DLL
    #define DLL_PUBLIC __declspec(dllexport)
  #else
    #define DLL_PUBLIC __declspec(dllimport)
  #endif
#else
    #define DLL_PUBLIC __attribute__ ((visibility ("default")))
#endif

extern "C" int DLL_PUBLIC cppfunc(void);
