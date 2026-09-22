#pragma once

#include <string>
#include "cmmodlib_export.h"

class CMMODLIB_EXPORT cmModClass {
  private:
    std::string str;
  public:
    cmModClass(std::string foo);

    std::string getStr() const;
};
