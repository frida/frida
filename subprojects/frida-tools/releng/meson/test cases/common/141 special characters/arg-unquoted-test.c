#include <assert.h>
#include <stdio.h>
#include <string.h>

#define Q(x) #x
#define QUOTE(x) Q(x)

int main(int argc, char **argv) {
  const char *s = QUOTE(CHAR);
  assert(argc == 2);
  assert(strlen(s) == 1);
  if (s[0] != argv[1][0])
    fprintf(stderr, "Expected %x, got %x\n", (unsigned int) s[0], (unsigned int) argv[1][0]);
  assert(s[0] == argv[1][0]);
  // There is no way to convert a macro argument into a character constant.
  // Otherwise we'd test that as well
  return 0;
}
