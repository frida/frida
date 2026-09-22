#include<entity.h>
#include<stdio.h>

#ifndef USING_ENT
#error "Entity use flag not used for compilation."
#endif

int main(void) {
    if(entity_func1() != 5) {
        printf("Error in func1.\n");
        return 1;
    }
    if(entity_func2() != 9) {
        printf("Error in func2.\n");
        return 2;
    }
    return 0;
}
