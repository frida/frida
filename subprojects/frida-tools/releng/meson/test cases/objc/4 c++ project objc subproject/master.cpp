
#include <iostream>

extern "C"
int foo();

int main(void) {
  std::cout << "Starting\n";
  std::cout << foo() << "\n";
  return 0;
}
