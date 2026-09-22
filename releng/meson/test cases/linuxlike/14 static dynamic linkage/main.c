#include "stdio.h"
#include "zlib.h"

int main(void) {
    printf("%s\n", zlibVersion());
    return 0;
}
