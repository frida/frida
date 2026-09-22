#include <assert.h>
#include "gen.h"

int main(int argc, char **argv)
{
  (void)argv;

  assert(argc == 3);
  return genfunc();
}
