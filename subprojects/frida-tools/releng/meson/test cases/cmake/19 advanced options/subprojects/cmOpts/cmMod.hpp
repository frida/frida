#pragma once

#include <string>

class cmModClass {
private:
  std::string str;

public:
  cmModClass(std::string foo);

  std::string getStr() const;
  int getInt() const;
};
