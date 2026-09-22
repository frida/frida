#include <iostream>
#include <cmMod.hpp>
#include <cmTest.hpp>

using namespace std;

int main(void) {
  cmModClass obj("Hello");
  cout << obj.getStr() << endl;

  int v1 = obj.getInt();
  int v2 = getTestInt();
  if (v1 != ((1 + v2) * 2)) {
    cerr << "Number test failed" << endl;
    return 1;
  }
  return 0;
}
