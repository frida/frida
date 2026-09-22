#if defined __GNUC__
  #define EXPORT_PUBLIC __attribute__ ((visibility("default")))
#else
  #pragma message ("Compiler does not support symbol visibility.")
  #define EXPORT_PUBLIC
#endif
