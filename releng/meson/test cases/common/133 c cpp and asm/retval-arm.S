#include "symbol-underscore.h"

.text
.globl SYMBOL_NAME(get_retval)
# ifdef __linux__
.type get_retval, %function
#endif

SYMBOL_NAME(get_retval):
    mov	r0, #0
    mov	pc, lr
