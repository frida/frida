#include "../lib.h"

int get_builto_value (void);

SYMBOL_EXPORT
int get_stodep_value (void) {
  return get_builto_value ();
}
