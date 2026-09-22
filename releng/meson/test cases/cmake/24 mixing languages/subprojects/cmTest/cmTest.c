#include "cmTest.h"
#include <stdio.h>

#if SOME_MAGIC_DEFINE != 42
#error "SOME_MAGIC_DEFINE != 42"
#endif

int foo(int x);

int doStuff(void) {
  printf("Hello World\n");
  return foo(42);
}
