
#include<stdio.h>

int main(void) {
  const char *message = "Hello without stdlib.\n";
  return simple_print(message, simple_strlen(message));
}
