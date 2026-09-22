#include<stdio.h>

extern const char *vcstag;

int main(void) {
    printf("Version is %s\n", vcstag);
    return 0;
}
