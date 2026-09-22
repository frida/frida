#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char * argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Takes exactly 2 arguments\n");
        return 1;
    }

    char * dirname = getenv("MESON_INSTALL_DESTDIR_PREFIX");
    char * fullname = malloc(strlen(dirname) + 1 + strlen(argv[1]) + 1);
    strcpy(fullname, dirname);
    strcat(fullname, "/");
    strcat(fullname, argv[1]);

    FILE * fp = fopen(fullname, "w");
    if (!fp)
        return 1;

    fputs("Some text\n", fp);
    fclose(fp);

    free(fullname);

    return 0;
}
