// This test only succeeds if run in the source root dir.

#include<stdio.h>

int main(void) {
    FILE *f = fopen("opener.c", "r");
    if(f) {
        fclose(f);
        return 0;
    }
    return 1;
}
