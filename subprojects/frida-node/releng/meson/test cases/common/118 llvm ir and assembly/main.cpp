#include <stdio.h>

extern "C" {
  unsigned square_unsigned (unsigned a);
}

int main (void)
{
  unsigned int ret = square_unsigned (2);
  if (ret != 4) {
    printf("Got %u instead of 4\n", ret);
    return 1;
  }
  return 0;
}
