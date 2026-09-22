#include "libB.hpp"
#include <zlib.h>

std::string getZlibVers(void) {
  return zlibVersion();
}
