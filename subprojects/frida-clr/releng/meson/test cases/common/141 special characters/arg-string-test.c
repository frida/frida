#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  const char *s = CHAR;
  assert(argc == 2);
  assert(strlen(s) == 1);
  if (s[0] != argv[1][0])
    fprintf(stderr, "Expected %x, got %x\n", (unsigned int) s[0], (unsigned int) argv[1][0]);
  assert(s[0] == argv[1][0]);
  return 0;
}
