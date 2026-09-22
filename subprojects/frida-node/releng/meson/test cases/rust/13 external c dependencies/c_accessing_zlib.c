#include <stdio.h>
#include <string.h>
#include <zlib.h>

void c_accessing_zlib(void) {
    struct z_stream_s zstream;
    printf("Hello from C!\n");
    memset(&zstream, 0, sizeof(zstream));
    inflateInit(&zstream);
}
