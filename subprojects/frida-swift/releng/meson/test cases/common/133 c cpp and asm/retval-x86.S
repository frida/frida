#include "symbol-underscore.h"

.text
.globl SYMBOL_NAME(get_retval)
/* Only supported on Linux with GAS */
# ifdef __linux__
.type get_retval, %function
#endif

SYMBOL_NAME(get_retval):
    xorl	%eax, %eax
    retl
