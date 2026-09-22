#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Incorrect number of arguments, got %i\n", argc);
        return 1;
    }
    puts(argv[1]);
    return 0;
}
