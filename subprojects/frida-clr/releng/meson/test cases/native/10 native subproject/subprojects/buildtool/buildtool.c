#include <stdio.h>

const char * gen_main(void);

int main() {
    printf("%s", gen_main());
    printf("{ return 0; }\n");
    return 0;
}
