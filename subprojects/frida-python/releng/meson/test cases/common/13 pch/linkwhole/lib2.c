#include<stdio.h>

void func2() {
    const char *cl = GetCommandLineA();
    printf("Command line was: %s\n", cl);
}
