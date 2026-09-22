#pragma once

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

class Ef {
private:
    int x;

public:

    DLL_PUBLIC Ef();
    int DLL_PUBLIC get_x() const;
};
