#include <unistd.h>

int
main (void)
{
  int remaining = 60;
  while (remaining != 0)
    remaining = sleep (remaining);
  return 0;
}
