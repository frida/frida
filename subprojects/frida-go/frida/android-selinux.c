#include "android-selinux.h"

void android_patch_selinux(void) {
#ifdef __ANDROID__
    frida_selinux_patch_policy();
#endif
}