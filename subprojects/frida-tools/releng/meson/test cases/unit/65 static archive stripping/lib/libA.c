#include <libA.h>

static int libA_func_impl(void) { return 0; }

int libA_func(void) { return libA_func_impl(); }
