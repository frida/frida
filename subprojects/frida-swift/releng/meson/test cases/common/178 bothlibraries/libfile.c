#include "mylib.h"

DO_EXPORT int retval = 42;

DO_EXPORT int func(void) {
    return retval;
}
