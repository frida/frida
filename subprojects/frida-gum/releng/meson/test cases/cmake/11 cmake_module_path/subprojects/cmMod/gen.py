with open('main.c', 'w') as fp:
  print('''
#include <stdio.h>

int main(void) {
  printf(\"Hello World\");
  return 0;
}
''', file=fp)
