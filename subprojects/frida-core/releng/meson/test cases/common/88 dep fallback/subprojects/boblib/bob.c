#include"bob.h"

#ifdef _MSC_VER
__declspec(dllexport)
#endif
const char* get_bob(void) {
    return "bob";
}
