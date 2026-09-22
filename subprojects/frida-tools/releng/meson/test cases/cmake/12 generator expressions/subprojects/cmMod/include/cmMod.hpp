#pragma once

#include <string>

#ifndef CMAKE_FLAG_REQUIRED_A
#error "The flag CMAKE_FLAG_REQUIRED_A was not set"
#endif

#ifndef CMAKE_FLAG_REQUIRED_B
#error "The flag CMAKE_FLAG_REQUIRED_B was not set"
#endif

#ifndef CMAKE_FLAG_REQUIRED_C
#error "The flag CMAKE_FLAG_REQUIRED_C was not set"
#endif

#ifdef CMAKE_FLAG_ERROR_A
#error "The flag CMAKE_FLAG_ERROR_A was set"
#endif

#ifndef CMAKE_TRUE_FLAG
#error "The flag CMAKE_TRUE_FLAG was not set"
#endif

#ifdef CMAKE_FALSE_FLAG
#error "The flag CMAKE_FALSE_FLAG was set"
#endif

#ifndef CMAKE_TGT_EXISTS
#error "The flag CMAKE_TGT_EXISTS was not set"
#endif

#ifdef CMAKE_TGT_NEXISTS
#error "The flag CMAKE_TGT_NEXISTS was set"
#endif

#ifndef CMAKE_PROP1_OK
#error "The flag CMAKE_PROP1_OK was not set"
#endif

#ifdef CMAKE_PROP1_ERROR
#error "The flag CMAKE_PROP1_ERROR was set"
#endif

#ifndef CMAKE_PROP2_OK
#error "The flag CMAKE_PROP2_OK was not set"
#endif

#ifdef CMAKE_PROP2_ERROR
#error "The flag CMAKE_PROP2_ERROR was set"
#endif

class cmModClass {
  private:
    std::string str;
  public:
    cmModClass(std::string foo) {
      str = foo + " World ";
      str += CMAKE_COMPILER_DEFINE_STR;
    }

    inline std::string getStr() const { return str; }
};
