#if defined (_MSC_VER) && defined (HAVE_COMPILER_BACKEND_LINKED)

#include <glib.h>

#ifdef HAVE_ARM64
# define FRIDA_CGO_INIT_FUNC _st0_arm64_windows_lib
#elif GLIB_SIZEOF_VOID_P == 8
# define FRIDA_CGO_INIT_FUNC _st0_amd64_windows_lib
#else
# define FRIDA_CGO_INIT_FUNC st0_386_windows_lib
#endif

extern void FRIDA_CGO_INIT_FUNC ();

void
_frida_compiler_backend_init_go_runtime (void)
{
  FRIDA_CGO_INIT_FUNC ();
}

#else

void
_frida_compiler_backend_init_go_runtime (void)
{
}

#endif
