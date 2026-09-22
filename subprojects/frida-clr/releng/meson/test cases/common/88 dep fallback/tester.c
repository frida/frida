#include"bob.h"
#include"genbob.h"
#include<string.h>
#include<stdio.h>

int main(void) {
    if(strcmp("bob", get_bob()) == 0) {
        printf("Bob is indeed bob.\n");
    } else {
        printf("ERROR: bob is not bob.\n");
        return 1;
    }
    return 0;
}
