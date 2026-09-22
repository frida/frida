#include <iostream>
#include "libA.hpp"
#include "libB.hpp"

using namespace std;

int main(void) {
  cout << getLibStr() << endl;
  cout << getZlibVers() << endl;
  return EXIT_SUCCESS;
}
