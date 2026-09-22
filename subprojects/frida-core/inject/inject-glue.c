#include "inject-glue.h"

#include "frida-core.h"
#ifdef HAVE_ANDROID
# include "frida-selinux.h"
#endif

void
frida_inject_environment_init (void)
{
  frida_init_with_runtime (FRIDA_RUNTIME_GLIB);

#ifdef HAVE_ANDROID
  frida_selinux_patch_policy ();
#endif
}
