#include "cmTest.hpp"

#if __cplusplus < 201103L
#error "At least C++11 is required"
#endif

#if __cplusplus >= 201402L
#error "At most C++11 is required"
#endif

#ifndef MESON_GLOBAL_FLAG
#error "MESON_GLOBAL_FLAG was not set"
#endif

#ifdef MESON_SPECIAL_FLAG1
#error "MESON_SPECIAL_FLAG1 *was* set"
#endif

#ifdef MESON_SPECIAL_FLAG2
#error "MESON_SPECIAL_FLAG2 *was* set"
#endif

int getTestInt() {
  return MESON_MAGIC_INT;
}
