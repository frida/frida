#include <stdio.h>
static int g_checked = 0;

static void __attribute__((constructor(101), used)) init_checked(void) {
    g_checked=100;
    fprintf(stdout, "inited\n");
}


int get_checked(void) {
    return g_checked;
}
