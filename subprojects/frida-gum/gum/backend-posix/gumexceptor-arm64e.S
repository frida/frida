/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/*
 * Tail-resume into a user-chosen PC and register set, bypassing sigreturn.
 * arm64e XNU rejects any PC minted by userspace, so the only way to honor
 * cpu_context modifications from a signal handler is to restore the state
 * by hand and branch raw.
 *
 * Registers NOT restored from ctx: X16/X17 (scratch here) and X18
 * (Darwin platform register).
 */

#ifdef __APPLE__
# define GUM_SYMBOL(s) _##s
#else
# define GUM_SYMBOL(s) s
#endif

.text
.align 4

.globl GUM_SYMBOL (_gum_exceptor_perform_divert)
GUM_SYMBOL (_gum_exceptor_perform_divert):
  mov x17, x0

  ldr x16, [x17, #0]

  ldr x0, [x17, #16]
  msr nzcv, x0

  ldr x0, [x17, #8]
  mov sp, x0

  ldp x0,  x1,  [x17, #(24 + 0  * 8)]
  ldp x2,  x3,  [x17, #(24 + 2  * 8)]
  ldp x4,  x5,  [x17, #(24 + 4  * 8)]
  ldp x6,  x7,  [x17, #(24 + 6  * 8)]
  ldp x8,  x9,  [x17, #(24 + 8  * 8)]
  ldp x10, x11, [x17, #(24 + 10 * 8)]
  ldp x12, x13, [x17, #(24 + 12 * 8)]
  ldp x14, x15, [x17, #(24 + 14 * 8)]

  ldp x19, x20, [x17, #(24 + 19 * 8)]
  ldp x21, x22, [x17, #(24 + 21 * 8)]
  ldp x23, x24, [x17, #(24 + 23 * 8)]
  ldp x25, x26, [x17, #(24 + 25 * 8)]
  ldr x27,      [x17, #(24 + 27 * 8)]
  ldr x28,      [x17, #(24 + 28 * 8)]

  ldp x29, x30, [x17, #256]

  br x16
