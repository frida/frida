#include <notzlib.h>

int main (int ac, char **av)
{
  if (not_a_zlib_function () != 42)
    return 1;
  return 0;
}
