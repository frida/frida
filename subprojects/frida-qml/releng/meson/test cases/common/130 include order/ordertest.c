#include "hdr.h"
#include "prefer-build-dir-over-src-dir.h"

#if !defined(SOME_DEFINE) || SOME_DEFINE != 42
#error "Should have picked up hdr.h from inc1/hdr.h"
#endif

int main(void)
{
  return 0;
}
