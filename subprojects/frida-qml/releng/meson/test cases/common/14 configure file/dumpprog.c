#define SHOULD_BE_UNDEFINED 1

#include"config3.h"
#include<string.h>
#include<stdio.h>

#ifdef SHOULD_BE_UNDEFINED
#error Token did not get undefined.
#endif

#ifndef SHOULD_BE_DEFINED
#error Token did not get defined
#endif

#define stringify(s) str(s)
#define str(s) #s

int main(void) {
#if !(SHOULD_BE_UNQUOTED_STRING == string)
        printf("String token (unquoted) defined wrong.\n");
        return 1;
#endif
    if(strcmp(SHOULD_BE_STRING, "string") != 0) {
        printf("String token defined wrong.\n");
        return 1;
    }
    if(strcmp(SHOULD_BE_STRING2, "A \"B\" C") != 0) {
        printf("String token 2 defined wrong.\n");
        return 1;
    }
    if(strcmp(SHOULD_BE_STRING3, "A \"\" C") != 0) {
        printf("String token 3 defined wrong.\n");
        return 1;
    }
    if(strcmp(SHOULD_BE_STRING4, "A \" C") != 0) {
        printf("String token 4 defined wrong.\n");
        return 1;
    }
    if(SHOULD_BE_ONE != 1) {
        printf("One defined incorrectly.\n");
        return 1;
    }
    if(SHOULD_BE_ZERO != 0) {
        printf("Zero defined incorrectly.\n");
        return 1;
    }
    if(strcmp(SHOULD_BE_QUOTED_ONE, "1") != 0) {
        printf("Quoted number defined incorrectly.\n");
        return 1;
    }
    SHOULD_BE_RETURN 0;
}
