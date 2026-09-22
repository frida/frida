#include<cstdio>

#ifndef CPPFLAG
#error CPPFLAG not set
#endif

#ifdef CFLAG
#error CFLAG is set
#endif

#ifndef CXXFLAG
#error CXXFLAG not set
#endif

int main(int argc, char **argv) {
    printf("%d %s\n", argc, argv[0]);
    return 0;
}
