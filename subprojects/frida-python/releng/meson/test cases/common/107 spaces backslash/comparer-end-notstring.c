#include "comparer.h"

#ifndef COMPARER_INCLUDED
#error "comparer.h not included"
#endif

/* This converts foo\\\\bar\\\\ to "foo\\bar\\" (string literal) */
#define Q(x) #x
#define QUOTE(x) Q(x)

#define COMPARE_WITH "foo\\bar\\" /* This is the literal `foo\bar\` */

int main(void) {
    if(strcmp(QUOTE(DEF_WITH_BACKSLASH), COMPARE_WITH)) {
        printf("Arg string is quoted incorrectly: %s instead of %s\n",
               QUOTE(DEF_WITH_BACKSLASH), COMPARE_WITH);
        return 1;
    }
    return 0;
}
