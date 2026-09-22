#include "cmMod.hpp"

using namespace std;

cmModClass::cmModClass(string foo) {
  str = foo + " World";
}

string cmModClass::getStr() const {
  return str;
}
