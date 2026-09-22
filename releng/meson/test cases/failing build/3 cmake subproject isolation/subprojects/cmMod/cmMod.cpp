#include "cmMod.hpp"
#include "fileA.hpp"

using namespace std;

cmModClass::cmModClass(string foo) {
  str = foo + SOME_DEFINE;
}

string cmModClass::getStr() const {
  return str;
}
