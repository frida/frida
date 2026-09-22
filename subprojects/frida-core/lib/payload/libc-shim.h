#ifndef __FRIDA_LIBC_SHIM_H__
#define __FRIDA_LIBC_SHIM_H__

#ifdef HAVE_LINUX
int dup3 (int oldfd, int newfd, int flags);
#endif

#endif
