#include "comparer.h"

#ifndef COMPARER_INCLUDED
#error "comparer.h not included"
#endif

#define COMPARE_WITH "foo\\bar\\" /* This is `foo\bar\` */

int main(void) {
    if (strcmp (DEF_WITH_BACKSLASH, COMPARE_WITH)) {
        printf ("Arg string is quoted incorrectly: %s vs %s\n",
                DEF_WITH_BACKSLASH, COMPARE_WITH);
        return 1;
    }
    return 0;
}
