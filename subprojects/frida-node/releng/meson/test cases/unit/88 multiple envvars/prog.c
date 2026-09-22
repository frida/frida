#include<stdio.h>

#ifndef CPPFLAG
#error CPPFLAG not set
#endif

#ifndef CFLAG
#error CFLAGS not set
#endif

#ifdef CXXFLAG
#error CXXFLAG is set
#endif

int main(int argc, char **argv) {
    printf("%d %s\n", argc, argv[0]);
    return 0;
}
