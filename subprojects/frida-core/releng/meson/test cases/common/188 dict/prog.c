#include <string.h>

int main(int argc, char **argv) {
  if (argc != 3)
    return 1;

  return strcmp(argv[1], argv[2]);
}
