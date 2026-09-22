#include "sharedlib/shared.hpp"

int main(void) {
  SharedClass cl1;
  if(cl1.getNumber() != 42) {
    return 1;
  }
  cl1.doStuff();
  if(cl1.getNumber() != 43) {
    return 2;
  }
  return 0;
}
