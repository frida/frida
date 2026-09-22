#include <liba.h>
#include "libb.h"

void libb_mul(int x)
{
  liba_add(liba_get() * (x - 1));
}
