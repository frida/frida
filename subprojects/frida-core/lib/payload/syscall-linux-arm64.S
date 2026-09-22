/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
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

#define frida_asm_align 0
#define frida_asm_custom_entry(f)
#define frida_asm_custom_end(f)
#define frida_asm_function_type %function

#include "syscall-linux.h"

ENTRY_PRIVATE(syscall)
  /* Move syscall No. from x0 to x8 */
  mov     x8, x0
  /* Move syscall parameters from x1 thru x6 to x0 thru x5 */
  mov     x0, x1
  mov     x1, x2
  mov     x2, x3
  mov     x3, x4
  mov     x4, x5
  mov     x5, x6
  svc     #0

  /* check if syscall returned successfully */
  cmn     x0, #(FRIDA_MAX_ERRNO + 1)
  cneg    x0, x0, hi
  b.hi    _frida_on_syscall_error

  ret
END(syscall)

ENTRY_PRIVATE(_frida_on_syscall_error)
  adrp    x0, _frida_set_errno
  add     x0, x0, :lo12:_frida_set_errno
  br      x0
END(_frida_on_syscall_error)
