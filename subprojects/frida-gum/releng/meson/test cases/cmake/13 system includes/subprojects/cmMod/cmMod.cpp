#include "cmMod.hpp"
#include "triggerWarn.hpp"

using namespace std;

cmModClass::cmModClass(string foo) {
  str = foo + " World " + to_string(bar(World));
}

string cmModClass::getStr() const {
  return str;
}
