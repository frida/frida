#include <iostream>
#include "cmMod.hpp"

using namespace std;

int main(void) {
  cmModClass obj("Hello (LIB TEST)");
  cout << obj.getStr() << endl;
  return 0;
}
