#include <stdio.h>

int
main (int argc, char * argv[])
{
  fputs ("Hello stdout", stdout);
  fputs ("Hello stderr", stderr);
  return 0;
}
