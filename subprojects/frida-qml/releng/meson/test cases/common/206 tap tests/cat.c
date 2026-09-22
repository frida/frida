#include <errno.h>
#include <stdio.h>

int main(int argc, char **argv) {
    char buf[1024];
    size_t len;
    FILE *fh;

    if (argc != 2) {
        fprintf(stderr, "Incorrect number of arguments, got %i\n", argc);
        return 1;
    }
    fh = fopen(argv[1], "r");
    if (fh == NULL) {
        fprintf(stderr, "Opening %s: errno=%i\n", argv[1], errno);
        return 1;
    }
    do {
        len = fread(buf, 1, sizeof(buf), fh);
        if (len > 0) {
            fwrite(buf, 1, len, stdout);
        }
    } while (len > 0);
    fclose(fh);
    return 0;
}
