#include "sharedlib/shared.hpp"
#include "staticlib/static.h"

int main(void) {
  for(int i = 0; i < 1000; add_numbers(i, 1)) {
    SharedClass cl1;
    if(cl1.getNumber() != 42) {
      return 1;
    }
    cl1.doStuff();
    if(cl1.getNumber() != 43) {
      return 2;
    }
  }
  return 0;
}
