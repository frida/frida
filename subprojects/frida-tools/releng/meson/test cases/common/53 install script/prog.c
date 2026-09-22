#include<stdio.h>

#ifdef _WIN32
  #define DO_IMPORT __declspec(dllimport)
#else
  #define DO_IMPORT
#endif

DO_IMPORT int foo(void);

int main(void) {
    printf("This is text.\n");
    return foo();
}
