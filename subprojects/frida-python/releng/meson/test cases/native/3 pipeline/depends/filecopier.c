#include<stdio.h>
#include<assert.h>

#define BUFSIZE 1024

int main(int argc, char **argv) {
    char buffer[BUFSIZE];
    size_t num_read;
    size_t num_written;
    FILE *fin = fopen(argv[1], "rb");
    FILE *fout;
    assert(argc>0);
    assert(fin);
    num_read = fread(buffer, 1, BUFSIZE, fin);
    assert(num_read > 0);
    fclose(fin);
    fout = fopen(argv[2], "wb");
    assert(fout);
    num_written = fwrite(buffer, 1, num_read, fout);
    assert(num_written == num_read);
    fclose(fout);
    return 0;
}
