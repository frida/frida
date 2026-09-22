#include<stdio.h>
#include<string.h>

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Incorrect number of arguments.\n");
        return 1;
    }
    if(strcmp(argv[1], "first") != 0) {
        fprintf(stderr, "First argument is wrong.\n");
        return 1;
    }
    if(strcmp(argv[2], "second") != 0) {
        fprintf(stderr, "Second argument is wrong.\n");
        return 1;
    }
    return 0;
}
