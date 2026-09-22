#include <stdio.h>

#if defined(_WIN32) || defined(__CYGWIN__)
 __declspec(dllimport)
#endif
unsigned square_unsigned (unsigned a);

int main(void)
{
  unsigned int ret = square_unsigned (2);
  if (ret != 4) {
    printf("Got %u instead of 4\n", ret);
    return 1;
  }
  return 0;
}
