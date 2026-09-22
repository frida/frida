#include <frida-gumjs.h>
#include <ptrauth.h>

_Static_assert (ptrauth_type_discriminator (__typeof__ (((GSourceFuncs *) 0)->prepare))
    == 0xe4fe, "the kernel's own ABI still discriminates by type");
