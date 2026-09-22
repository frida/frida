#include <stdlib.h>
#include <iostream>
#include "libA.hpp"
#include "libB.hpp"

using namespace std;

int main(void) {
  cout << getLibStr() << " -- " << getZlibVers() << endl;
  return EXIT_SUCCESS;
}
