#include <libB.h>

static int libB_func_impl(void) { return 0; }

int libB_func(void) { return libB_func_impl(); }
