#include "../lib.h"

int get_stnodep_value (void);

SYMBOL_EXPORT
int get_ststdep_value (void) {
  return get_stnodep_value ();
}
