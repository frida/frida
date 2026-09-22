#include<public_header.h>
#include<stdio.h>

int main(int argc, char **argv) {
    if(public_func() != 42) {
        printf("Something failed.\n");
        return 1;
    }
    return 0;
}
