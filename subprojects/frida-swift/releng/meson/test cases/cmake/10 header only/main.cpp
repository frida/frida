#include <iostream>
#include <cmMod.hpp>

using namespace std;

#define EXPECTED "Hello World compDef 42"

int main(void) {
  cmModClass obj("Hello");
  cout << obj.getStr() << endl;
  if (obj.getStr() != EXPECTED) {
    cerr << "Expected: '" << EXPECTED << "'" << endl;
    return 1;
  }
  return 0;
}
