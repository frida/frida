#include<stdio.h>

#ifndef FOO
#error FOO is not defined.
#endif

#ifndef BAR
#error BAR is not defined.
#endif

int main(int argc, char **argv) {
    printf("All is well.\n");
    return 0;
}
