#include <string.h>
#include <config6.h>

int main(void) {
    return strcmp(MESSAGE1, "foo")
        || strcmp(MESSAGE2, "@var1@")
        || strcmp(MESSAGE3, "\\foo")
        || strcmp(MESSAGE4, "\\@var1@")
        || strcmp(MESSAGE5, "@var1bar")
        || strcmp(MESSAGE6, "\\ @ @ \\@ \\@");
}
