#include <assert.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  assert(argc == 2);
  FILE *f = fopen(argv[1], "w");
  const char msg[] = "int main(void) {return 0;}\n";
  size_t w = fwrite(msg, 1, sizeof(msg) - 1, f);
  assert(w == sizeof(msg) - 1);
  int r = fclose(f);
  assert(r == 0);
  return 0;
}
