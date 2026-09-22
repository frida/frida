#pragma once

#include "cmmodlib++_export.h"
#include <string>

#if MESON_MAGIC_FLAG != 42 && MESON_MAGIC_FLAG != 21
#error "Invalid MESON_MAGIC_FLAG"
#endif

class CMMODLIB___EXPORT cmModClass {
private:
  std::string str;

public:
  cmModClass(std::string foo);

  std::string getStr() const;
};
