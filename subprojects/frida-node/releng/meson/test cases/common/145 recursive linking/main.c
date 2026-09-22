#include <stdio.h>

#include "lib.h"

int get_stnodep_value (void);
int get_stshdep_value (void);
int get_ststdep_value (void);
SYMBOL_IMPORT int get_shnodep_value (void);
SYMBOL_IMPORT int get_shshdep_value (void);
SYMBOL_IMPORT int get_shstdep_value (void);

int main(void) {
  int val;

  val = get_shnodep_value ();
  if (val != 1) {
    printf("shnodep was %i instead of 1\n", val);
    return -1;
  }
  val = get_stnodep_value ();
  if (val != 2) {
    printf("stnodep was %i instead of 2\n", val);
    return -2;
  }
  val = get_shshdep_value ();
  if (val != 1) {
    printf("shshdep was %i instead of 1\n", val);
    return -3;
  }
  val = get_shstdep_value ();
  if (val != 2) {
    printf("shstdep was %i instead of 2\n", val);
    return -4;
  }
  val = get_stshdep_value ();
  if (val != 1) {
    printf("shstdep was %i instead of 1\n", val);
    return -5;
  }
  val = get_ststdep_value ();
  if (val != 2) {
    printf("ststdep was %i instead of 2\n", val);
    return -6;
  }
  return 0;
}
