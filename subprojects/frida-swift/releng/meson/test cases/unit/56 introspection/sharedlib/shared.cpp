#include "shared.hpp"

void SharedClass::doStuff() {
  number++;
}

int SharedClass::getNumber() const {
  return number;
}
