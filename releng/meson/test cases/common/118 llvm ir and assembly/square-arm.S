#include "symbol-underscore.h"

#ifdef _MSC_VER

    AREA _TEXT, ARM, CODE, READONLY

    EXPORT SYMBOL_NAME(square_unsigned)
SYMBOL_NAME(square_unsigned) PROC
    mul r1, r0, r0
    mov r0, r1
    mov pc, lr
SYMBOL_NAME(square_unsigned) ENDP

    END

#else

.text
.globl SYMBOL_NAME(square_unsigned)
# ifdef __linux__
.type square_unsigned, %function
#endif

SYMBOL_NAME(square_unsigned):
    mul r1, r0, r0
    mov r0, r1
    mov pc, lr

#endif
