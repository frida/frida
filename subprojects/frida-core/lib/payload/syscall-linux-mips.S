/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define frida_asm_align 16
#define frida_asm_custom_entry(f) .ent f
#define frida_asm_custom_end(f) .end f
#define frida_asm_function_type @function

#include "syscall-linux.h"

/*
 * The caller is only required to allocate 16 bytes of stack for a0-a3.
 * syscall has up to 6 arguments, so we need space for the extra two arguments.
 */
#define STACKSIZE 2*4

ENTRY_PRIVATE(syscall)
  .set noreorder
  .cpload $t9
  move    $v0, $a0
  move    $a0, $a1
  move    $a1, $a2
  move    $a2, $a3
  lw      $a3, 16($sp)
  lw      $t0, 20($sp)
  lw      $t1, 24($sp)
  subu    $sp, STACKSIZE
  sw      $t0, 16($sp)
  sw      $t1, 20($sp)
  syscall
  addu    $sp, STACKSIZE
  bnez    $a3, 1f
  move    $a0, $v0
  j       $ra
  nop
1:
#ifdef __mips64
  dla     $t9, _frida_set_errno
#else
  la      $t9, _frida_set_errno
#endif
  j       $t9
  nop
  .set reorder
END(syscall)
