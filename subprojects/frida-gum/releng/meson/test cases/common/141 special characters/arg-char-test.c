#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv) {
  char c = CHAR;
  assert(argc == 2);
  if (c != argv[1][0])
    fprintf(stderr, "Expected %x, got %x\n", (unsigned int) c, (unsigned int) argv[1][0]);
  assert(c == argv[1][0]);
  return 0;
}
