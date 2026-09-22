// Verify we preprocess as C language, otherwise including math.h would fail.
// See https://github.com/mesonbuild/meson/issues/11940.
#include <math.h>
