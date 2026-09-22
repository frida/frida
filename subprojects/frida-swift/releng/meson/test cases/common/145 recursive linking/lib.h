#if defined _WIN32
  #ifdef MESON_STATIC_BUILD
   #define SYMBOL_EXPORT
   #define SYMBOL_IMPORT
  #else
   #define SYMBOL_IMPORT __declspec(dllimport)
   #define SYMBOL_EXPORT __declspec(dllexport)
  #endif
#else
  #define SYMBOL_IMPORT
  #if defined __GNUC__
    #define SYMBOL_EXPORT __attribute__ ((visibility("default")))
  #else
    #pragma message ("Compiler does not support symbol visibility.")
    #define SYMBOL_EXPORT
  #endif
#endif
