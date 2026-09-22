#include "cmMod.hpp"

using namespace std;

#define MESON_INCLUDE_IMPL
#include "fakeInc/cmModInc1.cpp"
#include "fakeInc/cmModInc2.cpp"
#include "fakeInc/cmModInc3.cpp"
#include "fakeInc/cmModInc4.cpp"
#undef MESON_INCLUDE_IMPL
