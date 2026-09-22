#include<stdio.h>

int func(void); /* Files in different subdirs return different values. */

int main(void) {
    if(func() == 0) {
        printf("Iz success.\n");
    } else {
        printf("Iz fail.\n");
        return 1;
    }
    return 0;
}
