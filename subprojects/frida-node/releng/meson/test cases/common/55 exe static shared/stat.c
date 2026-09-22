#include "subdir/exports.h"

int shlibfunc(void);

int DLL_PUBLIC statlibfunc(void) {
    return shlibfunc();
}
