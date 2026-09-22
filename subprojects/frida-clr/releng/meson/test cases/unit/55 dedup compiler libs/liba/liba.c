#include "liba.h"

static int val;

void liba_add(int x)
{
  val += x;
}

void liba_sub(int x)
{
  val -= x;
}

int liba_get(void)
{
  return val;
}
