#include <string.h>
/* config.h must not be in quotes:
 * https://gcc.gnu.org/onlinedocs/cpp/Search-Path.html
 */
#include <config.h>

#ifdef SHOULD_BE_UNDEF
#error "FAIL!"
#endif

int main(void) {
#ifndef BE_TRUE
    return 1;
#else
    return strcmp(MESSAGE, "mystring");
#endif
}
