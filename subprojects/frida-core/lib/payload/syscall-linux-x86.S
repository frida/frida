/*
 * Copyright (C) 2008 The Android Open Source Project
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
#define frida_asm_custom_entry(f)
#define frida_asm_custom_end(f)
#define frida_asm_function_type @function

#include "syscall-linux.h"

/*
 * Generic syscall call.
 * Upon entry:
 *   %eax: system call number  - caller save
 *   %ebx: arg0 to system call - callee save
 *   %ecx: arg1                - caller save
 *   %edx: arg2                - caller save
 *   %esi: arg3                - callee save
 *   %edi: arg4                - callee save
 *   %ebp: arg5                - callee save
 */

ENTRY_PRIVATE(syscall)
  # Push the callee save registers.
  push    %ebx
  .cfi_adjust_cfa_offset 4
  .cfi_rel_offset ebx, 0
  push    %esi
  .cfi_adjust_cfa_offset 4
  .cfi_rel_offset esi, 0
  push    %edi
  .cfi_adjust_cfa_offset 4
  .cfi_rel_offset edi, 0
  push    %ebp
  .cfi_adjust_cfa_offset 4
  .cfi_rel_offset ebp, 0

  # Get and save the system call entry address.
  lea frida_kernel_syscall@GOTOFF(%ebx), %eax
  push    %eax
  .cfi_adjust_cfa_offset 4
  .cfi_rel_offset eax, 0

  # Load all the arguments from the calling frame.
  # (Not all will be valid, depending on the syscall.)
  mov     24(%esp), %eax
  mov     28(%esp), %ebx
  mov     32(%esp), %ecx
  mov     36(%esp), %edx
  mov     40(%esp), %esi
  mov     44(%esp), %edi
  mov     48(%esp), %ebp

  # Make the system call.
  call    *(%esp)
  addl    $4, %esp

  # Error?
  cmpl    $-FRIDA_MAX_ERRNO, %eax
  jb      1f
  # Yes, so set errno.
  negl    %eax
  pushl   %eax
  call    _frida_set_errno
  addl    $4, %esp
1:
  # Restore the callee save registers.
  pop    %ebp
  .cfi_adjust_cfa_offset -4
  .cfi_restore ebp
  pop    %edi
  .cfi_adjust_cfa_offset -4
  .cfi_restore edi
  pop    %esi
  .cfi_adjust_cfa_offset -4
  .cfi_restore esi
  pop    %ebx
  .cfi_adjust_cfa_offset -4
  .cfi_restore ebx
  ret
END(syscall)

ENTRY_PRIVATE(frida_kernel_syscall)
  int $0x80
  ret
END(frida_kernel_syscall)
