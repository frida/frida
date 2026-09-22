#include <stdio.h>
#include <stdint.h>

int32_t hello_from_rust(const int32_t a, const int32_t b);

static void hello_from_c(void) {
    printf("Hello from C!\n");
}

void hello_from_both(void) {
    hello_from_c();
    if (hello_from_rust(2, 3) == 5)
        printf("Hello from Rust!\n");
}
