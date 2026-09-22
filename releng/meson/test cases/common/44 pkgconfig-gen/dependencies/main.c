#include <simple.h>

#ifndef LIBFOO
#error LIBFOO should be defined in pkgconfig cflags
#endif

int main(int argc, char *argv[])
{
  return simple_function() == 42 ? 0 : 1;
}
