#include "syscall.h"

ssize_t
frida_syscall_4 (size_t n, size_t a, size_t b, size_t c, size_t d)
{
  ssize_t result;

#if defined (__i386__)
  {
    register size_t ebx asm ("ebx") = a;
    register size_t ecx asm ("ecx") = b;
    register size_t edx asm ("edx") = c;
    register size_t esi asm ("esi") = d;

    asm volatile (
        "int $0x80\n\t"
        : "=a" (result)
        : "0" (n),
          "r" (ebx),
          "r" (ecx),
          "r" (edx),
          "r" (esi)
        : "cc", "memory"
    );
  }
#elif defined (__x86_64__)
  {
    register size_t rdi asm ("rdi") = a;
    register size_t rsi asm ("rsi") = b;
    register size_t rdx asm ("rdx") = c;
    register size_t r10 asm ("r10") = d;

    asm volatile (
        "syscall\n\t"
        : "=a" (result)
        : "0" (n),
          "r" (rdi),
          "r" (rsi),
          "r" (rdx),
          "r" (r10)
        : "rcx", "r11", "cc", "memory"
    );
  }
#elif defined (__arm__) && defined (__ARM_EABI__)
  {
    register ssize_t r6 asm ("r6") = n;
    register  size_t r0 asm ("r0") = a;
    register  size_t r1 asm ("r1") = b;
    register  size_t r2 asm ("r2") = c;
    register  size_t r3 asm ("r3") = d;

    asm volatile (
        "push {r7}\n\t"
        "mov r7, r6\n\t"
        "swi 0x0\n\t"
        "pop {r7}\n\t"
        : "+r" (r0)
        : "r" (r1),
          "r" (r2),
          "r" (r3),
          "r" (r6)
        : "memory"
    );

    result = r0;
  }
#elif defined (__arm__)
  {
    register ssize_t r0 asm ("r0") = n;
    register  size_t r1 asm ("r1") = a;
    register  size_t r2 asm ("r2") = b;
    register  size_t r3 asm ("r3") = c;
    register  size_t r4 asm ("r4") = d;

    asm volatile (
        "swi %[syscall]\n\t"
        : "+r" (r0)
        : "r" (r1),
          "r" (r2),
          "r" (r3),
          "r" (r4),
          [syscall] "i" (__NR_syscall)
        : "memory"
    );

    result = r0;
  }
#elif defined (__aarch64__)
  {
    register ssize_t x8 asm ("x8") = n;
    register  size_t x0 asm ("x0") = a;
    register  size_t x1 asm ("x1") = b;
    register  size_t x2 asm ("x2") = c;
    register  size_t x3 asm ("x3") = d;

    asm volatile (
        "svc 0x0\n\t"
        : "+r" (x0)
        : "r" (x1),
          "r" (x2),
          "r" (x3),
          "r" (x8)
        : "memory"
    );

    result = x0;
  }
#elif defined (__mips__)
  {
    register ssize_t v0 asm ("$16") = n;
    register  size_t a0 asm ("$4") = a;
    register  size_t a1 asm ("$5") = b;
    register  size_t a2 asm ("$6") = c;
    register  size_t a3 asm ("$7") = d;
    int status;
    ssize_t retval;

    asm volatile (
        ".set noreorder\n\t"
        "move $2, %1\n\t"
        "syscall\n\t"
        "move %0, $7\n\t"
        "move %1, $2\n\t"
        ".set reorder\n\t"
        : "=r" (status),
          "=r" (retval)
        : "r" (v0),
          "r" (a0),
          "r" (a1),
          "r" (a2),
          "r" (a3)
        : "$1", "$2", "$3",
          "$10", "$11", "$12", "$13", "$14", "$15",
          "$24", "$25",
          "hi", "lo",
          "memory"
    );

    result = (status == 0) ? retval : -retval;
  }
#elif defined (__riscv)
  {
    register ssize_t a7 asm ("a7") = n;
    register  size_t a0 asm ("a0") = a;
    register  size_t a1 asm ("a1") = b;
    register  size_t a2 asm ("a2") = c;
    register  size_t a3 asm ("a3") = d;

    asm volatile (
        "ecall\n\t"
        : "+r" (a0)
        : "r" (a1),
          "r" (a2),
          "r" (a3),
          "r" (a7)
        : "memory"
    );

    result = a0;
  }
#endif

  return result;
}
