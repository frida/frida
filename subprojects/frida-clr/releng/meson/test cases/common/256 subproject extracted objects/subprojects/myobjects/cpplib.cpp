#define BUILDING_DLL
#include "cpplib.h"

extern "C" int DLL_PUBLIC cppfunc(void) {
    return 42;
}
