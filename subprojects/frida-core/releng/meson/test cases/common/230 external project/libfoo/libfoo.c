#include "libfoo.h"

int func(void);

int call_foo()
{
  return func() == 1 ? 42 : 0;
}
