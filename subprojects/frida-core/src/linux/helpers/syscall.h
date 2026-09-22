#ifndef __FRIDA_SYSCALL_H__
#define __FRIDA_SYSCALL_H__

#ifndef NOLIBC
# include <unistd.h>
#endif
#include <sys/syscall.h>

#define frida_syscall_0(n)          frida_syscall_4 (n, 0, 0, 0, 0)
#define frida_syscall_1(n, a)       frida_syscall_4 (n, a, 0, 0, 0)
#define frida_syscall_2(n, a, b)    frida_syscall_4 (n, a, b, 0, 0)
#define frida_syscall_3(n, a, b, c) frida_syscall_4 (n, a, b, c, 0)

ssize_t frida_syscall_4 (size_t n, size_t a, size_t b, size_t c, size_t d);

#endif
