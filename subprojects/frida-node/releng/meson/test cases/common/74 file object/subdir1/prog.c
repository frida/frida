#include<stdio.h>

int func(void);

int main(void) {
    if(func() == 1) {
        printf("Iz success.\n");
    } else {
        printf("Iz fail.\n");
        return 1;
    }
    return 0;
}
