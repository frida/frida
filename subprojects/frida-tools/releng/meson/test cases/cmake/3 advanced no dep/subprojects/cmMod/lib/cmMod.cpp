#include "cmMod.hpp"
#include "config.h"

#if CONFIG_OPT != 42
#error "Invalid value of CONFIG_OPT"
#endif

using namespace std;

cmModClass::cmModClass(string foo) {
  str = foo + " World";
}

string cmModClass::getStr() const {
  return str;
}
