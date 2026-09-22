#include <string.h>
#include <config9a.h>
#include <config9b.h>

#if defined(A_UNDEFINED) || defined(B_UNDEFINED)
#error "Should not be defined"
#endif

#if !defined(A_DEFINED) || !defined(B_DEFINED)
#error "Should be defined"
#endif

int main(void) {
    return strcmp(A_STRING, "foo")
        || strcmp(B_STRING, "foo")
        || A_INT != 42
        || B_INT != 42;
}
