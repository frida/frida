#pragma once

#include "cmmodlib++_export.h"
#include <string>

class CMMODLIB___EXPORT cmModClass {
private:
  std::string str;

  std::string getStr1() const;
  std::string getStr2() const;
public:
  cmModClass(std::string foo);

  std::string getStr() const;
};
