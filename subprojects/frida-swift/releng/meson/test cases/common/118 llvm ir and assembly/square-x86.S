#include "symbol-underscore.h"

/* This sadly doesn't test the symbol underscore stuff. I can't figure out how
 * to not use an automatic stdcall mechanism and do everything manually. */
#ifdef _MSC_VER

.386
.MODEL FLAT, C

PUBLIC square_unsigned
_TEXT   SEGMENT

square_unsigned PROC var1:DWORD
    mov     eax, var1
    imul eax, eax
    ret

square_unsigned ENDP

_TEXT   ENDS
END

#else

.text
.globl SYMBOL_NAME(square_unsigned)
# ifdef __linux__
.type square_unsigned, %function
#endif

SYMBOL_NAME(square_unsigned):
    movl 4(%esp), %eax
    imull %eax, %eax
    retl

#endif
