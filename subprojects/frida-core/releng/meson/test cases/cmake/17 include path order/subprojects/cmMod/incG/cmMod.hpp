#pragma once

#include "cmmodlib++_export.h"
#include <string>

class CMMODLIB___EXPORT cmModClass {
private:
  std::string str;

public:
  cmModClass(std::string foo);

  std::string getStr() const;
};
