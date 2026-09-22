#include<stdio.h>
#include<assert.h>

#define ARRSIZE 80

int main(int argc, char **argv) {
    char arr[ARRSIZE];
    char *ifilename;
    char *ofilename;
    FILE *ifile;
    FILE *ofile;
    size_t bytes;

    if(argc != 3) {
        fprintf(stderr, "%s <input file> <output file>\n", argv[0]);
        return 1;
    }
    ifilename = argv[1];
    ofilename = argv[2];
    printf("%s\n", ifilename);
    ifile = fopen(ifilename, "r");
    if(!ifile) {
        fprintf(stderr, "Could not open source file %s.\n", ifilename);
        return 1;
    }
    ofile = fopen(ofilename, "w");
    if(!ofile) {
        fprintf(stderr, "Could not open target file %s\n", ofilename);
        fclose(ifile);
        return 1;
    }
    bytes = fread(arr, 1, ARRSIZE, ifile);
    assert(bytes < 80);
    assert(bytes > 0);
    fwrite(arr, 1, bytes, ofile);

    fclose(ifile);
    fclose(ofile);
    return 0;
}
